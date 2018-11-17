@echo OFF

cd "%~dp0%.."
set ROOT=%cd%

echo ROOT: "%ROOT%"
if not exist "%ROOT%" exit 1

set PROJECT_NAME=pluginval
set DEPLOYMENT_DIR=%ROOT%/bin/windows

set BINARY_NAME=%PROJECT_NAME%.exe
set APP_NAME=%BINARY_NAME%
set APP_FILE=%ROOT%\Builds\VisualStudio2017\x64\Release\App\%APP_NAME%

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

:: Resave Waveform project
"%PROJUCER_EXE%" --resave "%ROOT%/%PROJECT_NAME%.jucer"


::============================================================
::   Build Xcode projects
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
move "%APP_FILE%" "%DEPLOYMENT_DIR%"
