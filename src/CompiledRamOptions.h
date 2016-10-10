/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledRamOptions.h
 *
 * A header file offering command-line option support for compiled
 * RAM programs.
 *
 ***********************************************************************/

#pragma once

#include <string>
#include <iostream>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _OPENMP
    #include <omp.h>
#endif

namespace souffle {

/**
 * A utility class for parsing command line arguments within generated
 * query programs.
 */
class CmdOptions 
{
protected:
    /**
     * source file
     */
    std::string src;

    /**
     * fact directory
     */
    std::string input_dir;

    /**
     * output directory
     */
    std::string output_dir;

    /**
     * profiling flag
     */ 
    bool profiling; 

    /**
     * profile filename 
     */
    std::string profile_name;

    /**
     * The name of the sqlite database file to write to.
     */
    std::string dbFilename;

    /** 
     * number of threads
     */
    size_t num_jobs;

    /**
     * debug flag 
     */
    bool debug;

public:
    // all argument constructor
    CmdOptions(const char *s,
               const char *id,
               const char *od,
               bool pe,
               const char *pfn,
               const char *dbf,
               size_t nj,
               bool de)
        : src(s),
          input_dir(id),
          output_dir(od), 
          profiling(pe),
          profile_name(pfn),
          dbFilename(dbf),
          num_jobs(nj),
          debug(de) {}

    /**
     * get source code name 
     */
    const std::string &getSourceFileName() { return src; }

    /**
     * get input directory
     */
    const std::string &getInputFileDir() { return input_dir; }

    /** 
     * get output directory 
     */ 
    const std::string &getOutputFileDir() { return output_dir; }

    /**
     * is profiling switched on
     */ 
    bool isProfiling() { return profiling; }

    /** 
     * get filename of profile 
     */ 
    const std::string &getProfileName() { return profile_name; }

    /**
     * get filename of the output database (or empty string if no database is being used)
     */
    const std::string &getOutputDatabaseName() { return dbFilename; }

    /** 
     * get number of jobs  
     */ 
    size_t &getNumJobs() { return num_jobs; }
  
    /** 
     * is debug switch on
     */  
    bool isDebug() { return debug; }

    /**
     * Parses the given command line parameters, handles -h help requests or errors
     * and returns whether the parsing was successful or not.
     */
    bool parse(int argc, char **argv) {

        // get executable name
        std::string exec_name = "analysis";
        if (argc > 0) exec_name = argv[0];

        // local options
        std::string fact_dir = input_dir;
        std::string out_dir = output_dir;

// avoid warning due to Solaris getopt.h
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
        // long options
        option longOptions[] = {
            { "facts", true, nullptr, 'F' },
            { "output", true, nullptr, 'D' },
            { "profile", true, nullptr, 'p' },
            { "output-db", true, nullptr, 'o' },
#ifdef _OPENMP
            { "jobs", true, nullptr, 'j' },
#endif
            // the terminal option -- needs to be null
            { nullptr, false, nullptr, 0 }
        };
#pragma GCC diagnostic pop

        // check whether all options are fine
        bool ok = true;

        int c;     /* command-line arguments processing */
        while ((c = getopt_long(argc, argv, "D:F:hp:j:o:", longOptions, nullptr)) != EOF) {
            switch(c) {
                    /* Fact directories */
                case 'F':
                    if (!existDir(optarg)) {
                        printf("Fact directory %s does not exists!\n", optarg);
                        ok = false;
                    }
                    fact_dir = optarg;
                    break;
                    /* Output directory for resulting .csv files */
                case 'D':
                    if (*optarg && !existDir(optarg)) {
                        printf("Output directory %s does not exists!\n", optarg);
                        ok = false;
                    }
                    out_dir = optarg;
                    break;
                case 'p':
                    if (!profiling) {
                        std::cerr << "\nerror: profiling was not enabled in compilation\n\n";
                        printHelpPage(exec_name);
                        exit(1);
                    }
                    profile_name = optarg;
                    break;
                case 'o':
                    if (existFile(optarg)) {
                        printf("Database file %s already exists!\n", optarg);
                        ok = false;
                    }
                    dbFilename = optarg;
                    break;
#ifdef _OPENMP
                case 'j': 
                    if (std::string(optarg) == "auto") { 
                        num_jobs = 0; 
                    } else { 
                        int num = atoi(optarg);
                        if (num > 0) {
                            num_jobs = num;
                        } else {
                            std::cerr << "Invalid number of jobs [-j]: " << optarg << "\n";
                            ok = false;
                        }
                    }
                    break;
#else
                case 'j':
#endif
                case 'h':
                case '?':
                    printHelpPage(exec_name);
                    return false;
                default:
                    ASSERT("Default label in getopt switch");
            }
        }

        // update member fields
        input_dir = fact_dir;
        output_dir = out_dir;

#ifdef _OPENMP
        if (num_jobs > 0) { 
            omp_set_num_threads(num_jobs);
        } 
#endif

        // return success state
        return ok;
    }

private:

    /**
     * Prints the help page in case it has been requested or there was a typer in the command line arguments.
     */
    void printHelpPage(const std::string& exec_name) const {

        std::cerr << "====================================================================\n";
        std::cerr << " Datalog Program: " << src << "\n";
        std::cerr << " Usage: " << exec_name << " [OPTION]\n\n";
        std::cerr << " Options:\n";
        std::cerr << "    -D <DIR>, --output=<DIR>     -- Specify directory for output relations\n";
        std::cerr << "                                    (default: " << output_dir << ")\n";
        std::cerr << "                                    (suppress output with \"\")\n";
        std::cerr << "    -F <DIR>, --facts=<DIR>      -- Specify directory for fact files\n";
        std::cerr << "                                    (default: " << input_dir << ")\n";
        std::cerr << "    -o <file>, --output-db <file> -- output relations to database\n";
        if (profiling) {
            std::cerr << "    -p <file>, --profile=<file>  -- Specify filename for profiling\n";
            std::cerr << "                                    (default: " << profile_name << ")\n";
        }
#ifdef _OPENMP
        std::cerr << "    -j <NUM>, --jobs=<NUM>       -- Specify number of threads\n";
        if (num_jobs > 0) { 
            std::cerr << "                                    (default: " << num_jobs << ")\n";
        } else { 
            std::cerr << "                                    (default: auto)\n";
        } 
#endif
        std::cerr << "    -h                           -- prints this help page.\n";
        std::cerr << "--------------------------------------------------------------------\n";
        std::cerr << " Copyright (c) 2013, 2015, Oracle and/or its affiliates.\n";
        std::cerr << " All rights reserved.\n";
        std::cerr << "====================================================================\n";

    }

    /**
     *  Check whether a file exists in the file system
     */
    inline bool existFile (const std::string& name) {
        struct stat buffer;
        if (stat (name.c_str(), &buffer) == 0) {
            if ((buffer.st_mode & S_IFREG) != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     *  Check whether a directory exists in the file system
     */
    bool existDir (const std::string& name) {
        struct stat buffer;
        if (stat (name.c_str(), &buffer) == 0) {
            if ((buffer.st_mode & S_IFDIR) != 0) {
                return true;
            }
        }
        return false;
    }
};

} // end of namespace souffle

