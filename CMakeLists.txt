cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
project(LunaTpAlpha VERSION 0.2.1 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "Luna Tp's verified target is Windows x64 with MSVC.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(LUNA_BUILD_TESTS "Build the Luna Tp core tests and VST3 test host" ON)

include(FetchContent)
set(LUNA_JUCE_VERSION "9.0.5")
set(LUNA_JUCE_URL "https://codeload.github.com/juce-framework/JUCE/zip/refs/tags/${LUNA_JUCE_VERSION}")
set(LUNA_JUCE_ARCHIVE "" CACHE FILEPATH "Optional offline JUCE 9.0.5 ZIP with the official SHA-256")
if(LUNA_JUCE_ARCHIVE)
    set(LUNA_JUCE_URL "${LUNA_JUCE_ARCHIVE}")
endif()
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(juce
    URL "${LUNA_JUCE_URL}"
    URL_HASH SHA256=db891ee990a5ec8b58739f24e1ac62d8abd584205256a356c414a083d8927168
    TLS_VERIFY TRUE
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(juce)

add_library(LunaCore STATIC Source/LunaCore.cpp)
target_include_directories(LunaCore PUBLIC Source)
juce_add_plugin(LunaTp COMPANY_NAME "Luna Tp" BUNDLE_ID "org.lunatp.alpha"
    PLUGIN_MANUFACTURER_CODE LnTp PLUGIN_CODE Ln01 FORMATS VST3
    PRODUCT_NAME "Luna Tp" IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
target_sources(LunaTp PRIVATE Source/PluginProcessor.cpp)
target_link_libraries(LunaTp PRIVATE LunaCore juce::juce_audio_processors juce::juce_recommended_config_flags)
target_compile_definitions(LunaTp PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

if(LUNA_BUILD_TESTS)
    enable_testing()
    add_executable(LunaCoreTest Tests/CoreTest.cpp)
    target_link_libraries(LunaCoreTest PRIVATE LunaCore)
    add_test(NAME LunaCore COMMAND LunaCoreTest "${CMAKE_CURRENT_BINARY_DIR}/Core-B4.raw" 71 0.25 100 48000)
    add_executable(LunaPerformanceTest Tests/PerformanceTest.cpp)
    target_link_libraries(LunaPerformanceTest PRIVATE LunaCore)
    add_test(NAME LunaPerformance COMMAND LunaPerformanceTest)
    juce_add_console_app(LunaHostTest PRODUCT_NAME "Luna Tp Test Host")
    target_sources(LunaHostTest PRIVATE Tests/HostRender.cpp Source/PluginProcessor.cpp)
    target_compile_definitions(LunaHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(LunaHostTest PRIVATE LunaCore juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
