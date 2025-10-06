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
#include <iostream>

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
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            e.ts.time_since_epoch()) % 1000;
        std::tm tm;
#if defined(_MSC_VER)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << level_to_string(e.level) << "]"
            << " [TID:" << e.tid << "] "
            << e.message << "\n";

        if (to_stdout) {
            std::cout << oss.str() << std::flush;
        } else if (ofs && ofs->is_open()) {
            (*ofs) << oss.str() << std::flush;
        }
    }
};

Logger::Logger() : pimpl(std::make_unique<Impl>()) {}
Logger::~Logger() { shutdown(); }

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& filename, Level level) {
    std::lock_guard<std::mutex> lg(pimpl->mtx);
    if (pimpl->running.load()) return;

    pimpl->min_level.store(level);

    if (filename == "stdout") {
        pimpl->to_stdout = true;
    } else {
        pimpl->ofs = std::make_unique<std::ofstream>(filename, std::ios::app);
        if (!pimpl->ofs->is_open()) {
            throw std::runtime_error("Não foi possível abrir arquivo de log: " + filename);
        }
    }

    pimpl->running.store(true);
    pimpl->worker = std::thread(&Impl::worker_loop, pimpl.get());
}

void Logger::log(Level level, const std::string& msg) {
    if (level < pimpl->min_level.load()) return;

    LogEntry e;
    e.level = level;
    e.message = msg;
    e.ts = std::chrono::system_clock::now();
    e.tid = std::this_thread::get_id();

    {
        std::lock_guard<std::mutex> lg(pimpl->mtx);
        pimpl->q.push(std::move(e));
    }
    pimpl->cv.notify_one();
}

void Logger::debug(const std::string& msg) { log(Level::DEBUG, msg); }
void Logger::info(const std::string& msg)  { log(Level::INFO, msg); }
void Logger::warn(const std::string& msg)  { log(Level::WARN, msg); }
void Logger::error(const std::string& msg) { log(Level::ERROR, msg); }

void Logger::shutdown() {
    if (!pimpl->running.load()) return;
    pimpl->running.store(false);
    pimpl->cv.notify_all();
    if (pimpl->worker.joinable()) pimpl->worker.join();
    if (pimpl->ofs && pimpl->ofs->is_open()) {
        pimpl->ofs->close();
    }
}

void Logger::set_level(Level level) {
    pimpl->min_level.store(level);
}

std::string level_to_string(Level l) {
    switch(l) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO ";
        case Level::WARN:  return "WARN ";
        case Level::ERROR: return "ERROR";
        default: return "?????";
    }
}

}
