@echo off

set outDir=%1
if "%outDir%" == "" set outDir=%cd%

set flags=--target-env vulkan1.2 -g
set slang=..\..\thirdParty\bin\slang\slangc.exe

pushd %~dp0

echo Compiling Slang shaders
%slang% swapChain.slang -target spirv -entry vertexShader -o %outDir%\swapChain.vert.spv
%slang% swapChain.slang -target spirv -entry fragmentShader -o %outDir%\swapChain.frag.spv
%slang% imgui.slang -target spirv -entry vertexShader -o %outDir%\imgui.vert.spv
%slang% imgui.slang -target spirv -entry fragmentShader -o %outDir%\imgui.frag.spv
%slang% rayTrace.slang -target spirv -entry rayGenShader -o %outDir%\rayTrace.rgen.spv
%slang% rayTrace.slang -target spirv -entry missShader -o %outDir%\rayTrace.rmiss.spv
%slang% rayTrace.slang -target spirv -entry anyhitShader -o %outDir%\rayTrace.rahit.spv
%slang% rayTrace.slang -target spirv -entry closestHitShader -o %outDir%\rayTrace.rchit.spv

popd
