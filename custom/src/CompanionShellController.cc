#include "CompanionShellController.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QElapsedTimer>
#include <QtCore/QRandomGenerator>
#include <cstring>

#include "CompanionProtocol.h"
#include "LinkInterface.h"
#include "LinkManager.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

using namespace CompanionShell;

namespace {

constexpr qint64 kHeartbeatTimeoutMs = 3500;  // transport health, not authentication
constexpr qint64 kRetryMs = 1000;
constexpr int kMaxRetries = 5;
constexpr qint64 kQueryMs = 1000;
constexpr int kMaxText = 200000;
constexpr size_t kMaxHeldFrames = 64;
const QByteArray kZeroSession(kSessionLen, '\0');

/// Production route: one LinkInterface instance, valid only while LinkManager still owns it and it is connected.
class LinkInterfacePort final : public ShellLinkPort
{
public:
    explicit LinkInterfacePort(SharedLinkInterfacePtr link) : _link(std::move(link)) {}

    const void* identity() const override { return _link.get(); }

    bool usable() const override
    {
        return _link && _link->mavlinkChannelIsSet() && LinkManager::instance()->containsLink(_link.get()) &&
               _link->isConnected();
    }

    uint8_t channel() const override { return _link->mavlinkChannel(); }

    bool send(const mavlink_message_t& message) override
    {
        // Vehicle is nullptr on purpose: the shell must not inherit an unrelated Vehicle's outgoing-message adjustment.
        return Companion::sendMessage(nullptr, _link, message);
    }

private:
    SharedLinkInterfacePtr _link;
};

CompanionShellController::Environment defaultEnvironment()
{
    CompanionShellController::Environment env;
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    env.now = []() { return clock.elapsed(); };
    env.resolveLink = [](LinkInterface* link) -> std::shared_ptr<ShellLinkPort> {
        const SharedLinkInterfacePtr shared = LinkManager::instance()->sharedLinkInterfacePointerForLink(link);
        return shared ? std::make_shared<LinkInterfacePort>(shared) : nullptr;
    };
    env.activeSystemId = []() {
        const Vehicle* const vehicle = MultiVehicleManager::instance()->activeVehicle();
        return vehicle ? vehicle->id() : -1;
    };
    env.ourSystem = []() { return MAVLinkProtocol::instance()->getSystemId(); };
    env.ourComponent = []() { return MAVLinkProtocol::getComponentId(); };
    return env;
}

QByteArray randomBytes(int n)
{
    QByteArray out(n, '\0');
    for (int i = 0; i < n; i += 4) {
        const quint32 v = QRandomGenerator::system()->generate();
        std::memcpy(out.data() + i, &v, static_cast<size_t>(qMin(4, n - i)));
    }
    return out;
}

}  // namespace

CompanionShellController::CompanionShellController(QObject* parent)
    : CompanionShellController(defaultEnvironment(), parent)
{}

CompanionShellController::CompanionShellController(Environment env, QObject* parent)
    : QObject(parent), _env(std::move(env))
{
    _timer.setInterval(100);
    connect(&_timer, &QTimer::timeout, this, &CompanionShellController::tick);
    if (_env.autoStart) {
        _start();
    }
}

void CompanionShellController::_start()
{
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived, this,
            [this](LinkInterface* link, const mavlink_message_t& message) {
                handleMessage(message, _env.resolveLink(link));
            });
    connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged, this,
            [this](Vehicle* vehicle) { activeSystemIdChanged(vehicle ? vehicle->id() : -1); });
    _timer.start();
}

qint64 CompanionShellController::_now() const
{
    return _env.now ? _env.now() : 0;
}

QByteArray CompanionShellController::_sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

bool CompanionShellController::_lineAllowed(const QByteArray& utf8)
{
    for (const char ch : utf8) {
        const uchar c = static_cast<uchar>(ch);
        if ((c < 0x20 && c != 0x09) || c == 0x7F) {  // NUL, CR, LF and every other control byte; TAB is allowed
            return false;
        }
    }
    return utf8.size() <= kMaxLine;
}

// --- properties ----------------------------------------------------------------------------------------------------

