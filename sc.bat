doskey glslc=C:\VulkanSDK\1.4.335.0\Bin\glslc.exe $*
glslc -fshader-stage=vertex   -o lumina/data/shaders/spv/shader.vert.spv lumina/data/shaders/src/shader.vert
glslc -fshader-stage=fragment -o lumina/data/shaders/spv/shader.frag.spv lumina/data/shaders/src/shader.frag
pause