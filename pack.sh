pushd build
mkdir -p out
cmake --build build --target Raytracer --config Release
cp ./Release/Raytracer.exe out/Raytracer.exe
cp ./Release/SDL2.dll out/SDL2.dll
cp -r ./shaders ./out/shaders
popd