import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

ColumnLayout {
    id: root

    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelHeight

    QGCPalette { id: qgcPal }

    // Palette & Theme tokens
    readonly property color _accentBlue:    qgcPal.buttonHighlight
    readonly property color _accentGreen:   qgcPal.colorGreen
    readonly property color _accentOrange:  qgcPal.colorOrange
    readonly property color _bgDark:        qgcPal.window
    readonly property color _bgCard:        qgcPal.windowShade
    readonly property color _bgHeader:      qgcPal.window
    readonly property color _borderColor:   qgcPal.windowShadeDark
    readonly property color _textNormal:    qgcPal.text
    readonly property color _textMuted:     qgcPal.text
    readonly property color _textError:     qgcPal.colorRed

    property string _searchFilter: ""

    Component.onCompleted: {
        // Automatically request live parameters if connected
        if (CompanionController.vehicleConnected && CompanionController.ccParameters.length > 0) {
            CompanionController.requestCcParameters()
        }
    }

    // ─── File Dialog for Parameter Backup & Restore ─────────────────────────
    QGCFileDialog {
        id: fileDialog
        title: qsTr("Companion Computer Parameters")
        folder: QGroundControl.settingsManager.appSettings.parameterSavePath
        nameFilters: [
            qsTr("JSON Parameter Files (*.json)"),
            qsTr("Configuration Files (*.params *.param)"),
            qsTr("All Files (*)")
        ]

        onAcceptedForSave: (file) => {
            var ok = CompanionController.exportCcParameters(file)
            close()
        }

        onAcceptedForLoad: (file) => {
            var ok = CompanionController.importCcParameters(file)
            close()
        }
    }

    // ─── 1. Control & Action Toolbar ─────────────────────────────────────────
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Tham số Companion Computer (Comp ID 191)")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            // Status Row: Connection & Pending Changes
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                // Status indicator dot
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: CompanionController.vehicleConnected ? root._accentGreen : qgcPal.windowShadeDark
                }

                QGCLabel {
                    text: CompanionController.vehicleConnected
                          ? qsTr("Companion Agent (ID 191) đang kết nối")
                          : qsTr("Companion Offline (Đang hiển thị mẫu tham số có sẵn)")
                    font.bold: true
                    color: CompanionController.vehicleConnected ? root._accentGreen : root._textMuted
                }

                Item { Layout.fillWidth: true }

                // Modified count badge
                Rectangle {
                    visible: CompanionController.ccModifiedParamCount > 0
                    radius: 4
                    color: root._accentOrange
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.5
                    Layout.preferredWidth: modLabel.width + 16

                    QGCLabel {
                        id: modLabel
                        anchors.centerIn: parent
                        text: qsTr("%1 tham số đã sửa").arg(CompanionController.ccModifiedParamCount)
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize
                        color: qgcPal.text
                    }
                }
            }

            // Action Buttons Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                // Refresh Button
                QGCButton {
                    text: CompanionController.ccParametersLoading
                          ? qsTr("Đang tải...")
                          : qsTr("Tải lại từ CC")
                    enabled: CompanionController.vehicleConnected && !CompanionController.ccParametersLoading
                    onClicked: CompanionController.requestCcParameters()
                }

                // Import from file
                QGCButton {
                    text: qsTr("Nạp từ File...")
                    onClicked: fileDialog.openForLoad()
                }

                // Export to file
                QGCButton {
                    text: qsTr("Sao lưu File...")
                    onClicked: fileDialog.openForSave()
                }

                Item { Layout.fillWidth: true }

                // Discard changes
                QGCButton {
                    text: qsTr("Hủy sửa đổi")
                    enabled: CompanionController.ccModifiedParamCount > 0
                    onClicked: CompanionController.resetAllModifiedCcParameters()
                }

                // Apply button (prominent primary action)
                QGCButton {
                    primary: true
                    text: CompanionController.ccModifiedParamCount > 0
                          ? qsTr("Áp dụng lên CC (%1)").arg(CompanionController.ccModifiedParamCount)
                          : qsTr("Áp dụng lên CC")
                    enabled: CompanionController.vehicleConnected && CompanionController.ccModifiedParamCount > 0
                    onClicked: CompanionController.saveModifiedCcParameters()
                }
            }

            // Search & Filter Box
            QGCTextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Tìm kiếm tham số theo mã, tên hoặc mô tả...")
                onTextChanged: root._searchFilter = text.trim().toLowerCase()
            }
        }
    }

    // ─── 2. Parameter Groups ─────────────────────────────────────────────────
    Repeater {
        id: groupsRepeater
        model: CompanionController.ccParameterGroups

        delegate: SettingsGroupLayout {
            id: grpLayout
            required property string modelData
            required property int index

            Layout.fillWidth: true
            heading: {
                if (modelData === "Telemetry") return qsTr("1. Telemetry & MAVLink Links (FC / SIYI)")
                if (modelData === "Camera")    return qsTr("2. Camera & Video Streaming")
                if (modelData === "Vision")    return qsTr("3. Vision AI & Tracking")
                if (modelData === "Network")   return qsTr("4. Networking & Wi-Fi Hotspot")
                return modelData
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: CompanionController.ccParameters

                    delegate: Item {
                        id: itemWrapper
                        required property var modelData

                        // Group filter and search filter
                        readonly property bool inGroup: (modelData.group === grpLayout.modelData)
                        readonly property bool matchesSearch: {
                            if (root._searchFilter === "") return true
                            var s = root._searchFilter
                            var n = String(modelData.name).toLowerCase()
                            var l = String(modelData.label).toLowerCase()
                            var d = String(modelData.description).toLowerCase()
                            return (n.indexOf(s) !== -1 || l.indexOf(s) !== -1 || d.indexOf(s) !== -1)
                        }

                        visible: inGroup && matchesSearch
                        Layout.fillWidth: true
                        implicitHeight: visible ? cardRect.implicitHeight : 0

                        Rectangle {
                            id: cardRect
                            anchors.fill: parent
                            radius: 6
                            color: root._bgCard
                            border.color: modelData.isModified ? root._accentOrange : root._borderColor
                            border.width: modelData.isModified ? 2 : 1
                            implicitHeight: contentRow.implicitHeight + 16

                            RowLayout {
                                id: contentRow
                                anchors {
                                    fill: parent
                                    margins: 10
                                }
                                spacing: 12

                                // Left Column: Meta, description, live vs default
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        spacing: 8

                                        // Parameter Name (ID)
                                        QGCLabel {
                                            text: modelData.name
                                            font.bold: true
                                            font.family: ScreenTools.fixedFontFamily
                                            font.pointSize: ScreenTools.defaultFontPointSize
                                            color: root._accentBlue
                                        }

                                        // Label
                                        QGCLabel {
                                            text: "• " + modelData.label
                                            font.bold: true
                                            color: root._textNormal
                                        }

                                        // Modified Badge
                                        Rectangle {
                                            visible: modelData.isModified
                                            radius: 3
                                            color: root._accentOrange
                                            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.2
                                            Layout.preferredWidth: modBadgeTxt.width + 8

                                            QGCLabel {
                                                id: modBadgeTxt
                                                anchors.centerIn: parent
                                                text: qsTr("Đã sửa")
                                                font.bold: true
                                                font.pointSize: ScreenTools.smallFontPointSize * 0.8
                                                color: qgcPal.text
                                            }
                                        }

                                        // Reboot Required Badge
                                        Rectangle {
                                            visible: modelData.reboot
                                            radius: 3
                                            color: qgcPal.windowShadeDark
                                            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.2
                                            Layout.preferredWidth: rbtBadgeTxt.width + 8

                                            QGCLabel {
                                                id: rbtBadgeTxt
                                                anchors.centerIn: parent
                                                text: qsTr("Cần khởi động lại")
                                                font.pointSize: ScreenTools.smallFontPointSize * 0.8
                                                color: qgcPal.colorOrange
                                            }
                                        }
                                    }

                                    // Description
                                    QGCLabel {
                                        Layout.fillWidth: true
                                        text: modelData.description
                                        color: root._textMuted
                                        font.pointSize: ScreenTools.smallFontPointSize
                                        wrapMode: Text.WordWrap
                                    }

                                    // Status Line: Live value on CC & Default value
                                    RowLayout {
                                        spacing: 16

                                        QGCLabel {
                                            text: qsTr("Thực tế trên CC: %1%2").arg(modelData.liveValue).arg(modelData.units ? " " + modelData.units : "")
                                            color: modelData.isLiveSynced ? root._accentGreen : root._textMuted
                                            font.pointSize: ScreenTools.smallFontPointSize * 0.85
                                        }

                                        QGCLabel {
                                            text: qsTr("Mặc định: %1%2").arg(modelData.defaultValue).arg(modelData.units ? " " + modelData.units : "")
                                            color: root._textMuted
                                            font.pointSize: ScreenTools.smallFontPointSize * 0.85
                                        }
                                    }
                                }

                                // Right Column: Value Editor Controls
                                RowLayout {
                                    spacing: 8
                                    Layout.alignment: Qt.AlignVCenter

                                    // Dropdown (if parameter has options list)
                                    QGCComboBox {
                                        id: optCombo
                                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 16
                                        visible: modelData.options && modelData.options.length > 0
                                        model: modelData.options ? modelData.options : []

                                        function syncVal() {
                                            if (!model || model.length === 0) return
                                            var cur = String(modelData.value)
                                            var idx = model.indexOf(cur)
                                            currentIndex = (idx >= 0 ? idx : 0)
                                        }

                                        Component.onCompleted: syncVal()
                                        onModelChanged: syncVal()

                                        onActivated: (index) => {
                                            CompanionController.stageCcParameter(modelData.name, model[index])
                                        }
                                    }

                                    // Text Field (for string, ports, IPs, or freeform numbers)
                                    QGCTextField {
                                        id: txtField
                                        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 16
                                        visible: !optCombo.visible
                                        text: String(modelData.value)
                                        font.family: ScreenTools.fixedFontFamily

                                        onEditingFinished: {
                                            CompanionController.stageCcParameter(modelData.name, text.trim())
                                        }
                                    }

                                    // Revert button (active if modified)
                                    QGCButton {
                                        text: qsTr("Hủy")
                                        visible: modelData.isModified
                                        onClicked: CompanionController.resetCcParameter(modelData.name)
                                    }

                                    // Reset to Default button
                                    QGCButton {
                                        text: qsTr("Mặc định")
                                        onClicked: CompanionController.resetCcParameterToDefault(modelData.name)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
