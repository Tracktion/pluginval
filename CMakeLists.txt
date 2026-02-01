cmake_minimum_required(VERSION 3.15)

file(STRINGS VERSION CURRENT_VERSION LIMIT_COUNT 1)
project(pluginval VERSION ${CURRENT_VERSION})

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

# VST3 Validator integration - embeds Steinberg's vstvalidator directly into pluginval
option(PLUGINVAL_VST3_VALIDATOR "Build with embedded VST3 validator" ON)

if(PLUGINVAL_VST3_VALIDATOR)
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

option(PLUGINVAL_FETCH_JUCE "Fetch JUCE along with pluginval" ON)

if(PLUGINVAL_FETCH_JUCE)
    add_subdirectory(modules/juce)
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

# Add VST3 validator sources if enabled
if(PLUGINVAL_VST3_VALIDATOR)
    set(VST3ValidatorFiles
        Source/vst3validator/VST3ValidatorRunner.h
        Source/vst3validator/VST3ValidatorRunner.cpp
    )

    # Add platform-specific module loading sources from the SDK
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND VST3ValidatorFiles
            ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_linux.cpp)
    elseif(APPLE)
        set(VST3_MODULE_MAC_FILE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_mac.mm)
        list(APPEND VST3ValidatorFiles ${VST3_MODULE_MAC_FILE})
        # module_mac.mm requires ARC (Automatic Reference Counting)
        set_source_files_properties(${VST3_MODULE_MAC_FILE} PROPERTIES
            COMPILE_FLAGS "-fobjc-arc")
    elseif(WIN32)
        set(VST3_MODULE_WIN_FILE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_win32.cpp)
        list(APPEND VST3ValidatorFiles ${VST3_MODULE_WIN_FILE})
        # Add Windows-specific compile flags for compatibility
        if(MSVC)
            set_source_files_properties(${VST3_MODULE_WIN_FILE} PROPERTIES
                COMPILE_FLAGS "/DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING")
        else()
            set_source_files_properties(${VST3_MODULE_WIN_FILE} PROPERTIES
                COMPILE_DEFINITIONS "NOMINMAX;WIN32_LEAN_AND_MEAN;_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING")
        endif()
    endif()

    target_sources(pluginval PRIVATE ${VST3ValidatorFiles})
    source_group("Source/vst3validator" FILES ${VST3ValidatorFiles})

    target_include_directories(pluginval PRIVATE
        ${vst3sdk_SOURCE_DIR}
        ${vst3sdk_SOURCE_DIR}/pluginterfaces
        ${vst3sdk_SOURCE_DIR}/base
        ${vst3sdk_SOURCE_DIR}/public.sdk
        ${vst3sdk_SOURCE_DIR}/public.sdk/source
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting
    )
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

if (PLUGINVAL_VST3_VALIDATOR)
    target_link_libraries(pluginval PRIVATE
        sdk_hosting
        sdk)

    # Windows needs additional libraries for the module loading code
    if(WIN32)
        target_link_libraries(pluginval PRIVATE
            Ole32
            Shell32)
    endif()

    # macOS needs Cocoa framework for module loading
    if(APPLE)
        find_library(COCOA_FRAMEWORK Cocoa)
        target_link_libraries(pluginval PRIVATE
            ${COCOA_FRAMEWORK})
    endif()
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

set (cmdline_docs_out "${CMAKE_CURRENT_LIST_DIR}/docs/Command line options.md")

add_custom_command (OUTPUT "${cmdline_docs_out}"
                    COMMAND pluginval --help > "${cmdline_docs_out}"
                    COMMENT "Regenerating Command line options.md..."
                    USES_TERMINAL)

add_custom_target (PluginvalDocs DEPENDS "${cmdline_docs_out}")
