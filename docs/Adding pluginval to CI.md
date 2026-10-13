# Adding pluginval to CI

`pluginval` was designed from the start to easily fit in to build tools and continuous integration solutions. Doing this helps improve confidence in release builds, knowing plugins are continuously tested. Additionally, `pluginval` can automatically generate log files which can then be sent to Tracktion as part of their ["Verified by pluginval"](https://www.tracktion.com/develop/pluginval) scheme.

There a couple of different ways this can be done but the simplest is as follows:
1. As part of your build scripts, after the plugins have been built, download the latest binaries of `pluginval`
2. Unzip the `pluginval` binaries
3. Invoke `pluginval` from the command line to test each plugin
4. If validation fails, fail your build
5. If validation passes, retain the generated log files ready to submit to "Verified by pluginval"

### GitHub Actions
On GitHub Actions the simplest route is the `validate` action, which downloads pluginval, validates each plugin and fails the job if any fail:

```yaml
- uses: Tracktion/pluginval/.github/actions/validate@v2
  with:
    plugins: |
      build/MyPlugin_artefacts/Release/VST3/MyPlugin.vst3
      build/MyPlugin_artefacts/Release/AU/MyPlugin.component
    strictness-level: 5
    args: --skip-gui-tests
```

There is also an `acceptance` action for [acceptance tests](Acceptance%20testing.md). See the [actions README](../.github/actions/README.md) for all the inputs and outputs.

### Examples
In these examples the following is assumed:
 - macOS/Linux, curl is installed
 - You have a directory `./bin` where you are putting your build products
 - You are validating with a strictness level of 5 (the default). It is a much better idea to specify `--strictness-level 5` on the command line

##### macOS
```sh
$ curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip" -o pluginval.zip
$ unzip pluginval
$ pluginval.app/Contents/MacOS/pluginval validate --output-dir "./bin" "<path_to_plugin>" || exit 1
```

##### Linux
```sh
$ curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip" -o pluginval.zip
$ unzip pluginval
$ ./pluginval validate --output-dir "./bin" "<path_to_plugin>" || exit 1
```

##### Windows
```sh
> powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip -OutFile pluginval.zip"
> powershell -Command "Expand-Archive pluginval.zip -DestinationPath ."
> pluginval.exe validate --output-dir "./bin" "<path_to_plugin>"
> if %ERRORLEVEL% neq 0 exit /b 1
```

*Or install via Chocolatey on Windows*
```sh
> choco install pluginval
```

### Machine-readable reports
Both `pluginval validate` and `pluginval test` can write their results to a file, as well as printing them:

 - `--junit-xml <file>` writes a JUnit XML report, which most CI systems (GitHub Actions, GitLab, Jenkins, Azure Pipelines) can display as per-test results
 - `--json-results <file>` writes the same results as JSON, for scripts and dashboards

Relative paths are resolved against the working directory and any missing directories are created. Both options can be used together, and can also be set as the `JUNIT_XML` and `JSON_RESULTS` environment variables. The text output and log files are unchanged. If a report can't be written (e.g. the path isn't writable) the run fails with exit code `1`.

```sh
$ ./pluginval validate --strictness-level 5 --junit-xml "./reports/MyPlugin.xml" --json-results "./reports/MyPlugin.json" "<path_to_plugin>"
```

Each test pluginval runs becomes one `<testcase>`, with a `<failure>` holding the failure messages if it failed:

```xml
<testsuites name="pluginval" tests="20" failures="1" errors="0" time="12.345">
  <testsuite name="pluginval validate: MyPlugin.vst3" tests="20" failures="1" errors="0" time="12.345" skipped="0" timestamp="...">
    <testcase classname="pluginval.validate" name="Open plugin (cold)" time="0.150"/>
    <testcase classname="pluginval.validate" name="Plugin state" time="0.020">
      <failure message="...">...</failure>
    </testcase>
  </testsuite>
</testsuites>
```

The JSON report has the same content:

```json
{
  "pluginval_version": "2.0.0",
  "command": "validate",
  "target": "/path/to/MyPlugin.vst3",
  "start_time": "...",
  "duration_ms": 12345,
  "properties": { "plugin_name": "MyPlugin", "strictness_level": "5" },
  "summary": { "tests": 20, "passed": 19, "failed": 1, "errors": 0 },
  "tests": [
    { "name": "Open plugin (cold)", "status": "passed", "duration_ms": 150, "messages": [] }
  ]
}
```

`status` is `passed`, `failed` or `error`. For `pluginval test`, each test also has a `details` object holding the comparison results.

If pluginval doesn't finish (the plugin crashes it, or the run times out), the report contains a single `error` entry saying so, so a report left over from an earlier run is never mistaken for a pass.

##### GitHub Actions
```yaml
- name: Validate plugin
  run: ./pluginval validate --strictness-level 5 --skip-gui-tests --junit-xml reports/pluginval.xml ./build/MyPlugin.vst3

- name: Upload pluginval report
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: pluginval-report
    path: reports/
```
