/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file souffle2lb.cpp
 *
 * Main driver for translating Souffle programs to Logicblox programs.
 *
 * Logicblox V3/PA-Datalog is a Datalog engine that can be obtained here:
 *  http://snf-705535.vm.okeanos.grnet.gr/agreement.html
 *
 ***********************************************************************/

#include "AstArgument.h"
#include "AstAttribute.h"
#include "AstClause.h"
#include "AstComponentChecker.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "AstPragma.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstRelationIdentifier.h"
#include "AstSemanticChecker.h"
#include "AstTransforms.h"
#include "AstTranslationUnit.h"
#include "AstType.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "ComponentModel.h"
#include "DebugReport.h"
#include "ErrorReport.h"
#include "FunctorOps.h"
#include "Global.h"
#include "ParserDriver.h"
#include "SymbolTable.h"
#include "Util.h"
#include "config.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Conversion Exception for translation.
 */
class UnsupportedConstructException : public std::exception {
    std::string msg;

public:
    UnsupportedConstructException(std::string msg) : msg(std::move(msg)) {}
    const char* what() const noexcept override {
        return msg.c_str();
    }
};

class LogicbloxConverter : private AstVisitor<void, std::ostream&> {
    // literals aggregated to be added to the end of a rule while converting
    std::vector<std::string> extra_literals;
    std::ostream& iout;
    std::ostream& eout;
    std::ostream& dout;

public:
    LogicbloxConverter(std::ostream& imOut, std::ostream& exOut, std::ostream& decOut)
            : iout(imOut), eout(exOut), dout(decOut) {
        iout << "option,delimiter,\"\\t\"\n";
        iout << "option,hasColumnNames,false\n";
        iout << "\n";
        iout << "\n";

        eout << "option,delimiter,\"\\t\"\n";
        eout << "option,hasColumnNames,false\n";
        eout << "\n";
        eout << "\n";
    }

    void convert(std::ostream& out, const AstProgram& program) {
        visit(program, out);
    }

private:
    AstTypeIdentifier convertTypeName(const AstTypeIdentifier& name) {
        if (name == "number") {
            return AstTypeIdentifier("int[32]");
        } else if (name == "String") {
            return AstTypeIdentifier("string");
        } else
            return AstTypeIdentifier("string");
    }

    /**
     * The entry point for the conversion of a program, converting the basic top-level structure.
     */
    void visitProgram(const AstProgram& program, std::ostream& out) override {
        // declarations
        for (const auto& rel : program.getRelations()) {
            // process the relation declaration
            visit(*rel, out);
        }
        out << "\n";

        // rules
        for (const auto& rel : program.getRelations()) {
            for (const auto& clause : rel->getClauses()) {
                visit(*clause, out);
            }
        }
        out << "\n";
    }

    /**
     * Converting a relation by creating its declaration.
     */
    void visitRelation(const AstRelation& rel, std::ostream& /* out */) override {
        visitRelationIdentifier(rel.getName(), dout);

        // make nullary relations single-element relations
        dout << "(";
        dout << join(rel.getAttributes(), ",",
                [&](std::ostream& os, AstAttribute* cur) { os << cur->getAttributeName(); });
        dout << ") -> ";

        dout << join(rel.getAttributes(), ",", [&](std::ostream& os, AstAttribute* cur) {
            os << convertTypeName(cur->getTypeName()) << "(" << cur->getAttributeName() << ")";
        });

        dout << ".";
        dout << "\n";

        if (rel.isInput()) {
            int i = 0;
            iout << "fromFile,"
                 << "\"" << rel.getName() << ".facts\",";
            iout << join(rel.getAttributes(), ",", [&](std::ostream& os, AstAttribute* cur) {
                os << "column:" << (i) << "," << rel.getName() << ":" << (i);
                i++;
            });
            iout << "\n";
            iout << "toPredicate," << rel.getName() << ",";
            i = 0;
            iout << join(rel.getAttributes(), ",", [&](std::ostream& os, AstAttribute* cur) {
                os << rel.getName() << ":" << (i);
                i++;
            });
            iout << "\n";
        }
        if (rel.isOutput() || rel.isPrintSize()) {
            int i = 0;
            eout << "fromPredicate," << rel.getName() << ",";
            eout << join(rel.getAttributes(), ",", [&](std::ostream& os, AstAttribute* cur) {
                os << rel.getName() << ":" << (i);
                i++;
            });
            eout << "\n";
            i = 0;
            eout << "toFile,"
                 << "\"" << rel.getName() << ".csv\",";
            eout << join(rel.getAttributes(), ",", [&](std::ostream& os, AstAttribute* cur) {
                os << "column:" << (i) << "," << rel.getName() << ":" << (i);
                i++;
            });
            eout << "\n";
        }
    }

