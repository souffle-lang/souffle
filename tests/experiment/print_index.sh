#!/bin/bash

find . -type f -name "*.dl" | while read -r line ; do
    prefix="./"
    suffix=".dl"
    trimmed=${line#"$prefix"}
    trimmed=${trimmed%"$suffix"}
    # echo $trimmed
    ~/souffle/src/souffle_experimental $line --compile --show=transformed-ram | grep "Inequalities" > "./${trimmed}_default.out"
done
