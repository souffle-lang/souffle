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

#include "AstComponentChecker.h"
#include "AstPragma.h"
#include "AstSemanticChecker.h"
#include "AstTransforms.h"
#include "AstTranslationUnit.h"
#include "AstTranslator.h"
#include "ComponentModel.h"
#include "DebugReport.h"
#include "ErrorReport.h"
#include "Global.h"
#include "Interpreter.h"
#include "InterpreterInterface.h"
#include "ParserDriver.h"
#include "PrecedenceGraph.h"
#include "RamProgram.h"
#include "RamSemanticChecker.h"
#include "RamTransformer.h"
#include "RamTranslationUnit.h"
#include "SymbolTable.h"
#include "Synthesiser.h"
#include "Util.h"
#include "config.h"
#include "profile/Tui.h"

#ifdef USE_PROVENANCE
#include "Explain.h"
#endif

#ifdef USE_MPI
#include "Mpi.h"
#endif

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Executes a binary file.
 */
void executeBinary(const std::string& binaryFilename
#ifdef USE_MPI
        ,
        const int numberOfProcesses
#endif
) {
    assert(!binaryFilename.empty() && "binary filename cannot be blank");

    // check whether the executable exists
    if (!isExecutable(binaryFilename)) {
        throw std::invalid_argument("Generated executable <" + binaryFilename + "> could not be found");
    }

    // run the executable
    int exitCode;
#ifdef USE_MPI
    if (Global::config().get("engine") == "mpi") {
        std::stringstream ss;
        ss << "mpiexec";
        if (Global::config().has("hostfile")) {
            ss << " --hostfile " << Global::config().get("hostfile");
        }
        ss << " -n " << std::to_string(numberOfProcesses);
        ss << " " << binaryFilename;
        exitCode = system(ss.str().c_str());
    } else
#endif
    {
        exitCode = system(binaryFilename.c_str());
    }

    if (Global::config().get("dl-program").empty()) {
        remove(binaryFilename.c_str());
        remove((binaryFilename + ".cpp").c_str());
    }

    // exit with same code as executable
    if (exitCode != 0) {
        exit(exitCode);
    }
}

/**
 * Compiles the given source file to a binary file.
 */
void compileToBinary(std::string compileCmd, const std::string& sourceFilename) {
    // add source code
    compileCmd += sourceFilename;

    // run executable
    if (system(compileCmd.c_str()) != 0) {
        throw std::invalid_argument("failed to compile C++ source <" + sourceFilename + ">");
    }
}

