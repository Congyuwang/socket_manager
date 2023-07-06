# A C++ Library Developed In Rust Tokio To Manage Multiple TCP Connections

Easily manage multiple socket connections asynchronously in C++. 

## Installation

- Step 1: Install Rust

```shell
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

- Step 2: Install nightly toolchain

```shell
rustup toolchain install nightly
rustup default nightly
```

- Step 3: Install LLVM 16

macOS:
```shell
brew install llvm@16
```

linux
```shell
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 16 all
sudo ./update-alternatives-clang.sh 16 9999
```

- Step 4: Pull the source code

```shell
git clone https://github.com/Congyuwang/socket_manager.git
cd socket_manager
git submodule update --init
```

- Step 5: Build and install

```shell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
sudo cmake --install build --config Release
```

## Usage

In your CMakeLists.txt, add the following lines:

```cmake
# enable lto requires clang-16
SET(CMAKE_C_FLAGS "-Wall -std=c99")
SET(CMAKE_C_FLAGS_DEBUG "-g")
SET(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG -flto=full")
SET(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -flto=full")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

SET(CMAKE_CXX_FLAGS "-Wall")
SET(CMAKE_CXX_FLAGS_DEBUG "-g")
SET(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG -flto=full")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto=full")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

find_package(socket_manager 0.1.0 REQUIRED)
target_link_libraries(test_socket_manager PUBLIC socket_manager)
```
