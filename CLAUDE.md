# CLAUDE.md - AI Assistant Guide for pluginval

## Project Overview

**pluginval** is a cross-platform audio plugin validator and tester application developed by Tracktion Corporation. It tests VST, VST3, AU (Audio Unit), LV2, and LADSPA plugins for compatibility and stability with host applications.

- **Version**: 1.0.4 (see `VERSION` file)
- **License**: GPLv3
- **Framework**: Built on JUCE (v8.0.x)
- **Language**: C++20

### Key Features
- Tests VST/VST2/VST3/AU/LV2/LADSPA plugins
- Cross-platform (macOS, Windows, Linux)
- GUI and headless (CLI) operation modes
- Validation runs in a separate process to prevent crashes from bringing down the app
- Real-time safety checking via rtcheck (macOS only currently)
- Integration with native validators (auval for AU, vstvalidator for VST3)

## Important Instructions for Claude

Follow these guidelines when working on this codebase:

1. **Think first, then read**: Before making changes, think through the problem and read relevant files in the codebase. Understand the existing code before proposing modifications.

2. **Verify plans before major changes**: Before making any major changes, check in with the user to verify the plan. Get confirmation before proceeding with significant modifications.

3. **Provide high-level explanations**: At every step, give a high-level explanation of what changes were made. Keep explanations concise and focused on the "what" and "why."

4. **Keep changes simple**: Make every task and code change as simple as possible. Avoid massive or complex changes. Every change should impact as little code as possible. Simplicity is paramount.

5. **Maintain architecture documentation**: Keep this documentation file updated to describe how the architecture of the app works. Update relevant sections when making architectural changes.

6. **Never speculate about unread code**: Never make claims about code you haven't opened. If the user references a specific file, you MUST read the file before answering. Investigate and read relevant files BEFORE answering questions about the codebase. Give grounded, hallucination-free answers based on actual file contents.

## Directory Structure

```
pluginval/
├── Source/                    # Main application source code
│   ├── Main.cpp              # Application entry point
│   ├── MainComponent.cpp/h   # GUI main window component
│   ├── Validator.cpp/h       # Core validation orchestration
│   ├── PluginTests.cpp/h     # Test framework and base classes
│   ├── CommandLine.cpp/h     # CLI argument parsing
│   ├── CrashHandler.cpp/h    # Crash reporting utilities
│   ├── TestUtilities.cpp/h   # Helper functions for tests
│   ├── RTCheck.h             # Real-time safety checking macros
│   ├── PluginvalLookAndFeel.h # Custom UI styling
│   ├── StrictnessInfoPopup.h # Strictness level info UI
│   ├── binarydata/           # Binary resources (icons)
│   └── tests/                # Individual test implementations
│       ├── BasicTests.cpp    # Core plugin tests (info, state, audio)
│       ├── BusTests.cpp      # Audio bus configuration tests
│       ├── EditorTests.cpp   # Plugin editor/GUI tests
│       ├── ParameterFuzzTests.cpp  # Parameter fuzzing tests
│       ├── LocaleTest.cpp    # Locale handling tests
│       └── ExtremeTests.cpp  # Edge case tests
├── modules/
│   └── juce/                 # JUCE framework (git submodule)
├── cmake/
│   └── CPM.cmake             # CMake Package Manager
├── tests/
│   ├── AddPluginvalTests.cmake  # CMake module for CTest integration
│   ├── test_plugins/         # Test plugin files
│   ├── mac_tests/            # macOS-specific tests
│   └── windows_tests.bat     # Windows test scripts
├── docs/                     # Documentation
│   ├── Adding pluginval to CI.md
│   ├── Command line options.md
│   ├── Debugging a failed validation.md
│   └── Testing plugins with pluginval.md
├── CMakeLists.txt            # Main build configuration
├── VERSION                   # Version number file
├── CHANGELIST.md             # Release changelog
└── ROADMAP.md                # Future development plans
```

## Build System

### Prerequisites
- CMake 3.15+
- C++20 compatible compiler
- Git (for submodules)

### Building

```bash
# Initialize JUCE submodule
git submodule update --init

# Configure (Debug build)
cmake -B Builds/Debug -DCMAKE_BUILD_TYPE=Debug .

# Build
cmake --build Builds/Debug --config Debug
```

### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `PLUGINVAL_FETCH_JUCE` | Fetch JUCE with pluginval | ON |
| `WITH_ADDRESS_SANITIZER` | Enable AddressSanitizer | OFF |
| `WITH_THREAD_SANITIZER` | Enable ThreadSanitizer | OFF |
| `VST2_SDK_DIR` | Path to VST2 SDK (env var) | - |

