#include "RpcServer.h"
#include "RpcService.h"

namespace paradigm4 {
namespace pico {
namespace core {

class RpcContext;
class Dealer;

std::shared_ptr<Dealer> RpcServer::create_dealer() {
    std::shared_ptr<Dealer> ret = std::make_shared<Dealer>(_rpc_id, _service);
    ret->initialize_as_server(this);
    _service->ctx()->add_server_dealer(_rpc_id, _id, ret.get());
    _n_dealers->fetch_add(1, std::memory_order_acquire);
    return ret;
}

void RpcServer::release_dealer(Dealer* d) {
    _service->ctx()->remove_server_dealer(_rpc_id, _id, d);
    _n_dealers->fetch_add(-1, std::memory_order_release);
}

RpcServer::~RpcServer() {
    int n_dealers = _n_dealers->load(std::memory_order_acquire);
    SCHECK(n_dealers == 0) << "RpcServer {" << _rpc_name << ", " << _id
                           << "} deconstructed, but " << n_dealers
                           << " dealer are not deconstructed";
    _service->remove_server(this);
}

} // namespace core
} // namespace pico
} // namespace paradigm4


