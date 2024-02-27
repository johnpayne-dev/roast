#!/usr/bin/env bash

build_system="DEFAULT"
compiler=""
compiler_path=""

if [ $# -ge 1 ]; then
    build_system="$1"
fi

if [ $# -ge 2 ]; then
    compiler="$2"
    if [ "${compiler}" = "gcc" ]; then
        compiler_path="/opt/homebrew/bin/gcc-13"
    else
        echo "${compiler} is not a valid compiler option"
        exit 1
    fi
fi

if [ "${compiler_path}" = "" ]; then
    echo "CMake'ing to ${build_system} build system using DEFAULT compiler"
    if [ "${build_system}" = "DEFAULT" ]; then
        cmake -B "./build/${build_system}_DEFAULT" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH="../../bin/${build_system}_DEFAULT"
    else
        cmake -B "./build/${build_system}_DEFAULT" -G "${build_system}" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH="../../bin/${build_system}_DEFAULT"
    fi
    cmake --build "./build/${build_system}_DEFAULT"
else
    echo "CMake'ing to ${build_system} build system using ${compiler} compiler"
    if [ "${build_system}" = "DEFAULT" ]; then
        cmake -B "./build/${build_system}_${compiler}" -DCMAKE_C_COMPILER="${compiler_path}" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH="../../bin/${build_system}_${compiler}"
    else
        cmake -B "./build/${build_system}_${compiler}" -G "${build_system}" -DCMAKE_C_COMPILER="${compiler_path}" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH="../../bin/${build_system}_${compiler}"
    fi
    cmake --build "./build/${build_system}_${compiler}"
fi