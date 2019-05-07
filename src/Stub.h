#ifndef PARADIGM4_PICO_PRPC_STUB_H
#define PARADIGM4_PICO_PRPC_STUB_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>


#include <gflags/gflags.h>

#include "fetch_ip.h"

#include "RpcContext.h"
#include "common/include/MasterClient.h"
#include "common/include/ThreadedTimer.h"

namespace paradigm4 {
namespace pico {

/*
 * client端，构造需要指定使用哪个rpc_serivice
 * 会缓存dns信息，并定期更新
 *
 * error code包含
 * EAGAIN 应该不会出现
 * EMASTERTIMEDOUT 连接master超时
 * ERPCTIMEDOUT RPC消息超时
 * ENOSERVICE 找不到service
 */
class Stub : public NoncopyableObject {
public:
    typedef std::function<void(RpcResponse&& resp)> CallBackFunc;

    /*
     * 同步的rpc call
     * error code在返回值里
     * 重试次数和timeout是req的属性
     */
    void sync_rpc_call(RpcRequest&& req, RpcResponse& resp) {
        _dealer.send_request(std::move(req));
        _dealer.recv_response(resp);
        if (resp.error_code()) {
            dealer.reset();
        }
    }

    void send_request(RpcRequest&& req) {
        _dealer.send_request(std::move(req));
    }

    void recv_resp(RpcResponse& resp, int timeout) {
        bool ret = _dealer.recv_response(resp, timeout);
        if (!ret || (ret && resp.error_code())) {
            _dealer.reset();
        }
    }

    void send_request_one_way(RpcRequest&& req) {
        req.src_dealer = -1;
        _dealer.send_request(std::move(req), retry_times);
    }

    /*
     * 用户需要从RpcService中构造Stub
     */
    Stub(const std::string& rpc_service_name, RpcService* service) {
        _rpc_id = service->get_rpc_id(rpc_service_name);
        _rank = service->comm_info().global_rank;
        _dealer = Dealer(_rpc_id, _rank, service->ctx());
    }

private:
    /*
     * 这个数据结构要考虑线程安全
     * cb和用户线程公用
     */
    RWSpinLock _lk;
    std::unordered_map<int64_t, CallBackFunc> _cb;
    std::vector<std::pair<CallBackFunc, CallBackFunc>> _cb;
    // XXX 需要async写法再用，回调线程，未来可以改成一个thread group
    //std::thread _async_cb_processer;
};

} // namespace pico
} // namespace paradigm4

#endif // PICO_COMMON_INCLUDE_RPCSERVICE_H_
