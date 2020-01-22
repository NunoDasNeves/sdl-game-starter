@echo off

set EXE_NAME=sdl_game_starter.exe
set DLL_NAME=game_starter.dll

set SDL_DIR=C:\SDL2-2.0.10

:: Debug messages etc
set ADDITIONAL_FLAGS=/DSTDOUT_DEBUG /DFIXED_GAME_MEMORY


:: Disabled warnings
:: /wd4100  unreferenced formal parameter
:: /wd4189  local variable is initialized but not referenced
set DISABLED_WARNINGS=/wd4100 /wd4189

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
:: not set
:: /P       Preprocessor output to file
set COMMON_COMPILER_FLAGS=/Oi /GR- /EHa- /nologo /W4 /MT /Gm- /Z7 /Fm %DISABLED_WARNINGS%
set PLATFORM_COMPILER_FLAGS=%COMMON_COMPILER_FLAGS% /I %SDL_DIR%\include
set GAME_COMPILER_FLAGS=%COMMON_COMPILER_FLAGS% /LD

:: Linker flags
:: /opt:ref         remove unneeded stuff from .map file
:: /LIBPATH:        sdl library path, libraries to include, and additional arguments (enable console subsystem for debugging)
:: /INCREMENTAL:NO  perform a full link
set COMMON_LINKER_FLAGS=/INCREMENTAL:NO
set PLATFORM_LINKER_FLAGS=%COMMON_LINKER_FLAGS% /LIBPATH:%SDL_DIR%\lib\x64 SDL2.lib SDL2main.lib /SUBSYSTEM:CONSOLE
set GAME_LINKER_FLAGS=%COMMON_LINKER_FLAGS% /DLL /EXPORT:game_init_memory /EXPORT:game_update_and_render

:: Create build directory and copy SDL2.dll in case it isn't there
IF NOT EXIST build mkdir build
cd build
IF NOT EXIST SDL2.dll copy %SDL_DIR%\lib\x64\SDL2.dll .

IF EXIST %EXE_NAME% del %EXE_NAME%
IF EXIST %DLL_NAME% del %DLL_NAME%

:: Build platform executable
cl ..\src\sdl_game_starter.cpp %PLATFORM_COMPILER_FLAGS% %ADDITIONAL_FLAGS% /link %PLATFORM_LINKER_FLAGS%
:: Build game code dll
cl ..\src\game_starter.cpp %GAME_COMPILER_FLAGS% %ADDITIONAL_FLAGS% /link %GAME_LINKER_FLAGS%

cd ..