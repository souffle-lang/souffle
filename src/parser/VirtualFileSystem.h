/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include <filesystem>
#include <list>
#include <memory>
#include <string>

namespace souffle {

/** Interface of minimalist virtual file system that fullfill requirements of the Souffle scanner. */
class FileSystem {
public:
    virtual ~FileSystem() = default;

    /** Indicate if the given path exists. */
    virtual bool exists(const std::filesystem::path&) = 0;

    /**
     * Get the canonical path for the given path.
     *
     * Return an empty path and set the error code in case of failure.
     */
    virtual std::filesystem::path canonical(const std::filesystem::path&, std::error_code&) = 0;

    /**
     * Get the content of the file at the given path.
     *
     * Return an empty string and set the error code in case of failure.
     */
    virtual std::string readFile(const std::filesystem::path&, std::error_code&) = 0;
};

/** A virtual file system exposing the real file system. */
class RealFileSystem : public FileSystem {
public:
    bool exists(const std::filesystem::path&) override;
    std::filesystem::path canonical(const std::filesystem::path&, std::error_code&) override;
    std::string readFile(const std::filesystem::path&, std::error_code&) override;
};

/**
 * An overlay file system exposing a stack of several virtual file systems.
 *
 * If a file is not present in the top file-system, the next file system below
 * it is interrogated. The query is relayed to each file system in the stack in
 * order until the file is found or all file systems have been queried.
 *
 * When all file systems of the stack have been queried without success, the
 * requested file is considered absent.
 */
class OverlayFileSystem : public FileSystem {
public:
    OverlayFileSystem(std::shared_ptr<FileSystem> base);

    bool exists(const std::filesystem::path&) override;
    std::filesystem::path canonical(const std::filesystem::path&, std::error_code&) override;
    std::string readFile(const std::filesystem::path&, std::error_code&) override;

    /** Push a virtual file system on the top of the stack. */
    void pushOverlay(std::shared_ptr<FileSystem> fs);

    using FSlist = std::list<std::shared_ptr<FileSystem>>;
    using const_iterator = FSlist::const_reverse_iterator;

    /** Return an iterator on the file system at the top of the stack. */
    const_iterator fs_begin() const;

    /** Return an iterator on the bottom of the stack, past the last file
     * system. */
    const_iterator fs_end() const;

private:
    FSlist FSs;
};

}  // namespace souffle
