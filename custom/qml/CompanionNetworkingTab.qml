import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

ColumnLayout {
    id: root

    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelHeight

    QGCPalette { id: qgcPal }

    // Hotspot state
    property string _draftApSsid:     ""
    property string _draftApPass:     ""
    property int    _draftApChannel:  0

    // Ethernet state
    property string _draftEthIp:      ""
    property string _draftEthNetmask: ""
    property string _draftEthGateway: ""
    property string _draftSiyiAirIp:  ""

    function applyHotspot() {
        if (!CompanionController.hasNetworkTelemetry) return
        CompanionController.sendNetworkConfig(root._draftApChannel, root._draftApSsid, root._draftApPass, root._draftEthIp)
        CompanionController.applyConfig(3, true)
    }

    function applyEthernet() {
        if (!CompanionController.hasNetworkTelemetry) return
        CompanionController.sendNetworkConfig(root._draftApChannel, root._draftApSsid, root._draftApPass, root._draftEthIp)
        CompanionController.applyConfig(3, true)
    }

    function saveDefaults() {
        if (!CompanionController.hasNetworkTelemetry) return
        CompanionController.saveDefaultConfig(3)
    }

    function restoreDefaults() {
        if (!CompanionController.hasNetworkTelemetry) return
        CompanionController.restoreDefaultConfig(3)
    }

    Connections {
        target: CompanionController
        function onNetworkChanged() {
            if (CompanionController.hasNetworkTelemetry) {
                if (CompanionController.apSsid !== "") root._draftApSsid = CompanionController.apSsid
                if (CompanionController.apChannel > 0) root._draftApChannel = CompanionController.apChannel
                if (CompanionController.eth0Ip !== "") root._draftEthIp = CompanionController.eth0Ip
                if (CompanionController.eth0Netmask !== "") root._draftEthNetmask = CompanionController.eth0Netmask
            } else {
                root._draftApSsid = ""
                root._draftApChannel = 0
                root._draftEthIp = ""
                root._draftEthNetmask = ""
            }
        }
    }

    // —— 1. WiFi Hotspot (Access Point) ————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("WiFi Hotspot (Access Point)")

        // Status Row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 2

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Hotspot Status"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasNetworkTelemetry
                          ? (CompanionController.apStatus === 2 ? qsTr("ACTIVE (UP)") : qsTr("OFFLINE"))
                          : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                    color: CompanionController.hasNetworkTelemetry && CompanionController.apStatus === 2 ? "#2ECC71" : qgcPal.text
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Connected Clients"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasNetworkTelemetry ? ("" + CompanionController.apClientCount + " devices") : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Gateway IP"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasNetworkTelemetry && CompanionController.apIp !== "" ? CompanionController.apIp : "--"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }
        }

        // SSID Field
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("SSID:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? qsTr("Enter SSID") : qsTr("Waiting for telemetry...")
                text: root._draftApSsid
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftApSsid = text.trim() }
            }
        }

        // Password Field
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("Password:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? qsTr("Enter password") : qsTr("Waiting for telemetry...")
                text: root._draftApPass
                echoMode: TextInput.Password
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftApPass = text.trim() }
            }
        }

        // Channel Selector
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("WiFi Channel")
            enabled: CompanionController.hasNetworkTelemetry
            model: CompanionController.hasNetworkTelemetry
                   ? ["Channel 1 (2.412 GHz)", "Channel 6 (2.437 GHz)", "Channel 11 (2.462 GHz)"]
                   : ["--"]
            currentIndex: CompanionController.hasNetworkTelemetry
                          ? (root._draftApChannel === 1 ? 0 : (root._draftApChannel === 11 ? 2 : 1))
                          : 0
            onActivated: (index) => {
                if (CompanionController.hasNetworkTelemetry) {
                    root._draftApChannel = (index === 0) ? 1 : ((index === 2) ? 11 : 6)
                }
            }
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Apply Hotspot Config")
                primary: true
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.applyHotspot()
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: qsTr("Save Defaults")
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.saveDefaults()
            }

            QGCButton {
                text: qsTr("Restore Defaults")
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.restoreDefaults()
            }
        }
    }

    // —— 2. Ethernet (eth0) & SIYI Air Unit Configuration ——————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Ethernet (eth0) & SIYI Air Unit Configuration")

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("IP Address:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftEthIp
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftEthIp = text.trim() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("Subnet Mask:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "255.255.255.0" : qsTr("Waiting for telemetry...")
                text: root._draftEthNetmask
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftEthNetmask = text.trim() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("Gateway:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftEthGateway
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftEthGateway = text.trim() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("SIYI Air Unit IP:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftSiyiAirIp
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftSiyiAirIp = text.trim() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Apply Ethernet Config")
                primary: true
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.applyEthernet()
            }

            Item { Layout.fillWidth: true }
        }
    }

    // —— 3. Network Activity Log ———————————————————————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Network Activity Log")

        CompanionLogPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 12
            filterPrefix: "NET:"
            serviceName: "Networking"
        }
    }
}
