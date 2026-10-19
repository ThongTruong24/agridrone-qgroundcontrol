#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <optional>

#include "MAVLinkLib.h"
#include "QGCMAVLink.h"

/// Shell envelope v1 carried inside SERIAL_CONTROL (device 10) between QGC and the companion host service.
/// Spec: docs/superpowers/specs/2026-10-19-mav-shell-design.md. Pure functions: no links, no timers, no QObject, so the
/// Python service and this code are held to the same golden vectors
/// (docs/superpowers/specs/fixtures/mav-shell-v1.json).
namespace CompanionShell {

inline constexpr quint8 kShellCompId = MAV_COMP_ID_ONBOARD_COMPUTER2;  // 192; system id is the vehicle's (1)
inline constexpr quint8 kDeviceShell = 10;
inline constexpr quint8 kFlagReply = 1;
inline constexpr quint8 kRequestFlags = 22;  // EXCLUSIVE | RESPOND | MULTI: the only flags a shell request carries
inline constexpr int kHeaderLen = 28;
inline constexpr int kMaxCount = 70;
inline constexpr int kMaxBody = kMaxCount - kHeaderLen;  // 42
inline constexpr int kSessionLen = 16;
inline constexpr int kNonceLen = 16;
inline constexpr int kDigestLen = 32;
inline constexpr int kStateLen = 41;
inline constexpr int kAckLen = 18;
inline constexpr int kMaxPassword = 128;
inline constexpr int kMaxLine = 1024;
inline constexpr quint8 kNoResult = 255;

enum Opcode : quint8
{
    Open = 1,
    Query = 2,
    BeginAuth = 3,
    BeginLine = 4,
    Data = 5,
    Commit = 6,
    Interrupt = 7,
    Close = 8,
    StateOp = 128,
    AckOp = 129,
    Output = 130
};

enum ServerState : quint8
{
    Idle = 0,
    AuthRequired = 1,
    Ready = 2,
    Closing = 3,
    Closed = 4,
    Disabled = 5,
    Locked = 6,
    Busy = 7,
    InvalidSession = 8
};

enum Reason : quint8
{
    ReasonNone = 0,
    AuthFailedReason = 1,
    BadRequest = 2,
    IdleTimeout = 3,
    LeaseExpired = 4,
    ConfigDisabled = 5,
    ShellExit = 6,
    StartFailedReason = 7,
    TransportFailure = 8,
    Interrupted = 9
};

enum AckResult : quint8
{
    Ok = 0,
    Incomplete = 1,
    Conflict = 2,
    Stale = 3,
    Invalid = 4,
    Accepted = 5,
    Cancelled = 6,
    AuthFailed = 7,
    TimedOut = 8,
    HashMismatch = 9,
    StartFailed = 10,
    WriteFailed = 11
};

enum Phase : quint8
{
    PhaseNone = 0,
    PhaseReceiving = 1,
    PhaseWriting = 2
};

struct Packet
{
    quint8 opcode = 0;
    QByteArray sessionId = QByteArray(kSessionLen, '\0');
    quint32 requestId = 0;
    quint32 arg = 0;
    QByteArray body;

    bool operator==(const Packet& other) const = default;
};

struct State
{
    QByteArray clientNonce = QByteArray(kNonceLen, '\0');
    quint8 state = Idle;
    quint8 reason = ReasonNone;
    quint32 revision = 0;
    quint32 outputEpoch = 0;
    quint32 nextTransaction = 0;
    quint16 retryAfterSeconds = 0;
    quint32 lastTransaction = 0;
    quint8 lastKind = 0;
    quint8 lastResult = kNoResult;
    quint8 phase = PhaseNone;
    quint16 nextOffset = 0;

    bool operator==(const State& other) const = default;
};

struct Ack
{
    quint8 acknowledgedOpcode = 0;
    quint8 result = 0;
    quint32 nextOffset = 0;
    quint32 nextTransaction = 0;
    quint32 outputEpoch = 0;
    quint32 revision = 0;

    bool operator==(const Ack& other) const = default;
};

/// Header/body contract per opcode. Anything else is malformed and must not touch any state.
bool isValid(const Packet& packet);
QByteArray encode(const Packet& packet);  // empty when !isValid
std::optional<Packet> decode(QByteArrayView data);

QByteArray encodeState(const State& state);
std::optional<State> decodeState(QByteArrayView body);
QByteArray encodeAck(const Ack& ack);
std::optional<Ack> decodeAck(QByteArrayView body);

/// A decoded inbound reply with the route it names; only admitted frames are returned.
struct Reply
{
    quint8 sysid = 0;
    quint8 compid = 0;
    Packet packet;
};

/// SERIAL_CONTROL request from this GCS to the shell service. False when the packet is not valid.
bool packRequest(uint8_t ourSystem, uint8_t ourComponent, uint8_t channel, uint8_t targetSystem, const Packet& packet,
                 mavlink_message_t* message);

/// SERIAL_CONTROL reply addressed to (ourSystem, ourComponent) from the shell component, else nullopt.
std::optional<Reply> decodeReply(const mavlink_message_t& message, uint8_t ourSystem, uint8_t ourComponent);

/// HEARTBEAT of the shell component (192, ONBOARD_CONTROLLER, autopilot INVALID).
bool isShellHeartbeat(const mavlink_message_t& message);

}  // namespace CompanionShell
