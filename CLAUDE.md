# CLAUDE.md - AI Assistant Guide for pluginval

## Project Overview

**pluginval** is a cross-platform audio plugin validator and tester application developed by Tracktion Corporation. It tests VST, VST3, AU (Audio Unit), LV2, and LADSPA plugins for compatibility and stability with host applications.

- **Version**: 1.0.4 (see `VERSION` file; a 2.0.0 entry is staged in `CHANGELIST.md` but `VERSION` is not yet bumped)
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

## Getting CI Run Logs

### Configuration

- **Organisation:** `<organisation>`
- **Repository:** `<repo>`

For this project:
- **Organisation:** `Tracktion`
- **Repository:** `pluginval`

### Setup

Install the GitHub CLI:
```bash
brew install gh  # macOS
# or
sudo apt install gh  # Ubuntu/Debian
```

Authentication is handled via the `GH_TOKEN` environment variable (already configured).

### Workflow

1. **List recent workflow runs:**
   ```bash
   gh run list -R <organisation>/<repo>
   ```

2. **Find the most recent run for your branch** from the output above.

3. **View failed log details:**
   ```bash
   gh run view -R <organisation>/<repo> <run_id> --log-failed
   ```

Replace `<run_id>` with the ID from step 2.

## Directory Structure

```
pluginval/
├── source/                    # Main application source code
│   ├── Main.cpp              # Application entry point
│   ├── MainComponent.cpp/h   # GUI main window component
│   ├── Validator.cpp/h       # Core validation orchestration
│   ├── PluginTests.cpp/h     # Test framework and base classes
│   ├── CommandLine.cpp/h     # Thin CLI adapter (delegates to SettingsParser)
│   ├── PluginvalSettings.h   # Unified settings struct + JSON mapping + toPluginTestOptions()
│   ├── SettingsParser.cpp/h  # CLI/env/config -> merged JSON -> settings; child-process handoff
│   ├── SettingsSerializer.cpp/h # JSON load/save + value coercions (comma lists, hex seed)
│   ├── CrashHandler.cpp/h    # Crash reporting utilities
│   ├── TestUtilities.cpp/h   # Helper functions for tests
│   ├── RTCheck.h             # Real-time safety checking macros
│   ├── PluginvalLookAndFeel.h # Custom UI styling
│   ├── StrictnessInfoPopup.h # Strictness level info UI
│   ├── binarydata/           # Binary resources (icons)
│   ├── vst3validator/        # Embedded VST3 validator integration
│   │   ├── VST3ValidatorRunner.h
│   │   └── VST3ValidatorRunner.cpp
│   ├── acceptance/           # `pluginval test` golden-file subsystem (parallel to validate)
│   │   ├── TestConfig.cpp/h        # snake_case JSON config struct (independent of PluginvalSettings)
│   │   ├── AcceptanceTest.cpp/h    # plugin load + state + input + render; record-or-compare orchestrator
│   │   ├── ReferenceComparator.cpp/h # Comparator interface + registry + SampleComparator
│   │   └── TestReporter.cpp/h      # text + JSON result, exit code
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
│   ├── CPM.cmake             # CMake Package Manager
│   └── GenerateBinaryHeader.cmake  # Binary-to-C-header converter
├── tests/
│   ├── AddPluginvalTests.cmake  # CMake module for CTest integration
│   ├── test_plugins/         # Test plugin files
│   │   ├── tone_generator/   # Deterministic dogfood generator (PLUGINVAL_BUILD_TEST_PLUGINS)
│   │   └── gain/             # Deterministic dogfood gain effect (for the input.audio path)
│   ├── acceptance/           # Acceptance self-tests: configs (*.json.in), inputs/, checked-in refs/ WAVs
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
| `PLUGINVAL_VST3_VALIDATOR` | Build with embedded VST3 validator | ON |
| `WITH_ADDRESS_SANITIZER` | Enable AddressSanitizer | OFF |
| `WITH_THREAD_SANITIZER` | Enable ThreadSanitizer | OFF |
| `VST2_SDK_DIR` | Path to VST2 SDK (env var) | - |
| `PLUGINVAL_BUILD_TEST_PLUGINS` | Build the in-repo dogfood plugins + acceptance CTest self-tests | OFF |

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

### CLI Settings Pipeline

#### Subcommand dispatch layer

The command line is structured into subcommands, peeled off by a thin verb
dispatcher in front of the settings pipeline (it does **not** use CLI11-native
subcommands, so the env/config/CLI layering below is untouched):

- `pluginval validate [options] <plugin>` — the default; `<plugin>` is a
  positional argument. `pluginval <plugin>` and `pluginval [options] <plugin>`
  (no verb) also resolve to validate.
- `pluginval run-tests` — runs the internal unit tests.
- `pluginval strictness-help [level]` — lists tests at a strictness level.

`settings_parser::dispatch()` (in `SettingsParser.cpp`) takes the `tokenise()`d
command line and returns a `DispatchResult { command, validateTokens,
deprecatedAlias, strictnessLevel }`. For `validate` it strips the verb and hands
`validateTokens` to `parseTokens` unchanged. `CommandLine.cpp`'s
`performCommandLine()` switches on `command` and emits a one-line stderr notice
when `deprecatedAlias` is set.

The old flat flags (`--validate <plugin>`, `--run-tests`, `--strictness-help`)
are kept as **deprecated aliases** that route to the same commands with
`deprecatedAlias = true` (the bare-path shorthand and the internal child handoff
stay silent). `preprocess()` is now `tokenise()` + `insertImplicitValidate()`.
The child process is launched with the explicit verb:
`validate --config-base64 <b64> <path>`.

Note: `--config` is parsed manually (it is stripped from the tokens fed to the
CLI11 pass) so its greedy CLI11 vector parsing can't swallow the positional
plugin path; it stays registered only so it appears in `--help`.

#### Settings layering

Command-line parsing centres on one plain settings struct (`PluginvalSettings`)
that CLI11 binds to directly. A single instance is filled by successive layers,
**lowest to highest precedence: defaults → environment → `--config` → CLI**
(in `SettingsParser::parseTokens`):

1. **preprocess** the raw command line — rewrite the deprecated `strictnessLevel`,
   strip the macOS `-NSDocumentRevisionsDebugMode YES` flag, and insert an
   implicit `--validate` when the last argument is a bare plugin path.
2. **Environment layer.** Env-var names are *derived* from the registered
   options (`--strictness-level` → `STRICTNESS_LEVEL`), so there is no separate
   env table. A synthetic `--name=value` argv is built from the environment and
   parsed by CLI11, reusing all its coercion.
3. **`--config` layer.** Repeatable; each JSON file is `merge_patch`-ed in
   command-line order (later files win per key). Beats the environment.
4. **CLI layer.** The real arguments are parsed last and beat everything;
   CLI11 only overwrites a member when its option was actually provided.

`configureApp()` registers every option (bound to the struct) and is used for
both the env pass and the CLI pass. Comma lists use `->delimiter(',')`, the enum
uses a `CheckedTransformer`, and the hex/int seed is a small callback.
`PluginvalSettings::toPluginTestOptions()` converts to the JUCE-flavoured
`PluginTests::Options` at the boundary.

Adding a new option is three edits: a struct member, an entry in the nlohmann
macro list, and one `add_option(...)` line — its environment variable then works
automatically. `SettingsSerializer` handles JSON load/save plus the two
remaining conversions (hex seed, disabled-tests file).

The child validation process receives a fully-resolved, **authoritative**
settings set via a base64-encoded JSON argument (`--config-base64`), avoiding
per-flag re-serialisation and command-line quoting hazards. `--help`/`--version`
are handled by CLI11 (auto usage + a footer with the env-var/commands notes).

### Acceptance Testing (`pluginval test`)

A **parallel subsystem** to validate, in `source/acceptance/`. It answers "does
this plugin produce the expected output for a known input + state?" — a
deterministic *render + golden-file comparison*, not a unit-test pass/fail. Full
spec: `tests/acceptance/Acceptance testing design.md`; end-user guide:
`docs/Acceptance testing.md`.

- **CLI**: `pluginval test <config.json>`. A new `Command::test` is recognised
  by `settings_parser::dispatch()` (captures the positional config path into
  `DispatchResult::testConfigPath`), `isCommandLine()` and `getFooterText()`;
  `CommandLine.cpp`'s `performCommandLine()` has a `Command::test` branch that
  runs the acceptance runner **synchronously on the message thread** and quits.
- **Config**: `acceptance::TestConfig` (`TestConfig.cpp/h`) — std-typed struct,
  **snake_case JSON keys** mapped via explicit `to_json`/`from_json` (members
  stay camelCase). It is **independent** of `PluginvalSettings` / the `--config`
  layering: the test config is a positional argument loaded standalone.
- **Flow** (`AcceptanceTest.cpp`): load plugin → apply `state.file` then
  `state.parameters` (normalised, matched by index / case-insensitive name or
  paramID) → feed `input.audio`/`input.midi` or silence → render a fixed
  duration block-by-block (reusing the `AudioProcessingTest` shape + the
  VST3-safe helpers in `TestUtilities.h`). If no reference exists it **records**
  one (32-bit float WAV + `<name>.wav.json` sidecar manifest); otherwise it
  **compares** and writes a diff WAV on failure. Exit `0`/`1`.
- **Comparators** (`ReferenceComparator.cpp/h`): pluggable `Comparator` +
  `createComparator(name)` registry. v1 ships only `sample` (per-sample abs-diff
  tolerance, default one 16-bit LSB = `1/32768`; `0` = bit-exact). Adding
  `spectrum`/`crosscorr`/etc. is one registry entry, no config/runner changes.
- **Dogfood + self-tests**: two minimal deterministic `juce_add_plugin` targets
  behind `PLUGINVAL_BUILD_TEST_PLUGINS` — `tests/test_plugins/tone_generator/`
  (closed-form sine/square generator, phase resets on `prepareToPlay`) and
  `tests/test_plugins/gain/` (a gain effect, used to dogfood the `input.audio`
  path: it gains a checked-in full-height sine and is compared bit-exact).
  `tests/acceptance/` holds checked-in configs (`*.json.in`, the plugin paths +
  input dir substituted at configure time), `inputs/` and reference WAVs, run via
  CTest (`pluginval.acceptance.*`: sine-440, square-220, square-state, gain-half).
  Phase 2 items (config-array multiplexing,
  automation, playhead, extra comparators, child-process isolation) are notes
  only.

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

### VST3 Validator Integration

The VST3 validator (Steinberg's vstvalidator) is embedded into pluginval when built with `PLUGINVAL_VST3_VALIDATOR=ON` (the default). This provides single-file distribution while keeping vstvalidator completely isolated from pluginval's link dependencies.

**Architecture:**
1. The VST3 SDK is fetched via CPM during CMake configure
2. The SDK's own `validator` target is built as a separate executable
3. A CMake script (`cmake/GenerateBinaryHeader.cmake`) converts the compiled binary into a C byte array header
4. `VST3ValidatorRunner` (`source/vst3validator/`) extracts the embedded binary to a temp file on first use
5. When the `VST3validator` test runs, it spawns the extracted validator as a subprocess

**Key files:**
- `cmake/GenerateBinaryHeader.cmake` — binary-to-C-header conversion script
- `source/vst3validator/VST3ValidatorRunner.h/cpp` — extracts embedded binary, returns `juce::File`

**Disabling embedded validator:**
```bash
cmake -B Builds -DPLUGINVAL_VST3_VALIDATOR=OFF .
```

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
./pluginval validate --strictness-level 5 /path/to/plugin.vst3
```

