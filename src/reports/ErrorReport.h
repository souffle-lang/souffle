/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file reports/ErrorReport.h
 *
 * Defines a class for error reporting.
 *
 ***********************************************************************/

#pragma once

#include "parser/SrcLocation.h"
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class DiagnosticMessage {
public:
    DiagnosticMessage(std::string message, SrcLocation location)
            : message(std::move(message)), hasLoc(true), location(std::move(location)) {}

    DiagnosticMessage(std::string message) : message(std::move(message)), hasLoc(false) {}

    const std::string& getMessage() const {
        return message;
    }

    const SrcLocation& getLocation() const {
        assert(hasLoc);
        return location;
    }

    bool hasLocation() const {
        return hasLoc;
    }

    void print(std::ostream& out) const {
        out << message;
        if (hasLoc) {
            out << " in " << location.extloc();
        }
        out << "\n";
    }

    friend std::ostream& operator<<(std::ostream& out, const DiagnosticMessage& diagnosticMessage) {
        diagnosticMessage.print(out);
        return out;
    }

private:
    std::string message;
    bool hasLoc;
    SrcLocation location;
};

class Diagnostic {
public:
    enum class Type { ERROR, WARNING };

    Diagnostic(Type type, DiagnosticMessage primaryMessage, std::vector<DiagnosticMessage> additionalMessages)
            : type(type), primaryMessage(std::move(primaryMessage)),
              additionalMessages(std::move(additionalMessages)) {}

    Diagnostic(Type type, DiagnosticMessage primaryMessage)
            : type(type), primaryMessage(std::move(primaryMessage)) {}

    Type getType() const {
        return type;
    }

    const DiagnosticMessage& getPrimaryMessage() const {
        return primaryMessage;
    }

    const std::vector<DiagnosticMessage>& getAdditionalMessages() const {
        return additionalMessages;
    }

    void print(std::ostream& out) const {
        out << (type == Type::ERROR ? "Error: " : "Warning: ");
        out << primaryMessage;
        for (const DiagnosticMessage& additionalMessage : additionalMessages) {
            out << additionalMessage;
        }
    }

    friend std::ostream& operator<<(std::ostream& out, const Diagnostic& diagnostic) {
        diagnostic.print(out);
        return out;
    }

    bool operator<(const Diagnostic& other) const {
        if (primaryMessage.hasLocation() && !other.primaryMessage.hasLocation()) {
            return true;
        }
        if (other.primaryMessage.hasLocation() && !primaryMessage.hasLocation()) {
            return false;
        }

        if (primaryMessage.hasLocation() && other.primaryMessage.hasLocation()) {
            if (primaryMessage.getLocation() < other.primaryMessage.getLocation()) {
                return true;
            }
            if (other.primaryMessage.getLocation() < primaryMessage.getLocation()) {
                return false;
            }
        }

        if (type == Type::ERROR && other.getType() == Type::WARNING) {
            return true;
        }
        if (other.getType() == Type::ERROR && type == Type::WARNING) {
            return false;
        }

        if (primaryMessage.getMessage() < other.primaryMessage.getMessage()) {
            return true;
        }
        if (other.primaryMessage.getMessage() < primaryMessage.getMessage()) {
            return false;
        }

        return false;
    }

private:
    Type type;
    DiagnosticMessage primaryMessage;
    std::vector<DiagnosticMessage> additionalMessages;
};

enum class WarnType : std::size_t {
    DeprecatedTypeDecl,
    DeprecatedQualifier,
    DollarSign,
    LatticeMissingOperator,
    NoRulesNorFacts,
    NoSubsumptiveRule,
    VarAppearsOnce,
    UselessDocComment,
    UselessAnnotation,

    // last element, never used except for computing the size parameter of
    // bitset_type in the definition of WarnSet.
    LastWarnType
};

class WarnSet {
private:
    using bitset_type = std::bitset<(std::size_t)WarnType::LastWarnType>;

public:
    WarnSet() : warns(bitset_type()) {
        this->set();  // default to enabling all warnings
    }

    WarnSet(const WarnSet& other) = default;

    bool test(const WarnType warn);

    // Enable all warnings
    void set();

    // Enable one warning
    void set(const WarnType warn);

    // Disable all warnings
    void reset(const WarnType warn);

    // Disable one warning
    void reset();

    // Enable one warning
    //
    // Returns whether or not the string was valid
    bool setStr(const std::string& str);

    // Disable one warning
    //
    // Returns whether or not the string was valid
    bool resetStr(const std::string& str);

private:
    bitset_type warns;

    std::optional<WarnType> warnTypeFromString(const std::string& s);
};

class ErrorReport {
public:
    ErrorReport() : warns(WarnSet()) {}

    ErrorReport(WarnSet warns) : warns(warns) {}

    ErrorReport(const ErrorReport& other) = default;

    std::size_t getNumErrors() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
                [](const Diagnostic& d) -> bool { return d.getType() == Diagnostic::Type::ERROR; });
    }

    std::size_t getNumWarnings() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
                [](const Diagnostic& d) -> bool { return d.getType() == Diagnostic::Type::WARNING; });
    }

    std::size_t getNumIssues() const {
        return diagnostics.size();
    }

    /** Adds an error with the given message and location */
    void addError(const std::string& message, SrcLocation location) {
        diagnostics.insert(
                Diagnostic(Diagnostic::Type::ERROR, DiagnosticMessage(message, std::move(location))));
    }

    /** Adds a warning with the given message and location */
    void addWarning(const WarnType type, const std::string& message, SrcLocation location) {
        if (warns.test(type)) {
            diagnostics.insert(
                    Diagnostic(Diagnostic::Type::WARNING, DiagnosticMessage(message, std::move(location))));
        }
    }

    void addDiagnostic(const Diagnostic& diagnostic) {
        diagnostics.insert(diagnostic);
    }

    void exitIfErrors() {
        if (getNumErrors() == 0) {
            return;
        }

        std::cerr << *this << getNumErrors() << " errors generated, evaluation aborted\n";
        exit(EXIT_FAILURE);
    }

    void print(std::ostream& out) const {
        for (const Diagnostic& diagnostic : diagnostics) {
            out << diagnostic;
        }
    }

    friend std::ostream& operator<<(std::ostream& out, const ErrorReport& report) {
        report.print(out);
        return out;
    }

private:
    std::set<Diagnostic> diagnostics;
    WarnSet warns;
};

}  // end of namespace souffle
