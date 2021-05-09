@echo off

if not exist build (
	::rd /s /q build
	echo create build folder...
	mkdir build
)

echo create sln...
cd build
cmake ..
echo build project...
cmake --build .
pause