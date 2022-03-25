# Welcome!

This is the official repository for the [Soufflé](https://souffle-lang.github.io) language project.
The Soufflé language is similar to Datalog (but has terms known as records), and is frequently used as a
domain-specific language for analysis problems.

[![License: UPL](https://img.shields.io/badge/License-UPL--1.0-blue.svg)](https://github.com/souffle-lang/souffle/blob/master/LICENSE)
[![CI-Tests](https://github.com/souffle-lang/souffle/actions/workflows/CI-Tests.yml/badge.svg)](https://github.com/souffle-lang/souffle/actions/workflows/CI-Tests.yml)
[![MSVC-CI-Tests](https://github.com/souffle-lang/souffle/actions/workflows/VS-CI-Tests.yml/badge.svg)](https://github.com/souffle-lang/souffle/actions/workflows/VS-CI-Tests.yml)
[![codecov](https://codecov.io/gh/souffle-lang/souffle/branch/master/graph/badge.svg)](https://codecov.io/gh/souffle-lang/souffle)

## Features of Soufflé

*   Efficient translation to parallel C++ of Datalog programs (CAV'16, CC'16)

*   Efficient interpretation using de-specialization techniques (PLDI'21)

*   Specialized data structure for relations (PACT'19, PPoPP'19, PMAM'19) with optimal index selection (VLDB'18)

*   Extended semantics of Datalog, e.g., permitting unbounded recursions with numbers and terms

*   Simple component model for Datalog specifications

*   Recursively defined record types/ADTs (aka. constructors) for tuples

*   User-defined functors

*   Strongly-typed types for safety

*   Subsumption

*   Aggregation

*   Choice Construct (APLAS'21)

*   Extended I/O system for relations (including SQLITE3 interfaces)

*   C++/SWIG interfaces

*   Provenance/Debugging (TOPLAS'20)

*   Profiling tools


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

## Issues and Discussions 

Use either the [issue list](https://github.com/souffle-lang/souffle/issues) for

- bug reporting

- enhancements

- documentation

- proposals

- releases

- compatibility issues

- refactoring.

or use the [discussions](https://github.com/souffle-lang/souffle/discussions) bulletin board to engage with other community members and for asking questions you’re wondering about.

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

## For academics

If you use our work, please cite our work. A list of publications can be found [here](https://souffle-lang.github.io/publications). The main publications are:
 * Herbert Jordan, Bernhard Scholz, Pavle Subotić: Souffle: On Synthesis of Program Analyzers. CAV 2016.
 * Bernhard Scholz, Herbert Jordan, Pavle Subotić, Till Westmann: On fast large-scale program analysis in Datalog. CC 2016: 196-206
