#pragma once

#include <iostream>

namespace souffle {

namespace macro {

inline void call(const std::string& /*name*/, const std::string& text, const int code) {
    std::cerr << "Error: " << text << std::endl;
    exit(code);
}

template <typename T>
inline void call(const std::string& name, const std::string& text, const int code, T callback) {
    std::cerr << name << ": " << text << std::endl;
    callback();
    exit(code);
}
}  // namespace macro
}  // namespace souffle

/* Macro for BREAKPOINT */
#ifndef BREAKPOINT
#ifndef OPT
#define BREAKPOINT (std::cerr << "@" << __FILE__ << ":" << __LINE__ << std::endl)
#else
#define BREAKPOINT
#endif
#endif

/* Macro for ASSERT */
#ifndef ASSERT
#ifndef OPT
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif
#endif

/* Macro for ERROR */
#ifndef ERROR
#ifndef __ERROR_1__
#define __ERROR_1__(text) souffle::macro::call("Error", text, 1)
#endif
#ifndef __ERROR_2__
#define __ERROR_2__(text, callback) souffle::macro::call("Error", text, 1, callback)
#endif
#define GET_MACRO(_1, _2, NAME, ...) NAME
#define ERROR(...) GET_MACRO(__VA_ARGS__, __ERROR_2__, __ERROR_1__)(__VA_ARGS__)
#endif
