Validate plugins to test compatibility with hosts and verify plugin API
conformance


pluginval [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
          --version           Display program version information and exit
          --config TEXT ...   Path to a JSON settings file. Repeatable; later files win per
                              key.
          --validate TEXT     Validates the plugin at the given path (or AU id).
          --strictness-level INT 
                              Strictness level 1-10 (default 5).
          --timeout-ms INT    Test timeout in ms (default 30000, -1 to never timeout).
          --repeat INT        Number of times to repeat the tests.
          --randomise         Run the tests in a random order per repeat.
          --verbose           Output additional logging information.
          --skip-gui-tests    Avoid tests that create GUI windows (for headless CI).
          --sample-rates FLOAT ... 
                              Comma-separated sample rates (default 44100,48000,96000).
          --block-sizes INT ... 
                              Comma-separated block sizes (default 64,128,256,512,1024).
          --data-file TEXT    Path to a data file tests can use to configure themselves.
          --output-dir TEXT   Directory in which to write the log files.
          --output-filename TEXT 
                              Filename to write logs into.
          --disabled-tests TEXT 
                              Comma-separated test names, or a path to a file listing them.
          --random-seed TEXT  Random seed (hex 0x.. or int) for replicable test runs.
          --rtcheck ENUM:value in {disabled->0,enabled->1,relaxed->2} OR {0,1,2} 
                              Real-time safety checks: disabled, enabled or relaxed.

JUCE v8.0.13

Other commands:
--run-tests Run the internal unit tests.
--strictness-help [level] List all tests that run at the given strictness level.

Exit code:
0 if all tests complete successfully
1 if there are any errors

You can also specify any option as an environment variable by removing the
prefix
dashes, converting internal dashes to underscores and capitalising, e.g.
"--skip-gui-tests" -> "SKIP_GUI_TESTS=1"
"--timeout-ms 30000" -> "TIMEOUT_MS=30000"
Precedence (lowest to highest): defaults, environment variables, --config,
command-line options.
--config is repeatable; later files win per key.
