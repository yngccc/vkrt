pushd %~dp0

set flags=--target-env=vulkan1.2 -g

%VK_SDK_PATH%\bin\glslc.exe %flags% swapChain.vert -o %1\swapChain.vert.spv
%VK_SDK_PATH%\bin\glslc.exe %flags% swapChain.frag -o %1\swapChain.frag.spv

%VK_SDK_PATH%\bin\glslc.exe %flags% imgui.vert -o %1\imgui.vert.spv
%VK_SDK_PATH%\bin\glslc.exe %flags% imgui.frag -o %1\imgui.frag.spv

%VK_SDK_PATH%\bin\glslc.exe %flags% rayTrace.rgen -o %1\rayTrace.rgen.spv
%VK_SDK_PATH%\bin\glslc.exe %flags% rayTrace.rmiss -o %1\rayTrace.rmiss.spv
