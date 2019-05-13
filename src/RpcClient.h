#ifndef PARADIGM4_PICO_PRPC_CLIENT_H
#define PARADIGM4_PICO_PRPC_CLIENT_H

#include <gflags/gflags.h>

#include "Dealer.h"
#include "Master.h"
#include "MasterClient.h"

namespace paradigm4 {
namespace pico {
namespace core {

class RpcContext;
class Dealer;

class RpcClient {
public:

    std::shared_ptr<Dealer> create_dealer();
    void release_dealer(Dealer*);

    RpcClient(const RpcServiceInfo& info, RpcService* service)
        : _n_dealers(std::make_unique<std::atomic<int>>(0)) {
        _info = info;
        _service = service;
    }

    ~RpcClient();

private:
    RpcServiceInfo _info;
    RpcService* _service;
    std::unique_ptr<std::atomic<int>> _n_dealers;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PICO_CORE_INCLUDE_RPCSERVICE_H_
