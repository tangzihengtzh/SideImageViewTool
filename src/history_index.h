#pragma once
#include "core.h"
#include <set>
namespace sd {
class HistoryIndex {
    fs::path file_;
    std::set<std::string> hidden_;
    bool write(const std::set<std::string>& values) const;
public:
    bool load(const fs::path& file);
    bool contains(const fs::path& image) const;
    bool hide(const fs::path& image);
    bool restoreAll();
    size_t size() const {return hidden_.size();}
};
}
