#include "CompanionShellControllerTest.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaEnum>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <memory>
#include <vector>

#include "ColoredSvgImageProvider.h"
#include "CompanionShellController.h"
#include "CompanionShellProtocol.h"
#include "CustomPlugin.h"
#include "QmlComponentInfo.h"

UT_REGISTER_TEST(CompanionShellControllerTest, TestLabel::Unit)

using namespace CompanionShell;

namespace {

// Golden protocol fixtures copied unchanged from the companion service specification.
QJsonObject loadFixture(const QString& name)
{
    QFile f(QStringLiteral(":/shellfixtures/%1").arg(name));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

QByteArray hex(const QJsonValue& v)
{
    return QByteArray::fromHex(v.toString().toLatin1());
}

Packet packetFrom(const QJsonObject& v)
{
    Packet p;
    p.opcode = static_cast<quint8>(v["opcode"].toInt());
    p.sessionId = hex(v["session_hex"]);
    p.requestId = static_cast<quint32>(v["request_id"].toDouble());
    p.arg = static_cast<quint32>(v["arg"].toDouble());
    p.body = hex(v["body_hex"]);
    return p;
}

}  // namespace

void CompanionShellControllerTest::_testWireGoldenVectors()
{
    // 2026-10-19: Python and Qt must produce the same bytes for every opcode, or one side silently drops the other.
    const QJsonObject fx = loadFixture(QStringLiteral("mav-shell-v1.json"));
    const QJsonArray valid = fx["valid"].toArray();
    QVERIFY2(valid.size() >= 18, "golden vectors not found: the fixture resource is missing");
    for (const QJsonValue& entry : valid) {
        const QJsonObject v = entry.toObject();
        const QByteArray wire = hex(v["wire_hex"]);
        const Packet p = packetFrom(v);
        QVERIFY2(encode(p) == wire, qPrintable(v["name"].toString()));
        const auto back = decode(wire);
        QVERIFY2(back.has_value() && *back == p, qPrintable(v["name"].toString()));
    }
}

void CompanionShellControllerTest::_testInvalidEnvelopesRejected()
{
    const QJsonArray invalid = loadFixture(QStringLiteral("mav-shell-v1.json"))["invalid"].toArray();
    QVERIFY2(invalid.size() >= 26, "invalid vectors not found");
    for (const QJsonValue& entry : invalid) {
        const QJsonObject v = entry.toObject();
        QVERIFY2(!decode(hex(v["wire_hex"])).has_value(), qPrintable(v["name"].toString()));
    }
    Packet tooLong;
    tooLong.opcode = Data;
    tooLong.sessionId = QByteArray(kSessionLen, 'x');
    tooLong.body = QByteArray(kMaxBody + 1, 'a');
    QVERIFY(encode(tooLong).isEmpty());  // never emits what the receiver would refuse
}

void CompanionShellControllerTest::_testStateAndAckBodyVectors()
{
    const QJsonObject fx = loadFixture(QStringLiteral("mav-shell-v1.json"));
    for (const QJsonValue& entry : fx["state"].toArray()) {
        const QJsonObject v = entry.toObject();
        const QJsonObject f = v["fields"].toObject();
        State s;
        s.clientNonce = hex(f["client_nonce"]);
        s.state = static_cast<quint8>(f["state"].toInt());
        s.reason = static_cast<quint8>(f["reason"].toInt());
        s.revision = static_cast<quint32>(f["revision"].toDouble());
        s.outputEpoch = static_cast<quint32>(f["output_epoch"].toDouble());
        s.nextTransaction = static_cast<quint32>(f["next_transaction"].toDouble());
        s.retryAfterSeconds = static_cast<quint16>(f["retry_after_seconds"].toInt());
        s.lastTransaction = static_cast<quint32>(f["last_transaction"].toDouble());
        s.lastKind = static_cast<quint8>(f["last_kind"].toInt());
        s.lastResult = static_cast<quint8>(f["last_result"].toInt());
        s.phase = static_cast<quint8>(f["phase"].toInt());
        s.nextOffset = static_cast<quint16>(f["next_offset"].toInt());
        const QByteArray body = hex(v["body_hex"]);
        QCOMPARE(body.size(), kStateLen);
        QCOMPARE(encodeState(s), body);
        QVERIFY(decodeState(body) == s);
    }
    for (const QJsonValue& entry : fx["ack"].toArray()) {
        const QJsonObject v = entry.toObject();
        const QJsonObject f = v["fields"].toObject();
        Ack a;
        a.acknowledgedOpcode = static_cast<quint8>(f["acknowledged_opcode"].toInt());
        a.result = static_cast<quint8>(f["result"].toInt());
        a.nextOffset = static_cast<quint32>(f["next_offset"].toDouble());
        a.nextTransaction = static_cast<quint32>(f["next_transaction"].toDouble());
        a.outputEpoch = static_cast<quint32>(f["output_epoch"].toDouble());
        a.revision = static_cast<quint32>(f["revision"].toDouble());
        const QByteArray body = hex(v["body_hex"]);
        QCOMPARE(body.size(), kAckLen);
        QCOMPARE(encodeAck(a), body);
        QVERIFY(decodeAck(body) == a);
    }
    QVERIFY(!decodeState(QByteArray(40, '\0')).has_value());
    QVERIFY(!decodeAck(QByteArray(17, '\0')).has_value());
}

void CompanionShellControllerTest::_testOuterFramingRoundTrip()
{
    Packet p;
    p.opcode = Data;
    p.sessionId = QByteArray(kSessionLen, 'S');
    p.requestId = 7;
    p.arg = 42;
    p.body = QByteArray("abc\0\0", 5);  // trailing NULs must survive MAVLink2 payload trimming through `count`

    mavlink_message_t msg;
    QVERIFY(packRequest(255, 190, 0, 1, p, &msg));
    QCOMPARE(msg.msgid, uint32_t(MAVLINK_MSG_ID_SERIAL_CONTROL));
    mavlink_serial_control_t sc;
    mavlink_msg_serial_control_decode(&msg, &sc);
    QCOMPARE(sc.device, kDeviceShell);
    QCOMPARE(sc.flags, kRequestFlags);
    QCOMPARE(sc.target_system, uint8_t(1));
    QCOMPARE(sc.target_component, kShellCompId);
    QCOMPARE(int(sc.count), kHeaderLen + p.body.size());

    Packet bad = p;
    bad.body = QByteArray(kMaxBody + 1, 'a');
    QVERIFY(!packRequest(255, 190, 0, 1, bad, &msg));
}

void CompanionShellControllerTest::_testReplyAdmission()
{
    Packet st;
    st.opcode = StateOp;
    st.sessionId = QByteArray(kSessionLen, 'S');
    st.body = encodeState(State{});
    const QByteArray env = encode(st);
    uint8_t data[kMaxCount] = {};
    memcpy(data, env.constData(), size_t(env.size()));

    auto build = [&](uint8_t compid, uint8_t device, uint8_t flags, uint8_t tsys, uint8_t tcomp) {
        mavlink_message_t m;
        mavlink_msg_serial_control_pack_chan(1, compid, 0, &m, device, flags, 0, 0, uint8_t(env.size()), data, tsys,
                                             tcomp);
        return m;
    };
    QVERIFY(decodeReply(build(192, 10, kFlagReply, 255, 190), 255, 190).has_value());
    QVERIFY(!decodeReply(build(191, 10, kFlagReply, 255, 190), 255, 190).has_value());  // not the shell component
    QVERIFY(!decodeReply(build(192, 0, kFlagReply, 255, 190), 255, 190).has_value());   // another device
    QVERIFY(!decodeReply(build(192, 10, 0, 255, 190), 255, 190).has_value());           // not a REPLY
    QVERIFY(!decodeReply(build(192, 10, kFlagReply, 254, 190), 255, 190).has_value());  // addressed to someone else
    QVERIFY(!decodeReply(build(192, 10, kFlagReply, 255, 191), 255, 190).has_value());

    mavlink_message_t hb;
    mavlink_msg_heartbeat_pack_chan(1, 192, 0, &hb, MAV_TYPE_ONBOARD_CONTROLLER, MAV_AUTOPILOT_INVALID, 0, 0,
                                    MAV_STATE_ACTIVE);
    QVERIFY(isShellHeartbeat(hb));
    mavlink_msg_heartbeat_pack_chan(1, 192, 0, &hb, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4, 0, 0, MAV_STATE_ACTIVE);
    QVERIFY(!isShellHeartbeat(hb));
    mavlink_msg_heartbeat_pack_chan(1, 191, 0, &hb, MAV_TYPE_ONBOARD_CONTROLLER, MAV_AUTOPILOT_INVALID, 0, 0,
                                    MAV_STATE_ACTIVE);
    QVERIFY(!isShellHeartbeat(hb));
}

// =====================================================================================================================
// Controller tests. The clock is fake, the links are fakes, and nothing here touches a real LinkManager.
// =====================================================================================================================

namespace {

const QByteArray kToken = QByteArray::fromHex("a0a1a2a3a4a5a6a7a8a9aaabacadaeaf");
constexpr uint8_t kGcsSys = 255;
constexpr uint8_t kGcsComp = 190;

class FakeLink final : public ShellLinkPort
{
public:
    struct Frame
    {
        uint8_t device = 0;
        uint8_t flags = 0;
        uint8_t targetSystem = 0;
        uint8_t targetComponent = 0;
        std::optional<Packet> packet;
    };