bool CompanionShellController::online() const
{
    const qint64 now = _now();
    if (_sessionLink) {
        return !_offline && _sessionLink->usable();
    }
    return _candidate && _candidate->port && _candidate->port->usable() &&
           now - _candidate->lastHeartbeat <= kHeartbeatTimeoutMs;
}

bool CompanionShellController::canSendLine() const
{
    return _state == Ready && !_pending.active && !_interrupt.active && !_unresolved && !_awaitingState && online();
}

bool CompanionShellController::canLogin() const
{
    return _state == AuthRequired && !_pending.active && !_awaitingState && online();
}

void CompanionShellController::_notify()
{
    const bool canLoginNow = canLogin();
    if (canLoginNow != _lastCanLogin) {
        _lastCanLogin = canLoginNow;
        emit canLoginChanged();
    }
    const bool can = canSendLine();
    if (can != _lastCanSend) {
        _lastCanSend = can;
        emit canSendLineChanged();
    }
    const bool on = online();
    if (on != _lastOnline) {
        _lastOnline = on;
        emit onlineChanged();
    }
}

void CompanionShellController::_setState(SessionState state)
{
    if (state != _state) {
        _state = state;
        emit sessionStateChanged();
    }
    _notify();
}

void CompanionShellController::_setError(const QString& message)
{
    if (message != _lastError) {
        _lastError = message;
        emit lastErrorChanged();
    }
}

void CompanionShellController::_appendNotice(const QString& line)
{
    if (!_text.isEmpty() && !_text.endsWith(QLatin1Char('\n'))) {
        _text.append(QLatin1Char('\n'));
    }
    _text.append(line).append(QLatin1Char('\n'));
    emit textChanged();
}

void CompanionShellController::clear()
{
    _text.clear();
    _resetDecoder();  // display only: the session and its authentication are untouched
    emit textChanged();
}

// --- discovery and routing -----------------------------------------------------------------------------------------

void CompanionShellController::handleMessage(const mavlink_message_t& message,
                                             const std::shared_ptr<ShellLinkPort>& port)
{
    if (!port) {
        return;  // not a link LinkManager owns: nothing we send could be pinned to it
    }
    if (isShellHeartbeat(message)) {
        _onHeartbeat(message.sysid, port);
        return;
    }
    const auto reply =
        decodeReply(message, _env.ourSystem ? _env.ourSystem() : 255, _env.ourComponent ? _env.ourComponent() : 190);
    if (reply) {
        _onReply(*reply, port);
    }
}

void CompanionShellController::_onHeartbeat(uint8_t sysid, const std::shared_ptr<ShellLinkPort>& port)
{
    const qint64 now = _now();
    if (_sessionLink) {
        // Only the pinned route's heartbeat keeps this session online: the same sysid on another link proves nothing
        // about it.
        if (port->identity() == _sessionLink->identity() && sysid == _sessionSysId) {
            _sessionHeartbeat = now;
            if (_offline) {
                _offline = false;
                _recomputeState();
                if (!_token.isEmpty()) {
                    _sendQuery();  // the packet outage is over: ask for the authoritative state within the lease
                }
            }
        }
        _notify();
        return;
    }
    const int active = _env.activeSystemId ? _env.activeSystemId() : -1;
    if (active >= 0 && sysid != active) {
        return;
    }
    const bool fresh =
        _candidate && _candidate->port->usable() && now - _candidate->lastHeartbeat <= kHeartbeatTimeoutMs;
    if (fresh && (_candidate->port->identity() != port->identity() || _candidate->sysid != sysid)) {
        return;  // the first candidate stays while it is connected and fresh
    }
    _candidate = Candidate{sysid, port, now};
    _notify();
}

void CompanionShellController::activeSystemIdChanged(int systemId)
{
    if (systemId >= 0 && _sessionLink && systemId != _sessionSysId) {
        // A different vehicle: say goodbye over the old route first, then forget everything. Never reuse the token
        // elsewhere.
        if (!_token.isEmpty()) {
            _closeOverOldLink(_sessionLink, _token);
        }
        _retireSession(tr("Vehicle changed; the shell session was closed."), _pending.active);
        _candidate.reset();
    } else if (systemId >= 0 && !_sessionLink && _candidate && _candidate->sysid != systemId) {
        _candidate.reset();
    }
    // activeVehicleChanged(nullptr) alone is not a reset: the FC vanishing says nothing about the shell route.
    _notify();
}

