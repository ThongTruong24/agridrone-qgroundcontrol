#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringDecoder>
#include <QtCore/QTimer>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "CompanionShellProtocol.h"

class LinkInterface;

/// One MAVLink route the shell may use. The controller never reaches for "some connected link": a session is pinned to
/// the exact route that delivered the shell heartbeat. Production wraps a LinkInterface; tests supply fakes.
class ShellLinkPort
{
public:
    virtual ~ShellLinkPort() = default;
    virtual const void* identity() const = 0;  // the LinkInterface instance itself
    virtual bool usable() const = 0;           // still registered and connected
    virtual uint8_t channel() const = 0;       // MAVLink channel of that instance
    virtual bool send(const mavlink_message_t& message) = 0;
};

/// The app-owned session to the companion host shell (spec 2026-10-19-mav-shell-design.md). Owned by CustomPlugin,
/// never by the page: hiding and reopening the page reuses this object and asks the service for the authoritative state
/// instead of guessing it from a bash prompt. All protocol state lives here; QML only reads the typed properties below.
class CompanionShellController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(SessionState sessionState READ sessionState NOTIFY sessionStateChanged)
    Q_PROPERTY(bool canSendLine READ canSendLine NOTIFY canSendLineChanged)
    Q_PROPERTY(bool canLogin READ canLogin NOTIFY canLoginChanged)
    Q_PROPERTY(bool requestPending READ requestPending NOTIFY requestPendingChanged)
    Q_PROPERTY(bool hasSession READ hasSession NOTIFY sessionStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int retryAfterSeconds READ retryAfterSeconds NOTIFY sessionStateChanged)

public:
    enum SessionState
    {
        Disconnected,
        Connecting,
        AuthRequired,
        Authenticating,
        Ready,
        Uncertain,
        Closing,
        Disabled,
        Locked,
        Busy,
        Offline
    };
    Q_ENUM(SessionState)

    /// What the client concluded about its one pending request. Same names as `qt_resolutions` in the shared fixture.
    enum PendingResolution
    {
        NoPending,
        Authenticated,
        AuthFailed,
        StartFailed,
        Submitted,
        Cancelled,
        TransferFailed,
        TimedOut,
        WriteFailed,
        ResumeSameRequest,
        WaitForWriter,
        Unknown,
        IgnoreStale,
        UnknownRetireToken
    };
    Q_ENUM(PendingResolution)

    struct Environment
    {
        std::function<qint64()> now;          // monotonic milliseconds
        std::function<std::shared_ptr<ShellLinkPort>(LinkInterface*)> resolveLink;
        std::function<int()> activeSystemId;  // -1 when there is no active Vehicle
        std::function<uint8_t()> ourSystem;
        std::function<uint8_t()> ourComponent;
        bool autoStart = true;  // own timer + MAVLink/Vehicle connections
    };

    explicit CompanionShellController(QObject* parent = nullptr);
    explicit CompanionShellController(Environment env, QObject* parent = nullptr);

    QString text() const { return _text; }

    bool online() const;

    SessionState sessionState() const { return _state; }

    bool canSendLine() const;
    /// One rule for the password field and login(): authentication is wanted, nothing is pending, and the state is
    /// current.
    bool canLogin() const;

    bool requestPending() const { return _pending.active; }

    /// True from Connect until the session is retired: a token is held, or OPEN is still being answered.
    bool hasSession() const { return _sessionLink != nullptr; }

    QString lastError() const { return _lastError; }

    int retryAfterSeconds() const { return _retryAfter; }

    PendingResolution lastResolution() const { return _lastResolution; }

    Q_INVOKABLE void connectShell();
    Q_INVOKABLE void login(const QString& password);
    Q_INVOKABLE void sendLine(const QString& line);
    Q_INVOKABLE void sendCtrlC();
    Q_INVOKABLE void disconnectShell();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void refreshState();

    /// Entry point for every received MAVLink message. `port` is the route it arrived on (null when unmanaged).
    void handleMessage(const mavlink_message_t& message, const std::shared_ptr<ShellLinkPort>& port);
    /// The Vehicle changed: -1 means none. A different system id closes the session over its own link first.
    void activeSystemIdChanged(int systemId);
    /// Timers: online, retransmission, QUERY, OPEN/CLOSE/INTERRUPT retries. Driven by the QTimer, or directly by tests.
    void tick();

signals:
    void textChanged();
    void onlineChanged();
    void sessionStateChanged();
    void canSendLineChanged();
    void canLoginChanged();
    void requestPendingChanged();
    void lastErrorChanged();

