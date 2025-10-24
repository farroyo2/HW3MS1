#!/bin/bash

# Test script for assignment_code directory
# This script tests the csim implementation in the assignment_code/ subdirectory

# Get the directory where this script is located
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
parent_dir="$(dirname "$script_dir")"
csim_exec="$script_dir/assignment_code/csf_assign03/csim"

# Check if we need to build first
if [[ ! -x "$csim_exec" ]]; then
    echo "csim executable not found in assignment_code/. Attempting to build..."
    
    # Check if Makefile exists and try to build
    if [[ -f "$script_dir/assignment_code/Makefile" ]]; then
        echo "Found Makefile, running 'make' in assignment_code directory..."
        cd "$script_dir/assignment_code"
        make
        cd "$script_dir"
        
        # Check again if csim was built
        if [[ ! -x "$csim_exec" ]]; then
            echo "Error: Failed to build csim in assignment_code directory"
            exit 1
        fi
    else
        echo "Error: No csim executable or Makefile found in assignment_code/"
        echo "Please build your csim first or check the assignment_code directory"
        exit 1
    fi
fi

# Check arguments
if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 [LRU|FIFO] [cycles]"
    echo "  - First argument: Test only LRU or FIFO implementation"
    echo "  - Second argument (optional): If 'cycles' is provided, allow cycle counts to vary by 5%"
    exit 1
fi

# Set replacement policy to test
replacement_policy=$(echo "$1" | tr '[:upper:]' '[:lower:]')
if [[ "$replacement_policy" != "lru" && "$replacement_policy" != "fifo" ]]; then
    echo "Error: First argument must be either LRU or FIFO"
    exit 1
fi

# Enable cycle variation checking if "cycles" is the second argument
check_cycles=false
if [[ "$2" == "cycles" ]]; then
    check_cycles=true
fi

echo "Testing assignment_code csim with $1 replacement policy..."

# Define your specific test cases
declare -A commands
declare -A expected_outputs
declare -A command_labels

configs=(
    "1 1 4 write-allocate write-through write01.trace"
    "1 1024 128 no-write-allocate write-through write02.trace"
    "8192 1 16 write-allocate write-back read01.trace"
    "2048 4 16 no-write-allocate write-through gcc.trace"
    "256 4 128 write-allocate write-through read02.trace"
)

# Tracking test results
total_tests=0
passed_tests=0
failed_tests=0

# Generate test cases
cmd_index=1
for config in "${configs[@]}"; do
    read -ra config_parts <<< "$config"
    
    set_size="${config_parts[0]}"
    assoc="${config_parts[1]}"
    block_size="${config_parts[2]}"
    write_alloc="${config_parts[3]}"
    write_policy="${config_parts[4]}"
    trace_file="${config_parts[5]}"
    
    # Extract trace name without .trace extension
    trace_name="${trace_file%.trace}"
    trace_path="$script_dir/traces/$trace_file"
    
    if [[ ! -f "$trace_path" ]]; then
        echo "ERROR: Missing trace file: $trace_path"
        continue
    fi

    expected_dir="$script_dir/expected_results/$trace_name"
    
    if [[ ! -d "$expected_dir" ]]; then
        echo "ERROR: Expected results directory does not exist: $expected_dir"
        continue
    fi

    write_alloc_flag="wa"
    [[ "$write_alloc" == "no-write-allocate" ]] && write_alloc_flag="nwa"

    write_policy_flag="wt"
    [[ "$write_policy" == "write-back" ]] && write_policy_flag="wb"

    expected_filename="${set_size}_${assoc}_${block_size}_${write_alloc_flag}_${write_policy_flag}_${replacement_policy}.txt"
    expected_output="$expected_dir/$expected_filename"

    if [[ ! -f "$expected_output" ]]; then
        echo "ERROR: Expected result file missing: $expected_output"
        echo "Run ./generate_expected_results_simple.sh first to create expected results"
        continue
    fi

    formatted_command="assignment_code/csim $set_size $assoc $block_size $write_alloc $write_policy $replacement_policy < $trace_file"
    commands["cmd$cmd_index"]="$csim_exec $set_size $assoc $block_size $write_alloc $write_policy $replacement_policy < $trace_path"
    expected_outputs["cmd$cmd_index"]="$expected_output"
    command_labels["cmd$cmd_index"]="$formatted_command"
    ((cmd_index++))
done

# Function to compare output with expected result
compare_output() {
    local command="$1"
    local expected_output="$2"
    local formatted_command="$3"

    echo "$formatted_command"
    echo "-----------------------------------------------------------------------"

    output_file=$(mktemp)
    eval "$command" > "$output_file"

    ((total_tests++))

    if [[ ! -s "$output_file" ]]; then
        echo "ERROR: csim produced empty output"
        ((failed_tests++))
        rm -f "$output_file"
        return
    fi

    # Extract cycle information
    expected_cycles=$(grep -oP '(?<=Total cycles: )\d+' "$expected_output")
    actual_cycles=$(grep -oP '(?<=Total cycles: )\d+' "$output_file")

    # Decide how to handle cycle comparison based on command-line argument
    if $check_cycles && [[ -n "$expected_cycles" && -n "$actual_cycles" ]]; then
        # Calculate tolerance range
        min_cycles=$(awk "BEGIN {print $expected_cycles * 0.95}")
        max_cycles=$(awk "BEGIN {print $expected_cycles * 1.05}")

        # Check if actual cycles are within 5% of expected
        if (( $(awk "BEGIN {print ($actual_cycles >= $min_cycles && $actual_cycles <= $max_cycles)}") )); then
            echo "Test Passed (Cycles within 5%)"
            ((passed_tests++))
            rm -f "$output_file"
            return
        else
            echo "Test Failed (Cycles out of range)"
            echo "Expected: $expected_cycles 5% ($min_cycles - $max_cycles)"
            echo "Actual: $actual_cycles"
            ((failed_tests++))
            rm -f "$output_file"
            return
        fi
    fi

    # Compare the rest of the output line by line
    head -n -1 "$expected_output" > expected_trimmed.txt
    head -n -1 "$output_file" > output_trimmed.txt
    diff -w expected_trimmed.txt output_trimmed.txt > /dev/null

    if [[ $? -eq 0 ]]; then
        echo "Test Passed"
        ((passed_tests++))
    else
        echo "Test Failed"
        echo "Differences:"
        diff -w expected_trimmed.txt output_trimmed.txt | head -20
        ((failed_tests++))
    fi

    rm -f "$output_file" expected_trimmed.txt output_trimmed.txt
}

# Run tests
for cmd_key in "${!commands[@]}"; do
    compare_output "${commands[$cmd_key]}" "${expected_outputs[$cmd_key]}" "${command_labels[$cmd_key]}"
done

# Print final test summary
echo "=========================================="
echo "Total Tests Run: $total_tests"
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"

if [[ $failed_tests -eq 0 ]]; then
    echo "All tests passed successfully!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi