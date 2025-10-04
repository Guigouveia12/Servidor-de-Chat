#ifndef CHAT_CLIENT_HPP
#define CHAT_CLIENT_HPP


#include <string>


class ChatClient {
public:
ChatClient(const std::string& host, int port);
~ChatClient();
void run_cli();


private:
std::string host_;
int port_;
int sock_;
};


#endif