    /**
     * Converting a clause.
     */
    void visitClause(const AstClause& clause, std::ostream& out) override {
        visit(*clause.getHead(), out);

        // if it is a fact, that's it
        if (clause.isFact()) {
            // there must not be any additons
            if (!extra_literals.empty()) {
                throw UnsupportedConstructException("Unsupported fact: " + toString(clause));
            }
            out << ".\n";
            return;
        }

        // convert the body
        out << " <- ";
        out << join(
                clause.getBodyLiterals(), ",", [&](std::ostream& os, AstLiteral* cur) { visit(*cur, os); });

        // add extra_literals
        for (const auto& cur : extra_literals) {
            out << "," << cur;
        }
        extra_literals.clear();

        // done
        out << ".\n";
    }

    void visitAtom(const AstAtom& atom, std::ostream& out) override {
        visitRelationIdentifier(atom.getName(), out);

        // since no nullary-relations are allowed, we add a dummy value
        if (atom.getArguments().empty()) {
            out << "(0)";
            return;
        }

        out << "(";
        out << join(atom.getArguments(), ",", [&](std::ostream& os, AstArgument* cur) { visit(*cur, os); });
        out << ")";
    }

    void visitNegation(const AstNegation& neg, std::ostream& out) override {
        out << "!";
        visit(*neg.getAtom(), out);
    }

    void visitBinaryConstraint(const AstBinaryConstraint& cnstr, std::ostream& out) override {
        visit(*cnstr.getLHS(), out);
        out << toBinaryConstraintSymbol(cnstr.getOperator());
        visit(*cnstr.getRHS(), out);
    }

    void visitStringConstant(const AstStringConstant& str, std::ostream& out) override {
        out << str;
    }

    void visitNumberConstant(const AstNumberConstant& num, std::ostream& out) override {
        out << num;
    }

    void visitTypeCast(const AstTypeCast& cast, std::ostream& out) override {
        visit(*cast.getValue(), out);
    }

    void visitFunctor(const AstFunctor& fun, std::ostream& out) override {
        // only intrinsic binary operators supported
        const auto* inf = dynamic_cast<const AstIntrinsicFunctor*>(&fun);
        if (inf != nullptr && inf->getArity() == 2) {
            std::string sym = getSymbolForFunctorOp(inf->getFunction());
            out << "(";
            visit(*inf->getArg(0), out);
            out << sym;
            visit(*inf->getArg(1), out);
            out << ")";
        } else {
            throw UnsupportedConstructException("Unsupported function: " + toString(fun));
        }
    }

    void visitVariable(const AstVariable& var, std::ostream& out) override {
        out << var;
    }

    void visitUnnamedVariable(const AstUnnamedVariable& var, std::ostream& out) override {
        out << "_";
    }

    void visitRelationIdentifier(const AstRelationIdentifier& id, std::ostream& out) {
        out << join(id.getNames(), "_");
    }

    void visitNode(const AstNode& node, std::ostream& /*unused*/) override {
        throw UnsupportedConstructException(
                "Unable to convert the following language construct into bddbddb format: " + toString(node));
    }
};

void toLogicblox(std::ostream& out, std::ostream& impOut, std::ostream& expOut, std::ostream& decOut,
        const AstTranslationUnit& translationUnit) {
    // simply run the converter
    LogicbloxConverter(impOut, expOut, decOut).convert(out, *translationUnit.getProgram());
}

