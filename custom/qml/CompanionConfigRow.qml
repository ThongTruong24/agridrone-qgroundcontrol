
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QGroundControl
import QGroundControl.Controls
import Custom.AgriDrone
import QGC

/// One editable key-value row in the config panel
RowLayout {
    id: root

    property string configKey:   ""
    property var    configValue: ""
    property bool   readOnly:    false

    signal configChanged(string key, var value)

    readonly property color _labelColor:  "#8B949E"
    readonly property color _inputBg:     "#161B22"
    readonly property color _inputBorder: "#30363D"
    readonly property color _textColor:   "#E6EDF3"
    readonly property color _accentColor: "#1E8BC3"

    spacing: 8

    // Key label
    QGCLabel {
        text:              root.configKey
        color:             root._labelColor
        font.pointSize:    ScreenTools.smallFontPointSize
        font.family:       ScreenTools.fixedFontFamily
        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 22
        elide:             Text.ElideRight
        ToolTip.text:      root.configKey
        ToolTip.visible:   keyMa.containsMouse
        ToolTip.delay:     500
        MouseArea { id: keyMa; anchors.fill: parent; hoverEnabled: true }
    }

    // Value input
    TextField {
        id:                 valueField
        Layout.fillWidth:   true
        text:               root.configValue !== undefined ? String(root.configValue) : ""
        readOnly:           root.readOnly
        color:              root._textColor
        font.family:        ScreenTools.fixedFontFamily
        font.pointSize:     ScreenTools.smallFontPointSize
        background: Rectangle {
            color:        root._inputBg
            border.color: valueField.activeFocus ? root._accentColor : root._inputBorder
            border.width: valueField.activeFocus ? 2 : 1
            radius:       4
        }
        leftPadding: 8
        topPadding:  4
        bottomPadding: 4

        onEditingFinished: root.configChanged(root.configKey, valueField.text)
    }
}
