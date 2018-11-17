@echo OFF

cd "%~dp0%.."
set ROOT=%cd%

echo ROOT: "%ROOT%"
if not exist "%ROOT%" exit 1

set PROJECT_NAME=pluginval
set DEPLOYMENT_DIR=%ROOT%/bin/windows
set PLUGINVAL_EXE=%DEPLOYMENT_DIR%\%PROJECT_NAME%.exe

if not defined MSBUILD_EXE set MSBUILD_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe
echo "MSBUILD_EXE: %MSBUILD_EXE%"

::============================================================
::   First build pluginval
::============================================================
call "%ROOT%/install/windows_build.bat" || exit 1

::============================================================
::   Build Projucer and generate test plugin projects
::============================================================
echo "Building Projucer and creating projects"

set PROJUCER_ROOT=%ROOT%/modules/juce/extras/Projucer/Builds/VisualStudio2017
set PROJUCER_EXE=%PROJUCER_ROOT%/x64/Release/App/Projucer.exe
set PLUGINS_PIP_DIR=%ROOT%/modules/juce/examples/Plugins
set TEMP_DIR=%ROOT%/tmp
rd /S /Q "%TEMP_DIR%"

cd "%PROJUCER_ROOT%"
set CL=/DJUCER_ENABLE_GPL_MODE
"%MSBUILD_EXE%" Projucer.sln /p:VisualStudioVersion=15.0 /m /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
if not exist "%PROJUCER_EXE%" exit 1


::============================================================
::   Test plugins
::============================================================
:: Disabling these tests for now as they can't be built due to dependancies on the VST2 SDK which 
:: can't be found without the use of Projucer's Global Settings
rem call :TestPlugin "ArpeggiatorPlugin", "ArpeggiatorPluginDemo.h"
rem call :TestPlugin "AudioPluginDemo", "AudioPluginDemo.h"
rem call :TestPlugin "DSPModulePluginDemo", "DSPModulePluginDemo.h"
rem call :TestPlugin "GainPlugin", "GainPluginDemo.h"
rem call :TestPlugin "MultiOutSynthPlugin", "MultiOutSynthPluginDemo.h"
rem call :TestPlugin "NoiseGatePlugin", "NoiseGatePluginDemo.h"
rem call :TestPlugin "SamplerPlugin", "SamplerPluginDemo.h"
rem call :TestPlugin "SurroundPlugin", "SurroundPluginDemo.h"
exit /B %ERRORLEVEL%

:TestPlugin
    echo "=========================================================="
    echo "Testing: %~1%"
    set PLUGIN_NAME=%~1%
    set PLUGIN_PIP_FILE=%~2%
    set PLUGIN_VST3="%TEMP_DIR%/%PLUGIN_NAME%/Builds/VisualStudio2017/x64/Release/VST3/%PLUGIN_NAME%.vst3"
    call "%PROJUCER_EXE%" --create-project-from-pip "%PLUGINS_PIP_DIR%\%PLUGIN_PIP_FILE%" "%TEMP_DIR%" "%ROOT%/modules/juce/modules"
    call "%PROJUCER_EXE%" --resave "%TEMP_DIR%/%PLUGIN_NAME%/%PLUGIN_NAME%.jucer"
    cd "%TEMP_DIR%/%PLUGIN_NAME%/Builds/VisualStudio2017"
    set CL=/DJUCE_PLUGINHOST_VST#0
    "%MSBUILD_EXE%" %PLUGIN_NAME%.sln /p:VisualStudioVersion=15.0 /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64  /p:TreatWarningsAsErrors=true

    :: Test out of process
    call "%PLUGINVAL_EXE%" --strictness-level 5 --validate %PLUGIN_VST3%
    if %ERRORLEVEL% NEQ 0 exit 1

    :: Test in process
    call "%PLUGINVAL_EXE%" --validate-in-process --strictness-level 5 --validate %PLUGIN_VST3%
    if %ERRORLEVEL% NEQ 0 exit 1
exit /B 0