Commands:
- `validate [options] <plugin>` - Validate the plugin at the given path/AU id (the default; `./pluginval <plugin>` also works)
- `run-tests` - Run the internal unit tests
- `strictness-help [level]` - List the tests that run at a strictness level

The flat flags `--validate <plugin>`, `--run-tests` and `--strictness-help [level]` are deprecated aliases.

Key options (for `validate`):
- `--config [file.json]` - Load a full settings set from JSON (overridden by env vars and CLI options)
- `--strictness-level [1-10]` - Test thoroughness (default: 5)
- `--skip-gui-tests` - Skip GUI tests (for headless CI)
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
- **CLI11** (v2.6.2) - CLI argument parsing, header-only (fetched via CPM)
- **nlohmann/json** (3.12.0) - JSON settings layering/serialisation (fetched via CPM)
- **rtcheck** (optional, macOS) - Real-time safety checking (fetched via CPM)
- **VST3 SDK** (v3.7.x) - Steinberg VST3 SDK for embedded validator (fetched via CPM, optional)

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
./pluginval run-tests
```

## Release Process

1. Update `VERSION` file
2. Update `CHANGELIST.md`
3. Commit: `git commit -am "Version X.Y.Z"`
4. Tag: `git tag -a vX.Y.Z -m "X.Y.Z release"`
5. Push: `git push --tags`

## Common Tasks for AI Assistants

### Finding Where Tests Are Defined
- All test classes are in `source/tests/*.cpp`
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
- The GUI runs each validation in a separate child process for crash isolation (the default)
- The CLI `--validate` path runs in-process; a crashing plugin will terminate pluginval, and the signal handler reports it as a failure rather than a pass
- Real-time safety checking is only available on macOS currently (uses rtcheck library)
