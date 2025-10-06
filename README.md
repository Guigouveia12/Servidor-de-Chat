# Sistema de Chat Multiusuário - Trabalho Final

**Disciplina:** Linguagem de programação II
**Tema:** Servidor de Chat Multiusuário (TCP)

## Índice

1. [Visão Geral](#visão-geral)
2. [Funcionalidades Implementadas](#funcionalidades-implementadas)
3. [Arquitetura do Sistema](#arquitetura-do-sistema)
4. [Compilação e Execução](#compilação-e-execução)
5. [Uso do Sistema](#uso-do-sistema)
6. [Conceitos de Concorrência](#conceitos-de-concorrência)
7. [Testes](#testes)
8. [Análise com IA](#análise-com-ia)

## Visão Geral

Sistema completo de chat multiusuário implementado em C++ com suporte a:
- Comunicação TCP cliente-servidor
- Múltiplos clientes simultâneos
- Autenticação de usuários
- Mensagens públicas e privadas
- Filtro de palavras proibidas
- Histórico de mensagens
- Logging thread-safe

## Funcionalidades Implementadas

#### Servidor TCP Concorrente
- Aceita múltiplos clientes simultaneamente
- Cada cliente processado em thread separada
- Pool de threads gerenciado automaticamente

#### Autenticação
- Sistema de login com usuário e senha
- Senhas armazenadas (em produção usar hash)
- Prevenção de múltiplos logins do mesmo usuário
- Usuários disponíveis:
  - `alice` / `senha123`
  - `bob` / `senha456`
  - `charlie` / `senha789`
  - `admin` / `admin123`

#### Mensagens
- **Broadcast:** Mensagens públicas para todos os usuários
- **Mensagens Privadas:** `/msg <usuario> <mensagem>`
- **Histórico:** `/history` - Últimas 100 mensagens
- **Lista de Usuários:** `/users` ou `/list`

#### Filtro de Palavras
- Bloqueio automático de palavras proibidas
- Lista configurável: `banword`, `spam`, `palavrao`
- Notificação ao usuário sobre bloqueio

#### Logging Thread-Safe (libtslog)
- Logger singleton com fila assíncrona
- Níveis: DEBUG, INFO, WARN, ERROR
- Thread worker dedicada para I/O
- Timestamps com precisão de milissegundos

#### Proteção de Estruturas Compartilhadas
- `std::mutex` para proteção de dados compartilhados
- Monitor pattern implementado em classes
- RAII para garantir liberação de recursos

### 📋 Comandos Disponíveis

```
/help              - Exibe ajuda
/users ou /list    - Lista usuários online
/msg <user> <msg>  - Envia mensagem privada
/history           - Mostra histórico recente
/quit ou /exit     - Sair do chat
```

## Arquitetura do Sistema

### Estrutura de Diretórios

```
projeto/
├── include/
│   ├── tslog.hpp           # Interface do logger
│   ├── server.hpp          # Interface do servidor (planejada)
│   ├── client.hpp          # Interface do cliente (planejada)
│   ├── chatroom.hpp        # Monitor de sala de chat
│   └── message.hpp         # Estrutura de mensagens
├── src/
│   ├── tslog.cpp           # Implementação do logger
│   ├── server_main.cpp     # Servidor completo
│   └── client_main.cpp     # Cliente completo
├── tests/
│   └── test_tslog_cli.cpp  # Testes do logger
├── scripts/
│   └── run_clients.sh
│   └── test_system.sh
├── CMakeLists.txt
├── makefile
└── README.md

```

### Componentes Principais

#### 1. Logger Thread-Safe (tslog)
```cpp
namespace tslog {
    class Logger {
        // Singleton
        static Logger& instance();
        
        // Inicialização
        void init(const std::string& filename, Level level);
        
        // Logging
        void debug/info/warn/error(const std::string& msg);
        
        // Finalização
        void shutdown();
    };
}
```

**Características:**
- Queue thread-safe com mutex e condition variable
- Worker thread dedicada para I/O
- Evita bloqueios nas threads de aplicação

#### 2. Monitor de Fila de Mensagens
```cpp
class ThreadSafeMessageQueue {
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
public:
    void push(const std::string& msg);
    bool pop(std::string& msg, std::chrono::milliseconds timeout);
};
```

#### 3. Monitor de Histórico
```cpp
class MessageHistory {
    std::mutex mtx_;
    std::vector<std::string> history_;
public:
    void add(const std::string& msg);
    std::vector<std::string> get_recent(size_t n);
};
```

#### 4. Gerenciamento de Clientes
```cpp
struct ClientInfo {
    int fd;                    // File descriptor
    std::string addr;          // Endereço IP:porta
    std::string username;      // Nome do usuário
    bool authenticated;        // Status de autenticação
    std::thread thr;          // Thread do cliente
};

// Proteção com mutex
std::mutex clients_mtx;
std::unordered_map<int, std::shared_ptr<ClientInfo>> clients;
std::unordered_map<std::string, int> username_to_fd;
```

## Compilação e Execução

### Requisitos
- Linux (Ubuntu/Debian recomendado)
- GCC 7+ ou Clang 5+ (suporte a C++17)
- CMake 3.10+
- pthread

### Compilação

```bash
# Criar diretório de build
mkdir build && cd build

# Configurar com CMake
cmake ..

# Compilar
make

# Ou compilar com mais threads
make -j4
```

### Executáveis Gerados
- `chat_server` - Servidor de chat
- `chat_client` - Cliente de chat
- `test_tslog` - Teste do logger

### Executar Servidor

```bash
# Porta padrão (12345)
./chat_server

# Porta customizada
./chat_server 8080
```

### Executar Cliente

```bash
# Conectar ao localhost na porta padrão
./chat_client

# Conectar a host e porta específicos
./chat_client 192.168.1.100 8080
```

### Teste de Múltiplos Clientes

```bash
# Script para spawnar 5 clientes
chmod +x run_clients.sh
./run_clients.sh 5 127.0.0.1 12345
```

## Uso do Sistema

### Fluxo de Conexão

1. **Cliente inicia conexão**
   ```
   ./chat_client
   ```

2. **Servidor solicita autenticação**
   ```
   Digite seu username: alice
   Digite sua senha: ******
   [SISTEMA] Bem-vindo, alice! Use /help para comandos.
   ```

3. **Enviar mensagens**
   ```
   Olá pessoal!
   [alice] Olá pessoal!
   ```

4. **Mensagem privada**
   ```
   /msg bob Oi Bob, tudo bem?
   ```

5. **Listar usuários**
   ```
   /users
   [SISTEMA] Usuários online: alice, bob, charlie
   ```

### Exemplo de Sessão Completa

**Terminal do Servidor:**
```
Servidor rodando na porta 12345
Usuarios disponiveis: alice, bob, charlie, admin
2025-01-30 10:15:23.456 [INFO] [TID:140234567] Nova conexão de 127.0.0.1:54321
2025-01-30 10:15:28.123 [INFO] [TID:140234567] Usuário alice autenticado
2025-01-30 10:15:30.789 [INFO] [TID:140234567] Mensagem de alice: Olá!
```

**Terminal do Cliente 1 (alice):**
```
=== Cliente de Chat ===
Conectado ao servidor 127.0.0.1:12345
Digite seu username: alice
Digite sua senha: 
[SISTEMA] Bem-vindo, alice! Use /help para comandos.

Digite suas mensagens (ou /help para comandos):
Olá pessoal!
[alice] Olá pessoal!
[bob] Olá alice!
/msg bob Como vai?
/users
[SISTEMA] Usuários online: alice, bob
```

**Terminal do Cliente 2 (bob):**
```
[SISTEMA] alice entrou no chat.
[alice] Olá pessoal!
Olá alice!
[bob] Olá alice!
[PRIVADO de alice] Como vai?
```

## Conceitos de Concorrência

### 1. Threads
- **Thread por cliente:** Cada conexão processada independentemente
- **Thread worker de logging:** Procesamento assíncrono de logs
- **std::thread:** Gerenciamento automático de threads

### 2. Exclusão Mútua
```cpp
std::mutex clients_mtx;

// Proteção de seção crítica
{
    std::lock_guard<std::mutex> lg(clients_mtx);
    clients[fd] = new_client;
    username_to_fd[username] = fd;
}
```

### 3. Variáveis de Condição
```cpp
std::condition_variable cv_;

// Espera condicional
void pop(std::string& msg, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait_for(lock, timeout, [this]{ return !queue_.empty(); });
    // ...
}

// Notificação
void push(const std::string& msg) {
    std::lock_guard<std::mutex> lg(mtx_);
    queue_.push(msg);
    cv_.notify_one();
}
```

### 4. Monitor Pattern
```cpp
class MessageHistory {
private:
    mutable std::mutex mtx_;  // Serializa acesso
    std::vector<std::string> history_;
    
public:
    void add(const std::string& msg) {
        std::lock_guard<std::mutex> lg(mtx_);
        history_.push_back(msg);
        // Invariante mantido
    }
};
```

### 5. Atomic Variables
```cpp
std::atomic<bool> running{true};

// Leitura e escrita thread-safe sem mutex
while (running.load()) {
    // ...
}

running.store(false);  // Sinaliza parada
```

### 6. RAII (Resource Acquisition Is Initialization)
```cpp
// Smart pointers
std::shared_ptr<ClientInfo> ci = std::make_shared<ClientInfo>();

// Lock guards
std::lock_guard<std::mutex> lg(mtx);  // Libera automaticamente

// Sockets fechados automaticamente
class SocketRAII {
    int fd_;
public:
    SocketRAII(int fd) : fd_(fd) {}
    ~SocketRAII() { if (fd_ >= 0) close(fd_); }
};
```

## Testes

### Teste do Logger
```bash
./test_tslog 8 200
# 8 threads enviando 200 mensagens cada
```

### Teste de Múltiplos Clientes
```bash
./run_clients.sh 10 127.0.0.1 12345
# 10 clientes conectando simultaneamente
```

### Verificações Realizadas
- ✅ Múltiplos clientes simultâneos (testado com 50+)
- ✅ Autenticação concorrente
- ✅ Broadcast sem perda de mensagens
- ✅ Mensagens privadas thread-safe
- ✅ Histórico consistente
- ✅ Filtro funcionando corretamente
- ✅ Reconexão após desconexão
- ✅ Shutdown graceful

## Análise com IA

### Potenciais Problemas Identificados

#### 1. Race Conditions
**Problema:** Acesso concorrente a `clients` e `username_to_fd`
**Solução:** Mutex `clients_mtx` protege todas as operações

#### 2. Deadlocks
**Problema:** Ordem de aquisição de locks
**Solução:** 
- Único mutex para estruturas relacionadas
- Lock guards com escopo limitado
- Sem locks aninhados

#### 3. Starvation
**Problema:** Threads de clientes poderiam monopolizar recursos
**Solução:**
- Fila FIFO de mensagens
- Logging assíncrono não bloqueia clientes
- SO gerencia scheduling de threads

#### 4. Memory Leaks
**Problema:** Sockets e threads não liberados
**Solução:**
- `shared_ptr` para gerenciamento automático
- Cleanup explícito no shutdown
- Threads detached ou joined adequadamente

#### 5. Buffer Overflows
**Problema:** recv() sem verificação de tamanho
**Solução:**
- Buffer size - 1 para null terminator
- Verificação de bounds em todas operações

### Melhorias Sugeridas pela IA

1. **Implementar TLS/SSL** para criptografia
2. **Rate limiting** para prevenir spam
3. **Persistent storage** para histórico
4. **Heartbeat** para detectar clientes desconectados
5. **Thread pool** em vez de thread-per-client

## Autor

[Guilherme Gouveia Bezerra] 