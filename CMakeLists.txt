cmake_minimum_required(VERSION 3.15)

file(STRINGS VERSION CURRENT_VERSION LIMIT_COUNT 1)
project(pluginval VERSION ${CURRENT_VERSION})

# Just compliing pluginval
if (pluginval_IS_TOP_LEVEL)
    if (APPLE)
        # Target OS versions down to 10.11
        set (CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE INTERNAL "")

        # Uncomment to produce a universal binary
        # set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
        set(PLUGINVAL_ENABLE_RTCHECK ON)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Disable rtcheck on Linux for now until further testing on real Linux systems has been done
        # set(PLUGINVAL_ENABLE_RTCHECK ON)
    endif()
else()
    # compiling as a "dependency" of another JUCE CMake project
    if (NOT COMMAND juce_add_module)
        message(FATAL_ERROR "JUCE must be added to your project before pluginval!")
    endif ()
endif()

# sanitizer options, from https://github.com/sudara/cmake-includes/blob/main/Sanitizers.cmake
# set the corresponding option ON to turn on Address/Thread Sanitizer
option(WITH_ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)
option(WITH_THREAD_SANITIZER "Enable Thread Sanitizer" OFF)

message(STATUS "Sanitizers: ASan=${WITH_ADDRESS_SANITIZER} TSan=${WITH_THREAD_SANITIZER}")
if (WITH_ADDRESS_SANITIZER)
    if (MSVC)
        add_compile_options(/fsanitize=address)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # also enable UndefinedBehaviorSanitizer
        # https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        link_libraries(-fsanitize=address)
    endif ()
    message("Address Sanitizer enabled")
endif ()

if (WITH_THREAD_SANITIZER)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=thread -g -fno-omit-frame-pointer)
        link_libraries(-fsanitize=thread)
        message("Thread Sanitizer enabled")
    endif ()
endif ()

# Adds all the module sources so they appear correctly in the IDE
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

include(cmake/CPM.cmake)
CPMAddPackage("gh:Neargye/magic_enum#v0.9.7")

if(PLUGINVAL_ENABLE_RTCHECK)
    CPMAddPackage("gh:Tracktion/rtcheck#main")
endif()

# Only fetch JUCE when top-level (when used as dependency, JUCE is already available)
if (pluginval_IS_TOP_LEVEL)
    CPMAddPackage("gh:juce-framework/juce#8.0.3")
endif()


# VST3 Validator integration - embeds Steinberg's vstvalidator directly into pluginval
option(PLUGINVAL_VST3_VALIDATOR "Build with embedded VST3 validator" ON)

if(PLUGINVAL_VST3_VALIDATOR)
    # Use static CRT on Windows to match pluginval's settings
    set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)

    CPMAddPackage(
        NAME vst3sdk
        GITHUB_REPOSITORY steinbergmedia/vst3sdk
        GIT_TAG v3.7.14_build_55
        OPTIONS
            "SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF"
            "SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF"
            "SMTG_ENABLE_VSTGUI_SUPPORT OFF"
            "SMTG_ADD_VSTGUI OFF"
            "SMTG_RUN_VST_VALIDATOR OFF"
            "SMTG_CREATE_BUNDLE_FOR_WINDOWS OFF"
    )
endif()

if (DEFINED ENV{VST2_SDK_DIR})
    MESSAGE(STATUS "Building with VST2 SDK: $ENV{VST2_SDK_DIR}")
    juce_set_vst2_sdk_path($ENV{VST2_SDK_DIR})
else()
    MESSAGE(STATUS "Not building with VST2")
endif()

juce_add_gui_app(pluginval
    BUNDLE_ID                       com.Tracktion.pluginval
    COMPANY_NAME                    Tracktion
    ICON_BIG                        "${CMAKE_CURRENT_SOURCE_DIR}/Source/binarydata/icon.png"
    HARDENED_RUNTIME_ENABLED        TRUE
    HARDENED_RUNTIME_OPTIONS        com.apple.security.cs.allow-unsigned-executable-memory com.apple.security.cs.disable-library-validation com.apple.security.get-task-allow)

juce_generate_juce_header(pluginval)

target_compile_features(pluginval PRIVATE cxx_std_20)

