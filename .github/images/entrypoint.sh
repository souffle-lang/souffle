#!/bin/sh
set -euxo

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

# Print version
./build/src/souffle --version

# Run a few tests for consistency
cmake --build ./build --target test -- ARGS="-R access1"
