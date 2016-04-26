/*
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights reserved
 *
 * The Universal Permissive License (UPL), Version 1.0
 *
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 *
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 *
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 *
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/************************************************************************
 *
 * @file RamExecutor.h
 *
 * Declares entities capable of executing a RAM program.
 *
 ***********************************************************************/

#pragma once

#include <string>
#include <map>
#include <functional>

#include "RamRelation.h"

namespace souffle {

/** forward declaration */
class RamStatement;
class RamInsert;

/**
 * The general configuration covering configurable details of
 * the execution of a RAM program.
 */
class RamExecutorConfig {

    /** The name of the DATALOG source file */
    std::string sourceFileName;

    /** The directory utilized for loading fact files */
    std::string factFileDir;

    /** The directory to store output files to */
    std::string outputDir;

    /** The number of threads to be used for the computation (0 = parallel system default) */
    size_t num_threads;

    /** A flag for enabling logging during the computation */
    bool logging;

    /** The name of the compile script */
    std::string compileScript;

    /** A filename for profile log */
    std::string profileName;

    /** A flag for enabling debug mode */
    bool debug;

    /** A flag for suppressing warnings */
    bool quiet;

    /** Name of the CXX compiler specified by the user */	
    std::string cxx;

public:

    RamExecutorConfig() : sourceFileName("-unknown-"), factFileDir("./"), outputDir("./"), num_threads(1), logging(false), compileScript("souffle-compile"), debug(false), quiet(false), cxx("") {}

    // -- getters and setters --

    void setCXX(const std::string& name) {
        cxx = name;
    }

    const std::string& getCXX() const {
        return cxx;
    }

    void setSourceFileName(const std::string& name) {
        sourceFileName = name;
    }

    const std::string& getSourceFileName() const {
        return sourceFileName;
    }

    void setFactFileDir(const std::string& dir) {
        factFileDir = dir;
    }

    const std::string& getFactFileDir() const {
        return factFileDir;
    }

    void setOutputDir(const std::string& dir) {
        outputDir = dir;
    }

    const std::string& getOutputDir() const {
        return outputDir;
    }

    void setNumThreads(size_t num) {
        num_threads = num;
    }

    size_t getNumThreads() const {
        return num_threads;
    }

    bool isParallel() const {
        return num_threads != 1;
    }

    void setLogging(bool val = true) {
        logging = val;
    }

    bool isLogging() const {
        return logging;
    }

    void setCompileScript(const std::string& script) {
        compileScript = script;
    }

    const std::string& getCompileScript() const {
        return compileScript;
    }

    void setProfileName(const std::string& name) {
        profileName = name;
    }

    const std::string& getProfileName() const {
        return profileName;
    }

    void setQuiet(bool val = true) {
        quiet = val;
    }

    bool isQuiet() const {
        return quiet;
    }

    void setDebug(bool val = true) {
        debug = val;
    }

    bool isDebug() const {
        return debug;
    }

};

/**
 * An abstract base class for entities capable of processing a RAM program.
 */
class RamExecutor {

    /** The associated configuration */
    RamExecutorConfig config;

protected: 
    /** An optional stream to print logging information to */
    std::ostream* report;

public:
    using SymbolTable = souffle::SymbolTable; // XXX pending namespace cleanup
    RamExecutor() : report(nullptr) {}

    /** A virtual destructor to support safe inheritance */
    virtual ~RamExecutor() {}

    // -- getters and setters --

    void setConfig(const RamExecutorConfig& config) {
        this->config = config;
    }

    RamExecutorConfig& getConfig() {
        return config;
    }

    const RamExecutorConfig& getConfig() const {
        return config;
    }

    /**
     * Updates the target this executor is reporting to.
     */
    void setReportTarget(std::ostream& report) {
        this->report = &report;
    }

    /**
     * Disables the reporting. No more report messages will be printed.
     */
    void disableReporting() {
        report = nullptr;
    }

    // -- actual evaluation --

    /**
     * Runs the given RAM statement on an empty environment and returns
     * this environment after the completion of the execution.
     */
    RamEnvironment execute(SymbolTable& table, const RamStatement& stmt) const {
        RamEnvironment env(table);
        applyOn(stmt, env);
        return env;
    }

