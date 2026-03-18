@echo off
setlocal

:: Usage: spirv_generate_interface.bat <file_name>.glsl
:: Compiles the interface GLSL to SPIR-V, reflects it, and outputs a .hpp
:: to interfaces/headers/

set GLSLC=C:\VulkanSDK\1.4.335.0\Bin\glslc.exe
set REFLECT=build\Debug\bin\spirv_reflect_tool.exe
set IFACE_DIR=lumina\data\shaders\interfaces
set OUT_DIR=%IFACE_DIR%\headers

if "%~1"=="" (
    echo Usage: spirv_generate_interface.bat ^<file_name^>.glsl
    echo Example: spirv_generate_interface.bat standard_lit.vert.glsl
    exit /b 1
)

set GLSL_FILE=%~1
set BASE_NAME=%~n1

:: Determine shader stage from the file name (e.g. standard_lit.vert.glsl -> vert)
:: %~n1 strips .glsl giving "standard_lit.vert", then we extract the extension of that
for %%A in (%BASE_NAME%) do set STAGE_EXT=%%~xA
set STAGE=%STAGE_EXT:~1%

if "%STAGE%"=="vert" goto stage_vert
if "%STAGE%"=="frag" goto stage_frag
if "%STAGE%"=="global" goto stage_global

echo Error: Could not determine shader stage from file name "%GLSL_FILE%"
echo File must contain .vert., .frag., or .global. in its name.
exit /b 1

:stage_vert
set SHADER_STAGE=vertex
goto do_reflect

:stage_frag
set SHADER_STAGE=fragment
goto do_reflect

:stage_global
set SHADER_STAGE=vertex
goto do_reflect

:do_reflect
if not exist %OUT_DIR% mkdir %OUT_DIR%

:: Create temp wrapper with #version, #include, and stub main
set TMP_DIR=%TEMP%\lumina_reflect_tmp
if not exist %TMP_DIR% mkdir %TMP_DIR%

set TMP_FILE=%TMP_DIR%\%GLSL_FILE%.reflect

echo #version 450> %TMP_FILE%
echo #include "%GLSL_FILE%">> %TMP_FILE%
if "%SHADER_STAGE%"=="vertex" (
    echo void main^(^) { gl_Position = vec4^(0.0^); }>> %TMP_FILE%
) else (
    echo void main^(^) {}>> %TMP_FILE%
)

:: Compile to SPIR-V with -O0 to prevent dead-code removal
%GLSLC% -fshader-stage=%SHADER_STAGE% -O0 -I %IFACE_DIR% -o %TMP_DIR%\%BASE_NAME%.spv %TMP_FILE%
if errorlevel 1 (
    echo Error: GLSL compilation failed for %GLSL_FILE%
    del %TMP_FILE% 2>nul
    exit /b 1
)

:: Reflect SPIR-V to generate the .hpp header
%REFLECT% %TMP_DIR%\%BASE_NAME%.spv %OUT_DIR%\%BASE_NAME%.hpp
if errorlevel 1 (
    echo Error: SPIR-V reflection failed for %GLSL_FILE%
    del %TMP_FILE% 2>nul
    del %TMP_DIR%\%BASE_NAME%.spv 2>nul
    exit /b 1
)

:: Clean up temp files
del %TMP_FILE%
del %TMP_DIR%\%BASE_NAME%.spv

:: Format the generated header
"C:\Program Files\LLVM\bin\clang-format.exe" -i %OUT_DIR%\%BASE_NAME%.hpp

echo Generated %OUT_DIR%\%BASE_NAME%.hpp

endlocal