    explicit FakeLink(uint8_t channel) : _channel(channel) {}

    const void* identity() const override { return this; }

    bool usable() const override { return connected; }

    uint8_t channel() const override { return _channel; }

    bool send(const mavlink_message_t& message) override
    {
        mavlink_serial_control_t sc;
        mavlink_msg_serial_control_decode(&message, &sc);
        frames.push_back({sc.device, sc.flags, sc.target_system, sc.target_component,
                          decode(QByteArrayView(reinterpret_cast<const char*>(sc.data), sc.count))});
        return connected;
    }

    int count(quint8 opcode) const
    {
        int n = 0;
        for (const Frame& f : frames) {
            n += (f.packet && f.packet->opcode == opcode) ? 1 : 0;
        }
        return n;
    }

    Packet last(quint8 opcode) const
    {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            if (it->packet && it->packet->opcode == opcode) {
                return *it->packet;
            }
        }
        return {};
    }

    bool connected = true;
    std::vector<Frame> frames;

private:
    uint8_t _channel;
};

using LinkPtr = std::shared_ptr<FakeLink>;

struct Harness
{
    qint64 now = 10000;
    int active = -1;
    LinkPtr a = std::make_shared<FakeLink>(0);
    LinkPtr b = std::make_shared<FakeLink>(1);
    CompanionShellController ctl;
    QByteArray nonce;

    Harness() : ctl(environment(), nullptr) {}

    CompanionShellController::Environment environment()
    {
        CompanionShellController::Environment env;
        env.now = [this]() { return now; };
        env.activeSystemId = [this]() { return active; };
        env.ourSystem = []() { return kGcsSys; };
        env.ourComponent = []() { return kGcsComp; };
        env.autoStart = false;
        return env;
    }

    void advance(qint64 ms)
    {
        now += ms;
        ctl.tick();
    }

    /// Time passes while the service is alive: its heartbeat keeps coming, only the replies we wait for do not.
    void advanceAlive(const LinkPtr& link, qint64 ms)
    {
        for (qint64 done = 0; done < ms; done += 500) {
            now += 500;
            heartbeat(link);
            ctl.tick();
        }
    }

    void heartbeat(const LinkPtr& link, uint8_t sysid = 1)
    {
        mavlink_message_t m;
        mavlink_msg_heartbeat_pack_chan(sysid, kShellCompId, 0, &m, MAV_TYPE_ONBOARD_CONTROLLER, MAV_AUTOPILOT_INVALID,
                                        0, 0, MAV_STATE_ACTIVE);
        ctl.handleMessage(m, link);
    }

    void reply(const LinkPtr& link, const Packet& p, uint8_t sysid = 1)
    {
        const QByteArray env = encode(p);
        uint8_t data[kMaxCount] = {};
        memcpy(data, env.constData(), size_t(env.size()));
        mavlink_message_t m;
        mavlink_msg_serial_control_pack_chan(sysid, kShellCompId, 0, &m, kDeviceShell, kFlagReply, 0, 0,
                                             uint8_t(env.size()), data, kGcsSys, kGcsComp);
        ctl.handleMessage(m, link);
    }

    static State baseState(quint8 state, quint32 revision, quint32 nextTransaction = 1)
    {
        State s;
        s.state = state;
        s.revision = revision;
        s.nextTransaction = nextTransaction;
        return s;
    }

    void sendState(const LinkPtr& link, State s, const QByteArray& session = kToken, uint8_t sysid = 1)
    {
        s.clientNonce = nonce;
        reply(link, Packet{StateOp, session, 0, 0, encodeState(s)}, sysid);
    }

    void sendAck(const LinkPtr& link, quint8 opcode, quint8 result, quint32 rid, quint32 offset = 0, quint32 nextTx = 0,
                 quint32 epoch = 0, quint32 revision = 0)
    {
        reply(link, Packet{AckOp, kToken, rid, 0, encodeAck(Ack{opcode, result, offset, nextTx, epoch, revision})});
    }

    void sendOutput(const LinkPtr& link, quint32 seq, quint32 epoch, const QByteArray& bytes)
    {
        reply(link, Packet{Output, kToken, seq, epoch, bytes});
    }

    /// Heartbeat, Connect, and the service admitting the session in `state`.
    void open(const LinkPtr& link, quint8 state = AuthRequired, quint32 nextTx = 1)
    {
        heartbeat(link);
        ctl.connectShell();
        nonce = link->last(Open).body;
        sendState(link, baseState(state, 1, nextTx));
    }

    /// Answer every packet of the transfer in flight until COMMIT has been sent; returns its request id.
    quint32 driveToCommit(const LinkPtr& link, quint32 nextTx)
    {
        quint32 rid = 0;
        for (int guard = 0; guard < 64; ++guard) {
            const Packet p = *link->frames.back().packet;
            rid = p.requestId;
            if (p.opcode == Commit) {
                return rid;
            }
            if (p.opcode == BeginAuth || p.opcode == BeginLine) {
                sendAck(link, p.opcode, Ok, rid, 0, nextTx);
            } else if (p.opcode == Data) {
                sendAck(link, Data, Ok, rid, p.arg + quint32(p.body.size()), nextTx);
            } else {
                break;
            }
        }
        return rid;
    }

private:
    using Frame = FakeLink::Frame;
};

}  // namespace

