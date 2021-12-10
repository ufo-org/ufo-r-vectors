#!/bin/bash
[ ! -e "$(dirname $0)/gen" ] && (gcc -o "$(dirname $0)/gen" "$(dirname $0)/gen.c")

echo "provided: $0 $@"

if [ $# -ne 3 ]
then
    echo "usage: gen.sh FILE N_ITEMS ones|sequence|random"
    echo "provided: $0 $@"
    exit 1
fi

case "$3" in
    random)   ./gen "$1" "r";; #head -c $((4 * $2)) /dev/urandom > "$1";;
    ones)     ./gen "$1" "o";;
    sequence) ./gen "$1" "s";;
esac
