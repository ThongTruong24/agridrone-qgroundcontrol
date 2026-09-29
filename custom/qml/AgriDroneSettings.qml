
import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGC as QGCNative
import Custom.AgriDrone

SetupPage {
    id: setupPage
    pageComponent: pageComponent

    Component {
        id: pageComponent

        ColumnLayout {
            width: setupPage.availableWidth

            QGCCheckBoxSlider {
                Layout.fillWidth: true
                objectName: "agriDroneSwitch"
                checked: QGCNative.AgriDroneController.enabled
                text:    checked ? qsTr("ON") : qsTr("OFF")

                onClicked: QGCNative.AgriDroneController.enabled = checked
            }
        }
    }
}
