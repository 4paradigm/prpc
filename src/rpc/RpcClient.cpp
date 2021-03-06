#include "RpcClient.h"
#include "RpcService.h"

namespace paradigm4 {
namespace pico {
namespace core {

std::shared_ptr<Dealer> RpcClient::create_dealer() {
    auto ret = std::make_shared<Dealer>(_info.rpc_id, _service);
    ret->initialize_as_client(this);
    _service->ctx()->add_client_dealer(ret.get());
    _n_dealers->fetch_add(1, std::memory_order_acquire);
    return ret;
}

void RpcClient::release_dealer(Dealer* d) {
    _service->ctx()->remove_client_dealer(d);
    _n_dealers->fetch_add(-1, std::memory_order_release);
}

bool RpcClient::get_rpc_service_info(RpcServiceInfo& out)  {
    return _service->ctx()->get_rpc_service_info(_info.rpc_service_name, out);
}


bool RpcClient::get_available_servers(std::vector<int>& servers) {
    return _service->ctx()->get_avaliable_servers(_info.rpc_service_name, servers);
}


RpcClient::~RpcClient() {
    int n_dealers = _n_dealers->load(std::memory_order_acquire);
    SCHECK(n_dealers == 0) << "RpcClient " << _info << " deconstructed, but "
                           << n_dealers << " dealer are not deconstructed";
}

} // namespace core
} // namespace pico
} // namespace paradigm4


