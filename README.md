# Linux + Windows cross platform starting point for game development

This isn't a library, it is a starting point for game projects.

It's meant to be a personal project, though it may be educational to read.
Based (in part) on code from https://davidgow.net/handmadepenguin/, https://handmadehero.org/, http://lazyfoo.net/tutorials/SDL/index.php

## Build

### Windows

Install Visual Studio build tools
Install SDL 2.0.10 development libraries to C:\SDL2-2.0.10
Copy SDL2.dll to repo (it has to be in the same directory as the executable)

Run vcvarsall.bat to get cl in the PATH.

Run build.bat.

### Linux

Install g++ and SDL2

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
- May add Mac support
- May add 32 bit support
