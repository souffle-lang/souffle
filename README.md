# Welcome!

This is the official repository for the [Soufflé](https://souffle-lang.github.io) language project.  
The Soufflé language is similar to Datalog (but has terms known as records), and is frequently used as a 
domain-specific language for analysis problems. 

[![License: UPL](https://img.shields.io/badge/License-UPL--1.0-blue.svg)](https://github.com/souffle-lang/souffle/blob/master/LICENSE)
[![Build Status](https://travis-ci.org/souffle-lang/souffle.svg?branch=master)](https://travis-ci.org/souffle-lang/souffle)
[![codecov](https://codecov.io/gh/souffle-lang/souffle/branch/master/graph/badge.svg)](https://codecov.io/gh/souffle-lang/souffle)

## Features of Soufflé

*   Efficient translation to parallel C++ of Datalog programs
*   Efficient interpretation  
*   Extended semantics of Datalog, e.g., permitting unbounded recursions with numbers and terms 
*   Simple component model for Datalog specifications 
*   Recursively defined record types (aka. constructors) for tuples 

## How to get Soufflé
 
Use git to obtain the source code of Soufflé. 

    $ git clone git://github.com/souffle-lang/souffle.git
    
Build instructions can be found [here](https://souffle-lang.github.io/build).

## Legacy code

If you have written code for an older version of Souffle, please use the command line flag `--legacy`. 
Alternatively, please add the following line to the start of your source-code:
```
.pragma "legacy"
```

## How to contribute

Issues and bug reports for Souffle are found in the [issue list](https://github.com/souffle-lang/souffle/issues). 
This list is also where new contributors may find extensions / bug fixes to work on.

To contribute in this repo, please open a pull request from your fork of this repository.
The general workflow is as follows.

1. Find an issue in the issue list.
2. Fork the [souffle-lang/souffle](http://github.com/souffle-lang/souffle.git) repo.
3. Push your changes to a branch in your forked repo. 
4. Submit a pull request to souffle-lang/souffle from your forked repo.
Our continuous integration framework enforces coding guidelines with the help of clang-format and clang-tidy. 

For more information on building and developing Souffle, please read the [developer tutorial](https://souffle-lang.github.io/development).

## [Home Page](https://souffle-lang.github.io)

## [Documentation](https://souffle-lang.github.io/docs.html)

## [Contributors](https://souffle-lang.github.io/contributors)

## [Issues](https://github.com/souffle-lang/souffle/issues)

## [License](https://github.com/souffle-lang/souffle/blob/master/licenses/SOUFFLE-UPL.txt)
