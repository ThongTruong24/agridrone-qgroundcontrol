import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool active: false
    property color activeColor: "#54dfa2"
    property bool showDot: true
    property string text

    border.color: root.active ? Qt.alpha(root.activeColor, 0.42) : "#2b3c49"
    border.width: 1
    color: root.active ? Qt.alpha(root.activeColor, 0.1) : "#17222c"
    implicitHeight: 28
    implicitWidth: badgeRow.implicitWidth + 22
    radius: height / 2

    RowLayout {
        id: badgeRow

        anchors.centerIn: parent
        spacing: 7

        Rectangle {
            Layout.preferredHeight: 6
            Layout.preferredWidth: 6
            color: root.active ? root.activeColor : "#617181"
            radius: 3
            visible: root.showDot
        }

        Label {
            color: root.active ? root.activeColor : "#8494a2"
            font.letterSpacing: 0.6
            font.pixelSize: 10
            font.weight: Font.Bold
            text: root.text
        }
    }
}
