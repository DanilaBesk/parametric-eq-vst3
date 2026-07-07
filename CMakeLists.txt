cmake_minimum_required(VERSION 3.22)

project(AIAgentParametricEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

option(BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)

enable_testing()

add_library(eq_dsp INTERFACE)
target_include_directories(eq_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(eq_dsp INTERFACE cxx_std_20)

add_executable(eq_dsp_tests tests/EqDspTests.cpp)
target_link_libraries(eq_dsp_tests PRIVATE eq_dsp)
add_test(NAME eq_dsp_tests COMMAND eq_dsp_tests)

if(BUILD_PLUGIN)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  include(FetchContent)
  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(AIAgentParametricEQ
    COMPANY_NAME "Danila AI Audio"
    PLUGIN_MANUFACTURER_CODE DaAI
    PLUGIN_CODE Apeq
    FORMATS VST3 Standalone
    PRODUCT_NAME "AI Agent Parametric EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    BUNDLE_ID com.danila-ai-audio.AIAgentParametricEQ
  )

  juce_generate_juce_header(AIAgentParametricEQ)

  target_sources(AIAgentParametricEQ PRIVATE
    src/plugin/PluginProcessor.h
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.h
    src/plugin/PluginEditor.cpp
  )

  target_include_directories(AIAgentParametricEQ PRIVATE src)

  target_compile_definitions(AIAgentParametricEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
  )

  target_link_libraries(AIAgentParametricEQ PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
  )

  if(APPLE)
    add_custom_command(TARGET AIAgentParametricEQ_VST3 POST_BUILD
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/normalize_moduleinfo.py"
              "$<TARGET_BUNDLE_DIR:AIAgentParametricEQ_VST3>/Contents/Resources/moduleinfo.json"
      COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:AIAgentParametricEQ_VST3>"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/normalize_moduleinfo.py"
              "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/AI Agent Parametric EQ.vst3/Contents/Resources/moduleinfo.json"
      COMMAND /usr/bin/codesign --force --sign - "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/AI Agent Parametric EQ.vst3"
      VERBATIM
    )
  elseif(WIN32)
    add_custom_command(TARGET AIAgentParametricEQ_VST3 POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E echo "Windows VST3 built. Install/copy the .vst3 bundle to C:/Program Files/Common Files/VST3 for FL Studio scan."
      VERBATIM
    )
  endif()
endif()
