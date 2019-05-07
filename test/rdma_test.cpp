#include "RdmaSocket.h"
#include "TcpSocket.h"
#include <thread>

using namespace paradigm4::pico;

int main() {
    auto client_func = []() {
        auto s = std::make_unique<RdmaSocket>();
        LOG(INFO) << "before connect : " << s->fd();
        s->connect("192.168.0.2:12345");
        LOG(INFO) << "after connect : " << s->fd();
    };

    auto server_func = []() {
        auto s = std::make_unique<RdmaAcceptor>();
        s->bind("192.168.0.2:12345");
        s->listen(10);
        s->accept();
        LOG(INFO) << "after connect : " << s->fd();
    };

    std::thread server = std::thread(server_func);
    std::thread client = std::thread(client_func);

    client.join();
    server.join();


}