// --- actions --------------------------------------------------------------------------------------------------------

void CompanionShellController::connectShell()
{
    if (_sessionLink && !_token.isEmpty()) {
        return;
    }
    if (!_candidate || !_candidate->port->usable() || _now() - _candidate->lastHeartbeat > kHeartbeatTimeoutMs) {
        _setError(tr("No shell service heartbeat. Is the companion computer online?"));
        return;
    }
    _setError(QString());
    _sessionLink = _candidate->port;  // pinned: every packet of this session uses exactly this instance
    _sessionSysId = _candidate->sysid;
    _sessionHeartbeat = _candidate->lastHeartbeat;
    _nonce = randomBytes(kNonceLen);
    _token.clear();
    _haveRevision = false;
    _revision = 0;
    _epoch = 0;
    _nextTransaction = 1;
    _highestIssued = 0;
    _controlId = 0;
    _lastOutputSeq = 0;
    _serverState = Idle;
    _unresolved = false;
    _offline = false;
    _closeRequested = false;
    _pending = {};
    _transfer = {};
    _interrupt = {};
    _heldOutput.clear();
    _text.clear();
    _lastResolution = NoPending;
    _resetDecoder();
    emit textChanged();
    _control = {true, Packet{Open, kZeroSession, 0, 0, _nonce}, _now(), 0};
    _sendOnSession(_control.packet);
    _setState(Connecting);
}

void CompanionShellController::login(const QString& password)
{
    const QByteArray bytes = password.toUtf8();
    if (!canLogin()) {
        return;
    }
    if (bytes.isEmpty() || bytes.size() > kMaxPassword) {
        _setError(tr("The password must be 1 to %1 bytes.").arg(kMaxPassword));
        return;
    }
    _setError(QString());
    _beginTransfer(BeginAuth, bytes);
}

void CompanionShellController::sendLine(const QString& line)
{
    if (!canSendLine()) {
        return;
    }
    const QByteArray utf8 = line.toUtf8();
    if (utf8.size() > kMaxLine) {
        _setError(tr("A line can be at most %1 bytes.").arg(kMaxLine));
        return;
    }
    if (!_lineAllowed(utf8)) {
        _setError(tr("Control characters are not allowed in a line."));
        return;
    }
    _setError(QString());
    _beginTransfer(BeginLine, utf8);
}

void CompanionShellController::sendCtrlC()
{
    if (_token.isEmpty() || !_sessionLink || _interrupt.active) {
        return;
    }
    _interrupt = {true, Packet{Interrupt, _token, ++_controlId, _highestIssued, {}}, _now(), 0};
    _sendOnSession(_interrupt.packet);
    _transfer = {};  // the cancel fence retires the request; stop feeding it, QUERY settles what became of it
    _appendNotice(tr("Interrupt sent; old output discarded."));
    _notify();
}

void CompanionShellController::disconnectShell()
{
    if (!_sessionLink) {
        _setState(Disconnected);
        return;
    }
    if (_token.isEmpty()) {
        _retireSession(QString(), false);  // OPEN was never answered: the service closes an idle session by lease
        return;
    }
    _closeRequested = true;
    _pending = {};
    _clearTransfer();
    _control = {true, Packet{Close, _token, ++_controlId, 0, {}}, _now(), 0};
    if (!_sendOnSession(_control.packet)) {
        _setError(tr(
            "Disconnect unconfirmed: the shell link is gone. The service ends the session when its lease runs out."));
        _retireSession(QString(), false);
        return;
    }
    _recomputeState();
}

void CompanionShellController::refreshState()
{
    if (!_token.isEmpty()) {
        _awaitingState = true;  // a reopened page waits for the service's answer before it enables input
        _sendQuery();
        _notify();
    }
}

