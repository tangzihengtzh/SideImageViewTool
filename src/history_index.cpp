#include "history_index.h"
#include <fstream>
namespace sd {
bool HistoryIndex::load(const fs::path& file) {
    file_=file;hidden_.clear();std::error_code ec;
    if(!fs::exists(file_,ec))return !ec;
    std::ifstream input(file_,std::ios::binary);if(!input)return false;
    std::string name;
    while(std::getline(input,name)) {
        if(!name.empty() && name.back()=='\r')name.pop_back();
        if(name.empty())continue;
        // Store only UTF-8 basenames, so moving the portable folder keeps exclusions valid.
        if(name.find_first_of("/\\")!=std::string::npos || name=="." || name==".." || name.find('\0')!=std::string::npos)return false;
        hidden_.insert(name);
    }
    return !input.bad();
}
bool HistoryIndex::write(const std::set<std::string>& values) const {
    if(file_.empty())return false;
    fs::path temporary=file_.wstring()+L".tmp";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output)return false;
    for(const auto& name:values)output<<name<<'\n';
    output.close();if(!output)return false;
    // Only the application's index is replaced; no screenshot file is touched.
    return MoveFileExW(temporary.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
bool HistoryIndex::contains(const fs::path& image) const {return hidden_.count(image.filename().u8string())!=0;}
bool HistoryIndex::hide(const fs::path& image) {
    auto next=hidden_;next.insert(image.filename().u8string());
    if(next==hidden_)return true;if(!write(next))return false;hidden_=std::move(next);return true;
}
bool HistoryIndex::restoreAll() {if(!write({}))return false;hidden_.clear();return true;}
}
