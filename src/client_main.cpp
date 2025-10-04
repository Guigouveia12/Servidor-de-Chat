#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "tslog.hpp"

using namespace tslog;

std::atomic<bool> running{true};

void reader_thread_fn(int sockfd) {
    char buf[4096];
    while (running.load()) {
        ssize_t n = recv(sockfd, buf, sizeof(buf)-1, 0);
        if (n <= 0) {
            if (n == 0) Logger::instance().info("Servidor fechou a conexão");
            else Logger::instance().error("recv() erro no cliente");
            running.store(false);
            break;
        }
        buf[n] = '\0';
        std::cout << buf;
        std::cout.flush();
    }
}

int main(int argc, char** argv) {
    std::string host = (argc > 1) ? argv[1] : "127.0.0.1";
    std::string port = (argc > 2) ? argv[2] : "12345";

    Logger::instance().init("stdout", Level::DEBUG);
    Logger::instance().info("Cliente iniciando: conectando em " + host + ":" + port);

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
        Logger::instance().error("getaddrinfo() falhou");
        return 1;
    }

    int sockfd = -1;
    struct addrinfo *p;
    for (p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);
    if (sockfd < 0) {
        Logger::instance().error("Não foi possível conectar ao servidor");
        return 1;
    }

    std::thread reader(reader_thread_fn, sockfd);

    std::string line;
    while (running.load() && std::getline(std::cin, line)) {
        if (line == "/quit" || line == "/exit") {
            running.store(false);
            break;
        }
        if (line.empty()) continue;
        if (line.back() != '\n') line.push_back('\n');
        ssize_t n = send(sockfd, line.data(), line.size(), 0);
        if (n <= 0) {
            Logger::instance().error("send() falhou");
            running.store(false);
            break;
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    if (reader.joinable()) reader.join();
    Logger::instance().shutdown();
    std::cout << "Cliente encerrado.\n";
    return 0;
}
