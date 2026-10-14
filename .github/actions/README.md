# pluginval GitHub Actions

Two composite actions for using pluginval from another repository's workflows.
Their `action.yml` files are in `validate/` and `acceptance/` at the repository
root, which keeps the references short; the scripts they run are in this directory.
Both run on Linux, macOS and Windows runners, download the matching pluginval
release (or use a binary you give them), run everything they are given and fail
the step if anything failed.

Results are shown in two places: the job summary gets a table with a row per
plugin / config and its test count, followed by the name and message of every
failed test; and each failed plugin / config gets an error annotation naming its
failed tests, which also appears in the pull request's checks.

On Linux the plugin is run under `xvfb-run` when there is no `DISPLAY` (install
`xvfb` on runners that don't have it).

## validate

Validates one or more plugins.

```yaml
- uses: Tracktion/pluginval/validate@v2
  with:
    plugins: |
      build/MyPlugin_artefacts/Release/VST3/MyPlugin.vst3
      build/MyPlugin_artefacts/Release/AU/MyPlugin.component
    strictness-level: 10
    args: --skip-gui-tests
```

| Input | Default | |
| --- | --- | --- |
| `plugins` | (required) | The plugins to validate, one per line. Each is a path, a glob or an AU id. A glob that matches nothing fails. |
| `strictness-level` | `5` | The strictness level (1-10). |
| `args` | | Extra options for `pluginval validate`, split on whitespace. |
| `report-dir` | | Directory to write the JUnit XML reports to (a temporary directory if empty). |
| `pluginval-version` | `v2.0.0-beta1` | The pluginval release to download: a tag, or `latest`. |
| `pluginval-path` | | An existing pluginval binary to use instead of downloading one. |

Every `pluginval validate` option can also be set as an environment variable on
the step (e.g. `env: { TIMEOUT_MS: 60000 }`), which is the way to pass values
containing spaces.

To skip tests, set `DISABLED_TESTS` to their names, comma-separated (or to an
absolute path to a file listing one name per line). Test names contain spaces,
so they can't be given as `--disabled-tests` in `args`. For example, at strictness
8 and above "Discrete parameter state restoration" fails for plugins using
`juce::AudioParameterBool`, until [JUCE snaps its value to 0 or 1](https://forum.juce.com/t/pluginval-can-set-an-audioparameterbools-to-odd-values/50971):

```yaml
- uses: Tracktion/pluginval/validate@v2
  env:
    DISABLED_TESTS: Discrete parameter state restoration
  with:
    plugins: build/MyPlugin_artefacts/Release/VST3/MyPlugin.vst3
    strictness-level: 10
```

Run `pluginval strictness-help <level>` to list the test names.

## acceptance

Runs the [acceptance tests](../../docs/Acceptance%20testing.md) for every config
(`*.json`) found recursively under a directory, optionally against one built
plugin.

```yaml
- uses: Tracktion/pluginval/acceptance@v2
  with:
    config-dir: tests/acceptance
    plugin: build/MyPlugin_artefacts/Release/VST3/MyPlugin.vst3
```

| Input | Default | |
| --- | --- | --- |
| `config-dir` | (required) | Directory searched recursively for configs. |
| `plugin` | | Plugin path or AU id to test every config against, instead of the `plugin` in each config. |
| `record-missing` | `false` | Record a missing reference and pass, instead of failing. |
| `timeout-ms` | | Fail a test if it takes longer than this (pluginval's default if empty). |
| `report-dir` | | Directory to write the JUnit XML reports to (a temporary directory if empty). |
| `pluginval-version` | `v2.0.0-beta1` | The pluginval release to download: a tag, or `latest`. |
| `pluginval-path` | | An existing pluginval binary to use instead of downloading one. |

Notes:

- Every `*.json` under `config-dir` is treated as a config, apart from the
  `<reference>.wav.json` manifests. Keep other JSON files (and plugin bundles,
  which contain their own) out of it.
- A config holding an array of tests is reported as a failure: use one config
  per file.
- Relative `plugin` paths resolve against the working directory; the input,
  state and reference paths in a config resolve against the config's directory.

## Outputs

Both actions set:

| Output | |
| --- | --- |
| `passed` / `failed` | How many plugins / configs passed and failed. |
| `report-dir` | A directory holding a JUnit XML report for each one. For `acceptance` it also has a `diffs` directory with the `*-diff.wav` files found under `config-dir`. |

To keep the reports and diffs from a failed run:

```yaml
- uses: Tracktion/pluginval/acceptance@v2
  id: acceptance
  with:
    config-dir: tests/acceptance
    plugin: build/MyPlugin_artefacts/Release/VST3/MyPlugin.vst3

- uses: actions/upload-artifact@v4
  if: always()
  with:
    name: pluginval-acceptance-${{ runner.os }}
    path: ${{ steps.acceptance.outputs.report-dir }}
```

### Per-test results as a pull request check

The actions report failures through the job summary and annotations, which need
no extra permissions. If you would rather have every pluginval test listed as its
own check on the pull request, feed the JUnit reports to a reporter action such
as [mikepenz/action-junit-report](https://github.com/mikepenz/action-junit-report)
or [dorny/test-reporter](https://github.com/dorny/test-reporter):

```yaml
jobs:
  validate:
    runs-on: macos-latest
    permissions:
      contents: read
      checks: write
    steps:
      # ... build the plugin ...

      - uses: Tracktion/pluginval/validate@v2
        with:
          plugins: build/MyPlugin_artefacts/Release/VST3/MyPlugin.vst3
          report-dir: pluginval-reports

      - uses: mikepenz/action-junit-report@v5
        if: always()
        with:
          report_paths: pluginval-reports/*.xml
          check_name: pluginval
```

This needs the `checks: write` permission, which workflows triggered by pull
requests from forks don't get, so on those the reporter step can't publish.
