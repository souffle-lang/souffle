#!/bin/bash

# Find the changed lines in the diff. Arguments 1 and 2 are appended to the
#git diff command
changedLines() {
  range_start=-1
  
  for m in $(git diff -U0 $1 $2|grep '^@@'|sed -r 's/^@@.* \+([0-9][0-9]*),?([0-9]*) .*$/_\1 _\2/'); do
    n=${m:1};
    if (( range_start > -1 )) ; then
      if [[ "$n" == "" ]]
      then
        echo -n " -lines=$range_start:$range_start"
      else
        if (( n > 0 )) ; then
            echo -n " -lines=$range_start:$((n + range_start - 1))"
        fi
      fi
      range_start=-1
    else
      range_start=$n
    fi
  done
  
  if (( range_start > -1 )) ; then
    echo -n " -lines=$range_start:$range_start"
  fi
}

# Move to the root of the repo if we aren't there already so the paths returned
# by git are correct.
cd "$(git rev-parse --show-toplevel)"

# Find all changed files in the diff
for f in $(git diff --name-only --diff-filter=ACMRTUXB $1); do
  if ! echo "$f" | egrep -q "[.](cpp|h)$"; then
    continue
  fi
  if ! echo "$f" | egrep -q "^src/"; then
    continue
  fi
  d=$(diff -u0 "$f" <(/usr/bin/clang-format -style=file "$f")) || true
  if [ -n "$d" ]; then
    echo "!!! $f not compliant to coding style. A suggested fix is below."
    echo "To make the fix automatically, use clang-format -i $f"
    echo
    echo "$d"
    echo
    fail=1
  fi
done

if [ "$fail" == 1 ]
then
  exit 1
else
  exit 0
fi
