/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "VirtualFileSystem.h"

#include <fstream>
#include <sstream>

namespace souffle {

bool RealFileSystem::exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

std::filesystem::path RealFileSystem::canonical(const std::filesystem::path& path, std::error_code& ec) {
    return std::filesystem::canonical(path, ec);
}

std::string RealFileSystem::readFile(const std::filesystem::path& path, std::error_code& ec) {
    std::ifstream in(path.string().c_str());

    if (in.fail()) {
        ec = std::error_code(static_cast<int>(std::errc::no_such_file_or_directory), std::generic_category());
        return {};
    }

    ec.clear();

    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

OverlayFileSystem::OverlayFileSystem(std::shared_ptr<FileSystem> base) {
    FSs.push_back(base);
}

void OverlayFileSystem::pushOverlay(std::shared_ptr<FileSystem> fs) {
    FSs.push_back(fs);
}

OverlayFileSystem::const_iterator OverlayFileSystem::fs_begin() const {
    return FSs.rbegin();
}

OverlayFileSystem::const_iterator OverlayFileSystem::fs_end() const {
    return FSs.rend();
}

bool OverlayFileSystem::exists(const std::filesystem::path& path) {
    for (auto fs = fs_begin(); fs != fs_end(); ++fs) {
        if ((*fs)->exists(path)) {
            return true;
        }
    }
    return false;
}

std::filesystem::path OverlayFileSystem::canonical(const std::filesystem::path& path, std::error_code& ec) {
    // by construction there is at least one file-system in the list, so `ec` always get set.
    for (auto fs = fs_begin(); fs != fs_end(); ++fs) {
        std::filesystem::path res = (*fs)->canonical(path, ec);
        if (!ec) {
            return res;
        }
    }
    return {};
}

std::string OverlayFileSystem::readFile(const std::filesystem::path& path, std::error_code& ec) {
    for (auto fs = fs_begin(); fs != fs_end(); ++fs) {
        std::string res = (*fs)->readFile(path, ec);
        if (!ec) {
            return res;
        }
    }
    return {};
}

}  // namespace souffle
