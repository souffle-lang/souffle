#!/bin/sh

if [ "$GPG_SIGNING_KEY" = "" ]; then
  echo "ERROR: No GPG_SIGNING_KEY defined"
  exit 200
fi

mkdir -p ~/.gnupg/
echo "${GPG_SIGNING_KEY}" > ~/.gnupg/private.key
gpg --import ~/.gnupg/private.key

git config --global user.email "36495947+souffle-lang-bot@users.noreply.github.com"
git config --global user.name "souffle-lang-bot"
