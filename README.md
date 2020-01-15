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
- SDL initialization and window creation

## Roadmap
- Linux build script
- Software rendering in dummy game loop
- Unified input map
- Sound initialization and debugging
- Debugging stuff
- Basic frame rate enforcement pattern
- Performance counter helper functions
- Dummy game state
- File IO stuff
- ...other basic stuff I may have forgotten

## Future
- Compile game code as a dynamically loaded library for iterating at runtime
- Fullscreen and multiple monitor support
- May add 32 bit support
- May add Mac support
