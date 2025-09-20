#!/bin/bash

# --- Configuration ---
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color
SCORE=0
TOTAL_TESTS=2

# --- Cleanup from previous runs ---
rm -f worker manager commands.txt worker_log_*.txt

# --- Compilation ---
echo "--- Compiling the programs ---"
gcc -Wall -Werror ../../worker/src/main.c -o worker
if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation of worker.c failed. Exiting.${NC}"
    exit 1
fi
gcc -Wall -Werror ../src/main.c -o manager
if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation of manager.c failed. Exiting.${NC}"
    exit 1
fi
echo -e "${GREEN}Compilation successful!${NC}"
echo "-------------------"

# --- Setup Test Environment ---
echo "--- Setting up test environment ---"
# Create the commands.txt file for the test
cat <<EOF > commands.txt
worker1 SIGUSR1
worker2 SIGUSR2
worker1 SIGUSR2
worker2 SIGUSR1
EOF
echo "commands.txt created."
echo "-------------------"

# --- Run Test ---
echo "--- Running Test: Manager-Worker Communication ---"
# Start two workers in the background and capture their PIDs
./worker &
worker1_pid=$!
./worker &
worker2_pid=$!

# Give workers a moment to start
sleep 1

# Run the manager, passing the worker PIDs
./manager "$worker1_pid" "$worker2_pid"
echo "Manager program finished."
echo "-------------------"

# Wait for all background processes to terminate
wait

# --- Validation ---
echo "--- Validating worker log files ---"

# Expected output for worker 1
expected_worker1_log="SIGUSR1 received
SIGUSR2 received"

# Expected output for worker 2
expected_worker2_log="SIGUSR2 received
SIGUSR1 received"

# Check log file for worker 1
actual_worker1_log=$(cat "worker_log_${worker1_pid}.txt" 2>/dev/null)
if [ "$actual_worker1_log" == "$expected_worker1_log" ]; then
    echo -e "${GREEN}Test 1 Passed: worker_log_${worker1_pid}.txt content is correct.${NC}"
    SCORE=$((SCORE + 1))
else
    echo -e "${RED}Test 1 Failed: worker_log_${worker1_pid}.txt content is incorrect.${NC}"
fi

# Check log file for worker 2
actual_worker2_log=$(cat "worker_log_${worker2_pid}.txt" 2>/dev/null)
if [ "$actual_worker2_log" == "$expected_worker2_log" ]; then
    echo -e "${GREEN}Test 2 Passed: worker_log_${worker2_pid}.txt content is correct.${NC}"
    SCORE=$((SCORE + 1))
else
    echo -e "${RED}Test 2 Failed: worker_log_${worker2_pid}.txt content is incorrect.${NC}"
fi

# --- Final Score ---
echo "-------------------"
echo "Tests Passed: $SCORE / $TOTAL_TESTS"
FINAL_SCORE=$(( (SCORE * 100) / TOTAL_TESTS ))
echo -e "Final Grade: ${GREEN}${FINAL_SCORE}%${NC}"

# --- Cleanup ---
#rm -f worker manager commands.txt worker_log_*.txt