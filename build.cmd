@echo off

set COMMON=-fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-threadsafe-statics -fno-rtti -fno-exceptions -nostdlib -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe -w -Ilibs -Ishaders
set WASM=--target=wasm32 -Wl,--no-entry,--export-table,--export-dynamic,--strip-all,--allow-undefined,--compress-relocations
set WIN32=-fuse-ld=lld -Xlinker -stack:0x100000,0x100000 -Xlinker -subsystem:windows -Xlinker -fixed

rem clang src/client_wasm.c -Ofast -o static/client.wasm %COMMON% %WASM%
clang -std=c2x src/client.c -Ofast -o build/client_speed.exe %COMMON% %WIN32%
clang -std=c2x src/client.c -Oz -o build/client_size.exe %COMMON% %WIN32%
clang -std=c2x src/client.c -g -o build/client_debug.exe %COMMON% %WIN32%

rem del assets.pdb
rem del attributes
rem del icons
rem del indices
rem del textures
rem del vertices
