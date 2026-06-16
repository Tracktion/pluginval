# Acceptance testing (design)

> Status: **Phase 1 implemented.** The `pluginval test <config.json>` mode
> described here is built (`source/acceptance/`, wired into the CLI dispatcher)
> with the `sample` comparator, record-or-compare, float-WAV + JSON-sidecar
> references, a dogfood tone-generator plugin (`tests/test_plugins/tone_generator/`,
> behind `PLUGINVAL_BUILD_TEST_PLUGINS`) and CTest self-tests
> (`tests/acceptance/`). The Phase 2 items in §10 remain stubs/notes. This
> document is the reference spec for both the implementation and future extension.

## 1. Overview

The existing `validate` mode is a graded **unit-test suite** (`PluginTests :
juce::UnitTest`, self-registering `PluginTest` instances, pass/fail via
`expect`). It answers "does this plugin conform to the host API and behave
safely?"

Acceptance testing answers a different question: **"does this plugin produce the
output I expect for a known input and state?"** It is a deterministic
*render + golden-file comparison*:

1. Load a plugin, apply a known state / parameter set.
2. Feed a known input (audio and/or MIDI, or silence).
3. Render a fixed duration of audio.
4. If no reference exists, **record** one. If a reference exists, **compare**
   the freshly rendered output against it and emit a pass/fail verdict.

Because it is a fundamentally different activity from `validate`, it is built as
a **parallel subsystem** rather than as another `PluginTest`. It reuses the
plugin-loading, lifecycle, and render infrastructure but has its own CLI
command, config schema, and result model.

### Scope and honest caveats

- Acceptance testing is only meaningful for plugins that are **deterministic**
  given a fixed input + state. A plugin with free-running internal randomness
  cannot be golden-tested reliably. The planned playhead / seed controls help,
  but cannot make a non-deterministic plugin deterministic.
- The **cross-platform / cross-version portability** goal (matching a reference
  produced on a different OS, CPU, or plugin version) is realistic only with
  tolerant comparison methods. Exact / per-sample comparison rarely survives
  SIMD and platform floating-point differences. This is precisely why the
  comparator is a pluggable abstraction (see §5): spectrum, cross-correlation
  and fingerprint methods are what make portable references viable.

## 2. Command-line interface

A new subcommand sits alongside the existing verbs:

```
pluginval test <config.json>
```

- `<config.json>` is the **positional** acceptance-test definition (see §4). It
  is parsed by its own loader.
- If the config's reference file does not exist, it is **created** (record
  mode) and the command reports success.
- If the reference exists, the rendered output is **compared** against it and
  the command exits `0` (match) or `1` (mismatch), consistent with `validate`.

### Relationship to `--config` (important)

Do **not** confuse the acceptance-test config with the existing `--config`
flag. They are unrelated:

| | `--config file.json` | `pluginval test file.json` |
|---|---|---|
| Purpose | A *settings layer* for a **validate** run | The full **acceptance-test definition** |
| Schema | `PluginvalSettings` (strictness, timeouts, sample-rate lists, …) | Plugin + input + reference + state + comparison |
| Merge behaviour | `merge_patch`ed into the settings layering | Loaded standalone |
| How supplied | `--config` option | Positional argument to the `test` verb |

The acceptance-test definition is **never** routed through the
`PluginvalSettings` / `--config` layering.

## 3. Integration with the CLI pipeline

