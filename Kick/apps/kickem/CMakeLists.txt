cmake_minimum_required(VERSION 3.15)

# KT_KickEM: desktop simulator. Runs the KICK app (same start-up as apps/kick) and replaces the
# serial control surface with an on-screen panel (SIM_BUILD=1, see input_events/kick_SimulatorInterface.h).

set(TargetName KT_KickEM)
set(TargetVersion 0.3.0)
set(TargetCompany MusicTribe)

project(${TargetName} VERSION ${TargetVersion})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# The other apps generate config.h; the simulator had fallen out of step and did not.
set(TargetSrc_DIR ".")
configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

juce_add_gui_app(${TargetName}
        PRODUCT_NAME ${TargetName}
        COMPANY_NAME ${TargetCompany}
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "KICK records and samples from the audio input."
)

set_target_properties(${TargetName} PROPERTIES
        CXX_STANDARD                    17
        VS_DEBUGGER_WORKING_DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Put the app next to assets/, demos/ and templates/ (copied to the build root), like the Kick app.
if(MSVC OR APPLE)
    set_target_properties(${TargetName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
    )
endif()

target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../kick   # adapters shared with the Kick app
)

target_sources(${TargetName}
    PRIVATE
        Main.cpp
        simulator/kick_Simulator.h
        simulator/kick_Simulator.cpp
)

target_compile_definitions(${TargetName}
    PRIVATE
        SIM_BUILD=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_PROJECT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_VERSION>"
        JUCE_ALSA=1
        JUCE_JACK=1

        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCER_ENABLE_GPL_MODE=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_ASIO=0
        JUCE_ENABLE_REPAINT_DEBUGGING=0

        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1

        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_VST3=${USE_VST}
        JUCE_PLUGINHOST_AU=${USE_AU}

        THPAR_SQLITE=1
        USE_OPTICK=${USE_OPTICK_VAL}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(${TargetName} PRIVATE "-latomic")
endif()

target_link_libraries(${TargetName}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags

    PRIVATE
        audio_engine
        kick_ui
        program_edit_ui
        juce::juce_gui_extra
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        contracts
        ipc
)
