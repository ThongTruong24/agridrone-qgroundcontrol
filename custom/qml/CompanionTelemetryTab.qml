import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

ColumnLayout {
    id: root
    property bool   _fcAwaitingTelemetry: false
    property bool   _siyiAwaitingTelemetry: false
    property string _applySide: ""
    property string _pendingFcPort: ""
    property int    _pendingFcBaud: 0
    property string _pendingSiyiPort: ""
    property int    _pendingSiyiBaud: 0

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
        root._pendingSiyiPort = CompanionController.siyiPort
        root._pendingSiyiBaud = CompanionController.siyiBaud
        root._userInteractedFc = true
        root._applySide = "fc"
        CompanionController.applyFcLink(port, baud)
        return true
    }

    function sendSiyiDraft(port, baud) {
        root._pendingFcPort = CompanionController.fcPort
        root._pendingFcBaud = CompanionController.fcBaud
        root._pendingSiyiPort = port
        root._pendingSiyiBaud = baud
        root._userInteractedSiyi = true
        root._applySide = "siyi"
        CompanionController.applySiyiLink(port, baud)
        return true
    }

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

    readonly property bool hasPendingFcChanges: (
        CompanionController.hasLinksTelemetry &&
        ((root._selectedFcPort !== "" && root._selectedFcPort !== CompanionController.fcPort) ||
         (root._selectedFcBaud > 0 && root._selectedFcBaud !== CompanionController.fcBaud))
    )

    readonly property bool hasPendingSiyiChanges: (
        CompanionController.hasLinksTelemetry &&
        ((root._selectedSiyiPort !== "" && root._selectedSiyiPort !== CompanionController.siyiPort) ||
         (root._selectedSiyiBaud > 0 && root._selectedSiyiBaud !== CompanionController.siyiBaud))
    )

    readonly property var availablePortsList: {
        var list = []
        if (CompanionController.hasLinksTelemetry && CompanionController.availablePorts && CompanionController.availablePorts.length > 0) {
            for (var i = 0; i < CompanionController.availablePorts.length; ++i) {
                var p = CompanionController.availablePorts[i]
                if (list.indexOf(p) === -1) list.push(p)
            }
        }
        if (CompanionController.fcPort && CompanionController.fcPort !== "" && list.indexOf(CompanionController.fcPort) === -1) {
            list.push(CompanionController.fcPort)
        }
        if (CompanionController.siyiPort && CompanionController.siyiPort !== "" && list.indexOf(CompanionController.siyiPort) === -1) {
            list.push(CompanionController.siyiPort)
        }
        list.push(qsTr("Custom..."))
        return list
    }

    function formatRate(bytesPerSec) {
        if (!CompanionController.hasLinksTelemetry || isNaN(bytesPerSec) || bytesPerSec === undefined) return "--"
        if (bytesPerSec < 1024) return bytesPerSec.toFixed(0) + " B/s"
        if (bytesPerSec < 1024 * 1024) return (bytesPerSec / 1024).toFixed(1) + " KB/s"
        return (bytesPerSec / (1024 * 1024)).toFixed(2) + " MB/s"
    }

    function formatBytes(bytes) {
        if (!CompanionController.hasLinksTelemetry || isNaN(bytes) || bytes === undefined) return "--"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / (1024 * 1024)).toFixed(2) + " MB"
    }

    function formatDropRate(rate) {
        if (!CompanionController.hasLinksTelemetry || isNaN(rate) || rate === undefined) return "--"
        return rate.toFixed(1) + "%"
    }

    function getStatusText(status) {
        if (!CompanionController.hasLinksTelemetry) return "--"
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
                color:  CompanionController.hasLinksTelemetry
                        ? (CompanionController.fcStatus === 2 ? qgcPal.colorGreen : (CompanionController.fcStatus === 1 ? qgcPal.colorOrange : qgcPal.colorRed))
                        : qgcPal.windowShadeDark
            }

            QGCLabel {
                text: qsTr("Link Status: %1").arg(getStatusText(CompanionController.fcStatus))
                font.bold: true
                color: CompanionController.hasLinksTelemetry
                       ? (CompanionController.fcStatus === 2 ? qgcPal.text : qgcPal.colorOrange)
                       : qgcPal.text
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 2 }

            QGCLabel {
                text: CompanionController.hasLinksTelemetry ? qsTr("Transport: %1").arg(CompanionController.transportProtocol) : qsTr("Transport: --")
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
                enabled: CompanionController.hasLinksTelemetry && CompanionController.configStatus !== "APPLYING" && root.availablePortsList.length > 1
                model: root.availablePortsList

                function syncToActive() {
                    if (root._userInteractedFc || CompanionController.configStatus === "APPLYING") return
                    var target = CompanionController.fcPort
                    if (!target || target === "") return
                    var idx = model ? model.indexOf(target) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    } else if (model && model.length > 0) {
                        currentIndex = model.length - 1
                        if (fcCustomPortField) fcCustomPortField.text = target
                    }
                }

                Component.onCompleted: {
                    if (CompanionController.hasLinksTelemetry && CompanionController.fcPort !== "") {
                        syncToActive()
                        root._fcInitialized = true
                    }
                }
                onModelChanged: {
                    if (!root._fcInitialized) {
                        syncToActive()
                    }
                }

                onActivated: (index) => {
                    root._userInteractedFc = true
                    if (index !== model.length - 1) {
                        root._selectedFcPort = model[index].split(" ")[0]
                    }
                }
            }

            QGCTextField {
                id: fcCustomPortField
                visible: fcPortCombo.currentIndex === fcPortCombo.model.length - 1 && CompanionController.hasLinksTelemetry
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                text: root._selectedFcPort
                placeholderText: "/dev/tty..."
                onTextChanged: {
                    if (fcPortCombo.currentIndex === fcPortCombo.model.length - 1) {
                        root._userInteractedFc = true
                        root._selectedFcPort = text.trim()
                    }
                }
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth }

            QGCLabel { text: qsTr("Baudrate:"); font.bold: true }

            QGCComboBox {
                id: fcBaudCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                enabled: CompanionController.hasLinksTelemetry && CompanionController.configStatus !== "APPLYING"
                model: ["921600", "115200", "57600", "460800", "230400"]

                function syncToActive() {
                    if (root._userInteractedFc || CompanionController.configStatus === "APPLYING") return
                    var target = CompanionController.fcBaud
                    if (!target || target <= 0) return
                    var idx = model ? model.indexOf(target.toString()) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    }
                }

                Component.onCompleted: {
                    if (CompanionController.hasLinksTelemetry && CompanionController.fcBaud > 0) {
                        syncToActive()
                    }
                }

                onActivated: (index) => {
                    root._userInteractedFc = true
                    root._selectedFcBaud = parseInt(model[index].split(" ")[0]) || 0
                }
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: CompanionController.configStatus === "APPLYING" ? qsTr("Applying...") : qsTr("Apply")
                primary: true
                enabled: CompanionController.hasLinksTelemetry && CompanionController.configStatus !== "APPLYING"
                onClicked: {
                    var port = ""
                    if (fcPortCombo.currentIndex === fcPortCombo.model.length - 1) {
                        port = fcCustomPortField.text.trim()
                    } else if (root._userInteractedFc && root._selectedFcPort !== "") {
                        port = root._selectedFcPort
                    } else if (fcPortCombo.currentIndex >= 0 && fcPortCombo.currentIndex < fcPortCombo.model.length - 1) {
                        port = fcPortCombo.model[fcPortCombo.currentIndex].split(" ")[0]
                    } else {
                        port = CompanionController.fcPort
                    }

                    var baud = 0
                    if (root._userInteractedFc && root._selectedFcBaud > 0) {
                        baud = root._selectedFcBaud
                    } else if (fcBaudCombo.currentIndex >= 0) {
                        baud = parseInt(fcBaudCombo.model[fcBaudCombo.currentIndex].split(" ")[0]) || CompanionController.fcBaud
                    } else {
                        baud = CompanionController.fcBaud
                    }

                    root._userInteractedFc = true
                    root._selectedFcPort = port
                    root._selectedFcBaud = baud
                    CompanionController.applyFcLink(port, baud)
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
                    text: CompanionController.hasLinksTelemetry && CompanionController.fcPort !== "" ? CompanionController.fcPort : "--"
                    font.bold: true
                    color: CompanionController.hasLinksTelemetry ? qgcPal.colorGreen : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Baudrate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasLinksTelemetry && CompanionController.fcBaud > 0 ? (CompanionController.fcBaud + " bps") : "--"
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionController.fcTxRate); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionController.fcRxRate); font.bold: true }
            }

            // Row 2
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total TX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionController.fcBytesTx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total RX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionController.fcBytesRx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Errors"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasLinksTelemetry ? (CompanionController.fcTxErr + " pkts") : "--"
                    font.bold: true
                    color: CompanionController.hasLinksTelemetry && CompanionController.fcTxErr > 0 ? qgcPal.colorRed : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Loss / Drop"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: formatDropRate(CompanionController.fcRxLoss)
                    font.bold: true
                    color: CompanionController.hasLinksTelemetry && CompanionController.fcRxLoss > 2.0 ? qgcPal.colorRed : qgcPal.text
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
                color:  CompanionController.hasLinksTelemetry
                        ? (CompanionController.siyiStatus === 2 ? qgcPal.colorGreen : (CompanionController.siyiStatus === 1 ? qgcPal.colorOrange : qgcPal.colorRed))
                        : qgcPal.windowShadeDark
            }

            QGCLabel {
                text: qsTr("Link Status: %1").arg(getStatusText(CompanionController.siyiStatus))
                font.bold: true
                color: CompanionController.hasLinksTelemetry
                       ? (CompanionController.siyiStatus === 2 ? qgcPal.text : qgcPal.colorOrange)
                       : qgcPal.text
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 2 }

            QGCLabel {
                text: CompanionController.hasLinksTelemetry ? qsTr("Transport: %1").arg(CompanionController.transportProtocol) : qsTr("Transport: --")
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
                enabled: CompanionController.hasLinksTelemetry && CompanionController.configStatus !== "APPLYING" && root.availablePortsList.length > 1
                model: root.availablePortsList

                function syncToActive() {
                    if (root._userInteractedSiyi || CompanionController.configStatus === "APPLYING") return
                    var target = CompanionController.siyiPort
                    if (!target || target === "") return
                    var idx = model ? model.indexOf(target) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    } else if (model && model.length > 0) {
                        currentIndex = model.length - 1
                        if (siyiCustomPortField) siyiCustomPortField.text = target
                    }
                }

                Component.onCompleted: {
                    if (CompanionController.hasLinksTelemetry && CompanionController.siyiPort !== "") {
                        syncToActive()
                        root._siyiInitialized = true
                    }
                }
                onModelChanged: {
                    if (!root._siyiInitialized) {
                        syncToActive()
                    }
                }

                onActivated: (index) => {
                    root._userInteractedSiyi = true
                    if (index !== model.length - 1) {
                        root._selectedSiyiPort = model[index].split(" ")[0]
                    }
                }
            }

            QGCTextField {
                id: siyiCustomPortField
                visible: siyiPortCombo.currentIndex === siyiPortCombo.model.length - 1 && CompanionController.hasLinksTelemetry
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                text: root._selectedSiyiPort
                placeholderText: "/dev/tty..."
                onTextChanged: {
                    if (siyiPortCombo.currentIndex === siyiPortCombo.model.length - 1) {
                        root._userInteractedSiyi = true
                        root._selectedSiyiPort = text.trim()
                    }
                }
            }

            Item { Layout.preferredWidth: ScreenTools.defaultFontPixelWidth }

            QGCLabel { text: qsTr("Baudrate:"); font.bold: true }

            QGCComboBox {
                id: siyiBaudCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                enabled: CompanionController.hasLinksTelemetry && CompanionController.configStatus !== "APPLYING"
                model: ["115200", "57600", "921600", "460800", "230400"]

                function syncToActive() {
                    if (root._userInteractedSiyi || CompanionController.configStatus === "APPLYING") return
                    var target = CompanionController.siyiBaud
                    if (!target || target <= 0) return
                    var idx = model ? model.indexOf(target.toString()) : -1
                    if (idx !== -1) {
                        currentIndex = idx
                    }
                }

                Component.onCompleted: {
                    if (CompanionController.hasLinksTelemetry && CompanionController.siyiBaud > 0) {
                        syncToActive()
                    }
                }

                onActivated: (index) => {
                    root._userInteractedSiyi = true
                    root._selectedSiyiBaud = parseInt(model[index].split(" ")[0]) || 0
                }
            }

            Item { Layout.fillWidth: true }

            QGCButton {
                text: CompanionController.configStatus === "APPLYING" ? qsTr("Applying...") : qsTr("Apply")
                primary: true
                enabled: CompanionController.hasLinksTelemetry && CompanionController.configStatus !== "APPLYING"
                onClicked: {
                    var port = ""
                    if (siyiPortCombo.currentIndex === siyiPortCombo.model.length - 1) {
                        port = siyiCustomPortField.text.trim()
                    } else if (root._userInteractedSiyi && root._selectedSiyiPort !== "") {
                        port = root._selectedSiyiPort
                    } else if (siyiPortCombo.currentIndex >= 0 && siyiPortCombo.currentIndex < siyiPortCombo.model.length - 1) {
                        port = siyiPortCombo.model[siyiPortCombo.currentIndex].split(" ")[0]
                    } else {
                        port = CompanionController.siyiPort
                    }

                    var baud = 0
                    if (root._userInteractedSiyi && root._selectedSiyiBaud > 0) {
                        baud = root._selectedSiyiBaud
                    } else if (siyiBaudCombo.currentIndex >= 0) {
                        baud = parseInt(siyiBaudCombo.model[siyiBaudCombo.currentIndex].split(" ")[0]) || CompanionController.siyiBaud
                    } else {
                        baud = CompanionController.siyiBaud
                    }

                    // Keep _userInteractedSiyi true during apply so incoming 1Hz telemetry does NOT revert user selection
                    root._userInteractedSiyi = true
                    root._selectedSiyiPort = port
                    root._selectedSiyiBaud = baud
                    CompanionController.applySiyiLink(port, baud)
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
                    text: CompanionController.hasLinksTelemetry && CompanionController.siyiPort !== "" ? CompanionController.siyiPort : "--"
                    font.bold: true
                    color: CompanionController.hasLinksTelemetry ? qgcPal.colorGreen : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Active Baudrate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasLinksTelemetry && CompanionController.siyiBaud > 0 ? (CompanionController.siyiBaud + " bps") : "--"
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionController.siyiTxRate); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Rate"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatRate(CompanionController.siyiRxRate); font.bold: true }
            }

            // Row 2
            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total TX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionController.siyiBytesTx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("Total RX Bytes"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel { text: formatBytes(CompanionController.siyiBytesRx); font.bold: true }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("TX Errors"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: CompanionController.hasLinksTelemetry ? (CompanionController.siyiTxErr + " pkts") : "--"
                    font.bold: true
                    color: CompanionController.hasLinksTelemetry && CompanionController.siyiTxErr > 0 ? qgcPal.colorRed : qgcPal.text
                }
            }

            ColumnLayout {
                spacing: 2
                QGCLabel { text: qsTr("RX Loss / Drop"); font.pointSize: ScreenTools.smallFontPointSize; opacity: 0.7 }
                QGCLabel {
                    text: formatDropRate(CompanionController.siyiRxLoss)
                    font.bold: true
                    color: CompanionController.hasLinksTelemetry && CompanionController.siyiRxLoss > 2.0 ? qgcPal.colorRed : qgcPal.text
                }
            }
        }
    }

    
    RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        QGCButton {
            objectName: "saveUartButton"
            text: qsTr("Save UART Defaults")
            enabled: CompanionController.hasLinksTelemetry &&
                     CompanionController.configStatus !== "Applying" &&
                     CompanionController.configStatus !== "WaitingTelemetry" &&
                     !root.hasPendingFcChanges && !root.hasPendingSiyiChanges
            onClicked: CompanionController.saveLinksConfig()
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
            onClearRequested: {
                CompanionController.clearLogHistory()
            }
        }
    }

    // Keep draft values until fresh Companion telemetry confirms the applied values.
    Connections {
        target: CompanionController

        function onCommandAckReceived(command, result, text) {
            if (command === 44011 && result === 0) {
                root._fcAwaitingTelemetry = (root._applySide === "fc")
                root._siyiAwaitingTelemetry = (root._applySide === "siyi")
            }
        }

        function onConfigStatusChanged() {
            if (CompanionController.configStatus === "Success" && root._fcAwaitingTelemetry &&
                CompanionController.fcPort === root._pendingFcPort && CompanionController.fcBaud === root._pendingFcBaud) {
                root._fcAwaitingTelemetry = false
                root._userInteractedFc = false
                root._selectedFcPort = ""
                root._selectedFcBaud = 0
                root._applySide = ""
                fcPortCombo.syncToActive()
                fcBaudCombo.syncToActive()
            }
            if (CompanionController.configStatus === "Success" && root._siyiAwaitingTelemetry &&
                CompanionController.siyiPort === root._pendingSiyiPort && CompanionController.siyiBaud === root._pendingSiyiBaud) {
                root._siyiAwaitingTelemetry = false
                root._userInteractedSiyi = false
                root._selectedSiyiPort = ""
                root._selectedSiyiBaud = 0
                root._applySide = ""
                siyiPortCombo.syncToActive()
                siyiBaudCombo.syncToActive()
            }
            if (CompanionController.configStatus === "Failed" || CompanionController.configStatus === "Timeout") {
                root._fcAwaitingTelemetry = false
                root._siyiAwaitingTelemetry = false
                root._applySide = ""
            }
        }

        function onVehicleEpochChanged() {
            root.resetEditState()
        }

        function onLinksChanged() {
            if (CompanionController.hasLinksTelemetry) {
                if (!root._fcInitialized && CompanionController.fcPort !== "") {
                    root._fcInitialized = true
                    fcPortCombo.syncToActive()
                    fcBaudCombo.syncToActive()
                }
                if (!root._siyiInitialized && CompanionController.siyiPort !== "") {
                    root._siyiInitialized = true
                    siyiPortCombo.syncToActive()
                    siyiBaudCombo.syncToActive()
                }
            } else {
                root.resetEditState()
            }
        }
    }
}