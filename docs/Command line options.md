Hello rtcheck!
//==============================================================================
pluginval
JUCE v8.0.10

Description: 
  Validate plugins to test compatibility with hosts and verify plugin API conformance

Usage: 
  --version
    Print pluginval version.
  --validate [pathToPlugin]
    Validates the plugin at the given path.
    N.B. the "--validate" flag is optional if the path is the last argument.
    This enables you to validate a plugin with simply "pluginval path_to_plugin".

  --sample-rates [list of comma separated sample rates]
    If specified, sets the list of sample rates at which tests will be executed
    (default=44100,48000,96000)
  --block-sizes [list of comma separated block sizes]
    If specified, sets the list of block sizes at which tests will be executed
    (default=64,128,256,512,1024)
  --random-seed [hex or int]
    Sets the random seed to use for the tests. Useful for replicating test
    environments.
  --data-file [pathToFile]
    If specified, sets a path to a data file which can be used by tests to
    configure themselves. This can be useful for things like known audio output.

  --strictness-level [1-10]
    Sets the strictness level to use. A minimum level of 5 (also the default)
    is recomended for compatibility.
    Higher levels include longer, more thorough tests such as fuzzing.
  --timeout-ms [numMilliseconds]
    Sets a timout which will stop validation with an error if no output from any
    test has happened for this number of ms.
    By default this is 30s but can be set to "-1" (must be quoted) to never timeout.
  --rtcheck [empty, disabled, enabled or relaxed]
    Turns on real-time saftey checks using rtcheck (macOS and Linux only).
    relaxed mode doesn't run the checks for the first processing block as a lot of plugins
    use this to allocate or initialise thread-locals (which can allocate)

  --repeat [num repeats]
    If specified repeats the tests a given number of times. Note that this does
    not delete and re-instantiate the plugin for each repeat.
  --randomise
    If specified, the tests are run in a random order per repeat.

  --skip-gui-tests
    If specified, avoids tests that create GUI windows, which can cause problems
    on headless CI systems.
  --disabled-tests [pathToFile]
    If specified, sets a path to a file that should have the names of disabled
    tests on each row.
  --vst3validator [pathToValidator]
    If specified, this will run the VST3 validator as part of the test process.

  --output-dir [pathToDir]
    If specified, sets a directory to store the log files. This can be useful
    for continuous integration.
  --output-filename [filename]
    If specified, sets a filename for the log files (within 'output-dir' or
    (lacking that) the current directory.
    By default, the name is constructed from the plugin metainformation
  --verbose
    If specified, outputs additional logging information. It can be useful to
    turn this off when building with CI to avoid huge log files.

Exit code: 
  0 if all tests complete successfully
  1 if there are any errors

Additionally, you can specify any of the command line options as environment
variables by removing prefix dashes, converting internal dashes to underscores
and capitalising all letters, a.g.
    "--skip-gui-tests" > "SKIP_GUI_TESTS=1"
    "--timeout-ms 30000" > "TIMEOUT_MS=30000"
Specifying specific command-line options will override any environment variables
set for that option.

 pluginval --version                  Prints the current version number
 pluginval --help|-h                  Prints the list of commands
 pluginval --validate [pathToPlugin]  Validates the file (or IDs for AUs).
 pluginval --run-tests                Runs the internal unit tests.

