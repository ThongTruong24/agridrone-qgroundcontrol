import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone
import QGC

/// Companion log viewer for controller events and Companion STATUSTEXT.
/// Displays newest-on-top with severity colour coding.
Item {
    id: root

    /// Case-insensitive category or message filter.
    /// Example: "CAM:", "FC:", "SIYI:", "PI:", "NET:", "VIS:", "MISSION:"
    property string filterPrefix: ""
    /// Display name for placeholder
    property string serviceName: "Service"

    // External log model — set this to override the internal one
    property var logModel: _internalLogModel
    // Active vehicle reference
    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    QGCPalette { id: qgcPal }

    // Colour palette adapted to QGCPalette
    readonly property color _accentColor:   qgcPal.buttonHighlight
    readonly property color _bgColor:       qgcPal.windowShade
    readonly property color _headerColor:   qgcPal.window
    readonly property color _textNormal:    qgcPal.text
    readonly property color _textWarn:      "#F0A500"
    readonly property color _textError:     "#FF6B6B"
    readonly property color _textDebug:     qgcPal.text
    readonly property color _borderColor:   qgcPal.windowShadeDark

    // Internal log model
    ListModel { id: _internalLogModel }

    function _matchesDisplayFilter(category, message) {
        return CompanionUiAdapter.logMatchesFilter(root.filterPrefix, category, message) &&
               CompanionUiAdapter.logMatchesFilter(customFilterField.text, category, message)
    }

    // Populate from controller history using the same filter as live entries.
    function _loadLogsFromController() {
        logModel.clear()
        var logs = CompanionUiAdapter.getLogHistory(root.filterPrefix)
        for (var i = 0; i < logs.length; i++) {
            var item = logs[i]
            if (!root._matchesDisplayFilter(item.category, item.message)) continue
            logModel.append({
                ts:       item.timestamp,
                severity: item.severity,
                text:     item.message
            })
        }
    }

    function _severityColor(sev) {
        if (sev <= 3) return root._textError
        if (sev === 4) return root._textWarn
        if (sev === 5) return "#3FB950" // Green for TX
        if (sev === 6) return "#1E8BC3" // Cyan/Blue for RX
        if (sev === 7) return root._textDebug
        return root._textNormal
    }

    function _severityLabel(sev) {
        switch (sev) {
            case 0: return "EMRG"
            case 1: return "ALRT"
            case 2: return "CRIT"
            case 3: return "ERR "
            case 4: return "WARN"
            case 5: return "TX  "
            case 6: return "RX  "
            case 7: return "DBUG"
            default: return "LOG "
        }
    }

    Rectangle {
        id: logCard
        anchors.fill:  parent
        color:         root._bgColor
        radius:        6
        border.color:  root._borderColor
        border.width:  1
        clip:          true

        // ── Card Header ───────────────────────────────────────────────────
        Rectangle {
            id: headerBar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: ScreenTools.defaultFontPixelHeight * 1.8
            color:  root._headerColor
            radius: 6

            // Square off bottom corners via overlap
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 6
                color:  root._headerColor
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 8 }
                spacing: 8

                Rectangle {
                    Layout.preferredWidth:  8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: root._activeVehicle ? root._accentColor : "#555"
                    Behavior on color { ColorAnimation { duration: 300 } }
                }
                QGCLabel {
                    text:  root.serviceName + " Logs"
                    color: root._textNormal
                    font.bold: true
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    text:  "(" + logModel.count + ")"
                    color: root._textDebug
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                Item { Layout.fillWidth: true }
                // Clear button
                Rectangle {
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.5
                    implicitWidth: clearRow.implicitWidth + 14
                    radius: 3
                    color: clearMa.containsMouse ? Qt.rgba(255/255, 107/255, 107/255, 0.25) : Qt.rgba(255/255, 255/255, 255, 0.08)
                    border.color: clearMa.containsMouse ? root._textError : root._borderColor
                    border.width: 1

                    RowLayout {
                        id: clearRow
                        anchors.centerIn: parent
                        spacing: 4
                        QGCColoredImage {
                            source: "/res/TrashDelete.svg"
                            width: ScreenTools.defaultFontPixelHeight * 0.85
                            height: width
                            color: clearMa.containsMouse ? root._textError : root._textDebug
                            fillMode: Image.PreserveAspectFit
                            sourceSize.height: height
                        }
                        QGCLabel {
                            text: qsTr("Clear Log")
                            color: clearMa.containsMouse ? root._textError : root._textNormal
                            font.bold: true
                            font.pointSize: ScreenTools.smallFontPointSize - 1
                        }
                    }
                    MouseArea {
                        id: clearMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            logModel.clear()
                            CompanionUiAdapter.clearLogHistory(root.filterPrefix)
                        }
                    }
                }
            }

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1
                color:  root._borderColor
            }
        }

        // ── Filter toolbar ────────────────────────────────────────────────
        Rectangle {
            id: filterBar
            anchors { top: headerBar.bottom; left: parent.left; right: parent.right }
            height: ScreenTools.defaultFontPixelHeight * 1.6
            color:  Qt.darker(root._headerColor, 1.1)

            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 8 }
                spacing: 6

                QGCLabel {
                    text: qsTr("Filter:")
                    color: root._textDebug
                    font.pointSize: ScreenTools.smallFontPointSize
                }

                TextField {
                    id: customFilterField
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.2
                    placeholderText: root.filterPrefix || "any text"
                    color: root._textNormal
                    font.pointSize: ScreenTools.smallFontPointSize
                    font.family: ScreenTools.fixedFontFamily
                    background: Rectangle {
                        color: root._bgColor
                        border.color: customFilterField.activeFocus ? root._accentColor : root._borderColor
                        border.width: 1
                        radius: 3
                    }
                    leftPadding: 6; topPadding: 2; bottomPadding: 2
                    onTextChanged: root._loadLogsFromController()
                }

                Item { Layout.fillWidth: true }

                // Auto-scroll toggle indicator
                Rectangle {
                    id: autoscrollBadge
                    property bool active: true
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.2
                    radius: 3
                    color: active ? Qt.rgba(30/255, 139/255, 195/255, 0.2) : "transparent"
                    border.color: active ? root._accentColor : root._borderColor
                    border.width: 1

                    QGCLabel {
                        anchors.centerIn: parent
                        text: autoscrollBadge.active ? qsTr("Auto ↓") : qsTr("Manual")
                        color: autoscrollBadge.active ? root._accentColor : root._textDebug
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: autoscrollBadge.active = !autoscrollBadge.active
                    }
                }
            }

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1
                color: root._borderColor
            }
        }

        // ── Log list ─────────────────────────────────────────────────────
        ListView {
            id: logListView
            anchors {
                top: filterBar.bottom; bottom: parent.bottom
                left: parent.left; right: parent.right
                margins: 4
            }
            clip:    true
            spacing: 2
            model:   logModel

            delegate: Rectangle {
                id: itemDelegate
                required property string ts
                required property int    severity
                required property string text
                required property int    index

                width:  logListView.width
                height: rowInner.implicitHeight + 4
                color:  (itemDelegate.index % 2 === 0) ? "transparent" : Qt.rgba(1, 1, 1, 0.02)
                radius: 2

                RowLayout {
                    id: rowInner
                    anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                    spacing: 8

                    // Timestamp
                    QGCLabel {
                        text:        itemDelegate.ts
                        color:       root._textDebug
                        font.family: ScreenTools.fixedFontFamily
                        font.pointSize: ScreenTools.smallFontPointSize
                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 8
                    }

                    // Severity tag badge
                    Rectangle {
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 5
                        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.1
                        radius: 3
                        color:  Qt.rgba(root._severityColor(itemDelegate.severity).r,
                                        root._severityColor(itemDelegate.severity).g,
                                        root._severityColor(itemDelegate.severity).b, 0.18)
                        border.color: root._severityColor(itemDelegate.severity)
                        border.width: 1

                        QGCLabel {
                            anchors.centerIn: parent
                            text:        root._severityLabel(itemDelegate.severity)
                            color:       root._severityColor(itemDelegate.severity)
                            font.family: ScreenTools.fixedFontFamily
                            font.pointSize: ScreenTools.smallFontPointSize
                            font.bold:   true
                        }
                    }

                    // Message text
                    QGCLabel {
                        text:        itemDelegate.text
                        color:       root._severityColor(itemDelegate.severity)
                        font.family: ScreenTools.fixedFontFamily
                        font.pointSize: ScreenTools.smallFontPointSize
                        wrapMode:  Text.WrapAtWordBoundaryOrAnywhere
                        Layout.fillWidth: true
                    }
                }
            }

            // Empty state
            QGCLabel {
                anchors.centerIn: parent
                visible:  logModel.count === 0
                text:     root._activeVehicle
                          ? qsTr("Listening for MAVLink traffic (%1)…").arg(root.serviceName)
                          : qsTr("No vehicle connected")
                color:    root._textDebug
                font.pointSize: ScreenTools.smallFontPointSize
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // Real-time MAVLink packet telemetry (TX and RX) from CompanionUiAdapter
    Connections {
        target: CompanionUiAdapter

        function onMavlinkLogMessage(category, direction, message, severity) {
            if (!root._matchesDisplayFilter(category, message)) return

            logModel.insert(0, {
                ts:       Qt.formatTime(new Date(), "hh:mm:ss.zzz"),
                severity: severity,
                text:     message
            })
            if (logModel.count > 500) logModel.remove(499, logModel.count - 499)
        }
    }

    Component.onCompleted: {
        root._loadLogsFromController()
    }
    onFilterPrefixChanged: root._loadLogsFromController()
}
