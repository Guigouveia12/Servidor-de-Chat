cmake_minimum_required(VERSION 3.10)
project(chat_etapa2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(tslog STATIC src/tslog.cpp)

add_executable(test_tslog tests/test_tslog_cli.cpp)
target_link_libraries(test_tslog PRIVATE tslog)

# novos executáveis (servidor e cliente)
add_executable(chat_server src/server_main.cpp)
target_link_libraries(chat_server PRIVATE tslog)

add_executable(chat_client src/client_main.cpp)
target_link_libraries(chat_client PRIVATE tslog)

# install targets (opcional)
install(TARGETS tslog chat_server chat_client ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
