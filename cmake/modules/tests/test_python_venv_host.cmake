# Run with: cmake -P cmake/modules/tests/test_python_venv_host.cmake
cmake_minimum_required(VERSION 3.25)

# Keep this path independent of the checkout's real Python environment.
set(CMAKE_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/qgc-python-venv-host-fixture")
set(QGC_AUTO_PYTHON_VENV OFF CACHE BOOL "" FORCE)
# An explicitly supplied interpreter skips bootstrap and dependency installation.
set(Python3_EXECUTABLE "explicit-python" CACHE FILEPATH "" FORCE)

foreach(host_windows IN ITEMS TRUE FALSE)
    foreach(target_windows IN ITEMS TRUE FALSE)
        set(CMAKE_HOST_WIN32 ${host_windows})
        set(WIN32 ${target_windows})
        include("${CMAKE_CURRENT_LIST_DIR}/../PythonVenv.cmake")
        if(host_windows)
            set(expected "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
        else()
            set(expected "${CMAKE_SOURCE_DIR}/.venv/bin/python")
        endif()
        if(NOT _qgc_venv_python STREQUAL expected)
            message(FATAL_ERROR
                "Host Windows=${host_windows}, target Windows=${target_windows}: "
                "expected ${expected}, got ${_qgc_venv_python}")
        endif()
    endforeach()
endforeach()
message(STATUS "Python venv paths match the host for native and cross builds")
