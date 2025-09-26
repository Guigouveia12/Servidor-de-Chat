#include <iostream>
#include <thread>
#include <vector>
#include "../include/tslog.hpp"


using namespace tslog;


void worker_fn(int idx, int messages) {
for (int i = 0; i < messages; ++i) {
Logger::instance().info("worker " + std::to_string(idx) + " message " + std::to_string(i));
if (i % 10 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(5));
}
}


int main(int argc, char** argv) {
const int nthreads = (argc > 1) ? std::stoi(argv[1]) : 8;
const int msgs = (argc > 2) ? std::stoi(argv[2]) : 200;


Logger::instance().init("stdout", Level::DEBUG);


std::vector<std::thread> workers;
for (int i = 0; i < nthreads; ++i) {
workers.emplace_back(worker_fn, i, msgs);
}


for (auto &t : workers) t.join();

std::this_thread::sleep_for(std::chrono::milliseconds(200));
Logger::instance().shutdown();


std::cout << "Test concluído. Veja as saídas de log acima." << std::endl;
return 0;
}
