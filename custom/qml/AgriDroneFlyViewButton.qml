import QtQuick

import QGroundControl
import QGC as QGCNative

ToolStripAction {
    id:         root
    objectName: "flyToolStrip_agriDroneActuatorButton"
    text:       qsTr("Actuator")
    iconSource: "/custom/images/control_actuator.png"
    visible:    true
    enabled:    QGCNative.AgriDroneController.enabled &&
                    QGCNative.AgriDroneController.anyActuatorAvailable

    onEnabledChanged: {
        if (!root.enabled) {
            root.checked = false
        }
    }

    dropPanelComponent: Component {
        AgriDroneFlyViewDropPanel { }
    }
}
