import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGC as QGCNative

GridLayout {
    id: root

    columns:       2
    columnSpacing: ScreenTools.defaultFontPixelWidth * 2
    rowSpacing:    ScreenTools.defaultFontHeight / 2

    QGCLabel {
        text: qsTr("Bounding Box")
    }

    QGCCheckBoxSlider {
        objectName:       "aiVisionBoundingBoxSwitch"
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        checked:          QGCNative.CompanionController.boundingBoxEnabled

        onClicked: QGCNative.CompanionController.boundingBoxEnabled = checked
    }

    QGCLabel {
        text:    qsTr("Tracking")
        enabled: QGCNative.CompanionController.boundingBoxEnabled
    }

    QGCCheckBoxSlider {
        objectName:       "aiVisionTrackingSwitch"
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        checked:          QGCNative.CompanionController.trackingEnabled
        enabled:          QGCNative.CompanionController.boundingBoxEnabled

        onClicked: QGCNative.CompanionController.trackingEnabled = checked
    }

    QGCLabel {
        text:    qsTr("Following")
        enabled: QGCNative.CompanionController.boundingBoxEnabled
    }

    QGCCheckBoxSlider {
        objectName:       "aiVisionFollowingSwitch"
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        checked:          QGCNative.CompanionController.followingEnabled
        enabled:          QGCNative.CompanionController.boundingBoxEnabled

        onClicked: QGCNative.CompanionController.followingEnabled = checked
    }
}
