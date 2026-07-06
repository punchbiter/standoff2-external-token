@echo off
setlocal
set "NDK_PATH=C:\Users\APIHACKER\AppData\Local\Android\Sdk\ndk\29.0.14206865"
set "NDK_BUILD=%NDK_PATH%\ndk-build.cmd"
set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
call "%NDK_BUILD%" ^
    NDK_PROJECT_PATH="%PROJECT_DIR%" ^
    APP_BUILD_SCRIPT="%PROJECT_DIR%\Android.mk" ^
    NDK_APPLICATION_MK="%PROJECT_DIR%\Application.mk" ^
    -j%NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Build failed.
    pause
    exit /b %ERRORLEVEL%
)
echo [OK] Build succeeded!
if not exist "%PROJECT_DIR%\output" mkdir "%PROJECT_DIR%\output"
copy /Y "%PROJECT_DIR%\libs\x86_64\token" "%PROJECT_DIR%\output\token" >nul
copy /Y "%PROJECT_DIR%\libs\arm64-v8a\token" "%PROJECT_DIR%\output\token_arm64-v8a" >nul
pause
endlocal
