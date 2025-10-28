# pluginval Change List

### 1.0.5
- Added static linking to the Windows runtime so it should run on more Windows systems (particularly non-dev machines)
- Made `PluginInfoTest` run on the message thread as the functions it calls aren't thread-safe 

### 1.0.4
- Limit auval's stress test to 20 seconds (vs 600) [#135]
- Fixed incorrect "ALL TESTS PASSED" message when validating out of process [#125]
- Updated juce to 8.0.3 [#133]
- Added LV2 support [#25]
- Provide CMake module for registering pluginval tests with CTest [#123] 
- Changed minimum Linux version to Ubuntu 22.04

### 1.0.3
-  Fix VST3 host bus issue

### 1.0.2
- Added com.apple.security.get-task-allow to allow debugging on pluginval [macOS]

### 1.0.1
- Fixed support for Ubuntu 18.04

### 1.0.0
- Added running auval as a test for Audio Units on macOS (levels 5 and up)
- Added running VST3's validator as a test for VST3s (levels 5 and up)
- Added better conformance to the VST3 standard when running at levels 5 and below
- Refactored internals for better stability

### 0.2.9
- Fixed support for macOS from 10.11 and Windows without the runtime DLLs

### 0.2.8
- Added a test to process audio with different sample rate and block sizes but not call releaseResources in between

### 0.2.6
- Avoided a deadlock when opening plugin windows for the second time on Linux

### 0.2.5
- Fixed Linux architecture for pre-built binaries

### 0.2.4
- Added tests for plugin programs
- Added a new CLI option to pass a file with a list of tests to disable
- Added notarisation to macOS builds for Catalina compatibility

### 0.2.3
 - Fixed some failing tests on startup

### 0.2.2
 - Added a test for disabled HiDPI awareness on Windows
 - Updated JUCE to v5.4.3

### 0.2.1
  - Added an EditorAutomationTest to adjust parameter values whilst showing the plugin editor

### 0.2.0
  - Removed built-in support for VST2 testing, this can be built with by setting the VST2_SDK_DIR environment variable before running the `tests/` scripts
  - Added the option to repeat tests a specified number of times
  - Added the option to randomise the test order each repeat
  - Added some message loop running to editor tests to ensure the UI is actually displayed
  - Added a test for processing audio whilst showing the editor
  - Added an option to automatically save log files with a sensible name (see the "--output-dir" option)
  - Disabled the LargerThanPreparedBlockSizeTest for AU, VST & VST3 formats
  - Added a "Test File" button to the UI for validating plugins that won't scan (this improves the stack trace information in the log file)

### 0.1.5
  - Fixed a problem catching allocations in clang release builds
  - Avoided accidently setting the bypass parameter for VST3 plugins
  - Simplified some parameter iteration methods to avoid returning bypass or non-automatable parameters
  - Added MIDI note on and off messages for synth processing tests
  - Added a Parameter thread safety test which attempts to call setValue on parameters from multiple threads concurrently mimicking automation and GUI interaction
  - Added a PluginStateTestRestoration test which calculates some checksums before randomising and restoring plugin state to check state restoration is correct
  - Added the ability to set command line arguments as environment variables e.g. "--skip-gui-tests" > "SKIP_GUI_TESTS=1", "--timeout-ms 30000" > "TIMEOUT_MS=30000"
  - Added the ability to set a random seed to use for the tests

### 0.1.4
  - Added stack backtraces to crashed validation output
  - Added tests for detecting memory allocations and deallocations in the audio thread
  - Added a test for creating an editor with an uninitialised plugin and a 0 sample rate and block size prepared plugin
  - Added a test calling processBlock with more samples than initialised with

### 0.1.3
  - Reduced the amount of default logging
  - Added the "--verbose" command line flag
  - Added some basic bus tests

### 0.1.2
  - Added optional logging from the slave process
  - Added an option to validate in the same process in GUI mode
  - Added the "--validate-in-process" command line flag
  - Replaced the "strictnessLevel" command line option with the more canonical "strictness-level" form
  - Added a timeout option which can be set from the GUI "Options" menu or the command line "--timeoutMs [numMs]" flag
