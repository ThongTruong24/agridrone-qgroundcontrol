import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

ColumnLayout {
    id: root
    objectName: "companionTelemetryTab"

    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelHeight

    QGCPalette { id: qgcPal }

    // User draft selection (independent of 1Hz telemetry updates)
    property string _selectedFcPort: ""
    property int    _selectedFcBaud: 0
    property bool   _userInteractedFc: false
    property bool   _fcInitialized: false

    property string _selectedSiyiPort: ""
    property int    _selectedSiyiBaud: 0
    property bool   _userInteractedSiyi: false
    property bool   _siyiInitialized: false
    property bool _fcAwaitingTelemetry: false
    property bool _siyiAwaitingTelemetry: false
    property string _applySide: ""
    property string _pendingFcPort: ""
    property int _pendingFcBaud: 0
    property string _pendingSiyiPort: ""
    property int _pendingSiyiBaud: 0

    function resetEditState() {
        root._selectedFcPort = ""
        root._selectedFcBaud = 0
        root._userInteractedFc = false
        root._fcInitialized = false
        root._selectedSiyiPort = ""
        root._selectedSiyiBaud = 0
        root._userInteractedSiyi = false
        root._siyiInitialized = false
        root._fcAwaitingTelemetry = false
        root._siyiAwaitingTelemetry = false
        root._applySide = ""
        root._pendingFcPort = ""
        root._pendingFcBaud = 0
        root._pendingSiyiPort = ""
        root._pendingSiyiBaud = 0
    }

    function sendFcDraft(port, baud) {
        root._pendingFcPort = port
        root._pendingFcBaud = baud
        root._pendingSiyiPort = CompanionUiAdapter.siyiPort
        root._pendingSiyiBaud = CompanionUiAdapter.siyiBaud
        root._userInteractedFc = true
        root._applySide = "fc"
        CompanionUiAdapter.applyFcLink(port, baud)
    }

    function sendSiyiDraft(port, baud) {
        root._pendingFcPort = CompanionUiAdapter.fcPort
        root._pendingFcBaud = CompanionUiAdapter.fcBaud
        root._pendingSiyiPort = port
        root._pendingSiyiBaud = baud
        root._userInteractedSiyi = true
        root._applySide = "siyi"
        CompanionUiAdapter.applySiyiLink(port, baud)
    }

    readonly property bool hasPendingFcChanges: (
        CompanionUiAdapter.hasLinksTelemetry &&
        ((root._selectedFcPort !== "" && root._selectedFcPort !== CompanionUiAdapter.fcPort) ||
         (root._selectedFcBaud > 0 && root._selectedFcBaud !== CompanionUiAdapter.fcBaud))
    )

    readonly property bool hasPendingSiyiChanges: (
        CompanionUiAdapter.hasLinksTelemetry &&
        ((root._selectedSiyiPort !== "" && root._selectedSiyiPort !== CompanionUiAdapter.siyiPort) ||
         (root._selectedSiyiBaud > 0 && root._selectedSiyiBaud !== CompanionUiAdapter.siyiBaud))
    )

    readonly property var availablePortsList: {
        var list = []
        if (CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.availablePorts && CompanionUiAdapter.availablePorts.length > 0) {
            for (var i = 0; i < CompanionUiAdapter.availablePorts.length; ++i) {
                var p = CompanionUiAdapter.availablePorts[i]
                if (list.indexOf(p) === -1) list.push(p)
            }
        }
        if (CompanionUiAdapter.fcPort && CompanionUiAdapter.fcPort !== "" && list.indexOf(CompanionUiAdapter.fcPort) === -1) {
            list.push(CompanionUiAdapter.fcPort)
        }
        if (CompanionUiAdapter.siyiPort && CompanionUiAdapter.siyiPort !== "" && list.indexOf(CompanionUiAdapter.siyiPort) === -1) {
            list.push(CompanionUiAdapter.siyiPort)
        }
        list.push(qsTr("Custom..."))
        return list
    }

    function formatRate(bytesPerSec) {
        if (!CompanionUiAdapter.hasLinksTelemetry || isNaN(bytesPerSec) || bytesPerSec === undefined) return "--"
        if (bytesPerSec < 1024) return bytesPerSec.toFixed(0) + " B/s"
        if (bytesPerSec < 1024 * 1024) return (bytesPerSec / 1024).toFixed(1) + " KB/s"
        return (bytesPerSec / (1024 * 1024)).toFixed(2) + " MB/s"
    }

    function formatBytes(bytes) {
        if (!CompanionUiAdapter.hasLinksTelemetry || isNaN(bytes) || bytes === undefined) return "--"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / (1024 * 1024)).toFixed(2) + " MB"
    }

    function formatDropRate(rate) {
        if (!CompanionUiAdapter.hasLinksTelemetry || isNaN(rate) || rate === undefined) return "--"
        return rate.toFixed(1) + "%"
    }

    function getStatusText(status) {
        if (!CompanionUiAdapter.hasLinksTelemetry) return "--"
        if (status === 2) return qsTr("ONLINE")
        if (status === 1) return qsTr("STANDBY")
        if (status === 3) return qsTr("ERROR")
        return qsTr("OFFLINE")
    }

    // —— 1. Cube Orange Plus / FC Telemetry Link Group —————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("Cube Orange Plus (FC) Telemetry Link")

        // Top Status Header: Live indicator & Transport protocol
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 1.5

            Rectangle {
                width:  ScreenTools.defaultFontPixelHeight * 0.7
                height: width
                radius: width / 2
                color:  CompanionUiAdapter.hasLinksTelemetry
                        ? (CompanionUiAdapter.fcStatus === 2 ? "#2ECC71" : (CompanionUiAdapter.fcStatus === 1 ? "#F39C12" : "#E74C3C"))
                        : "#7F8C8D"
            }

            QGCLabel {
                text: qsTr("Link Status: %1").arg(getStatusText(CompanionUiAdapter.fcStatus))
                font.bold: true
                color: CompanionUiAdapter.hasLinksTelemetry
                       ? (CompanionUiAdapter.fcStatus === 2 ? qgcPal.text : qgcPal.colorOrange)
                       : qgcPal.text
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 2 }

            QGCLabel {
                text: CompanionUiAdapter.hasLinksTelemetry ? qsTr("Transport: %1").arg(CompanionUiAdapter.transportProtocol) : qsTr("Transport: --")
                font.pointSize: ScreenTools.smallFontPointSize
                opacity: 0.8
            }

            Item { Layout.fillWidth: true }
        }

        // Port & Baud Configuration Row (Draft controls for Apply command)
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 1.5

            QGCLabel { text: qsTr("Port:"); font.bold: true }

            QGCComboBox {
                id: fcPortCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 22
                enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" && CompanionUiAdapter.configStatus !== "WaitingTelemetry" && root.availablePortsList.length > 1
                model: root.availablePortsList

                function syncToActive() {
                    if (root._userInteractedFc || CompanionUiAdapter.configStatus === "Applying") return
                    var target = CompanionUiAdapter.fcPort
                    if (!target || target === "") return
                    var idx = model ? model.indexOf(target) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    } else if (model && model.length > 0) {
                        currentIndex = model.length - 1
                        if (fcCustomPortField) fcCustomPortField.text = target
                    }
                }

                Component.onCompleted: syncToActive()
                onModelChanged: syncToActive()

                onActivated: (index) => {
                    root._userInteractedFc = true
                    if (index !== model.length - 1) {
                        root._selectedFcPort = model[index].split(" ")[0]
                    }
                }
            }

            QGCTextField {
                id: fcCustomPortField
                visible: fcPortCombo.currentIndex === fcPortCombo.model.length - 1 && CompanionUiAdapter.hasLinksTelemetry
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                text: root._selectedFcPort
                placeholderText: "/dev/tty..."
                onTextChanged: {
                    if (fcPortCombo.currentIndex === fcPortCombo.model.length - 1) {
                        root._selectedFcPort = text.trim()
                    }
                }
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth }

            QGCLabel { text: qsTr("Baudrate:"); font.bold: true }

            QGCComboBox {
                id: fcBaudCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" && CompanionUiAdapter.configStatus !== "WaitingTelemetry"
                model: ["921600", "115200", "57600", "460800", "230400"]

                function syncToActive() {
                    if (root._userInteractedFc || CompanionUiAdapter.configStatus === "Applying") return
                    var target = CompanionUiAdapter.fcBaud
                    if (!target || target <= 0) return
                    var idx = model ? model.indexOf(target.toString()) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    }
                }

                Component.onCompleted: syncToActive()
                onModelChanged: syncToActive()

                onActivated: (index) => {
                    root._userInteractedFc = true
                    root._selectedFcBaud = parseInt(model[index].split(" ")[0]) || 0
                }
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: CompanionUiAdapter.configStatus === "Applying" ? qsTr("Applying...") : qsTr("Apply FC Link")
                primary: true
                enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" && CompanionUiAdapter.configStatus !== "WaitingTelemetry"
                onClicked: {
                    var port = ""
                    if (fcPortCombo.currentIndex === fcPortCombo.model.length - 1) {
                        port = fcCustomPortField.text.trim()
                    } else if (root._userInteractedFc && root._selectedFcPort !== "") {
                        port = root._selectedFcPort
                    } else if (fcPortCombo.currentIndex >= 0 && fcPortCombo.currentIndex < fcPortCombo.model.length - 1) {
                        port = fcPortCombo.model[fcPortCombo.currentIndex].split(" ")[0]
                    } else {
                        port = CompanionUiAdapter.fcPort
                    }

                    var baud = 0
                    if (root._userInteractedFc && root._selectedFcBaud > 0) {
                        baud = root._selectedFcBaud
                    } else if (fcBaudCombo.currentIndex >= 0) {
                        baud = parseInt(fcBaudCombo.model[fcBaudCombo.currentIndex].split(" ")[0]) || CompanionUiAdapter.fcBaud
                    } else {
                        baud = CompanionUiAdapter.fcBaud
                    }

                    // Keep _userInteractedFc true during apply so incoming 1Hz telemetry does NOT revert user selection
                    root.sendFcDraft(port, baud)
                }
            }
        }

        // Live Telemetry & Rates Grid (Row 1: Active Port, Active Baud, TX Rate, RX Rate; Row 2: Total TX, Total RX, TX Errors, RX Loss)
        GridLayout {
            Layout.fillWidth: true
            columns: ScreenTools.isMobile ? 2 : 4
            rowSpacing: ScreenTools.defaultFontPixelHeight * 0.6
            columnSpacing: ScreenTools.defaultFontPixelWidth * 2

            // Row 1
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Port"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.fcPort !== "" ? CompanionUiAdapter.fcPort : "--"
                    font.bold: true
                    color: CompanionUiAdapter.hasLinksTelemetry ? "#2ECC71" : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Baudrate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.fcBaud > 0 ? (CompanionUiAdapter.fcBaud + " bps") : "--"
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionUiAdapter.fcTxRate); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionUiAdapter.fcRxRate); font.bold: true }
            }

            // Row 2
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total TX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionUiAdapter.fcBytesTx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total RX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionUiAdapter.fcBytesRx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Errors"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasLinksTelemetry && !isNaN(CompanionUiAdapter.fcTxErr) ? (CompanionUiAdapter.fcTxErr + " pkts") : "--"
                    font.bold: true
                    color: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.fcTxErr > 0 ? "#E74C3C" : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Loss / Drop"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: formatDropRate(CompanionUiAdapter.fcRxLoss)
                    font.bold: true
                    color: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.fcRxLoss > 2.0 ? "#E74C3C" : qgcPal.text
                }
            }
        }
    }

    // —— 2. SIYI Gimbal & Camera Telemetry Link Group ———————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("SIYI Gimbal & Camera Telemetry Link")

        // Top Status Header: Live indicator & Transport protocol
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 1.5

            Rectangle {
                width:  ScreenTools.defaultFontPixelHeight * 0.7
                height: width
                radius: width / 2
                color:  CompanionUiAdapter.hasLinksTelemetry
                        ? (CompanionUiAdapter.siyiStatus === 2 ? "#2ECC71" : (CompanionUiAdapter.siyiStatus === 1 ? "#F39C12" : "#E74C3C"))
                        : "#7F8C8D"
            }

            QGCLabel {
                text: qsTr("Link Status: %1").arg(getStatusText(CompanionUiAdapter.siyiStatus))
                font.bold: true
                color: CompanionUiAdapter.hasLinksTelemetry
                       ? (CompanionUiAdapter.siyiStatus === 2 ? qgcPal.text : qgcPal.colorOrange)
                       : qgcPal.text
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 2 }

            QGCLabel {
                text: CompanionUiAdapter.hasLinksTelemetry ? qsTr("Transport: %1").arg(CompanionUiAdapter.transportProtocol) : qsTr("Transport: --")
                font.pointSize: ScreenTools.smallFontPointSize
                opacity: 0.8
            }

            Item { Layout.fillWidth: true }
        }

        // Port & Baud Configuration Row (Draft controls for Apply command)
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth * 1.5

            QGCLabel { text: qsTr("Port:"); font.bold: true }

            QGCComboBox {
                id: siyiPortCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 22
                enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" && CompanionUiAdapter.configStatus !== "WaitingTelemetry" && root.availablePortsList.length > 1
                model: root.availablePortsList

                function syncToActive() {
                    if (root._userInteractedSiyi || CompanionUiAdapter.configStatus === "Applying") return
                    var target = CompanionUiAdapter.siyiPort
                    if (!target || target === "") return
                    var idx = model ? model.indexOf(target) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    } else if (model && model.length > 0) {
                        currentIndex = model.length - 1
                        if (siyiCustomPortField) siyiCustomPortField.text = target
                    }
                }

                Component.onCompleted: syncToActive()
                onModelChanged: syncToActive()

                onActivated: (index) => {
                    root._userInteractedSiyi = true
                    if (index !== model.length - 1) {
                        root._selectedSiyiPort = model[index].split(" ")[0]
                    }
                }
            }

            QGCTextField {
                id: siyiCustomPortField
                visible: siyiPortCombo.currentIndex === siyiPortCombo.model.length - 1 && CompanionUiAdapter.hasLinksTelemetry
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                text: root._selectedSiyiPort
                placeholderText: "/dev/tty..."
                onTextChanged: {
                    if (siyiPortCombo.currentIndex === siyiPortCombo.model.length - 1) {
                        root._selectedSiyiPort = text.trim()
                    }
                }
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth }

            QGCLabel { text: qsTr("Baudrate:"); font.bold: true }

            QGCComboBox {
                id: siyiBaudCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" && CompanionUiAdapter.configStatus !== "WaitingTelemetry"
                model: ["115200", "57600", "921600", "460800", "230400"]

                function syncToActive() {
                    if (root._userInteractedSiyi || CompanionUiAdapter.configStatus === "Applying") return
                    var target = CompanionUiAdapter.siyiBaud
                    if (!target || target <= 0) return
                    var idx = model ? model.indexOf(target.toString()) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    }
                }

                Component.onCompleted: syncToActive()
                onModelChanged: syncToActive()

                onActivated: (index) => {
                    root._userInteractedSiyi = true
                    root._selectedSiyiBaud = parseInt(model[index].split(" ")[0]) || 0
                }
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: CompanionUiAdapter.configStatus === "Applying" ? qsTr("Applying...") : qsTr("Apply SIYI Link")
                primary: true
                enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" && CompanionUiAdapter.configStatus !== "WaitingTelemetry"
                onClicked: {
                    var port = ""
                    if (siyiPortCombo.currentIndex === siyiPortCombo.model.length - 1) {
                        port = siyiCustomPortField.text.trim()
                    } else if (root._userInteractedSiyi && root._selectedSiyiPort !== "") {
                        port = root._selectedSiyiPort
                    } else if (siyiPortCombo.currentIndex >= 0 && siyiPortCombo.currentIndex < siyiPortCombo.model.length - 1) {
                        port = siyiPortCombo.model[siyiPortCombo.currentIndex].split(" ")[0]
                    } else {
                        port = CompanionUiAdapter.siyiPort
                    }

                    var baud = 0
                    if (root._userInteractedSiyi && root._selectedSiyiBaud > 0) {
                        baud = root._selectedSiyiBaud
                    } else if (siyiBaudCombo.currentIndex >= 0) {
                        baud = parseInt(siyiBaudCombo.model[siyiBaudCombo.currentIndex].split(" ")[0]) || CompanionUiAdapter.siyiBaud
                    } else {
                        baud = CompanionUiAdapter.siyiBaud
                    }

                    // Keep _userInteractedSiyi true during apply so incoming 1Hz telemetry does NOT revert user selection
                    root.sendSiyiDraft(port, baud)
                }
            }
        }

        // Live Telemetry & Rates Grid (Row 1: Active Port, Active Baud, TX Rate, RX Rate; Row 2: Total TX, Total RX, TX Errors, RX Loss)
        GridLayout {
            Layout.fillWidth: true
            columns: ScreenTools.isMobile ? 2 : 4
            rowSpacing: ScreenTools.defaultFontPixelHeight * 0.6
            columnSpacing: ScreenTools.defaultFontPixelWidth * 2

            // Row 1
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Port"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.siyiPort !== "" ? CompanionUiAdapter.siyiPort : "--"
                    font.bold: true
                    color: CompanionUiAdapter.hasLinksTelemetry ? "#2ECC71" : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Baudrate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.siyiBaud > 0 ? (CompanionUiAdapter.siyiBaud + " bps") : "--"
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionUiAdapter.siyiTxRate); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionUiAdapter.siyiRxRate); font.bold: true }
            }

            // Row 2
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total TX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionUiAdapter.siyiBytesTx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total RX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionUiAdapter.siyiBytesRx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Errors"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionUiAdapter.hasLinksTelemetry && !isNaN(CompanionUiAdapter.siyiTxErr) ? (CompanionUiAdapter.siyiTxErr + " pkts") : "--"
                    font.bold: true
                    color: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.siyiTxErr > 0 ? "#E74C3C" : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Loss / Drop"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: formatDropRate(CompanionUiAdapter.siyiRxLoss)
                    font.bold: true
                    color: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.siyiRxLoss > 2.0 ? "#E74C3C" : qgcPal.text
                }
            }
        }
    }

    // —— 3. MAVLink Command & Event Audit Log Group —————————————————————
    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("MAVLink Command & Event Audit Log")

        CompanionLogPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 14
            filterPrefix: ""
            serviceName: "MAVLink"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        QGCLabel { text: CompanionUiAdapter.configStatus + ": " + CompanionUiAdapter.configMessage }
        Item { Layout.fillWidth: true }
        QGCButton {
            objectName: "saveUartButton"
            text: qsTr("Save UART defaults")
            enabled: CompanionUiAdapter.hasLinksTelemetry && CompanionUiAdapter.configStatus !== "Applying" &&
                     CompanionUiAdapter.configStatus !== "WaitingTelemetry" &&
                     !root.hasPendingFcChanges && !root.hasPendingSiyiChanges
            onClicked: CompanionUiAdapter.saveDefaultConfig(1)
        }
    }

    // Keep draft values until fresh Companion telemetry confirms the applied values.
    Connections {
        target: CompanionUiAdapter

        function onCommandAckReceived(command, result, text) {
            if (command === 44011 && result === 0) {
                root._fcAwaitingTelemetry = root._applySide === "fc"
                root._siyiAwaitingTelemetry = root._applySide === "siyi"
            }
        }

        function onConfigStatusChanged() {
            if (CompanionUiAdapter.configStatus === "Success" && root._fcAwaitingTelemetry &&
                CompanionUiAdapter.fcPort === root._pendingFcPort && CompanionUiAdapter.fcBaud === root._pendingFcBaud) {
                    root._fcAwaitingTelemetry = false
                    root._userInteractedFc = false
                    root._selectedFcPort = ""
                    root._selectedFcBaud = 0
                    root._applySide = ""
                    fcPortCombo.syncToActive()
                    fcBaudCombo.syncToActive()
            }
            if (CompanionUiAdapter.configStatus === "Success" && root._siyiAwaitingTelemetry &&
                CompanionUiAdapter.siyiPort === root._pendingSiyiPort && CompanionUiAdapter.siyiBaud === root._pendingSiyiBaud) {
                    root._siyiAwaitingTelemetry = false
                    root._userInteractedSiyi = false
                    root._selectedSiyiPort = ""
                    root._selectedSiyiBaud = 0
                    root._applySide = ""
                    siyiPortCombo.syncToActive()
                    siyiBaudCombo.syncToActive()
            }
            if (CompanionUiAdapter.configStatus === "Failed" || CompanionUiAdapter.configStatus === "Timeout") {
                root._fcAwaitingTelemetry = false
                root._siyiAwaitingTelemetry = false
                root._applySide = ""
            }
        }

        function onVehicleEpochChanged() {
            root.resetEditState()
        }

        function onLinksChanged() {
            if (CompanionUiAdapter.hasLinksTelemetry) {
                if (!root._fcInitialized && CompanionUiAdapter.fcPort !== "") {
                    root._fcInitialized = true
                    fcPortCombo.syncToActive()
                    fcBaudCombo.syncToActive()
                }
                if (!root._siyiInitialized && CompanionUiAdapter.siyiPort !== "") {
                    root._siyiInitialized = true
                    siyiPortCombo.syncToActive()
                    siyiBaudCombo.syncToActive()
                }
            }
        }
    }
}
