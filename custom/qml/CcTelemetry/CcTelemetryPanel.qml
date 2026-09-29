pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool animateData: false
    property bool previewMode: false
    required property var telemetry

    signal resetRequested

    color: "transparent"

    ColumnLayout {
        anchors.bottomMargin: 22
        anchors.fill: parent
        anchors.leftMargin: Math.max(28, parent.width * 0.045)
        anchors.rightMargin: Math.max(28, parent.width * 0.045)
        anchors.topMargin: 0
        spacing: 0

        RowLayout {
            Layout.bottomMargin: 27
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                spacing: 5

                Label {
                    color: "#f7f9fc"
                    font.letterSpacing: -0.5
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    text: qsTr("CC Telemetry")
                }

                Label {
                    color: "#7890a6"
                    font.pixelSize: 14
                    text: qsTr("Companion computer telemetry overview")
                }
            }

            Item {
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 10
                visible: root.previewMode

                StatusBadge {
                    active: true
                    activeColor: "#48c8ff"
                    text: qsTr("MOCK DATA")
                }

                Switch {
                    id: animateSwitch

                    checked: root.animateData
                    text: qsTr("Animate")

                    contentItem: Label {
                        color: "#8ba0b1"
                        font.pixelSize: 11
                        leftPadding: animateSwitch.indicator.width + animateSwitch.spacing
                        text: animateSwitch.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    onToggled: root.animateData = checked
                }

                Button {
                    flat: true
                    text: qsTr("Reset")

                    contentItem: Label {
                        color: parent.hovered ? "#d9e3ea" : "#8ba0b1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        text: parent.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.resetRequested()
                }
            }
        }

        TabBar {
            id: tabBar

            Layout.bottomMargin: 22
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            spacing: 34

            background: Rectangle {
                color: "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    color: "#3a5a78"
                    height: 2
                }
            }

            Repeater {
                model: [qsTr("Links"), qsTr("Camera"), qsTr("Network"), qsTr("Vision")]

                TabButton {
                    id: tabButton

                    required property int index
                    required property string modelData

                    font.pixelSize: 14
                    font.weight: tabBar.currentIndex === tabButton.index ? Font.DemiBold : Font.Normal
                    text: modelData
                    width: Math.max(108, implicitWidth)

                    contentItem: Text {
                        text: tabButton.text
                        font: tabButton.font
                        color: tabBar.currentIndex === tabButton.index ? "#f5c84c" : "#788da1"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "transparent"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#f5c84c"
                            height: 3
                            radius: 2
                            visible: tabBar.currentIndex === tabButton.index
                            width: 34
                        }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: tabBar.currentIndex

            LinksTab {
                telemetry: root.telemetry
            }

            CameraTab {
                telemetry: root.telemetry
            }

            NetworkTab {
                telemetry: root.telemetry
            }

            VisionTab {
                telemetry: root.telemetry
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            color: "#465d70"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
            text: qsTr("Preview source: THACO MAVLink dialect  |  Ctrl+R reloads QML")
            visible: root.previewMode
        }
    }
}
