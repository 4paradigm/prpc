#include "Dealer.h"
#include "RpcService.h"
#include "RpcServer.h"
#include "RpcClient.h"

namespace paradigm4 {
namespace pico {
namespace core {

Dealer::Dealer(int rpc_id, RpcService* service)
    : _rpc_id(rpc_id), _g_rank(service->global_rank()), _service(service),
      _ctx(_service->ctx()), _id(gen_id()) {}

Dealer::Dealer(const std::string& rpc_name, RpcService* service) {
    _rpc_id = service->register_rpc_service(rpc_name);
    _g_rank = service->global_rank();
    _service = service;
    _ctx = service->ctx();
    _id = gen_id();
}

int Dealer::gen_id() {
    static SpinLock _lk;
    static std::unordered_set<int> _used;
    static uint32_t next = 0;
    int ret = -1;
    lock_guard<SpinLock> _(_lk);
    for (;;) {
        ret = next++ & std::numeric_limits<int>::max();
        if (_used.count(ret) == 0) {
            _used.insert(ret);
            break;
        }
    }
    return ret;
}

void Dealer::initialize_as_server(RpcServer* server) {
    SCHECK(!_initialized_server);
    _rpc_server = server;
    _server_req_ch = std::make_shared<req_ch_t>();
    _initialized_server = true;
}

void Dealer::initialize_as_client(RpcClient* client) {
    SCHECK(!_initialized_client);
    _rpc_client = client;
    _client_resp_ch = std::make_shared<resp_ch_t>();
    _initialized_client = true;
}

void Dealer::terminate() {
    if (_server_req_ch) {
        _server_req_ch->terminate();
    }
    if (_client_resp_ch) {
        _client_resp_ch->terminate();
    }
    _terminated = true;
}

bool Dealer::is_terminated() {
    return _terminated;
}

void Dealer::finalize() {
    if (_initialized_server) {
        if (_rpc_server) {
            _rpc_server->release_dealer(this);
        } else {
            _ctx->remove_server_dealer(_rpc_id, -1, this);
        }
        _server_req_ch.reset();
        _initialized_server = false;
    }

    if (_initialized_client) {
        if (_rpc_client) {
            _rpc_client->release_dealer(this);
        } else {
            _ctx->remove_client_dealer(this);
        }
        _client_resp_ch.reset();
        _initialized_client = false;
    }
}

/*
void Dealer::reset() {
    auto initialized_client = _initialized_client;
    auto initialized_server = _initialized_server;
    finalize();
    if (initialized_client) {
        initialize_as_client();
    }
    if (initialized_server) {
        initialize_as_server();
    }
}*/

// retry现在是摆设
void Dealer::_send_request(RpcRequest&& req) {
    SCHECK(_initialized_client);
    comm_rank_t dest_g_rank = 0;
    std::shared_ptr<frontend_t> f = nullptr;
    // 寻找可用的服务
    auto err = SUCC;
    if (req.head().sid != -1) {
        f = _ctx->get_client_frontend_by_sid(_rpc_id, req.head().sid);
        errno = ENOSUCHSERVER;
    } else if (req.head().dest_rank != -1) {
        f = _ctx->get_client_frontend_by_rank(req.head().dest_rank);
        errno = ENOSUCHRANK;
    } else {
        f = _ctx->get_client_frontend_by_rpc_id(_rpc_id, req.head().sid);
        errno = ENOSUCHSERVICE;
    }
    if (!f) {
        SLOG(WARNING) << "no such service. " << req.head();
        RpcResponse resp;
        resp.set_error_code(err);
        _client_resp_ch->send(std::move(resp));
        return;
    }
    req.head().src_rank = _g_rank;
    req.head().dest_rank = dest_g_rank;
    req.head().rpc_id = _rpc_id;
    if (f->info.global_rank == _g_rank) {
        _ctx->_spin_lock.lock_shared();
        _ctx->push_request(std::move(req));
        _ctx->_spin_lock.unlock_shared();
    } else {
        // XXX 没有建立连接，自动连，最好启动另一个线程做这件事儿
        int state = f->state.load(std::memory_order_acquire);
        if (state == FRONTEND_DISCONNECT && !_ctx->connect(f)) { 
            RpcResponse resp;
            resp.set_error_code(ECONNECTION);
            _client_resp_ch->send(std::move(resp));
            return;
        }
        if (!f->socket->send(std::move(req))) {
            SLOG(WARNING) << "sending request to " << dest_g_rank
                          << " failed. ";
            _ctx->epipe(f);
        }
    }
}

/*
 * server method
 * 返回response不会做重连
 * 如果连接断了，resp就扔了
 * 等client超时重发
 */
void Dealer::send_response(RpcResponse&& resp) {
    if (resp.head().dest_dealer == -1) {
        return;
    }
    comm_rank_t dest_g_rank = resp.head().dest_rank;
    if (dest_g_rank == _g_rank) {
        _ctx->_spin_lock.lock_shared();
        _ctx->push_response(std::move(resp));
        _ctx->_spin_lock.unlock_shared();
    } else {
        auto f = _ctx->get_server_frontend_by_rank(resp.head().dest_rank);
        if (!f) {
            SLOG(WARNING) << "send response to a dead node, drop it";
            return;
        }
        if (!f->socket->send(std::move(resp))) {
            SLOG(WARNING) << "sending response to " << dest_g_rank
                          << " failed. ";
            _ctx->epipe(f);
        }
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4
