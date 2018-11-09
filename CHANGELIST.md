# pluginval Change List

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
  - Added a test calling processBlock with more samples than initalised with

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