### Enabling VST2 Support

VST2 SDK is not included. Set the environment variable before configuring:
```bash
VST2_SDK_DIR=/path/to/vst2sdk cmake -B Builds/Debug .
```

### Target Platforms
- **macOS**: 10.11+ (deployment target), supports Apple Silicon via universal binary
- **Windows**: MSVC with static runtime linking
- **Linux**: Ubuntu 22.04+, statically links libstdc++

## Architecture

### Core Components

1. **PluginValidatorApplication** (`Main.cpp`)
   - JUCE application entry point
   - Handles both GUI and CLI modes
   - Manages preferences and window lifecycle

2. **Validator** (`Validator.h/cpp`)
   - Orchestrates validation passes
   - Supports in-process and child-process validation
   - Listener interface for progress callbacks

3. **ValidationPass** (`Validator.h`)
   - Single async validation for one plugin
   - Can run in separate process for crash isolation

4. **PluginTests** (`PluginTests.h/cpp`)
   - UnitTest subclass that runs all registered tests
   - Manages plugin loading and test execution
   - Configurable via `Options` struct

5. **PluginTest** (`PluginTests.h`)
   - Base class for individual tests
   - Auto-registers via static instance pattern
   - Defines requirements (thread, GUI needs)

### Test Framework

Tests are self-registering. To find all tests, look for static instances:
```cpp
static MyTest myTest;  // Registers automatically
```

**Strictness Levels (1-10)**:
- Level 1-4: Basic tests, quick execution
- Level 5: Recommended minimum for host compatibility (default)
- Level 6+: Extended tests, parameter fuzzing, longer duration
- Level 10: Most thorough, includes real-time safety checks

**Test Requirements**:
```cpp
struct Requirements {
    Thread thread;  // backgroundThread or messageThread
    GUI gui;        // noGUI or requiresGUI
};
```

### Key Test Files by Category

| File | Tests Included |
|------|----------------|
| `BasicTests.cpp` | PluginInfo, Programs, Editor, AudioProcessing, PluginState, Automation, auval, VST3validator |
| `BusTests.cpp` | Bus layout, channel configuration |
| `EditorTests.cpp` | Editor creation, resizing |
| `ParameterFuzzTests.cpp` | Random parameter value testing |
| `LocaleTest.cpp` | Locale handling verification |
| `ExtremeTests.cpp` | Edge cases, stress tests |

## Adding New Tests

1. Create a subclass of `PluginTest`:
```cpp
struct MyNewTest : public PluginTest
{
    MyNewTest()
        : PluginTest ("My Test Name",
                      5,  // strictness level (1-10)
                      { Requirements::Thread::backgroundThread,
                        Requirements::GUI::noGUI })
    {
    }

    void runTest (PluginTests& ut, juce::AudioPluginInstance& instance) override
    {
        // Use ut.expect(), ut.expectEquals(), ut.logMessage()
        ut.logMessage ("Running my test...");
        ut.expect (someCondition, "Test failed because...");
    }

    std::vector<TestDescription> getDescription (int strictnessLevel) const override
    {
        return { { name, "Description of what this test does" } };
    }
};

// Register the test with a static instance
static MyNewTest myNewTest;
```

2. Add the source file to `CMakeLists.txt` in the `SourceFiles` list.

### Test Utilities

Located in `TestUtilities.h`:
- `getNonBypassAutomatableParameters()` - Get automatable params
- `fillNoise()` - Fill buffer with random audio
- `countNaNs()`, `countInfs()`, `countSubnormals()` - Audio validation
- `ScopedEditorShower` - RAII editor creation/destruction
- `callPrepareToPlayOnMessageThreadIfVST3()` - VST3-safe lifecycle
- `ScopedAllocationDisabler` - Detect allocations in audio thread

### Real-time Safety Checking

Use the `RTC_REALTIME_CONTEXT_IF_ENABLED` macro around `processBlock` calls:
```cpp
{
    RTC_REALTIME_CONTEXT_IF_ENABLED(ut.getOptions().realtimeCheck, blockNum)
    instance.processBlock(ab, mb);
}
```

## Code Conventions

### Style
- JUCE coding style (CamelCase for types, camelCase for variables)
- 4-space indentation
- Braces on same line for control structures
- Use JUCE types: `juce::String`, `juce::Array`, `juce::File`, etc.

### Header Guards
Use `#pragma once` (not traditional include guards)

