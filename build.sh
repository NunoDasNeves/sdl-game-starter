#/bin/bash
rm -rf build
mkdir -p build
cd build

NAME=sdl-game-starter
SOURCES=$(ls ../src/*.cpp)
OBJS=$(find ../src/ -name *.cpp -printf '%f\n' | sed s/cpp/o/g)

OTHER_FLAGS="-DSTDOUT_DEBUG -DFIXED_GAME_MEMORY"
COMPILER_FLAGS="-c -Wall"
LINKER_FLAGS=""

echo compiling
for src in ${SOURCES}; do
    echo "  $src"
    g++ ${src} ${COMPILER_FLAGS} ${OTHER_FLAGS} || exit 1
done
echo linking
g++ ${OBJS} -lSDL2 -o ${NAME} || exit 1
echo done


cd ..
mv build/${NAME} .
