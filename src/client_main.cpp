#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>
#include <termios.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "tslog.hpp"

using namespace tslog;

std::atomic<bool> running{true};

// Função para ler senha sem exibir caracteres
std::string read_password() {
    std::string password;
    struct termios oldt, newt;

    // Desabilitar echo
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::getline(std::cin, password);

    // Restaurar echo
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;

    return password;
}

// Thread para receber mensagens do servidor
void reader_thread_fn(int sockfd) {
    char buf[4096];
    while (running.load()) {
        ssize_t n = recv(sockfd, buf, sizeof(buf)-1, 0);
        if (n <= 0) {
            if (n == 0) {
                std::cout << "\n[SISTEMA] Conexão fechada pelo servidor.\n";
            } else {
                if (running.load()) {
                    Logger::instance().error("Erro ao receber dados");
                }
            }
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

    Logger::instance().init("client.log", Level::INFO);
    Logger::instance().info("Cliente iniciando: " + host + ":" + port);

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
        std::cerr << "Erro: não foi possível resolver o endereço.\n";
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
        std::cerr << "Erro: não foi possível conectar ao servidor.\n";
        Logger::instance().error("Falha ao conectar");
        return 1;
    }

    std::cout << "=== Cliente de Chat ===" << std::endl;
    std::cout << "Conectado ao servidor " << host << ":" << port << std::endl;
    Logger::instance().info("Conectado com sucesso");

    // Processo de autenticação
    char buf[256];

    // Receber prompt de username
    ssize_t n = recv(sockfd, buf, sizeof(buf)-1, 0);
    if (n <= 0) {
        std::cerr << "Erro na comunicação durante autenticação.\n";
        close(sockfd);
        return 1;
    }
    buf[n] = '\0';
    std::cout << buf;

    // Enviar username
    std::string username;
    std::getline(std::cin, username);
    username += "\n";
    send(sockfd, username.data(), username.size(), 0);

    // Receber prompt de senha
    n = recv(sockfd, buf, sizeof(buf)-1, 0);
    if (n <= 0) {
        std::cerr << "Erro na comunicação durante autenticação.\n";
        close(sockfd);
        return 1;
    }
    buf[n] = '\0';
    std::cout << buf;

    // Enviar senha (sem exibir)
    std::string password = read_password();
    password += "\n";
    send(sockfd, password.data(), password.size(), 0);

    // Receber resposta de autenticação
    n = recv(sockfd, buf, sizeof(buf)-1, 0);
    if (n <= 0) {
        std::cerr << "Erro na resposta de autenticação.\n";
        close(sockfd);
        return 1;
    }
    buf[n] = '\0';
    std::cout << buf;

    // Verificar se autenticação foi bem-sucedida
    std::string auth_response(buf);
    if (auth_response.find("falhou") != std::string::npos ||
        auth_response.find("já está online") != std::string::npos) {
        close(sockfd);
        Logger::instance().error("Autenticação falhou");
        return 1;
    }

    Logger::instance().info("Autenticado com sucesso");
    std::cout << "\nDigite suas mensagens (ou /help para comandos):\n";

    // Iniciar thread de leitura
    std::thread reader(reader_thread_fn, sockfd);

    // Loop principal de envio
    std::string line;
    while (running.load() && std::getline(std::cin, line)) {
        if (line == "/quit" || line == "/exit") {
            std::cout << "Encerrando cliente...\n";
            running.store(false);
            break;
        }

        if (line.empty()) continue;

        line += "\n";
        ssize_t n = send(sockfd, line.data(), line.size(), 0);
        if (n <= 0) {
            Logger::instance().error("Erro ao enviar mensagem");
            running.store(false);
            break;
        }
    }

    // Cleanup
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    if (reader.joinable()) reader.join();

    Logger::instance().info("Cliente encerrado");
    Logger::instance().shutdown();

    std::cout << "Cliente desconectado.\n";
    return 0;
}
