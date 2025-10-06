# Makefile para Sistema de Chat Multiusuário
# Alternativa ao CMake

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -I./include
LDFLAGS = -pthread

# Diretórios
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = tests

# Arquivos fonte
TSLOG_SRC = $(SRC_DIR)/tslog.cpp
SERVER_SRC = $(SRC_DIR)/server_main.cpp
CLIENT_SRC = $(SRC_DIR)/client_main.cpp
TEST_SRC = $(TEST_DIR)/test_tslog_cli.cpp

# Objetos
TSLOG_OBJ = $(BUILD_DIR)/tslog.o
SERVER_OBJ = $(BUILD_DIR)/server_main.o
CLIENT_OBJ = $(BUILD_DIR)/client_main.o
TEST_OBJ = $(BUILD_DIR)/test_tslog_cli.o

# Executáveis
SERVER_BIN = $(BIN_DIR)/chat_server
CLIENT_BIN = $(BIN_DIR)/chat_client
TEST_BIN = $(BIN_DIR)/test_tslog

# Alvos principais
.PHONY: all clean directories test run-server run-client

all: directories $(SERVER_BIN) $(CLIENT_BIN) $(TEST_BIN)

directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Biblioteca tslog
$(TSLOG_OBJ): $(TSLOG_SRC) $(INC_DIR)/tslog.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Servidor
$(SERVER_OBJ): $(SERVER_SRC) $(INC_DIR)/tslog.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SERVER_BIN): $(SERVER_OBJ) $(TSLOG_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Cliente
$(CLIENT_OBJ): $(CLIENT_SRC) $(INC_DIR)/tslog.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CLIENT_BIN): $(CLIENT_OBJ) $(TSLOG_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Teste
$(TEST_OBJ): $(TEST_SRC) $(INC_DIR)/tslog.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_OBJ) $(TSLOG_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compilação com debug
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: all

# Compilação com otimização
release: CXXFLAGS += -O3 -DNDEBUG
release: all

# Limpeza
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	rm -f *.log client_*.log

# Executar servidor
run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

# Executar cliente
run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

# Executar testes
test: $(TEST_BIN)
	./$(TEST_BIN) 8 200

# Ajuda
help:
	@echo "Alvos disponíveis:"
	@echo "  all          - Compilar tudo (padrão)"
	@echo "  debug        - Compilar com símbolos de debug"
	@echo "  release      - Compilar com otimizações"
	@echo "  clean        - Remover arquivos compilados"
	@echo "  test         - Executar testes"
	@echo "  run-server   - Executar servidor"
	@echo "  run-client   - Executar cliente"
	@echo "  help         - Mostrar esta ajuda"