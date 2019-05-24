#!/usr/bin/env bash

CMD="$@"
DELAY=0.1

while true; do
    echo "${CMD}"
    sleep ${DELAY}
done | nc -u 192.168.3.254 35037