void CompanionShellControllerTest::_testSameSourceDifferentSessions()
{
    Harness h;
    h.open(h.a);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::AuthRequired);
    // A reply for someone else's nonce (same GCS identity, other client) changes nothing.
    State foreign = Harness::baseState(Ready, 9);
    foreign.clientNonce = QByteArray(kNonceLen, 'x');
    h.reply(h.a, Packet{StateOp, kToken, 0, 0, encodeState(foreign)});
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::AuthRequired);
    // Nor does a pre-session reply once a token has been accepted.
    State late = Harness::baseState(Busy, 0, 0);
    late.clientNonce = h.nonce;
    h.reply(h.a, Packet{StateOp, QByteArray(kSessionLen, '\0'), 0, 0, encodeState(late)});
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::AuthRequired);
    // Nor another shell's reply (wrong sysid on the pinned route).
    h.sendState(h.a, Harness::baseState(Ready, 5), kToken, 7);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::AuthRequired);
}

void CompanionShellControllerTest::_testStateIndependentOfOutput()
{
    Harness h;
    h.open(h.a, Ready, 2);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);
    QVERIFY(h.ctl.canSendLine());
    h.sendOutput(h.a, 1, 0, "Password: \r\nthaco:~$ ");  // a prompt that looks like a login must mean nothing
    QVERIFY(h.ctl.text().contains(QStringLiteral("Password: ")));
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);
    QVERIFY(h.ctl.canSendLine());
    h.ctl.login(QStringLiteral("pw"));  // login is only for AuthRequired, whatever the text says
    QCOMPARE(h.a->count(BeginAuth), 0);
}

void CompanionShellControllerTest::_testStopAndWaitRetries()
{
    Harness h;
    h.open(h.a);
    h.ctl.login(QStringLiteral("secret"));
    QCOMPARE(h.a->count(BeginAuth), 1);
    const Packet first = h.a->last(BeginAuth);
    QCOMPARE(first.requestId, quint32(1));
    QCOMPARE(first.arg, quint32(6));
    QCOMPARE(first.body, QCryptographicHash::hash("secret", QCryptographicHash::Sha256));
    QVERIFY(h.ctl.requestPending());
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Authenticating);

    h.advance(999);
    QCOMPARE(h.a->count(BeginAuth), 1);  // not yet
    h.advance(1);
    QCOMPARE(h.a->count(BeginAuth), 2);
    QVERIFY(h.a->last(BeginAuth) == first);  // the identical packet, same id
    for (int i = 0; i < 4; ++i) {
        h.advanceAlive(h.a, 1000);
    }
    QCOMPARE(h.a->count(BeginAuth), 6);  // one send plus five retransmits
    h.advanceAlive(h.a, 1000);
    QCOMPARE(h.a->count(BeginAuth), 6);  // the budget is spent
    h.advanceAlive(h.a, 1000);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Uncertain);
    QVERIFY(!h.ctl.canSendLine());
    QVERIFY(!h.ctl.lastError().isEmpty());
    QCOMPARE(h.a->count(BeginAuth), 6);  // never a fresh id automatically
}

void CompanionShellControllerTest::_testLineTransferChunks()
{
    Harness h;
    h.open(h.a, Ready, 2);
    const QString line = QString(100, QLatin1Char('x'));  // 100 bytes: three DATA chunks of 42, 42 and 16
    h.ctl.sendLine(line);
    QCOMPARE(h.a->count(BeginLine), 1);
    QCOMPARE(h.a->last(BeginLine).requestId, quint32(2));
    QCOMPARE(h.a->last(BeginLine).arg, quint32(100));
    const quint32 rid = h.driveToCommit(h.a, 2);
    QCOMPARE(rid, quint32(2));
    QCOMPARE(h.a->count(Data), 3);
    QCOMPARE(h.a->last(Data).arg, quint32(84));
    QCOMPARE(h.a->last(Data).body.size(), 16);
    QCOMPARE(h.a->count(Commit), 1);
    // A DATA ACK that says the service holds less than we sent makes us resend only the rest.
    h.sendAck(h.a, Commit, Incomplete, rid, 42, 2);
    QCOMPARE(h.a->last(Data).arg, quint32(42));
    // The accepted ACK ends the request; the line is never sent again.
    h.driveToCommit(h.a, 2);
    h.sendAck(h.a, Commit, Accepted, rid, 0, 3, 0, 5);
    QVERIFY(!h.ctl.requestPending());
    QCOMPARE(h.ctl.lastResolution(), CompanionShellController::Submitted);
    QCOMPARE(h.a->count(BeginLine), 1);
    // Lines the service would refuse are refused here first.
    h.sendState(h.a, Harness::baseState(Ready, 6, 3));
    h.ctl.sendLine(QStringLiteral("a\nb"));
    h.ctl.sendLine(QString(1025, QLatin1Char('y')));
    QCOMPARE(h.a->count(BeginLine), 1);
    QVERIFY(!h.ctl.lastError().isEmpty());
    h.ctl.sendLine(QStringLiteral("printf x\ty"));  // TAB is allowed
    QCOMPARE(h.a->count(BeginLine), 2);
}

void CompanionShellControllerTest::_testLostCommitAckDoesNotResubmit()
{
    Harness h;
    h.open(h.a, Ready, 2);
    h.ctl.sendLine(QStringLiteral("echo hello"));
    const quint32 rid = h.driveToCommit(h.a, 2);
    QCOMPARE(h.a->count(Commit), 1);
    for (int i = 0; i < 6; ++i) {
        h.advanceAlive(h.a, 1000);    // the service is alive but the COMMIT ACK never arrives
    }
    QCOMPARE(h.a->count(Commit), 6);  // the identical COMMIT, retransmitted five times
    h.advanceAlive(h.a, 1000);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Uncertain);
    QVERIFY(!h.ctl.canSendLine());
    QVERIFY(h.ctl.requestPending());
    QCOMPARE(h.a->count(BeginLine), 1);
    QCOMPARE(h.a->count(Data), 1);  // nothing about the line was ever re-sent
    // The service's STATE is authoritative: it holds the accepted result for exactly this id and kind.
    State s = Harness::baseState(Ready, 9, 3);
    s.lastTransaction = rid;
    s.lastKind = BeginLine;
    s.lastResult = Accepted;
    h.sendState(h.a, s);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);
    QVERIFY(!h.ctl.requestPending());
    QCOMPARE(h.ctl.lastResolution(), CompanionShellController::Submitted);
    QCOMPARE(h.a->count(BeginLine), 1);
    QCOMPARE(h.a->count(Commit), 6);
}

