Usage: pluginval [OPTIONS...]
Validate plugins to test compatibility with hosts and verify plugin API conformance

Examples:

  pluginval --strictness-level 5 --validate path/to/plugin
  pluginval path/to/plugin
  pluginval --config settings.json --validate path/to/plugin

Options:

      --validate=VALUE         Validates the plugin at the given path (or AU id). Optional if the path is the last argument.
      --strictness-level=VALUE
                               Strictness level 1-10 (default 5, minimum 5 recommended).
      --random-seed=VALUE      Random seed (hex 0x.. or int) for replicable test runs.
      --timeout-ms=VALUE       Test timeout in ms (default 30000, -1 to never timeout).
      --repeat=VALUE           Number of times to repeat the tests.
      --randomise              Run the tests in a random order per repeat.
      --verbose                Output additional logging information.
      --skip-gui-tests         Avoid tests that create GUI windows (for headless CI).
      --sample-rates=VALUE     Comma-separated sample rates (default 44100,48000,96000).
      --block-sizes=VALUE      Comma-separated block sizes (default 64,128,256,512,1024).
      --data-file=VALUE        Path to a data file tests can use to configure themselves.
      --output-dir=VALUE       Directory in which to write the log files.
      --output-filename=VALUE  Filename to write logs into.
      --disabled-tests=VALUE   Comma-separated test names, or a path to a file listing them.
      --rtcheck=VALUE          Real-time safety checks: disabled, enabled or relaxed.
      --config=VALUE           Path to a JSON file of settings (overridden by env vars and CLI options).

  -?, --help                   show this message
      --version                print program version

Other commands:
  --version                   Print the pluginval version.
  --run-tests                 Run the internal unit tests.
  --strictness-help [level]   List all tests that run at the given strictness level.

Exit code:
  0 if all tests complete successfully
  1 if there are any errors

Additionally, you can specify any of the command line options as environment
variables by removing prefix dashes, converting internal dashes to underscores
and capitalising all letters, e.g.
    "--skip-gui-tests" > "SKIP_GUI_TESTS=1"
    "--timeout-ms 30000" > "TIMEOUT_MS=30000"
Precedence is command-line options > environment variables > --config file.

