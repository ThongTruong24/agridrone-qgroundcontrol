# ============================================================================
# AgriDrone Custom Build
# ============================================================================

message(STATUS "QGC: Adding AgriDrone custom build")

list(APPEND CUSTOM_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc
)
list(REMOVE_DUPLICATES CUSTOM_RESOURCES)
list(APPEND QGC_RESOURCES ${CUSTOM_RESOURCES})
list(REMOVE_DUPLICATES QGC_RESOURCES)
set(QGC_RESOURCES ${QGC_RESOURCES} CACHE STRING "Paths to .qrc Resources" FORCE)

qt_add_library(AgriDroneModule STATIC)

qgc_set_qt_resource_alias(
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneFlyViewButton.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneFlyViewDropPanel.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneSettings.qml
)

qt_add_qml_module(AgriDroneModule
    URI Custom.AgriDrone
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneFlyViewButton.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneFlyViewDropPanel.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneSettings.qml
    NO_PLUGIN
)

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AgriDroneController.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AgriDroneController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/AgriDroneComponent.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/AgriDroneComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.h
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_LIBRARIES
    AgriDroneModule
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
    CACHE INTERNAL "" FORCE
)
