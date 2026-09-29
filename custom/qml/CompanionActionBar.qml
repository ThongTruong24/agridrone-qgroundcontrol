
import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone
import QGC

/// Reusable Bottom Action Bar for Companion Configuration Panels
/// Provides:
///   - Visual status pill (Synchronized / Pending unapplied changes)
///   - [↺ Undo] (Undo changes)
///   - [⚙ Mặc định] (Restore Factory Defaults)
///   - [[SAVE]] (Save current settings as permanent default)
///   - [✓ Áp dụng] (Apply all staged changes)
Rectangle {
    id: root

    property bool   hasChanges:     false
    property bool   isBusy:         false
    property string statusMessage:  ""

    signal undoClicked()
    signal restoreDefaultsClicked()
    signal saveDefaultClicked()
    signal applyClicked()

    readonly property color _bgCard:      "#161B22"
    readonly property color _borderColor: "#30363D"
    readonly property color _accentGreen: "#3FB950"
    readonly property color _accentYellow:"#D29922"
    readonly property color _textNormal:  "#E6EDF3"
    readonly property color _textMuted:   "#8B949E"

    Layout.fillWidth:       true
    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2.8
    color:                  _bgCard
    radius:                 6
    border.color:           root.hasChanges ? root._accentYellow : root._borderColor
    border.width:           root.hasChanges ? 2 : 1

    Behavior on border.color { ColorAnimation { duration: 200 } }

    RowLayout {
        anchors {
            fill: parent
            leftMargin: 12
            rightMargin: 12
        }
        spacing: 10

        // Status Indicator Pill
        Rectangle {
            radius: 4
            color: root.hasChanges ? "#2D2200" : "#0D2115"
            border.color: root.hasChanges ? root._accentYellow : root._accentGreen
            border.width: 1
            implicitWidth: statusLayout.implicitWidth + 16
            implicitHeight: ScreenTools.defaultFontPixelHeight * 1.8

            RowLayout {
                id: statusLayout
                anchors.centerIn: parent
                spacing: 6

                QGCLabel {
                    text: root.hasChanges ? "!" : ""
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    text: root.statusMessage !== "" ? root.statusMessage :
                          (root.hasChanges ? qsTr("Unsaved changes") : qsTr("Config synced"))
                    color: root.hasChanges ? root._accentYellow : root._accentGreen
                    font.bold: true
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Button: Undo (Undo)
        QGCButton {
            id: btnUndo
            text: qsTr("Undo")
            enabled: root.hasChanges && !root.isBusy
            onClicked: root.undoClicked()
        }

        // Button: Restore Defaults (Restore Defaults)
        QGCButton {
            id: btnDefaults
            text: qsTr("Restore Defaults")
            enabled: !root.isBusy
            onClicked: root.restoreDefaultsClicked()
        }

        // Button: Save Default (Save as Default)
        QGCButton {
            id: btnSaveDefault
            text: qsTr("Save Default")
            enabled: !root.isBusy
            onClicked: root.saveDefaultClicked()
        }

        // Button: Áp dụng (Apply)
        QGCButton {
            id: btnApply
            text: qsTr("Apply Config")
            enabled: root.hasChanges && !root.isBusy
            primary: true
            onClicked: root.applyClicked()
        }
    }
}
