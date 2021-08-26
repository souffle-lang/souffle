#!/bin/sh

PACKAGE_CLOUD_API_KEY="$1"

# Run the build command
case "$DOMAIN_SIZE" in
  	"64bit")
		cmake -S . -B ./build -DSOUFFLE_DOMAIN_64BIT=ON
    ;;

  	"32bit")
		cmake -S . -B ./build
    ;;
esac

# Create the package
cmake --build ./build --parallel "$(nproc)" --target package

cd build

# Upload the package to packagecloud.io
PACKAGECLOUD_TOKEN="$PACKAGE_CLOUD_API_KEY" package_cloud push souffle-lang/souffle/$PKG_CLOUD_OS_NAME "$(ls *$PKG_EXTENSION | head -n1)"
