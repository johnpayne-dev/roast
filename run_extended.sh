#!/usr/bin/env bash

build_system="DEFAULT"
compiler="DEFAULT"

usage() {
    echo "Usage: $0 [-b <build_system>] [-c <compiler>] [--] [roast_args...]"
    echo "Options:"
    echo "  -b <build_system>   Build system that built executable to run"
    echo "  -c <compiler>       Compiler that built executable to run"
    exit 1
}

while getopts ":b:c:" opt; do
    case ${opt} in
        b )
            build_system=$OPTARG
            ;;
        c )
            compiler=$OPTARG
            ;;
        \? )
            echo "Invalid option: $OPTARG" 1>&2
            usage
            ;;
        : )
            echo "Option -$OPTARG requires an argument." 1>&2
            usage
            ;;
    esac
done
shift $((OPTIND -1))

echo "Running binary from ${build_system} build system using ${compiler} compiler"
./bin/"${build_system}"_"${compiler}"/roast "$@"