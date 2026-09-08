pragma ComponentBehavior: Bound

import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.Viewer3D
import Custom.AgriDrone

ToolStripActionList {
    id: _root

    signal displayPreFlightChecklist

    model: [
        Viewer3DShowAction { },
        // The action is a nested component which forwards to the outer list signal.
        // qmllint disable unqualified
        PreFlightCheckListShowAction { onTriggered: _root.displayPreFlightChecklist() },
        // qmllint enable unqualified
        GuidedActionTakeoff { },
        GuidedActionLand { },
        GuidedActionRTL { },
        GuidedActionPause { },
        FlyViewAdditionalActionsButton { },
        AgriDroneFlyViewButton { },
        FlyViewGripperButton { }
    ]
}
