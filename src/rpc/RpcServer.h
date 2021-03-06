#ifndef PARADIGM4_PICO_CORE_PRPC_SERVER_H
#define PARADIGM4_PICO_CORE_PRPC_SERVER_H

#include <unordered_set>

#include "Dealer.h"
#include "MasterClient.h"

namespace paradigm4 {
namespace pico {
namespace core {

class RpcContext;
class RpcService;
class Dealer;

class RpcServer {
public:
    int32_t id() const {
        return _id;
    }

    int32_t rpc_id() const {
        return _rpc_id;
    }

    const std::string rpc_name() const {
        return _rpc_name;
    }

    std::shared_ptr<Dealer> create_dealer();

    void terminate();

    // terminate()并join所有dealer后，可以restart()
    void restart();

    RpcServer(int rpc_id,
          int server_id,
          const std::string& rpc_name,
          RpcService* service) {
        _rpc_id = rpc_id;
        _id = server_id;
        _rpc_name = rpc_name;
        _service = service;
    }

    RpcService* rpc_service() {
        return _service;
    }

    ~RpcServer();

private:
    friend Dealer;
    void release_dealer(Dealer*);

private:
    int _id;
    int _rpc_id;
    std::string _rpc_name;
    RpcService* _service;
    SpinLock _lk;
    std::unordered_set<Dealer*> _dealers;
    bool _terminate = false;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PICO_CORE_INCLUDE_RPCSERVICE_H_