private:
    friend class CompanionShellControllerTest;

    struct Candidate
    {
        uint8_t sysid = 0;
        std::shared_ptr<ShellLinkPort> port;
        qint64 lastHeartbeat = 0;
    };

    struct Pending
    {
        bool active = false;
        quint32 id = 0;
        quint8 kind = 0;
        quint32 revisionSeen = 0;
    };

    struct Transfer
    {
        QByteArray data;
        CompanionShell::Packet current;  // the one packet awaiting its ACK, retransmitted verbatim
        qint64 sentAt = 0;
        int retries = 0;
        bool exhausted = false;
        bool beginAcked = false;
        int offset = 0;  // bytes the service has staged, from its DATA ACKs
    };

    struct Control
    {
        bool active = false;
        CompanionShell::Packet packet;
        qint64 sentAt = 0;
        int retries = 0;
    };

    void _start();
    qint64 _now() const;
    void _onHeartbeat(uint8_t sysid, const std::shared_ptr<ShellLinkPort>& port);
    void _onReply(const CompanionShell::Reply& reply, const std::shared_ptr<ShellLinkPort>& port);
    void _onState(const CompanionShell::Packet& packet);
    void _onAck(const CompanionShell::Packet& packet);
    void _onOutput(const CompanionShell::Packet& packet);
    PendingResolution _reconcile(const CompanionShell::State& s);
    void _applyResolution(PendingResolution resolution);
    void _recomputeState();
    void _setState(SessionState state);
    void _setError(const QString& message);
    void _notify();
    void _appendNotice(const QString& line);
    void _linkLost();
    void _onTransferAck(const CompanionShell::Ack& ack);
    void _finishOpenWithout(SessionState state, const QString& message);
    void _retry(Control& control, const QString& giveUp, const std::function<void()>& onGiveUp);

    bool _sendPacket(const std::shared_ptr<ShellLinkPort>& link, const CompanionShell::Packet& packet);
    bool _sendOnSession(const CompanionShell::Packet& packet);
    void _beginTransfer(quint8 kind, const QByteArray& data);
    void _sendTransferStep();
    void _failTransfer(PendingResolution resolution);
    void _clearTransfer();
    void _retireSession(const QString& reason, bool commandOutcomeUnknown);
    void _closeOverOldLink(const std::shared_ptr<ShellLinkPort>& link, const QByteArray& token);
    void _sendQuery();
    void _setEpoch(quint32 epoch);
    void _resetDecoder();
    void _appendOutput(const QByteArray& bytes);
    void _releaseHeldOutput();
    static QByteArray _sha256(const QByteArray& data);
    static bool _lineAllowed(const QByteArray& utf8);

    Environment _env;
    QTimer _timer;
    QString _text;
    QString _lastError;
    int _retryAfter = 0;

    // discovery
    std::optional<Candidate> _candidate;
    qint64 _sessionHeartbeat = 0;
    bool _lastOnline = false;

    // session (all memory only: a QGC restart loses the token on purpose, there is no takeover)
    std::shared_ptr<ShellLinkPort> _sessionLink;
    uint8_t _sessionSysId = 0;
    QByteArray _nonce;
    QByteArray _token;
    SessionState _state = Disconnected;
    quint8 _serverState = CompanionShell::Idle;
    bool _unresolved = false;
    quint32 _revision = 0;
    quint32 _epoch = 0;
    quint32 _nextTransaction = 1;
    quint32 _highestIssued = 0;
    quint32 _controlId = 0;
    bool _haveRevision = false;
    qint64 _lastQuery = 0;
    PendingResolution _lastResolution = NoPending;

    // one in-flight transaction
    Pending _pending;
    Transfer _transfer;
    Control _control;    // OPEN, CLOSE or INTERRUPT awaiting its answer
    Control _interrupt;  // INTERRUPT awaiting its ACK: output of the current epoch is not rendered meanwhile
    bool _closeRequested = false;
    bool _offline = false;
    bool _awaitingState = false;  // refreshState() asked and nothing answered yet: input stays disabled
    CompanionShell::State _lastState;
    bool _lastCanSend = false;
    bool _lastCanLogin = false;

    // output
    quint32 _lastOutputSeq = 0;
    std::vector<CompanionShell::Packet> _heldOutput;  // frames of an epoch we have not been told about yet
    QStringDecoder _utf8{QStringDecoder::Utf8};
    int _escState = 0;                                // 0 text, 1 after ESC, 2 CSI, 3 OSC, 4 OSC after ESC
};
