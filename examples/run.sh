#!/bin/sh

program="./falsesharing-dthread";

for i in {1..2001}
do
  #./fprintf-dthread
  $program
  exit_code="$?"
  if [[ "${exit_code}" -ne 0 ]]; then
    exit;
  fi
  echo "$i times"
done
  
#strace -ff -o out ./fprintf-dthread
