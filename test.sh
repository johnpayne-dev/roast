#!/usr/bin/env bash

stage="all"
build_system="DEFAULT"
compiler="DEFAULT"

if [ $# -ge 1 ]; then
    stage="$1"
    shift
fi

if [ $# -ge 1 ]; then
    build_system="$1"
    shift
fi

if [ $# -ge 1 ]; then
    compiler="$1"
    shift
fi

./build.sh "$build_system" "$compiler"

if [ $? -ne 0 ]; then
    exit 1
fi

for stage_dir in "./tests/"{"scan","parse","inter"}"/"; do
    stage_name=$(basename "$stage_dir")
    echo "Testing stage: $stage_name"

    for expected_result_dir in "${stage_dir}"{"fail","succeed"}"/"; do
        expected_result=$(basename "$expected_result_dir")
        echo "   Running tests that are expected to $expected_result:"

        for test_file in "$expected_result_dir"*; do
            result_message="didn't pass!"
            ./run.sh "$build_system" "$compiler" "$test_file" -t "$stage_name" > /dev/null 2>&1
            actual_result=$?

            if [ "$expected_result" = "fail" ] && [ "${actual_result}" -ne 0 ]; then
                result_message="passed!"
            elif [ "$expected_result" = "fail" ] && [ "${actual_result}" -eq 0 ]; then
                result_message="didn't pass!"
            elif [ "$expected_result" = "succeed" ] && [ "${actual_result}" -eq 0 ]; then
                result_message="passed!"
            elif [ "$expected_result" = "succeed" ] && [ "${actual_result}" -ne 0 ]; then
                result_message="didn't pass!"
            else
                echo "This line shouldn't be running"
                exit 1
            fi

            echo "     Running test: $test_file - $result_message"
        done
    done

    if [ "${stage_name}" = "${stage}" ]; then
        break
    fi
done