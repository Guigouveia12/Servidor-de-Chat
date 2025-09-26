cmake_minimum_required(VERSION 3.10)
project(etapa1_tslog LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories(${CMAKE_SOURCE_DIR}/include)


add_library(tslog STATIC src/tslog.cpp)


add_executable(test_tslog tests/test_tslog_cli.cpp)
target_link_libraries(test_tslog PRIVATE tslog)


# install targets (opcional)
install(TARGETS tslog ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)