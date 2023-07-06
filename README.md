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
export PATH=/opt/homebrew/opt/llvm/bin:${PATH}
export CC=/opt/homebrew/opt/llvm/bin/clang
export CXX=/opt/homebrew/opt/llvm/bin/clang++
```

linux
```shell
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 16

chmod +x update-alternatives-clang.sh
sudo ./update-alternatives-clang.sh 16 9999
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
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
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

find_package(socket_manager 0.1.0 REQUIRED)
target_link_libraries(test_socket_manager PUBLIC socket_manager)
```
