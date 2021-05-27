@echo off

set outDir=%1
if "%outDir%" == "" set outDir=%cd%

set flags=--target-env vulkan1.2 -g
set slang=..\..\thirdParty\slang\slangc.exe

pushd %~dp0

echo Compiling Slang shaders
%slang% swapChain.slang -target spirv -entry vertexShader -o %outDir%\swapChain_vert.spv
%slang% swapChain.slang -target spirv -entry fragmentShader -o %outDir%\swapChain_frag.spv
%slang% imgui.slang -target spirv -entry vertexShader -o %outDir%\imgui_vert.spv
%slang% imgui.slang -target spirv -entry fragmentShader -o %outDir%\imgui_frag.spv
%slang% pathTrace.slang -target spirv -entry rayGenShader -o %outDir%\pathTrace_rgen.spv
%slang% pathTrace.slang -target spirv -entry missShader -o %outDir%\pathTrace_rmiss.spv
%slang% pathTrace.slang -target spirv -entry anyhitShader -o %outDir%\pathTrace_rahit.spv
%slang% pathTrace.slang -target spirv -entry primaryRayClosestHitShader -o %outDir%\pathTrace_primary_rchit.spv
%slang% pathTrace.slang -target spirv -entry closestHitShader -o %outDir%\pathTrace_rchit.spv

popd