    /**
     * Runs the given statement on the given environment.
     */
    virtual void applyOn(const RamStatement& stmt, RamEnvironment& env) const = 0;

};

/**
 * A class representing the order of elements.
 */
class Order {

    /** The covered order */
    std::vector<unsigned> order;

public:

    static Order getIdentity(unsigned size) {
        Order res;
        for(unsigned i = 0; i<size; i++) {
            res.append(i);
        }
        return res;
    }

    void append(unsigned pos) {
        order.push_back(pos);
    }

    unsigned operator[](unsigned index) const {
        return order[index];
    }

    std::size_t size() const {
        return order.size();
    }

    bool isComplete() const {
        for(size_t i = 0; i<order.size(); i++) {
            if (!contains(order, i)) return false;
        }
        return true;
    }

    const std::vector<unsigned> &getOrder() const {
        return order;
    }

    void print(std::ostream& out) const {
        out << order;
    }

    friend std::ostream& operator<<(std::ostream& out, const Order& order) {
        order.print(out);
        return out;
    }
};

/**
 * The summary to be returned from a statement executor.
 */
struct ExecutionSummary {
    Order order;
    long time;
};


/** Defines the type of execution strategies */
typedef std::function<
        ExecutionSummary(const RamExecutorConfig& config, const RamInsert&, RamEnvironment& env, std::ostream*)
>  QueryExecutionStrategy;


// -- example strategies --

/** With this strategy queries will be processed as they are stated by the user */
extern const QueryExecutionStrategy DirectExecution;

/** With this strategy queries will be dynamically rescheduled before each execution */
extern const QueryExecutionStrategy ScheduledExecution;


/**
 * An interpreter based implementation of a RAM executor. The RAM program will
 * be processed within the callers process. Before every query operation, an
 * optional scheduling step will be conducted.
 */
class RamGuidedInterpreter : public RamExecutor {

    /** The executor processing a query */
    QueryExecutionStrategy queryStrategy;

public:

    /** A constructor accepting a query executor strategy */
    RamGuidedInterpreter(const QueryExecutionStrategy& queryStrategy = ScheduledExecution)
        : queryStrategy(queryStrategy) {}

    /**
     * The implementation of the interpreter applying the given program
     * on the given environment.
     */
    virtual void applyOn(const RamStatement& stmt, RamEnvironment& env) const;

};


/**
 * An interpreter based implementation of a RAM executor. The RAM program will
 * be processed within the callers process. In this version, no scheduling
 * will be conducted.
 */
struct RamInterpreter : public RamGuidedInterpreter {

    /** A constructor setting the query policy for the base class */
    RamInterpreter() : RamGuidedInterpreter(DirectExecution) {
    };

};


/**
 * A RAM executor based on the creation and compilation of an executable conducting
 * the actual computation.
 */
class RamCompiler : public RamExecutor {

    /**
     * The file name of the executable to be created, empty if a temporary
     * file should be utilized.
     */
    mutable std::string fileName;

public:

    /** A simple constructore */
    RamCompiler(const std::string& fn = "") : fileName(fn) {}

    /**
     * Updates the file name of the binary to be utilized by
     * this executor.
     */
    void setBinaryFile(const std::string& name) {
        fileName = name;
    }

    /**
     * Obtains the name of the binary utilized by this executer.
     */
    const std::string& getBinaryFile() const {
        return fileName;
    }

    /**
     * Generates the code for the given ram statement.The target file
     * name is either set by the corresponding member field or will
     * be determined randomly. The chosen file-name will be returned.
     */
    std::string generateCode(const SymbolTable& symTable, const RamStatement& stmt, const std::string& filename = "") const;

    /**
     * Compiles the given statement to a binary file. The target file
     * name is either set by the corresponding member field or will
     * be determined randomly. The chosen file-name will be returned.
     */
    std::string compileToBinary(const SymbolTable& symTable, const RamStatement& stmt) const;

    /**
     * The actual implementation of this executor encoding the given
     * program into a source file, compiling and executing it.
     */
    virtual void applyOn(const RamStatement& stmt, RamEnvironment& env) const;

private:

    /**
     * Obtains a file name for the resulting source and executable file.
     */
    std::string resolveFileName() const;

};

} // end of namespace souffle

