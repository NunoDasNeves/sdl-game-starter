# Linux + Windows sdl layer for game development

This isn't a library, it is a starting point for game projects; this layer is meant to evolve to suit the requirements of the game.

Based loosely on videos from [Handmade Hero](https://handmadehero.org/) and the Linux version, [Handmade Penguin](https://davidgow.net/handmadepenguin/).

## Build

### Windows

Install Visual Studio build tools
Install SDL 2.0.10 development libraries to C:\SDL2-2.0.10
Copy SDL2.dll to repo (it has to be in the same directory as the executable)

Run vcvarsall.bat to get compiler in the PATH.

Run build.bat.

### Linux

Install g++ and SDL2 with your package manager.

Run build.sh.

## Features
- Windows build script
- Linux build script
- SDL initialization and window creation
- Software rendering in dummy game loop
- Sound initialization and debug sine wave
- Basic frame rate enforcement pattern
- Basic input capture from keyboard and controller
- Dummy game state
- Debug IO for loading/saving files
- Compile game code as a dynamically loaded library for iterating at runtime

## Roadmap
- See TODO.txt

## Possible
- Text input support as per: https://wiki.libsdl.org/Tutorials/TextInput
- Mac support
- 32 bit support
