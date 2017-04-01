/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IODirectives.h
 *
 ***********************************************************************/

#pragma once

#include <map>
#include <sstream>
#include <string>

namespace souffle {

class IODirectives {
public:
    IODirectives() {}

    IODirectives(const std::map<std::string, std::string>& directiveMap) {
        for (const auto& pair : directiveMap) {
            directives[pair.first] = pair.second;
        }
    }

    ~IODirectives() = default;

    const std::string& getIOType() const {
        return get("IO");
    }

    void setIOType(const std::string& type) {
        directives["IO"] = type;
    }

    const std::string& get(const std::string& key) const {
        if (directives.count(key) == 0) {
            throw std::invalid_argument("Requested IO directive <" + key + "> was not specified");
        }
        return directives.at(key);
    }

    void set(const std::string& key, const std::string& value) {
        directives[key] = value;
    }

    bool has(const std::string& key) const {
        return directives.count(key) > 0;
    }

    const std::string& getFileName() const {
        return get("filename");
    }

    void setFileName(const std::string& filename) {
        directives["filename"] = filename;
    }

    const std::string& getRelationName() const {
        return get("name");
    }

    void setRelationName(const std::string& name) {
        directives["name"] = name;
    }

    bool isEmpty() {
        return directives.empty();
    }

    void print(std::ostream& out) const {
        auto cur = directives.begin();
        if (cur == directives.end()) {
            return;
        }

        out << "{{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
        ++cur;
        for (; cur != directives.end(); ++cur) {
            out << ",{\"" << cur->first << "\",\"" << escape(cur->second) << "\"}";
        }
        out << '}';
    }

    friend std::ostream& operator<<(std::ostream& out, const IODirectives ioDirectives) {
        ioDirectives.print(out);
        return out;
    }

private:
    std::string escape(const std::string& inputString) const {
        std::string escaped = escape(inputString, "\"", "\\\"");
        escaped = escape(escaped, "\t", "\\t");
        escaped = escape(escaped, "\r", "\\r");
        escaped = escape(escaped, "\n", "\\n");
        return escaped;
    }

    std::string escape(
            const std::string& inputString, const std::string& needle, const std::string replacement) const {
        std::string result = inputString;
        size_t pos = 0;
        while ((pos = result.find(needle, pos)) != std::string::npos) {
            result = result.replace(pos, needle.length(), replacement);
            pos += replacement.length();
        }
        return result;
    }

    std::map<std::string, std::string> directives;
};
}  // namespace souffle
