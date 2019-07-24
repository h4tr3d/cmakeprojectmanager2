#!/usr/bin/env bash

# Compose list to apply
FILES=

# Filter out some patches
for p in *.patch
do
    id=$(cat $p | grep 'Change-Id:')
    git log | grep "$id" > /dev/null 2>&1 
    rez=$?
    if [ $rez -eq 1 ]; then
        FILES="$FILES $p"
    else
        echo "skip: $p"
    fi
done

echo "new changes:"
for p in $FILES
do
    echo "  $p"
done

# Apply
git am -p4 $FILES
