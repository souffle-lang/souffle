/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ErrorReport.h
 *
 * Defines a class for error reporting.
 *
 ***********************************************************************/
#pragma once

#include "AstSrcLocation.h"

#include <algorithm>
#include <cassert>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace souffle {

class DiagnosticMessage {
private:
    std::string message;
    bool hasLoc;
    AstSrcLocation location;

public:
    DiagnosticMessage(std::string message, AstSrcLocation location)
            : message(std::move(message)), hasLoc(true), location(std::move(location)) {}

    DiagnosticMessage(std::string message) : message(std::move(message)), hasLoc(false) {}

    const std::string& getMessage() const {
        return message;
    }

    const AstSrcLocation& getLocation() const {
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
};

class Diagnostic {
public:
    enum Type { ERROR, WARNING };

private:
    Type type;
    DiagnosticMessage primaryMessage;
    std::vector<DiagnosticMessage> additionalMessages;

public:
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
        out << (type == ERROR ? "Error: " : "Warning: ");
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

        if (type == ERROR && other.getType() == WARNING) {
            return true;
        }
        if (other.getType() == ERROR && type == WARNING) {
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
};

class ErrorReport {
    std::set<Diagnostic> diagnostics;
    bool nowarn;

public:
    ErrorReport(bool nowarn = false) : nowarn(nowarn) {}

    ErrorReport(const ErrorReport& other) = default;

    unsigned getNumErrors() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
                [](Diagnostic d) -> bool { return d.getType() == Diagnostic::ERROR; });
    }

    unsigned getNumWarnings() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
                [](Diagnostic d) -> bool { return d.getType() == Diagnostic::WARNING; });
    }

    unsigned getNumIssues() const {
        return diagnostics.size();
    }

    /** Adds an error with the given message and location */
    void addError(const std::string& message, AstSrcLocation location) {
        diagnostics.insert(Diagnostic(Diagnostic::ERROR, DiagnosticMessage(message, location)));
    }

    /** Adds a warning with the given message and location */
    void addWarning(const std::string& message, AstSrcLocation location) {
        if (!nowarn) {
            diagnostics.insert(Diagnostic(Diagnostic::WARNING, DiagnosticMessage(message, location)));
        }
    }

    void addDiagnostic(Diagnostic diagnostic) {
        diagnostics.insert(diagnostic);
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
};

}  // end of namespace souffle
