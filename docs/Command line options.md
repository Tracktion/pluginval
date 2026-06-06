Validate plugins to test compatibility with hosts and verify plugin API
conformance


pluginval [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
          --version           Display program version information and exit
          --config TEXT       Path to a JSON settings file (overridden by env vars and CLI
                              options).
          --validate TEXT     Validates the plugin at the given path (or AU id).
          --strictness-level INT (Env:STRICTNESS_LEVEL) 
                              Strictness level 1-10 (default 5).
          --timeout-ms INT (Env:TIMEOUT_MS) 
                              Test timeout in ms (default 30000, -1 to never timeout).
          --repeat INT (Env:REPEAT) 
                              Number of times to repeat the tests.
          --randomise (Env:RANDOMISE) 
                              Run the tests in a random order per repeat.
          --verbose (Env:VERBOSE) 
                              Output additional logging information.
          --skip-gui-tests (Env:SKIP_GUI_TESTS) 
                              Avoid tests that create GUI windows (for headless CI).
          --sample-rates FLOAT ... (Env:SAMPLE_RATES) 
                              Comma-separated sample rates (default 44100,48000,96000).
          --block-sizes INT ... (Env:BLOCK_SIZES) 
                              Comma-separated block sizes (default 64,128,256,512,1024).
          --data-file TEXT (Env:DATA_FILE) 
                              Path to a data file tests can use to configure themselves.
          --output-dir TEXT (Env:OUTPUT_DIR) 
                              Directory in which to write the log files.
          --output-filename TEXT (Env:OUTPUT_FILENAME) 
                              Filename to write logs into.
          --disabled-tests TEXT 
                              Comma-separated test names, or a path to a file listing them.
          --random-seed TEXT (Env:RANDOM_SEED) 
                              Random seed (hex 0x.. or int) for replicable test runs.
          --rtcheck ENUM:value in {disabled->0,enabled->1,relaxed->2} OR {0,1,2} (Env:RTCHECK) 
                              Real-time safety checks: disabled, enabled or relaxed.

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
Precedence: command-line options > environment variables > --config file.
