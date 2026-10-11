#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sd {
namespace fs = std::filesystem;
template<class T> using Com = Microsoft::WRL::ComPtr<T>;
struct Pixels { UINT w=0, h=0; std::vector<uint8_t> bgra; bool valid() const; };
using Picture = std::shared_ptr<Pixels>;
enum class Edge { Left, Right, Top, Bottom };
struct Monitor { RECT work{}; RECT screen{}; std::wstring name; };
RECT normalized(POINT a, POINT b);
RECT intersect(RECT a, RECT b);
RECT dockRect(RECT work, Edge edge, bool expanded, int dpi);
float zoomValue(float current, int delta);
Picture crop(const Pixels& source, RECT area);
Picture thumbnail(const Pixels& source, UINT maxSide=360);
Picture readImage(const fs::path& path, UINT maxSide=0);
bool savePng(const Pixels& image, const fs::path& path);
Picture captureDesktop(RECT area);
bool copyPicture(HWND owner, const Pixels& image);
Picture pastePicture(HWND owner);
std::vector<Monitor> monitors();
std::wstring lastError(DWORD error=GetLastError());
std::wstring stamp();
std::wstring friendlyTime(const fs::path& file);
class Worker {
    std::thread thread_; std::mutex mutex_; std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_; bool stopping_=false;
public:
    Worker(); ~Worker();
    void push(std::function<void()> fn);
};
}
