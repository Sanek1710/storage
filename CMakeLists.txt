cmake_minimum_required(VERSION 3.11.0)
project(supertrie VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG        407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) # 12.1.0
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        486b55554f11c9cccc913e11a87085b2a91f706f) # v1.16.0
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  unordered_dense
  GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
  GIT_TAG        3234af2c03549bc85656bfd3a86993bf1cd8aef1) # v4.8.1
FetchContent_MakeAvailable(unordered_dense)

add_executable(supertrie main.cpp)
target_link_libraries(supertrie fmt::fmt spdlog::spdlog unordered_dense)

add_executable(depgraph depgraph.cpp)
target_link_libraries(depgraph fmt::fmt spdlog::spdlog unordered_dense)
