cbind:
    cbindgen -q --config cbindgen.toml --crate tokio-socket-manager --output include/socket_manager_c_api.h

clean:
    rm -rf build

dev-docker:
    docker build -f ./dockerfile/dev-containers/focal/Dockerfile -t socket-manager-dev:focal .
    docker build -f ./dockerfile/dev-containers/jammy/Dockerfile -t socket-manager-dev:jammy .

test:
    cd build && SOCKET_LOG=debug ctest --output-on-failure && cd ..

test-linking:
    cd tests/test_find_package
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --verbose
    cd ../..

debug:
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --config Debug
    just test

build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --verbose
    just test

build-static:
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
    cmake --build build --config Release --verbose
    just test

install:
    just clean
    just build
    sudo cmake --install build --config Release

install-static:
    just clean
    just build-static
    sudo cmake --install build --config Release
