@echo OFF

cd "%~dp0%.."
set ROOT=%cd%

echo ROOT: "%ROOT%"
if not exist "%ROOT%" exit 1

set PROJECT_NAME=pluginval
set DEPLOYMENT_DIR=%ROOT%\bin\windows

set BINARY_NAME=%PROJECT_NAME%.exe
set APP_NAME=%BINARY_NAME%
set APP_FILE="%ROOT%\Builds\VisualStudio2017\x64\Release\App\%APP_NAME%"

set ZIP_FILE="%DEPLOYMENT_DIR%\%PROJECT_NAME%_Windows.zip"

if not defined MSBUILD_EXE set MSBUILD_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe

:: First clear the bin
echo "=========================================================="
echo "Removing old files %ROOT%/bin/windows"
rd /S /Q "%ROOT%\bin\windows"

::============================================================
::   Build Projucer and generate projects
::============================================================
echo "Building Projucer and creating projects"
set PROJUCER_ROOT=%ROOT%/modules/juce/extras/Projucer/Builds/VisualStudio2017
set PROJUCER_EXE=%PROJUCER_ROOT%/x64/Release/App/Projucer.exe

cd "%PROJUCER_ROOT%"
set CL=/DJUCER_ENABLE_GPL_MODE
"%MSBUILD_EXE%" Projucer.sln /p:VisualStudioVersion=15.0 /m /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
if not exist "%PROJUCER_EXE%" exit 1

:: Resave project
"%PROJUCER_EXE%" --resave "%ROOT%/%PROJECT_NAME%.jucer"


::============================================================
::   Prepare VST2 SDK
::============================================================
if "%VST2_SDK_URL%" == "" goto SKIP_VST2_URL
	rd /S /Q "%ROOT%/tmp"
	mkdir "%ROOT%/tmp"
	cd "%ROOT%/tmp"
	powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest %VST2_SDK_URL% -OutFile vstsdk2.4.zip"
	powershell -Command "Expand-Archive vstsdk2.4.zip -DestinationPath ."
    if exist "vstsdk2.4" set VST2_SDK_DIR=%ROOT%/tmp/vstsdk2.4
:SKIP_VST2_URL


::============================================================
::   Add VST2 capabilities
::============================================================
if not exist "%VST2_SDK_DIR%" goto SKIP_VST2_SUPPORT

	echo "Modifying jucer project for VST2 support"
	powershell -Command "(Get-Content '%ROOT%/%PROJECT_NAME%.jucer') -replace '/format_types/VST3_SDK', '/format_types/VST3_SDK;%VST2_SDK_DIR%' -replace 'JUCEOPTIONS JUCE_PLUGINHOST_AU=\"1\"', 'JUCEOPTIONS JUCE_PLUGINHOST_VST=\"1\" JUCE_PLUGINHOST_AU=\"1\"' | Set-Content '%ROOT%/%PROJECT_NAME%.jucer'"
	"%PROJUCER_EXE%" --resave "%ROOT%/%PROJECT_NAME%.jucer"
	goto DONE_VST2_SUPPORT

:SKIP_VST2_SUPPORT
	echo "Not building with VST2 support. To enable VST2 support, set the VST2_SDK_DIR environment variable"

:DONE_VST2_SUPPORT


::============================================================
::   Build projects
::============================================================
echo "=========================================================="
echo "Building products"
cd "%ROOT%/Builds/VisualStudio2017"
rd /S /Q "x64/Release"
"%MSBUILD_EXE%" %PROJECT_NAME%.sln /p:VisualStudioVersion=15.0 /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64  /p:TreatWarningsAsErrors=true


::============================================================
::   Copy to deployment directory
::============================================================
cd $ROOT
rd /S /Q "%DEPLOYMENT_DIR%"
mkdir "%DEPLOYMENT_DIR%"

echo "\nDeploying to: " %DEPLOYMENT_DIR%
powershell -Command "Compress-Archive -Path '%APP_FILE%' -DestinationPath '%ZIP_FILE%'"