### JUCE Namespace
Either use `juce::` prefix or have `using namespace juce;` in cpp files (not headers)

### Thread Safety
- Tests may run on background or message thread (specify in Requirements)
- VST3 plugins require certain operations on message thread (use `*OnMessageThreadIfVST3` helpers)
- Use `juce::WaitableEvent` for thread synchronization

### Logging
```cpp
ut.logMessage("Important message");      // Always shown
ut.logVerboseMessage("Detail message");  // Only with --verbose flag
```

## Command Line Interface

Basic usage:
```bash
./pluginval --strictness-level 5 /path/to/plugin.vst3
```

Key options:
- `--validate [path]` - Validate plugin at path
- `--strictness-level [1-10]` - Test thoroughness (default: 5)
- `--skip-gui-tests` - Skip GUI tests (for headless CI)
- `--validate-in-process` - Don't use child process (for debugging)
- `--timeout-ms [ms]` - Test timeout (default: 30000, -1 for none)
- `--verbose` - Enable verbose logging
- `--output-dir [dir]` - Directory for log files
- `--sample-rates [list]` - Comma-separated sample rates
- `--block-sizes [list]` - Comma-separated block sizes
- `--rtcheck [disabled|enabled|relaxed]` - Real-time safety checking

Environment variables can substitute CLI args:
- `--skip-gui-tests` -> `SKIP_GUI_TESTS=1`
- `--timeout-ms 30000` -> `TIMEOUT_MS=30000`

Exit codes: 0 = success, 1 = failure

## CI Integration

### CMake/CTest Integration

Use `tests/AddPluginvalTests.cmake`:
```cmake
include(AddPluginvalTests)
add_pluginval_tests(MyPluginTarget
    TEST_PREFIX "MyPlugin.pluginval"
    LOG_DIR "${CMAKE_BINARY_DIR}/logs"
)
```

### GitHub Actions Example

```yaml
- name: Download pluginval
  run: |
    curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_${{ runner.os }}.zip" -o pluginval.zip
    unzip pluginval.zip

- name: Validate Plugin
  run: |
    ./pluginval --strictness-level 5 --skip-gui-tests ./build/MyPlugin.vst3
```

## Dependencies

### External
- **JUCE** (v8.0.x) - Audio application framework (git submodule)
- **magic_enum** (v0.9.7) - Enum reflection (fetched via CPM)
- **rtcheck** (optional, macOS) - Real-time safety checking (fetched via CPM)

### System
- macOS: CoreAudio, AudioUnit frameworks
- Linux: ALSA, X11
- Windows: WASAPI, DirectSound

## Testing pluginval Itself

Debug unit tests run automatically in debug builds:
```cpp
#if JUCE_DEBUG
    juce::UnitTestRunner testRunner;
    testRunner.runTestsInCategory ("pluginval");
#endif
```

Run internal tests via CLI:
```bash
./pluginval --run-tests
```

## Release Process

1. Update `VERSION` file
2. Update `CHANGELIST.md`
3. Commit: `git commit -am "Version X.Y.Z"`
4. Tag: `git tag -a vX.Y.Z -m "X.Y.Z release"`
5. Push: `git push --tags`

## Common Tasks for AI Assistants

### Finding Where Tests Are Defined
- All test classes are in `Source/tests/*.cpp`
- Search for `static.*Test.*Test;` to find registrations
- Each test subclasses `PluginTest`

### Understanding Test Flow
1. `Main.cpp` creates `Validator` or `CommandLineValidator`
2. `Validator` creates `ValidationPass` for each plugin
3. `ValidationPass` spawns child process or runs in-process
4. `PluginTests::runTest()` iterates all registered `PluginTest` instances
5. Each `PluginTest::runTest()` performs its specific validation

### Modifying Build Configuration
- All in `CMakeLists.txt`
- Source files listed in `SourceFiles` variable
- JUCE modules linked via `target_link_libraries`

### Adding Platform-Specific Code
```cpp
#if JUCE_MAC
    // macOS specific
#elif JUCE_WINDOWS
    // Windows specific
#elif JUCE_LINUX
    // Linux specific
#endif
```

## Important Notes

- Always test changes on multiple platforms when possible
- VST3 plugins have specific threading requirements - use the `*OnMessageThreadIfVST3` helpers
- Child process validation is the default and recommended for production use
- In-process validation (`--validate-in-process`) is useful for debugging but a crashing plugin will crash pluginval
- Real-time safety checking is only available on macOS currently (uses rtcheck library)
