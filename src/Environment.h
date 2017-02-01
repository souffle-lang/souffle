#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <ctype.h>
#include <getopt.h>

#include "Util.h"

namespace souffle {

/** A simple table class. */
class StringTable {

    private:

        /** An empty string object, used to return by reference. */
        const std::string empty;

        /** A table of key value pairs. */
        std::map<std::string, std::string> table;

    public:

        /** Constructor for the table class. */
        StringTable() : empty (std::string()), table (std::map<std::string, std::string>())  {}

        /** Get the value for a given key from the table, returning an empty string if it does not exist. */
        const std::string& get(const std::string& key) const { return (has(key)) ? table.at(key) : empty; }

        /** Get the value for a given key from the table, returning the specified value if it does not exist. */
        const std::string& get(const std::string& key, const std::string& value) const  { return (has(key)) ? table.at(key) : value; }

        /** Check if the table has any value for the given key. */
        const bool has(const std::string& key) const { return table.find(key) != table.end(); }

        /** Check if the table has the specified value for the given key. */
        const bool has(const std::string& key, const std::string& value) const { return has(key) && table.at(key) == value; }

        /** Set the value for a key in the table to an empty string. */
        void set(const std::string& key) { table[key] = empty; }

        /** Set the value for a key in the table to the given value. */
        void set(const std::string& key, const std::string& value) { table[key] = value; }

        /** Print the table to the specified output stream. */
        void print(std::ostream& os) { os << table << std::endl; }
};

struct Option {
       const std::string name;
       const char flag;
       const std::string argument;
       const std::string by_default;
       const std::string description;
};

/** Class to handle the command line arguments. */
class Environment : public StringTable {

    private:

        /** Argument count, passed from main. */
        int argc;

        /** Argument array, passed from main. */
        char** argv;

        /** Header, for the help text. */
        const std::string header;

        /** Footer, for the help text. */
        const std::string footer;

        /** Options, for both the command line arguments and the help text. */
        const std::vector<Option> options;


    public:

        /** Constructor for the environment. */
        Environment(int argc, char** argv, const std::string header, const std::string footer, const std::vector<Option> options);

        /** Print all available options to the given stream. */
        void printOptions(std::ostream& os);

        /** Print an error message, the help text, and exit. */
        void error();

};

};