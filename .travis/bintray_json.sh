#!/bin/bash


#$1 = Repository
#$2 = Version
#$3 = Output filename
#$4 = Repository type (debian/centos/fedora)
print_json () {
DATE=`date -u +"%Y-%m-%d"`

if [ "$4" = fedora ];
then
  #For distro dependent settings
  #RELEASE=`grep "^VERSION_ID=" /etc/os-release|sed 's/VERSION_ID=//'|tr -d '"'`
  #DIST=`grep "^ID=" /etc/os-release|sed 's/ID=//'|tr -d '"'`
  #ARCH=$(uname -i)

  #Fedora 27 x86_64
  DIST='fedora'
  RELEASE='27'
  ARCH="$5"

  FILES="[{\"includePattern\": \"deploy/(.*\.rpm)\", \"uploadPattern\": \"$DIST/$RELEASE/$ARCH/\$1\"
    }],"
elif [ "$4" = centos ];
then
  #For distro dependent settings
  #RELEASE=`grep "^VERSION_ID=" /etc/os-release|sed 's/VERSION_ID=//'|tr -d '"'`
  #DIST=`grep "^ID=" /etc/os-release|sed 's/ID=//'|tr -d '"'`
  #ARCH=$(uname -i)

  #Fedora 27 x86_64
  DIST='centos'
  RELEASE='8'
  ARCH="$5"

  FILES="[{\"includePattern\": \"deploy/(.*\.rpm)\", \"uploadPattern\": \"$DIST/$RELEASE/$ARCH/\$1\"
    }],"

elif [ "$4" = osx ];
then
  FILES="[{\"includePattern\": \"./(.*\.tar.gz)\", \"uploadPattern\": \"\$1\"}],"

else
  DIST="$4"
  ARCH="$5"
  if [ "$ARCH" = "x86_64" ];
  then
    ARCH=amd64
  fi

  FILES="[{\"includePattern\": \"deploy/(.*\.deb)\", \"uploadPattern\": \"pool/main/s/souffle/\$1\",
    \"matrixParams\": {
        \"deb_distribution\": \"$DIST\",
        \"deb_component\": \"main\",
        \"deb_architecture\": \"$ARCH\",
        \"override\": 1 }
    }],"
fi

cat > ${3} <<EOF
{
    "package": {
        "name": "souffle",
        "repo": "$1",
        "subject": "souffle-lang",
        "website_url": "http://souffle-lang.org/",
        "issue_tracker_url": "https://github.com/souffle-lang/souffle/issues",
        "vcs_url": "https://github.com/souffle-lang/souffle.git",
        "github_use_tag_release_notes": true,
        "github_release_notes_file": "debian/changelog",
        "licenses": ["UPL-1.0"],
        "public_download_numbers": false,
        "public_stats": false
    },

    "version": {
        "name": "$2",
        "released": "$DATE"
    },

    "files":
    $FILES
    "publish": true
}
EOF
}
if [ -n "$2" ];
then
  ARCH="$2"
else
  ARCH=x86_64
fi

if [ "$1" = debian ];
then
  VERSION=$(grep VERSION_CODENAME /etc/os-release |sed 's/VERSION_CODENAME=//')
  print_json "deb"  "`git describe --tags --always`" "bintray-deb-stable.json" "$VERSION" "$ARCH"
  print_json "deb-unstable" "`git describe --tags --always`" "bintray-deb-unstable.json" "$VERSION" "$ARCH"
elif [ "$1" = fedora ];
then
  print_json "rpm"  "`git describe --tags --always`" "bintray-rpm-stable.json" "fedora" "$ARCH"
  print_json "rpm-unstable"  "`git describe --tags --always`" "bintray-rpm-unstable.json" "fedora" "$ARCH"
elif [ "$1" = centos ];
then
  print_json "rpm"  "`git describe --tags --always`" "bintray-rpm-stable.json" "centos" "$ARCH"
  print_json "rpm-unstable"  "`git describe --tags --always`" "bintray-rpm-unstable.json" "centos" "$ARCH"
elif [ "$1" = osx ];
then
  print_json "osx"  "`git describe --tags --always`" "bintray-osx.json" "osx"
else
  exit 1
fi

