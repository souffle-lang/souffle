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

#include <config.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <getopt.h>

#include <chrono>
#include <list>
#include <iostream>
#include <fstream>
#include <string>

#include "AstProgram.h"
#include "AstUtils.h"
#include "AstTuner.h"
#include "AstAnalysis.h"
#include "AstTranslationUnit.h"
#include "AstTransformer.h"
#include "AstSemanticChecker.h"
#include "AstTransforms.h"
#include "BddbddbBackend.h"
#include "PrecedenceGraph.h"
#include "Util.h"
#include "SymbolTable.h"
#include "ParserDriver.h"
#include "ComponentModel.h"
#include "Environment.h"

#include "RamTranslator.h"
#include "RamExecutor.h"
#include "RamStatement.h"

namespace souffle {

void fail(const std::string& str) {
    // print error message
    std::cerr << str << "\n";
    exit(1);
}

static void wrapPassesForDebugReporting(std::vector<std::unique_ptr<AstTransformer>> &transforms) {
    for (unsigned int i = 0; i < transforms.size(); i++) {
        transforms[i] = std::unique_ptr<AstTransformer>(new DebugReporter(std::move(transforms[i])));
    }
}

int main(int argc, char **argv)
{
    /* Time taking for overall runtime */
    auto souffle_start = std::chrono::high_resolution_clock::now();

    Environment env = Environment(
        argc,
        argv,
        []() {
            std::stringstream header;
            header << "=======================================================================================================" << std::endl;
            header << "souffle -- Oracle Labs' datalog engine." << std::endl;
            header << "Usage: souffle [OPTION] FILE." << std::endl;
            header << "-------------------------------------------------------------------------------------------------------" << std::endl;
            header << "Options:" << std::endl;
            return header.str();
        }(),
        []() {
            std::stringstream footer;
            footer << "-------------------------------------------------------------------------------------------------------" << std::endl;
            footer << "Version: " << PACKAGE_VERSION << "" << std::endl;
            footer << "-------------------------------------------------------------------------------------------------------" << std::endl;
            footer << "Copyright (c) 2013, 2015, Oracle and/or its affiliates." << std::endl;
            footer << "All rights reserved." << std::endl;
            footer << "=======================================================================================================" << std::endl;
            return footer.str();
        }(),
        /* new command line options, the environment will be filled with the arguments passed to them, or the empty string if they take none */
        []() {
            Option opts[] = {

                /* each option is { long option, short option, argument name, default value, description } */
                {"fact-dir",        'F', "DIR",     ".",    "Specify directory for fact files."},
                {"include-dir",     'I', "DIR",     ".",    "Specify directory for include files."},
                {"output-dir",      'D', "DIR",     ".",    "Specify directory for output relations (if <DIR> is -, output is written to stdout)."},
                {"jobs",            'j', "N",       "1",    "Run interpreter/compiler in parallel using N threads, N=auto for system default."},
                {"compile",         'c', "",        "",     "Compile datalog (translating to C++)."},
                // if the short option is non-alphabetical, it is ommitted from the help text
                {"auto-schedule",     1,  "",       "",     "Switch on automated clause scheduling for compiler."},
                {"generate",        'g', "FILE",    "",     "Only generate sources of compilable analysis and write it to <FILE>."},
                {"no-warn",         'w', "",        "",     "Disable warnings."},
                {"dl-program",      'o', "FILE",    "",     "Write executable program to <FILE> (without executing it)."},
                {"profile",         'p', "FILE",    "",     "Enable profiling and write profile data to <FILE>."},
                {"debug",           'd', "",        "",     "Enable debug mode."},
                {"bddbddb",         'b', "FILE",    "",     "Convert input into bddbddb file format."},
                // if the short option is non-alphabetical, it is ommitted from the help text
                {"debug-report",      2, "FILE",    "",     "Write debugging output to HTML report."},
                {"verbose",         'v', "",        "",     "Verbose output."},
                {"help",            'h', "",        "",     "Display this help message."},

                // TODO: uncomment to allow breadth and depth limits as command line parameters, must also do so below
                // {"breadth-limit",   3,   "N",       "2",    "Specify the breadth limit used for the topological ordering of strongly connected components."},
                // {"depth-limit",     4,   "N",       "2",    "Specify the depth limit used for the topological ordering of strongly connected components."}

            };
            return std::vector<Option>(std::begin(opts), std::end(opts));
        }()
    );

    // ------ command line arguments -------------

    /* for the help option, if given simply print the help text then exit */
    if (env.has("help")) {
        env.printOptions(std::cerr);
        return 1;
   }

   /* turn on compilation of executables */
   if (env.has("dl-program"))
        env.set("compile");

    /* for the jobs option, to determine the number of threads used */
    static unsigned num_threads = 1; /* the number of threads to use for execution, 0 system-choice, 1 sequential */
    if (env.has("jobs")) {
        if (env.has("jobs", "auto")) {
            num_threads = 0;
        } else if (isNumber(env.get("jobs").c_str())) {
            num_threads = atoi(env.get("jobs").c_str());
            if (num_threads == 0) {
               fail("Number of jobs in the -j/--jobs options must be greater than zero!");

            }
        } else {
            fail("Wrong parameter " + env.get("jobs") + " for option -j/--jobs!");
        }
    }

    /* if an output directory is given, check it exists */
    if (env.has("output-dir") && !env.has("output-dir", "-") && !existDir(env.get("output-dir")))
        fail("error: output directory " + env.get("output-dir") + " does not exists");

    /* turn on compilation if auto-scheduling is enabled */
    if (env.has("auto-schedule") && !env.has("compile"))
        env.set("compile");

    /* ensure that if auto-scheduling is enabled an output file is given */
    if (env.has("auto-schedule") && !env.has("dl-program"))
       fail("error: no executable is specified for auto-scheduling (option -o <FILE>)");

    // TODO: uncomment to allow breadth and depth limits as command line parameters, must also do so above
    /* set the breadth and depth limits for the topological ordering of strongly connected components */
    // if (env.has("breadth-limit")) {
    //    int limit = std::stoi(env.get("breadth-limit"));
    //    if (limit <= 0)
    //        fail("error: breadth limit must be a natural number");
    //    TopologicallySortedSCCGraph::BREADTH_LIMIT = limit;
    // }
    // if (env.has("depth-limit")) {
    //     int limit = std::stoi(env.get("depth-limit"));
    //     if (limit <= 0)
    //         fail("error: depth limit must be a natural number");
    //     TopologicallySortedSCCGraph::DEPTH_LIMIT = limit;
    // }

    /* collect all input directories for the c pre-processor */
    if (env.has("include-dir")) {
        std::string currentInclude = "";
        std::string allIncludes = "";
        env.set("include-dir");
        for (const char& ch : env.get("include-dir")) {
            if (ch == ' ') {
                if (!existDir(currentInclude)) {
                    fail("error: include directory " + currentInclude + " does not exists");
                } else {
                    allIncludes += " -I ";
                    allIncludes += currentInclude;
                }
            } else {
                currentInclude += ch;
            }
        }
        env.set("include-dir", allIncludes);
    }

    /* collect all input files for the C pre-processor */
    std::string filenames = "";
    if (optind < argc) {
        for (; optind < argc; optind++) {
            if (!existFile(argv[optind])) {
                fail("error: cannot open file " + std::string(argv[optind]));
            }
            if (filenames == "") {
                filenames = argv[optind];
            } else {
                filenames = filenames + " " + std::string(argv[optind]);
            }
        }
    } else {
        env.error();
    }

    // ------ start souffle -------------

    std::string programName = which(argv[0]);
    if (programName.empty())
        fail("error: failed to determine souffle executable path");

    /* Create the pipe to establish a communication between cpp and souffle */
    std::string cmd = ::findTool("souffle-mcpp", programName, ".");

    if (!isExecutable(cmd))
        fail("error: failed to locate souffle preprocessor");

    cmd  += " " + env.get("include-dir") + " " + filenames;
    FILE* in = popen(cmd.c_str(), "r");

    /* Time taking for parsing */
    auto parser_start = std::chrono::high_resolution_clock::now();

    // ------- parse program -------------

    // parse file
    std::unique_ptr<AstTranslationUnit> translationUnit = ParserDriver::parseTranslationUnit("<stdin>", in, env.has("no-warn"));

    // close input pipe
    int preprocessor_status = pclose(in);
    if (preprocessor_status == -1) {
        perror(NULL);
        fail("error: failed to close pre-processor pipe");
    }

    /* Report run-time of the parser if verbose flag is set */
    if (env.has("verbose")) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::cout << "Parse Time: " << std::chrono::duration<double>(parser_end-parser_start).count()<< "sec\n";
    }

