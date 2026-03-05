#!/bin/bash
# Automated test script for testing the kernel

set -e

# flag #included by other files to indicate that testing is on
# just changing this won't work as intended, need to change the #includes 
# in C files as well
TEST_HEADER_FILE=test.h

# logs about the testing script itself
# contains qemu compilation messages, errors etc
TEST_LOGFILE=test.log

# what message should the kernel print in case it panics
# while testing is on?
PANICMSG="PANIC IN TESTING: SHUTDOWN"
PANIC_LOG=panic.log

# csv file containing test results
CSV_FILE="runs.csv"

# add any variable names here that you want tracked in the CSV headers
TUNE_VARS=("CPUS")

# initial Values
CPUS=1

# invoked when "clean" specified as argv[1]
clean() {
    echo -e "\e[33m[*] Cleaning up testing related files...\e[0m"
    rm -f "$TEST_LOGFILE" "$PANIC_LOG" "$CSV_FILE"
    rm -f .skeleton_done .remove_test_h_on_exit tmptestflagfile.log .csv_done
    rm -f .run_*.tmp .col_data.tmp "${CSV_FILE}.tmp"
    rm -f output tmpoutput.log
    rm -f $CSV_FILE
    
    # Restore test.h if a backup exists
    if [ -f tmptestflagfile.log ]; then
        mv tmptestflagfile.log "$TEST_FLAG_FILE"
        echo "[+] $TEST_FLAG_FILE restored from backup."
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

  # restore the header file
  echo -e "\n\n\e[33m[*] Stopping tests and restoring environment...\e[0m"
  cp tmptestflagfile.log $TEST_HEADER_FILE -f
  rm tmptestflagfile.log -f
  echo "[+] $TEST_HEADER_FILE restored."

  # if the script exits with an error code, dump the log
  # this can occur if make fails, or something similar
  if [ $exit_status -ne 0 ]; then
    echo -e "\n\e[31m[!] FAILURE DETECTED\e[0m"
    echo -e "\e[31m[!] check the entire logs in $TEST_LOGFILE\e[0m"
    echo "----------------- LAST 25 LINES OF LOG -----------------"
    tail -n 25 "$TEST_LOGFILE"
    echo "--------------------------------------------------------"
  fi
  exit
}

trap restore_env_report_err EXIT

init_csv() {
    # Clean up the skeleton tracker
    rm -f .csv_done

    # Create the Header Row (The first column in your old version)
    # We use -n to keep it on one line so we can append test names later
    echo -n "Run_Number" > "$CSV_FILE"
    
    # Add the tuning variable names as Column Headers
    for var in "${TUNE_VARS[@]}"; do
        echo -n ",$var" >> "$CSV_FILE"
    done
    
    # Add the separator header
    echo -n ",---,Category" >> "$CSV_FILE"
    
    # We do NOT add a newline yet because we need the test names 
    # from the first run to finish the header row.
}

update_csv() {
    local run_num=$1
    local output_src="output"

    if [ ! -s "$output_src" ]; then return; fi

    # 1. COMPLETING THE HEADER (Only on the very first successful run)
    if [ ! -f .csv_done ]; then
        # Extract test names, join them with commas, and append to the header line
        local test_names=$(grep "\[TEST\]" "$output_src" | awk -F' ' '{print $2}' | sed 's/://' | tr '\n' ',' | sed 's/,$//')
        echo ",$test_names" >> "$CSV_FILE"
        touch .csv_done
    fi

    # 2. BUILDING THE DATA ROW
    # Start the row with the run number
    local row_data="$run_num"

    # Append the values of your tuning variables (CPUS, TRYLOCK, etc.)
    for var in "${TUNE_VARS[@]}"; do
        row_data="$row_data,${!var}"
    done

    # Add the placeholders for the separator and 'Total' label
    row_data="$row_data,---,Total"

    # Extract the timing values, join with commas
    local timings=$(grep "\[TEST\]" "$output_src" | awk -F': ' '{print $2}' | tr '\n' ',' | sed 's/,$//')

    # 3. APPEND THE ROW
    echo "$row_data,$timings" >> "$CSV_FILE"
}

