import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

/// Line-mode shell on the companion computer over MAVLink. The session lives in CompanionShellController (owned by the
/// plugin), never in this page: closing and reopening it keeps the session and asks the service for the authoritative
/// state. Nothing here parses terminal output to decide who is logged in.
AnalyzePage {
    id: root
    objectName: "companionShellPage"

    pageComponent: pageComponent
    pageDescription: qsTr("Runs one command line at a time on the companion computer. Log in first. Disconnect ends every job of the session, including background and nohup jobs.")

    // Overridable so tests can drive the page with their own controller.
    property var shell: QGroundControl.corePlugin.companionShell
    // The command lines the user typed. Never the password, never terminal output.
    property var history: []
    property int _historyIndex: -1
    readonly property int maxHistory: 100

    // Mirrors CompanionShellController::SessionState (declared there with Q_ENUM, values are what QML sees).
    readonly property int stDisconnected: 0
    readonly property int stConnecting: 1
    readonly property int stAuthRequired: 2
    readonly property int stAuthenticating: 3
    readonly property int stReady: 4
    readonly property int stUncertain: 5
    readonly property int stClosing: 6
    readonly property int stDisabled: 7
    readonly property int stLocked: 8
    readonly property int stBusy: 9
    readonly property int stOffline: 10

    readonly property int _state: shell ? shell.sessionState : stDisconnected
    readonly property bool _hasSession: shell ? shell.hasSession : false
    readonly property bool _sessionLive: _state === stAuthRequired || _state === stAuthenticating || _state === stReady || _state === stUncertain || _state === stOffline

    function stateText() {
        switch (_state) {
        case stConnecting:      return qsTr("Connecting...")
        case stAuthRequired:    return qsTr("Login required")
        case stAuthenticating:  return qsTr("Authenticating...")
        case stReady:           return qsTr("Ready")
        case stUncertain:       return qsTr("Outcome unknown: the last request was not confirmed and will not be sent again")
        case stClosing:         return qsTr("Closing. The companion shell restarts for a few seconds")
        case stDisabled:        return qsTr("The shell is disabled on the companion computer")
        case stLocked:          return qsTr("Locked after wrong passwords. Try again in %1 s").arg(shell.retryAfterSeconds)
        case stBusy:            return qsTr("Another client owns the shell session")
        case stOffline:         return qsTr("Offline: waiting for the shell service heartbeat")
        default:                return qsTr("Disconnected")
        }
    }

    // The loaded content (below) owns the fields; these forward to it so a test or a shortcut can call them on the page.
    property var _content: null
    function submitLogin() { if (_content) _content.submitLogin() }
    function submitCommand() { if (_content) _content.submitCommand() }
    function recallHistory(step) { if (_content) _content.recallHistory(step) }

    // A reopened page asks the service; input stays disabled (controller) until its answer arrives.
    Component.onCompleted: if (shell) shell.refreshState()
    onVisibleChanged: if (visible && shell) shell.refreshState()

    Component {
        id: pageComponent

        ColumnLayout {
            height: root.availableHeight
            width: root.availableWidth
            spacing: ScreenTools.defaultFontPixelHeight * 0.5

            QGCPalette { id: qgcPal }

            Component.onCompleted: root._content = this

            // The password goes straight to the controller and the field is emptied at once; it never touches `history`.
            function submitLogin() {
                if (!root.shell || !passwordField.enabled) {
                    return
                }
                root.shell.login(passwordField.text)
                passwordField.text = ""
            }

            function submitCommand() {
                if (!root.shell || !commandField.enabled) {
                    return
                }
                const line = commandField.text
                root.shell.sendLine(line)
                if (root.shell.requestPending) {        // accepted by the controller: it is on its way
                    if (line !== "") {
                        const h = root.history.slice()
                        h.push(line)
                        root.history = h.slice(Math.max(0, h.length - root.maxHistory))
                    }
                    root._historyIndex = -1
                    commandField.text = ""
                }
            }

            function recallHistory(step) {
                if (root.history.length === 0) {
                    return
                }
                const from = root._historyIndex === -1 ? root.history.length : root._historyIndex
                root._historyIndex = Math.max(-1, Math.min(root.history.length - 1, from + step))
                commandField.text = root._historyIndex === -1 ? "" : root.history[root._historyIndex]
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    objectName: "shellState"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.bold: true
                    text: root.stateText()
                }
                QGCLabel {
                    objectName: "shellTransport"
                    // Transport health only: the heartbeat of the shell service, not a login.
                    text: root.shell && root.shell.online ? qsTr("Service online") : qsTr("Service offline")
                    color: root.shell && root.shell.online ? qgcPal.text : qgcPal.warningText
                }
            }

            QGCLabel {
                objectName: "shellError"
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.WordWrap
                color: qgcPal.warningText
                text: root.shell ? root.shell.lastError : ""
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth

                QGCButton {
                    objectName: "connectButton"
                    text: qsTr("Connect")
                    enabled: root.shell && !root._hasSession && root.shell.online
                    onClicked: root.shell.connectShell()
                }
                QGCTextField {
                    id: passwordField
                    objectName: "shellPassword"
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 28
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password        // unconditional: no prompt text can change it
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enabled: root.shell && root.shell.canLogin
                    onAccepted: submitLogin()
                }
                QGCButton {
                    objectName: "loginButton"
                    text: qsTr("Login")
                    enabled: passwordField.enabled && passwordField.text !== ""
                    onClicked: submitLogin()
                }
                Item { Layout.fillWidth: true }
                QGCButton {
                    objectName: "ctrlCButton"
                    text: qsTr("Ctrl-C")
                    // Independent of the line/transfer state: it must work exactly when everything else is stuck.
                    enabled: root._sessionLive
                    onClicked: root.shell.sendCtrlC()
                }
                QGCButton {
                    objectName: "clearButton"
                    text: qsTr("Clear")
                    onClicked: if (root.shell) root.shell.clear()    // the display only; the session and login are untouched
                }
                QGCButton {
                    objectName: "disconnectButton"
                    text: qsTr("Disconnect")
                    enabled: root._hasSession
                    onClicked: root.shell.disconnectShell()
                }
            }

            QGCFlickable {
                id: flickable
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: output.width
                contentHeight: output.height
                clip: true

                TextArea.flickable: TextArea {
                    id: output
                    objectName: "shellOutput"
                    width: flickable.width
                    readOnly: true
                    wrapMode: Text.WrapAnywhere
                    textFormat: TextEdit.PlainText      // terminal text is data, never markup
                    color: qgcPal.text
                    font.family: ScreenTools.fixedFontFamily
                    font.pointSize: ScreenTools.defaultFontPointSize
                    text: root.shell ? root.shell.text : ""
                    onTextChanged: if (flickable.contentHeight > flickable.height) flickable.contentY = flickable.contentHeight - flickable.height
                }
            }

            QGCTextField {
                id: commandField
                objectName: "shellCommand"
                Layout.fillWidth: true
                placeholderText: qsTr("Command (one line)")
                font.family: ScreenTools.fixedFontFamily
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                enabled: root.shell && root.shell.canSendLine
                onAccepted: submitCommand()
                Keys.onUpPressed: recallHistory(-1)
                Keys.onDownPressed: recallHistory(1)
                Keys.onPressed: (event) => {
                    // Ctrl-C with nothing selected is an interrupt, as in a terminal; with a selection it stays "copy".
                    if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C && selectedText === "" && root._sessionLive) {
                        root.shell.sendCtrlC()
                        event.accepted = true
                    }
                }
            }
        }
    }
}
