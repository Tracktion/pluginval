# Adding pluginval to CI

`pluginval` was designed from the start to easily fit in to build tools and continuous integration solutions. Doing this helps improve confidence in release builds, knowing plugins are continuously tested. Additionally, `pluginval` can automatically generate log files which can then be sent to Tracktion as part of their ["Verified by pluginval"](https://www.tracktion.com/develop/pluginval) scheme.

There a couple of different ways this can be done but the simplest is as follows:
1. As part of your build scripts, after the plugins have been built, download the latest binaries of `pluginval`
2. Unzip the `pluginval` binaries
3. Invoke `pluginval` from the command line to test each plugin
4. If validation fails, fail your build
5. If validation passes, retain the generated log files ready to submit to "Verified by pluginval"

### Examples
In these examples the following is assumed:
 - macOS/Linux, curl is installed
 - You have a directory `./bin` where you are putting your build products
 - You are validating with a strictness level of 5 (the default). It is a much better idea to specify `--strictness-level 5` on the command line

##### macOS
```sh
$ curl -L "https://github.com/Tracktion/pluginval/releases/download/latest_release/pluginval_macOS.zip" -o pluginval.zip
$ unzip pluginval
$ pluginval.app/Contents/MacOS/pluginval --validate-in-process --output-dir "./bin" --validate "<path_to_plugin>" || exit 1
```

##### Linux
```sh
$ curl -L "https://github.com/Tracktion/pluginval/releases/download/latest_release/pluginval_Linux.zip" -o pluginval.zip
$ unzip pluginval
$ ./pluginval --validate-in-process --output-dir "./bin" --validate "<path_to_plugin>" || exit 1
```

##### Windows
```sh
> powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/Tracktion/pluginval/releases/download/latest_release/pluginval_Windows.zip -OutFile pluginval.zip"
> powershell -Command "Expand-Archive pluginval.zip -DestinationPath ."
> pluginval.exe --validate-in-process --output-dir "./bin" --validate "<path_to_plugin>"
> if %ERRORLEVEL% neq 0 exit /b 1
```