void CompanionShellControllerTest::_testOldEpochIgnored()
{
    Harness h;
    h.open(h.a, Ready, 2);
    State s = Harness::baseState(Ready, 2, 2);
    s.outputEpoch = 1;
    h.sendState(h.a, s);
    h.sendOutput(h.a, 1, 0, "old-epoch\r\n");  // a delayed packet already on the link
    QVERIFY(!h.ctl.text().contains(QStringLiteral("old-epoch")));
    h.sendOutput(h.a, 2, 1, "current\r\n");
    QVERIFY(h.ctl.text().contains(QStringLiteral("current")));
    // OUTPUT never changes the epoch by itself: a future epoch waits for a STATE that announces it.
    const int queries = h.a->count(Query);
    h.sendOutput(h.a, 3, 2, "future\r\n");
    QVERIFY(!h.ctl.text().contains(QStringLiteral("future")));
    QCOMPARE(h.a->count(Query), queries + 1);
    s.revision = 3;
    s.outputEpoch = 2;
    h.sendState(h.a, s);
    QVERIFY(h.ctl.text().contains(QStringLiteral("future")));
    h.sendOutput(h.a, 4, 1, "stale-again\r\n");
    QVERIFY(!h.ctl.text().contains(QStringLiteral("stale-again")));
}

void CompanionShellControllerTest::_testOutputGapAndDecoder()
{
    Harness h;
    h.open(h.a, Ready, 2);
    h.sendOutput(h.a, 1, 0, "abc");
    h.sendOutput(h.a, 1, 0, "abc");  // duplicate
    QCOMPARE(h.ctl.text(), QStringLiteral("abc"));
    h.sendOutput(h.a, 3, 0, "def");  // frame 2 never arrived
    QVERIFY(h.ctl.text().contains(QStringLiteral("[output lost]")));
    QVERIFY(h.ctl.text().endsWith(QStringLiteral("def")));
    h.ctl.clear();
    QVERIFY(h.ctl.text().isEmpty());
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);  // Clear is display only

    // A multi-byte character split across frames, and an escape sequence split across frames.
    h.sendOutput(h.a, 4, 0, QByteArray("h\xc3", 2));
    h.sendOutput(h.a, 5, 0, QByteArray("\xa9llo \x1b[3", 8));
    h.sendOutput(h.a, 6, 0, QByteArray("1mred\x1b[0m\r\n", 11));
    QCOMPARE(h.ctl.text(), QStringLiteral("héllo red\n"));

    // An escape sequence cut by a gap must not leak into the text that follows it.
    h.ctl.clear();
    h.sendOutput(h.a, 7, 0, QByteArray("x\x1b[", 3));
    h.sendOutput(h.a, 9, 0, QByteArray("2Jyz", 4));  // frame 8 is lost
    QVERIFY(h.ctl.text().contains(QStringLiteral("2Jyz")));
}

void CompanionShellControllerTest::_testInterruptSuspendsOutputUntilAck()
{
    Harness h;
    h.open(h.a, Ready, 2);
    h.ctl.sendLine(QStringLiteral("sleep 30"));
    h.driveToCommit(h.a, 2);
    h.sendAck(h.a, Commit, Accepted, 2, 0, 3);
    h.ctl.sendCtrlC();
    QCOMPARE(h.a->count(Interrupt), 1);
    const Packet intr = h.a->last(Interrupt);
    QCOMPARE(intr.arg, quint32(2));  // the cancel fence: the highest transaction this client has issued
    QCOMPARE(intr.requestId, quint32(1));
    QVERIFY(h.ctl.text().contains(QStringLiteral("Interrupt sent")));
    h.sendOutput(h.a, 1, 0, "stale-backlog\r\n");  // rendering is suspended while the interrupt is outstanding
    QVERIFY(!h.ctl.text().contains(QStringLiteral("stale-backlog")));
    h.advance(1000);
    QCOMPARE(h.a->count(Interrupt), 2);            // retransmitted verbatim
    QVERIFY(h.a->last(Interrupt) == intr);
    h.sendAck(h.a, Interrupt, Ok, 1, 0, 3, 1, 6);  // the ACK carries the new epoch
    h.sendOutput(h.a, 2, 0, "old-epoch-straggler\r\n");
    QVERIFY(!h.ctl.text().contains(QStringLiteral("old-epoch-straggler")));
    h.sendOutput(h.a, 3, 1, "thaco:~$ ");
    QVERIFY(h.ctl.text().contains(QStringLiteral("thaco:~$")));
    // No ACK at all: after the retry budget rendering resumes and the failure is reported, not hidden.
    h.ctl.sendCtrlC();
    for (int i = 0; i < 7; ++i) {
        h.advance(1000);
    }
    QVERIFY(h.ctl.lastError().contains(QStringLiteral("unconfirmed")));
    h.sendOutput(h.a, 4, 1, "after\r\n");
    QVERIFY(h.ctl.text().contains(QStringLiteral("after")));
}

void CompanionShellControllerTest::_testQueryAfterPageReopen()
{
    Harness h;
    h.open(h.a, Ready, 2);
    QVERIFY(h.ctl.canSendLine());
    h.advance(1000);
    QVERIFY(h.a->count(Query) >= 1);  // the controller keeps asking once a second on its own
    const int before = h.a->count(Query);
    h.ctl.refreshState();             // the page is reopened
    QCOMPARE(h.a->count(Query), before + 1);
    QVERIFY(!h.ctl.canSendLine());    // input waits for the authoritative answer
    h.sendState(h.a, Harness::baseState(Ready, 2, 2));
    QVERIFY(h.ctl.canSendLine());
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);  // no new password needed
}

void CompanionShellControllerTest::_testVehicleSwitchRetiresToken()
{
    Harness h;
    h.active = 1;
    h.open(h.b, Ready, 2);
    const int opens = h.b->count(Open);
    h.active = 2;
    h.ctl.activeSystemIdChanged(2);  // a different vehicle: say goodbye on the old route, then forget the token
    QCOMPARE(h.b->count(Close), 1);
    QCOMPARE(h.b->last(Close).sessionId, kToken);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Disconnected);
    QVERIFY(!h.ctl.canSendLine());
    h.ctl.sendLine(QStringLiteral("ls"));
    QCOMPARE(h.b->count(BeginLine), 0);
    QCOMPARE(h.a->frames.size(), size_t(0));
    h.advance(5000);
    QCOMPARE(h.b->count(Query), h.b->count(Query));  // (no QUERY for a retired token)
    const int queriesAfter = h.b->count(Query);
    h.advance(5000);
    QCOMPARE(h.b->count(Query), queriesAfter);
    QCOMPARE(h.b->count(Open), opens);  // a new session needs an explicit Connect
}

void CompanionShellControllerTest::_testTwoLinksPinHeartbeatRoute()
{
    Harness h;
    // Link A is connected and first in any list, but only link B carries the shell heartbeat.
    h.heartbeat(h.b);
    h.ctl.connectShell();
    QCOMPARE(h.b->count(Open), 1);
    QCOMPARE(h.a->frames.size(), size_t(0));
    h.nonce = h.b->last(Open).body;
    h.sendState(h.b, Harness::baseState(AuthRequired, 1));
    h.ctl.login(QStringLiteral("pw"));
    h.driveToCommit(h.b, 1);
    h.advance(1000);
    QCOMPARE(h.a->frames.size(), size_t(0));  // every kind of packet went to B
    QVERIFY(h.b->count(BeginAuth) >= 1 && h.b->count(Query) >= 1 && h.b->count(Commit) >= 1);

    // A heartbeat from the same system id on link A says nothing about the session on B.
    for (int i = 0; i < 6; ++i) {
        h.advance(1000);
        h.heartbeat(h.a);
    }
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Offline);
    QVERIFY(!h.ctl.online());
    QCOMPARE(h.a->frames.size(), size_t(0));
    // Replies that arrive on A are not this session's either: not its STATE, ACK or OUTPUT, and they do not count as
    // life signs.
    const quint32 revision = h.ctl._revision;
    h.sendState(h.a, Harness::baseState(Ready, 9, 2));
    h.sendAck(h.a, Commit, Accepted, 1, 0, 2, 0, 9);
    h.sendOutput(h.a, 1, 0, "from-the-wrong-link\r\n");
    h.advance(100);  // a reply that was wrongly accepted would have refreshed the heartbeat by now
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Offline);
    QVERIFY(!h.ctl.online());
    QCOMPARE(h.ctl._revision, revision);
    QVERIFY(!h.ctl.text().contains(QStringLiteral("from-the-wrong-link")));
    // B's heartbeat returns: the session recovers by QUERY over B, within the lease.
    const int queries = h.b->count(Query);
    h.heartbeat(h.b);
    QCOMPARE(h.b->count(Query), queries + 1);
    QVERIFY(h.ctl.online());
}

