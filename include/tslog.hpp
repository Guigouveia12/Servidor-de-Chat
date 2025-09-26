#ifndef TSLOG_HPP
#define TSLOG_HPP


#include <string>
#include <memory>


namespace tslog {


enum class Level { DEBUG = 0, INFO, WARN, ERROR };


class Logger {
public:
static Logger& instance();

void init(const std::string& filename, Level level = Level::DEBUG);

void log(Level level, const std::string& msg);

void debug(const std::string& msg);
void info(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& msg);

void shutdown();

void set_level(Level level);

private:
Logger();
~Logger();

Logger(const Logger&) = delete;
Logger& operator=(const Logger&) = delete;


struct Impl;
std::unique_ptr<Impl> pimpl;
};

std::string level_to_string(Level l);

}

#endif
