@echo off

pushd shaders
rem glslangValidator.exe --target-env vulkan1.0 empty.frag -V --vn empty_frag -o empty_frag.h
rem glslangValidator --target-env vulkan1.0 empty.frag -V -o empty.frag.spv
rem spirv-opt empty.frag.spv -Os -o empty.frag.spv
rem spirv-dis empty.frag.spv -o empty.frag.spvasm

rem glslangValidator.exe --target-env vulkan1.0 text.vert -V --vn text_vert -o text_vert.h
rem glslangValidator.exe --target-env vulkan1.0 text.frag -V --vn text_frag -o text_frag.h

rem glslangValidator.exe --target-env vulkan1.0 image2d.vert -V --vn image2d_vert -o image2d_vert.h
rem glslangValidator --target-env vulkan1.0 image2d.vert -V -o image2d.vert.spv
rem spirv-opt image2d.vert.spv -Os -o image2d.vert.spv
rem spirv-dis image2d.vert.spv -o image2d.vert.spvasm

rem glslangValidator.exe --target-env vulkan1.0 image2d.frag -V --vn image2d_frag -o image2d_frag.h
rem glslangValidator.exe --target-env vulkan1.0 triangle.vert -V --vn triangle_vert -o triangle_vert.h
rem glslangValidator.exe --target-env vulkan1.0 triangle.frag -V --vn triangle_frag -o triangle_frag.h

rem glslangValidator.exe --target-env vulkan1.0 path2d.vert -V --vn path2d_vert -o path2d_vert.h
rem glslangValidator --target-env vulkan1.0 path2d.vert -V -o path2d.vert.spv
rem spirv-opt path2d.vert.spv -Os -o path2d.vert.spv
rem spirv-dis path2d.vert.spv -o path2d.vert.spvasm

rem glslangValidator.exe --target-env vulkan1.0 path2d.frag -V --vn path2d_frag -o path2d_frag.h
rem glslangValidator --target-env vulkan1.0 path2d.frag -V -o path2d.frag.spv
rem spirv-opt path2d.frag.spv -Os -o path2d.frag.spv
rem spirv-dis path2d.frag.spv -o path2d.frag.spvasm

rem glslangValidator.exe --target-env vulkan1.0 skybox.vert -V --vn skybox_vert -o skybox_vert.h
rem glslangValidator --target-env vulkan1.0 skybox.vert -V -o skybox.vert.spv
rem spirv-opt skybox.vert.spv -Os -o skybox.vert.spv
rem spirv-dis skybox.vert.spv -o skybox.vert.spvasm

rem glslangValidator.exe --target-env vulkan1.0 skybox.frag -V --vn skybox_frag -o skybox_frag.h

rem compressonatorcli -fd bc4 -EncodeWith GPU -nomipmap -Quality 1.0 assets/gear.png gear.ktx2
rem compressonatorcli -fd bc4 -EncodeWith GPU -nomipmap -Quality 1.0 assets/discord.png discord.ktx2
rem compressonatorcli -fd bc4 -EncodeWith GPU -nomipmap -Quality 1.0 assets/anticlockwise-rotation.png anticlockwise-rotation.ktx2
rem compressonatorcli -fd bc7 -EncodeWith GPU -Quality 1.0 assets/Planks033B_1K_Color.png Planks033B_1K_Color.ktx2

for %%F in (*.spv) do (
	if /I "%%~xF" == ".spv" (
		del "%%F"
	)
)

spirv-as shaders.spvasm --target-env vulkan1.0 -o shaders.spv
spirv-val shaders.spv

popd

set COMMON=-fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-threadsafe-statics -fno-rtti -fno-exceptions -nostdlib -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe -w -Ilibs -Ishaders
set WASM=--target=wasm32 -Wl,--no-entry,--export-table,--export-dynamic,--strip-all,--allow-undefined,--compress-relocations
set WIN32=-fuse-ld=lld -Xlinker -stack:0x100000,0x100000 -Xlinker -subsystem:windows -Xlinker -fixed

rem clang src/assets.c -g -o assets.exe %COMMON% %WIN32%

rem assets.exe

rem del *.ktx2