int main(int argc, char** argv) {
    /* Time taking for overall runtime */
    auto souffle_start = std::chrono::high_resolution_clock::now();

#ifdef USE_MPI
    mpi::init(argc, argv);
    if (mpi::commRank() != 0) {
        throw std::runtime_error("Error: Souffle can only be run with one MPI process.");
    }
#endif

    /* have all to do with command line arguments in its own scope, as these are accessible through the global
     * configuration only */
    try {
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
                    footer << "Copyright (c) 2016-18 The Souffle Developers." << std::endl;
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
                            {"", 0, "", "", false, ""},  // the datalog program itself, key is always empty
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
                            {"generate", 'g', "FILE", "", false,
                                    "Generate C++ source code for the given Datalog program and write it to "
                                    "<FILE>."},
                            {"no-warn", 'w', "", "", false, "Disable warnings."},
                            {"magic-transform", 'm', "RELATIONS", "", false,
                                    "Enable magic set transformation changes on the given relations, use '*' "
                                    "for all."},
                            {"macro", 'M', "MACROS", "", false,
                                    "Set macro definitions for the pre-processor"},
                            {"disable-transformers", 'z', "TRANSFORMERS", "", false,
                                    "Disable the given AST transformers."},
                            {"dl-program", 'o', "FILE", "", false,
                                    "Generate C++ source code, written to <FILE>, and compile this to a "
                                    "binary executable (without executing it)."},
                            {"live-profile", 'l', "", "", false, "Enable live profiling."},
                            {"profile", 'p', "FILE", "", false,
                                    "Enable profiling, and write profile data to <FILE>."},
                            {"profile-use", 'u', "FILE", "", false,
                                    "Use profile log-file <FILE> for profile-guided optimization."},
                            {"debug-report", 'r', "FILE", "", false, "Write HTML debug report to <FILE>."},
                            {"pragma", 'P', "OPTIONS", "", false, "Set pragma options."},
#ifdef USE_PROVENANCE
                            {"provenance", 't', "[ none | explain | explore ]", "", false,
                                    "Enable provenance instrumentation and interaction."},
#endif
                            {"engine", 'e', "[ file | mpi ]", "", false,
                                    "Specify communication engine for distributed execution."},
                            {"hostfile", '\1', "FILE", "", false,
                                    "Specify --hostfile option for call to mpiexec when using mpi as "
                                    "execution engine."},
                            {"verbose", 'v', "", "", false, "Verbose output."},
                            {"version", '\2', "", "", false, "Version."},
                            {"help", 'h', "", "", false, "Display this help message."}};
                    return std::vector<MainOption>(std::begin(opts), std::end(opts));
                }());

        // ------ command line arguments -------------

        /* for the version option, if given print the version text then exit */
        if (Global::config().has("version")) {
            std::cout << "Souffle: " << PACKAGE_VERSION << "" << std::endl;
            std::cout << "Copyright (c) 2016-18 The Souffle Developers." << std::endl;
            std::cout << "Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
            return 0;
        }
        Global::config().set("version", PACKAGE_VERSION);

        /* for the help option, if given simply print the help text then exit */
        if (!Global::config().has("") || Global::config().has("help")) {
            std::cout << Global::config().help();
            return 0;
        }

        /* check that datalog program exists */
        if (!existFile(Global::config().get(""))) {
            throw std::runtime_error("cannot open file " + std::string(Global::config().get("")));
        }

        /* for the jobs option, to determine the number of threads used */
        if (Global::config().has("jobs")) {
#ifdef _OPENMP
            if (isNumber(Global::config().get("jobs").c_str())) {
                if (std::stoi(Global::config().get("jobs")) < 1) {
                    throw std::runtime_error(
                            "Number of jobs in the -j/--jobs options must be greater than zero!");
                }
            } else {
                if (!Global::config().has("jobs", "auto")) {
                    throw std::runtime_error(
                            "Wrong parameter " + Global::config().get("jobs") + " for option -j/--jobs!");
                }
                Global::config().set("jobs", "0");
            }
#else
            // Check that -j option has not been changed from the default
            if (Global::config().get("jobs") != "1") {
                std::cerr << "\nWarning: OpenMP is not enabled\n";
            }
#endif
        } else {
            throw std::runtime_error(
                    "Wrong parameter " + Global::config().get("jobs") + " for option -j/--jobs!");
        }

        /* if an output directory is given, check it exists */
        if (Global::config().has("output-dir") && !Global::config().has("output-dir", "-") &&
                !existDir(Global::config().get("output-dir")) &&
                !(Global::config().has("generate") ||
                        (Global::config().has("dl-program") && !Global::config().has("compile")))) {
            throw std::runtime_error(
                    "output directory " + Global::config().get("output-dir") + " does not exists");
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

        /* collect all macro definitions for the pre-processor */
        if (Global::config().has("macro")) {
            std::string currentMacro = "";
            std::string allMacros = "";
            for (const char& ch : Global::config().get("macro")) {
                if (ch == ' ') {
                    allMacros += " -D";
                    allMacros += currentMacro;
                    currentMacro = "";
                } else {
                    currentMacro += ch;
                }
            }
            allMacros += " -D" + currentMacro;
            Global::config().set("macro", allMacros);
        }

        /* turn on compilation of executables */
        if (Global::config().has("dl-program")) {
            Global::config().set("compile");
        }

        /* disable provenance with engine option */
        if (Global::config().has("provenance")) {
            if (Global::config().has("engine")) {
                throw std::runtime_error("provenance cannot be enabled with distributed execution.");
            }
        }

        /* ensure that souffle has been compiled with support for the execution engine, if specified */
        if (Global::config().has("engine")) {
            if (!(Global::config().has("compile") || Global::config().has("dl-program") ||
                        Global::config().has("generate"))) {
                throw std::invalid_argument("Error: Use of engine option not yet available for interpreter.");
            }
            const auto& engine = Global::config().get("engine");
            if (engine != "file" && engine != "mpi") {
                throw std::invalid_argument("Error: Use of engine '" + engine + "' is not supported.");
            }
#ifndef USE_MPI
            if (engine == "mpi") {
                throw std::invalid_argument("Error: Use of engine '" + engine +
                                            "' requires configure option '--enable-" + engine + "'.");
            }
            if (Global::config().has("hostfile")) {
                throw std::invalid_argument(
                        "Error: Use of hostfile option requires configure option '--enable-" + engine + "'.");
            }
#else
            if (engine != "mpi" && Global::config().has("hostfile")) {
                throw std::invalid_argument(
                        "Error: Use of hostfile option requires execution engine '" + engine + "'.");
            }
#endif
        }

        if (Global::config().has("live-profile") && !Global::config().has("profile")) {
            Global::config().set("profile");
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    // ------ start souffle -------------

    std::string souffleExecutable = which(argv[0]);

    if (souffleExecutable.empty()) {
        throw std::runtime_error("failed to determine souffle executable path");
    }

    /* Create the pipe to establish a communication between cpp and souffle */
    std::string cmd = ::which("mcpp");

    if (!isExecutable(cmd)) {
        throw std::runtime_error("failed to locate mcpp pre-processor");
    }

    cmd += " -e utf8 -W0 " + Global::config().get("include-dir");
    if (Global::config().has("macro")) {
        cmd += " " + Global::config().get("macro");
    }
    cmd += " " + Global::config().get("");
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
                    std::make_unique<MagicSetTransformer>(), std::make_unique<ResolveAliasesTransformer>(),
                    std::make_unique<RemoveRelationCopiesTransformer>(),
                    std::make_unique<RemoveEmptyRelationsTransformer>(),
                    std::make_unique<RemoveRedundantRelationsTransformer>()));

    // Equivalence pipeline
    auto equivalencePipeline =
            std::make_unique<PipelineTransformer>(std::make_unique<MinimiseProgramTransformer>(),
                    std::make_unique<RemoveRelationCopiesTransformer>(),
                    std::make_unique<RemoveEmptyRelationsTransformer>(),
                    std::make_unique<RemoveRedundantRelationsTransformer>());

