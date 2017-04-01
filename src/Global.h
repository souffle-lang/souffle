#pragma once

#include "Util.h"

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <ctype.h>
#include <getopt.h>

namespace souffle {

// TODO: this is the wrong place for this class, find somewhere better to put it
/* A simple table class, to be used as a base for others to extend from. */
template <typename K, typename V>
class BaseTable {
private:
    /* Default object made by empty constructor to return by reference. */
    const V _default;
    /* The raw data backing this table. */
    std::map<K, V> _data;

public:
    /* Empty constructor. */
    BaseTable() : _default(V()), _data(std::map<K, V>()) {}
    /* Copy constructor. */
    BaseTable(const BaseTable& other) {
        data(other.data());
    }
    /* Assignment operator. */
    BaseTable& operator=(const BaseTable& other) {
        data(other.data());
        return *this;
    }
    /* Set the raw data. */
    const std::map<K, V>& data() const {
        return _data;
    }
    /* Get the raw data. */
    void data(const std::map<K, V>& otherData) {
        _data = otherData;
    }
    /* Get a value by its key, if not found return a reference to an object of the value class called with an
     * empty constructor. */
    const V& get(const K& key) const {
        return (has(key)) ? _data.at(key) : _default;
    }
    /* Get a value by its key, if not found return the specified value. */
    const V& get(const K& key, const V& value) const {
        return (has(key)) ? _data.at(key) : value;
    }
    /* Check the table has the specified key. */
    const bool has(const K& key) const {
        return _data.find(key) != _data.end();
    }
    /* Check the table has the specified key and the specified value for that key. */
    const bool has(const K& key, const V& value) const {
        return has(key) && _data.at(key) == value;
    }
    /* Set the entry in the table for the specified key to an object of the value class called with an empty
     * constructor. */
    void set(const K& key) {
        _data[key] = _default;
    }
    /* Set the entry in the table for the specified key to the specified value. */
    void set(const K& key, const V& value) {
        _data[key] = value;
    }
    /* Print the raw backing data to the specified stream. */
    void print(std::ostream& os) {
        os << _data << std::endl;
    }
};

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
class MainConfig : public BaseTable<std::string, std::string> {
private:
    /* The help text, printed if there is an error in the command line arguments. */
    std::string _help;

public:
    /* Empty constructor, does nothing. */
    MainConfig() : BaseTable<std::string, std::string>() {}
    /* The argument processing method, this takes the arguments provided to main, a header, a footer, and a
       list of options.
       From these, we construct the help text and the global configuration. See Global.cpp for details. */
    void processArgs(int argc, char** argv, const std::string header, const std::string footer,
            const std::vector<MainOption> mainOptions);
    /* Obtain the help text as a string. Note that 'processArgs' must be called before this is used. */
    const std::string& help() const {
        return _help;
    }
};

/* The global class. Currently used to provide a singleton instance of the global config. This class may be
 * used to isolate all globals. */
class Global {
private:
    /* Private empty constructor, there is only one global instance. */
    Global() = default;

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
};
}  // namespace souffle