# copy the test header file into a temporary
touch $TEST_HEADER_FILE
cp -f $TEST_HEADER_FILE tmptestflagfile.log

# add your tuning variables here!!
# typically after the #define TESTING_PANICMSG line
echo "Starting Tests"
echo "// auto-generated file by $0 — do not edit
// if TESTING is 1, then init directly fork() exec()s
// usertests instead of the shell
#define TESTING 1
#define TESTING_PANICMSG \"$PANICMSG\"" > $TEST_HEADER_FILE

# copy the output file into a temporary
touch output
cp -f output tmpoutput.log
rm -f output

# initialise the csv based on the tuning variables
init_csv

# TODO: refactor loop by splitting into various functions for readability
i=1
REP=4
while [ 1 ]
do
  echo "----------------------------- Run $i ----------------------------------"
  # print variables in a nice horizontal bar: [VAR1=VAL] [VAR2=VAL] ...
  echo -ne "\e[36mConfig: "
  for var in "${TUNE_VARS[@]}"; do
      echo -n "[$var=${!var}] "
  done
  # \e[0m resets the color so the next lines are normal white
  echo -e "\e[0m\n" 

  # generic update of test header file
  # using \b to ensure we only match the exact variable name
  sed -i "s/^#define $var\b.*/#define $var ${!var}/" $TEST_HEADER_FILE

  # compile and run the kernel with tuned variables updated
  # pass the CPUS to Makefile since that is the only parameter that we would
  # like to tune in the Makefile
  make clean > "$TEST_LOGFILE" 2>&1
  make qemu CPUS=$CPUS >> "$TEST_LOGFILE" 2>&1

  # if the kernel panics, then log that to a file and continue with tests
  if grep -q "$PANICMSG" "$TEST_LOGFILE"; then
      echo -e "\e[1;31m[!] Panic detected in Run $i. Logging to $PANIC_LOG\e[0m"

      {
          echo "**************************************************"
          echo " RUN NUMBER: $i"
          # print variables in a nice horizontal bar: [VAR1=VAL] [VAR2=VAL] ...
          echo -ne "\e[36mConfig: "
          for var in "${TUNE_VARS[@]}"; do
            echo -n "[$var=${!var}] "
          done
          echo -e "\e[0m\n" # \e[0m resets the color so the next lines are normal white
          echo " TIME: $(date)"
          echo "**************************************************"
          cat "$TEST_LOGFILE"
          echo -e "\n\n"
      } >> "$PANIC_LOG"

      grep -A 5 "$PANICMSG" "$TEST_LOGFILE"
  else
      echo -e "\e[32mRun $i passed.\e[0m"
      
      # search for the timings in the output file and print them into a csv
      # along with the test names
      update_csv "$i"
      set +e
      grep Total output
      set -e
  fi

  # backup output and clear for next run
  if [ -f output ]; then
    cat output >> tmpoutput.log
    rm -f output
  fi
  rm -f output

  # update the tunable parameters as per requirement
  # we can create a separate update_params() function and just call it here
  # CPUS=$(( (i % 2) == 1 ? 1 : 2 ))
  i=`expr $i + 1`

  # Check if we have completed a batch of $REP runs
  if [ $(( (i - 1) % $REP )) -eq 0 ]; then
    echo -e "\e[35m[*] Batch of $REP completed for CPUS=$CPUS.\e[0m"

      # Increment CPUS
      CPUS=$((CPUS + 1))

      # Reset condition: If we've finished CPUS=6, loop back to 1
      if [ $CPUS -gt 8 ]; then
        echo -e "\e[33m[!] Max CPUs (8) reached. Exiting...\e[0m"
        exit
      fi
  fi
done
