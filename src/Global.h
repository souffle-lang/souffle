/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Global.h
 *
 * Defines a configuration environment
 *
 ***********************************************************************/

#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/* Struct to represent an option given to the main function by command line arguments. */
struct MainOption {
    std::string longName;  /* The long name for this option, e.g. 'option' for '--option'. */
    char shortName;        /* The short name for this option where a non-character option means none will be
                              displayed, e.g. 'o' for '-o'. */
    std::string argument;  /* The argument this option, e.g. if longName is 'option', shortName is 'o', and
                              argument is 'ARG', then we have '-o=ARG' and '--option=ARG'. */
    std::string byDefault; /* The default value for this option, used if no this option is not specified as a
                              command line argument. */
    bool takesMany; /* Whether this option takes many arguments, false for 'it takes only one' true for 'it
                       takes one or more'. */
    std::string description; /* The description of what this option does, used in the help text produced
                                from the options. */
};

/* The MainConfig class, used to handle the global configuration and the help text. */
class MainConfig {
    using Single = std::string;
    using Many = std::vector<Single>;

public:
    /* The argument processing method, this takes the arguments provided to main, a header, a footer, and a
       list of options.
       From these, we construct the help text and the global configuration. See Global.cpp for details. */
    void processArgs(int argc, char** argv, const std::string& header, const std::string& footer,
            const std::vector<MainOption>& mainOptions);

    /* Obtain the help text as a string. Note that 'processArgs' must be called before this is used. */
    const std::string& help() const {
        return _help;
    }

    /* Get a single-value key if present, else returns the default single-value (empty string) */
    const Single& get(std::string_view key) const;
    /* Get a multi-value key if present, else returns the default multi-value (empty vector) */
    const Many& getMany(std::string_view key) const;
    /* Get a single-value key if present, else returns the second argument */
    const Single& get(std::string_view key, const Single&) const;
    /* Get a multi-value key if present, else returns the second argument */
    const Many& getMany(std::string_view key, const Many&) const;

    /* Check the table has the specified key. */
    bool has(std::string_view key) const;
    /* Returns true IFF the table contains this key-value pair */
    bool has(std::string_view key, std::string_view value) const;

    // Appends a value to a multi-value key
    void append(std::string_view key, Single);
    /* Set the entry in the table for the specified key to the specified value. */
    void set(std::string key, Single value = {});
    /* Set the entry in the table for the specified key to the specified value. */
    void set(std::string key, Many value);
    /* Erase the entry in the table for the specified key. */
    void unset(std::string_view key);

    enum class State { unset, default_, set };

    State state(std::string_view key) const;
    bool allowsMultiple(std::string_view key) const;

    std::map<std::string, Many, std::less<>> const& data() const {
        return _map;
    }

private:
    Single _default_single;
    Many _default_many;
    std::map<std::string, Many, std::less<>> _map;
    // whether this option was explicitly set (not present -> default value or not set)
    std::set<std::string, std::less<>> _explicitly_set;
    // whether this option allows multiple instances
    std::set<std::string, std::less<>> _allows_multiple;
    /* The help text, printed if there is an error in the command line arguments. */
    std::string _help;
};

/* The global class. Currently used to provide a singleton instance of the global config. This class may be
 * used to isolate all globals. */
class Global {
public:
    /* Deleted copy constructor. */
    Global(const Global&) = delete;
    /* Deleted assignment operator. */
    Global& operator=(const Global&) = delete;
    /* Obtain the global configuration. */
    static MainConfig& config() {
        static MainConfig _config;
        return _config;
    }

private:
    /* Private empty constructor, there is only one global instance. */
    Global() = default;
};
}  // namespace souffle
