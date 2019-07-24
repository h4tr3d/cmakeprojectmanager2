#!/usr/bin/env bash

if [ ! -f qtcreator.pro ]; then
    echo "This command must be executed from the ROOT folder of Qt Creator source tree"
    exit 1
fi

if [ -z "$1" ]; then
    echo "Use $(basename $0) Last-Change-Id"
    exit 1
fi

id=$1

commit=$(git log | grep '^commit \|Change-Id: ' | awk -v id=$id '
BEGIN {
    commit=""
}
{
    if ($1 ~ /^commit/) {
        commit=$2;
    }
    if ($1 ~ /^Change-Id:/ && $2 == id) 
    {
        print commit; 
        exit;
    }
}')

git format-patch $commit -- src/plugins/cmakeprojectmanager/
