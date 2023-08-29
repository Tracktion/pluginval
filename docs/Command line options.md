//==============================================================================
pluginval
JUCE v7.0.2

Description: 
  Validate plugins to test compatibility with hosts and verify plugin API conformance

Usage: 
  --validate [pathToPlugin]
    Validates the plugin at the given path.
    N.B. the --validate flag is optional if the path is the last argument. This enables you to validate a plugin with simply "pluginval path_to_plugin".
  --strictness-level [1-10]
    Sets the strictness level to use. A minimum level of 5 (also the default) is recomended for compatibility. Higher levels include longer, more thorough tests such as fuzzing.
  --random-seed [hex or int]
    Sets the random seed to use for the tests. Useful for replicating test environments.
  --timeout-ms [numMilliseconds]
    Sets a timout which will stop validation with an error if no output from any test has happened for this number of ms.
    By default this is 30s but can be set to -1 to never timeout.
  --verbose
    If specified, outputs additional logging information. It can be useful to turn this off when building with CI to avoid huge log files.
  --skip-gui-tests
    If specified, avoids tests that create GUI windows, which can cause problems on headless CI systems.
  --repeat [num repeats]
    If specified repeats the tests a given number of times. Note that this does not delete and re-instantiate the plugin for each repeat.  --randomise
    If specified the tests are run in a random order per repeat.  --data-file [pathToFile]
    If specified, sets a path to a data file which can be used by tests to configure themselves. This can be useful for things like known audio output.
  --output-dir [pathToDir]
    If specified, sets a directory to store the log files. This can be useful for continuous integration.
  --disabled-tests [pathToFile]
    If specified, sets a path to a file that should have the names of disabled tests on each row.
  --sample-rates [list of comma separated sample rates]
    If specified, sets the list of sample rates at which tests will be executed (default=44100,48000,96000)
  --block-sizes [list of comma separated block sizes]
    If specified, sets the list of block sizes at which tests will be executed (default=64,128,256,512,1024)
  --vst3validator [pathToValidator]
    If specified, this will run the VST3 validator as part of the test process.
  --version
    Print pluginval version.

Exit code: 
  0 if all tests complete successfully
  1 if there are any errors

Additionally, you can specify any of the command line options as environment varibles by removing prefix dashes, converting internal dashes to underscores and captialising all letters e.g. "--skip-gui-tests" > "SKIP_GUI_TESTS=1", "--timeout-ms 30000" > "TIMEOUT_MS=30000"
Specifying specific command-line options will override any environment variables set for that option.

 pluginval --version                  Prints the current version number
 pluginval --help|-h                  Prints the list of commands
 pluginval --validate [pathToPlugin]  Validates the file (or IDs for AUs).
 pluginval --run-tests                Runs the internal unit tests.

