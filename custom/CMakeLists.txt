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

set(AGRIDRONE_QML_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneAIVisionButton.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneAIVisionDropPanel.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneFlyViewButton.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneFlyViewDropPanel.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/AgriDroneSettings.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionUiAdapter.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionSettings.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionTelemetryTab.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionNetworkingTab.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionCameraTab.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionVisionTab.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionMissionTab.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionLogPanel.qml
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/qml/CompanionUiAdapter.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qgc_set_qt_resource_alias(${AGRIDRONE_QML_FILES})

qt_add_qml_module(AgriDroneModule
    URI Custom.AgriDrone
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        ${AGRIDRONE_QML_FILES}
    NO_PLUGIN
)

# Keep the upstream SettingsPages.json untouched. Generate a custom page model and
# expose it at the resource path recognized by CustomOverrideInterceptor.
set(AGRIDRONE_SETTINGS_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_settings_pages.py")
set(AGRIDRONE_SETTINGS_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/src/AppSettings/SettingsPages.custom.json")
set(AGRIDRONE_SETTINGS_MODEL "${CMAKE_CURRENT_BINARY_DIR}/generated/SettingsPagesModel.qml")

file(GLOB AGRIDRONE_SETTINGS_GENERATOR_DEPENDENCIES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/generators/settings_qml/*.py"
    "${CMAKE_SOURCE_DIR}/tools/generators/settings_qml/templates/*.j2"
    "${CMAKE_SOURCE_DIR}/tools/generators/common/*.py"
    "${CMAKE_SOURCE_DIR}/tools/generators/*.py"
)
file(GLOB AGRIDRONE_SETTINGS_PAGE_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/AppSettings/pages/*.json"
)

add_custom_command(
    OUTPUT "${AGRIDRONE_SETTINGS_MODEL}"
    COMMAND Python3::Interpreter -m custom.tools.generate_settings_pages
        --base-pages-dir "${CMAKE_SOURCE_DIR}/src/AppSettings/pages"
        --custom-pages "${AGRIDRONE_SETTINGS_MANIFEST}"
        --output "${AGRIDRONE_SETTINGS_MODEL}"
    DEPENDS
        "${AGRIDRONE_SETTINGS_GENERATOR}"
        "${AGRIDRONE_SETTINGS_MANIFEST}"
        ${AGRIDRONE_SETTINGS_GENERATOR_DEPENDENCIES}
        ${AGRIDRONE_SETTINGS_PAGE_INPUTS}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating AgriDrone settings page override"
    VERBATIM
)
add_custom_target(GenerateAgriDroneSettingsPages DEPENDS "${AGRIDRONE_SETTINGS_MODEL}")
add_dependencies(AgriDroneModule GenerateAgriDroneSettingsPages)

set_source_files_properties("${AGRIDRONE_SETTINGS_MODEL}"
    PROPERTIES
        GENERATED TRUE
        QT_RESOURCE_ALIAS "QGroundControl/AppSettings/SettingsPagesModel.qml"
)
qt_add_resources(AgriDroneModule "agri_settings_override"
    PREFIX "/Custom/qml"
    FILES "${AGRIDRONE_SETTINGS_MODEL}"
)

set(AGRIDRONE_TEST_SOURCES)
if(QGC_BUILD_TESTING AND NOT ANDROID)
    list(APPEND AGRIDRONE_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test/CompanionControllerTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/CompanionControllerTest.h
    )
    include(QGCTest)
    add_qgc_test(CompanionControllerTest LABELS Unit Vehicle)
    add_qgc_test(CompanionVehicleLifecycleTest LABELS Integration Vehicle)
endif()

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AgriDroneController.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AgriDroneController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionController.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionConfigService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionConfigService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionLinksService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionLinksService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionLogService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionLogService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/AgriDroneComponent.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/AgriDroneComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MAVLink/THACOMAVLink.h
    ${AGRIDRONE_TEST_SOURCES}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MAVLink
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
    CACHE INTERNAL "" FORCE
)
