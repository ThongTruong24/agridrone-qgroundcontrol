import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property string messageName
    required property bool received
    required property bool stale

    Layout.fillWidth: true
    border.color: root.stale ? "#745f2b" : "#244659"
    border.width: 1
    color: root.stale ? "#241f13" : "#0d1a25"
    implicitHeight: 52
    radius: 10
    visible: !root.received || root.stale

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        StatusBadge {
            active: root.stale
            activeColor: "#f5c84c"
            text: root.stale ? qsTr("STALE") : qsTr("NO DATA")
        }

        Label {
            Layout.fillWidth: true
            color: root.stale ? "#d5c185" : "#7890a6"
            font.pixelSize: 12
            text: root.stale ? qsTr("No recent %1 message; showing the last received values.").arg(root.messageName) : qsTr("Waiting for %1 from the active vehicle.").arg(root.messageName)
        }
    }
}
