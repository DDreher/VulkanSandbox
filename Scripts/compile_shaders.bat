%VULKAN_SDK%\Bin32\glslc.exe %~dp0..\assets\shaders\shader.vert -o %~dp0..\assets\shaders\vert.spv
%VULKAN_SDK%\Bin32\glslc.exe %~dp0..\assets\shaders\shader.frag -o %~dp0..\assets\shaders\frag.spv

EXIT /B %ERRORLEVEL%