void CompanionShellControllerTest::_testFcLifecycleKeepsPinnedRoute()
{
    Harness h;
    h.active = -1;  // no FC yet
    h.open(h.b, Ready, 2);
    const size_t sentOnA = h.a->frames.size();
    for (int active : {1, -1, 1, -1}) {  // the FC appears and disappears with the same system id; its primary link is A
        h.active = active;
        h.ctl.activeSystemIdChanged(active);
        h.heartbeat(h.a);  // the vehicle's own link keeps heartbeating the same sysid
        QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);
    }
    h.ctl.sendLine(QStringLiteral("uptime"));
    QCOMPARE(h.b->count(BeginLine), 1);
    QCOMPARE(h.a->frames.size(), sentOnA);  // nothing ever fell back to the Vehicle's link
    h.ctl.sendCtrlC();
    QCOMPARE(h.b->count(Interrupt), 1);
    QCOMPARE(h.a->frames.size(), size_t(0));
}

void CompanionShellControllerTest::_testDisconnectUsesOldLink()
{
    Harness h;
    h.active = 1;
    h.open(h.b, Ready, 2);
    h.active = -1;  // the FC went away; its primary link would now be A
    h.ctl.activeSystemIdChanged(-1);
    h.ctl.disconnectShell();
    QCOMPARE(h.b->count(Close), 1);
    QCOMPARE(h.b->last(Close).sessionId, kToken);
    QCOMPARE(h.a->frames.size(), size_t(0));
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Closing);
    h.advance(1000);
    QCOMPARE(h.b->count(Close), 2);  // retransmitted on the same link
    h.sendAck(h.b, Close, Ok, 1);
    State closed = Harness::baseState(CompanionShell::Closed, 3, 2);
    h.sendState(h.b, closed);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Disconnected);
}

void CompanionShellControllerTest::_testRemovedLinkNeverFallsBack()
{
    Harness h;
    h.open(h.b, Ready, 2);
    h.heartbeat(h.a);
    h.ctl.sendLine(QStringLiteral("make"));
    h.driveToCommit(h.b, 2);
    const size_t sentOnB = h.b->frames.size();
    h.b->connected = false;  // link B goes away mid-command
    h.advance(100);
    QCOMPARE(h.ctl.sessionState(),
             CompanionShellController::Uncertain);  // the command's outcome is unknown, not "failed"
    QVERIFY(!h.ctl.lastError().isEmpty());
    QCOMPARE(h.ctl.lastResolution(), CompanionShellController::UnknownRetireToken);
    QVERIFY(!h.ctl.canSendLine());
    h.advance(10000);
    QCOMPARE(h.b->frames.size(), sentOnB);    // nothing more on the dead link
    QCOMPARE(h.a->frames.size(), size_t(0));  // and the token never moved to link A
    // Late callbacks from B are ignored once the session is retired.
    h.b->connected = true;
    h.sendState(h.b, Harness::baseState(Ready, 40, 3));
    h.sendOutput(h.b, 1, 0, "late\r\n");
    QVERIFY(!h.ctl.text().contains(QStringLiteral("late")));
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Uncertain);
    h.ctl.connectShell();  // only an explicit Connect starts over, and it needs fresh heartbeat evidence
    QCOMPARE(h.a->frames.size(), size_t(0));
}

void CompanionShellControllerTest::_testTextCap()
{
    Harness h;
    h.open(h.a, Ready, 2);
    const QByteArray line(41, 'z');
    quint32 seq = 0;
    for (int i = 0; i < 6000; ++i) {  // 6000 x 42 bytes is 252000 characters
        h.sendOutput(h.a, ++seq, 0, line + "\n");
    }
    QVERIFY(h.ctl.text().size() <= 200000);
    QVERIFY(h.ctl.text().size() > 190000);
    QVERIFY(h.ctl.text().startsWith(QLatin1Char('z')));  // cut at a line boundary
    QVERIFY(h.ctl.text().endsWith(QLatin1Char('\n')));
}

void CompanionShellControllerTest::_testLoginRules()
{
    Harness h;
    h.open(h.a);
    h.ctl.login(QString());
    QCOMPARE(h.a->count(BeginAuth), 0);
    QVERIFY(!h.ctl.lastError().isEmpty());
    h.ctl.login(QString(129, QLatin1Char('p')));
    QCOMPARE(h.a->count(BeginAuth), 0);
    h.ctl.login(QString(128, QLatin1Char('p')));  // exactly the limit
    QCOMPARE(h.a->count(BeginAuth), 1);
    QCOMPARE(h.a->last(BeginAuth).arg, quint32(128));
    h.ctl.login(QStringLiteral("again"));  // one request at a time
    QCOMPARE(h.a->count(BeginAuth), 1);
    // The password is wiped from the controller once the request resolves.
    h.sendAck(h.a, BeginAuth, Ok, 1, 0, 1);
    h.sendAck(h.a, Data, Ok, 1, 42, 1);
    h.sendAck(h.a, Commit, CompanionShell::AuthFailed, 1, 0, 2, 0, 3);
    QCOMPARE(h.ctl.lastResolution(), CompanionShellController::AuthFailed);
    QVERIFY(h.ctl.lastError().contains(QStringLiteral("password")));
    QVERIFY(h.ctl._transfer.data.isEmpty());
    QVERIFY(!h.ctl.requestPending());
}

namespace {

QString enumName(int value, const char* enumeration)
{
    const QMetaEnum e = CompanionShellController::staticMetaObject.enumerator(
        CompanionShellController::staticMetaObject.indexOfEnumerator(enumeration));
    return QString::fromLatin1(e.valueToKey(value));
}

}  // namespace

