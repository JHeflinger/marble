if [ -d "build/cache" ]; then
    rm -rf build/cache
fi
if [ -d "build/vendor" ]; then
    rm -rf build/vendor
fi
if [ -f "build/bin.exe" ]; then
    rm build/bin.exe
fi
if [ -d "build/expanded" ]; then
    rm -rf build/expanded
fi
if [ -d "penv" ]; then
    rm -rf penv
fi
