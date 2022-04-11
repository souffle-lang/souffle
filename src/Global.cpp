/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Global.cpp
 *
 * Defines a configuration environment
 *
 ***********************************************************************/

#include "Global.h"
#include "souffle/utility/FileUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <cassert>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef USE_CUSTOM_GETOPTLONG
#include "souffle/utility/GetOptLongImpl.h"
#else
#include <getopt.h>
#endif

namespace souffle {

void MainConfig::processArgs(int argc, char** argv, const std::string& header, const std::string& footer,
        const std::vector<MainOption>& mainOptions) {
    constexpr auto ONE_SPACE = " ";
    constexpr auto TWO_SPACES = "  ";
    constexpr auto THREE_SPACES = "   ";

    // construct the help text using the main options
    {
        // create a stream to be 'printed' to
        std::stringstream ss;

        // print the header
        ss << header;

        // compute the maximum length of a line of the help text without including the description
        std::size_t maxLineLengthWithoutDescription = 0;
        {
            // initially compute the maximum line length without the long option, arguments, or description
            std::stringstream lineSchema;
            const auto shortOption = "?";
            const auto longOption = "";
            const auto arguments = "";
            const auto description = "";
            lineSchema << TWO_SPACES << "-" << shortOption << "," << ONE_SPACE << "--" << longOption << "=<"
                       << arguments << ">" << TWO_SPACES << description;
            maxLineLengthWithoutDescription = lineSchema.str().size();
        }
        {
            // then compute the maximum length of the long option plus the description
            std::size_t maxLongOptionPlusArgumentLength = 0;
            for (const MainOption& opt : mainOptions) {
                if (opt.longName.empty()) {
                    continue;
                }
                const auto longOptionPlusArgumentLength = opt.longName.size() + opt.argument.size();
                if (longOptionPlusArgumentLength > maxLongOptionPlusArgumentLength) {
                    maxLongOptionPlusArgumentLength = longOptionPlusArgumentLength;
                }
            }
            maxLineLengthWithoutDescription += maxLongOptionPlusArgumentLength;
        }

        auto indentDescription = [&](auto&& os, size_t curr_len) {
            for (auto n = curr_len; n < maxLineLengthWithoutDescription; ++n)
                os << ONE_SPACE;
        };

        // #NormaliseStatementExpressions #NoLambdaNeeded
        auto multi_line_descr_sep = [&]() {
            std::stringstream ss;
            ss << "\n";
            indentDescription(ss, 0);
            return ss.str();
        }();

        // iterate over the options and pretty print them, using the computed maximum line length without the
        // description
        for (const MainOption& opt : mainOptions) {
            // the current line
            std::stringstream line;

            // if it is the main option, do nothing
            if (opt.longName.empty()) {
                continue;
            }

            // print the short form name and the argument parameter
            line << TWO_SPACES;
            if (isalpha(opt.shortName) != 0) {
                line << "-" << opt.shortName << ",";
            } else {
                line << THREE_SPACES;
            }

            // print the long form name
            line << ONE_SPACE << "--" << opt.longName;

            // print the argument parameter
            if (!opt.argument.empty()) {
                line << "=<" << opt.argument << ">";
            }

            auto&& line_str = line.str();
            ss << line_str;
            indentDescription(ss, line_str.size());

            ss << join(splitView(opt.description, "\n"), multi_line_descr_sep) << "\n";
        }

        // print the footer
        ss << footer;

        // finally, store the help text as a string
        _help = ss.str();
    }

    // use the main options to define the global configuration
    {
        // array of long names for classic getopt processing
        std::unique_ptr<option[]> longNames = std::make_unique<option[]>(mainOptions.size());
        // string of short names for classic getopt processing
        std::string shortNames = "";
        // table to map the short name to its option
        std::map<const char, const MainOption*> optionTable;
        std::set<std::string> optionSeen;  // options seen at least once in arg list

        // counter to be incremented at each loop
        int i = 0;
        // iterate over the options provided
        for (const MainOption& opt : mainOptions) {
            assert(opt.shortName != '?' && "short name for option cannot be '?'");
            // put the option in the table, referenced by its short name
            optionTable[opt.shortName] = &opt;
            // set the default value for the option, if it exists
            if (!opt.byDefault.empty()) {
                // don't use `set` since we don't want to flag it as explicitly set (it's an implicit default)
                _map[opt.longName] = {opt.byDefault};
            }
            if (opt.takesMany) {
                _allows_multiple.insert(opt.longName);
            }
            // skip the next bit if it is the option for the datalog file
            if (opt.longName.empty()) {
                continue;
            }
            // convert the main option to a plain old getopt option and put it in the array
            longNames[i] = {opt.longName.c_str(), opt.argument.empty() ? 0 : 1, nullptr, opt.shortName};
            // append the short name of the option to the string of short names
            shortNames += opt.shortName;
            // indicating with a ':' if it takes an argument
            if (!opt.argument.empty()) {
                shortNames += ":";
            }
            // increment counter
            ++i;
        }
        // the terminal option, needs to be null
        longNames[i] = {nullptr, 0, nullptr, 0};

        // use getopt to process the arguments given to the command line, with the parameters being the
        // short and long names from above
        int c;
        while ((c = getopt_long(argc, argv, shortNames.c_str(), longNames.get(), nullptr)) != -1) {
            // case for the unknown option
            if (c == '?') {
                std::cerr << Global::config().help();
                throw std::runtime_error("Error: Unknown command line option.");
            }
            // obtain an iterator to the option in the table referenced by the current short name
            auto iter = optionTable.find(c);
            // case for the unknown option, again
            assert(iter != optionTable.end() && "unexpected case in getopt");
            auto&& [_, opt] = *iter;
            // define the value for the option in the global configuration as its argument or an empty string
            //  if no argument exists
            std::string arg = optarg != nullptr ? std::string(optarg) : std::string();

            if (iter->second->argument == "FILE" || iter->second->argument == "DIR") {
                // convert to prefered directory separator
                makePreferred(arg);
            }

            // if the option allows multiple arguments
            if (opt->takesMany) {
                // set the value of the option in the global config to the concatenation of its previous
                // value, a space and the current argument
                append(opt->longName, std::move(arg));
                // otherwise, set the value of the option in the global config
            } else {
                // but only if it isn't set already
                if (state(opt->longName) == State::set) {
                    throw std::runtime_error(
                            "Error: Only one argument allowed for option '" + opt->longName + "'");
                }
                set(opt->longName, std::move(arg));
            }
        }
    }

    // obtain the name of the datalog file, and store it in the option with the empty key
    if (argc > 1 && !Global::config().has("help") && !Global::config().has("version")) {
        std::string filename = "";
        // ensure that the optind is less than the total number of arguments
        if (argc > 1 && optind >= argc) {
            std::cerr << Global::config().help();
            throw std::runtime_error("Error: Missing source file path.");
        }

        if (mainOptions[0].longName.empty()) {
            // for each of the command line arguments not associated with an option
            Many filenames;
            for (; optind < argc; optind++)
                filenames.push_back(argv[optind]);

            if (!mainOptions[0].takesMany && 1 < filenames.size()) {
                throw std::runtime_error("Error: Only one argument allowed for datalog file");
            }

            set(mainOptions[0].longName, std::move(filenames));
        }
    }
}

const MainConfig::Single& MainConfig::get(std::string_view key) const {
    return get(key, _default_single);
}

const MainConfig::Many& MainConfig::getMany(std::string_view key) const {
    return getMany(key, _default_many);
}

const MainConfig::Single& MainConfig::get(std::string_view key, const Single& default_) const {
    auto it = _map.find(key);
    if (it == _map.end()) return default_;

    auto&& [_, vs] = *it;
    assert(vs.size() == 1 && "option has multiple values");
    return vs.front();
}

const MainConfig::Many& MainConfig::getMany(std::string_view key, const Many& default_) const {
    auto it = _map.find(key);
    if (it == _map.end()) return default_;

    auto&& [_, vs] = *it;
    return vs;
}

bool MainConfig::has(std::string_view key) const {
    return _map.find(key) != _map.end();
}

bool MainConfig::has(std::string_view key, std::string_view value) const {
    auto it = _map.find(key);
    if (it != _map.end()) {
        auto&& [_, vs] = *it;
        for (auto&& v : vs)
            if (v == value) return true;
    }

    return false;
}

void MainConfig::append(std::string_view key, Single value) {
    if (!contains(_explicitly_set, key)) {
        _explicitly_set.insert(std::string(key));
    }

    auto it = _map.find(key);
    if (it == _map.end()) {
        it = _map.insert({std::string(key), Many{}}).first;
    }

    it->second.push_back(std::move(value));
}

/* Set the entry in the table for the specified key to the specified value. */
void MainConfig::set(std::string key, Single value) {
    set(std::move(key), std::vector{std::move(value)});
}

void MainConfig::set(std::string key, Many value) {
    _explicitly_set.insert(key);
    _map[std::move(key)] = std::move(value);
}

/* Erase the entry in the table for the specified key. */
void MainConfig::unset(std::string_view key) {
    _map.erase(_map.find(key));
    _explicitly_set.erase(_explicitly_set.find(key));
}

MainConfig::State MainConfig::state(std::string_view key) const {
    if (contains(_explicitly_set, key)) return State::set;
    if (has(key)) return State::default_;
    return State::unset;
}

bool MainConfig::allowsMultiple(std::string_view key) const {
    return contains(_allows_multiple, key);
}

}  // namespace souffle
