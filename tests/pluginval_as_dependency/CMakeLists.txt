cmake_minimum_required(VERSION 3.22)
project(PluginvalAsDependency VERSION 1.0.0)

set(JUCE_VERSION "8.0.3" CACHE STRING "JUCE version to test against")

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/CPM.cmake)

CPMAddPackage("gh:juce-framework/JUCE#${JUCE_VERSION}")

# Minimal plugin to test pluginval as a dependency
# Plugin name must match CMAKE_PROJECT_NAME for pluginval's CLI target to work
# Must be defined BEFORE adding pluginval
juce_add_plugin(${CMAKE_PROJECT_NAME}
    PLUGIN_MANUFACTURER_CODE Test
    PLUGIN_CODE Test
    FORMATS VST3 AU
    PRODUCT_NAME "TestPlugin")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE TestPlugin.cpp)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE juce::juce_audio_processors)

CPMAddPackage(
    NAME pluginval
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..
)

target_compile_definitions(pluginval PRIVATE JUCE_DISABLE_JUCE_VERSION_PRINTING=1)

# Verify pluginval runs and reports the expected JUCE version
add_custom_target(verify_pluginval
    COMMAND $<TARGET_FILE:pluginval> --help | grep "JUCE v${JUCE_VERSION}"
    DEPENDS pluginval
    COMMENT "Verify pluginval built with JUCE ${JUCE_VERSION}")
