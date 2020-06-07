#!/bin/bash

set -e

# Find the changed lines in the diff. Arguments 1 and 2 are appended to the
#git diff command

CLANGFORMAT=clang-format
VERSION=$($CLANGFORMAT --version)

echo "Testing style using $VERSION"

# Move to the root of the repo if we aren't there already so the paths returned
# by git are correct.
cd "$(git rev-parse --show-toplevel)"

# Find all changed files in the diff
for f in $(git diff --name-only --diff-filter=ACMRTUXB $1); do
  if echo "$f" | egrep -q "[.](cpp|h)$"; then
    $CLANGFORMAT -style=file "$f" -i
    d=$(git diff --minimal --color=always --ws-error-highlight=all $f) || true
    if [ -n "$d" ]; then
      echo "!!! $f not compliant to coding style. A suggested fix is below."
      echo "To make the fix automatically, use $CLANGFORMAT -i $f"
      echo
      echo "$d"
      echo
      fail=1
    fi
  elif echo "$f" | egrep -q "[.](dl)$"; then
    sed -i 's/[ \t]*$//' "$f" || true
    d=$(git diff --minimal --color=always --ws-error-highlight=all $f) || true
    if [ -n "$d" ]; then
      echo "$f not compliant to coding style.
      echo "Stray white space at line endings found:
      echo
      echo "$d"
      echo
      fail=1
    fi
  fi
done

if [ "$fail" == 1 ]
then
  exit 1
else
  exit 0
fi
