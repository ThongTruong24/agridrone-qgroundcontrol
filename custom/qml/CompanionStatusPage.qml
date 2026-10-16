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
    // The CC's heartbeat and lock decide, not a Vehicle: the FC may be silent (spec §5, §7).
    readonly property bool writeAllowed: !!system.connected && !system.locked && !system.loadError
    readonly property string blockedText: !system.connected ? qsTr("Mất kết nối tới Companion") : (system.loadError || system.lockReason || "")
    property string localError: ""
    property string draftSsid: ""
    property string draftCidr: ""
    property bool draftDhcp: false
    property bool dirty: false
    property bool readbackRequested: false
    function ensureReadback() {
        if (!system || !system.connected) { readbackRequested = false; return }
        if (!readbackRequested && !system.pending) {
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
    onSystemChanged: ensureReadback()
    onNetworkChanged: { refreshDraft(); ensureReadback() }
    showExpand: true
    function display(value) { return value === undefined || value === null || value === "" ? qsTr("N/A") : String(value) }
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
                QGCLabel { objectName: "companionBlockedText"; visible: !page.writeAllowed; text: page.blockedText; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Repeater {
                    model: CompanionController.serialLinks.length // every link the CC reports
                    delegate: ColumnLayout {
                        id: portRow
                        required property int index
                        readonly property var linkData: (CompanionController.serialLinks && portRow.index < CompanionController.serialLinks.length)
                                                        ? CompanionController.serialLinks[portRow.index]
                                                        : null
                        readonly property int linkIndex: portRow.linkData ? portRow.linkData.index : -1 // the CC's link index
                        readonly property bool isFresh: portRow.linkData ? portRow.linkData.fresh : false
                        readonly property int currentBaud: portRow.linkData && portRow.linkData.baudrate !== undefined ? portRow.linkData.baudrate : 0

                        readonly property string currentPort: portRow.linkData && portRow.linkData.port !== undefined ? portRow.linkData.port : ""
                        property int selectedBaud: 0
                        property string selectedPort: ""
                        property bool userEdited: false
                        // The port values of the combo: the CC's boot list (CC_SER_PORTS), the current port even when it
                        // was absent at boot, and "" (off). label() is what the combo shows for each.
                        function values() {
                            const list = CompanionController.availablePorts.slice()
                            if (portRow.currentPort !== "" && list.indexOf(portRow.currentPort) < 0) list.push(portRow.currentPort)
                            list.push("")
                            return list
                        }
                        function label(port) {
                            if (port === "") return qsTr("Tắt")
                            const owner = portRow.usedBy(port)
                            if (owner !== "") return qsTr("%1 (dùng bởi %2)").arg(port).arg(owner)
                            // An empty list is not read yet: nothing is marked absent before the CC reports it.
                            const ports = CompanionController.availablePorts
                            return ports.length && ports.indexOf(port) < 0 ? qsTr("%1 (không có)").arg(port) : port
                        }
                        // The name of another link on that port; "" = free. QGCComboBox cannot disable an item, so a choice
                        // of a used port is refused with this name.
                        function usedBy(port) {
                            for (let i = 0; i < CompanionController.serialLinks.length; ++i)
                                if (i !== portRow.index && port !== "" && CompanionController.serialLinks[i].port === port)
                                    return CompanionController.serialLinks[i].name
                            return ""
                        }

                        onCurrentPortChanged: { if (!userEdited) selectedPort = currentPort }

                        onCurrentBaudChanged: {
                            if (!userEdited && !baudCombo.popup.visible) {
                                selectedBaud = currentBaud
                            } else if (userEdited && currentBaud === selectedBaud) {
                                userEdited = false
                            }
                        }

                        Component.onCompleted: {
                            selectedPort = currentPort
                            selectedBaud = currentBaud
                        }

                        Layout.fillWidth: true
                        QGCLabel { text: qsTr("Port %1").arg(portRow.index + 1); font.bold: true }
                        LabelledLabel { label: qsTr("Name"); labelText: portRow.isFresh ? portRow.linkData.name : qsTr("N/A") }
                        LabelledLabel { label: qsTr("Status"); labelText: portRow.isFresh ? (portRow.linkData.status === 2 ? qsTr("CONNECTED") : qsTr("DISCONNECTED")) : qsTr("N/A") }
                        LabelledLabel { label: qsTr("Baudrate"); labelText: portRow.isFresh ? portRow.linkData.baudrate : qsTr("N/A") }
                        LabelledLabel { label: qsTr("TX (B/s)"); labelText: portRow.isFresh ? Math.round(portRow.linkData.txRate) : qsTr("N/A") }
                        LabelledLabel { label: qsTr("RX (B/s)"); labelText: portRow.isFresh ? Math.round(portRow.linkData.rxRate) : qsTr("N/A") }
                        LabelledLabel { label: qsTr("RX errors (/s)"); labelText: portRow.isFresh ? portRow.linkData.rxErrors : qsTr("N/A") }

                        QGCComboBox {
                            id: portCombo
                            objectName: "portCombo" + portRow.linkIndex
                            Layout.fillWidth: true
                            model: portRow.values().map(portRow.label)
                            // On `model`: a new model resets the index to 0, and this binding then puts it back.
                            currentIndex: model.indexOf(portRow.label(portRow.selectedPort))
                            enabled: page.writeAllowed && !page.system.pending && portRow.isFresh
                            onActivated: (idx) => {
                                const port = portRow.values()[idx]
                                const owner = portRow.usedBy(port)
                                if (owner !== "") {
                                    page.localError = qsTr("Cổng đang được dùng bởi %1").arg(owner)
                                    currentIndex = Qt.binding(() => model.indexOf(portRow.label(portRow.selectedPort)))
                                    return
                                }
                                portRow.selectedPort = port
                                portRow.userEdited = (port !== portRow.currentPort || portRow.selectedBaud !== portRow.currentBaud)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCComboBox {
                                id: baudCombo
                                objectName: "baudCombo" + portRow.linkIndex
                                Layout.fillWidth: true
                                enabled: page.writeAllowed && !page.system.pending
                                // Read again with linkData: a link N's options come when the CC reports its parameters.
                                model: portRow.linkData ? CompanionController.baudOptions(portRow.linkIndex) : []
                                currentIndex: model.indexOf(String(portRow.selectedBaud)) // none until the CC reports a baud

                                onActivated: (idx) => {
                                    var chosen = Number(textAt(idx))
                                    portRow.selectedBaud = chosen
                                    portRow.userEdited = (chosen !== portRow.currentBaud || portRow.selectedPort !== portRow.currentPort)
                                }
                            }

                            QGCButton {
                                id: updateBtn
                                objectName: "updateLink" + portRow.linkIndex
                                // The current values again still send an APPLY when one is due (a SAVE block, a lost APPLY).
                                // current* first: reading one can run its change handler, which writes selected* (binding loop).
                                readonly property bool same: portRow.currentBaud === portRow.selectedBaud && portRow.currentPort === portRow.selectedPort
                                readonly property bool retry: same && !!page.system.needsApply
                                Layout.alignment: Qt.AlignVCenter
                                iconSource: "/custom/images/system-update.png"
                                text: retry ? qsTr("Áp dụng lại") : qsTr("Update")
                                primary: portRow.selectedBaud > 0 && (!same || retry)
                                enabled: page.writeAllowed && !page.system.pending && portRow.selectedBaud > 0 && (!same || retry)
                                onClicked: {
                                    page.localError = CompanionController.setLinkConfig(portRow.linkIndex, portRow.selectedPort, portRow.selectedBaud) ? "" : (page.system.linkError || qsTr("Request unavailable"))
                                }
                            }
                        }
                    }
                }
                QGCLabel { objectName: "linksUnsavedNote"; visible: !!page.system.unsaved; text: qsTr("Đã áp dụng, chưa lưu — reboot sẽ về cấu hình cũ"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                QGCButton {
                    objectName: "saveLinks"
                    visible: !!page.system.unsaved
                    text: qsTr("Lưu")
                    enabled: page.writeAllowed && !page.system.pending
                    onClicked: page.localError = CompanionController.saveCompanionGroup("links") ? "" : qsTr("Không gửi được lệnh lưu")
                }
                QGCButton {
                    objectName: "cancelCompanionWait"
                    visible: !!page.system.waitingReconnect
                    text: qsTr("Hủy chờ")
                    onClicked: CompanionController.cancelCompanionWait()
                }
                QGCLabel { text: qsTr("Networking"); font.bold: true }
                LabelledLabel { label: qsTr("WiFi"); labelText: page.display(page.network.wifiName) }
                LabelledLabel { label: qsTr("WiFi status"); labelText: page.network.fresh ? (page.network.wifiConnected ? qsTr("CONNECTED") : qsTr("DISCONNECTED")) : qsTr("N/A") }
                LabelledLabel { label: qsTr("Hotspot"); labelText: page.display(page.network.ssid) }
                LabelledLabel { label: qsTr("Hotspot status"); labelText: page.network.fresh ? (page.network.open ? qsTr("ON") : qsTr("OFF")) : qsTr("N/A") }
                LabelledLabel { label: qsTr("IP/CIDR"); labelText: page.display(page.network.ipCidr) }
                LabelledLabel { label: qsTr("DHCP server"); labelText: page.network.available ? (page.network.dhcpEnabled ? qsTr("ON") : qsTr("OFF")) : qsTr("N/A") }
                QGCLabel { text: page.system.linkError || page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true } // the CC's reason first
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
                text: page.network.needsApply ? qsTr("Áp dụng lại") : qsTr("Apply")
                enabled: page.writeAllowed && page.network.available && !page.network.pending
                onClicked: {
                    page.localError = CompanionController.applyHotspot({ssid: ssid.text, password: password.text, ipCidr: cidr.text, dhcpEnabled: page.draftDhcp}) ? "" : qsTr("Hotspot request unavailable")
                    password.clear()
                }
            }
            QGCLabel { visible: !!page.network.unsaved; text: qsTr("Đã áp dụng, chưa lưu — reboot sẽ về cấu hình cũ"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            QGCButton {
                objectName: "saveHotspot"
                visible: !!page.network.unsaved
                text: qsTr("Lưu")
                enabled: page.writeAllowed && !page.system.pending
                onClicked: page.localError = CompanionController.saveCompanionGroup("hotspot") ? "" : qsTr("Không gửi được lệnh lưu")
            }
            QGCLabel { text: page.network.error || page.localError; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
