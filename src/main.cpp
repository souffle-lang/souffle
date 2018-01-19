/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file main.cpp
 *
 * Main driver for Souffle
 *
 ***********************************************************************/

#include "AstAnalysis.h"
#include "AstPragma.h"
#include "AstProgram.h"
#include "AstSemanticChecker.h"
#include "AstTransformer.h"
#include "AstTransforms.h"
#include "AstTranslationUnit.h"
#include "AstTuner.h"
#include "AstUtils.h"
#include "BddbddbBackend.h"
#include "ComponentModel.h"
#ifdef USE_PROVENANCE
#include "Explain.h"
#endif
#include "AstTranslator.h"
#include "Global.h"
#include "Interpreter.h"
#include "InterpreterInterface.h"
#include "ParserDriver.h"
#include "PrecedenceGraph.h"
#include "RamStatement.h"
#include "SymbolTable.h"
#include "Synthesiser.h"
#include "Util.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <string>

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace souffle {

static void wrapPassesForDebugReporting(std::vector<std::unique_ptr<AstTransformer>>& transforms) {
    for (unsigned int i = 0; i < transforms.size(); i++) {
        transforms[i] = std::unique_ptr<AstTransformer>(new DebugReporter(std::move(transforms[i])));
    }
}

int main(int argc, char** argv) {
    /* Time taking for overall runtime */
    auto souffle_start = std::chrono::high_resolution_clock::now();

    /* have all to do with command line arguments in its own scope, as these are accessible through the global
     * configuration only */
    {
        Global::config().processArgs(argc, argv,
                []() {
                    std::stringstream header;
                    header << "============================================================================"
                           << std::endl;
                    header << "souffle -- A datalog engine." << std::endl;
                    header << "Usage: souffle [OPTION] FILE." << std::endl;
                    header << "----------------------------------------------------------------------------"
                           << std::endl;
                    header << "Options:" << std::endl;
                    return header.str();
                }(),
                []() {
                    std::stringstream footer;
                    footer << "----------------------------------------------------------------------------"
                           << std::endl;
                    footer << "Version: " << PACKAGE_VERSION << "" << std::endl;
                    footer << "----------------------------------------------------------------------------"
                           << std::endl;
                    footer << "Copyright (c) 2016-17 The Souffle Developers." << std::endl;
                    footer << "Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
                    footer << "All rights reserved." << std::endl;
                    footer << "============================================================================"
                           << std::endl;
                    return footer.str();
                }(),
                // command line options, the environment will be filled with the arguments passed to them, or
                // the empty string if they take none
                []() {
                    MainOption opts[] = {
                            {"", 0, "", "", false,
                                    ""},  // main option, the datalog program itself, key is always empty
                            {"fact-dir", 'F', "DIR", ".", false, "Specify directory for fact files."},
                            {"include-dir", 'I', "DIR", ".", true, "Specify directory for include files."},
                            {"output-dir", 'D', "DIR", ".", false,
                                    "Specify directory for output files (if <DIR> is -, stdout is used)."},
                            {"jobs", 'j', "N", "1", false,
                                    "Run interpreter/compiler in parallel using N threads, N=auto for system "
                                    "default."},
                            {"compile", 'c', "", "", false,
                                    "Generate C++ source code, compile to a binary executable, then run this "
                                    "executable."},
                            {"auto-schedule", 'a', "", "", false,
                                    "Switch on automated clause scheduling for compiler."},
                            {"generate", 'g', "FILE", "", false,
                                    "Generate C++ source code for the given Datalog program and write it to "
                                    "<FILE>."},
                            {"no-warn", 'w', "", "", false, "Disable warnings."},
                            {"magic-transform", 'm', "RELATIONS", "", false,
                                    "Enable magic set transformation changes on the given relations, use '*' "
                                    "for all."},
                            {"dl-program", 'o', "FILE", "", false,
                                    "Generate C++ source code, written to <FILE>, and compile this to a "
                                    "binary executable (without executing it)."},
                            {"profile", 'p', "FILE", "", false,
                                    "Enable profiling, and write profile data to <FILE>."},
                            {"bddbddb", 'b', "FILE", "", false, "Convert input into bddbddb file format."},
                            {"debug-report", 'r', "FILE", "", false, "Write HTML debug report to <FILE>."},
                            {"fault-tolerance", 'f', "", "", false,
                                    "Enable fault tolerance to recover from failure on program restart."},
                            {"stratify", 's', "FILE", "", false,
                                    "Generate/compile to multiple subprograms, and write an execution graph "
                                    "to FILE (valid extensions are '.dot' or '.json')."},
#ifdef USE_PROVENANCE
                            {"provenance", 't', "EXPLAIN", "", false,
                                    "Enable provenance information via guided SLD."},
#endif
                            {"verbose", 'v', "", "", false, "Verbose output."},
                            {"help", 'h', "", "", false, "Display this help message."}};
                    return std::vector<MainOption>(std::begin(opts), std::end(opts));
                }());

        // ------ command line arguments -------------

        /* for the help option, if given simply print the help text then exit */
        if (!Global::config().has("") || Global::config().has("help")) {
            std::cerr << Global::config().help();
            return 0;
        }

        /* check that datalog program exists */
        if (!existFile(Global::config().get(""))) {
            ERROR("cannot open file " + std::string(Global::config().get("")));
        }

        /* for the jobs option, to determine the number of threads used */
        if (Global::config().has("jobs")) {
            if (isNumber(Global::config().get("jobs").c_str())) {
                if (std::stoi(Global::config().get("jobs")) < 1) {
                    ERROR("Number of jobs in the -j/--jobs options must be greater than zero!");
                }
            } else {
                if (!Global::config().has("jobs", "auto")) {
                    ERROR("Wrong parameter " + Global::config().get("jobs") + " for option -j/--jobs!");
                }
                Global::config().set("jobs", "0");
            }
        } else {
            ERROR("Wrong parameter " + Global::config().get("jobs") + " for option -j/--jobs!");
        }

        /* if an output directory is given, check it exists */
        if (Global::config().has("output-dir") && !Global::config().has("output-dir", "-") &&
                !existDir(Global::config().get("output-dir")) &&
                !(Global::config().has("generate") ||
                        (Global::config().has("dl-program") && !Global::config().has("compile")))) {
            ERROR("output directory " + Global::config().get("output-dir") + " does not exists");
        }

        /* ensure that if auto-scheduling is enabled an output file is given */
        if (Global::config().has("auto-schedule") && !Global::config().has("dl-program")) {
            ERROR("no executable is specified for auto-scheduling (option -o <FILE>)");
        }

        /* collect all input directories for the c pre-processor */
        if (Global::config().has("include-dir")) {
            std::string currentInclude = "";
            std::string allIncludes = "";
            for (const char& ch : Global::config().get("include-dir")) {
                if (ch == ' ') {
                    if (!existDir(currentInclude)) {
                        ERROR("include directory " + currentInclude + " does not exists");
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

        /* ensure that code generation and/or compilation is enabled if stratification is for non-none
         * options*/
        if (Global::config().has("stratify")) {
            if (Global::config().has("profile")) {
                ERROR("stratification cannot be enabled with option 'profile'.");
            } else if (Global::config().get("stratify") == "-" && Global::config().has("generate")) {
                ERROR("stratification cannot be enabled with format 'auto' and option 'generate'.");
            } else if (!(Global::config().has("compile") || Global::config().has("dl-program") ||
                               Global::config().has("generate"))) {
                ERROR("one of 'compile', 'dl-program', or 'generate' options must be present for "
                      "stratification");
            }
        }

        /* turn on compilation of executables */
        if (Global::config().has("dl-program")) {
            Global::config().set("compile");
        }

        /* disable provenance with multithreading */
        if (Global::config().has("provenance")) {
            if (Global::config().has("jobs")) {
                if (Global::config().get("jobs") != "1") {
                    ERROR("provenance cannot be enabled with multiple jobs.");
                }
            }
        }
    }

    // ------ start souffle -------------

    std::string souffleExecutable = which(argv[0]);

    if (souffleExecutable.empty()) {
        ERROR("failed to determine souffle executable path");
    }

    /* Create the pipe to establish a communication between cpp and souffle */
    std::string cmd = ::which("mcpp");

    if (!isExecutable(cmd)) {
        ERROR("failed to locate mcpp pre-processor");
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
    std::unique_ptr<AstTranslationUnit> translationUnit =
            ParserDriver::parseTranslationUnit("<stdin>", in, symTab, errReport, debugReport);

    // close input pipe
    int preprocessor_status = pclose(in);
    if (preprocessor_status == -1) {
        perror(nullptr);
        ERROR("failed to close pre-processor pipe");
    }

    /* Report run-time of the parser if verbose flag is set */
    if (Global::config().has("verbose")) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::cout << "Parse Time: " << std::chrono::duration<double>(parser_end - parser_start).count()
                  << "sec\n";
    }

    // ------- check for parse errors -------------
    if (translationUnit->getErrorReport().getNumErrors() != 0) {
        std::cerr << translationUnit->getErrorReport();
        std::cerr << std::to_string(translationUnit->getErrorReport().getNumErrors()) +
                             " errors generated, evaluation aborted"
                  << std::endl;
        exit(1);
    }

    // ------- rewriting / optimizations -------------

    /* set up additional global options based on pragma declaratives */
    (std::unique_ptr<AstTransformer>(new AstPragmaChecker()))->apply(*translationUnit);
    std::vector<std::unique_ptr<AstTransformer>> transforms;

    transforms.push_back(std::unique_ptr<AstTransformer>(new ComponentInstantiationTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new UniqueAggregationVariablesTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new AstSemanticChecker()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new InlineRelationsTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new ExtractDisconnectedLiteralsTransformer()));
    if (Global::config().get("bddbddb").empty()) {
        transforms.push_back(std::unique_ptr<AstTransformer>(new ResolveAliasesTransformer()));
    }
    transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveRelationCopiesTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new MaterializeAggregationQueriesTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveEmptyRelationsTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveRedundantRelationsTransformer()));

    if (Global::config().has("magic-transform")) {
        transforms.push_back(std::unique_ptr<AstTransformer>(new NormaliseConstraintsTransformer()));
        transforms.push_back(std::unique_ptr<AstTransformer>(new MagicSetTransformer()));

        if (Global::config().get("bddbddb").empty()) {
            transforms.push_back(std::unique_ptr<AstTransformer>(new ResolveAliasesTransformer()));
        }
        transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveRelationCopiesTransformer()));
        transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveEmptyRelationsTransformer()));
        transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveRedundantRelationsTransformer()));
    }

    transforms.push_back(std::unique_ptr<AstTransformer>(new AstExecutionPlanChecker()));

    if (Global::config().has("auto-schedule")) {
        transforms.push_back(std::unique_ptr<AstTransformer>(new AutoScheduleTransformer()));
    }
