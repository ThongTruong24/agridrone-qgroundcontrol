#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <deque>

#include "QGCMAVLink.h"

class LinkConfiguration;

/// Per-hop bandwidth table (spec 2026-10-20-stream-bandwidth-design.md): CC_BANDWIDTH plus what this QGC receives.
/// Every number is the mean of the last 3 one-second samples, on both sides alike.
class CompanionBandwidth
{
public:
    static constexpr int kWindow = 3;
    static constexpr qint64 kSilenceMs = 3000;
    static constexpr qint64 kDuplicateMs = 500;

    struct Link {
        QString name;
        int endpoint = -1; ///< router udp index 0 (14550), 1 (14551), 2 (QGC_Direct), -1 unpaired
        quint64 bytes = 0; ///< cumulative bytes received on this link
    };

    static int endpointFor(const LinkConfiguration *config);

    void addCc(const mavlink_cc_bandwidth_t &sample, qint64 nowMs);
    /// Once a second: cumulative video bytes (-1 = backend cannot count) and every open link's cumulative rx bytes.
    void sampleLocal(qint64 nowMs, qint64 videoBytes, const QList<Link> &links);
    QVariantMap state(qint64 sessionStartMs, qint64 nowMs, float lossPercent) const;
    void resetCc();

private:
    template<typename F> double ccMeanOf(uint8_t bit, F get) const;
    static double mean(const std::deque<double> &values);

    std::deque<mavlink_cc_bandwidth_t> _cc;
    qint64 _lastCcMs = -1;
    qint64 _lastLocalMs = -1;
    qint64 _lastVideoBytes = -1;
    std::deque<double> _videoKbps;
    QHash<QString, quint64> _lastLinkBytes;
    QHash<QString, std::deque<double>> _linkRates;
    QList<Link> _links;
};