set_target_properties(pluginval PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

set(SourceFiles
    Source/CommandLine.h
    Source/CrashHandler.h
    Source/MainComponent.h
    Source/PluginTests.h
    Source/TestUtilities.h
    Source/Validator.h
    Source/CommandLine.cpp
    Source/CrashHandler.cpp
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/PluginTests.cpp
    Source/tests/BasicTests.cpp
    Source/tests/LocaleTest.cpp
    Source/tests/BusTests.cpp
    Source/tests/ParameterFuzzTests.cpp
    Source/TestUtilities.cpp
    Source/Validator.cpp)

target_sources(pluginval PRIVATE ${SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX Source FILES ${SourceFiles})

# Add VST3 validator runner sources to pluginval (extracts embedded binary at runtime)
if(PLUGINVAL_VST3_VALIDATOR)
    set(VST3ValidatorFiles
        Source/vst3validator/VST3ValidatorRunner.h
        Source/vst3validator/VST3ValidatorRunner.cpp
    )
    target_sources(pluginval PRIVATE ${VST3ValidatorFiles})
    source_group("Source/vst3validator" FILES ${VST3ValidatorFiles})
endif()

if (DEFINED ENV{VST2_SDK_DIR})
    target_compile_definitions(pluginval PRIVATE
        JUCE_PLUGINHOST_VST=1)
endif()

target_compile_definitions(pluginval PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_GUI_BASICS_INCLUDE_XHEADERS=1
    $<$<BOOL:${PLUGINVAL_ENABLE_RTCHECK}>:PLUGINVAL_ENABLE_RTCHECK=1>
    $<$<BOOL:${PLUGINVAL_VST3_VALIDATOR}>:PLUGINVAL_VST3_VALIDATOR=1>
    VERSION="${CURRENT_VERSION}")


if(MSVC AND NOT CMAKE_MSVC_RUNTIME_LIBRARY)
    # Default to statically linking the runtime libraries
    set_property(TARGET pluginval PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(pluginval PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_warning_flags
    magic_enum)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(pluginval PRIVATE
        -static-libstdc++)
endif()

# Embed the SDK's validator binary into pluginval
if(PLUGINVAL_VST3_VALIDATOR)
    set(VSTVALIDATOR_HEADER "${CMAKE_BINARY_DIR}/generated/vstvalidator_data.h")
    add_custom_command(
        OUTPUT "${VSTVALIDATOR_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=$<TARGET_FILE:validator>
            -DOUTPUT_FILE=${VSTVALIDATOR_HEADER}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBinaryHeader.cmake
        DEPENDS validator
        COMMENT "Embedding vstvalidator binary into header...")

    target_sources(pluginval PRIVATE "${VSTVALIDATOR_HEADER}")
    target_include_directories(pluginval PRIVATE "${CMAKE_BINARY_DIR}/generated")
endif()

if (PLUGINVAL_ENABLE_RTCHECK)
    target_link_libraries(pluginval PRIVATE
        rtcheck)

    if (APPLE)
        add_custom_command(TARGET pluginval POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Executable path: $<TARGET_FILE_DIR:pluginval>")
        add_custom_command(TARGET pluginval POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    $<TARGET_FILE:rtcheck>
                    $<TARGET_FILE_DIR:pluginval>/../Libraries/librtcheck.dylib)
    endif ()
endif()

if (pluginval_IS_TOP_LEVEL)
    set (cmdline_docs_out "${CMAKE_CURRENT_LIST_DIR}/docs/Command line options.md")

    add_custom_command (OUTPUT "${cmdline_docs_out}"
                        COMMAND pluginval --help > "${cmdline_docs_out}"
                        COMMENT "Regenerating Command line options.md..."
                        USES_TERMINAL)
    add_custom_target (PluginvalDocs DEPENDS "${cmdline_docs_out}")
else()
    # Custom pluginval CLI target
    set(PLUGINVAL_STRICTNESS_LEVEL 10 CACHE STRING "Pluginval --strictness argument (1-10)")
    set_property(CACHE PLUGINVAL_STRICTNESS_LEVEL PROPERTY STRINGS 1 2 3 4 5 6 7 8 9 10)

    # Set the target based on the platform
    # Makes the assumption both are being built
    if(APPLE)
        set(PLUGINVAL_TARGET "${CMAKE_PROJECT_NAME}_AU")
    else()
        set(PLUGINVAL_TARGET "${CMAKE_PROJECT_NAME}_VST3")
    endif()

    get_target_property(artefact ${PLUGINVAL_TARGET} JUCE_PLUGIN_ARTEFACT_FILE)

    # TODO: This doesn't populate the executable in clion
    add_custom_target(${CMAKE_PROJECT_NAME}_pluginval_cli
            COMMAND $<TARGET_FILE:pluginval>
            --validate ${artefact}
            --strictness-level 10
            DEPENDS pluginval ${PLUGINVAL_TARGET}
            COMMENT "Run pluginval CLI with strict validation")
endif()