#ifdef USE_PROVENANCE
    // Add provenance information by transforming to records
    if (Global::config().has("provenance")) {
        transforms.push_back(std::unique_ptr<AstTransformer>(new ProvenanceTransformer()));
    }
#endif
    if (!Global::config().get("debug-report").empty()) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::string runtimeStr =
                "(" + std::to_string(std::chrono::duration<double>(parser_end - parser_start).count()) + "s)";
        DebugReporter::generateDebugReport(*translationUnit, "Parsing", "After Parsing " + runtimeStr);
        wrapPassesForDebugReporting(transforms);
    }

    for (const auto& transform : transforms) {
        transform->apply(*translationUnit);

        /* Abort evaluation of the program if errors were encountered */
        if (translationUnit->getErrorReport().getNumErrors() != 0) {
            std::cerr << translationUnit->getErrorReport();
            std::cerr << std::to_string(translationUnit->getErrorReport().getNumErrors()) +
                                 " errors generated, evaluation aborted"
                      << std::endl;
            exit(1);
        }
    }
    if (translationUnit->getErrorReport().getNumIssues() != 0) {
        std::cerr << translationUnit->getErrorReport();
    }

    // ------- (optional) conversions -------------

    // conduct the bddbddb file export
    if (!Global::config().get("bddbddb").empty()) {
        try {
            if (Global::config().get("bddbddb") == "-") {
                // use STD-OUT
                toBddbddb(std::cout, *translationUnit);
            } else {
                // create an output file
                std::ofstream out(Global::config().get("bddbddb").c_str());
                toBddbddb(out, *translationUnit);
            }
        } catch (const UnsupportedConstructException& uce) {
            ERROR("failed to convert input specification into bddbddb syntax because " +
                    std::string(uce.what()));
        }
        return 0;
    }

    // ------- execution -------------

    auto ram_start = std::chrono::high_resolution_clock::now();

    /* translate AST to RAM */
    std::unique_ptr<RamProgram> ramProg =
            AstTranslator(Global::config().has("profile")).translateProgram(*translationUnit);

    if (!Global::config().get("debug-report").empty()) {
        if (ramProg) {
            auto ram_end = std::chrono::high_resolution_clock::now();
            std::string runtimeStr =
                    "(" + std::to_string(std::chrono::duration<double>(ram_end - ram_start).count()) + "s)";
            std::stringstream ramProgStr;
            ramProgStr << *ramProg;
            translationUnit->getDebugReport().addSection(DebugReporter::getCodeSection(
                    "ram-program", "RAM Program " + runtimeStr, ramProgStr.str()));
        }

        if (!translationUnit->getDebugReport().empty()) {
            std::ofstream debugReportStream(Global::config().get("debug-report"));
            debugReportStream << translationUnit->getDebugReport();
        }
    }

    if (!ramProg->getMain()) {
        return 0;
    };

    if (!Global::config().has("compile") && !Global::config().has("dl-program") &&
            !Global::config().has("generate")) {
        // ------- interpreter -------------

        // configure interpreter
        std::unique_ptr<Interpreter> interpreter =
                (Global::config().has("auto-schedule"))
                        ? std::unique_ptr<Interpreter>(new Interpreter(ScheduledExecution))
                        : std::unique_ptr<Interpreter>(new Interpreter(DirectExecution));
        std::unique_ptr<InterpreterEnvironment> env =
                interpreter->execute(translationUnit->getSymbolTable(), *ramProg);

#ifdef USE_PROVENANCE
        // only run explain interface if interpreted
        if (Global::config().has("provenance") && env != nullptr) {
            // construct SouffleProgram from env
            SouffleInterpreterInterface interface(
                    *ramProg, *interpreter, *env, translationUnit->getSymbolTable());

            if (Global::config().get("provenance") == "1") {
                explain(interface, true, false);
            } else if (Global::config().get("provenance") == "2") {
                explain(interface, true, true);
            }
        }
#endif

    } else {
        // ------- compiler -------------

        std::vector<std::unique_ptr<RamProgram>> strata;
        if (Global::config().has("stratify")) {
            if (RamSequence* sequence = dynamic_cast<RamSequence*>(ramProg->getMain())) {
                sequence->moveSubprograms(strata);
            } else {
                strata.push_back(std::move(ramProg));
            }
        } else {
            strata.push_back(std::move(ramProg));
        }

        if (Global::config().has("stratify") && Global::config().get("stratify") != "-") {
            const std::string filePath = Global::config().get("stratify");
            std::ofstream os(filePath);
            if (!os.is_open()) {
                ERROR("could not open '" + filePath + "' for writing.");
            }
            translationUnit->getAnalysis<SCCGraph>()->print(
                    os, fileExtension(Global::config().get("stratify")));
        }

        /* Locate souffle-compile script */
        std::string compileCmd = ::findTool("souffle-compile", souffleExecutable, ".");
        /* Fail if a souffle-compile executable is not found */
        if (!isExecutable(compileCmd)) {
            ERROR("failed to locate souffle-compile");
        }
        compileCmd += " ";

        int index = -1;
        std::unique_ptr<Synthesiser> synthesiser;
        for (auto&& stratum : strata) {
            if (Global::config().has("stratify")) index++;

            // configure compiler
            synthesiser = std::unique_ptr<Synthesiser>(new Synthesiser(compileCmd));
            if (Global::config().has("verbose")) {
                synthesiser->setReportTarget(std::cout);
            }
            try {
                // check if this is code generation only
                if (Global::config().has("generate")) {
                    // just generate, no compile, no execute
                    synthesiser->generateCode(translationUnit->getSymbolTable(), *stratum,
                            Global::config().get("generate"), index);

                    // check if this is a compile only
                } else if (Global::config().has("compile") && Global::config().has("dl-program")) {
                    // just compile, no execute
                    synthesiser->compileToBinary(translationUnit->getSymbolTable(), *stratum,
                            Global::config().get("dl-program"), index);
                } else {
                    // run compiled C++ program
                    synthesiser->executeBinary(translationUnit->getSymbolTable(), *stratum);
                }

            } catch (std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        }
    }

    /* Report overall run-time in verbose mode */
    if (Global::config().has("verbose")) {
        auto souffle_end = std::chrono::high_resolution_clock::now();
        std::cout << "Total Time: " << std::chrono::duration<double>(souffle_end - souffle_start).count()
                  << "sec\n";
    }

    return 0;
}

}  // end of namespace souffle

int main(int argc, char** argv) {
    return souffle::main(argc, argv);
}
