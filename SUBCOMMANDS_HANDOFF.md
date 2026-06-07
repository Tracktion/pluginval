# HANDOFF: Restructure pluginval's CLI into subcommands

## Goal
Turn the current "mode" flags into proper subcommands, each with its own
argument set, while keeping the old flat flags working as **deprecated aliases
for one release**:

| New (target) | Old (keep as deprecated alias) |
|---|---|
| `pluginval validate [options] <plugin>` (the **default**) | `pluginval --validate <plugin>` / `pluginval <plugin>` |
| `pluginval run-tests` | `pluginval --run-tests` |
| `pluginval strictness-help [level]` | `pluginval --strictness-help [level]` |
| `pluginval --version` / `pluginval --help` | (unchanged) |

All settings options (`--strictness-level`, `--config`, `--rtcheck`, …),
environment variables, and the JSON precedence pipeline belong to the `validate`
subcommand and are otherwise unchanged.

This was deliberately deferred from the CLI11 refactor PR (#174). The pipeline
and `PluginvalSettings` were designed to be reused unchanged.

## Read these first (don't trust this summary — verify against the files)
- `Source/SettingsParser.cpp/.h` — the parser. Key pieces to reuse:
  - `preprocess()` — deprecation rewrite, macOS flag strip, implicit `--validate`, tokenise.
  - `parseTokens(tokens, env)` — env → `--config` → CLI layering into one `PluginvalSettings`. **This is the `validate` body.**
  - `configureApp(app, s)` — registers every option on a `CLI::App`, used for both the env and CLI passes.
  - `createChildProcessCommandLine()` — the parent→child base64 handoff (`--config-base64 <b64> --validate <path>`).
  - `isCommandLine(tokens)` — what makes `shouldPerformCommandLine` return true.
- `Source/CommandLine.cpp` — `performCommandLine()` is the dispatcher today:
  token-scans for `--run-tests` / `--strictness-help`, otherwise runs `parseTokens` for validate; `--help`/`--version` go through CLI11. Also `runUnitTests()`, `printStrictnessHelp()`.
- `Source/Main.cpp` — calls `shouldPerformCommandLine()` then `performCommandLine()` with `getCommandLineParameters()` (a single `juce::String`).
- `Source/CommandLineTests.cpp` — the test contract.

## Recommended approach: a thin `argv[1]` verb dispatcher (not CLI11 subcommands)
The existing `parseTokens` pipeline (env/config/CLI layering via two `CLI::App`
passes) is the hard part and already works. Rather than re-express it inside
CLI11's native `add_subcommand` machinery (which complicates the env/config
layering because the options live on the subcommand), peel the verb off the
front and route:

1. In a new `dispatch()` step (in `SettingsParser` or `CommandLine.cpp`):
   - Tokenise the command line (reuse `preprocess` minus the implicit-validate step, or add a pre-step).
   - Look at the first non-option token:
     - `validate` → strip it, run the existing validate pipeline on the rest.
     - `run-tests` → `runUnitTests()`.
     - `strictness-help` → `printStrictnessHelp(level)`.
     - otherwise → **default to validate** (this preserves `pluginval <plugin>` and `pluginval --strictness-level 5 <plugin>`).
2. Each verb keeps its own small set of expected args. `validate` reuses
   `configureApp`/`parseTokens` verbatim. `run-tests` takes none.
   `strictness-help` takes an optional level.

This keeps `parseTokens` and the precedence layering untouched — the subcommand
work is purely a routing layer in front of it.

(If you prefer CLI11-native subcommands instead: put `configureApp` options on a
`validate` subcommand, and make `buildEnvArgv`/the env pass target that
subcommand's options. Doable, but more invasive for no functional gain here.)

## Deprecated-alias behaviour (one release)
Keep the old flat forms working, but print a one-line notice to stderr pointing
at the new syntax, e.g.:
- `pluginval --validate x` → run validate; warn `"--validate is deprecated; use 'pluginval validate x'"`.
- `pluginval --run-tests` → warn `"use 'pluginval run-tests'"`.
- `pluginval --strictness-help` → warn `"use 'pluginval strictness-help'"`.
- `pluginval <plugin>` (bare path) → **no warning** (still the documented shorthand for `validate`).

Gate the warnings behind detection of the old flag so the new subcommand form is
silent. Remove the aliases in the release after next; note it in `CHANGELIST.md`.

## Files to touch
- `Source/SettingsParser.{h,cpp}` — add the verb dispatch + a `Command` result (validate/run-tests/strictness-help/help/version), or expose a `dispatch()` that returns which verb + the remaining tokens. Keep `parseTokens` as the validate body.
- `Source/CommandLine.cpp` — `performCommandLine()` routes on the verb; emit the deprecation notices for old flat flags. `shouldPerformCommandLine()` must also recognise the bare verbs (`validate`/`run-tests`/`strictness-help`) in addition to the old flags.
- `Source/CommandLineTests.cpp` — add tests: each subcommand; default-to-validate; bare-path shorthand; every deprecated alias still works (and warns); `run-tests`/`strictness-help` arg handling.
- `docs/Command line options.md` — regenerate (`pluginval --help`); CLI11 can show per-subcommand help if you go native, otherwise hand-format the verb list.
- `CHANGELIST.md` — note the subcommand syntax + the deprecation.
- `CLAUDE.md` — update the "CLI Settings Pipeline" section to mention the verb layer.

## Gotchas / decisions to make
- **Child-process handoff.** `createChildProcessCommandLine()` emits `--config-base64 <b64> --validate <path>`. Decide whether the child invocation becomes `validate --config-base64 …` or stays flat. Simplest: keep it flat and have the dispatcher treat a leading `--config-base64`/`--validate` as the (deprecated, unwarned-for-internal) validate path. Note `--config-base64` is now hardened to reject being combined with non-`--validate` options — keep that working under whichever form you choose.
- **`shouldPerformCommandLine`** is what flips pluginval into CLI (vs GUI) mode in `Main.cpp`. It must return true for `pluginval run-tests` etc., not just the old flags.
- **`--help` scope.** With the dispatcher, `pluginval --help` is the top-level help (list verbs + the validate options). Consider `pluginval validate --help` for the full option list. CLI11-native subcommands give this for free.
- **Implicit validate** currently lives in `preprocess`. With an explicit `validate` verb, make sure `pluginval <plugin>` (no verb) still resolves to validate, and `pluginval validate <plugin>` doesn't double-insert `--validate`.
- **Reconcile** a positional plugin path under `validate` (e.g. `pluginval validate <plugin>`) with the existing `--validate <plugin>` option — pick one canonical form (recommend the positional for the new syntax, mapping it onto `s.validatePath`).

## Verify
- `pluginval run-tests` passes the full unit suite (it must, it's how CI runs tests).
- `pluginval validate --strictness-level 10 <plugin>` and `pluginval <plugin>` both validate.
- Every deprecated alias produces identical behaviour to before (plus a notice).
- CI matrix green (Linux/macOS/Windows build + dependency). Remember `.github/workflows/build.yaml` uses `--run-tests` and `--strictness-level 10 --validate …`; update those to the new syntax **and** keep an alias test, or the deprecation will fire in CI.
