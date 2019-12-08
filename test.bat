@echo off
mkdir build
cd build
cmake ..
msbuild my_gameboy_all.sln
cd ..
.\build\test\Debug\my_gameboy_test.exe