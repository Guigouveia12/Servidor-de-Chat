cmake_minimum_required(VERSION 3.10)
project(chat_multiusuario LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# Diretórios
include_directories(${CMAKE_SOURCE_DIR}/include)

# Biblioteca tslog
add_library(tslog STATIC src/tslog.cpp)

# Executável do servidor
add_executable(chat_server src/server_main.cpp)
target_link_libraries(chat_server PRIVATE tslog pthread)

# Executável do cliente
add_executable(chat_client src/client_main.cpp)
target_link_libraries(chat_client PRIVATE tslog pthread)

# Teste da biblioteca tslog
add_executable(test_tslog tests/test_tslog_cli.cpp)
target_link_libraries(test_tslog PRIVATE tslog pthread)

# Instalação
install(TARGETS tslog chat_server chat_client test_tslog
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY include/ DESTINATION include)

# Documentação
add_custom_target(docs
    COMMAND echo "Documentação disponível em docs/"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})