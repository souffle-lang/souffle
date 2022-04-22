#!/bin/sh

BASEDIR=$(pwd)

DEBPATH=$BASEDIR/downloads

# Get current tag
SOUFFLE_TAG=$(git describe --tags)

# Create a temp directory to work out of
TMPDIR=$(mktemp -d)
cd $TMPDIR

# clone the ppa repo and move into it
git clone "https://x-access-token:$API_TOKEN_GITHUB@github.com/$GITHUB_REPOSITORY_OWNER/ppa"

#DEB files
cd ppa/ubuntu/
PPADIR=$(pwd)

mkdir -p pool/stable
cp $DEBPATH/*/*deb pool/stable/


STABLEDIST=${PPADIR}/dists/stable/

mkdir -p $STABLEDIST/main/binary-amd64

# Make package file for all packages
apt-ftparchive packages -c $BASEDIR/.github/scripts/apt-ftparchive.conf pool/ | tee dists/stable/main/binary-amd64/Packages | gzip > dists/stable/main/binary-amd64/Packages.gz
# Make Release file
apt-ftparchive release -c $BASEDIR/.github/scripts/apt-ftparchive.conf dists/stable/ > dists/stable/Release

gpg -u A319AEAB83D9859296F41980194B22AF03FAC2DD -abs > ${STABLEDIST}Release.gpg < ${STABLEDIST}Release
gpg -u A319AEAB83D9859296F41980194B22AF03FAC2DD -abs --clearsign > ${STABLEDIST}InRelease < ${STABLEDIST}Release

git add pool dists
git commit -m "Added deb files for $SOUFFLE_TAG"

#RPM files
## install repo handlers
sudo apt-get update
sudo apt-get install createrepo rpm

## Set up rpm signing with gpg
echo "%_gpg_name Bot\n%__gpg_sign_cmd %{__gpg} gpg --force-v3-sigs --batch --verbose --no-armor --no-secmem-warning -u \"%{_gpg_name}\" -sbo %{__signature_filename} --digest-algo sha256 %{__plaintext_filename}'" > ~/.rpmmacros

## Fedora
mkdir -p $TMPDIR/ppa/fedora/34/x86_64
cd $TMPDIR/ppa/fedora

for i in $DEBPATH/*fedora-34*/*rpm
do
    rpm --addsign $i
done

cp $DEBPATH/*fedora-34*/*rpm 34/x86_64/

createrepo 34/x86_64

git add .
git commit -m "Added fedora rpm files for $SOUFFLE_TAG"

## oraclelinux
mkdir -p $TMPDIR/ppa/ol/8/x86_64
cd $TMPDIR/ppa/ol

for i in $DEBPATH/*oracle*8*/*rpm
do
    rpm --addsign $i
done

cp $DEBPATH/*oracle*8*/*rpm 8/x86_64/

createrepo 8/x86_64

git add .
git commit -m "Added oraclelinux rpm files for $SOUFFLE_TAG"


git push
