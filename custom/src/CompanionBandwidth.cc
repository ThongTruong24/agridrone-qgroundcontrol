#include "CompanionBandwidth.h"

#include <algorithm>

#include "UDPLink.h"

namespace {
void push(std::deque<double> &values, double v)
{
    values.push_back(v);
    while (values.size() > size_t(CompanionBandwidth::kWindow)) values.pop_front();
}
}

int CompanionBandwidth::endpointFor(const LinkConfiguration *config)
{
    const auto *udp = dynamic_cast<const UDPConfiguration*>(config);
    if (!udp) return -1;
    const auto hosts = udp->targetHosts();
    if (!hosts.isEmpty()) {
        const quint16 port = hosts.first()->port;
        return port == 14550 ? 0 : port == 14551 ? 1 : -1;
    }
    return udp->localPort() == 14550 ? 2 : -1; // router QGC_Direct sends to GCS_IP:14550
}

void CompanionBandwidth::addCc(const mavlink_cc_bandwidth_t &sample, qint64 nowMs)
{
    if (_lastCcMs >= 0 && nowMs - _lastCcMs < kDuplicateMs) return; // 1 Hz sender; duplicates arrive over a second link
    if (_lastCcMs >= 0 && nowMs - _lastCcMs > kSilenceMs) _cc.clear(); // pre-gap samples must not average with recovered ones
    _cc.push_back(sample);
    while (_cc.size() > size_t(kWindow)) _cc.pop_front();
    _lastCcMs = nowMs;
}

void CompanionBandwidth::resetCc()
{
    _cc.clear();
    _lastCcMs = -1;
}

void CompanionBandwidth::sampleLocal(qint64 nowMs, qint64 videoBytes, const QList<Link> &links)
{
    const qint64 dt = _lastLocalMs < 0 ? 0 : nowMs - _lastLocalMs;
    if (dt > 0) {
        if (videoBytes >= 0 && _lastVideoBytes >= 0) {
            push(_videoKbps, videoBytes < _lastVideoBytes ? 0.0 : double(videoBytes - _lastVideoBytes) * 8.0 / double(dt));
        }
        for (const Link &link : links) {
            if (_lastLinkBytes.contains(link.name)) {
                const quint64 last = _lastLinkBytes.value(link.name);
                push(_linkRates[link.name], link.bytes < last ? 0.0 : double(link.bytes - last) * 1000.0 / double(dt));
            }
        }
    }
    if (videoBytes < 0) _videoKbps.clear();
    _lastVideoBytes = videoBytes;
    _lastLocalMs = nowMs;
    _lastLinkBytes.clear();
    for (const Link &link : links) _lastLinkBytes.insert(link.name, link.bytes);
    for (auto it = _linkRates.begin(); it != _linkRates.end();) { // a link that left must not return with its old rate
        it = _lastLinkBytes.contains(it.key()) ? std::next(it) : _linkRates.erase(it);
    }
    _links = links;
}

double CompanionBandwidth::mean(const std::deque<double> &values)
{
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / double(values.size());
}

template<typename F>
double CompanionBandwidth::ccMeanOf(uint8_t bit, F get) const
{
    std::deque<double> values;
    for (const auto &s : _cc) {
        if (s.valid & bit) values.push_back(double(get(s)));
    }
    return mean(values);
}

QVariantMap CompanionBandwidth::state(qint64 sessionStartMs, qint64 nowMs, float lossPercent) const
{
    QVariantMap m;
    const bool fresh = _lastCcMs >= 0 && nowMs - _lastCcMs <= kSilenceMs;
    if (sessionStartMs < 0) m["ccState"] = QStringLiteral("noCc");
    else if (fresh) m["ccState"] = QStringLiteral("ok");
    else if (nowMs - std::max(sessionStartMs, _lastCcMs) >= kSilenceMs) m["ccState"] = QStringLiteral("unsupported");
    else m["ccState"] = QStringLiteral("waiting");

    const uint8_t latest = _cc.empty() ? 0 : _cc.back().valid;
    const bool ok = m["ccState"] == QStringLiteral("ok");
    m["routerValid"] = ok && (latest & CC_BANDWIDTH_VALID_ROUTER);
    m["cameraValid"] = ok && (latest & CC_BANDWIDTH_VALID_CAMERA);
    m["networkValid"] = ok && (latest & CC_BANDWIDTH_VALID_NETWORK);
    m["loss"] = lossPercent;

    static const QStringList kEndpoints = {QStringLiteral("UDP 14550"), QStringLiteral("UDP 14551"), QStringLiteral("QGC_Direct")};
    QVariantList telemetry;
    for (int i = 0; i < 3; ++i) {
        QVariantMap row{{"endpoint", kEndpoints[i]},
                        {"ccTx", ccMeanOf(CC_BANDWIDTH_VALID_ROUTER, [i](const auto &s) { return s.udp_tx_rate[i]; })},
                        {"ccRx", ccMeanOf(CC_BANDWIDTH_VALID_ROUTER, [i](const auto &s) { return s.udp_rx_rate[i]; })}};
        for (const Link &link : _links) {
            if (link.endpoint == i) {
                row["qgcLink"] = link.name;
                if (_linkRates.contains(link.name)) row["qgcRx"] = mean(_linkRates.value(link.name));
                break;
            }
        }
        telemetry.append(row);
    }
    m["telemetry"] = telemetry;
    QVariantList others;
    for (const Link &link : _links) {
        if (link.endpoint >= 0) continue;
        QVariantMap other{{"name", link.name}};
        if (_linkRates.contains(link.name)) other["qgcRx"] = mean(_linkRates.value(link.name));
        others.append(other);
    }
    m["otherLinks"] = others;

    QVariantList out;
    for (int i = 0; i < 4; ++i) out.append(ccMeanOf(CC_BANDWIDTH_VALID_CAMERA, [i](const auto &s) { return s.video_out_kbps[i]; }));
    QVariantMap video{{"inKbps", ccMeanOf(CC_BANDWIDTH_VALID_CAMERA, [](const auto &s) { return s.video_in_kbps; })},
                      {"submitFps", ccMeanOf(CC_BANDWIDTH_VALID_CAMERA, [](const auto &s) { return s.video_submit_fps; })},
                      {"readers", _cc.empty() ? 0 : int(_cc.back().video_readers)},
                      {"out", out}};
    if (!_videoKbps.empty()) video["qgcRxKbps"] = mean(_videoKbps);
    m["video"] = video;

    static const QStringList kIfaces = {QStringLiteral("uap0"), QStringLiteral("eth0 (SIYI)"), QStringLiteral("wlan0")};
    QVariantList interfaces;
    for (int i = 0; i < 3; ++i) {
        interfaces.append(QVariantMap{{"name", kIfaces[i]},
                                      {"rxKbps", ccMeanOf(CC_BANDWIDTH_VALID_NETWORK, [i](const auto &s) { return s.if_rx_kbps[i]; })},
                                      {"txKbps", ccMeanOf(CC_BANDWIDTH_VALID_NETWORK, [i](const auto &s) { return s.if_tx_kbps[i]; })}});
    }
    m["interfaces"] = interfaces;
    return m;
}
