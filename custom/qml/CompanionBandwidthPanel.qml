pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls

// Per-hop bandwidth (spec 2026-10-20-stream-bandwidth-design.md): CC and QGC numbers side by side, no verdict.
ColumnLayout {
    id: panel
    property var bw: ({})
    spacing: ScreenTools.defaultFontPixelHeight / 4
    // A NaN/Inf from a faulty CC shows as "—", never "NaN kbps".
    function bytes(v) { return typeof v !== "number" || !isFinite(v) ? "—" : v >= 1000 ? qsTr("%1 KB/s").arg((v / 1000).toFixed(1)) : qsTr("%1 B/s").arg(Math.round(v)) }
    function kbps(v) { return typeof v !== "number" || !isFinite(v) ? "—" : qsTr("%1 kbps").arg(Math.round(v)) }
    // Fixed-size Repeater models read their row through this, so a missing row shows "—" and delegates are not rebuilt every second.
    function at(list, i) { return list && list[i] ? list[i] : ({}) }
    function noData(owner) { return qsTr("no data (%1 not reporting)").arg(owner) }

    component Row3: RowLayout {
        property string a
        property string b
        property string c
        Layout.fillWidth: true
        QGCLabel { text: parent.a; Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14 }
        QGCLabel { text: parent.b; Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18 }
        QGCLabel { text: parent.c; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }

    QGCLabel { text: qsTr("Bandwidth (3 s mean)"); font.bold: true }
    QGCLabel {
        objectName: "bandwidthState"
        visible: panel.bw.ccState !== "ok"
        text: panel.bw.ccState === "unsupported" ? qsTr("CC does not support bandwidth or MAVLink definitions differ")
            : panel.bw.ccState === "waiting" ? qsTr("Waiting for CC bandwidth…") : qsTr("N/A")
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    ColumnLayout {
        visible: panel.bw.ccState === "ok"
        Layout.fillWidth: true
        Row3 { a: qsTr("Telemetry"); b: qsTr("CC sends"); c: qsTr("QGC receives") }
        Repeater {
            model: 3
            delegate: Row3 {
                required property int index
                readonly property var d: panel.at(panel.bw.telemetry, index)
                objectName: "bandwidthTelemetryRow" + index
                a: d.endpoint || "—"
                b: panel.bw.routerValid ? panel.bytes(d.ccTx) : panel.noData("edge-router")
                c: d.qgcLink ? qsTr("%1 (%2)").arg(panel.bytes(d.qgcRx)).arg(d.qgcLink) : qsTr("other client")
            }
        }
        QGCLabel { text: qsTr("Loss %1 % (all links)").arg(panel.bw.loss >= 0 ? panel.bw.loss.toFixed(1) : "—") }
        Row3 {
            objectName: "bandwidthCameraRow"
            a: qsTr("Encoder → MTX")
            b: panel.bw.cameraValid ? qsTr("%1 @ %2 fps").arg(panel.kbps(panel.bw.video.inKbps)).arg(isFinite(panel.bw.video.submitFps) ? panel.bw.video.submitFps.toFixed(1) : "—") : panel.noData("edge-camera")
            c: qsTr("target %1 kbps, %2 fps").arg(panel.bw.targetKbps || "—").arg(panel.bw.targetFps || "—")
        }
        Repeater {
            model: [qsTr("out uap0"), qsTr("out eth0 (SIYI)"), qsTr("out wlan0"), qsTr("out other")]
            delegate: Row3 {
                required property string modelData
                required property int index
                objectName: "bandwidthOutRow" + index
                a: modelData
                b: panel.bw.cameraValid ? panel.kbps(panel.at(panel.bw.video.out, index)) : panel.noData("edge-camera")
                c: index === 3 && panel.bw.cameraValid ? qsTr("readers %1").arg(panel.bw.video.readers) : ""
            }
        }
        Repeater {
            model: 3
            delegate: Row3 {
                required property int index
                readonly property var d: panel.at(panel.bw.interfaces, index)
                a: (d.name || "—") + qsTr(" tx/rx")
                b: panel.bw.networkValid ? qsTr("%1 / %2").arg(panel.kbps(d.txKbps)).arg(panel.kbps(d.rxKbps)) : panel.noData("edge-network")
                c: ""
            }
        }
    }

    // What this QGC receives: valid with or without a CC, so it sits outside the CC-gated table.
    QGCLabel { text: qsTr("This QGC"); font.bold: true }
    Row3 {
        objectName: "bandwidthQgcVideoRow"
        a: qsTr("This QGC receives")
        b: panel.kbps(panel.bw.video ? panel.bw.video.qgcRxKbps : undefined)
        c: panel.bw.ccState === "ok" ? qsTr("normally 1–3 % below CC (RTP overhead)") : ""
    }
    Repeater {
        model: 3
        delegate: Row3 {
            required property int index
            readonly property var d: panel.at(panel.bw.telemetry, index)
            visible: !!d.qgcLink
            a: d.qgcLink || ""
            b: panel.bytes(d.qgcRx)
            c: d.endpoint || ""
        }
    }
    Repeater {
        model: panel.bw.otherLinks ? panel.bw.otherLinks.length : 0
        delegate: Row3 {
            required property int index
            readonly property var d: panel.at(panel.bw.otherLinks, index)
            a: d.name || "—"
            b: panel.bytes(d.qgcRx)
            c: ""
        }
    }
}
