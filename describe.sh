#!/usr/bin/env sh

# Generate a release id for Souffle. If no .git directory is present and the
# current directory is named 'A-B', return 'B'. Otherwise, return 'unknown'.

if [ -d .git ]
then
    git describe --tags --always
else
    VER=$(basename $PWD | cut -d '-' -f 2-)
    if [ -z "$VER" ]
    then
        echo unknown
    else
        echo $VER
    fi
fi
