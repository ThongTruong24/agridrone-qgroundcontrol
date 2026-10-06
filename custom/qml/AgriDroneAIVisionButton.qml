pragma ComponentBehavior: Bound

import QGroundControl

ToolStripAction {
    id: root

    objectName: "flyToolStrip_agriDroneAIVisionButton"
    text:       qsTr("AI Vision")
    iconSource: "qrc:/InstrumentValueIcons/view-show.svg"
    visible:    true
    enabled:    true

    dropPanelComponent: Component {
        AgriDroneAIVisionDropPanel { }
    }
}
