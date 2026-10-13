pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls

ToolIndicatorPage {
    id: page
    QGCPalette { id: qgcPal }
    readonly property var system: CompanionController.companionState
    readonly property var network: CompanionController.hotspotState
    readonly property bool writeAllowed: activeVehicle && !activeVehicle.armed && !activeVehicle.vehicleLinkManager.communicationLost
    property string localError: ""
    property string draftSsid: ""
    property string draftCidr: ""
    property bool draftDhcp: false
    property bool dirty: false
    property bool readbackRequested: false
    function ensureReadback() {
        if (!system || !network) return
        if (!system.connected && !network.available && !CompanionController.serialLinks.length) { readbackRequested = false; return }
        if (!readbackRequested && activeVehicle && !system.pending) {
            readbackRequested = true
            CompanionController.requestCcParameters()
        }
    }
    function refreshDraft() {
        if (dirty) return
        draftSsid = network.ssid || ""
        draftCidr = network.ipCidr || ""
        draftDhcp = !!network.dhcpEnabled
    }
    Component.onCompleted: { refreshDraft(); ensureReadback() }
    onActiveVehicleChanged: { readbackRequested = false; ensureReadback() }
    onSystemChanged: ensureReadback()
    onNetworkChanged: { refreshDraft(); ensureReadback() }
    showExpand: true
    function display(value) { return value === undefined || value === null || value === "" ? qsTr("N/A") : String(value) }
    readonly property var baudRates: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600", "1500000"]
    contentComponent: Component {
        ScrollView {
            implicitWidth: ScreenTools.defaultFontPixelWidth * 38
            implicitHeight: Math.min(contentHeight, ScreenTools.screenHeight * 0.65)
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: ScreenTools.defaultFontPixelHeight / 2
                QGCLabel { text: qsTr("Vehicle Companion Status"); font.bold: true }
                LabelledLabel { label: qsTr("Status"); labelText: page.system.connected ? qsTr("Connected") : qsTr("Disconnected") }
                LabelledLabel { label: qsTr("Name"); labelText: page.display(page.system.name) }
                LabelledLabel { label: qsTr("CPU (%)"); labelText: page.display(page.system.cpuUsage) }
                LabelledLabel { label: qsTr("CPU temperature (°C)"); labelText: page.display(page.system.cpuTemp) }
                Repeater {
                    model: 2
                    delegate: ColumnLayout {
                        id: portRow
                        required property int index
                        readonly property var linkData: (CompanionController.serialLinks && portRow.index < CompanionController.serialLinks.length)
                                                        ? CompanionController.serialLinks[portRow.index]
                                                        : null
                        readonly property bool isFresh: portRow.linkData ? portRow.linkData.fresh : false
                        readonly property int currentBaud: portRow.linkData && portRow.linkData.baudrate !== undefined ? portRow.linkData.baudrate : 0

                        readonly property string currentPort: portRow.linkData && portRow.linkData.port !== undefined ? portRow.linkData.port : ""
                        property int selectedBaud: 0
                        property string selectedPort: ""
                        property bool userEdited: false
                        // The CC's detected devices, plus the one in use when it is not in that list.
                        function portChoices() {
                            const list = CompanionController.availablePorts.slice()
                            if (portRow.currentPort !== "" && list.indexOf(portRow.currentPort) < 0) list.push(portRow.currentPort)
                            return list
                        }

                        function syncBaudCombo() {
                            var target = userEdited ? selectedBaud : currentBaud
                            if (target > 0 && baudCombo.model) {
                                var idx = baudCombo.model.indexOf(String(target))
                                if (idx !== -1 && baudCombo.currentIndex !== idx) {
                                    baudCombo.currentIndex = idx
                                }
                            }
                        }

                        onCurrentPortChanged: { if (!userEdited) { selectedPort = currentPort; portCombo.currentIndex = portCombo.model.indexOf(currentPort) } }

                        onCurrentBaudChanged: {
                            if (!userEdited && currentBaud > 0 && !baudCombo.popup.visible) {
                                selectedBaud = currentBaud
                                syncBaudCombo()
                            } else if (userEdited && currentBaud === selectedBaud) {
                                userEdited = false
                            }
                        }

                        Component.onCompleted: {
                            selectedPort = currentPort
                            if (currentBaud > 0) {
                                selectedBaud = currentBaud
                                syncBaudCombo()
                            }
                        }

                        Layout.fillWidth: true
                        QGCLabel { text: qsTr("Port %1").arg(portRow.index + 1); font.bold: true }
                        LabelledLabel { label: qsTr("Name"); labelText: portRow.isFresh ? portRow.linkData.name : qsTr("N/A") }
                        LabelledLabel { label: qsTr("Status"); labelText: portRow.isFresh ? (portRow.linkData.status === 2 ? qsTr("CONNECTED") : qsTr("DISCONNECTED")) : qsTr("N/A") }
                        LabelledLabel { label: qsTr("Baudrate"); labelText: portRow.isFresh ? portRow.linkData.baudrate : qsTr("N/A") }
                        LabelledLabel { label: qsTr("TX (B/s)"); labelText: portRow.isFresh ? Math.round(portRow.linkData.txRate) : qsTr("N/A") }
                        LabelledLabel { label: qsTr("RX (B/s)"); labelText: portRow.isFresh ? Math.round(portRow.linkData.rxRate) : qsTr("N/A") }

                        QGCComboBox {
                            id: portCombo
                            Layout.fillWidth: true
                            model: portRow.portChoices()
                            currentIndex: model.indexOf(portRow.currentPort)
                            enabled: page.writeAllowed && !page.system.pending && portRow.isFresh
                            onActivated: (idx) => {
                                portRow.selectedPort = textAt(idx)
                                portRow.userEdited = (portRow.selectedPort !== portRow.currentPort || portRow.selectedBaud !== portRow.currentBaud)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCComboBox {
                                id: baudCombo
                                Layout.fillWidth: true
                                enabled: page.writeAllowed && !page.system.pending
                                model: page.baudRates

                                Component.onCompleted: portRow.syncBaudCombo()

                                onActivated: (idx) => {
                                    var chosen = Number(textAt(idx))
                                    portRow.selectedBaud = chosen
                                    portRow.userEdited = (chosen !== portRow.currentBaud || portRow.selectedPort !== portRow.currentPort)
                                }
                            }

                            QGCButton {
                                id: updateBtn
                                Layout.alignment: Qt.AlignVCenter
                                iconSource: "/custom/images/system-update.png"
                                text: qsTr("Update")
                                primary: portRow.selectedBaud > 0 && (portRow.selectedBaud !== portRow.currentBaud || portRow.selectedPort !== portRow.currentPort)
                                enabled: page.writeAllowed && !page.system.pending && portRow.selectedBaud > 0 && (portRow.selectedBaud !== portRow.currentBaud || portRow.selectedPort !== portRow.currentPort)
                                onClicked: {
                                    page.localError = CompanionController.setLinkConfig(portRow.index, portRow.selectedPort, portRow.selectedBaud) ? "" : (page.system.linkError || qsTr("Request unavailable"))
                                }
                            }
                        }
                    }
                }
                QGCLabel { text: qsTr("Networking"); font.bold: true }
                LabelledLabel { label: qsTr("WiFi"); labelText: page.display(page.network.wifiName) }
                LabelledLabel { label: qsTr("WiFi status"); labelText: page.network.fresh ? (page.network.wifiConnected ? qsTr("CONNECTED") : qsTr("DISCONNECTED")) : qsTr("N/A") }
                LabelledLabel { label: qsTr("Hotspot"); labelText: page.display(page.network.ssid) }
                LabelledLabel { label: qsTr("Hotspot status"); labelText: page.network.fresh ? (page.network.open ? qsTr("ON") : qsTr("OFF")) : qsTr("N/A") }
                LabelledLabel { label: qsTr("IP/CIDR"); labelText: page.display(page.network.ipCidr) }
                LabelledLabel { label: qsTr("DHCP server"); labelText: page.network.available ? (page.network.dhcpEnabled ? qsTr("ON") : qsTr("OFF")) : qsTr("N/A") }
                QGCLabel { text: page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                CompanionOperationLog { operation: page.network.operation }
            }
        }
    }
    expandedComponent: Component {
        ColumnLayout {
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 32
            spacing: ScreenTools.defaultFontPixelHeight / 2
            QGCLabel { text: qsTr("Hotspot Config"); font.bold: true }
            QGCLabel { text: qsTr("SSID") }
            QGCTextField { id: ssid; Layout.fillWidth: true; text: page.draftSsid; onTextEdited: { page.draftSsid = text; page.dirty = true } enabled: page.writeAllowed && page.network.available && !page.network.pending }
            QGCLabel { text: qsTr("Password (blank keeps current)") }
            QGCTextField { id: password; Layout.fillWidth: true; echoMode: TextInput.Password; enabled: page.writeAllowed && page.network.available && !page.network.pending }
            QGCLabel { text: qsTr("IP/CIDR") }
            QGCTextField { id: cidr; Layout.fillWidth: true; text: page.draftCidr; onTextEdited: { page.draftCidr = text; page.dirty = true } enabled: page.writeAllowed && page.network.available && !page.network.pending }
            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    text: qsTr("DHCP server")
                    Layout.fillWidth: true
                }

                QGCLabel {
                    text: dhcpSwitch.checked ? qsTr("ON") : qsTr("OFF")
                    font.bold: true
                    color: dhcpSwitch.checked ? "#34C759" : qgcPal.text
                }

                CompanionToggleSwitch {
                    id: dhcpSwitch
                    checked: page.draftDhcp
                    enabled: page.writeAllowed && page.network.available && !page.network.pending
                    onToggled: (val) => {
                        page.draftDhcp = val
                        page.dirty = true
                    }
                }
            }
            QGCButton {
                text: qsTr("Apply")
                enabled: page.writeAllowed && page.network.available && !page.network.pending
                onClicked: {
                    page.localError = CompanionController.applyHotspot({ssid: ssid.text, password: password.text, ipCidr: cidr.text, dhcpEnabled: page.draftDhcp}) ? "" : qsTr("Hotspot request unavailable")
                    password.clear()
                }
            }
            QGCLabel { text: page.network.error || page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QGCLabel { visible: !page.writeAllowed; text: page.activeVehicle && page.activeVehicle.armed ? qsTr("Configuration locked while armed") : qsTr("Configuration requires an active vehicle link"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
