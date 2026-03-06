#!/bin/bash
# Automated test script for testing the kernel

set -e

# flag #included by other files to indicate that testing is on
TEST_HEADER_FILE=test.h

# logs about the testing script itself
TEST_LOGFILE=test.log

# what message should the kernel print in case it panics
PANICMSG="PANIC IN TESTING: SHUTDOWN"
PANIC_LOG=panic.log

# Log for tests that exceed the time limit
TIMEOUT_LOG=timeout.log
TIMEOUT=120

# csv file containing test results
CSV_FILE="runs.csv"

# add any variable names here that you want tracked in the CSV headers
TUNE_VARS=("CPUS")

# initial Values
CPUS=1

# invoked when "clean" specified as argv[1]
clean() {
    echo -e "\e[33m[*] Cleaning up testing related files...\e[0m"
    rm -f "$TEST_LOGFILE" "$PANIC_LOG" "$CSV_FILE" "$TIMEOUT_LOG"
    rm -f .csv_done .remove_test_h_on_exit tmptestflagfile.log
    rm -f .run_*.tmp .col_data.tmp "${CSV_FILE}.tmp"
    rm -f output tmpoutput.log
    rm -f $CSV_FILE

    # Restore test.h if a backup exists
    if [ -f tmptestflagfile.log ]; then
        mv tmptestflagfile.log "$TEST_HEADER_FILE"
        echo "[+] $TEST_HEADER_FILE restored from backup."
    fi
    echo "[+] Clean complete."
}

# Check if "clean" was passed as an argument
if [ "$1" == "clean" ]; then
    clean
    exit 0
fi

restore_env_report_err() {
  exit_status=$?
  echo -e "\n\n\e[33m[*] Stopping tests and restoring environment...\e[0m"
  cp tmptestflagfile.log $TEST_HEADER_FILE -f
  rm tmptestflagfile.log -f
  echo "[+] $TEST_HEADER_FILE restored."

  if [ $exit_status -ne 0 ] && [ $exit_status -le 128 ]; then
    echo -e "\n\e[31m[!] FAILURE DETECTED\e[0m"
    echo -e "\e[31m[!] check the entire logs in $TEST_LOGFILE\e[0m"
    tail -n 25 "$TEST_LOGFILE"
  fi
  exit
}

trap restore_env_report_err EXIT

init_csv() {
    rm -f .csv_done
    echo -n "Run_Number" > "$CSV_FILE"
    for var in "${TUNE_VARS[@]}"; do
        echo -n ",$var" >> "$CSV_FILE"
    done
    echo -n ",---,Category" >> "$CSV_FILE"
}

update_csv() {
    local run_num=$1
    local output_src="output"
    if [ ! -s "$output_src" ]; then return; fi

    if [ ! -f .csv_done ]; then
        local test_names=$(grep "\[TEST\]" "$output_src" | awk -F' ' '{print $2}' | sed 's/://' | tr '\n' ',' | sed 's/,$//')
        echo ",$test_names" >> "$CSV_FILE"
        touch .csv_done
    fi

    local row_data="$run_num"
    for var in "${TUNE_VARS[@]}"; do
        row_data="$row_data,${!var}"
    done
    row_data="$row_data,---,Total"
    local timings=$(grep "\[TEST\]" "$output_src" | awk -F': ' '{print $2}' | tr '\n' ',' | sed 's/,$//')
    echo "$row_data,$timings" >> "$CSV_FILE"
}

touch $TEST_HEADER_FILE
cp -f $TEST_HEADER_FILE tmptestflagfile.log

echo "Starting Tests"
echo "// auto-generated file by $0 — do not edit
#define TESTING 1
#define TESTING_PANICMSG \"$PANICMSG\"" > $TEST_HEADER_FILE

touch output
cp -f output tmpoutput.log
rm -f output

init_csv

i=1
REP=4
while [ 1 ]
do
  echo "----------------------------- Run $i ----------------------------------"
  echo -ne "\e[36mConfig: "
  for var in "${TUNE_VARS[@]}"; do
      echo -n "[$var=${!var}] "
  done
  echo -e "\e[0m\n"

  sed -i "s/^#define $var\b.*/#define $var ${!var}/" $TEST_HEADER_FILE

  make clean > "$TEST_LOGFILE" 2>&1

  # 1. Start QEMU in a new session. This makes it a "Session Leader".
  # Redirecting to log here ensures window shows up but console output is saved.
  setsid make qemu CPUS=$CPUS >> "$TEST_LOGFILE" 2>&1 &
  QEMU_PID=$!

  # 2. Timer: After $TIMEOUT, we kill the entire session.
  # This targets the PID of the session leader and all its children.
  ( sleep $TIMEOUT && pkill -9 -s $QEMU_PID 2>/dev/null || true ) &
  TIMER_PID=$!

  # 3. Wait for the QEMU session leader to finish
  set +e
  wait $QEMU_PID
  EXIT_CODE=$?
  set -e

  # 4. Clean up timer
  kill $TIMER_PID 2>/dev/null || true
  wait $TIMER_PID 2>/dev/null || true

  # 5. Check for timeout (exit code 137/143 or manually verified)
  # If the process was killed via pkill, the wait status will reflect it.
  if [ $EXIT_CODE -gt 128 ]; then
      echo -e "\e[31mTime out\e[0m"
      echo -e "\e[1;31m[!] Timeout detected in Run $i. Logging to $TIMEOUT_LOG\e[0m"
      {
          echo "**************************************************"
          echo " TIMEOUT IN RUN: $i"
          echo -ne "Config: "
          for var in "${TUNE_VARS[@]}"; do echo -n "[$var=${!var}] "; done
          echo -e "\n TIME: $(date)\n**************************************************"
          tail -n 50 "$TEST_LOGFILE"
          echo -e "\n\n"
      } >> "$TIMEOUT_LOG"

      # Final nuke to ensure the window is gone
      pkill -9 -s $QEMU_PID 2>/dev/null || true

      echo "Restarting Run $i..."
      continue
  fi

  if grep -q "$PANICMSG" "$TEST_LOGFILE"; then
      echo -e "\e[1;31m[!] Panic detected in Run $i. Logging to $PANIC_LOG\e[0m"
      {
          echo "**************************************************"
          echo " RUN NUMBER: $i"
          echo -ne "\e[36mConfig: "
          for var in "${TUNE_VARS[@]}"; do echo -n "[$var=${!var}] "; done
          echo -e "\n TIME: $(date)\n**************************************************"
          cat "$TEST_LOGFILE"
          echo -e "\n\n"
      } >> "$PANIC_LOG"
      grep -A 5 "$PANICMSG" "$TEST_LOGFILE"
  else
      echo -e "\e[32mRun $i passed.\e[0m"
      update_csv "$i"
      set +e
      grep Total output
      set -e
  fi

  if [ -f output ]; then
    cat output >> tmpoutput.log
    rm -f output
  fi
  rm -f output

  i=`expr $i + 1`

  if [ $(( (i - 1) % $REP )) -eq 0 ]; then
    echo -e "\e[35m[*] Batch of $REP completed for CPUS=$CPUS.\e[0m"
      CPUS=$((CPUS + 1))
      if [ $CPUS -gt 8 ]; then
        echo -e "\e[33m[!] Max CPUs (8) reached. Exiting...\e[0m"
        exit
      fi
  fi
done
