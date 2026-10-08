import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone

// Bottom bar of the Parameters editor (see ParameterEditorController.externalActionsUrl): Companion Computer edits made
// in the Developer category stay staged until applied here, and the apply reports every step until the CC confirms.
Rectangle {
    id: root

    readonly property string _state:    CompanionController.editorApplyState
    readonly property bool   _running:  _state === "running"
    readonly property bool   _staged:   CompanionController.ccModifiedParamCount > 0
    readonly property bool   shown:     _staged || _state !== "idle"

    height: layout.implicitHeight + ScreenTools.defaultFontPixelHeight
    color:  qgcPal.windowShade

    QGCPalette { id: qgcPal }

    function _lineColor(line) {
        return line.indexOf("✗") >= 0 ? qgcPal.colorRed : (line.indexOf("✓") >= 0 ? qgcPal.colorGreen : qgcPal.text)
    }

    ColumnLayout {
        id:                 layout
        anchors.fill:       parent
        anchors.margins:    ScreenTools.defaultFontPixelWidth
        spacing:            ScreenTools.defaultFontPixelHeight / 2

        RowLayout {
            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.fillWidth:   true
                wrapMode:           Text.WordWrap
                font.bold:          root._state !== "idle"
                color:              root._state === "failed" ? qgcPal.colorRed : (root._state === "success" ? qgcPal.colorGreen : qgcPal.text)
                text: {
                    if (root._state === "running") return qsTr("Applying to the Companion Computer...")
                    if (root._state === "success") return qsTr("Applied: confirmed by the Companion Computer")
                    if (root._state === "failed")  return qsTr("Apply failed")
                    return qsTr("%1 Companion Computer change(s) staged, not applied yet").arg(CompanionController.ccModifiedParamCount)
                }
            }

            QGCButton {
                text:       qsTr("Dismiss")
                visible:    root._state === "success" || root._state === "failed"
                onClicked:  CompanionController.dismissEditorApply()
            }

            QGCButton {
                text:       qsTr("Discard")
                visible:    root._staged
                enabled:    !root._running
                onClicked:  CompanionController.resetAllModifiedCcParameters()
            }

            QGCButton {
                primary:    true
                text:       qsTr("Apply to Companion Computer")
                visible:    root._staged
                enabled:    CompanionController.vehicleConnected && !root._running
                onClicked:  CompanionController.applyModifiedCcParameters()
            }
        }

        // One line per step, newest at the bottom.
        Flickable {
            id:                 steps
            Layout.fillWidth:   true
            Layout.preferredHeight: Math.min(contentHeight, ScreenTools.defaultFontPixelHeight * 9)
            visible:            CompanionController.editorApplyLog.length > 0
            contentHeight:      stepColumn.height
            clip:               true
            flickableDirection: Flickable.VerticalFlick
            onContentHeightChanged: contentY = Math.max(0, contentHeight - height)

            Column {
                id:     stepColumn
                width:  steps.width

                Repeater {
                    model: CompanionController.editorApplyLog

                    QGCLabel {
                        width:      stepColumn.width
                        wrapMode:   Text.WordWrap
                        text:       modelData
                        color:      root._lineColor(modelData)
                    }
                }
            }
        }
    }
}