// --- transactions ---------------------------------------------------------------------------------------------------

void CompanionShellController::_beginTransfer(quint8 kind, const QByteArray& data)
{
    _pending = {true, _nextTransaction, kind, _revision};
    _lastResolution = NoPending;
    _highestIssued = qMax(_highestIssued, _pending.id);
    _unresolved = false;
    _transfer = {};
    _transfer.data = data;
    emit requestPendingChanged();
    _sendTransferStep();
    _recomputeState();
}

void CompanionShellController::_sendTransferStep()
{
    if (!_pending.active) {
        return;
    }
    Packet p;
    p.sessionId = _token;
    p.requestId = _pending.id;
    if (!_transfer.beginAcked) {
        p.opcode = _pending.kind;
        p.arg = static_cast<quint32>(_transfer.data.size());
        p.body = _sha256(_transfer.data);
    } else if (_transfer.offset < _transfer.data.size()) {
        p.opcode = Data;
        p.arg = static_cast<quint32>(_transfer.offset);
        p.body = _transfer.data.mid(_transfer.offset, kMaxBody);
    } else {
        p.opcode = Commit;
    }
    _transfer.current = p;
    _transfer.sentAt = _now();
    _sendOnSession(p);
}

void CompanionShellController::_clearTransfer()
{
    _transfer.data.fill('\0');  // a password must not linger
    _transfer = {};
}

void CompanionShellController::_failTransfer(PendingResolution resolution)
{
    _applyResolution(resolution);
    _recomputeState();
}

void CompanionShellController::_onTransferAck(const Ack& ack)
{
    if (!_pending.active || _transfer.current.opcode == 0) {
        return;
    }
    switch (ack.acknowledgedOpcode) {
        case BeginAuth:
        case BeginLine:
            if (ack.result == Ok) {
                _transfer.beginAcked = true;
                _transfer.retries = 0;
                _sendTransferStep();
            } else {
                _failTransfer(TransferFailed);
            }
            return;
        case Data:
            if (ack.result == Ok || ack.result == Incomplete) {
                _transfer.offset =
                    static_cast<int>(ack.nextOffset);  // the service says what it holds; resend only the rest
                _transfer.retries = 0;
                _sendTransferStep();
            } else {
                _failTransfer(TransferFailed);
            }
            return;
        case Commit:
            switch (ack.result) {
                case CompanionShell::Accepted:
                    _applyResolution(_pending.kind == BeginAuth ? Authenticated : Submitted);
                    break;
                case CompanionShell::AuthFailed:
                    _applyResolution(CompanionShellController::AuthFailed);
                    break;
                case CompanionShell::StartFailed:
                    _applyResolution(CompanionShellController::StartFailed);
                    break;
                case CompanionShell::Cancelled:
                    _applyResolution(CompanionShellController::Cancelled);
                    break;
                case CompanionShell::TimedOut:
                    _applyResolution(CompanionShellController::TimedOut);
                    break;
                case CompanionShell::WriteFailed:
                    _applyResolution(CompanionShellController::WriteFailed);
                    break;
                case CompanionShell::Incomplete:
                    _transfer.offset = static_cast<int>(ack.nextOffset);
                    _sendTransferStep();
                    return;
                default:
                    _applyResolution(TransferFailed);
                    break;
            }
            _recomputeState();
            return;
        default:
            return;
    }
}

// --- replies --------------------------------------------------------------------------------------------------------

void CompanionShellController::_onReply(const Reply& reply, const std::shared_ptr<ShellLinkPort>& port)
{
    // A reply counts only on the route the session is pinned to, from the shell it is bound to.
    if (!_sessionLink || port->identity() != _sessionLink->identity() || reply.sysid != _sessionSysId) {
        return;
    }
    _sessionHeartbeat = _now();  // the service answering is as good as its heartbeat
    switch (reply.packet.opcode) {
        case StateOp:
            _onState(reply.packet);
            break;
        case AckOp:
            _onAck(reply.packet);
            break;
        case Output:
            _onOutput(reply.packet);
            break;
        default:
            break;
    }
}

