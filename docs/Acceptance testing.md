# Acceptance testing

Acceptance testing answers a different question from normal validation. Instead of
"does this plugin conform to the host API and behave safely?" it asks **"does this
plugin still produce the output I expect for a known input and state?"** — a
deterministic *render + golden-file comparison*. It is ideal for catching
accidental DSP changes (a wrong coefficient, a refactor that shifts the output) in
your own plugins from CI.

It is driven by a small JSON config and a dedicated command:

```
pluginval test myPlugin-default.json
```

- The **first** run renders the plugin and, finding no reference, **records** one
  (a `.wav` plus a `.wav.json` manifest) next to the config and reports success.
- **Subsequent** runs render again and **compare** against that reference,
  exiting `0` on a match and `1` on a mismatch (writing a diff `.wav` to help you
  see what changed).

So the workflow is: write a config, run once to record the reference, **commit the
config and the reference**, then let CI run the same command on every change.

### A minimal config

```json
{
  "name": "myReverb-default",
  "plugin": "/path/to/MyReverb.vst3",
  "input": { "audio": "inputs/drums.wav" },
  "reference": "refs/myReverb-default.wav",
  "state": { "parameters": { "Mix": 0.5, "Decay": 0.8 } },
  "sample_rate": 48000,
  "block_size": 512,
  "render_duration": 2.0,
  "comparison": { "sample": 1e-6 }
}
```

JSON keys are `snake_case`. The most useful fields:

| Field | Notes |
|---|---|
| `plugin` | Path to the plugin (or an AU identifier). **Required.** |
| `input.audio` / `input.midi` | Input files to feed it. Omit both for silence (e.g. instruments driven only by MIDI, or generators). |
| `state.parameters` | A map of parameter name (or index) → **normalised** value (`0`–`1`), applied before rendering. |
| `state.file` | A binary `getStateInformation` blob to restore first (e.g. a captured preset). Applied before `state.parameters`. |
| `reference` | The golden `.wav`. Defaults to `<name>.wav` next to the config. |
| `render_duration` | Seconds to render. If omitted, the input audio's length is used. |
| `comparison` | How to compare. `{ "sample": <tolerance> }` is a per-sample absolute-difference tolerance (`0` = bit-exact); the default is one 16-bit LSB. |

### Determinism matters

Acceptance testing only works for output that is reproducible from a fixed input
and state. A plugin with free-running randomness can't be golden-tested reliably.
For the same reason, references are only safely **portable across platforms** when
you allow a tolerance — exact per-sample matches rarely survive different CPUs and
floating-point libraries. If a reference recorded on one OS fails on another, raise
the `sample` tolerance (or use a more tolerant comparison method as they are added).

### Running from CI

`pluginval test` is just a command that returns an exit code, so any CI system can
run it the same way it runs your other checks:

```bash
pluginval test tests/acceptance/myReverb-default.json
```

A non-zero exit fails the build. Commit the config and its reference `.wav`
alongside your project so every run compares against the same golden file.

For the complete schema, the comparator design and the planned roadmap, see the
[design document](<../tests/acceptance/Acceptance testing design.md>).
