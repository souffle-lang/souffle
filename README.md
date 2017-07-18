
# [Soufflé](https://souffle-lang.gitio.com)

[![Build Status](https://travis-ci.org/souffle-lang/souffle.svg?branch=master)](https://travis-ci.org/souffle-lang/souffle)

Soufflé is a translator of declarative Datalog programs into the C++ language.  Soufflé is used as a domain-specific language for static program analysis, over large code bases with millions of lines of code.  Soufflé aims at producing high-performance C++ code that can be compiled with the native compiler on the target machine.  

## Features of Soufflé

*   Efficient translation to parallel C++ of Datalog programs
*   Extended semantics of Pure Datalog, e.g., permitting unbounded recursions with numbers 
*   Simple component model for Datalog specifications 
*   Recursively defined record types for tuples 

This is the development repository of Soufflé. Oracle Lab's official repository can be found here:

http://github.com/oracle/souffle

## How to get Soufflé
 
Use git to obtain the source code of Soufflé. 

    $ git clone git://github.com/souffle-lang/souffle.git

## Soufflé home page

The URL of the Soufflé home page is:

http://souffle-lang.github.io

## Documentation

http://github.com/souffle-lang/souffle/wiki

## Contributors

http://github.com/souffle-lang/souffle/wiki/contributors

## Mailing list

There is no mailing list to talk about Soufflé at the moment. It will be established soon. 

## How to compile and install 

Follow the steps below to compile and install Soufflé on a UNIX system:

1.  Install the needed dependencies. G++ 4.8 or greater is recommended to compile Soufflé. openjdk-7 can be used if openjdk-8 is not available on your platform.

```
apt-get install build-essential pkg-config
apt-get install autoconf automake bison doxygen flex g++ git graphviz libsqlite3-dev libtool libncurses5-dev lsb-release make openjdk-8-jdk git zlib1g-dev
```

2.  Run `sh ./bootstrap` to generate configure files 

3.  For Linux users, skip this point. MAC OS X does not have OpenMP nor a bison version 3.0.2 or higher installed.
    We recommend [brew](http://brew.sh) to install the required tools to build Soufflé. Run the following commands prior to executing `./configure`:

```
     brew update                
     brew reinstall gcc --without-multilib                
     brew install bison  
     brew install boost
     brew link bison --force
     brew install libtool automake
     export CXX=/usr/local/bin/g++-5                
     export CXXFLAGS=-fopenmp                
     export SOUFFLECPP=/usr/local/bin/cpp-5
     export BISON=/usr/local/opt/bison/bin/bison
```

4.  Run `./configure`

5.  Run `make` to build the executable of Soufflé

6.  Test the executable with `make check` to check whether the compilation of Soufflé succeeded.

7.  Run `make install`

    This command will create the directories and install files in `${DESTDIR}${prefix}` for system-wide use in your system.

## License

See [LICENSE](https://github.com/souffle-lang/souffle/blob/master/licenses/SOUFFLE-UPL.txt).

## For souffle developers

### Multiple builds

Souffle supports out-of-source builds, to enable multipe builds using e.g. different compilers or debug options based on the same source base. 

The configure script uses the compiler specified by an `CXX` environment variable as the compiler to be used. Furthermore, the environment variable `BUILD_TYPE` may be set to "Debug" to create a debug build, instead of the default release build.

The following commands create a gcc 5 debug, a gcc 5 release, and a clang build:

```
# basic setup
git clone git://github.com/souffle-lang/souffle.git souffle
cd souffle
sh ./bootstrap

# create gcc debug build directory
mkdir build_gcc_debug
cd build_gcc_debug
CXX=g++-5 BUILD_TYPE=Debug ../configure
make -j4
cd ..

# create gcc release build directory
mkdir build_gcc_release
cd build_gcc_debug
CXX=g++-5 ../configure
make -j4
cd ..

# create clang release build directory
mkdir build_clang_release
cd build_clang_release
CXX=clang++-3.6 ../configure
make -j4
cd ..
```

### Parallel Testing

The unit tests in the source directory can be executed in parallel using the `-j` option of make. For instance,

```
cd src
make -j4 check
```

will run the unit tests of the current build directory using 4 threads.

By exporting the following environment variable

```
export TESTSUITEFLAGS=-j4
```

the integration test script will also process tests in parallel.

### Selective Testing

To run an individual integration test, the script `tests/testsuite` can be used. The command

```
testsuite -l
```

lists all the test cases, with their associated number. To run an individual test,

```
testsuite <case_nr>
```

can be used.
