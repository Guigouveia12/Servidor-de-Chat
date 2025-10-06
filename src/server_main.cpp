#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <csignal>
#include <memory>
#include <queue>
#include <condition_variable>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "tslog.hpp"

constexpr int DEFAULT_PORT = 12345;
constexpr int BACKLOG = 10;
constexpr size_t BUF_SIZE = 4096;
constexpr size_t MAX_HISTORY = 100;

using namespace tslog;

// Estrutura para clientes autenticados
struct ClientInfo {
    int fd;
    std::string addr;
    std::string username;
    bool authenticated;
    std::thread thr;
};

// Monitor para gerenciar fila thread-safe de mensagens
class ThreadSafeMessageQueue {
public:
    void push(const std::string& msg) {
        std::lock_guard<std::mutex> lg(mtx_);
        queue_.push(msg);
        cv_.notify_one();
    }

    bool pop(std::string& msg, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (cv_.wait_for(lock, timeout, [this]{ return !queue_.empty(); })) {
            msg = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lg(mtx_);
        return queue_.size();
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
};

// Monitor para histórico de mensagens
class MessageHistory {
public:
    void add(const std::string& msg) {
        std::lock_guard<std::mutex> lg(mtx_);
        history_.push_back(msg);
        if (history_.size() > MAX_HISTORY) {
            history_.erase(history_.begin());
        }
    }

    std::vector<std::string> get_recent(size_t n) const {
        std::lock_guard<std::mutex> lg(mtx_);
        size_t start = history_.size() > n ? history_.size() - n : 0;
        return std::vector<std::string>(history_.begin() + start, history_.end());
    }

private:
    mutable std::mutex mtx_;
    std::vector<std::string> history_;
};

// Variáveis globais protegidas
std::mutex clients_mtx;
std::unordered_map<int, std::shared_ptr<ClientInfo>> clients;
std::unordered_map<std::string, int> username_to_fd;
std::atomic<bool> running{true};
int listen_fd = -1;
MessageHistory msg_history;
ThreadSafeMessageQueue broadcast_queue;

// Filtro de palavras proibidas
std::unordered_set<std::string> banned_words = {
    "banword", "spam", "palavrao"
};

// Senhas simples (em produção, usar hash + salt)
std::unordered_map<std::string, std::string> user_passwords = {
    {"alice", "senha123"},
    {"bob", "senha456"},
    {"charlie", "senha789"},
    {"admin", "admin123"}
};

// Função para broadcast de mensagens
void broadcast_message(const std::string& msg, int except_fd = -1) {
    std::lock_guard<std::mutex> lg(clients_mtx);
    for (auto& pair : clients) {
        auto& c = pair.second;
        if (!c || c->fd == except_fd || !c->authenticated) continue;

        ssize_t n = send(c->fd, msg.data(), msg.size(), MSG_NOSIGNAL);
        if (n <= 0) {
            Logger::instance().error("Erro ao enviar para " + c->username +
                                   " (fd " + std::to_string(c->fd) + ")");
        }
    }
}

// Enviar mensagem privada
void send_private_message(const std::string& from_user, const std::string& to_user,
                         const std::string& msg) {
    std::lock_guard<std::mutex> lg(clients_mtx);

    auto it = username_to_fd.find(to_user);
    if (it == username_to_fd.end()) {
        auto from_it = username_to_fd.find(from_user);
        if (from_it != username_to_fd.end()) {
            std::string err = "[SISTEMA] Usuário '" + to_user + "' não encontrado.\n";
            send(from_it->second, err.data(), err.size(), MSG_NOSIGNAL);
        }
        return;
    }

    int to_fd = it->second;
    std::string pm = "[PRIVADO de " + from_user + "] " + msg + "\n";
    ssize_t n = send(to_fd, pm.data(), pm.size(), MSG_NOSIGNAL);

    if (n > 0) {
        Logger::instance().info("Mensagem privada de " + from_user + " para " + to_user);
    }
}

// Verificar filtro de palavras
bool contains_banned_word(const std::string& msg) {
    std::string lower_msg = msg;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

    for (const auto& word : banned_words) {
        if (lower_msg.find(word) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Remover cliente
void remove_client(int fd) {
    std::lock_guard<std::mutex> lg(clients_mtx);

    auto it = clients.find(fd);
    if (it != clients.end() && it->second) {
        username_to_fd.erase(it->second->username);
        clients.erase(it);
    }
}

// Listar usuários online
std::string list_online_users() {
    std::lock_guard<std::mutex> lg(clients_mtx);
    std::ostringstream oss;
    oss << "[SISTEMA] Usuários online: ";

    bool first = true;
    for (const auto& pair : clients) {
        if (pair.second && pair.second->authenticated) {
            if (!first) oss << ", ";
            oss << pair.second->username;
            first = false;
        }
    }
    oss << "\n";
    return oss.str();
}

// Processar comandos
bool process_command(std::shared_ptr<ClientInfo> ci, const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string command;
    iss >> command;

    if (command == "/quit" || command == "/exit") {
        return false;
    }
    else if (command == "/users" || command == "/list") {
        std::string list = list_online_users();
        send(ci->fd, list.data(), list.size(), MSG_NOSIGNAL);
    }
    else if (command == "/msg" || command == "/pm") {
        std::string to_user, message;
        iss >> to_user;
        std::getline(iss, message);
        if (!message.empty() && message[0] == ' ') message = message.substr(1);

        if (to_user.empty() || message.empty()) {
            std::string err = "[SISTEMA] Uso: /msg <usuario> <mensagem>\n";
            send(ci->fd, err.data(), err.size(), MSG_NOSIGNAL);
        } else {
            send_private_message(ci->username, to_user, message);
        }
    }
    else if (command == "/history") {
        auto recent = msg_history.get_recent(10);
        std::string hist = "[SISTEMA] Últimas mensagens:\n";
        for (const auto& msg : recent) {
            hist += msg;
        }
        send(ci->fd, hist.data(), hist.size(), MSG_NOSIGNAL);
    }
    else if (command == "/help") {
        std::string help =
            "[SISTEMA] Comandos disponíveis:\n"
            "  /users, /list - Listar usuários online\n"
            "  /msg, /pm <user> <msg> - Mensagem privada\n"
            "  /history - Ver histórico recente\n"
            "  /help - Esta ajuda\n"
            "  /quit, /exit - Sair\n";
        send(ci->fd, help.data(), help.size(), MSG_NOSIGNAL);
    }
    else {
        std::string err = "[SISTEMA] Comando desconhecido. Use /help\n";
        send(ci->fd, err.data(), err.size(), MSG_NOSIGNAL);
    }

    return true;
}

// Autenticação do cliente
bool authenticate_client(std::shared_ptr<ClientInfo> ci) {
    char buf[256];

    // Solicitar username
    std::string prompt = "Digite seu username: ";
    send(ci->fd, prompt.data(), prompt.size(), MSG_NOSIGNAL);

    ssize_t n = recv(ci->fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';

    std::string username(buf);
    username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());
    username.erase(std::remove(username.begin(), username.end(), '\r'), username.end());

    // Solicitar senha
    prompt = "Digite sua senha: ";
    send(ci->fd, prompt.data(), prompt.size(), MSG_NOSIGNAL);

    n = recv(ci->fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';

    std::string password(buf);
    password.erase(std::remove(password.begin(), password.end(), '\n'), password.end());
    password.erase(std::remove(password.begin(), password.end(), '\r'), password.end());

    // Verificar credenciais
    auto it = user_passwords.find(username);
    if (it == user_passwords.end() || it->second != password) {
        std::string err = "[SISTEMA] Autenticação falhou!\n";
        send(ci->fd, err.data(), err.size(), MSG_NOSIGNAL);
        Logger::instance().warn("Falha de autenticação para username: " + username);
        return false;
    }

    // Verificar se usuário já está online
    {
        std::lock_guard<std::mutex> lg(clients_mtx);
        if (username_to_fd.find(username) != username_to_fd.end()) {
            std::string err = "[SISTEMA] Usuário já está online!\n";
            send(ci->fd, err.data(), err.size(), MSG_NOSIGNAL);
            return false;
        }
        username_to_fd[username] = ci->fd;
    }

    ci->username = username;
    ci->authenticated = true;

    std::string welcome = "[SISTEMA] Bem-vindo, " + username + "! Use /help para comandos.\n";
    send(ci->fd, welcome.data(), welcome.size(), MSG_NOSIGNAL);

    // Notificar outros usuários
    std::string join_msg = "[SISTEMA] " + username + " entrou no chat.\n";
    broadcast_message(join_msg, ci->fd);
    msg_history.add(join_msg);

    Logger::instance().info("Usuário " + username + " autenticado com sucesso");
    return true;
}

// Thread para lidar com cliente
void handle_client(std::shared_ptr<ClientInfo> ci) {
    Logger::instance().info("Conexão de " + ci->addr + " (fd " + std::to_string(ci->fd) + ")");

    // Autenticar cliente
    if (!authenticate_client(ci)) {
        close(ci->fd);
        remove_client(ci->fd);
        return;
    }

    char buf[BUF_SIZE];
    while (running.load()) {
        ssize_t n = recv(ci->fd, buf, sizeof(buf)-1, 0);
        if (n <= 0) {
            if (n == 0) {
                Logger::instance().info("Cliente " + ci->username + " desconectou");
            } else {
                Logger::instance().error("Erro recv() para " + ci->username);
            }
            break;
        }

        buf[n] = '\0';
        std::string msg(buf);
        msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());
        if (!msg.empty() && msg.back() == '\n') msg.pop_back();

        // Processar comandos
        if (!msg.empty() && msg[0] == '/') {
            if (!process_command(ci, msg)) break;
            continue;
        }

        // Verificar filtro
        if (contains_banned_word(msg)) {
            std::string notice = "[SISTEMA] Mensagem bloqueada: contém palavra proibida.\n";
            send(ci->fd, notice.data(), notice.size(), MSG_NOSIGNAL);
            Logger::instance().warn("Mensagem de " + ci->username + " bloqueada por filtro");
            continue;
        }

        // Broadcast da mensagem
        std::string full_msg = "[" + ci->username + "] " + msg + "\n";
        Logger::instance().info("Mensagem de " + ci->username + ": " + msg);

        broadcast_message(full_msg, ci->fd);
        msg_history.add(full_msg);
    }

    // Notificar saída
    std::string leave_msg = "[SISTEMA] " + ci->username + " saiu do chat.\n";
    broadcast_message(leave_msg);
    msg_history.add(leave_msg);

    close(ci->fd);
    remove_client(ci->fd);
}

void sigint_handler(int) {
    running.store(false);
    Logger::instance().info("Sinal de interrupção recebido");
    if (listen_fd >= 0) {
        shutdown(listen_fd, SHUT_RDWR);
        close(listen_fd);
    }
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : DEFAULT_PORT;

    Logger::instance().init("server.log", Level::DEBUG);
    Logger::instance().info("=== Servidor de Chat Iniciando ===");
    Logger::instance().info("Porta: " + std::to_string(port));

    std::signal(SIGINT, sigint_handler);
    std::signal(SIGTERM, sigint_handler);
    std::signal(SIGPIPE, SIG_IGN);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        Logger::instance().error("Falha ao criar socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr*)&srv, sizeof(srv)) < 0) {
        Logger::instance().error("Falha no bind()");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        Logger::instance().error("Falha no listen()");
        close(listen_fd);
        return 1;
    }

    Logger::instance().info("Servidor escutando na porta " + std::to_string(port));
    std::cout << "Servidor rodando na porta " << port << std::endl;
    std::cout << "Usuarios disponiveis: alice, bob, charlie, admin" << std::endl;
    std::cout << "Senhas: senha123, senha456, senha789, admin123" << std::endl;

    while (running.load()) {
        sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(listen_fd, (sockaddr*)&cli, &cli_len);

        if (cfd < 0) {
            if (!running.load()) break;
            Logger::instance().error("Falha no accept()");
            continue;
        }

        std::string cli_addr = std::string(inet_ntoa(cli.sin_addr)) +
                              ":" + std::to_string(ntohs(cli.sin_port));

        auto ci = std::make_shared<ClientInfo>();
        ci->fd = cfd;
        ci->addr = cli_addr;
        ci->authenticated = false;

        {
            std::lock_guard<std::mutex> lg(clients_mtx);
            clients[cfd] = ci;
        }

        ci->thr = std::thread(&handle_client, ci);
        ci->thr.detach();
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lg(clients_mtx);
        for (auto& pair : clients) {
            if (pair.second) close(pair.second->fd);
        }
        clients.clear();
    }

    if (listen_fd >= 0) close(listen_fd);
    Logger::instance().info("Servidor encerrado");
    Logger::instance().shutdown();

    std::cout << "Servidor encerrado com sucesso.\n";
    return 0;
}
