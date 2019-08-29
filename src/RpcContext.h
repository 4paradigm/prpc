#ifndef PARADIGM4_PICO_CORE_RPCCONTEXT_H
#define PARADIGM4_PICO_CORE_RPCCONTEXT_H

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

#include "Master.h"
#include "RpcChannel.h"
#include "SpinLock.h"
#include "TcpSocket.h"
#include "common.h"
#include "FrontEnd.h"
#ifdef USE_RDMA
#include "RdmaSocket.h"
#endif
#include "MasterClient.h"
#include "RpcServer.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

struct RpcConfig {
    RpcConfig() = default;

    template <typename T>
    RpcConfig(const T& o) {
        bind_ip = o.bind_ip;
        io_thread_num = o.io_thread_num;
        protocol = o.protocol;
#ifdef USE_RDMA
        rdma = o.rdma;
#endif
        tcp = o.tcp;
    }

    std::string bind_ip = "127.0.0.1";
    size_t io_thread_num = 1;
    std::string protocol = "tcp";
#ifdef USE_RDMA
    RdmaConfig rdma;
#endif
    TcpConfig tcp;
};

class Dealer;
class RpcServer;

/*
 * 暂时LB的策略是random
 * 一个好一点做法是Dealer来要请求
 */
class FairQueue : public NoncopyableObject {
public:
    FairQueue() {}
    void add_server(int sid);
    void remove_server(int sid);
    void add_server_dealer(int sid, Dealer* dealer);
    void remove_server_dealer(int sid, Dealer* dealer);
    bool empty();
    Dealer* next();
    Dealer* next(int sid);

    bool push_request(int sid, RpcRequest&& req);
private:

    // 与server和stub共享dealer的所有权
    std::unordered_map<int, std::vector<Dealer*>> _sid2dealers;
    std::vector<int> _sids;
    std::atomic<int> _sids_rr_index;
    std::atomic<size_t> _dealer_id_rr_index;
    std::unordered_map<int, std::unique_ptr<MpscQueue<RpcRequest>>> _sid2cache;

    // 只有一个server时加速查表
    std::vector<Dealer*> quick_dealer;
};

class RpcContext {
public:
    friend class Dealer;
    friend class RpcService;
    friend class FrontEnd;

    typedef RpcChannel<RpcRequest> req_ch_t;
    typedef RpcChannel<RpcResponse> resp_ch_t;
    RpcContext() {}

    void initialize(bool is_use_rdma, comm_rank_t rank, int io_thread_num = 1);

    void finalize();

    void async(std::function<void()>);

    void bind(const std::string& ip, int backlog = 20);

    ~RpcContext() {
        for (auto epfd : _epfds) {
            ::close(epfd);
        }
    }

    const std::string& endpoint() {
        shared_lock_guard<RWSpinLock> l(_spin_lock);
        return _acceptor->endpoint();
    }

    void begin_add_server();

    void end_add_server(int rpc_id, int sid);

    void remove_server(int rpc_id, int sid);

    void add_server_dealer(int rpc_id, int sid, Dealer* dealer);

    void remove_server_dealer(int rpc_id, int sid, Dealer* dealer);
    /*
     * thread safe
     */
    void add_client_dealer(Dealer* dealer);

    /*
     * thread safe
     * finalize stub用的
     */
    void remove_client_dealer(Dealer* dealers);

    void poll_wait(std::vector<epoll_event>& events, int tid, int timeout);

    std::shared_ptr<FrontEnd>* get_client_frontend_by_rank(comm_rank_t rank);
    /*
     * 瞎写的，每次返回第一个，LB需要好好写
     */
    std::shared_ptr<FrontEnd>* get_client_frontend_by_rpc_id(int rpc_id);

    std::shared_ptr<FrontEnd>* get_client_frontend_by_sid(int rpc_id, int server_id);

    std::shared_ptr<FrontEnd>* get_server_frontend_by_rank(comm_rank_t rank);

    void handle_message_event(int fd);

    std::vector<CommInfo> get_comm_info();
    
    void update_comm_info(const std::vector<CommInfo>& list, MasterClient* master);
       
    void update_service_info(const std::vector<RpcServiceInfo>& list);

    void wait(const std::function<bool(RpcContext*)>&);

    void accept();

    shared_lock_guard<RWSpinLock> shared_lock() {
        return shared_lock_guard<RWSpinLock>(_spin_lock);
    }

    lock_guard<RWSpinLock> lock() {
        return lock_guard<RWSpinLock>(_spin_lock);
    }

    CommInfo self() {
        return _self;
    }

    bool get_rpc_service_info(const std::string rpc_name, RpcServiceInfo& out);

    // 返回选用了哪一个frontend
    comm_rank_t send_request(RpcMessage&& req);

    void send_response(RpcMessage&& resp, bool nonblock);

    /*
     * only for proxy
     * 假设外部已经抢到读锁
     */
    void push_request(RpcRequest&& req);

     /* 
      * 假设外部已经抢到读锁
      */
    void push_response(RpcResponse&& resp);
       
    void add_frontend_event(FrontEnd* f);
       
    void remove_frontend_event(FrontEnd* f);

private:
    void remove_frontend(FrontEnd* f);
       
    void add_event(int fd, int epfd, bool edge_trigger);
       
    void add_event(int fd, bool edge_trigger) {
        for (int epfd : _epfds) {
            add_event(fd, epfd, edge_trigger);
        }
    }


    void del_event(int fd, int epfd);


    RWSpinLock _spin_lock;
    bool _is_use_rdma = true;

    // backend 相关
    std::unordered_map<int, std::shared_ptr<FairQueue>> _server_backend;
    std::unordered_map<int, Dealer*> _client_backend;

    /*
     * frontend相关
     */
    std::set<comm_rank_t> _to_del_client_sockets;
    std::unordered_map<comm_rank_t, std::shared_ptr<FrontEnd>> _client_sockets; // rank->socket
    std::unordered_map<comm_rank_t, std::shared_ptr<FrontEnd>> _server_sockets; // rank->socket
    std::unordered_map<int, FrontEnd*> _fd_map;
    std::unique_ptr<RpcAcceptor> _acceptor;

    /*
     * rpc_service相关
     * 两个数据结构联动，一致
     */
    std::unordered_map<std::string, RpcServiceInfo> _rpc_info;
    std::unordered_map<int, std::unordered_map<int, ServerInfo*>> _rpc_server_info;
    std::unordered_map<int, std::vector<std::shared_ptr<FrontEnd>>>
          _rpc_server_frontend;
    
    /*
     * 加速 get_client_frontend_by_sid
     */
    std::unordered_map<uint64_t, std::shared_ptr<FrontEnd>> _rpc_server_id_frontend;

    /*
     * 用于等待rpc info满足要求的
     */
    std::condition_variable _rpc_waiter;
    std::mutex _rpc_mu;

    std::vector<int> _epfds;
    std::atomic<int> _n_events = {0};

    CommInfo _self;
    int _io_thread_num;

    std::atomic<size_t> _async_thread_num = {0};
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_RPCCONTEXT_H