    // ------- check for parse errors -------------
    if (translationUnit->getErrorReport().getNumErrors() != 0) {
        std::cerr << translationUnit->getErrorReport();
        fail(std::to_string(translationUnit->getErrorReport().getNumErrors()) + " errors generated, evaluation aborted");
    }

    // ------- rewriting / optimizations -------------

    std::vector<std::unique_ptr<AstTransformer>> transforms;
    transforms.push_back(std::unique_ptr<AstTransformer>(new ComponentInstantiationTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new UniqueAggregationVariablesTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new AstSemanticChecker()));
    if (env.get("bddbddb").empty()) {
    	transforms.push_back(std::unique_ptr<AstTransformer>(new ResolveAliasesTransformer()));
    }
    transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveRelationCopiesTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new MaterializeAggregationQueriesTransformer()));
    transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveEmptyRelationsTransformer()));
    if (!env.has("debug")) {
        transforms.push_back(std::unique_ptr<AstTransformer>(new RemoveRedundantRelationsTransformer()));
    }
    transforms.push_back(std::unique_ptr<AstTransformer>(new AstExecutionPlanChecker()));
    if (env.has("auto-schedule")) {
        transforms.push_back(std::unique_ptr<AstTransformer>(new AutoScheduleTransformer(env.get("fact-dir"), env.has("verbose"), !env.get("debug-report").empty())));
    }
    if (!env.get("debug-report").empty()) {
        auto parser_end = std::chrono::high_resolution_clock::now();
        std::string runtimeStr = "(" + std::to_string(std::chrono::duration<double>(parser_end-parser_start).count()) + "s)";
        DebugReporter::generateDebugReport(*translationUnit, "Parsing", "After Parsing " + runtimeStr);
        wrapPassesForDebugReporting(transforms);
    }

    for (const auto &transform : transforms) {
        transform->apply(*translationUnit);

        /* Abort evaluation of the program if errors were encountered */
        if (translationUnit->getErrorReport().getNumErrors() != 0) {
            std::cerr << translationUnit->getErrorReport();
            fail(std::to_string(translationUnit->getErrorReport().getNumErrors()) + " errors generated, evaluation aborted");
        }
    }
    if (translationUnit->getErrorReport().getNumIssues() != 0) {
        std::cerr << translationUnit->getErrorReport();
    }

    // ------- (optional) conversions -------------

    // conduct the bddbddb file export
    if (!env.get("bddbddb").empty()) {
    	try {
			if (env.get("bddbddb") == "-") {
				// use STD-OUT
				toBddbddb(std::cout,*translationUnit);
			} else {
				// create an output file
				std::ofstream out(env.get("bddbddb").c_str());
				toBddbddb(out,*translationUnit);
			}
    	} catch(const UnsupportedConstructException& uce) {
    		std::cout << "Failed to convert input specification into bddbddb syntax:\n";
    		std::cout << "Reason: " << uce.what();
            return 1;
    	}
    	return 0;
    }

    // ------- execution -------------

    auto ram_start = std::chrono::high_resolution_clock::now();

    /* translate AST to RAM */
    std::unique_ptr<RamStatement> ramProg = RamTranslator(env.has("profile")).translateProgram(*translationUnit);

    if (!env.get("debug-report").empty()) {
        if (ramProg) {
            auto ram_end = std::chrono::high_resolution_clock::now();
            std::string runtimeStr = "(" + std::to_string(std::chrono::duration<double>(ram_end-ram_start).count()) + "s)";
            std::stringstream ramProgStr;
            ramProgStr << *ramProg;
            translationUnit->getDebugReport().addSection(DebugReporter::getCodeSection("ram-program", "RAM Program " + runtimeStr, ramProgStr.str()));
        }

        if (!translationUnit->getDebugReport().empty()) {
            std::ofstream debugReportStream(env.get("debug-report"));
            debugReportStream << translationUnit->getDebugReport();
        }
    }

    /* run RAM program */
    if (!ramProg)
        return 0;

    // pick executor

    std::unique_ptr<RamExecutor> executor;
    if (env.has("generate") || env.has("compile")) {
        // configure compiler
        executor = std::unique_ptr<RamExecutor>(new RamCompiler(env.get("dl-program")));
        if (env.has("verbose")) {
           executor -> setReportTarget(std::cout);
        }
    } else {
        // configure interpreter
        if (env.has("auto-schedule")) {
            executor = std::unique_ptr<RamExecutor>(new RamGuidedInterpreter());
        } else {
            executor = std::unique_ptr<RamExecutor>(new RamInterpreter());
        }
    }

    // configure executor
    auto& config = executor->getConfig();
    config.setSourceFileName(filenames);
    config.setFactFileDir(env.get("fact-dir"));
    config.setOutputDir(env.get("output-dir"));
    config.setNumThreads(num_threads);
    config.setLogging(env.has("profile"));
    config.setProfileName(env.get("profile"));
    config.setDebug(env.has("debug"));

    /* Locate souffle-compile script */
    std::string compileCmd = ::findTool("souffle-compile", programName, ".");

    /* Fail if a souffle-compile executable is not found */
    if (!isExecutable(compileCmd))
        fail("error: failed to locate souffle-compile");

    config.setCompileScript(compileCmd + " ");

    // check if this is code generation only
    if (env.has("generate")) {

    	// just generate, no compile, no execute
		static_cast<const RamCompiler*>(executor.get())->generateCode(translationUnit->getSymbolTable(), *ramProg, env.get("generate"));

    	// check if this is a compile only
    } else if (env.has("compile") && env.has("dl-program")) {
        // just compile, no execute
        static_cast<const RamCompiler*>(executor.get())->compileToBinary(translationUnit->getSymbolTable(), *ramProg);
    } else {
        // run executor
        executor->execute(translationUnit->getSymbolTable(), *ramProg);
    }

    /* Report overall run-time in verbose mode */
    if (env.has("verbose")) {
        auto souffle_end = std::chrono::high_resolution_clock::now();
        std::cout << "Total Time: " << std::chrono::duration<double>(souffle_end-souffle_start).count() << "sec\n";
    }

    return 0;
}

} // end of namespace souffle

int main(int argc, char **argv) {
    return souffle::main(argc, argv);
}


