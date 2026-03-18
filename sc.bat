set GLSLC=C:\VulkanSDK\1.4.335.0\Bin\glslc.exe
set SPV_DIR=lumina\data\shaders\spv

if not exist %SPV_DIR% mkdir %SPV_DIR%

:: ============================================================
:: Compile shaders to SPIR-V
:: ============================================================

set IFACE_DIR=lumina\data\shaders\interfaces

%GLSLC% -fshader-stage=vertex   -I %IFACE_DIR% -o %SPV_DIR%\shader.vert.spv lumina\data\shaders\src\shader.vert
%GLSLC% -fshader-stage=fragment -I %IFACE_DIR% -o %SPV_DIR%\shader.frag.spv lumina\data\shaders\src\shader.frag

pause
