./build.sh
if [ $? -ne 0 ]; then
    exit 1
fi
cp -r build/shaders/* penv/build/shaders/
cd penv
LD_LIBRARY_PATH=../vendor/steamaudio/lib/linux-x64 ./../build/bin.exe
cd ..
