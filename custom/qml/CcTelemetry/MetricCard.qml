import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property color accentColor: "#54dfa2"
    property string detail
    property string iconText
    property string title
    property string unit
    property string value: "--"

    border.color: "#1d3545"
    border.width: 1
    color: "#0d1a25"
    implicitHeight: 188
    implicitWidth: 330
    radius: 18

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 18
        color: root.accentColor
        height: 34
        opacity: 0.7
        radius: 3
        width: 5
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 19
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 11

            Rectangle {
                Layout.preferredHeight: 36
                Layout.preferredWidth: 36
                color: "#142a38"
                radius: 10

                Label {
                    anchors.centerIn: parent
                    color: "#cedbe5"
                    font.pixelSize: root.iconText.length > 2 ? 10 : 12
                    font.weight: Font.Bold
                    text: root.iconText
                }
            }

            Label {
                Layout.fillWidth: true
                color: "#89a0b2"
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.Medium
                text: root.title
            }
        }

        Item {
            Layout.preferredHeight: 2
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 7

            Label {
                color: "#f5f8fb"
                font.letterSpacing: -0.5
                font.pixelSize: 30
                font.weight: Font.Bold
                text: root.value
            }

            Label {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 5
                color: root.accentColor
                font.pixelSize: 12
                font.weight: Font.DemiBold
                text: root.unit
            }

            Item {
                Layout.fillWidth: true
            }
        }

        Label {
            Layout.fillWidth: true
            color: "#536c7f"
            elide: Text.ElideRight
            font.pixelSize: 11
            text: root.detail
        }
    }
}
