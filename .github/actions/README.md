# pluginval GitHub Actions

Two composite actions for using pluginval from another repository's workflows.
Both run on Linux, macOS and Windows runners, download the matching pluginval
release (or use a binary you give them), run everything they are given, write a
results table to the job summary and fail the step if anything failed.

On Linux the plugin is run under `xvfb-run` when there is no `DISPLAY` (install
`xvfb` on runners that don't have it).

## validate

Validates one or more plugins.

```yaml
- uses: Tracktion/pluginval/.github/actions/validate@v2
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
| `pluginval-version` | `v2.0.0-beta1` | The pluginval release to download: a tag, or `latest`. |
| `pluginval-path` | | An existing pluginval binary to use instead of downloading one. |

Every `pluginval validate` option can also be set as an environment variable on
the step (e.g. `env: { TIMEOUT_MS: 60000 }`), which is the way to pass values
containing spaces.

## acceptance

Runs the [acceptance tests](../../docs/Acceptance%20testing.md) for every config
(`*.json`) found recursively under a directory, optionally against one built
plugin.

```yaml
- uses: Tracktion/pluginval/.github/actions/acceptance@v2
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
- uses: Tracktion/pluginval/.github/actions/acceptance@v2
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
