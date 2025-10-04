#ifndef CHATROOM_HPP
#define CHATROOM_HPP


#include <string>
#include <vector>
#include <mutex>


struct ClientInfo {
int sock;
std::string username;
};


class ChatRoom {
public:
void join(const ClientInfo& client);
void leave(int sock);
void broadcast(const std::string& msg, int from_sock);


private:
std::mutex mtx_;
std::vector<ClientInfo> clients_;
};

#endif
