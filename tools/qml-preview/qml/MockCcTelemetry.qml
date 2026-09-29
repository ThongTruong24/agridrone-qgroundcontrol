import QtQuick

// Raw preview data. Every snake_case property below is a field from the
// corresponding message in message_definitions/v1.0/thaco.xml.
QtObject {
    id: root

    property Timer _animationTimer: Timer {
        interval: 1000
        repeat: true
        running: root.animate

        onTriggered: {
            root._phase += 0.35;
            root.ccTelemetryLinks.fc_bytes_rx += 355612;
            root.ccTelemetryLinks.fc_bytes_tx += 126944;
            root.ccTelemetryLinks.fc_bitrate_kbps = 2845.7 + Math.sin(root._phase) * 185.0;
            root.ccTelemetryVision.inference_fps = 5.0 + Math.sin(root._phase * 0.7) * 0.4;
            root.ccTelemetryVision.detections_count = 5 + Math.round((Math.sin(root._phase) + 1) * 2);
        }
    }
    property real _phase: 0
    property bool animate: true
    readonly property bool cameraReceived: true
    readonly property bool cameraStale: false
    readonly property bool linksReceived: true
    readonly property bool linksStale: false
    readonly property bool networkReceived: true
    readonly property bool networkStale: false
    readonly property bool visionReceived: true
    readonly property bool visionStale: false
    readonly property QtObject ccTelemetryCamera: QtObject {
        property int bitrate_kbps: 2500
        property int bitrate_max_kbps: 3000
        property string camera_type: "realsense"
        property string codec: "h264"
        property int depth_fps: 30
        property int depth_height: 480
        property int depth_width: 640
        property int enable_emitter: 1
        property string encoder_mode: "hardware"
        property int profile_mode: 1
        property int rotation: 0
        property string rtsp_url: "rtsp://192.168.10.1:8554/live"
        property string serial_number: "239722071021"
        property int vbv_buffer_kb: 1250
        property int video_fps: 30
        property int video_height: 720
        property int video_width: 1280
    }
    readonly property QtObject ccTelemetryLinks: QtObject {
        // QML has no uint value type; real preserves the complete uint32 range.
        property real fc_baudrate: 921600
        property real fc_bitrate_kbps: 2845.7
        property real fc_bytes_rx: 184293744
        property real fc_bytes_tx: 92342881
        property string fc_port: "/dev/ttyAMA4"
        property int link_status_flags: 0x03
        property real siyi_baudrate: 115200
        property string siyi_port: "/dev/ttyAMA0"
    }
    readonly property QtObject ccTelemetryNetwork: QtObject {
        property int ap_channel: 6
        property int ap_client_count: 3
        property string ap_hw_mode: "g"
        property int ap_ieee80211n: 1
        property string ap_ip: "192.168.4.1"
        property string ap_key_mgmt: "WPA-PSK"
        property string ap_ssid: "AP_DRONE"
        property int ap_wmm_enabled: 1
        property int ap_wpa: 2
        property string ap_wpa_passphrase: "agridrone2026"
        property int dnsmasq_status: 1
        property string eth0_ip: "192.168.10.2"
        property int wlan0_dhcp: 1
        property string wlan0_ip: "10.42.0.24"
    }
    readonly property QtObject ccTelemetryVision: QtObject {
        property real confidence_thresh: 0.35
        property int detections_count: 7
        property real inference_fps: 5.0
        property int input_height: 360
        property string input_source: "realsense"
        property int input_width: 640
        property string model_name: "yolo26n.pt"
        property int status_flags: 0x03
        property int video_fps: 20
    }

    function reset() {
        ccTelemetryLinks.fc_bytes_rx = 184293744;
        ccTelemetryLinks.fc_bytes_tx = 92342881;
        ccTelemetryLinks.fc_bitrate_kbps = 2845.7;
        ccTelemetryLinks.link_status_flags = 0x03;
        ccTelemetryVision.inference_fps = 5.0;
        ccTelemetryVision.detections_count = 7;
        ccTelemetryVision.status_flags = 0x03;
        _phase = 0;
    }
}
