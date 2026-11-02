cmake_minimum_required(VERSION 3.22)
project(AetherPiano VERSION 0.1.0 LANGUAGES C CXX)

option(AETHER_BUILD_PLUGIN "Build JUCE plugin targets when JUCE is available" ON)
option(AETHER_BUILD_TESTS "Build native tests" ON)

add_library(PianoDSP
  src/dsp/PerformanceEngine.cpp
  src/dsp/PianoEngine.cpp
  src/dsp/string/DispersionFilter.cpp
  src/dsp/string/FractionalDelay.cpp
  src/dsp/string/HammerContact.cpp
  src/dsp/string/LossFilter.cpp
  src/dsp/string/WaveguideString.cpp
  src/model/NoteParameters.cpp)
add_library(AetherPiano::DSP ALIAS PianoDSP)
target_include_directories(PianoDSP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(PianoDSP PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(PianoDSP PRIVATE /W4 /permissive-)
else()
  target_compile_options(PianoDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(offline_render tools/offline_render/main.cpp)
target_link_libraries(offline_render PRIVATE PianoDSP)

add_executable(rt_bench tools/rt_bench/main.cpp)
target_link_libraries(rt_bench PRIVATE PianoDSP)

add_executable(performance_render tools/performance_render/main.cpp)
target_link_libraries(performance_render PRIVATE PianoDSP)

add_executable(polyphony_bench tools/polyphony_bench/main.cpp)
target_link_libraries(polyphony_bench PRIVATE PianoDSP)

if(AETHER_BUILD_TESTS)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(aether_tests tests/test_main.cpp)
  target_link_libraries(aether_tests PRIVATE PianoDSP)
  add_test(NAME aether_tests COMMAND aether_tests)
  add_executable(performance_tests tests/test_performance.cpp)
  target_link_libraries(performance_tests PRIVATE PianoDSP)
  add_test(NAME performance_tests COMMAND performance_tests)
  add_test(NAME reference_solver_tests
    COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -p test_reference_solver.py)
  add_test(NAME p02_feature_regression
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/analyzer/analyze_p02.py
      --render $<TARGET_FILE:offline_render>)
  add_test(NAME p03_pitch_regression
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/analyzer/analyze_p03.py
      --renderer $<TARGET_FILE:performance_render>)
  set_tests_properties(reference_solver_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(p02_feature_regression PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(p03_pitch_regression PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(AETHER_BUILD_PLUGIN)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)
    juce_add_plugin(AetherPianoPlugin
      COMPANY_NAME "Aether Piano"
      BUNDLE_ID "audio.aether.piano"
      PLUGIN_MANUFACTURER_CODE Aeth
      PLUGIN_CODE AeP0
      FORMATS VST3 Standalone AU
      PRODUCT_NAME "Aether Piano"
      IS_SYNTH TRUE
      NEEDS_MIDI_INPUT TRUE
      NEEDS_MIDI_OUTPUT FALSE
      COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(AetherPianoPlugin PRIVATE src/plugin/PluginProcessor.cpp)
    target_include_directories(AetherPianoPlugin PRIVATE src)
    target_link_libraries(AetherPianoPlugin PRIVATE PianoDSP juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    target_compile_definitions(AetherPianoPlugin PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

    add_executable(plugin_callback_bench
      tools/plugin_callback_bench/main.cpp
      src/plugin/PluginProcessor.cpp)
    target_include_directories(plugin_callback_bench PRIVATE src)
    target_link_libraries(plugin_callback_bench PRIVATE PianoDSP juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_compile_definitions(plugin_callback_bench PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  else()
    message(FATAL_ERROR "JUCE missing. Initialize pinned submodule: git submodule update --init --recursive")
  endif()
endif()
