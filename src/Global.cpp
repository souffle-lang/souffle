#include "Global.h"

namespace souffle {

void MainConfig::processArgs(int argc, char** argv, const std::string header, const std::string footer,
        const std::vector<MainOption> mainOptions) {
    // construct the help text using the main options
    {
        // create a stream to be 'printed' to
        std::stringstream ss;

        // print the header
        ss << header;

        // iterate over the options and obtain the maximum name and argument lengths
        int maxLongNameLength = 0, maxArgumentIdLength = 0;
        for (const MainOption& opt : mainOptions) {
            // if it is the main option, do nothing
            if (opt.longName == "") {
                continue;
            }
            // otherwise, proceed with the calculation
            maxLongNameLength =
                    ((int)opt.longName.size() > maxLongNameLength) ? opt.longName.size() : maxLongNameLength;
            maxArgumentIdLength = ((int)opt.argument.size() > maxArgumentIdLength) ? opt.argument.size()
                                                                                   : maxArgumentIdLength;
        }

        // iterator over the options and pretty print them, using padding as determined
        // by the maximum name and argument lengths
        int length;
        for (const MainOption& opt : mainOptions) {
            // if it is the main option, do nothing
            if (opt.longName == "") {
                continue;
            }

            // print the short form name and the argument parameter
            length = 0;
            ss << "\t";
            if (isalpha(opt.shortName)) {
                ss << "-" << opt.shortName;
                if (!opt.argument.empty()) {
                    ss << "<" << opt.argument << ">";
                    length = opt.argument.size() + 2;
                }
            } else {
                ss << "  ";
            }

            // pad with empty space for prettiness
            for (; length < maxArgumentIdLength + 2; ++length) {
                ss << " ";
            }

            // print the long form name and the argument parameter
            length = 0;
            ss << "\t"
               << "--" << opt.longName;
            if (!opt.argument.empty()) {
                ss << "=<" << opt.argument << ">";
                length = opt.argument.size() + 3;
            }

            // again, pad with empty space for prettiness
            for (length += opt.longName.size(); length < maxArgumentIdLength + maxLongNameLength + 3;
                    ++length) {
                ss << " ";

                // print the description
            }
            ss << "\t" << opt.description << std::endl;
        }

        // print the footer
        ss << footer;

        // finally, store the help text as a string
        _help = ss.str();
    }

    // use the main options to define the global configuration
    {
        // array of long names for classic getopt processing
        option longNames[mainOptions.size()];
        // string of short names for classic getopt processing
        std::string shortNames = "";
        // table to map the short name to its option
        std::map<const char, const MainOption*> optionTable;
        // counter to be incremented at each loop
        int i = 0;
        // iterate over the options provided
        for (const MainOption& opt : mainOptions) {
            assert(opt.shortName != '?' && "short name for option cannot be '?'");
            // put the option in the table, referenced by its short name
            optionTable[opt.shortName] = &opt;
            // set the default value for the option, if it exists
            if (!opt.byDefault.empty()) {
                set(opt.longName, opt.byDefault);
            }
            // skip the next bit if it is the option for the datalog file
            if (opt.longName == "") {
                continue;
            }
            // convert the main option to a plain old getopt option and put it in the array
            longNames[i] = (option){opt.longName.c_str(), (!opt.argument.empty()), nullptr, opt.shortName};
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
        longNames[i] = {nullptr, false, nullptr, 0};

        // use getopt to process the arguments given to the command line, with the parameters being the
        // short and long names from above
        int c;
        while ((c = getopt_long(argc, argv, shortNames.c_str(), longNames, nullptr)) != EOF) {
            // case for the unknown option
            if (c == '?') {
                ERROR("unexpected command line argument", []() { std::cerr << Global::config().help(); });
            }
            // obtain an iterator to the option in the table referenced by the current short name
            auto iter = optionTable.find(c);
            // case for the unknown option, again
            assert(iter != optionTable.end() && "unexpected case in getopt");
            // define the value for the option in the global configuration as its argument or an empty string
            //  if no argument exists
            std::string arg = (optarg) ? std::string(optarg) : std::string();
            // if the option allows multiple arguments
            if (iter->second->takesMany) {
                // set the value of the option in the global config to the concatenation of its previous
                // value, a space and the current argument
                set(iter->second->longName, get(iter->second->longName) + ' ' + arg);
                // otherwise, set the value of the option in the global config
            } else {
                // but only if it isn't set already
                if (has(iter->second->longName) &&
                        (iter->second->byDefault.empty() ||
                                !has(iter->second->longName, iter->second->byDefault))) {
                    ERROR("only one argument allowed for option '" + iter->second->longName + "'");
                }
                set(iter->second->longName, arg);
            }
        }
    }

    // obtain the name of the datalog file, and store it in the option with the empty key
    if (argc > 1 && !Global::config().has("help")) {
        std::string filename = "";
        // ensure that the optind is less than the total number of arguments
        if (argc > 1 && optind >= argc) {
            ERROR("unexpected command line argument", []() { std::cerr << Global::config().help(); });
        }
        // if only one datalog program is allowed
        if (mainOptions[0].longName == "" && mainOptions[0].takesMany) {
            // set the option in the global config for the main datalog file to that specified by the command
            // line arguments
            set("", std::string(argv[optind]));
            // otherwise, if multiple input filenames are allowed
        } else {
            std::string filenames = "";
            // for each of the command line arguments not associated with an option
            for (; optind < argc; optind++) {
                // append this filename to the concatenated string of filenames
                if (filenames == "") {
                    filenames = argv[optind];
                } else {
                    filenames = filenames + " " + std::string(argv[optind]);
                }
            }
            // set the option in the global config for the main datalog file to all those specified by the
            // command line arguments
            set("", filenames);
        }
    }
}
}  // namespace souffle
