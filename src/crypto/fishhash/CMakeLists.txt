cmake_minimum_required(VERSION 3.9)
project(LibFishHash CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0)

add_library(
    FishHash
    STATIC
    FishHash.cpp
    blake3/blake3.c
    blake3/blake3_portable.c
    blake3/blake3_dispatch.c
    blake3/keccak.c
)

install(TARGETS FishHash DESTINATION .)
