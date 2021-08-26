/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SubProcess.h
 *
 * Wrapper for launching subprocesses.
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/Types.h"
#include "souffle/utility/span.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <optional>
#include <type_traits>
#include <sys/wait.h>
#include <unistd.h>

namespace souffle {

namespace detail {
[[noreturn]] inline void perrorExit(char const* msg) {
    ::perror(msg);
    std::exit(EXIT_FAILURE);
}

// These are used by bash and are a defacto standard on Linux.
// This list is incomplete.
enum LinuxExitCode : int {
    cannot_execute = 126,
    command_not_found = 127,
};

using LinuxWaitStatus = int;
}  // namespace detail

/**
 * Executes a process w/ the given `argv` arguments and `envp` overriden env-vars.
 *
 * @param   argv  The arguments to the process.
 *                Do not include the 'program invoked as' argument 0. This is implicitly done for you.
 * @param   envp  Collection of env vars to override.
 *                Any env not specified in `envp` is inherited from this process' environment.
 * @return  `None` IFF unable to launch `program`, otherwise `program`'s `wait` status.
 *           NB: This is not the exit code, though the exit code can be obtained from it.
 *               However, you can do `execute(...) == 0` if you only care that it succeeded.
 */
template <typename Envp = span<std::pair<char const*, char const*>>,
        typename = std::enable_if_t<is_iterable_of<Envp, std::pair<char const*, char const*> const>>>
std::optional<detail::LinuxWaitStatus> execute(
        std::string const& program, span<char const* const> argv = {}, Envp&& envp = {}) {
    using EC = detail::LinuxExitCode;

    auto pid = ::fork();
    switch (pid) {
        case -1: return {};  // unable to fork. likely hit a resource limit of some kind.

        case 0: {  // child
            // thankfully we're a fork. we can trash this proc's `::environ` w/o reprocussions
            for (auto&& [k, v] : envp) {
                if (::setenv(k, v, 1)) detail::perrorExit("setenv");
            }

            char* argv_temp[argv.size() + 2];
            argv_temp[0] = const_cast<char*>(program.c_str());
            std::copy_n(argv.data(), argv.size(), const_cast<char const**>(argv_temp) + 1);
            argv_temp[argv.size() + 1] = nullptr;

            ::execvp(program.c_str(), argv_temp);
            std::exit(EC::cannot_execute);
        }

        default: {  // parent
            detail::LinuxWaitStatus status;
            if (::waitpid(pid, &status, 0) == -1) {
                // not recoverable / should never happen.
                detail::perrorExit("`waitpid` failed");
            }

            // check it exited or signaled (didn't specify `WNOHANG` or `WUNTRACED`)
            assert(WIFSIGNALED(status) || WIFEXITED(status));

            // check that the fork child successfully `exec`'d
            if (WIFEXITED(status)) {
                switch (WEXITSTATUS(status)) {
                    default: break;

                    case EC::cannot_execute:                // FALL THRU: command_not_found
                    case EC::command_not_found: return {};  // fork couldn't execute the program
                }
            }

            return status;
        }
    }
}

/**
 * Executes a process w/ the given `argv` arguments and `envp` overriden env-vars.
 *
 * @param   argv  The arguments to the process.
 *                Do not include the 'program invoked as' argument 0. This is implicitly done for you.
 * @param   envp  Collection of env vars to override.
 *                Any env not specified in `envp` is inherited from this process' environment.
 * @return  `None` IFF unable to launch `program`, otherwise `program`'s `wait` status.
 *           NB: This is not the exit code, though the exit code can be obtained from it.
 *               However, you can do `execute(...) == 0` if you only care that it succeeded.
 */
template <typename Envp = span<std::pair<char const*, std::string>>,
        typename = std::enable_if_t<is_iterable_of<Envp, std::pair<char const*, std::string> const>>>
std::optional<detail::LinuxWaitStatus> execute(
        std::string const& program, span<std::string const> argv, Envp&& envp = {}) {
    auto go = [](auto* dst, auto&& src, auto&& f) {
        size_t i = 0;
        for (auto&& x : src)
            dst[i++] = f(x);
        return span<std::remove_pointer_t<decltype(dst)>>{dst, dst + src.size()};
    };

    char const* argv_temp[argv.size()];
    std::pair<char const*, char const*> envp_temp[envp.size()];
    auto argv_ptr = go(argv_temp, argv, [](auto&& x) { return x.c_str(); });
    auto envp_ptr = go(envp_temp, envp, [](auto&& kv) { return std::pair{kv.first, kv.second.c_str()}; });
    return souffle::execute(program, argv_ptr, envp_ptr);
}

}  // namespace souffle
