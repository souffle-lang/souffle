/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslationUnitBase.h
 *
 * Define a translation unit
 *
 ***********************************************************************/

#pragma once

#include "Global.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/utility/DynamicCasting.h"
#include "souffle/utility/Types.h"
#include <cassert>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace souffle::detail {

/**
 * @brief Abstract class for an analysis.
 * TODO: make `name` a template parameter to enforce `constexpr`ness
 */
class AnalysisBase {
public:
    // PRECONDITION: `name_as_cstr_literal` must have program lifetime (i.e. make it a literal)
    AnalysisBase(const char* name_as_cstr_literal) : name_as_cstr_literal(name_as_cstr_literal) {
        assert(name_as_cstr_literal);
    }
    virtual ~AnalysisBase() = default;

    /** @brief get name of the analysis */
    char const* getName() const {
        return name_as_cstr_literal;
    }

    /** @brief Print the analysis result in textual format */
    virtual void print(std::ostream& /* os */) const {}

    /** @brief Print the analysis result in HTML format (or fallback to textual format) */
    virtual void printHTML(std::ostream& os) const {
        print(os);
    }

private:
    char const* const name_as_cstr_literal;
};

inline std::ostream& operator<<(std::ostream& os, const AnalysisBase& rhs) {
    rhs.print(os);
    return os;
}

/**
 * @brief A translation context for a program.
 *
 * Comprises the program, symbol table, error report, debug report, and analysis caching.
 */
template <typename Impl, typename Program>
struct TranslationUnitBase {
    struct Analysis : souffle::detail::AnalysisBase {
        using AnalysisBase::AnalysisBase;
        virtual void run(Impl const&) = 0;
    };

    TranslationUnitBase(Global& g, Own<Program> prog, ErrorReport& e, DebugReport& d)
            : glb(g), program(std::move(prog)), errorReport(e), debugReport(d) {
        assert(program != nullptr && "program is a null-pointer");
    }

    /** get analysis: analysis is generated on the fly if not present */
    template <class A, typename = std::enable_if_t<std::is_base_of_v<AnalysisBase, A>>>
    A& getAnalysis() const {
        static_assert(std::is_same_v<char const* const, decltype(A::name)>,
                "`name` member must be a static literal");
        auto it = analyses.find(A::name);
        if (it == analyses.end()) {
            it = analyses.insert({A::name, mk<A>()}).first;

            auto& analysis = *it->second;
            assert(analysis.getName() == A::name && "must be same pointer");
            analysis.run(static_cast<Impl const&>(*this));
            logAnalysis(analysis);
        }

        return asAssert<A>(it->second.get());
    }

    /** @brief Get all alive analyses */
    std::set<const AnalysisBase*> getAliveAnalyses() const {
        std::set<const AnalysisBase*> result;
        for (auto const& a : analyses) {
            result.insert(a.second.get());
        }
        return result;
    }

    /** @brief Invalidate all alive analyses of the translation unit */
    void invalidateAnalyses() {
        analyses.clear();
    }

    /** @brief Get the global configuration */
    Global& global() const {
        return glb;
    }

    /** @brief Get the RAM Program of the translation unit  */
    Program& getProgram() const {
        return *program;
    }

    /** @brief Obtain error report */
    ErrorReport& getErrorReport() {
        return errorReport;
    }

    /** @brief Obtain debug report */
    DebugReport& getDebugReport() {
        return debugReport;
    }

protected:
    virtual void logAnalysis(Analysis&) const {}

    /* Cached analyses */
    // HACK: (GCC bug?) using `char const*` and GCC 9.2.0 w/ -O1+ and asan => program corruption
    //       Clang is happy. GCC 9.2.0 -O1+ w/o asan is happy. Go figure.
    //       Using `std::string` appears to suppress the issue (bug?).
    mutable std::map<std::string, Own<Analysis>> analyses;

    Global& glb;

    /* RAM program */
    Own<Program> program;

    /* Error report for raising errors and warnings */
    ErrorReport& errorReport;

    /* Debug report for logging information */
    DebugReport& debugReport;
};

}  // namespace souffle::detail
