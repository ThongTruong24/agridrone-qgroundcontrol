import QtQuick

QtObject {
    readonly property var camera: source.ccTelemetryCamera
    readonly property bool cameraReceived: source.cameraReceived
    readonly property bool cameraStale: source.cameraStale
    readonly property var links: source.ccTelemetryLinks
    readonly property bool linksReceived: source.linksReceived
    readonly property bool linksStale: source.linksStale
    readonly property var network: source.ccTelemetryNetwork
    readonly property bool networkReceived: source.networkReceived
    readonly property bool networkStale: source.networkStale
    property bool previewMode: false
    required property var source
    readonly property var vision: source.ccTelemetryVision
    readonly property bool visionReceived: source.visionReceived
    readonly property bool visionStale: source.visionStale

    function accessPointIsOnline() {
        // The dialect has no AP-online field. Preserve the established Preview
        // visual only; QGC displays N/A rather than inferring runtime state.
        return previewMode && network.ap_ssid.length > 0 && network.dnsmasq_status === 1;
    }

    function activeText(value) {
        if (value === 1 || value === true)
            return qsTr("Active");
        if (value === 0 || value === false)
            return qsTr("Inactive");
        return qsTr("Unknown (%1)").arg(value);
    }

    function bitIsSet(value, bit) {
        return (value & (1 << bit)) !== 0;
    }

    function enabledText(value) {
        if (value === 1 || value === true)
            return qsTr("Enabled");
        if (value === 0 || value === false)
            return qsTr("Disabled");
        return qsTr("Unknown (%1)").arg(value);
    }

    function formatBaudrate(value) {
        return formatInteger(value) + " baud";
    }

    function formatInteger(value) {
        return Number(value).toLocaleString(Qt.locale("en_US"), "f", 0);
    }

    function fresh(received, stale) {
        return received && !stale;
    }

    function hexByte(value) {
        return "0x" + Number(value).toString(16).toUpperCase().padStart(2, "0");
    }

    function maskedSecret(value) {
        return value.length > 0 ? "\u2022".repeat(Math.min(value.length, 12)) : "--";
    }

    function messageState(received, stale) {
        if (!received)
            return qsTr("NO DATA");
        return stale ? qsTr("STALE") : qsTr("LIVE DATA");
    }

    function optionalText(received, value) {
        return received ? String(value) : "\u2014";
    }

    function profileMode(value) {
        const modes = ["RGB only", "RGB + depth", "RGB + depth + point cloud"];
        return value >= 0 && value < modes.length ? modes[value] : "Unknown (" + value + ")";
    }

    function resolution(width, height, fps) {
        return width + " x " + height + "  @  " + fps + " fps";
    }

    function rtspIsLive() {
        // No RTSP status exists in CC_TELEMETRY_CAMERA. QGC must not infer Live
        // from a non-empty URL; the derived state is Preview-only.
        return previewMode && camera.rtsp_url.length > 0;
    }

    function statusText(received, stale, active, activeLabel, inactiveLabel) {
        if (!received)
            return qsTr("NO DATA");
        if (stale)
            return qsTr("STALE");
        return active ? activeLabel : inactiveLabel;
    }
}
