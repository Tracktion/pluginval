# pluginval

master: ![master](https://github.com/Tracktion/pluginval/workflows/Build/badge.svg?branch=master) 

develop: ![develop](https://github.com/Tracktion/pluginval/workflows/Build/badge.svg?branch=develop)

pluginval is a cross-platform plugin validator and tester application. It is designed to be used by both plugin and host developers to ensure stability and compatibility between plugins and hosts.

If you are a plugin user looking to report a problem with a plugin to the developers, you can use `pluginval` to create a detailed log which can drastically improve the time to fix issues. Please follow the instructions here to get started: [Testing plugins with pluginval](<docs/Testing plugins with pluginval.md>)


###### Highlights:
  - Test VST/AU/VST3 plugins
  - Compatible with macOS/Windows/Linux
  - Run in GUI or headless mode
  - Validation is performed in a separate process to avoid crashing


### Installation

The easiest way to get started with pluginval is to grab pre-compiled binaries from the [Releases page](https://github.com/Tracktion/pluginval/releases). Note that these are compiled with support for testing VST2. 

Once you need to [debug a failed validation](https://github.com/Tracktion/pluginval/blob/develop/docs/Debugging%20a%20failed%20validation.md#quick-debugging) you'll probably want to build locally in Debug:

```
git submodule update --init # grab JUCE 
cmake -B Builds/Debug -DCMAKE_BUILD_TYPE=Debug . # configure
cmake --build Builds/Debug --config Debug # build
```

### Third-party Installation
###### _Chocolatey (Windows):_
```shell
> choco install pluginval
```

You can find the chocolatey package of pluginval [here](https://chocolatey.org/packages/pluginval), and the source of the chocolatey package [here](https://github.com/Xav83/chocolatey-packages/tree/develop/automatic/pluginval), currently maintained by [Xav83](https://github.com/Xav83).

##### N.B. Enabling VST2 Testing:

The [pre-compiled binaries](https://github.com/Tracktion/pluginval/releases) are built with VST2 support. 

The VST2 SDK is no longer included in JUCE so VST2 support isn't available out from a fresh clone. 

To enable VST2 testing locally, set the `VST2_SDK_DIR` environment variable to point to the VST2 source directory on your system.

```
VST2_SDK_DIR=Builds/Debug/pluginval_artefacts/ cmake -B Builds/Debug .
``` 

### Running in GUI Mode
Once the app has built for your platform it will be found in `/bin`. Simply open the app to start it in GUI mode. Once open, you'll be presented with an empty plugin list. Click "Options" to scan for plugins, making sure to add any directories required.
Alternatively, you can drag a plugin file and drop it on the list to add only that plugin.

Once the list has populated, simply select a plugin and press the "Test Selected" button to validate it. The plugin will be loaded and each of the tests run in turn. Any output from the tests will be shown on the "Console" tab.
If you find problems with a plugin, this can be useful to send to the plugin developers.

### Running in Headless Mode
As well as being a GUI app, `pluginval` can be run from the command line in a headless mode.
This is great if you want to add validation as part of your CI process and be notified immediately if tests start failing.

###### Basic usage is as follows:
```
./pluginval --strictness-level 5 <path_to_plugin>
```
This will run all the tests up to level 5 on the plugin at the specified path.
Output will be fed to the console.
If all the tests pass cleanly, `pluginval` will return with an exit code of `0`. If any tests fail, the exit code will be `1`.
This means you can check the exit code on your various CI and mark builds a failing if all tests don't pass.

`strictness-level` is optional but can be between 1 & 10 with 5 being generally recognised as the lowest level for host compatibility. Lower levels are generally quick tests, mainly checking call coverage for crashes. Higher levels contain tests which take longer to run such as parameter fuzz tests and multiple state restoration.

###### You can also list all the options with:
```
./pluginval -h
```

### Guides
 - [Testing plugins with pluginval](<docs/Testing plugins with pluginval.md>)
 - [Debugging a failed validation](<docs/Debugging a failed validation.md>)
 - [Adding pluginval to CI](<docs/Adding pluginval to CI.md>)

### Contributing
If you would like to contribute to the project please do! It's very simple to add tests, simply:
1) Subclass `PluginTest`
    ```
    struct FuzzParametersTest  : public PluginTest
    ```
2) Override `runTest` to perform any tests
    ```
    void runTest (UnitTest& ut, AudioPluginInstance& instance) override
    ```
3) Log passes or failures with the `UnitTest` parameter
    ```
    ut.expect (editor != nullptr, "Unable to create editor");
    ```
4) Register your test with a static instance of it in a cpp file:
   ```
   static FuzzParametersTest fuzzParametersTest;
   ```

If you have a case you would like tests, please simply write the test in a fork and create a pull request. The more tests the better!

License
----

Licensing is under the `GPLv3` as we want `pluginval` to be as transparent as possible. If this conflicts with your requirements though please let us know and we can accommodate these.
