#!/usr/bin/env bash

# Script de Testes Completo para Sistema de Chat
# Testa funcionalidades, concorrência e robustez

set -e

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
HOST=${1:-127.0.0.1}
PORT=${2:-12345}
NUM_CLIENTS=${3:-10}
SERVER_BIN="./bin/chat_server"
CLIENT_BIN="./bin/chat_client"
LOG_DIR="test_logs"

# Funções auxiliares
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERRO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[AVISO]${NC} $1"
}

# Criar diretório de logs
mkdir -p "$LOG_DIR"

# Limpar logs antigos
cleanup() {
    log_info "Limpando ambiente..."
    pkill -f chat_server 2>/dev/null || true
    pkill -f chat_client 2>/dev/null || true
    rm -f server.log client*.log
    sleep 1
}

# Verificar se binários existem
check_binaries() {
    log_info "Verificando binários..."
    if [ ! -f "$SERVER_BIN" ]; then
        log_error "Servidor não encontrado: $SERVER_BIN"
        exit 1
    fi
    if [ ! -f "$CLIENT_BIN" ]; then
        log_error "Cliente não encontrado: $CLIENT_BIN"
        exit 1
    fi
    log_success "Binários encontrados"
}

# Iniciar servidor
start_server() {
    log_info "Iniciando servidor na porta $PORT..."
    $SERVER_BIN $PORT > "$LOG_DIR/server.log" 2>&1 &
    SERVER_PID=$!
    sleep 2
    
    if ps -p $SERVER_PID > /dev/null; then
        log_success "Servidor iniciado (PID: $SERVER_PID)"
        return 0
    else
        log_error "Falha ao iniciar servidor"
        return 1
    fi
}

# Parar servidor
stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        log_info "Parando servidor..."
        kill -SIGINT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        log_success "Servidor parado"
    fi
}

# Teste 1: Conexão básica
test_basic_connection() {
    log_info "=== Teste 1: Conexão Básica ==="
    
    (
        echo "alice"
        echo "senha123"
        sleep 1
        echo "Teste de mensagem"
        sleep 1
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test1_client.log" 2>&1 &
    
    local pid=$!
    wait $pid
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 143 ]; then
        log_success "Teste 1: PASSOU"
        return 0
    else
        log_error "Teste 1: FALHOU (exit code: $exit_code)"
        return 1
    fi
}

# Teste 2: Autenticação inválida
test_invalid_auth() {
    log_info "=== Teste 2: Autenticação Inválida ==="
    
    (
        echo "usuario_invalido"
        echo "senha_errada"
        sleep 1
    ) | timeout 5 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test2_client.log" 2>&1 &
    
    local pid=$!
    wait $pid
    
    if grep -q "falhou" "$LOG_DIR/test2_client.log"; then
        log_success "Teste 2: PASSOU (autenticação rejeitada)"
        return 0
    else
        log_error "Teste 2: FALHOU"
        return 1
    fi
}

# Teste 3: Múltiplos clientes simultâneos
test_multiple_clients() {
    log_info "=== Teste 3: Múltiplos Clientes ($NUM_CLIENTS clientes) ==="
    
    local pids=()
    local users=("alice" "bob" "charlie" "admin")
    local passwords=("senha123" "senha456" "senha789" "admin123")
    
    for i in $(seq 1 $NUM_CLIENTS); do
        local user_idx=$((i % 4))
        local user="${users[$user_idx]}"
        local pass="${passwords[$user_idx]}"
        
        (
            echo "$user"
            echo "$pass"
            sleep $((RANDOM % 3 + 1))
            echo "Mensagem do cliente $i"
            sleep 2
            echo "/quit"
        ) | timeout 15 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test3_client_$i.log" 2>&1 &
        
        pids+=($!)
        sleep 0.2
    done
    
    log_info "Aguardando clientes terminarem..."
    local failed=0
    for pid in "${pids[@]}"; do
        wait $pid || ((failed++))
    done
    
    if [ $failed -eq 0 ]; then
        log_success "Teste 3: PASSOU (todos os clientes conectaram)"
        return 0
    else
        log_warn "Teste 3: $failed clientes falharam"
        return 1
    fi
}

