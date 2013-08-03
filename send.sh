#!/bin/sh

while true; do
  for i in 41000 41001 41002 41003 41004 41005; do
    echo "balls" | nc $1 $i;
  done;

  sleep 1;
done
