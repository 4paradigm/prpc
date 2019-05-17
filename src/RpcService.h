#ifndef PARADIGM4_PICO_CORE_RPCSERVICE_H
#define PARADIGM4_PICO_CORE_RPCSERVICE_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>


#include <gflags/gflags.h>

#include "fetch_ip.h"

#include "Dealer.h"
#include "MasterClient.h"
#include "RpcContext.h"
#include "RpcServer.h"
#include "RpcClient.h"

namespace paradigm4 {
namespace pico {
namespace core {

class RpcService : public NoncopyableObject {
public:
    RpcService() : _ctx() {}

    void initialize(MasterClient* master_client,
          const RpcConfig& config,
          const std::string& rpc_service_api = "RpcService");

    void finalize();

    comm_rank_t global_rank() {
        return _self.global_rank;
    }

    CommInfo& comm_info() {
        return _self;
    }

    RpcContext* ctx() {
        return &_ctx;
    }

    // MT Safe
    std::unique_ptr<RpcServer> create_server(const std::string& rpc_name,
          int server_id = -1);

    std::unique_ptr<RpcClient> create_client(const std::string& rpc_name,
          int expected_server_num = 0);

    std::shared_ptr<Dealer> create_dealer(const std::string& rpc_name);

    void remove_dealer(Dealer*);

    void remove_server(RpcServer* server);

    int register_rpc_service(const std::string& rpc_name);

    void deregister_rpc_service(const std::string& rpc_name);

    void update_ctx();
      
    uint16_t magic() {
        return 0;
    }

    std::string get_bind_ip() {
        return _bind_ip;
    }

private:

    void handle_accept_event();

    void handle_message_event(int fd);

    void receiving(int tid);

    std::vector<std::thread> _proxy_threads;
    int _terminate_fd;

    RpcContext _ctx;
    std::string _rpc_service_api;
    std::string _bind_ip;
    CommInfo _self;

    /*
     * 用于跟中心节点(zk/etcd...)
     * 维持全局ctx
     */
    MasterClient* _master_client;
    WatcherHandle _watch_master_hdl;
};


} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PICO_CORE_INCLUDE_RPCSERVICE_H_
