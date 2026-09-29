cmake_minimum_required(VERSION 3.25)

project(CCQmlPreview VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.11.1)

qt_add_executable(cc-qml-preview
    src/main.cpp
)

set(SHARED_CC_TELEMETRY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../custom/qml/CcTelemetry")
set(SHARED_CC_TELEMETRY_ADAPTER "${SHARED_CC_TELEMETRY_DIR}/CcTelemetryAdapter.qml")
set_source_files_properties(${SHARED_CC_TELEMETRY_ADAPTER} PROPERTIES QT_RESOURCE_ALIAS CcTelemetryAdapter.qml)

qt_add_qml_module(cc-qml-preview
    URI CC.TelemetryPreview
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/MockCcTelemetry.qml
        ${SHARED_CC_TELEMETRY_ADAPTER}
)

target_compile_definitions(cc-qml-preview PRIVATE
    PREVIEW_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    PREVIEW_SHARED_QML_SOURCE_DIR="${SHARED_CC_TELEMETRY_DIR}"
)

target_link_libraries(cc-qml-preview PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(cc-qml-preview PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "CC Telemetry Preview"
    WIN32_EXECUTABLE FALSE
)