The CLI was rewritten (PRs #175 and #176) onto **CLI11** + an **nlohmann/json**
settings pipeline with a subcommand dispatcher. Acceptance testing plugs into
that dispatcher; it does **not** touch the validate settings-layering at all.

Required edits (small and localised):

| Location | Edit |
|---|---|
| `SettingsParser.h` — `enum class Command` | add `test` |
| `SettingsParser.cpp` — `dispatch()` | recognise the verb `test`, peel it, capture the positional config path(s) |
| `SettingsParser.cpp` — `isCommandLine()` | recognise `test` so CLI mode is triggered |
| `SettingsParser.cpp` — `getFooterText()` | add `test` to the `Commands:` help block |
| `CommandLine.cpp` — `performCommandLine()` | add a `Command::test` branch that runs the acceptance runner and async-quits |

Because `test` is brand new there are **no deprecated-alias** concerns.

### Process isolation

For v1, acceptance tests run **in-process**: record and compare both need the
rendered buffer back in the calling process, so in-process is the natural
choice. Crash isolation can be layered on later by mirroring the validate
child-process handoff (`createChildProcessCommandLine` in `SettingsParser.cpp`,
which passes an authoritative base64-JSON blob via `--config-base64`); the
equivalent would be `test --config-base64 <b64>` with the child writing the
reference / diff to disk and returning an exit code.

## 4. Config schema

The config is a plain, std-typed struct that follows the same **nlohmann/json
(de)serialisation pattern** as `PluginvalSettings.h`, plus a `toX()` boundary
conversion to JUCE types. **JSON keys are `snake_case`** (e.g. `sample_rate`).
Because the C++ members stay JUCE-style `camelCase`, the snake_case keys are
mapped explicitly via per-struct `to_json` / `from_json` (rather than the bare
`NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT` macro, which would emit the
member names verbatim). Missing keys still fall back to the defaults.

```jsonc
{
  "name": "myReverb-default",
  "plugin": "/path/to/Plugin.vst3",          // path or AU identifier string
  "input": { "audio": "in.wav", "midi": "in.mid" },   // either / both / omitted -> silence
  "reference": "refs/myReverb-default.wav",  // optional; default derived from "name"
  "state": { "file": "preset.state" },        // OR "parameters": { "Mix": 0.5, "3": 1.0 }
  "sample_rate": 48000,
  "block_size": 512,
  "render_duration": 2.0,                      // seconds; omitted -> use input length
  "comparison": { "sample": 1e-6 },            // omitted -> default 1/32768 (16-bit LSB)
  "playhead": { "bpm": 120 },                  // future
  "automation": [ /* phase 2 */ ]
}
```

### Field reference

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | string | basename of config file | Used to derive the default reference path and to label results. |
| `plugin` | string | — (required) | Absolute/relative path, or an AU identifier string. Relative paths resolve against the working directory. |
| `input.audio` | string | none | Path to an input audio file (WAV/AIFF/FLAC via `AudioFormatManager`). Used for effects. |
| `input.midi` | string | none | Path to a `.mid` file (`juce::MidiFile`). Used for instruments. |
| `reference` | string | `<dir-of-config>/<name>.wav` | The golden file. Absent -> record mode; present -> compare mode. |
| `state.file` | string | none | Binary blob from `getStateInformation`, applied via the VST3-safe helper. |
| `state.parameters` | object | none | `name-or-index -> normalised value`. Applied after `state.file` if both are given. |
| `sample_rate` | number | 44100 | Single value (not a list, unlike validate). |
| `block_size` | number | 512 | Single value. |
| `render_duration` | number | input length | Seconds. `num_samples = round(duration * sample_rate)`. Input shorter than the duration is padded with silence; longer is truncated. |
| `comparison` | object | `{ "sample": 0.0000305 }` | Map of comparator name -> its sub-config. See §5. |
| `playhead` | object | none | **Future.** Fixed tempo / time signature for time-dependent plugins. |
| `automation` | array | none | **Phase 2.** Parameter changes scheduled at sample positions. |

### State precedence

If both `state.file` and `state.parameters` are present, the binary state is
applied **first** (it represents the full plugin state), then the parameter map
is applied on top as overrides.

### Multiplexing (future)

The loader accepts **either a single object or a top-level array** of configs.
v1 may execute only the single / first entry; phase 2 iterates the array to
multiplex many test cases from one file. Designing the parser for arrays now
costs nothing and avoids a schema break later. Each array entry should carry a
`name` so its reference path and result are uniquely identifiable.

## 5. Comparator abstraction (the core extension point)

Comparison is **pluggable**. The config selects one or more methods; v1 ships
only `sample`, and additional methods register later with **zero** changes to
config parsing or the runner.

```cpp
struct ComparisonResult
{
    bool         passed = false;
    double       score  = 0.0;   // method-specific metric (e.g. max abs diff)
    juce::String summary;        // human-readable
    nlohmann::json details;      // structured, for the machine-readable report
};

struct Comparator
{
    virtual ~Comparator() = default;
    virtual juce::String getName() const = 0;          // "sample", "spectrum", ...
    virtual ComparisonResult compare (const juce::AudioBuffer<float>& reference,
                                      const juce::AudioBuffer<float>& output,
                                      const nlohmann::json& config) = 0;  // value under comparison[name]
};
```

- A small **registry** maps method name -> factory.
- `config` is whatever sits under the comparator's key — a bare number
  (`1e-6`) for `sample`, or an object for richer methods
  (e.g. `"spectrum": { "tolerance": 0.05, "fftSize": 2048 }`).
- When multiple methods are listed, **all must pass** (logical AND); each is
  reported separately.

### `comparison` defaults

When `comparison` is omitted, the default is:

```json
"comparison": { "sample": 0.0000305 }   // 1.0 / 32768.0, i.e. one 16-bit LSB
```

### Roadmap of comparators

| Method | Phase | Description |
|---|---|---|
| `sample` | **v1** | Per-sample absolute-difference tolerance (`0` = bit-exact) plus a length check. |
| `peakrms` | future | Peak and RMS difference thresholds. |
| `spectrum` | future | FFT-magnitude comparison with tolerance; robust to small phase/SIMD differences. |
| `crosscorr` | future | Cross-correlation, tolerant to small latency offsets. |
| `fingerprint` | future | Acoustic fingerprint match; most robust for cross-platform/version. |

The `spectrum` / `crosscorr` / `fingerprint` methods are what make the
**cross-platform-portable** reference goal practical.

## 6. Reference artifact format

A reference is **two files**:

1. **`<name>.wav`** — the rendered output as a **32-bit float WAV** (preserves
   full precision; it is the source of truth for comparison).
2. **`<name>.wav.json`** — a sidecar manifest:

```jsonc
{
  "plugin":      { "name": "...", "manufacturer": "...", "version": "...",
                   "format": "VST3", "uid": "..." },
  "render":      { "sample_rate": 48000, "block_size": 512, "num_channels": 2,
                   "num_samples": 96000, "length_seconds": 2.0 },
  "pluginval_version": "2.0.0",
  "config_hash":  "…",   // hash of the resolved config (excluding the reference path)
  "created_on":   { "os": "macOS", "arch": "arm64", "date": "2026-…" }  // informational only
}
```

- `created_on` is **informational only** — it is never used for matching, because
  references are meant to be portable and shared.
- `config_hash` lets the runner detect a **stale** reference (one produced from a
  different config than the one now being run) and warn.
- The default reference path deliberately contains **no platform/arch**, since
  references are intended to be portable and checked into a repo.

### Diff output on failure

When a comparison fails, the runner also writes a **diff WAV** (`output − reference`)
next to the result to aid debugging.

## 7. Module layout

```
source/acceptance/
  TestConfig.h/.cpp          // std-typed struct + NLOHMANN_DEFINE_TYPE..._WITH_DEFAULT
                             //   + toRenderSpec() boundary conversion (mirrors PluginvalSettings.h)
  AcceptanceTest.h/.cpp      // resolve + load plugin, apply state, feed input, render -> buffer
  ReferenceComparator.h/.cpp // Comparator interface + registry + SampleComparator; record-or-compare
  TestReporter.h/.cpp        // text + JSON result, exit code
```

Reused infrastructure:

- Plugin loading / scanning: `AudioPluginFormatManager::createPluginInstance`
  and `KnownPluginList` (as in `PluginTests.cpp`).
- VST3-safe lifecycle helpers in `TestUtilities.h`:
  `callPrepareToPlayOnMessageThreadIfVST3`,
  `callSetStateInformationOnMessageThreadIfVST3`, etc. (VST3 requires several
  operations on the message thread).
- The render-loop shape from the `AudioProcessingTest` in
  `source/tests/BasicTests.cpp` (`prepareToPlay` -> `processBlock` loop with
  `AudioBuffer` + `MidiBuffer`).

Build wiring: add the new `.cpp` files to the `SourceFiles` list in
`CMakeLists.txt`, and add acceptance-mode cases to `source/CommandLineTests.cpp`
following the existing test style.

## 8. Dogfood test plugin (tone generator)

To develop and self-test the acceptance feature we need a **device under test
that is fully deterministic** — something whose output is known exactly, doesn't
depend on a third-party binary, and is stable across runs and platforms. We
build a tiny in-repo **tone-generator plugin** for this purpose ("dogfooding"
the feature with our own plugin).

### What it is

A minimal JUCE plugin (built with `juce_add_plugin`) that synthesises a simple,
deterministic tone:

- **Parameters** (so we can exercise state / parameter application):
  - `waveform` — choice: `sine`, `square` (extendable to `saw`, `triangle`).
  - `frequency` — Hz (e.g. 20–20000, default 440).
  - `gain` — linear or dB (default −6 dB).
- **Determinism**: the oscillator **phase resets to 0 on `prepareToPlay`** and
  the waveform is computed in closed form from the sample index, so the same
  config always renders the identical buffer (ideal for the `sample`
  comparator, including bit-exact). No randomness, no denormal-sensitive
  feedback paths.
- **No audio input required**: it is a generator, so acceptance configs for it
  use silence input (or omit `input`) and a fixed `render_duration`.
- Produces a VST3 on all platforms (and an AU on macOS) so the same plugin
  exercises both formats.

### Where it lives

A new CMake target alongside the existing `tests/test_plugins/`, e.g.
`pluginval_tone_generator`, built on demand (guarded behind an option such as
`PLUGINVAL_BUILD_TEST_PLUGINS`, off for normal release builds). Suggested
layout:

```
tests/test_plugins/tone_generator/
  CMakeLists.txt          // juce_add_plugin target
  ToneGeneratorPlugin.h/.cpp
```

### How it self-tests the acceptance feature

Check in a handful of acceptance configs plus their recorded reference WAVs and
wire them into CTest, so CI both validates the tone generator's stability and
exercises the full record/compare path:

```
tests/acceptance/
  sine-440.json.in        // tone gen, waveform=sine,  state.parameters
  square-220.json.in      // tone gen, waveform=square, state.parameters, bit-exact
  square-state.json.in    // tone gen, state.file blob + a gain parameter override
  gain-half.json.in       // gain effect, input.audio = a full-height sine, bit-exact
  inputs/sine-full.wav    // checked-in input for gain-half (±1.0 sine)
  refs/<case>.wav (+ .wav.json)   // checked-in references + sidecar manifests
  refs/square-state.state         // checked-in getStateInformation blob
```

The configs are checked-in **templates** (`*.json.in`): the plugin artefact
paths and the input/reference directories are substituted at CMake configure
time (the tests can't hardcode a build-tree plugin path). Each `pluginval test
<config>` CTest case asserts exit code `0` against the checked-in reference.
Because the dogfood plugins are closed-form deterministic, these references are
stable enough to commit — a first smoke test of cross-platform portability for
the `sample` comparator and the baseline for future comparators (`spectrum`,
`crosscorr`, …).

The four cases cover the distinct render paths:

- **`sine-440` / `square-220`** — the `state.parameters` (name/index → normalised
  value) path. `square-220` compares bit-exact (`"sample": 0`).
- **`square-state`** — the binary `state.file` (`setStateInformation`) path,
  plus a `state.parameters` `gain` override on top, exercising the
  state-then-parameters precedence of §4. The blob is captured once and checked
  in alongside its reference WAV.
- **`gain-half`** — the **`input.audio`** effect path: a second dogfood plugin
  (`tests/test_plugins/gain/`, a deterministic gain effect) gains a checked-in
  full-height (±1.0) sine by 0.5 and is compared bit-exact (0.5 is exact in
  float). It also omits `render_duration`, so the render length is derived from
  the input file.

## 9. Execution flow

1. Parse `pluginval test <config.json>` into one or more `TestConfig`.
2. Resolve the plugin (path or ID); create the instance at the config's
   `sample_rate` / `block_size`.
3. Apply `state.file` then `state.parameters`.
4. Load `input.audio` and/or `input.midi`; otherwise use silence.
5. `prepareToPlay`, render `render_duration` worth of blocks, accumulating the
   output into a single buffer.
6. **No reference exists** -> write the float WAV + manifest (record mode);
   report "reference created".
7. **Reference exists** -> run each configured comparator; report each verdict
   and the overall pass/fail; on failure write a diff WAV; exit `0` / `1`.

## 10. Phasing

**Phase 1 (initial implementation)**

- `pluginval test <config.json>` subcommand wired into `settings_parser`.
- Single-config (object) execution; parser already accepts arrays.
- Plugin load + state (file and/or parameter map) + file-based audio/MIDI input
  (or silence).
- Fixed-duration render in-process.
- Record-or-compare with float-WAV + JSON-sidecar references.
- `sample` comparator only (default tolerance = one 16-bit LSB).
- Text + JSON result reporting; diff WAV on failure.
- **Dogfood tone-generator plugin** (§8) plus a CTest self-test that runs the
  full record/compare path against checked-in references.

**Phase 2 and beyond**

- Multiplexed execution of config arrays.
- Parameter `automation` timelines.
- Fixed `playhead` (tempo / time signature) for time-dependent plugins.
- Additional comparators: `peakrms`, `spectrum`, `crosscorr`, `fingerprint`.
- Synthesised inputs (`input.generator`: noise / sine, with seed).
- Optional child-process isolation mirroring the validate handoff.
