cmake_minimum_required(VERSION 3.24)

project(
    TreadmillLocomotionVR
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

find_package(CommonLibSSE CONFIG REQUIRED)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "rclae and Codex"
    NAME "Treadmill Locomotion VR"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY VR
)

target_sources(
    ${PROJECT_NAME}
    PRIVATE
        src/main.cpp
        src/LocomotionAnalysis.cpp
        src/LocomotionAnalysis.h
        src/Settings.cpp
        src/Settings.h
        src/Telemetry.cpp
        src/Telemetry.h
        src/Version.h
        src/XInputTelemetryProbe.cpp
        src/XInputTelemetryProbe.h
        src/PCH.h
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE TLV_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /EHsc)
endif()

install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION SKSE/Plugins
)

install(
    FILES config/TreadmillLocomotionVR.ini
    DESTINATION SKSE/Plugins
)

include(CTest)

if(BUILD_TESTING)
    add_executable(
        TreadmillLocomotionVRTests
        tests/LocomotionAnalysisTests.cpp
        src/LocomotionAnalysis.cpp
        src/LocomotionAnalysis.h
    )
    target_include_directories(TreadmillLocomotionVRTests PRIVATE src)
    target_compile_features(TreadmillLocomotionVRTests PRIVATE cxx_std_23)
    add_test(NAME TreadmillLocomotionVRTests COMMAND TreadmillLocomotionVRTests)
endif()
