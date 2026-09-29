import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    default property alias content: body.data
    property int contentSpacing: 12
    property string iconText
    property string subtitle
    property string title

    border.color: "#1d3545"
    border.width: 1
    color: "#0d1a25"
    implicitHeight: Math.max(188, cardLayout.implicitHeight + 36)
    implicitWidth: 330
    radius: 18

    Rectangle {
        anchors.left: parent.left
        anchors.margins: 1
        anchors.right: parent.right
        anchors.top: parent.top
        height: 70
        opacity: 0.58
        radius: parent.radius - 1

        gradient: Gradient {
            GradientStop {
                color: "#142737"
                position: 0.0
            }

            GradientStop {
                color: "#0d1a25"
                position: 1.0
            }
        }
    }

    ColumnLayout {
        id: cardLayout

        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredHeight: 38
                Layout.preferredWidth: 38
                border.color: "#244659"
                border.width: 1
                color: "#142a38"
                radius: 11
                visible: root.iconText.length > 0

                Label {
                    anchors.centerIn: parent
                    color: "#d8e4ec"
                    font.letterSpacing: 0.5
                    font.pixelSize: root.iconText.length > 2 ? 10 : 12
                    font.weight: Font.Bold
                    text: root.iconText
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    color: "#f2f6fa"
                    elide: Text.ElideRight
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    text: root.title
                }

                Label {
                    Layout.fillWidth: true
                    color: "#627b8e"
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: root.subtitle
                    visible: root.subtitle.length > 0
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#182d3c"
        }

        ColumnLayout {
            id: body

            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: root.contentSpacing
        }
    }
}
