/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LogStatement.h
 *
 * Generate log statements for multiple file types.
 *
 ***********************************************************************/

#include "parser/SrcLocation.h"
#include <sstream>
#include <string>

namespace souffle {

class LogStatement {
public:
    static const std::string tNonrecursiveRelation(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@t-nonrecursive-relation";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";";
        return line.str();
    }
    static const std::string tRelationLoadTime(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@t-relation-loadtime";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";loadtime;";
        return line.str();
    }

    static const std::string tRelationSaveTime(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@t-relation-savetime";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";savetime;";
        return line.str();
    }

    static const std::string nNonrecursiveRelation(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@n-nonrecursive-relation";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";";
        return line.str();
    }

    static const std::string tNonrecursiveRule(
            const std::string& relationName, const SrcLocation& srcLocation, const std::string& datalogText) {
        const char* messageType = "@t-nonrecursive-rule";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";" << datalogText << ";";
        return line.str();
    }

    static const std::string nNonrecursiveRule(
            const std::string& relationName, const SrcLocation& srcLocation, const std::string& datalogText) {
        const char* messageType = "@n-nonrecursive-rule";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";" << datalogText << ";";
        return line.str();
    }

    static const std::string tRecursiveRule(const std::string& relationName, const std::size_t version,
            const SrcLocation& srcLocation, const std::string& datalogText) {
        const char* messageType = "@t-recursive-rule";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << version << ";" << srcLocation << ";"
             << datalogText << ";";
        return line.str();
    }

    static const std::string nRecursiveRule(const std::string& relationName, const std::size_t version,
            const SrcLocation& srcLocation, const std::string& datalogText) {
        const char* messageType = "@n-recursive-rule";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << version << ";" << srcLocation << ";"
             << datalogText << ";";
        return line.str();
    }

    static const std::string tRecursiveRelation(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@t-recursive-relation";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";";
        return line.str();
    }

    static const std::string nRecursiveRelation(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@n-recursive-relation";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";";
        return line.str();
    }

    static const std::string cRecursiveRelation(
            const std::string& relationName, const SrcLocation& srcLocation) {
        const char* messageType = "@c-recursive-relation";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";";
        return line.str();
    }

    static const std::string pProofCounter(
            const std::string& relationName, const SrcLocation& srcLocation, const std::string& datalogText) {
        // TODO (#590): the profiler should be modified to use this type of log message, as currently these
        // messages are ignored
        const char* messageType = "#p-proof-counter";
        std::stringstream line;
        line << messageType << ";" << relationName << ";" << srcLocation << ";" << datalogText << ";";
        // TODO (#590): the additional semicolon is added to maintain backwards compatibility and should
        // eventually be removed
        line << ";";
        return line.str();
    }

    static const std::string runtime() {
        const char* messageType = "@runtime";
        std::stringstream line;
        line << messageType << ";";
        return line.str();
    }

    static const std::string startDebug() {
        const char* messageType = "@start-debug";
        std::stringstream line;
        line << messageType;
        return line.str();
    }
};

}  // end of namespace souffle
