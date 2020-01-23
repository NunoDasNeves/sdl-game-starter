#/bin/bash
rm -rf build
mkdir -p build
cd build

EXECUTABLE_NAME=sdl_game_starter
SO_NAME=game_starter.so

GAME_SOURCES="../src/game_starter.cpp"
GAME_OBJS="game_starter.o"
PLATFORM_SOURCES="../src/sdl_game_starter.cpp"
PLATFORM_OBJS="sdl_game_starter.o"

OTHER_FLAGS="-DSTDOUT_DEBUG -DFIXED_GAME_MEMORY"
COMMON_COMPILER_FLAGS="-c -Wall"
PLATFORM_COMPILER_FLAGS="${COMMON_COMPILER_FLAGS}"
GAME_COMPILER_FLAGS="${COMMON_COMPILER_FLAGS} -fPIC"

COMMON_LINKER_FLAGS=""
PLATFORM_LINKER_FLAGS="${COMMON_LINKER_FLAGS} -lSDL2"
GAME_LINKER_FLAGS="${COMMON_LINKER_FLAGS} -shared"

echo "compiling platform"
for src in ${PLATFORM_SOURCES}; do
    echo "  $src"
    g++ ${src} ${PLATFORM_COMPILER_FLAGS} ${OTHER_FLAGS} || exit 1
done

echo "compiling game"
for src in ${GAME_SOURCES}; do
    echo "  $src"
    g++ ${src} ${GAME_COMPILER_FLAGS} ${OTHER_FLAGS} || exit 1
done

echo "linking"
g++ ${PLATFORM_OBJS} ${PLATFORM_LINKER_FLAGS} -o ${EXECUTABLE_NAME}
g++ ${GAME_OBJS} ${GAME_LINKER_FLAGS} -o ${SO_NAME}
echo "done"


cd ..
mv build/${EXECUTABLE_NAME} .
mv build/${SO_NAME} .
