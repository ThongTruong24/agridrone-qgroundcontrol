#include "CompanionShellProtocol.h"

#include <QtCore/QtEndian>
#include <cstring>

namespace CompanionShell {

namespace {

const QByteArray kZeroSession(kSessionLen, '\0');

quint32 rd32(const char* p)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(p));
}

quint16 rd16(const char* p)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(p));
}

void wr32(QByteArray& out, quint32 v)
{
    char b[4];
    qToLittleEndian(v, reinterpret_cast<uchar*>(b));
    out.append(b, 4);
}

void wr16(QByteArray& out, quint16 v)
{
    char b[2];
    qToLittleEndian(v, reinterpret_cast<uchar*>(b));
    out.append(b, 2);
}

}  // namespace

bool isValid(const Packet& p)
{
    if (p.sessionId.size() != kSessionLen || p.body.size() > kMaxBody) {
        return false;
    }
    const int n = p.body.size();
    const bool sessionSet = p.sessionId != kZeroSession;
    switch (p.opcode) {
        case Open:
            return !sessionSet && p.requestId == 0 && p.arg == 0 && n == kNonceLen;
        case Query:
            return p.requestId == 0 && p.arg == 0 && n == kNonceLen;
        case StateOp:
            return p.arg == 0 && p.requestId == 0 && n == kStateLen;
        case AckOp:
            return p.arg == 0 && n == kAckLen;
        default:
            break;
    }
    if (!sessionSet) {
        return false;
    }
    switch (p.opcode) {
        case BeginAuth:
            return p.arg >= 1 && p.arg <= static_cast<quint32>(kMaxPassword) && n == kDigestLen;
        case BeginLine:
            return p.arg <= static_cast<quint32>(kMaxLine) && n == kDigestLen;
        case Data:
        case Output:
            return n >= 1;
        case Commit:
        case Close:
            return p.arg == 0 && n == 0;
        case Interrupt:
            return n == 0;
        default:
            return false;
    }
}

QByteArray encode(const Packet& p)
{
    if (!isValid(p)) {
        return {};
    }
    QByteArray out;
    out.reserve(kHeaderLen + p.body.size());
    out.append("TS", 2);
    out.append(char(1));
    out.append(char(p.opcode));
    out.append(p.sessionId);
    wr32(out, p.requestId);
    wr32(out, p.arg);
    out.append(p.body);
    return out;
}

std::optional<Packet> decode(QByteArrayView data)
{
    if (data.size() < kHeaderLen || data.size() > kMaxCount || data[0] != 'T' || data[1] != 'S' || data[2] != 1) {
        return std::nullopt;
    }
    Packet p;
    p.opcode = static_cast<quint8>(data[3]);
    p.sessionId = QByteArray(data.constData() + 4, kSessionLen);
    p.requestId = rd32(data.constData() + 20);
    p.arg = rd32(data.constData() + 24);
    p.body = QByteArray(data.constData() + kHeaderLen, data.size() - kHeaderLen);
    return isValid(p) ? std::optional<Packet>(p) : std::nullopt;
}

QByteArray encodeState(const State& s)
{
    QByteArray out;
    out.append(s.clientNonce.leftJustified(kNonceLen, '\0', true));
    out.append(char(s.state));
    out.append(char(s.reason));
    wr32(out, s.revision);
    wr32(out, s.outputEpoch);
    wr32(out, s.nextTransaction);
    wr16(out, s.retryAfterSeconds);
    wr32(out, s.lastTransaction);
    out.append(char(s.lastKind));
    out.append(char(s.lastResult));
    out.append(char(s.phase));
    wr16(out, s.nextOffset);
    return out;
}

std::optional<State> decodeState(QByteArrayView body)
{
    if (body.size() != kStateLen) {
        return std::nullopt;
    }
    const char* p = body.constData();
    State s;
    s.clientNonce = QByteArray(p, kNonceLen);
    s.state = static_cast<quint8>(p[16]);
    s.reason = static_cast<quint8>(p[17]);
    s.revision = rd32(p + 18);
    s.outputEpoch = rd32(p + 22);
    s.nextTransaction = rd32(p + 26);
    s.retryAfterSeconds = rd16(p + 30);
    s.lastTransaction = rd32(p + 32);
    s.lastKind = static_cast<quint8>(p[36]);
    s.lastResult = static_cast<quint8>(p[37]);
    s.phase = static_cast<quint8>(p[38]);
    s.nextOffset = rd16(p + 39);
    return s;
}

QByteArray encodeAck(const Ack& a)
{
    QByteArray out;
    out.append(char(a.acknowledgedOpcode));
    out.append(char(a.result));
    wr32(out, a.nextOffset);
    wr32(out, a.nextTransaction);
    wr32(out, a.outputEpoch);
    wr32(out, a.revision);
    return out;
}

std::optional<Ack> decodeAck(QByteArrayView body)
{
    if (body.size() != kAckLen) {
        return std::nullopt;
    }
    const char* p = body.constData();
    Ack a;
    a.acknowledgedOpcode = static_cast<quint8>(p[0]);
    a.result = static_cast<quint8>(p[1]);
    a.nextOffset = rd32(p + 2);
    a.nextTransaction = rd32(p + 6);
    a.outputEpoch = rd32(p + 10);
    a.revision = rd32(p + 14);
    return a;
}

bool packRequest(uint8_t ourSystem, uint8_t ourComponent, uint8_t channel, uint8_t targetSystem, const Packet& packet,
                 mavlink_message_t* message)
{
    const QByteArray env = encode(packet);
    if (env.isEmpty() || message == nullptr) {
        return false;
    }
    uint8_t data[kMaxCount] = {};
    std::memcpy(data, env.constData(), static_cast<size_t>(env.size()));
    mavlink_msg_serial_control_pack_chan(ourSystem, ourComponent, channel, message, kDeviceShell, kRequestFlags, 0, 0,
                                         static_cast<uint8_t>(env.size()), data, targetSystem, kShellCompId);
    return true;
}

std::optional<Reply> decodeReply(const mavlink_message_t& message, uint8_t ourSystem, uint8_t ourComponent)
{
    if (message.msgid != MAVLINK_MSG_ID_SERIAL_CONTROL || message.compid != kShellCompId) {
        return std::nullopt;
    }
    mavlink_serial_control_t sc;
    mavlink_msg_serial_control_decode(&message, &sc);
    // Replies carry exactly REPLY; target extensions must name this GCS. Anything else is another client's traffic.
    if (sc.device != kDeviceShell || sc.flags != kFlagReply || sc.count > kMaxCount || sc.target_system != ourSystem ||
        sc.target_component != ourComponent) {
        return std::nullopt;
    }
    const auto packet = decode(QByteArrayView(reinterpret_cast<const char*>(sc.data), sc.count));
    if (!packet) {
        return std::nullopt;
    }
    return Reply{message.sysid, message.compid, *packet};
}

bool isShellHeartbeat(const mavlink_message_t& message)
{
    if (message.msgid != MAVLINK_MSG_ID_HEARTBEAT || message.compid != kShellCompId) {
        return false;
    }
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&message, &hb);
    return hb.type == MAV_TYPE_ONBOARD_CONTROLLER && hb.autopilot == MAV_AUTOPILOT_INVALID;
}

}  // namespace CompanionShell
