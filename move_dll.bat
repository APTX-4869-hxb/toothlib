@echo off
set "source_dir=cmake\libtorch\lib"
set "destination_dir=build\Debug"

echo coping DLL files from %source_dir% to %destination_dir% ...

if not exist "%source_dir%" (
    echo Source directory %source_dir% not found.
    exit /b 1
)

if not exist "%destination_dir%" (
    echo Destination directory %destination_dir% not found.
    exit /b 1
)

for /r "%source_dir%" %%f in (*.dll) do (
    echo Moving "%%~nxf" ...
    copy /y "%%f" "%destination_dir%\%%~nxf" > nul
)

echo DLL files copied successfully!
