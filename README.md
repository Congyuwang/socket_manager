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

- Step 3: Pull the source code

```shell
git clone https://github.com/Congyuwang/socket_manager.git
cd socket_manager
```

- Step 4: Build and install

```shell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
sudo cmake --install build --config Release
```

## Usage

In your CMakeLists.txt, add the following lines:

```cmake
find_package(socket_manager 0.1.0 REQUIRED)
target_link_libraries(test_socket_manager PUBLIC socket_manager)
```
