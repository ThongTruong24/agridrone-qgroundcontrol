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

<<<<<<< HEAD
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
=======
    Connections {
        target: CompanionUiAdapter
        function onNetworkChanged() {
            if (CompanionUiAdapter.hasNetworkTelemetry) {
                if (CompanionUiAdapter.apSsid !== "") root._draftApSsid = CompanionUiAdapter.apSsid
                if (CompanionUiAdapter.apChannel > 0) root._draftApChannel = CompanionUiAdapter.apChannel
                if (CompanionUiAdapter.eth0Ip !== "") root._draftEthIp = CompanionUiAdapter.eth0Ip
                if (CompanionUiAdapter.eth0Netmask !== "") root._draftEthNetmask = CompanionUiAdapter.eth0Netmask
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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
<<<<<<< HEAD
                    text: CompanionController.hasNetworkTelemetry ? ("" + CompanionController.apClientCount + " devices") : "--"
=======
                    text: CompanionUiAdapter.hasNetworkTelemetry ? ("" + CompanionUiAdapter.apClientCount + " devices") : "--"
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
            }

                QGCLabel {
<<<<<<< HEAD
                    text: CompanionController.hasNetworkTelemetry && CompanionController.apIp !== "" ? CompanionController.apIp : "--"
=======
                    text: CompanionUiAdapter.hasNetworkTelemetry && CompanionUiAdapter.apIp !== "" ? CompanionUiAdapter.apIp : "--"
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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

            QGCTextField {
                Layout.fillWidth: true
<<<<<<< HEAD
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? qsTr("Enter SSID") : qsTr("Waiting for telemetry...")
                text: root._draftApSsid
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftApSsid = text.trim() }
=======
                enabled: CompanionUiAdapter.hasNetworkTelemetry
                placeholderText: CompanionUiAdapter.hasNetworkTelemetry ? qsTr("Enter SSID") : qsTr("Waiting for telemetry...")
                text: root._draftApSsid
                onTextChanged: { if (CompanionUiAdapter.hasNetworkTelemetry) root._draftApSsid = text.trim() }
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
            }
        }

        // Password Field
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

                text: qsTr("Password:")
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
            }

                echoMode: TextInput.Password
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftApPass = text.trim() }
=======
                enabled: CompanionUiAdapter.hasNetworkTelemetry
                placeholderText: CompanionUiAdapter.hasNetworkTelemetry ? qsTr("Enter password") : qsTr("Waiting for telemetry...")
                text: root._draftApPass
                echoMode: TextInput.Password
                onTextChanged: { if (CompanionUiAdapter.hasNetworkTelemetry) root._draftApPass = text.trim() }
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
            }
        }

        // Channel Selector
        LabelledComboBox {
            Layout.fillWidth: true
            label: qsTr("WiFi Channel")
<<<<<<< HEAD
            enabled: CompanionController.hasNetworkTelemetry
            model: CompanionController.hasNetworkTelemetry
                   ? ["Channel 1 (2.412 GHz)", "Channel 6 (2.437 GHz)", "Channel 11 (2.462 GHz)"]
                   : ["--"]
            currentIndex: CompanionController.hasNetworkTelemetry
                          ? (root._draftApChannel === 1 ? 0 : (root._draftApChannel === 11 ? 2 : 1))
                          : 0
            onActivated: (index) => {
                if (CompanionController.hasNetworkTelemetry) {
=======
            enabled: CompanionUiAdapter.hasNetworkTelemetry
            model: CompanionUiAdapter.hasNetworkTelemetry
                   ? ["Channel 1 (2.412 GHz)", "Channel 6 (2.437 GHz)", "Channel 11 (2.462 GHz)"]
                   : ["--"]
            currentIndex: CompanionUiAdapter.hasNetworkTelemetry
                          ? (root._draftApChannel === 1 ? 0 : (root._draftApChannel === 11 ? 2 : 1))
                          : 0
            onActivated: (index) => {
                if (CompanionUiAdapter.hasNetworkTelemetry) {
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
                    root._draftApChannel = (index === 0) ? 1 : ((index === 2) ? 11 : 6)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
<<<<<<< HEAD
                text: qsTr("Apply Hotspot Config")
                primary: true
                enabled: CompanionController.hasNetworkTelemetry
<<<<<<< HEAD
                text: qsTr("Save Defaults")
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.saveDefaults()
            }

            QGCButton {
                text: qsTr("Restore Defaults")
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.restoreDefaults()
=======
                  text: qsTr("Save Defaults")
                  enabled: false
            }

            QGCButton {
                  text: qsTr("Restore Defaults")
                  enabled: false
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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
<<<<<<< HEAD
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftEthIp
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftEthIp = text.trim() }
=======
                enabled: CompanionUiAdapter.hasNetworkTelemetry
                placeholderText: CompanionUiAdapter.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftEthIp
                onTextChanged: { if (CompanionUiAdapter.hasNetworkTelemetry) root._draftEthIp = text.trim() }
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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
<<<<<<< HEAD
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "255.255.255.0" : qsTr("Waiting for telemetry...")
                text: root._draftEthNetmask
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftEthNetmask = text.trim() }
=======
                enabled: CompanionUiAdapter.hasNetworkTelemetry
                placeholderText: CompanionUiAdapter.hasNetworkTelemetry ? "255.255.255.0" : qsTr("Waiting for telemetry...")
                text: root._draftEthNetmask
                onTextChanged: { if (CompanionUiAdapter.hasNetworkTelemetry) root._draftEthNetmask = text.trim() }
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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
<<<<<<< HEAD
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftEthGateway
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftEthGateway = text.trim() }
=======
                enabled: CompanionUiAdapter.hasNetworkTelemetry
                placeholderText: CompanionUiAdapter.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftEthGateway
                onTextChanged: { if (CompanionUiAdapter.hasNetworkTelemetry) root._draftEthGateway = text.trim() }
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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
<<<<<<< HEAD
                enabled: CompanionController.hasNetworkTelemetry
                placeholderText: CompanionController.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftSiyiAirIp
                onTextChanged: { if (CompanionController.hasNetworkTelemetry) root._draftSiyiAirIp = text.trim() }
=======
                enabled: CompanionUiAdapter.hasNetworkTelemetry
                placeholderText: CompanionUiAdapter.hasNetworkTelemetry ? "192.168.xxx.xxx" : qsTr("Waiting for telemetry...")
                text: root._draftSiyiAirIp
                onTextChanged: { if (CompanionUiAdapter.hasNetworkTelemetry) root._draftSiyiAirIp = text.trim() }
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text: qsTr("Apply Ethernet Config")
<<<<<<< HEAD
                primary: true
                enabled: CompanionController.hasNetworkTelemetry
                onClicked: root.applyEthernet()
=======
                  primary: true
                  enabled: false
>>>>>>> 8b6f56d30ce7c6e10ee9813269705ecab608fdde
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
