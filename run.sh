#!/usr/bin/env bash

build_system="DEFAULT"
compiler="DEFAULT"

if [ $# -ge 1 ]; then
    build_system="$1"
    shift
fi

if [ $# -ge 1 ]; then
    compiler="$1"
    shift
fi

if [ "${build_system}" = "Xcode" ]; then
    echo "Xcode is currently not supported by this run script"
    exit 1
fi

echo "Running binary from ${build_system} build system using ${compiler} compiler"
./bin/"${build_system}"_"${compiler}"/roast "$@"