void CompanionShellController::_finishOpenWithout(SessionState state, const QString& message)
{
    _control = {};
    _sessionLink.reset();
    _nonce.clear();
    _setError(message);
    _setState(state);
}

void CompanionShellController::_onState(const Packet& packet)
{
    const auto parsed = decodeState(packet.body);
    if (!parsed || parsed->clientNonce != _nonce) {
        return;  // another client's reply, or one for a retired attempt
    }
    const State& s = *parsed;
    if (packet.sessionId == kZeroSession) {
        if (!_token.isEmpty()) {
            return;  // a delayed pre-session reply: a live session changes state only through its own token
        }
        switch (s.state) {
            case CompanionShell::Disabled:
                _finishOpenWithout(CompanionShellController::Disabled,
                                   tr("The shell is disabled on the companion computer."));
                break;
            case CompanionShell::Locked:
                _retryAfter = s.retryAfterSeconds;
                _finishOpenWithout(CompanionShellController::Locked,
                                   tr("Too many wrong passwords. Try again in %1 s.").arg(s.retryAfterSeconds));
                break;
            case CompanionShell::Busy:
                _finishOpenWithout(CompanionShellController::Busy,
                                   s.reason == StartFailedReason
                                       ? tr("The shell service is still cleaning up an earlier session.")
                                       : tr("Another client owns the shell session."));
                break;
            default:
                break;
        }
        return;
    }
    if (_token.isEmpty()) {
        if (s.state == InvalidSession) {
            return;
        }
        _token = packet.sessionId;  // the service admitted our OPEN
        _control = {};
    } else if (packet.sessionId != _token) {
        return;
    }
    if (s.state == InvalidSession) {
        _applyResolution(_pending.active ? UnknownRetireToken : NoPending);
        _retireSession(tr("The shell session is gone (service restarted or timed out). Connect and log in again."),
                       _pending.active);
        return;
    }
    if (_haveRevision) {
        if (s.revision < _revision) {  // an old snapshot never rolls the client back
            if (_pending.active) {
                _applyResolution(IgnoreStale);
                _recomputeState();
            }
            return;
        }
        if (s.revision == _revision &&
            (s.state != _lastState.state || s.nextTransaction != _lastState.nextTransaction ||
             s.lastTransaction != _lastState.lastTransaction || s.phase != _lastState.phase)) {
            return;  // same revision, different content: not an update
        }
    }
    _awaitingState = false;
    _lastState = s;
    _revision = s.revision;
    _haveRevision = true;
    _serverState = s.state;
    _retryAfter = s.retryAfterSeconds;
    if (s.nextTransaction > 0) {
        _nextTransaction = s.nextTransaction;
    }
    _setEpoch(s.outputEpoch);

    const PendingResolution resolution = _pending.active ? _reconcile(s) : NoPending;
    const bool resume = resolution == ResumeSameRequest;
    _applyResolution(resolution);
    if (resume && _transfer.current.opcode != 0 && !_transfer.exhausted) {
        _transfer.beginAcked = true;
        _transfer.offset = s.nextOffset;  // continue the same request where the service stopped, never a new id
        _sendTransferStep();
    }
    if (s.state == CompanionShell::Closing || s.state == CompanionShell::Closed) {
        _control = {};
        _closeRequested = _closeRequested || s.state == CompanionShell::Closing;
    }
    if (s.state == CompanionShell::Closed) {
        _retireSession(QString(), false);
        return;
    }
    _recomputeState();
}

void CompanionShellController::_onAck(const Packet& packet)
{
    const auto ack = decodeAck(packet.body);
    if (!ack) {
        return;
    }
    if (ack->outputEpoch > _epoch) {
        _setEpoch(ack->outputEpoch);  // the epoch moves only on an ACK or STATE, never on OUTPUT
    }
    switch (ack->acknowledgedOpcode) {
        case Interrupt:
            if (_interrupt.active && packet.requestId == _interrupt.packet.requestId) {
                _interrupt = {};
                if (ack->result != Ok) {
                    _setError(tr("Ctrl-C could not be delivered to the shell."));
                }
            }
            break;
        case Close:
            if (_control.active && _control.packet.opcode == Close && packet.requestId == _control.packet.requestId) {
                _control = {};
            }
            break;
        default:
            if (packet.requestId == _pending.id) {
                _onTransferAck(*ack);
            }
            break;
    }
    _notify();
}

