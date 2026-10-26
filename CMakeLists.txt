cmake_minimum_required(VERSION 3.22)
project(AntrixSnap VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
option(SNAP_TESTS_ONLY "Build DSP tests without JUCE" OFF)
enable_testing()
add_executable(SnapTests tests/EngineTests.cpp)
target_include_directories(SnapTests PRIVATE Source)
add_test(NAME SnapEngine COMMAND SnapTests)
if(SNAP_TESTS_ONLY)
  return()
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(formats VST3 Standalone)
if(APPLE)
  list(APPEND formats AU)
endif()
juce_add_binary_data(SnapArt SOURCES Assets/reference.jpg Assets/hand.png)
juce_add_plugin(Snap COMPANY_NAME "Antrix Studioz" BUNDLE_ID "com.antrixstudioz.snap"
  PLUGIN_MANUFACTURER_CODE Antr PLUGIN_CODE SnAp PRODUCT_NAME "Snap"
  IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE FORMATS ${formats}
  VST3_CATEGORIES Instrument Drum AU_MAIN_TYPE kAudioUnitType_MusicDevice)
target_sources(Snap PRIVATE Source/Plugin.cpp)
target_compile_definitions(Snap PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Snap PRIVATE SnapArt juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

