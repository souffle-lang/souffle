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
    virtual bool exists(const std::filesystem::path&) = 0;
    virtual std::filesystem::path canonical(const std::filesystem::path&, std::error_code&) = 0;
    virtual std::string readFile(const std::filesystem::path&, std::error_code&) = 0;
};

class RealFileSystem : public FileSystem {
  public:
    bool exists(const std::filesystem::path&) override;
    std::filesystem::path canonical(const std::filesystem::path&, std::error_code&) override;
    std::string readFile(const std::filesystem::path&, std::error_code&) override;
};

class OverlayFileSystem : public FileSystem {
  public:
    OverlayFileSystem(std::shared_ptr<FileSystem> base);

    bool exists(const std::filesystem::path&) override;
    std::filesystem::path canonical(const std::filesystem::path&, std::error_code&) override;
    std::string readFile(const std::filesystem::path&, std::error_code&) override;

    void pushOverlay(std::shared_ptr<FileSystem> fs);

    using FSlist = std::list<std::shared_ptr<FileSystem>>;
    using const_iterator = FSlist::const_reverse_iterator;

    const_iterator fs_begin() const;
    const_iterator fs_end() const;

  private:
    FSlist FSs;
};

}