int main(int argc, char** argv) {
    /* Time taking for overall runtime */
    auto souffle_start = std::chrono::high_resolution_clock::now();

    /* have all to do with command line arguments in its own scope, as these are accessible through the global
     * configuration only */
    {
        std::stringstream header;
        header << "============================================================================" << std::endl;
        header << "souffle2lb -- translating souffle programs to Logicblox programs." << std::endl;
        header << "Usage: souffle2lb [OPTION] FILE." << std::endl;
        header << "----------------------------------------------------------------------------" << std::endl;
        header << "Options:" << std::endl;

        std::stringstream footer;
        footer << "----------------------------------------------------------------------------" << std::endl;
        footer << "Version: " << PACKAGE_VERSION << "" << std::endl;
        footer << "----------------------------------------------------------------------------" << std::endl;
        footer << "Copyright (c) 2016-18 The Souffle Developers." << std::endl;
        footer << "Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
        footer << "All rights reserved." << std::endl;
        footer << "============================================================================" << std::endl;

        // command line options, the environment will be filled with the arguments passed to them, or
        // the empty string if they take none
        // main option, the datalog program itself, has an empty key
        std::vector<MainOption> opts{{"", 0, "", "", false, ""},
                {"include-dir", 'I', "DIR", ".", true, "Specify directory for include files."},
                {"output", 'o', "FILE", "", false, "Generate bddbddb Datalog program"},
                {"debug-report", 'r', "FILE", "", false, "Write HTML debug report to <FILE>."},
                {"no-warn", 'w', "", "", false, "Disable warnings."},
                {"verbose", 'v', "", "", false, "Verbose output."},
                {"help", 'h', "", "", false, "Display this help message."}};
        Global::config().processArgs(argc, argv, header.str(), footer.str(), opts);

        // ------ command line arguments -------------

        /* for the help option, if given simply print the help text then exit */
        if (!Global::config().has("") || Global::config().has("help")) {
            std::cerr << Global::config().help();
            return 0;
        }

        /* check that Datalog program exists */
        if (!existFile(Global::config().get(""))) {
            throw std::runtime_error("cannot open file " + std::string(Global::config().get("")));
        }

        /* collect all input directories for the c pre-processor */
        if (Global::config().has("include-dir")) {
            std::string currentInclude = "";
            std::string allIncludes = "";
            for (const char& ch : Global::config().get("include-dir")) {
                if (ch == ' ') {
                    if (!existDir(currentInclude)) {
                        throw std::runtime_error("include directory " + currentInclude + " does not exists");
                    } else {
                        allIncludes += " -I";
                        allIncludes += currentInclude;
                        currentInclude = "";
                    }
                } else {
                    currentInclude += ch;
                }
            }
            allIncludes += " -I" + currentInclude;
            Global::config().set("include-dir", allIncludes);
        }

        // ------ start souffle -------------

        /* Create the pipe to establish a communication between cpp and souffle */
        std::string cmd = ::which("mcpp");

        if (!isExecutable(cmd)) {
            throw std::runtime_error("failed to locate mcpp pre-processor");
        }

        cmd += " -W0 " + Global::config().get("include-dir") + " " + Global::config().get("");
        FILE* in = popen(cmd.c_str(), "r");

        /* Time taking for parsing */
        auto parser_start = std::chrono::high_resolution_clock::now();

        // ------- parse program -------------

        // parse file
        SymbolTable symTab;
        ErrorReport errReport(Global::config().has("no-warn"));
        DebugReport debugReport;
        std::unique_ptr<AstTranslationUnit> astTranslationUnit =
                ParserDriver::parseTranslationUnit("<stdin>", in, symTab, errReport, debugReport);

        // close input pipe
        int preprocessor_status = pclose(in);
        if (preprocessor_status == -1) {
            perror(nullptr);
            throw std::runtime_error("failed to close pre-processor pipe");
        }

        /* Report run-time of the parser if verbose flag is set */
        if (Global::config().has("verbose")) {
            auto parser_end = std::chrono::high_resolution_clock::now();
            std::cout << "Parse Time: " << std::chrono::duration<double>(parser_end - parser_start).count()
                      << "sec\n";
        }

        // ------- check for parse errors -------------
        if (astTranslationUnit->getErrorReport().getNumErrors() != 0) {
            std::cerr << astTranslationUnit->getErrorReport();
            std::cerr << std::to_string(astTranslationUnit->getErrorReport().getNumErrors()) +
                                 " errors generated, evaluation aborted"
                      << std::endl;
            exit(1);
        }

        // ------- rewriting / optimizations -------------

        /* set up additional global options based on pragma declaratives */
        (std::make_unique<AstPragmaChecker>())->apply(*astTranslationUnit);

        /* construct the transformation pipeline */

        // Magic-Set pipeline
        auto magicPipeline = std::make_unique<ConditionalTransformer>(Global::config().has("magic-transform"),
                std::make_unique<PipelineTransformer>(std::make_unique<NormaliseConstraintsTransformer>(),
                        std::make_unique<MagicSetTransformer>(),
                        std::make_unique<RemoveRelationCopiesTransformer>(),
                        std::make_unique<RemoveEmptyRelationsTransformer>(),
                        std::make_unique<RemoveRedundantRelationsTransformer>()));

        // Main pipeline
        auto pipeline = std::make_unique<PipelineTransformer>(std::make_unique<AstComponentChecker>(),
                std::make_unique<ComponentInstantiationTransformer>(),
                std::make_unique<UniqueAggregationVariablesTransformer>(),
                std::make_unique<AstSemanticChecker>(),
                std::make_unique<RemoveBooleanConstraintsTransformer>(),
                std::make_unique<InlineRelationsTransformer>(),
                std::make_unique<ReduceExistentialsTransformer>(),
                std::make_unique<PartitionBodyLiteralsTransformer>(),
                std::make_unique<RemoveRelationCopiesTransformer>(),
                std::make_unique<MaterializeAggregationQueriesTransformer>(),
                std::make_unique<RemoveEmptyRelationsTransformer>(),
                std::make_unique<RemoveRedundantRelationsTransformer>(), std::move(magicPipeline),
                std::make_unique<AstExecutionPlanChecker>());

        // Set up the debug report if necessary
        if (!Global::config().get("debug-report").empty()) {
            auto parser_end = std::chrono::high_resolution_clock::now();
            std::string runtimeStr =
                    "(" + std::to_string(std::chrono::duration<double>(parser_end - parser_start).count()) +
                    "s)";
            DebugReporter::generateDebugReport(*astTranslationUnit, "Parsing", "After Parsing " + runtimeStr);

            pipeline->setDebugReport();
        }

        // Toggle pipeline verbosity
        pipeline->setVerbosity(Global::config().has("verbose"));

        // Apply all the transformations
        pipeline->apply(*astTranslationUnit);

        try {
            if (Global::config().get("output") == "") {
                // use STD-OUT
                toLogicblox(std::cout, std::cout, std::cout, std::cout, *astTranslationUnit);
            } else {
                // create an output file
                std::ofstream out((Global::config().get("output") + "_rules.logic").c_str());
                std::ofstream impOut((Global::config().get("output") + "_facts.import").c_str());
                std::ofstream expOut((Global::config().get("output") + "_rels.import").c_str());
                std::ofstream decOut((Global::config().get("output") + "_declarations.logic").c_str());
                toLogicblox(out, impOut, expOut, decOut, *astTranslationUnit);
            }
        } catch (const UnsupportedConstructException& uce) {
            throw std::runtime_error("failed to convert input specification into bddbddb syntax because " +
                                     std::string(uce.what()));
        }
        return 0;
    }

    // ------- execution -------------

    /* Report overall run-time in verbose mode */
    if (Global::config().has("verbose")) {
        auto souffle_end = std::chrono::high_resolution_clock::now();
        std::cout << "Total Time: " << std::chrono::duration<double>(souffle_end - souffle_start).count()
                  << "sec\n";
    }

    return 0;
}
}  // namespace souffle
// end of namespace souffle

/** main program */
int main(int argc, char** argv) {
    return souffle::main(argc, argv);
}