// --- reconciliation -------------------------------------------------------------------------------------------------

CompanionShellController::PendingResolution CompanionShellController::_reconcile(const State& s)
{
    const quint32 q = _pending.id;
    const quint8 kind = _pending.kind;
    if (s.lastTransaction == q && s.phase == PhaseNone && (s.lastKind == kind || s.lastKind == 0)) {
        // last_kind 0 means the fenced id never reached the service; only a CANCELLED result can carry it
        if (s.lastKind == 0 && s.lastResult != CompanionShell::Cancelled) {
            return Unknown;
        }
        switch (s.lastResult) {
            case Accepted:
                return kind == BeginAuth ? Authenticated : Submitted;
            case CompanionShell::AuthFailed:
                return CompanionShellController::AuthFailed;
            case CompanionShell::StartFailed:
                return CompanionShellController::StartFailed;
            case CompanionShell::Cancelled:
                return CompanionShellController::Cancelled;
            case CompanionShell::TimedOut:
                return CompanionShellController::TimedOut;
            case CompanionShell::WriteFailed:
                return CompanionShellController::WriteFailed;
            case Conflict:
            case HashMismatch:
            case Invalid:
            case Stale:
            case Incomplete:
                return TransferFailed;
            default:
                return Unknown;
        }
    }
    if (s.nextTransaction == q && s.phase == PhaseReceiving) {
        return ResumeSameRequest;
    }
    if (s.nextTransaction == q && s.phase == PhaseWriting) {
        return WaitForWriter;
    }
    return Unknown;  // not started, or a later result replaced ours: the outcome stays unknown, never assumed
}

void CompanionShellController::_applyResolution(PendingResolution resolution)
{
    if (resolution != NoPending) {
        _lastResolution = resolution;  // sticky: the STATE that follows an ACK must not erase what the ACK concluded
    }
    switch (resolution) {
        case Authenticated:
        case Submitted:
        case Cancelled:
            break;
        case AuthFailed:
            _setError(tr("Wrong password."));
            break;
        case StartFailed:
            _setError(tr("The shell could not be started."));
            break;
        case TransferFailed:
            _setError(tr("The service refused the transfer."));
            break;
        case TimedOut:
            _setError(tr("The transfer timed out."));
            break;
        case WriteFailed:
            _setError(tr("The shell could not accept the line."));
            break;
        default:
            break;
    }
    const bool terminal = resolution == Authenticated || resolution == AuthFailed || resolution == StartFailed ||
                          resolution == Submitted || resolution == Cancelled || resolution == TransferFailed ||
                          resolution == TimedOut || resolution == WriteFailed;
    if (terminal) {
        if (resolution == Authenticated || resolution == Submitted) {
            _setError(QString());
        }
        _pending = {};
        _clearTransfer();
        _unresolved = false;
        emit requestPendingChanged();
    } else if (resolution == NoPending) {
        _unresolved = false;
    } else {
        _unresolved = true;  // ResumeSameRequest, WaitForWriter, Unknown, IgnoreStale, UnknownRetireToken
    }
}

void CompanionShellController::_recomputeState()
{
    if (_token.isEmpty()) {
        _notify();
        return;  // Connecting / Disconnected / Disabled / Locked / Busy are set where they are decided
    }
    SessionState next = Disconnected;
    switch (_serverState) {
        case CompanionShell::AuthRequired:
            next = (_pending.active && _pending.kind == BeginAuth && !_unresolved) ? Authenticating : AuthRequired;
            break;
        case CompanionShell::Ready:
            next = Ready;
            break;
        case CompanionShell::Closing:
            next = CompanionShellController::Closing;
            break;
        case CompanionShell::Locked:
            next = CompanionShellController::Locked;
            break;
        case CompanionShell::Idle:
            next = Connecting;
            break;
        default:
            next = Disconnected;
            break;
    }
    if (_unresolved && (_serverState == CompanionShell::AuthRequired || _serverState == CompanionShell::Ready)) {
        next = Uncertain;
    }
    if (_closeRequested && next != Disconnected) {
        next = CompanionShellController::Closing;
    }
    if (_offline) {
        next = Offline;
    }
    _setState(next);
}

