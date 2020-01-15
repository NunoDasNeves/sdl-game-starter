@echo off


set SDL_DIR=C:\SDL2-2.0.10

:: Debug messages etc
set ADDITIONAL_FLAGS=/DCONSOLE_DEBUG

:: Disabled warnings
:: /wd4100  unreferenced formal parameter
::
set DISABLED_WARNINGS=/wd4100

:: Compiler flags
:: /Oi		compiler intrinsics
:: /GR- 	no runtime type info
:: /EHa- 	turn off exception handling
:: /nologo 	turn off compiler banner thing
:: /W4		warning level 4
:: /MT		statically link C runtime into executable for portability
:: /Gm-		Turn off incremental build
:: /Z7		Old style debugging info
:: /Fm		Generate map file
:: /I       Additional include directory (SDL headers)
set COMPILER_FLAGS=/Oi /GR- /EHa- /nologo /W4 /MT /Gm- /Z7 /Fm /I %SDL_DIR%\include

:: Linker flags
:: /opt:ref     remove unneeded stuff from .map file
:: /LIBPATH:    sdl library path, libraries to include, and additional arguments (enable console subsystem for debugging)
set LINKER_FLAGS=/LIBPATH:%SDL_DIR%\lib\x64 SDL2.lib SDL2main.lib /SUBSYSTEM:CONSOLE

:: Create build directory and copy SDL2.dll in case it isn't there
IF NOT EXIST build mkdir build
cd build
IF NOT EXIST SDL2.dll copy %SDL_DIR%\lib\x64\SDL2.dll .

:: Build the thing!
cl ..\src\sdl-game-starter.cpp %COMPILER_FLAGS% %DISABLED_WARNINGS% %ADDITIONAL_FLAGS% /link %LINKER_FLAGS%

cd ..