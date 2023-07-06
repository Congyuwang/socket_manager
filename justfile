export PATH := `echo $HOMEBREW_PREFIX/opt/llvm/bin:$PATH`
export CC := `echo $HOMEBREW_PREFIX/opt/llvm/bin/clang`
export CXX := `echo $HOMEBREW_PREFIX/opt/llvm/bin/clang++`

cbind:
    cbindgen -q --config cbindgen.toml --crate tokio-socket-manager --output include/socket_manager_c_api.h

clean:
    rm -rf build

test:
    cd build && SOCKET_LOG=debug ctest --output-on-failure && cd ..

debug:
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --config Debug
    just test

build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    just test

install:
    just clean
    just build
    sudo cmake --install build --config Release
