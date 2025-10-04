// src/server_main.cpp
// Compilar: parte do CMakeLists abaixo
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <csignal>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "tslog.hpp"

constexpr int DEFAULT_PORT = 12345;
constexpr int BACKLOG = 10;
constexpr size_t BUF_SIZE = 4096;

using namespace tslog;

struct ClientInfo {
    int fd;
    std::string addr;
    std::thread thr;
};

std::mutex clients_mtx;
std::vector<std::shared_ptr<ClientInfo>> clients;
std::atomic<bool> running{true};
int listen_fd = -1;

void broadcast_message(const std::string &msg, int except_fd = -1) {
    std::lock_guard<std::mutex> lg(clients_mtx);
    for (auto &c : clients) {
        if (!c) continue;
        if (c->fd == except_fd) continue;
        ssize_t n = send(c->fd, msg.data(), msg.size(), 0);
        if (n <= 0) {
            Logger::instance().error("send() erro para " + c->addr + " (fd " + std::to_string(c->fd) + ")");
        }
    }
    Logger::instance().debug("Broadcast feito: " + msg);
}

void remove_client(int fd) {
    std::lock_guard<std::mutex> lg(clients_mtx);
    clients.erase(std::remove_if(clients.begin(), clients.end(),
        [fd](const std::shared_ptr<ClientInfo>& c){ return !c || c->fd == fd; }),
        clients.end());
}

void handle_client(std::shared_ptr<ClientInfo> ci) {
    Logger::instance().info("Thread cliente iniciada para " + ci->addr + " (fd " + std::to_string(ci->fd) + ")");
    char buf[BUF_SIZE];
    while (running.load()) {
        ssize_t n = recv(ci->fd, buf, sizeof(buf)-1, 0);
        if (n <= 0) {
            if (n == 0) {
                Logger::instance().info("Cliente desconectado: " + ci->addr);
            } else {
                Logger::instance().error("recv() erro para " + ci->addr);
            }
            close(ci->fd);
            remove_client(ci->fd);
            return;
        }
        buf[n] = '\0';
        std::string msg(buf);

        msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());

        std::string out = "[" + ci->addr + "] " + msg;
        Logger::instance().info("Recebido: " + out);

        std::string filter = "banword";
        if (msg.find(filter) != std::string::npos) {
            std::string notice = "Mensagem bloqueada por filtro.\n";
            send(ci->fd, notice.data(), notice.size(), 0);
            Logger::instance().info("Mensagem de " + ci->addr + " bloqueada por filtro.");
            continue;
        }

        if (out.back() != '\n') out += '\n';
        broadcast_message(out, ci->fd);
    }
}

void sigint_handler(int) {
    running.store(false);
    Logger::instance().info("Sinal recebido: encerrando servidor...");
    if (listen_fd >= 0) close(listen_fd);
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : DEFAULT_PORT;
    Logger::instance().init("stdout", Level::DEBUG);
    Logger::instance().info("Iniciando servidor de chat (etapa2) na porta " + std::to_string(port));

    std::signal(SIGINT, sigint_handler);
    std::signal(SIGTERM, sigint_handler);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        Logger::instance().error("socket() falhou");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr*)&srv, sizeof(srv)) < 0) {
        Logger::instance().error("bind() falhou");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        Logger::instance().error("listen() falhou");
        close(listen_fd);
        return 1;
    }

    Logger::instance().info("Servidor escutando...");

    while (running.load()) {
        sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(listen_fd, (sockaddr*)&cli, &cli_len);
        if (cfd < 0) {
            if (!running.load()) break;
            Logger::instance().error("accept() falhou");
            continue;
        }
        std::string cli_addr = std::string(inet_ntoa(cli.sin_addr)) + ":" + std::to_string(ntohs(cli.sin_port));
        Logger::instance().info("Nova conexão de " + cli_addr + " (fd " + std::to_string(cfd) + ")");

        auto ci = std::make_shared<ClientInfo>();
        ci->fd = cfd;
        ci->addr = cli_addr;

        {
            std::lock_guard<std::mutex> lg(clients_mtx);
            clients.push_back(ci);
        }

        ci->thr = std::thread(&handle_client, ci);
        ci->thr.detach();
    }

    {
        std::lock_guard<std::mutex> lg(clients_mtx);
        for (auto &c : clients) {
            if (!c) continue;
            close(c->fd);
        }
        clients.clear();
    }

    if (listen_fd >= 0) close(listen_fd);
    Logger::instance().shutdown();
    std::cout << "Servidor encerrado.\n";
    return 0;
}
