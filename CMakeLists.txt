cmake_minimum_required(VERSION 3.22)
project(AetherPiano VERSION 0.1.0 LANGUAGES C CXX)

option(AETHER_BUILD_PLUGIN "Build JUCE plugin targets when JUCE is available" ON)
option(AETHER_BUILD_TESTS "Build native tests" ON)

add_library(PianoDSP src/dsp/PianoEngine.cpp)
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

if(AETHER_BUILD_TESTS)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(aether_tests tests/test_main.cpp)
  target_link_libraries(aether_tests PRIVATE PianoDSP)
  add_test(NAME aether_tests COMMAND aether_tests)
  add_test(NAME reference_solver_tests
    COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -p test_reference_solver.py)
  set_tests_properties(reference_solver_tests PROPERTIES
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
  else()
    message(FATAL_ERROR "JUCE missing. Initialize pinned submodule: git submodule update --init --recursive")
  endif()
endif()