void CompanionShellControllerTest::_testSharedTransitionCases()
{
    // 2026-10-19: the Python service tests drive the same cases; neither side keeps its own expected-state table.
    const QJsonObject fx = loadFixture(QStringLiteral("mav-shell-transitions-v1.json"));
    const QJsonObject defaults = fx["defaults"].toObject();
    const QJsonArray resolutions = fx["qt_resolutions"].toArray();
    QVERIFY2(fx["cases"].toArray().size() >= 40, "transition fixture not found");
    for (const QJsonValue& rv : resolutions) {  // the vocabulary is the enum: no extra, none missing
        QVERIFY2(CompanionShellController::staticMetaObject
                         .enumerator(CompanionShellController::staticMetaObject.indexOfEnumerator("PendingResolution"))
                         .keyToValue(rv.toString().toLatin1().constData()) >= 0,
                 qPrintable(rv.toString()));
    }
    int exercised = 0;
    for (const QJsonValue& cv : fx["cases"].toArray()) {
        const QJsonObject c = cv.toObject();
        const QJsonObject qt = c["qt"].toObject();
        if (c["qt"].isNull() || qt.isEmpty()) {
            continue;
        }
        const QString name = c["name"].toString();
        auto field = [&](const QJsonObject& before, const QJsonObject& after, const char* key) {
            const QJsonValue v = after.contains(key) ? after[key] : before.contains(key) ? before[key] : defaults[key];
            return quint32(v.toDouble());
        };
        const QJsonObject before = c["before"].toObject();
        const QJsonObject after = c["after"].toObject();
        const QJsonObject none;

        Harness h;
        h.heartbeat(h.a);
        h.ctl.connectShell();
        h.nonce = h.a->last(Open).body;
        // Establish a live token at the client's last-seen revision, in the state its pending request was made in.
        const bool hasPending = qt.contains("pending_id");
        const quint8 kind = quint8(qt["kind"].toInt());
        quint8 startState = quint8(field(before, none, "state"));
        if (hasPending) {
            startState = (kind == BeginAuth) ? quint8(AuthRequired) : quint8(Ready);
        }
        const quint32 seen = hasPending ? quint32(qt["revision_seen"].toDouble(field(before, none, "revision")))
                                        : field(before, none, "revision");
        quint8 initialState = startState;
        if (!hasPending && startState != Locked && startState != CompanionShell::Closing) {
            initialState = AuthRequired;  // anything else is not a state a live token can be in before the case's event
        }
        State s0 = Harness::baseState(
            initialState, seen,
            hasPending ? quint32(qt["pending_id"].toDouble()) : field(before, none, "next_transaction"));
        s0.outputEpoch = field(before, none, "output_epoch");
        h.sendState(h.a, s0);
        QVERIFY2(h.ctl.sessionState() != CompanionShellController::Connecting, qPrintable(name));
        if (hasPending) {
            h.ctl._pending = {true, quint32(qt["pending_id"].toDouble()), kind, seen};
            h.ctl._transfer = {};
            h.ctl._transfer.data = QByteArray(int(field(before, none, "declared_length")), 'a');
            h.ctl._transfer.beginAcked = true;
            h.ctl._transfer.offset = h.ctl._transfer.data.size();
            h.ctl._transfer.current = Packet{Commit, kToken, h.ctl._pending.id, 0, {}};
            h.ctl._transfer.sentAt = h.now;
        }
        const quint32 rid = hasPending ? quint32(qt["pending_id"].toDouble()) : 0;

        // Feed exactly the packets the service emits for this case, in order.
        for (const QJsonValue& rv : c["reply"].toArray()) {
            const QJsonObject r = rv.toObject();
            if (r["packet"].toString() == QStringLiteral("ACK")) {
                const quint8 op = quint8(r["opcode"].toInt());
                const quint32 request = (op == Interrupt || op == Close) ? 1 : rid;
                h.sendAck(h.a, op, quint8(r["result"].toInt()), request, quint32(r["next_offset"].toInt()),
                          field(before, after, "next_transaction"), field(before, after, "output_epoch"),
                          field(before, after, "revision"));
            } else {
                State st;
                st.state = quint8(field(before, after, "state"));
                st.reason = quint8(r["reason"].toInt());
                st.revision = field(before, after, "revision");
                st.outputEpoch = field(before, after, "output_epoch");
                st.nextTransaction = field(before, after, "next_transaction");
                st.retryAfterSeconds = quint16(field(before, after, "retry_after_seconds"));
                st.lastTransaction = field(before, after, "last_transaction");
                st.lastKind = quint8(field(before, after, "last_kind"));
                st.lastResult = quint8(field(before, after, "last_result"));
                st.phase = quint8(field(before, after, "phase"));
                st.nextOffset = quint16(field(before, after, "next_offset"));
                h.sendState(h.a, st);
            }
        }
        QVERIFY2(enumName(h.ctl.lastResolution(), "PendingResolution") == qt["resolution"].toString(),
                 qPrintable(QStringLiteral("%1: resolution is %2, fixture says %3 (state %4, pending %5)")
                                .arg(name, enumName(h.ctl.lastResolution(), "PendingResolution"),
                                     qt["resolution"].toString(), enumName(h.ctl.sessionState(), "SessionState"),
                                     h.ctl.requestPending() ? "yes" : "no")));
        QVERIFY2(enumName(h.ctl.sessionState(), "SessionState") == qt["state"].toString(),
                 qPrintable(QStringLiteral("%1: state is %2, fixture says %3")
                                .arg(name, enumName(h.ctl.sessionState(), "SessionState"), qt["state"].toString())));
        ++exercised;
    }
    QVERIFY2(exercised >= 18,
             qPrintable(QStringLiteral("only %1 fixture cases carry a Qt expectation").arg(exercised)));
}

void CompanionShellControllerTest::_testQueryReconcilesUncertain()
{
    // Each of the five QUERY branches of the spec, through the real STATE path.
    struct Branch
    {
        quint32 lastTx;
        quint8 lastKind;
        quint8 lastResult;
        quint32 next;
        quint8 phase;
        CompanionShellController::PendingResolution expect;
        CompanionShellController::SessionState state;
    };

    const Branch branches[] = {
        {2, BeginLine, Accepted, 3, PhaseNone, CompanionShellController::Submitted, CompanionShellController::Ready},
        {2, BeginLine, CompanionShell::TimedOut, 3, PhaseNone, CompanionShellController::TimedOut,
         CompanionShellController::Ready},
        {0, 0, kNoResult, 2, PhaseReceiving, CompanionShellController::ResumeSameRequest,
         CompanionShellController::Uncertain},
        {0, 0, kNoResult, 2, PhaseWriting, CompanionShellController::WaitForWriter,
         CompanionShellController::Uncertain},
        {0, 0, kNoResult, 2, PhaseNone, CompanionShellController::Unknown, CompanionShellController::Uncertain},
        {3, BeginLine, Accepted, 4, PhaseNone, CompanionShellController::Unknown,
         CompanionShellController::Uncertain},  // replaced
    };
    for (const Branch& b : branches) {
        Harness h;
        h.open(h.a, Ready, 2);
        h.ctl.sendLine(QStringLiteral("make"));
        h.driveToCommit(h.a, 2);  // request id 2 is in flight
        State s = Harness::baseState(Ready, 5, b.next);
        s.lastTransaction = b.lastTx;
        s.lastKind = b.lastKind;
        s.lastResult = b.lastResult;
        s.phase = b.phase;
        h.sendState(h.a, s);
        QCOMPARE(h.ctl.lastResolution(), b.expect);
        QCOMPARE(h.ctl.sessionState(), b.state);
        QVERIFY(h.a->count(BeginLine) == 1);  // reconciling never allocates a new id
    }
    // INVALID_SESSION retires the token even when a restarted service reports revision 0.
    Harness h;
    h.open(h.a, Ready, 2);
    h.ctl.sendLine(QStringLiteral("make"));
    h.driveToCommit(h.a, 2);
    State invalid = Harness::baseState(InvalidSession, 0, 0);
    h.sendState(h.a, invalid);
    QCOMPARE(h.ctl.lastResolution(), CompanionShellController::UnknownRetireToken);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Uncertain);
    QVERIFY(!h.ctl.canSendLine());
    h.advance(5000);
    QCOMPARE(h.a->count(BeginLine), 1);  // never replayed into a new session
}

