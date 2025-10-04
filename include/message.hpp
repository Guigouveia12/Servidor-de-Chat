#ifndef MESSAGE_HPP
#define MESSAGE_HPP


#include <string>


struct Message {
std::string from;
std::string to;
std::string body;
std::chrono::system_clock::time_point ts;
};


#endif
