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

for stage_dir in "./tests/"{"scan","parse","inter","assembly"}"/"; do
    stage_name=$(basename "$stage_dir")
    echo "Testing stage: $stage_name"

    for expected_result_dir in "${stage_dir}"{"fail","succeed"}"/"; do
        expected_result=$(basename "$expected_result_dir")
        echo "   Running tests that are expected to $expected_result:"

        for test_file in "$expected_result_dir"*; do
            result_message="didn't pass!"

            if [ "$stage_name" != "assembly" ]; then
                ./run_extended.sh "-b" "$build_system" "-c" "$compiler" "--" "$test_file" -t "$stage_name" > /dev/null 2>&1
            elif [ "$stage_name" = "assembly" ]; then
                assemblies_dir=./bin/"${build_system}"_"${compiler}"/assemblies
                mkdir -p "${assemblies_dir}"
                assembly_file="${assemblies_dir}"/"$(basename "${test_file%????}")".asm
                ./bin/"${build_system}"_"${compiler}"/roast "$test_file" -t "$stage_name" -o "${assembly_file}" 2>/dev/null

                executables_dir=./bin/"${build_system}"_"${compiler}"/executables
                mkdir -p "${executables_dir}"
                exectuable_file="${executables_dir}"/"$(basename "${test_file%????}")"
                /opt/homebrew/bin/gcc-13 -O0 -no-pie "${assembly_file}" -o "${exectuable_file}"

                chmod +x "${exectuable_file}"

                if [ "${expected_result}" = "fail" ]; then
                    "${exectuable_file}"
                elif [ "${expected_result}" = "succeed" ]; then
                    executable_outputs_dir=./bin/"${build_system}"_"${compiler}"/executable_outputs
                    mkdir -p "${executable_outputs_dir}"
                    executable_output_file="${executable_outputs_dir}"/"$(basename "${test_file%}")".out

                    "${exectuable_file}" > "${executable_output_file}"

                    succeed_output_file=./tests/assembly/succeed_outputs/"$(basename "${test_file%}")".out
                    if diff "${executable_output_file}" "${succeed_output_file}" > /dev/null; then
                        (exit 0)
                    else
                        (exit 1)
                    fi
                else
                    echo "This line shouldn't be running"
                    exit 1
                fi
            else
                echo "This line shouldn't be running"
                exit 1
            fi

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