void CompanionShellControllerTest::_testPluginOwnsOneController()
{
    // The session must outlive any page: it is the plugin's, created once, and QML reaches it as
    // corePlugin.companionShell.
    CustomPlugin* const plugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    QVERIFY(plugin != nullptr);
    CompanionShellController* const controller = plugin->companionShell();
    QVERIFY2(controller != nullptr, "CustomPlugin::init() did not create the shell controller");
    QCOMPARE(plugin->companionShell(), controller);
    QCOMPARE(controller->parent(), static_cast<QObject*>(plugin));
    QCOMPARE(plugin->property("companionShell").value<CompanionShellController*>(), controller);  // what QML reads
    QCOMPARE(controller->sessionState(), CompanionShellController::Disconnected);
}

// =====================================================================================================================
// The page. It is loaded with the Harness's controller; the real LinkManager is never involved.
// =====================================================================================================================

namespace {

struct PageHost
{
    QQmlEngine engine;
    std::unique_ptr<QObject> host;
    QObject* page = nullptr;

    PageHost()
    {
        engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
        engine.addImportPath("qrc:/qml");
    }

    /// Loads the page the way AnalyzeView does: through a Loader, inside a context that provides `qgcPal`.
    /// Returns an empty string on success, else the QML error.
    QString load(CompanionShellController* shell)
    {
        QQmlComponent component(&engine);
        component.setData(R"QML(
import QtQuick
import QGroundControl
import QGroundControl.Controls
Item {
    id: host
    property var shell: null
    width: 900; height: 600
    QGCPalette { id: qgcPal }
    Loader {
        anchors.fill: parent
        Component.onCompleted: setSource("qrc:/qml/Custom/AgriDrone/CompanionShellPage.qml", {"shell": host.shell})
    }
}
)QML",
                          QUrl(QStringLiteral("qrc:/test/ShellPageHost.qml")));
        if (!component.isReady()) {
            return component.errorString();
        }
        host.reset(component.createWithInitialProperties({{"shell", QVariant::fromValue(shell)}}));
        page = host ? find(host.get(), QStringLiteral("companionShellPage")) : nullptr;
        return page ? QString() : QStringLiteral("page not created: ") + component.errorString();
    }

    static QObject* find(QObject* root, const QString& name)
    {
        if (root == nullptr) {
            return nullptr;
        }
        if (root->objectName() == name) {
            return root;
        }
        if (auto* item = qobject_cast<QQuickItem*>(root)) {
            for (QQuickItem* child : item->childItems()) {
                if (QObject* hit = find(child, name)) {
                    return hit;
                }
            }
        }
        for (QObject* child : root->children()) {
            if (QObject* hit = find(child, name)) {
                return hit;
            }
        }
        return nullptr;
    }

    QObject* item(const char* name) const { return find(page, QString::fromLatin1(name)); }

    bool enabled(const char* name) const { return item(name) && item(name)->property("enabled").toBool(); }

    void call(const char* method) { QMetaObject::invokeMethod(page, method); }
};

}  // namespace

void CompanionShellControllerTest::_testPasswordFieldNeverUsesOutputPrompt()
{
    Harness h;
    h.open(h.a);  // AUTH_REQUIRED
    PageHost host;
    const QString error = host.load(&h.ctl);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QObject* const password = host.item("shellPassword");
    QVERIFY(password != nullptr && host.item("shellCommand") != nullptr && host.item("shellOutput") != nullptr);
    QVERIFY(!host.enabled("shellPassword"));  // a reopened page waits for the service's answer first
    h.sendState(h.a, Harness::baseState(AuthRequired, 2));
    for (const char* name :
         {"shellState", "connectButton", "loginButton", "ctrlCButton", "clearButton", "disconnectButton"}) {
        QVERIFY2(host.item(name) != nullptr, name);
    }
    QCOMPARE(password->property("echoMode").toInt(), 2);  // TextInput.Password, unconditionally
    QVERIFY(host.enabled("shellPassword"));
    QVERIFY(!host.enabled("shellCommand"));               // no command line before login

    // Bash printing "Password:" is only text: it changes neither the mask nor the login state.
    h.sendOutput(h.a, 1, 0, "Password: \r\n");
    QVERIFY(host.item("shellOutput")->property("text").toString().contains(QStringLiteral("Password:")));
    QCOMPARE(password->property("echoMode").toInt(), 2);
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::AuthRequired);
    QVERIFY(!host.enabled("shellCommand"));

    // Submitting hands the password to the controller and empties the field at once.
    password->setProperty("text", QStringLiteral("hunter2"));
    host.call("submitLogin");
    QCOMPARE(h.a->count(BeginAuth), 1);
    QCOMPARE(h.a->last(BeginAuth).arg, quint32(7));
    QVERIFY(password->property("text").toString().isEmpty());
    QVERIFY(host.page->property("history").toList().isEmpty());  // the password is not a command
    QCOMPARE(password->property("echoMode").toInt(), 2);
}

void CompanionShellControllerTest::_testReopenPageQueriesSameController()
{
    Harness h;
    h.open(h.a, Ready, 2);
    {
        PageHost first;
        QVERIFY2(first.load(&h.ctl).isEmpty(), "page 1 did not load");
        QVERIFY(h.a->count(Query) >= 1);
        QVERIFY(!first.enabled("shellCommand"));  // waiting for the authoritative answer
        h.sendState(h.a, Harness::baseState(Ready, 2, 2));
        QVERIFY(first.enabled("shellCommand"));
    }  // the page is destroyed: the session is not
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);
    const int queries = h.a->count(Query);
    PageHost second;
    QVERIFY2(second.load(&h.ctl).isEmpty(), "page 2 did not load");
    QCOMPARE(h.a->count(Query), queries + 1);  // reopening asks the service, it does not guess
    QVERIFY(!second.enabled("shellCommand"));
    h.sendState(h.a, Harness::baseState(Ready, 3, 2));
    QVERIFY(second.enabled("shellCommand"));
    QCOMPARE(h.a->count(Open), 1);  // no new session and no new password
    QCOMPARE(h.a->count(BeginAuth), 0);
}

void CompanionShellControllerTest::_testClearPreservesAuth()
{
    Harness h;
    h.open(h.a, Ready, 2);
    h.sendOutput(h.a, 1, 0, "some output\r\n");
    PageHost host;
    QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
    h.sendState(h.a, Harness::baseState(Ready, 2, 2));
    QVERIFY(host.item("shellOutput")->property("text").toString().contains(QStringLiteral("some output")));
    QMetaObject::invokeMethod(host.item("clearButton"), "clicked");
    QVERIFY(host.item("shellOutput")->property("text").toString().isEmpty());
    QCOMPARE(h.ctl.sessionState(), CompanionShellController::Ready);  // Clear is display only
    QVERIFY(h.ctl.canSendLine());
    QCOMPARE(h.a->count(Close), 0);
    QCOMPARE(h.a->count(BeginAuth), 0);
}

