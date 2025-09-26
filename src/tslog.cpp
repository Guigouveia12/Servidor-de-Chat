#include "tslog.hpp"


#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>


namespace tslog {


struct LogEntry {
Level level;
std::string message;
std::chrono::system_clock::time_point ts;
std::thread::id tid;
};


struct Logger::Impl {
std::mutex mtx;
std::condition_variable cv;
std::queue<LogEntry> q;
std::thread worker;
std::atomic<bool> running{false};
std::atomic<Level> min_level{Level::DEBUG};
std::unique_ptr<std::ofstream> ofs;
bool to_stdout{false};


Impl() = default;


void worker_loop() {
std::unique_lock<std::mutex> lock(mtx);
while (running.load() || !q.empty()) {
if (q.empty()) {
cv.wait(lock, [this]{ return !running.load() || !q.empty(); });
}
while (!q.empty()) {
LogEntry e = std::move(q.front());
q.pop();
lock.unlock();
write_entry(e);
lock.lock();
}
}
if (ofs && ofs->is_open()) ofs->flush();
}


void write_entry(const LogEntry& e) {
auto tt = std::chrono::system_clock::to_time_t(e.ts);
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(e.ts.time_since_epoch()) % 1000;
std::tm tm;
#if defined(_MSC_VER)
localtime_s(&tm, &tt);
#else
localtime_r(&tt, &tm);
}
