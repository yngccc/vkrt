@echo off

set curDir=%cd%
set flags=--target-env vulkan1.2 -g

pushd %~dp0
for %%A in (*.vert) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %curDir%\%%A.spv
for %%A in (*.frag) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %curDir%\%%A.spv
for %%A in (*.rgen) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %curDir%\%%A.spv
for %%A in (*.rmiss) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %curDir%\%%A.spv
for %%A in (*.rchit) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %curDir%\%%A.spv
for %%A in (*.rahit) do %VK_SDK_PATH%\bin\glslangValidator.exe %flags% %%A -o %curDir%\%%A.spv
popd
