import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property int bit
    property bool flagSet
    property string label

    Layout.fillWidth: true
    spacing: 10

    Rectangle {
        Layout.preferredHeight: 20
        Layout.preferredWidth: 24
        color: "#142836"
        radius: 6

        Label {
            anchors.centerIn: parent
            color: "#6f899b"
            font.family: "Consolas"
            font.pixelSize: 9
            font.weight: Font.Bold
            text: "b" + root.bit
        }
    }

    Label {
        Layout.fillWidth: true
        color: "#9babb8"
        font.pixelSize: 12
        text: root.label
    }

    StatusBadge {
        active: root.flagSet
        showDot: false
        text: root.flagSet ? qsTr("ON") : qsTr("OFF")
    }
}
