#/bin/bash
rm -rf build
mkdir -p build
cd build

NAME=sdl-game-starter
SOURCES=$(ls ../src/*.cpp)
OBJS=$(find ../src/ -name *.cpp -printf '%f\n' | sed s/cpp/o/g)

echo compiling
for src in ${SOURCES}; do
    echo "  $src"
    g++ ${src} -c -Wall || exit 1
done
echo linking
g++ ${OBJS} -lSDL2 -o ${NAME} || exit 1
echo done


cd ..
mv build/${NAME} .
