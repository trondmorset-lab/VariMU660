cmake_minimum_required(VERSION 3.22)

project(VariMu660 VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) og macOS 10.13+
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# JUCE: bruk lokal kopi hvis JUCE_DIR er satt, ellers lastes den ned automatisk
set(JUCE_DIR "" CACHE PATH "Sti til en lokal JUCE-mappe (valgfritt)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(VariMu660
    COMPANY_NAME "Trond Morset"
    BUNDLE_ID "no.trondmorset.varimu660"
    PLUGIN_MANUFACTURER_CODE Tmor
    PLUGIN_CODE Vm66
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "VariMu 660"
    ICON_BIG assets/v-mu-logo-1024.png
    ICON_SMALL assets/v-mu-logo-256.png
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VariMu660 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Logoen bakes inn i pluginen og vises i GUI-et
juce_add_binary_data(VariMuAssets SOURCES assets/v-mu-logo-256.png)

target_compile_definitions(VariMu660 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VariMu660
    PRIVATE
        VariMuAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Valgfrie tester (av som standard): cmake -DVARIMU_BUILD_TESTS=ON
option(VARIMU_BUILD_TESTS "Bygg testprogrammer" OFF)
if(VARIMU_BUILD_TESTS)
    add_executable(DspTest Tests/DspTest.cpp)
    juce_add_console_app(GuiSnapshot PRODUCT_NAME "GuiSnapshot")
    target_sources(GuiSnapshot PRIVATE Tests/GuiSnapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(GuiSnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="VariMu 660")
    target_link_libraries(GuiSnapshot PRIVATE VariMuAssets juce::juce_audio_utils juce::juce_dsp)
endif()
