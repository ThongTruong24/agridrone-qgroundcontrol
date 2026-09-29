import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string label
    property bool monospace: false
    property string value: "--"
    property color valueColor: "#dce5ec"

    Layout.fillWidth: true
    spacing: 12

    Label {
        Layout.fillWidth: true
        color: "#71899b"
        elide: Text.ElideRight
        font.pixelSize: 12
        text: root.label
    }

    Label {
        color: root.valueColor
        font.family: root.monospace ? "Consolas" : "Segoe UI"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignRight
        text: root.value
    }
}
