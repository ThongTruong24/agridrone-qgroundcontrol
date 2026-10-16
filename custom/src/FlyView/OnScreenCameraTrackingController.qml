import QtQuick

import QGC as QGCNative

Item {
    id: rootItem

    required property var  camera
    required property real videoWidth
    required property real videoHeight

    function mouseClicked(mouseX, mouseY) {
        if (!QGCNative.CompanionController.boundingBoxEnabled || videoWidth <= 0 || videoHeight <= 0) {
            return
        }

        const marginH = (width - videoWidth) / 2
        const marginV = (height - videoHeight) / 2
        if (mouseX < marginH || mouseX > marginH + videoWidth ||
                mouseY < marginV || mouseY > marginV + videoHeight) {
            return
        }

        const normalizedX = (mouseX - marginH) / videoWidth
        const normalizedY = (mouseY - marginV) / videoHeight
        const normalizedRadius = Math.min(20 / videoWidth, 1.0)
        QGCNative.CompanionController.sendAiVisionTrackPoint(normalizedX, normalizedY, normalizedRadius)
    }

    function mouseDragStart(mouseX, mouseY) {
    }

    function mouseDragPositionChanged(mouseX, mouseY) {
    }

    function mouseDragEnd(mouseX, mouseY) {
    }
}
