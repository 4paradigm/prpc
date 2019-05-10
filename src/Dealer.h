#ifndef PARADIGM4_PICO_COMMON_DEALER_H
#define PARADIGM4_PICO_COMMON_DEALER_H

#include <memory>

#include "MasterClient.h"
#include "RpcChannel.h"
#include "RpcContext.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

class RpcContext;
class RpcService;
class RpcServer;
class RpcClient;

class Dealer {
public:
    typedef RpcChannel<RpcRequest> req_ch_t;
    typedef RpcChannel<RpcResponse> resp_ch_t;

    Dealer() = default;

    Dealer(int rpc_id, RpcService* ctx);

    Dealer(const std::string& rpc_name, RpcService* service);

    Dealer(const Dealer&) = delete;

    Dealer& operator=(Dealer&& rhs) {
        _service = rhs._service;
        _ctx = rhs._ctx;
        _rpc_id = rhs._rpc_id;
        _g_rank = rhs._g_rank;
        _id = rhs._id;
        _server_req_ch = std::move(rhs._server_req_ch);
        _client_resp_ch = std::move(rhs._client_resp_ch);
        _initialized_server = rhs._initialized_server;
        _initialized_client = rhs._initialized_client;
        rhs._initialized_server = false;
        rhs._initialized_client = false;
        return *this;
    }

    Dealer(Dealer&& rhs) {
        *this = std::move(rhs);
    }

    ~Dealer() {
        finalize();
    }

    void initialize_as_server(RpcServer* server = nullptr);

    void initialize_as_client(RpcClient* client = nullptr);

    void terminate();

    void finalize();

    bool initialized() {
        return (_initialized_client || _initialized_server);
    }

    // void reset();

    void send_request_one_way(RpcRequest&& req) {
        req.head().src_dealer = -1;
        _send_request(std::move(req));
    }

    void send_request(RpcRequest&& req) {
        req.head().src_dealer = _id;
        _send_request(std::move(req));
    }

    RpcResponse sync_rpc_call(RpcRequest&& req) {
        RpcResponse resp;
        send_request(std::move(req));
        SCHECK(recv_response(resp));
        return resp;
    }

    // retry现在是摆设
    void _send_request(RpcRequest&& req);

    void push_response(RpcResponse&& resp) {
        SCHECK(_initialized_client);
        return _client_resp_ch->send(std::move(resp));
    }

    bool recv_response(RpcResponse& resp, int timeout = -1) {
        SCHECK(_initialized_client);
        return _client_resp_ch->recv(resp, timeout, 1);
    }

    /*
     * server method
     * 返回response不会做重连
     * 如果连接断了，resp就扔了
     * 等client超时重发
     */
    void send_response(RpcResponse&& resp);

    void push_request(RpcRequest&& req) {
        SCHECK(_initialized_server);
        return _server_req_ch->send(std::move(req));
    }

    bool recv_request(RpcRequest& req, int timeout = -1) {
        SCHECK(_initialized_server);
        return _server_req_ch->recv(req, timeout, 64);
    }

    int id() {
        return _id;
    }

private:
    int _rpc_id;
    comm_rank_t _g_rank;
    RpcService* _service;
    RpcContext* _ctx;
    RpcServer* _rpc_server = nullptr;
    RpcClient* _rpc_client = nullptr;
    int64_t _id;

    std::shared_ptr<req_ch_t> _server_req_ch;
    std::shared_ptr<resp_ch_t> _client_resp_ch;

    bool _initialized_server = false;
    bool _initialized_client = false;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_DEALER_H
