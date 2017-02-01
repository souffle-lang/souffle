#include "Environment.h"

namespace souffle {

Environment::Environment(int argc, char** argv, const std::string header, const std::string footer, const std::vector<Option> options)
    : StringTable()
    , argc (argc)
    , argv (argv)
    , header (header)
    , footer (footer)
    , options (options)
{
    option longOptions[options.size() + 1];
    std::map<const char, std::string> optionTable;
    int i = 0;
    std::string shortOptions = "";
    for (const Option& opt : options) {
        longOptions[i] = (option) { opt.name.c_str(), (!opt.argument.empty()), nullptr, opt.flag };
        optionTable[opt.flag] = opt.name;
        shortOptions += opt.flag;
        if (!opt.argument.empty()) {
            shortOptions += ":";
        }
        ++i;
    }
    longOptions[i] = {nullptr, false, nullptr, 0}; // the terminal option, needs to be null

    int c;     /* command-line arguments processing */
    while ((c = getopt_long(argc, argv, shortOptions.c_str(), longOptions, nullptr)) != EOF) {
        if (c == '?') error();
        auto iter = optionTable.find(c);
        if (iter == optionTable.end()) {
            ASSERT("Default label in getopt switch.");
            abort();
        }
        if (!has(iter->second))
            set(iter->second);
        else
            set(iter->second, get(iter->second) + " ");
        if (optarg)
            set(iter->second, get(iter->second) + std::string(optarg));
    }

}

void Environment::printOptions(std::ostream& os) {
    os << header;
    int namelen = 0, arglen = 0;
    for (const Option& opt : options) {
        namelen = ((int) opt.name.size() > namelen) ? opt.name.size() : namelen;
        arglen = ((int) opt.argument.size() > arglen) ? opt.argument.size() : arglen;
    }

    int length;
    for (const Option& opt : options) {

        length = 0;
        os << "\t";
        if (isalpha(opt.flag)) {
            os << "-" << opt.flag;
            if (!opt.argument.empty()) {
                os << "<" << opt.argument << ">";
                length = opt.argument.size() + 2;
            }
        } else {
            os << "  ";
        }

        for (; length < arglen + 2; ++length) os << " ";

        length = 0;
        os << "\t" << "--" << opt.name;
        if (!opt.argument.empty()) {
            os << "=<" << opt.argument << ">";
            length = opt.argument.size() + 3;
        }
        for (length += opt.name.size(); length < arglen + namelen + 3; ++length) os << " ";

        os << "\t" << opt.description << std::endl;

    }

    os << footer;
}

void Environment::error() {
    for(int i = 0; i < argc; i++) {
        std::cerr << argv[i] << " ";
    }
    std::cerr << "\nError parsing command-line arguments.\n";
    printOptions(std::cerr);
    exit(1);
}

};