# Teste 4: Broadcast de mensagens
test_broadcast() {
    log_info "=== Teste 4: Broadcast de Mensagens ==="
    
    # Cliente 1 (alice) envia mensagem
    (
        echo "alice"
        echo "senha123"
        sleep 2
        echo "Mensagem broadcast de alice"
        sleep 3
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test4_alice.log" 2>&1 &
    local pid1=$!
    
    sleep 1
    
    # Cliente 2 (bob) recebe mensagem
    (
        echo "bob"
        echo "senha456"
        sleep 4
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test4_bob.log" 2>&1 &
    local pid2=$!
    
    wait $pid1
    wait $pid2
    
    if grep -q "alice.*Mensagem broadcast" "$LOG_DIR/test4_bob.log"; then
        log_success "Teste 4: PASSOU (broadcast funcionou)"
        return 0
    else
        log_error "Teste 4: FALHOU (mensagem não recebida)"
        return 1
    fi
}

# Teste 5: Mensagens privadas
test_private_messages() {
    log_info "=== Teste 5: Mensagens Privadas ==="
    
    # Cliente 1 (alice)
    (
        echo "alice"
        echo "senha123"
        sleep 2
        echo "/msg bob Mensagem privada para bob"
        sleep 3
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test5_alice.log" 2>&1 &
    local pid1=$!
    
    sleep 1
    
    # Cliente 2 (bob)
    (
        echo "bob"
        echo "senha456"
        sleep 4
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test5_bob.log" 2>&1 &
    local pid2=$!
    
    wait $pid1
    wait $pid2
    
    if grep -q "PRIVADO de alice" "$LOG_DIR/test5_bob.log"; then
        log_success "Teste 5: PASSOU (mensagem privada funcionou)"
        return 0
    else
        log_error "Teste 5: FALHOU"
        return 1
    fi
}

# Teste 6: Filtro de palavras
test_word_filter() {
    log_info "=== Teste 6: Filtro de Palavras ==="
    
    (
        echo "alice"
        echo "senha123"
        sleep 2
        echo "Esta mensagem tem banword aqui"
        sleep 2
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test6_client.log" 2>&1 &
    
    wait
    
    if grep -q "bloqueada" "$LOG_DIR/test6_client.log"; then
        log_success "Teste 6: PASSOU (filtro funcionou)"
        return 0
    else
        log_error "Teste 6: FALHOU"
        return 1
    fi
}

# Teste 7: Comandos do sistema
test_system_commands() {
    log_info "=== Teste 7: Comandos do Sistema ==="
    
    (
        echo "alice"
        echo "senha123"
        sleep 2
        echo "/help"
        sleep 1
        echo "/users"
        sleep 1
        echo "/history"
        sleep 1
        echo "/quit"
    ) | timeout 10 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test7_client.log" 2>&1 &
    
    wait
    
    local passed=0
    grep -q "Comandos disponíveis" "$LOG_DIR/test7_client.log" && ((passed++))
    grep -q "Usuários online" "$LOG_DIR/test7_client.log" && ((passed++))
    
    if [ $passed -eq 2 ]; then
        log_success "Teste 7: PASSOU ($passed/2 comandos funcionaram)"
        return 0
    else
        log_warn "Teste 7: PARCIAL ($passed/2 comandos funcionaram)"
        return 1
    fi
}

# Teste 8: Reconexão após desconexão
test_reconnection() {
    log_info "=== Teste 8: Reconexão ==="
    
    # Primeira conexão
    (
        echo "alice"
        echo "senha123"
        sleep 1
        echo "/quit"
    ) | timeout 5 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test8_first.log" 2>&1
    
    sleep 1
    
    # Segunda conexão (mesmo usuário)
    (
        echo "alice"
        echo "senha123"
        sleep 1
        echo "Reconectado com sucesso"
        sleep 1
        echo "/quit"
    ) | timeout 5 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test8_second.log" 2>&1
    
    if grep -q "Bem-vindo" "$LOG_DIR/test8_second.log"; then
        log_success "Teste 8: PASSOU (reconexão funcionou)"
        return 0
    else
        log_error "Teste 8: FALHOU"
        return 1
    fi
}

# Teste 9: Stress test
test_stress() {
    log_info "=== Teste 9: Stress Test (50 clientes) ==="
    
    local num_stress=50
    local pids=()
    
    for i in $(seq 1 $num_stress); do
        local user_idx=$((i % 4))
        local users=("alice" "bob" "charlie" "admin")
        local passwords=("senha123" "senha456" "senha789" "admin123")
        
        (
            echo "${users[$user_idx]}"
            echo "${passwords[$user_idx]}"
            sleep $((RANDOM % 2 + 1))
            for j in $(seq 1 5); do
                echo "Stress msg $i.$j"
                sleep 0.1
            done
            echo "/quit"
        ) | timeout 20 $CLIENT_BIN $HOST $PORT > "$LOG_DIR/test9_client_$i.log" 2>&1 &
        
        pids+=($!)
        sleep 0.05
    done
    
    log_info "Aguardando $num_stress clientes..."
    local failed=0
    for pid in "${pids[@]}"; do
        wait $pid || ((failed++))
    done
    
    local success=$((num_stress - failed))
    local percent=$((success * 100 / num_stress))
    
    if [ $percent -ge 90 ]; then
        log_success "Teste 9: PASSOU ($success/$num_stress clientes = $percent%)"
        return 0
    else
        log_warn "Teste 9: FALHOU ($success/$num_stress clientes = $percent%)"
        return 1
    fi
}

# Teste 10: Verificar logs do servidor
test_server_logs() {
    log_info "=== Teste 10: Verificação de Logs ==="
    
    if [ -f "$LOG_DIR/server.log" ]; then
        local errors=$(grep -c "ERROR" "$LOG_DIR/server.log" || true)
        local warnings=$(grep -c "WARN" "$LOG_DIR/server.log" || true)
        
        log_info "Erros encontrados: $errors"
        log_info "Avisos encontrados: $warnings"
        
        if [ $errors -eq 0 ]; then
            log_success "Teste 10: PASSOU (sem erros no servidor)"
            return 0
        else
            log_warn "Teste 10: $errors erros encontrados"
            return 1
        fi
    else
        log_error "Teste 10: Log do servidor não encontrado"
        return 1
    fi
}

# Relatório final
generate_report() {
    log_info "=== RELATÓRIO FINAL ==="
    echo ""
    echo "Testes executados: $total_tests"
    echo "Testes passados: $passed_tests"
    echo "Testes falhados: $failed_tests"
    echo "Taxa de sucesso: $(( passed_tests * 100 / total_tests ))%"
    echo ""
    echo "Logs salvos em: $LOG_DIR/"
    echo ""
    
    if [ $failed_tests -eq 0 ]; then
        log_success "TODOS OS TESTES PASSARAM! ✓"
        return 0
    else
        log_warn "ALGUNS TESTES FALHARAM"
        return 1
    fi
}

# Função principal
main() {
    echo "=========================================="
    echo "  Sistema de Chat - Suite de Testes"
    echo "=========================================="
    echo ""
    
    trap cleanup EXIT
    
    cleanup
    check_binaries
    
    if ! start_server; then
        log_error "Não foi possível iniciar o servidor"
        exit 1
    fi
    
    total_tests=0
    passed_tests=0
    failed_tests=0
    
    # Executar testes
    tests=(
        "test_basic_connection"
        "test_invalid_auth"
        "test_multiple_clients"
        "test_broadcast"
        "test_private_messages"
        "test_word_filter"
        "test_system_commands"
        "test_reconnection"
        "test_stress"
        "test_server_logs"
    )
    
    for test in "${tests[@]}"; do
        ((total_tests++))
        echo ""
        if $test; then
            ((passed_tests++))
        else
            ((failed_tests++))
        fi
        sleep 2
    done
    
    echo ""
    echo "=========================================="
    generate_report
    echo "=========================================="
    
    stop_server
}

# Executar
main "$@"