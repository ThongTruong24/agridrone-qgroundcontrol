
import QtQuick
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone
import QGC

/// Config panel that fetches and edits a service's YAML config via companion API.
Item {
    QGCPalette { id: qgcPal }
    id: root

    /// REST API base URL, e.g. "http://192.168.10.1:8080"
    property string apiBase:     "http://192.168.10.1:8080"
    /// Service name as known to companion-gcs-api, e.g. "drone-camera-rtsp"
    property string serviceName: ""
    /// Human-readable title
    property string serviceLabel:"Service"

    readonly property color _bgColor:      qgcPal.window
    readonly property color _headerColor:  qgcPal.windowShade
    readonly property color _borderColor:  qgcPal.windowShadeDark
    readonly property color _accentColor:  qgcPal.buttonHighlight
    readonly property color _successColor: qgcPal.colorGreen
    readonly property color _textNormal:   qgcPal.text
    readonly property color _textDebug:    qgcPal.text
    readonly property color _textError:    qgcPal.colorRed

    property bool   _loading:        false
    property bool   _saving:         false
    property string _statusMsg:      ""
    property bool   _statusIsError:  false
    property var    _configData:     ({})
    property var    _pendingChanges: ({})

    // Flatten nested JSON/YAML object to dot-separated keys
    function _flattenConfig(obj, prefix) {
        var res = {}
        prefix = prefix || ""
        for (var k in obj) {
            if (!obj.hasOwnProperty(k)) continue
            var fullKey = prefix ? prefix + "." + k : k
            var v = obj[k]
            if (v !== null && typeof v === "object" && !Array.isArray(v)) {
                var nested = _flattenConfig(v, fullKey)
                for (var nk in nested) {
                    res[nk] = nested[nk]
                }
            } else {
                res[fullKey] = v
            }
        }
        return res
    }

    function loadConfig() {
        if (!root.serviceName) return
        root._loading = true
        root._statusMsg = ""
        root._pendingChanges = {}

        var xhr = new XMLHttpRequest()
        xhr.open("GET", root.apiBase + "/v1/config/" + root.serviceName, true)
        xhr.timeout = 5000
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            root._loading = false
            if (xhr.status === 200) {
                var parsed = JSON.parse(xhr.responseText)
                root._configData = root._flattenConfig(parsed.config || {})
                configModel.clear()
                var keys = Object.keys(root._configData).sort()
                for (var i = 0; i < keys.length; i++) {
                    configModel.append({ key: keys[i], value: String(root._configData[keys[i]]) })
                }
                root._statusMsg = qsTr("Loaded %1 keys").arg(keys.length)
                root._statusIsError = false
            } else {
                root._statusMsg = xhr.status === 0
                    ? qsTr("Cannot reach companion API")
                    : qsTr("Error %1").arg(xhr.status)
                root._statusIsError = true
            }
        }
        xhr.send()
    }

    function applyConfig() {
        if (Object.keys(root._pendingChanges).length === 0) {
            root._statusMsg = qsTr("No changes to apply")
            root._statusIsError = false
            return
        }
        root._saving = true
        root._statusMsg = ""
        var xhr = new XMLHttpRequest()
        xhr.open("PUT", root.apiBase + "/v1/config/" + root.serviceName, true)
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.timeout = 10000
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            root._saving = false
            if (xhr.status === 200) {
                var parsed = JSON.parse(xhr.responseText)
                root._statusMsg = qsTr("Applied. Container: %1").arg(parsed.restart || "ok")
                root._statusIsError = false
                root._pendingChanges = {}
            } else {
                try {
                    var err = JSON.parse(xhr.responseText)
                    root._statusMsg = qsTr("Error: %1").arg(err.error || xhr.status)
                } catch(e) {
                    root._statusMsg = qsTr("Error %1").arg(xhr.status)
                }
                root._statusIsError = true
            }
        }
        xhr.send(JSON.stringify({ config: root._pendingChanges }))
    }

    function _onValueChanged(key, val) {
        root._pendingChanges[key] = val
    }

    Component.onCompleted: loadConfig()

    ListModel { id: configModel }

    Rectangle {
        anchors.fill:  parent
        color:         root._bgColor
        radius:        6
        border.color:  root._borderColor
        border.width:  1
        clip:          true

        // ── Header ───────────────────────────────────────────────────────
        Rectangle {
            id: cfgHeader
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: ScreenTools.defaultFontPixelHeight * 1.8
            color:  root._headerColor
            radius: 6
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 6; color: root._headerColor
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 8 }
                spacing: 8
                QGCLabel {
                    text: "⚙  " + root.serviceLabel + " Config"
                    color: root._textNormal; font.bold: true
                    font.pointSize: ScreenTools.smallFontPointSize
                    Layout.fillWidth: true
                }
                // Refresh
                Rectangle {
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight * 4
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.4
                    radius: 4
                    color: refreshMa.containsMouse ? Qt.rgba(30/255, 139/255, 195/255, 0.2) : "transparent"
                    border.color: root._accentColor; border.width: 1
                    QGCLabel {
                        anchors.centerIn: parent
                        text: root._loading ? qsTr("…") : qsTr("Refresh")
                        color: root._accentColor; font.pointSize: ScreenTools.smallFontPointSize
                    }
                    MouseArea {
                        id: refreshMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        enabled: !root._loading
                        onClicked: root.loadConfig()
                    }
                }
                // Apply
                Rectangle {
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight * 4
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.4
                    radius: 4
                    color: applyMa.containsMouse ? Qt.rgba(63/255, 185/255, 80/255, 0.2) : "transparent"
                    border.color: root._successColor; border.width: 1
                    QGCLabel {
                        anchors.centerIn: parent
                        text: root._saving ? qsTr("…") : qsTr("Apply")
                        color: root._successColor; font.pointSize: ScreenTools.smallFontPointSize
                    }
                    MouseArea {
                        id: applyMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        enabled: !root._saving
                        onClicked: root.applyConfig()
                    }
                }
            }
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root._borderColor
            }
        }

        // ── Status bar ───────────────────────────────────────────────────
        Rectangle {
            id: statusBar
            anchors { top: cfgHeader.bottom; left: parent.left; right: parent.right }
            height: root._statusMsg ? ScreenTools.defaultFontPixelHeight * 1.4 : 0
            color: root._statusIsError ? Qt.rgba(1,0.42,0.42,0.12) : Qt.rgba(0.25,0.73,0.31,0.12)
            visible: root._statusMsg !== ""
            QGCLabel {
                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 10 }
                text:  root._statusMsg
                color: root._statusIsError ? root._textError : root._successColor
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }

        // ── Config rows ──────────────────────────────────────────────────
        QGCFlickable {
            anchors {
                top: statusBar.bottom; bottom: parent.bottom
                left: parent.left; right: parent.right
                margins: 8
            }
            contentHeight: colLayout.implicitHeight
            clip: true

            ColumnLayout {
                id: colLayout
                width: parent.width
                spacing: 4

                // Loading indicator
                QGCLabel {
                    visible: root._loading
                    text:    qsTr("Loading config…")
                    color:   root._textDebug
                    font.pointSize: ScreenTools.smallFontPointSize
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                // Empty state
                QGCLabel {
                    visible: !root._loading && configModel.count === 0
                    text:    qsTr("No config loaded. Check companion API connection.")
                    color:   root._textDebug
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Repeater {
                    model: configModel
                    delegate: CompanionConfigRow {
                        required property string key
                        required property string value
                        Layout.fillWidth: true
                        configKey:   key
                        configValue: value
                        onConfigChanged: (k, v) => root._onValueChanged(k, v)
                    }
                }
            }
        }
    }
}