void CompanionShellController::_retireSession(const QString& reason, bool commandOutcomeUnknown)
{
    const bool unknown = commandOutcomeUnknown && _pending.active;
    if (unknown) {
        _lastResolution = UnknownRetireToken;  // the command's fate is unknown; it is never replayed in a new session
    }
    _sessionLink.reset();
    _nonce.clear();
    _token.clear();
    _pending = {};
    _clearTransfer();
    _control = {};
    _interrupt = {};
    _heldOutput.clear();
    _unresolved = false;
    _awaitingState = false;
    _closeRequested = false;
    _offline = false;
    _haveRevision = false;
    _serverState = Idle;
    if (!reason.isEmpty()) {
        _setError(reason);
    }
    emit requestPendingChanged();
    _setState(unknown ? Uncertain : Disconnected);  // stays Uncertain until the user connects again
}

void CompanionShellController::_linkLost()
{
    const bool pending = _pending.active;
    _retireSession(tr("The shell link was lost; the session ended. Reconnect and log in again."), pending);
    _candidate.reset();  // no resend, no resume, no fallback to another link
}

void CompanionShellController::_closeOverOldLink(const std::shared_ptr<ShellLinkPort>& link, const QByteArray& token)
{
    if (!link || !link->usable()) {
        return;  // best effort: the service's 10 s lease cleans up; the token is never sent over another link
    }
    mavlink_message_t message;
    const Packet close{Close, token, ++_controlId, 0, {}};
    if (packRequest(_env.ourSystem(), _env.ourComponent(), link->channel(), _sessionSysId, close, &message)) {
        link->send(message);
    }
}

// --- timers ---------------------------------------------------------------------------------------------------------

bool CompanionShellController::_sendPacket(const std::shared_ptr<ShellLinkPort>& link, const Packet& packet)
{
    if (!link || !link->usable()) {
        return false;
    }
    mavlink_message_t message;
    if (!packRequest(_env.ourSystem(), _env.ourComponent(), link->channel(), _sessionSysId, packet, &message)) {
        return false;
    }
    return link->send(message);
}

bool CompanionShellController::_sendOnSession(const Packet& packet)
{
    return _sendPacket(_sessionLink, packet);
}

void CompanionShellController::_sendQuery()
{
    if (_token.isEmpty()) {
        return;
    }
    _lastQuery = _now();
    _sendOnSession(Packet{Query, _token, 0, 0, _nonce});
}

void CompanionShellController::_retry(Control& control, const QString& giveUp, const std::function<void()>& onGiveUp)
{
    if (!control.active || _now() - control.sentAt < kRetryMs) {
        return;
    }
    if (control.retries >= kMaxRetries) {
        control.active = false;
        _setError(giveUp);
        if (onGiveUp) {
            onGiveUp();
        }
        return;
    }
    ++control.retries;
    control.sentAt = _now();
    _sendOnSession(control.packet);  // the identical packet: the service deduplicates by id
}

