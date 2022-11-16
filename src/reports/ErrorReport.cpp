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

#include "reports/ErrorReport.h"

namespace souffle {

std::optional<WarnType> WarnSet::warnTypeFromString(const std::string& s) {
    if (s == "deprecated-type-decl") {
        return std::optional<WarnType>(WarnType::DeprecatedTypeDecl);
    } else if (s == "deprecated-qualifier") {
        return std::optional<WarnType>(WarnType::DeprecatedQualifier);
    } else if (s == "dollar-sign") {
        return std::optional<WarnType>(WarnType::DollarSign);
    } else if (s == "no-rules-nor-facts") {
        return std::optional<WarnType>(WarnType::NoRulesNorFacts);
    } else if (s == "no-subsumptive-rule") {
        return std::optional<WarnType>(WarnType::NoSubsumptiveRule);
    } else if (s == "var-appears-once") {
        return std::optional<WarnType>(WarnType::VarAppearsOnce);
    } else if (s == "useless-doc-comment") {
        return {WarnType::UselessDocComment};
    } else if (s == "useless-annotation") {
        return {WarnType::UselessAnnotation};
    }
    return std::optional<WarnType>(std::nullopt);
}

bool WarnSet::test(const WarnType warn) {
    return warns.test(static_cast<std::size_t>(warn));
}

void WarnSet::set(const WarnType warn) {
    warns.set(static_cast<std::size_t>(warn));
}

void WarnSet::set() {
    warns.set();
}

void WarnSet::reset(const WarnType warn) {
    warns.reset(static_cast<std::size_t>(warn));
}

void WarnSet::reset() {
    warns.reset();
}

bool WarnSet::setStr(const std::string& str) {
    const auto warn = warnTypeFromString(str);
    if (warn.has_value()) {
        this->set(warn.value());
        return true;
    }
    return false;
}

bool WarnSet::resetStr(const std::string& str) {
    const auto warn = warnTypeFromString(str);
    if (warn.has_value()) {
        this->reset(warn.value());
        return true;
    }
    return false;
}

};  // namespace souffle
