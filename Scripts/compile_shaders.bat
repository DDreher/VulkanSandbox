%VULKAN_SDK%\Bin32\glslc.exe %~dp0..\assets\shaders\shader.vert -o vert.spv
%VULKAN_SDK%\Bin32\glslc.exe %~dp0..\assets\shaders\shader.frag -o frag.spv

EXIT /B %ERRORLEVEL%
