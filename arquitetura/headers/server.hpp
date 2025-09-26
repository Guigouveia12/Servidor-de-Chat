#ifndef CHAT_SERVER_HPP
#define CHAT_SERVER_HPP


#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>


class ChatServer {
public:
ChatServer(int port);
~ChatServer();
void start();
void stop();


private:
void accept_loop();
void client_handler(int client_sock);


int port_;
int listen_sock_;
std::atomic<bool> running_;

std::mutex clients_mtx_; _
std::vector<int> clients_;
};


#endif