void CompanionShellController::tick()
{
    const qint64 now = _now();
    if (_sessionLink) {
        if (!_sessionLink->usable()) {
            _linkLost();
            return;
        }
        const bool stale = now - _sessionHeartbeat > kHeartbeatTimeoutMs;
        if (stale != _offline) {
            _offline = stale;
            _recomputeState();
        }
    }
    if (_control.active && _control.packet.opcode == Open) {
        _retry(_control, tr("The shell service did not answer."),
               [this]() { _finishOpenWithout(Disconnected, _lastError); });
    } else if (_control.active && _control.packet.opcode == Close) {
        _retry(_control, tr("Disconnect unconfirmed. The service ends the session when its lease runs out."),
               [this]() { _retireSession(QString(), false); });
    }
    if (_interrupt.active) {
        _retry(_interrupt, tr("Interrupt unconfirmed."), [this]() { _interrupt = {}; });  // rendering resumes
    }
    if (_pending.active && _transfer.current.opcode != 0 && !_transfer.exhausted &&
        now - _transfer.sentAt >= kRetryMs) {
        if (_transfer.retries >= kMaxRetries) {
            _transfer.exhausted = true;  // never a new id automatically; QUERY and the user's Cancel decide
            _unresolved = true;
            _lastResolution = Unknown;
            _setError(tr("No answer from the shell service; waiting for its state."));
            _recomputeState();
        } else {
            ++_transfer.retries;
            _transfer.sentAt = now;
            _sendOnSession(_transfer.current);
        }
    }
    if (!_token.isEmpty() && now - _lastQuery >= kQueryMs && !_offline) {
        _sendQuery();
    }
    _notify();
}

// --- output ---------------------------------------------------------------------------------------------------------

void CompanionShellController::_setEpoch(quint32 epoch)
{
    if (epoch <= _epoch) {
        return;
    }
    _epoch = epoch;
    _lastOutputSeq = 0;  // sequence numbers are judged within an epoch
    _resetDecoder();
    _interrupt = {};     // the new epoch is the interrupt's confirmation
    _releaseHeldOutput();
}

void CompanionShellController::_releaseHeldOutput()
{
    std::vector<Packet> held;
    held.swap(_heldOutput);
    for (const Packet& p : held) {
        if (p.arg == _epoch) {
            _onOutput(p);
        }
    }
}

void CompanionShellController::_resetDecoder()
{
    _utf8.resetState();
    _escState = 0;
}

void CompanionShellController::_onOutput(const Packet& packet)
{
    const quint32 epoch = packet.arg;
    const quint32 seq = packet.requestId;
    if (epoch < _epoch) {
        return;  // old epoch, including delayed packets that were already on the link
    }
    if (epoch > _epoch) {
        if (_heldOutput.size() < kMaxHeldFrames) {
            _heldOutput.push_back(packet);  // a future epoch waits for a STATE/ACK that announces it
        }
        refreshState();
        return;
    }
    if (_interrupt.active) {
        return;  // rendering is suspended until the interrupt is acknowledged
    }
    if (_lastOutputSeq != 0 && seq <= _lastOutputSeq) {
        return;  // duplicate or older than what is already shown
    }
    if (_lastOutputSeq != 0 && seq > _lastOutputSeq + 1) {
        _resetDecoder();  // never feed a partial escape sequence across a gap
        _appendNotice(tr("[output lost]"));
    }
    _lastOutputSeq = seq;
    _appendOutput(packet.body);
}

void CompanionShellController::_appendOutput(const QByteArray& bytes)
{
    QByteArray visible;
    visible.reserve(bytes.size());
    for (const char ch : bytes) {
        const uchar c = static_cast<uchar>(ch);
        switch (_escState) {
            case 0:
                if (c == 0x1B) {
                    _escState = 1;
                } else if (c != '\r') {
                    visible.append(ch);
                }
                break;
            case 1:  // after ESC
                _escState = (c == '[') ? 2 : (c == ']') ? 3 : 0;
                break;
            case 2:  // CSI: parameters until a final byte
                if (c >= 0x40 && c <= 0x7E) {
                    _escState = 0;
                }
                break;
            case 3:  // OSC: until BEL or ESC \ .
                _escState = (c == 0x07) ? 0 : (c == 0x1B) ? 4 : 3;
                break;
            default:
                _escState = 0;
                break;
        }
    }
    const QString decoded =
        _utf8(QByteArrayView(visible));  // a character split across two frames is held until complete
    if (decoded.isEmpty()) {
        return;
    }
    _text.append(decoded);
    if (_text.size() > kMaxText) {
        const int excess = static_cast<int>(_text.size() - kMaxText);
        const int newline = static_cast<int>(_text.indexOf(QLatin1Char('\n'), excess));
        _text.remove(0, newline >= 0 ? newline + 1 : excess);  // cut at a line boundary, hard cap as the fallback
    }
    emit textChanged();
}
