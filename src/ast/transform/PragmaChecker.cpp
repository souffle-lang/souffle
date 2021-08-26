/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PragmaChecker.cpp
 *
 * Defines a transformer that applies pragmas found in parsed input.
 *
 ***********************************************************************/

#include "ast/transform/PragmaChecker.h"
#include "Global.h"
#include "ast/Pragma.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Visitor.h"
#include "reports/ErrorReport.h"
#include <utility>
#include <vector>

namespace souffle::ast::transform {

PragmaChecker::Merger::Merger() {
    auto& config = Global::config();

    for (auto&& [k, v] : config.data()) {
        if (config.state(k) == MainConfig::State::set) {
            locked_keys.insert(k);
        }
    }
}

bool PragmaChecker::Merger::operator()(std::string_view k, std::string_view v) {
    // Command line options take precedence, even if the param allows multiple
    if (contains(locked_keys, k)) return false;

    auto& config = Global::config();
    if (config.allowsMultiple(k))
        config.append(k, std::string(v));
    else
        config.set(std::string(k), std::string(v));

    return true;
}

bool PragmaChecker::transform(TranslationUnit& translationUnit) {
    Merger merger;

    auto& program = translationUnit.getProgram();
    auto& error = translationUnit.getErrorReport();
    bool changed = false;
    std::map<std::string, Pragma const*> previous_pragma;

    // Take in pragma options from the datalog file
    for (auto&& pragma : program.getPragmaDirectives()) {
        auto&& [k, v] = pragma->getkvp();

        // warn if subsequent pragmas override one another
        if (!Global::config().allowsMultiple(k)) {
            auto it = previous_pragma.find(k);
            if (it != previous_pragma.end()) {
                error.addDiagnostic({Diagnostic::Type::WARNING,
                        {tfm::format("overriding previous pragma for key `%s`", k), pragma->getSrcLoc()},
                        {{tfm::format("previous pragma for key `%s`", k), it->second->getSrcLoc()}}});
            }

            previous_pragma[k] = pragma.get();
        }

        changed |= merger(k, v);
    }

    return changed;
}
}  // namespace souffle::ast::transform