void CompanionShellControllerTest::_testStateDisablesInput()
{
    struct Row
    {
        const char* what;
        bool command;
        bool password;
        bool ctrlC;
        bool disconnect;
    };

    auto check = [](Harness& h, PageHost& host, const Row& r) {
        QVERIFY2(host.enabled("shellCommand") == r.command, r.what);
        QVERIFY2(host.enabled("shellPassword") == r.password, r.what);
        QVERIFY2(host.enabled("ctrlCButton") == r.ctrlC, r.what);
        QVERIFY2(host.enabled("disconnectButton") == r.disconnect, r.what);
        QVERIFY2(host.enabled("connectButton") == (!h.ctl.hasSession() && h.ctl.online()), r.what);
    };
    {
        Harness h;
        h.heartbeat(h.a);
        PageHost host;
        QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
        check(h, host, {"disconnected", false, false, false, false});
        h.ctl.connectShell();
        check(h, host, {"connecting", false, false, false, true});
    }
    {
        Harness h;
        h.open(h.a);
        PageHost host;
        QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
        h.sendState(h.a, Harness::baseState(AuthRequired, 2));
        check(h, host, {"auth required", false, true, true, true});
        host.item("shellPassword")->setProperty("text", QStringLiteral("pw"));
        host.call("submitLogin");
        check(h, host, {"authenticating", false, false, true, true});
    }
    {
        Harness h;
        h.open(h.a, Ready, 2);
        PageHost host;
        QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
        h.sendState(h.a, Harness::baseState(Ready, 2, 2));
        check(h, host, {"ready", true, false, true, true});
        host.item("shellCommand")->setProperty("text", QStringLiteral("make"));
        host.call("submitCommand");
        h.driveToCommit(h.a, 2);
        check(h, host,
              {"request pending", false, false, true, true});  // one transaction at a time; Ctrl-C stays available
        for (int i = 0; i < 7; ++i) {
            h.advanceAlive(h.a, 1000);
        }
        QCOMPARE(h.ctl.sessionState(), CompanionShellController::Uncertain);
        check(h, host, {"uncertain", false, false, true, true});  // never type into an unknown state
    }
    {
        Harness h;
        h.open(h.a, Ready, 2);
        PageHost host;
        QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
        h.sendState(h.a, Harness::baseState(CompanionShell::Closing, 3, 2));
        QCOMPARE(h.ctl.sessionState(), CompanionShellController::Closing);
        check(h, host, {"closing", false, false, false, true});
    }
    {
        Harness h;
        h.heartbeat(h.a);
        h.ctl.connectShell();
        h.nonce = h.a->last(Open).body;
        State disabled = Harness::baseState(CompanionShell::Disabled, 0, 0);
        disabled.clientNonce = h.nonce;
        h.reply(h.a, Packet{StateOp, QByteArray(kSessionLen, '\0'), 0, 0, encodeState(disabled)});
        h.heartbeat(h.a);
        PageHost host;
        QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
        QCOMPARE(h.ctl.sessionState(), CompanionShellController::Disabled);
        check(h, host, {"disabled", false, false, false, false});
    }
}

void CompanionShellControllerTest::_testHistoryContainsCommandsOnly()
{
    Harness h;
    h.open(h.a, Ready, 2);
    PageHost host;
    QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
    quint32 revision = 2;
    quint32 next = 2;
    h.sendState(h.a, Harness::baseState(Ready, revision, next));
    for (int i = 0; i < 120; ++i) {  // more than the 100 the page keeps
        QVERIFY2(host.enabled("shellCommand"), qPrintable(QString::number(i)));
        host.item("shellCommand")->setProperty("text", QStringLiteral("cmd%1").arg(i));
        host.call("submitCommand");
        QVERIFY(host.item("shellCommand")->property("text").toString().isEmpty());  // accepted: the field is emptied
        const quint32 rid = h.driveToCommit(h.a, next);
        h.sendAck(h.a, Commit, Accepted, rid, 0, next + 1, 0, ++revision);
        h.sendState(h.a, Harness::baseState(Ready, ++revision, ++next));
    }
    const QVariantList history = host.page->property("history").toList();
    QCOMPARE(history.size(), 100);
    QCOMPARE(history.first().toString(), QStringLiteral("cmd20"));
    QCOMPARE(history.last().toString(), QStringLiteral("cmd119"));
    // A line the controller refuses (a control byte) neither empties the field nor enters the history.
    host.item("shellCommand")->setProperty("text", QStringLiteral("bad\nline"));
    host.call("submitCommand");
    QCOMPARE(host.page->property("history").toList().size(), 100);
    QCOMPARE(host.item("shellCommand")->property("text").toString(), QStringLiteral("bad\nline"));
    QVERIFY(!h.ctl.lastError().isEmpty());
}

void CompanionShellControllerTest::_testPageStateConstantsMatchEnum()
{
    // The page mirrors the enum numerically; if the enum is reordered this must fail instead of mislabelling states.
    Harness h;
    PageHost host;
    QVERIFY2(host.load(&h.ctl).isEmpty(), "page did not load");
    const QMetaEnum e = CompanionShellController::staticMetaObject.enumerator(
        CompanionShellController::staticMetaObject.indexOfEnumerator("SessionState"));
    const QPair<const char*, int> mirrored[] = {{"stDisconnected", CompanionShellController::Disconnected},
                                                {"stConnecting", CompanionShellController::Connecting},
                                                {"stAuthRequired", CompanionShellController::AuthRequired},
                                                {"stAuthenticating", CompanionShellController::Authenticating},
                                                {"stReady", CompanionShellController::Ready},
                                                {"stUncertain", CompanionShellController::Uncertain},
                                                {"stClosing", CompanionShellController::Closing},
                                                {"stDisabled", CompanionShellController::Disabled},
                                                {"stLocked", CompanionShellController::Locked},
                                                {"stBusy", CompanionShellController::Busy},
                                                {"stOffline", CompanionShellController::Offline}};
    QCOMPARE(e.keyCount(), int(std::size(mirrored)));
    for (const auto& m : mirrored) {
        QCOMPARE(host.page->property(m.first).toInt(), m.second);
    }
}

void CompanionShellControllerTest::_testAnalyzePagesIncludeShell()
{
    // 2026-10-19: the page is added to the stock Analyze list, not substituted for it, and needs no Vehicle.
    CustomPlugin* const plugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    QVERIFY(plugin != nullptr);
    const QVariantList pages = plugin->analyzePages();
    const QVariantList stock = plugin->QGCCorePlugin::analyzePages();
    QCOMPARE(pages.size(), stock.size() + 1);
    for (int i = 0; i < stock.size(); ++i) {
        QCOMPARE(pages.at(i), stock.at(i));  // every stock page is still there, in order
    }
    const auto* info = pages.last().value<QmlComponentInfo*>();
    QVERIFY(info != nullptr);
    QVERIFY(info->url().toString().endsWith(QStringLiteral("Custom/AgriDrone/CompanionShellPage.qml")));
    QVERIFY(!info->requiresVehicle());
    QCOMPARE(info->title(), QStringLiteral("Companion Shell"));
}