#ifdef USE_PROVENANCE
    // Provenance pipeline
    auto provenancePipeline = std::make_unique<PipelineTransformer>(std::make_unique<ConditionalTransformer>(
            Global::config().has("provenance"), std::make_unique<ProvenanceTransformer>()));
#else
    auto provenancePipeline = std::make_unique<PipelineTransformer>();
#endif

    // Main pipeline
    auto pipeline = std::make_unique<PipelineTransformer>(std::make_unique<AstComponentChecker>(),
            std::make_unique<ComponentInstantiationTransformer>(),
            std::make_unique<UniqueAggregationVariablesTransformer>(), std::make_unique<AstSemanticChecker>(),
            std::make_unique<RemoveBooleanConstraintsTransformer>(),
            std::make_unique<ResolveAliasesTransformer>(), std::make_unique<MinimiseProgramTransformer>(),
            std::make_unique<InlineRelationsTransformer>(), std::make_unique<ResolveAliasesTransformer>(),
            std::make_unique<RemoveRedundantRelationsTransformer>(),
            std::make_unique<RemoveRelationCopiesTransformer>(),
            std::make_unique<RemoveEmptyRelationsTransformer>(),
            std::make_unique<ReplaceSingletonVariablesTransformer>(),
            std::make_unique<FixpointTransformer>(
                    std::make_unique<PipelineTransformer>(std::make_unique<ReduceExistentialsTransformer>(),
                            std::make_unique<RemoveRedundantRelationsTransformer>())),
            std::make_unique<RemoveRelationCopiesTransformer>(),
            std::make_unique<PartitionBodyLiteralsTransformer>(),
            std::make_unique<MinimiseProgramTransformer>(),
            std::make_unique<RemoveRelationCopiesTransformer>(),
            std::make_unique<ReorderLiteralsTransformer>(),
            std::make_unique<MaterializeAggregationQueriesTransformer>(),
            std::make_unique<RemoveEmptyRelationsTransformer>(),
            std::make_unique<ReorderLiteralsTransformer>(), std::move(magicPipeline),
            std::make_unique<AstExecutionPlanChecker>(), std::move(provenancePipeline));

    // Disable unwanted transformations
    if (Global::config().has("disable-transformers")) {
        std::vector<std::string> givenTransformers =
                splitString(Global::config().get("disable-transformers"), ',');
        pipeline->disableTransformers(
                std::set<std::string>(givenTransformers.begin(), givenTransformers.end()));
    }

    // Set up the debug report if necessary
    if (!Global::config().get("debug-report").empty()) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::string runtimeStr =
                "(" + std::to_string(std::chrono::duration<double>(parser_end - parser_start).count()) + "s)";
        DebugReporter::generateDebugReport(*astTranslationUnit, "Parsing", "After Parsing " + runtimeStr);

        pipeline->setDebugReport();
    }

    // Toggle pipeline verbosity
    pipeline->setVerbosity(Global::config().has("verbose"));

    // Apply all the transformations
    pipeline->apply(*astTranslationUnit);

    // ------- execution -------------

    /* translate AST to RAM */
    std::unique_ptr<RamTranslationUnit> ramTranslationUnit =
            AstTranslator().translateUnit(*astTranslationUnit);

    std::vector<std::unique_ptr<RamTransformer>> ramTransforms;
    ramTransforms.push_back(std::make_unique<RamSemanticChecker>());

    for (const auto& transform : ramTransforms) {
        transform->apply(*ramTranslationUnit);

        /* Abort evaluation of the program if errors were encountered */
        if (ramTranslationUnit->getErrorReport().getNumErrors() != 0) {
            std::cerr << ramTranslationUnit->getErrorReport();
            std::cerr << std::to_string(ramTranslationUnit->getErrorReport().getNumErrors()) +
                                 " errors generated, evaluation aborted"
                      << std::endl;
            exit(1);
        }
    }
    if (ramTranslationUnit->getErrorReport().getNumIssues() != 0) {
        std::cerr << ramTranslationUnit->getErrorReport();
    }

    if (!ramTranslationUnit->getProgram()->getMain()) {
        return 0;
    };

    if (!Global::config().has("compile") && !Global::config().has("dl-program") &&
            !Global::config().has("generate")) {
        // ------- interpreter -------------

        // configure interpreter
        std::unique_ptr<Interpreter> interpreter = std::make_unique<Interpreter>(*ramTranslationUnit);

        std::thread profiler;
        // Start up profiler if needed
        if (Global::config().has("live-profile") && !Global::config().has("compile")) {
            profiler = std::thread([]() { profile::Tui().runProf(); });
        }
        // execute translation unit
        interpreter->executeMain();

        // If the profiler was started, join back here once it exits.
        if (profiler.joinable()) {
            profiler.join();
        }

#ifdef USE_PROVENANCE
        // only run explain interface if interpreted
        if (Global::config().has("provenance")) {
            // construct SouffleProgram from env
            InterpreterProgInterface interface(*interpreter);
            if (Global::config().get("provenance") == "explain") {
                explain(interface, true, false);
            } else if (Global::config().get("provenance") == "explore") {
                explain(interface, true, true);
            }
        }
#endif

    } else {
        // ------- compiler -------------

        std::string compileCmd = ::findTool("souffle-compile", souffleExecutable, ".");
        /* Fail if a souffle-compile executable is not found */
        if (!isExecutable(compileCmd)) {
            throw std::runtime_error("failed to locate souffle-compile");
        }
        compileCmd += " ";

        std::unique_ptr<Synthesiser> synthesiser = std::make_unique<Synthesiser>();

        try {
            // Find the base filename for code generation and execution
            std::string baseFilename;
            if (Global::config().has("dl-program")) {
                baseFilename = Global::config().get("dl-program");
            } else if (Global::config().has("generate")) {
                baseFilename = Global::config().get("generate");
                // trim .cpp extension if it exists
                if (baseFilename.size() >= 4 && baseFilename.substr(baseFilename.size() - 4) == ".cpp") {
                    baseFilename = baseFilename.substr(0, baseFilename.size() - 4);
                }
            } else {
                baseFilename = tempFile();
            }
            if (baseName(baseFilename) == "/" || baseName(baseFilename) == ".") {
                baseFilename = tempFile();
            }

            std::string baseIdentifier = identifier(simpleName(baseFilename));
            std::string sourceFilename = baseFilename + ".cpp";

            bool withSharedLibrary;
            std::ofstream os(sourceFilename);
            synthesiser->generateCode(*ramTranslationUnit, os, baseIdentifier, withSharedLibrary);
            os.close();

            if (withSharedLibrary) {
                compileCmd += "-s ";
            }

            if (Global::config().has("compile")) {
                auto start = std::chrono::high_resolution_clock::now();
                compileToBinary(compileCmd, sourceFilename);
                /* Report overall run-time in verbose mode */
                if (Global::config().has("verbose")) {
                    auto end = std::chrono::high_resolution_clock::now();
                    std::cout << "Compilation Time: " << std::chrono::duration<double>(end - start).count()
                              << "sec\n";
                }
                // run compiled C++ program if requested.
                if (!Global::config().has("dl-program")) {
                    executeBinary(baseFilename
#ifdef USE_MPI
                            ,
                            ((int)astTranslationUnit->getAnalysis<SCCGraph>()->getNumberOfSCCs()) + 1
#endif
                    );
                }
            }
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            std::exit(1);
        }
    }

// finalize mpi, this is necessary for the symbol table
#ifdef USE_MPI
    mpi::finalize();
#endif

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
