@echo off

set outDir=%1
if "%outDir%" == "" set outDir=%cd%

set flags=--target-env vulkan1.2 -g
set slang=..\..\thirdParty\bin\slang\slangc.exe

pushd %~dp0

rem for %%A in (*.vert) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %outDir%\%%A.spv
rem for %%A in (*.frag) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %outDir%\%%A.spv
rem for %%A in (*.rgen) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %outDir%\%%A.spv
rem for %%A in (*.rmiss) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %outDir%\%%A.spv
rem for %%A in (*.rchit) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %outDir%\%%A.spv
rem for %%A in (*.rahit) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %outDir%\%%A.spv

echo Compiling Slang shaders
%slang% swapChain.slang -target spirv -entry vertexShader -o %outDir%\swapChain.vert.spv
%slang% swapChain.slang -target spirv -entry fragmentShader -o %outDir%\swapChain.frag.spv
%slang% imgui.slang -target spirv -entry vertexShader -o %outDir%\imgui.vert.spv
%slang% imgui.slang -target spirv -entry fragmentShader -o %outDir%\imgui.frag.spv
%slang% rayTrace.slang -target spirv -entry rayGenShader -o %outDir%\rayTrace.rgen.spv
%slang% rayTrace.slang -target spirv -entry missShader -o %outDir%\rayTrace.rmiss.spv
%slang% rayTrace.slang -target spirv -entry anyhitShader -o %outDir%\rayTrace.rahit.spv
%slang% rayTrace.slang -target spirv -entry closestHitShader -o %outDir%\rayTrace.rchit.spv

%slang% rayTrace.slang -target glsl -entry rayGenShader -o %outDir%\rayTrace.rgen.glsl
%slang% rayTrace.slang -target glsl -entry missShader -o %outDir%\rayTrace.rmiss.glsl
%slang% rayTrace.slang -target glsl -entry anyhitShader -o %outDir%\rayTrace.rahit.glsl
%slang% rayTrace.slang -target glsl -entry closestHitShader -o %outDir%\rayTrace.rchit.glsl

popd
