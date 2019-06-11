#include "RpcService.h"

namespace paradigm4 {
namespace pico {
namespace core {

void RpcService::initialize(MasterClient* master_client,
      const RpcConfig& config,
      const std::string& rpc_service_api) {
    _master_client = master_client;
    size_t io_thread_num = config.io_thread_num;
    _self.global_rank
          = _master_client->generate_id(rpc_service_api + "$gen_rank");
    if (config.protocol == "tcp") {
        _ctx.initialize(false, _self.global_rank, io_thread_num);
#ifdef USE_RDMA
    } else if (config.protocol == "rdma") {
        RdmaContext::singleton().initialize(config.rdma);
        _ctx.initialize(true, _self.global_rank, io_thread_num);
#endif
    } else {
        SLOG(FATAL) << "unsupported protocol " << config.protocol;
    }

    _rpc_service_api = rpc_service_api;
    _bind_ip = config.bind_ip;
    if (_bind_ip == "") {
        SCHECK(fetch_ip(_master_client->endpoint(), &_bind_ip))
              << "fetch ip failed";
    }
    _terminate_fd = eventfd(0, EFD_SEMAPHORE);

    _ctx.add_event(_terminate_fd, false);
    _ctx.bind(_bind_ip);
    _self.endpoint = _ctx.endpoint();
    _proxy_threads.resize(io_thread_num);
    for (size_t i = 0; i < _proxy_threads.size(); ++i) {
        _proxy_threads[i] = std::thread(&RpcService::receiving, this, i);
    }
    _master_client->register_node(_self);
    _watch_master_hdl
          = _master_client->watch_rpc_service_info(_rpc_service_api, [this]() {
                _watcher.notify();
            });
    _terminate.store(false);
    _watch_thread = std::thread(&RpcService::watching, this);
}

void RpcService::finalize() {
    int64_t _ = _ctx._io_thread_num;
    SCHECK(::write(_terminate_fd, &_, sizeof(_)) == sizeof(int64_t));
    for (auto& t : _proxy_threads) {
        t.join();
    }
    ::close(_terminate_fd);
    _master_client->cancle_watch(_watch_master_hdl);
    _terminate.store(true);
    _watcher.notify();
    _watch_thread.join();
    SLOG(INFO) << "rpc service finalize";
}

// MT Safe
std::unique_ptr<RpcServer> RpcService::create_server(
      const std::string& rpc_name,
      int server_id) {
    int rpc_id;
    _ctx.begin_add_server();
    SCHECK(_master_client->register_server(
          _rpc_service_api, rpc_name, _self.global_rank, rpc_id, server_id));
    BLOG(1) << "Registered rpc serivce: " << rpc_name
            << " with rpc id: " << rpc_id << ", server id: " << server_id;
    _ctx.end_add_server(rpc_id, server_id);
    return std::make_unique<RpcServer>(rpc_id, server_id, rpc_name, this);
}

std::unique_ptr<RpcClient> RpcService::create_client(
      const std::string& rpc_name,
      int expected_server_num) {
    _ctx.wait([rpc_name, expected_server_num](RpcContext* ctx) {
        RpcServiceInfo info;
        bool ret = ctx->get_rpc_service_info(rpc_name, info);
        return ret && (int)info.servers.size() >= expected_server_num;
    }); 
    RpcServiceInfo info;
    SCHECK(_ctx.get_rpc_service_info(rpc_name, info));
    //SCHECK(ret && info.servers.size() >= expected_server_num);
    return std::make_unique<RpcClient>(info, this);
}

std::shared_ptr<Dealer> RpcService::create_dealer(const std::string& rpc_name) {
    int rpc_id = register_rpc_service(rpc_name);
    auto ret = std::make_shared<Dealer>(rpc_id, this);
    ret->initialize_as_client();
    ret->initialize_as_server();
    SLOG(INFO) << rpc_name << " " << rpc_id;
    _ctx.add_server_dealer(rpc_id, -1, ret.get());
    _ctx.add_client_dealer(ret.get());
    return ret;
}

std::shared_ptr<Dealer> RpcService::create_dealer() {
    return std::make_shared<Dealer>(-1, this);
}

void RpcService::remove_server(RpcServer* server) {
    bool ret = _master_client->deregister_server(
          _rpc_service_api, server->rpc_name(), server->id());
    _ctx.remove_server(server->rpc_id(), server->id());
    SCHECK(ret) << ret;
}

// MT Safe
int RpcService::register_rpc_service(const std::string& rpc_name) {
    int rpc_id;
    _master_client->register_rpc_service(
          _rpc_service_api, rpc_name, rpc_id);
    return rpc_id;
}

// MT Safe
void RpcService::deregister_rpc_service(const std::string& rpc_name) {
    _master_client->deregister_rpc_service(
          _rpc_service_api, rpc_name);
}

void RpcService::update_ctx() {
    bool ret;
    std::vector<CommInfo> comm_info;
    ret = _master_client->get_comm_info(comm_info);
    if (ret) {
        _ctx.update_comm_info(comm_info);
    } else {
        SLOG(WARNING) << "get comm info failed.";
    }
    std::vector<RpcServiceInfo> rpc_info;
    ret = _master_client->get_rpc_service_info(_rpc_service_api, rpc_info);
    if (ret) {
        _ctx.update_service_info(rpc_info);
    } else {
        SLOG(WARNING) << "get service info failed.";
    }
}

void RpcService::handle_accept_event() {
    _ctx.accept();
}

void RpcService::handle_message_event(int fd) {
    _ctx.handle_message_event(fd);
}

void RpcService::receiving(int tid) {
    std::vector<epoll_event> events;
    bool terminate = false;
    while (!terminate) {
        _ctx.poll_wait(events, tid, -1);
        for (auto& e : events) {
            if (e.data.fd == _ctx._acceptor->fd()) {
                handle_accept_event();
            } else if (e.data.fd == _terminate_fd) {
                terminate = true;
            } else {
                handle_message_event(e.data.fd);
            }
        }
    }
}

void RpcService::watching() {
    _watcher.wait([this](){
        if (_terminate.load()) {
            return true;
        }
        update_ctx();
        return false;
    });
}

} // namespace core
} // namespace pico
} // namespace paradigm4

