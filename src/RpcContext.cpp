#include "RpcContext.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*
 * 暂时LB的策略是random
 * 一个好一点做法是Dealer来要请求
 */

void FairQueue::add_server(int sid) {
    //std::vector<RpcRequest> vec;
    //SCHECK(_sid2cache.emplace(sid, std::move(vec)).second);
    auto cache_que = std::make_unique<MpscQueue<RpcRequest>>();
    SCHECK(_sid2cache.emplace(sid, std::move(cache_que)).second);
}

void FairQueue::remove_server(int sid) {
    auto cit = _sid2cache.find(sid);
    SCHECK(cit != _sid2cache.end());
    RpcRequest req;
    if (cit->second->pop(req)) {
        SLOG(WARNING)
            << "remove server. Drop cached requests. "
            << " rpc_id is " << req.head().rpc_id
            << " sid is " << req.head().sid;
    }
    _sid2cache.erase(cit);
}

void FairQueue::add_server_dealer(int sid,
      Dealer* dealer) {
    auto it = _sid2dealers.find(sid);
    if (it == _sid2dealers.end()) {
        _sid2dealers.insert({sid, {dealer}});
    } else {
        it->second.push_back(dealer);
    }
    _sids.push_back(sid);
    auto cit = _sid2cache.find(sid);
    if (cit != _sid2cache.end()) {
        RpcRequest req;
        //add server dealer之前会加写锁，所以应该不会少pop
        while (cit->second->pop(req)) {
            dealer->push_request(std::move(req));
        }
    }
}

void FairQueue::remove_server_dealer(int sid,
      Dealer* dealer) {
    auto it = _sid2dealers.find(sid);
    SCHECK(it != _sid2dealers.end());
    auto& dealers = it->second;
    for (size_t i = 0; i < dealers.size(); ++i) {
        if (dealers[i] == dealer) {
            dealers[i] = std::move(dealers.back());
            dealers.pop_back();
        }
    }
    for (size_t i = 0; i < _sids.size(); ++i) {
        if (_sids[i] == sid) {
            _sids[i] = std::move(_sids.back());
            _sids.pop_back();
            break;
        }
    }
    if (it->second.empty()) {
        _sid2dealers.erase(it);
    }
}

bool FairQueue::empty() {
    return _sid2dealers.empty() && _sid2cache.empty();
}

Dealer* FairQueue::next() {
    SCHECK(!_sids.empty()) << "no server.";
    int sid = _sids[_sids_rr_index.fetch_add(1, std::memory_order_relaxed)
                    % _sids.size()];
    if (_sid2dealers.empty()) {
        return nullptr;
    } else {
        return next(sid);
    }
}

Dealer* FairQueue::next(int sid) {
    if (sid == -1) {
        if (_sids.empty()) {
            return nullptr;
        }
        sid = _sids[_sids_rr_index.fetch_add(1, std::memory_order_relaxed)
                    % _sids.size()];
    }
    auto it = _sid2dealers.find(sid);
    if (it == _sid2dealers.end()) {
        return nullptr;
    }
    auto& d = it->second;
    SCHECK(!d.empty()) << "no dealer.";
    size_t index =  _dealer_id_rr_index.load(std::memory_order_relaxed);
    index += 1;
    if (index >= d.size()) {
        index = 0;
    }
    _dealer_id_rr_index.store(index, std::memory_order_relaxed);
    return d[index];
}

bool FairQueue::push_request(int sid, RpcRequest&& req) {
    auto it = _sid2dealers.find(sid);
    if (it != _sid2dealers.end()) {
        SCHECK(!it->second.empty()) << "no dealer.";
        it->second.back()->push_request(std::move(req));
        return true;
    }
    auto cit = _sid2cache.find(sid);
    if (cit != _sid2cache.end()) {
        cit->second->push(std::move(req));
        return true;
    }
    return false;
}

void RpcContext::initialize(bool is_use_rdma,
      comm_rank_t rank,
      int io_thread_num) {
    _self.global_rank = rank;
    _is_use_rdma = is_use_rdma;
    _io_thread_num = io_thread_num;
    for (int i = 0; i < io_thread_num; ++i) {
        _epfds.push_back(epoll_create1(EPOLL_CLOEXEC));
    }
    if (_is_use_rdma) {
#ifdef USE_RDMA
        _acceptor = std::make_unique<RdmaAcceptor>();
#else
        SLOG(FATAL) << "rdma not supported.";
#endif
    } else {
        _acceptor = std::make_unique<TcpAcceptor>();
    }
}

void RpcContext::bind(const std::string& ip, int backlog) {
    lock_guard<RWSpinLock> l(_spin_lock);
    PSCHECK(_acceptor->bind_on_random_port(ip) == 0);
    _self.endpoint = _acceptor->endpoint();
    PSCHECK(_acceptor->listen(backlog) == 0);
    add_event(_acceptor->fd(), _epfds[0], false);
    SLOG(INFO) << "bind success. endpoint is " << _acceptor->endpoint();
}

void RpcContext::begin_add_server() {
    _spin_lock.lock();
    //SLOG(INFO) << "begin add server ";
}

void RpcContext::end_add_server(int rpc_id, int sid) {
    auto it = _server_backend.find(rpc_id);
    if (it == _server_backend.end()) {
        std::tie(it, std::ignore)
              = _server_backend.emplace(rpc_id, std::make_shared<FairQueue>());
    }
    it->second->add_server(sid);
    //SLOG(INFO) << "add server : " << rpc_id << " " << sid;
    _spin_lock.unlock();
}

void RpcContext::remove_server(int rpc_id, int sid) {
    lock_guard<RWSpinLock> l(_spin_lock);
    auto it = _server_backend.find(rpc_id);
    SCHECK(it != _server_backend.end()) << _server_backend.size();
    // XXX 想一想死锁问题，fq里也有个锁
    auto fq = it->second;
    fq->remove_server(sid);
    if (fq->empty()) {
        _server_backend.erase(it);
    }
}

void RpcContext::add_server_dealer(int rpc_id,
      int sid,
      Dealer* dealer) {
    lock_guard<RWSpinLock> l(_spin_lock);
    auto it = _server_backend.find(rpc_id);
    if (it == _server_backend.end()) {
        std::tie(it, std::ignore)
              = _server_backend.emplace(rpc_id, std::make_shared<FairQueue>());
    }
    it->second->add_server_dealer(sid, dealer);
}

void RpcContext::remove_server_dealer(int rpc_id,
      int sid,
      Dealer* dealer) {
    lock_guard<RWSpinLock> l(_spin_lock);
    auto it = _server_backend.find(rpc_id);
    SCHECK(it != _server_backend.end()) << _server_backend.size();
    // XXX 想一想死锁问题，fq里也有个锁
    auto fq = it->second;
    fq->remove_server_dealer(sid, dealer);
    if (fq->empty()) {
        _server_backend.erase(it);
    }
}

/*
 * thread safe
 */
void RpcContext::add_client_dealer(Dealer* dealer) {
    lock_guard<RWSpinLock> l(_spin_lock);
    _client_backend.emplace(dealer->id(), dealer);
}

/*
 * thread safe
 * finalize stub用的
 */
void RpcContext::remove_client_dealer(Dealer* dealer) {
    lock_guard<RWSpinLock> l(_spin_lock);
    _client_backend.erase(dealer->id());
}

void RpcContext::poll_wait(std::vector<epoll_event>& events,
      int tid,
      int timeout) {
    events.resize(_n_events);
    int n = retry_eintr_call(
          ::epoll_wait, _epfds[tid], events.data(), events.size(), timeout);
    PSCHECK(n >= 0);
    events.resize(n);
}

std::shared_ptr<FrontEnd> RpcContext::get_client_frontend_by_rank(
      comm_rank_t rank) {
    //shared_lock_guard<RWSpinLock> l(_spin_lock);
    auto it = _client_sockets.find(rank);
    if (it == _client_sockets.end()) {
        SLOG(WARNING) << "no client frontend of rank " << rank;
        return nullptr;
    } else {
        if (it->second->available()) {
            return it->second;
        } else {
            return nullptr;
        }
    }
}

/*
 * 这个msg只能是request
 */
comm_rank_t RpcContext::send_request(RpcMessage&& msg, bool nonblock) {
    shared_lock_guard<RWSpinLock> l(_spin_lock);
    std::shared_ptr<FrontEnd> f = nullptr;
    auto sid = msg.head()->sid;
    auto dest_rank = msg.head()->dest_rank;
    auto rpc_id = msg.head()->rpc_id;
    if (dest_rank != -1) {
        f = get_client_frontend_by_rank(dest_rank);
    } else if (sid != -1) {
        f = get_client_frontend_by_sid(rpc_id, sid);
    } else {
        f = get_client_frontend_by_rpc_id(rpc_id);
    }
    if (!f) {
        RpcResponse resp(*msg.head());
        resp.set_error_code(RpcErrorCodeType::ENOSUCHSERVER);
        //shared_lock();
        push_response(std::move(resp));
        return -1;
    }
    comm_rank_t ret = f->info().global_rank;
    SLOG(INFO) << "ret : " << ret;

    if (f->info() == _self) {
        push_request(std::move(msg));
        return ret;
    }
    if (nonblock) {
        if ((f->state() & FRONTEND_DISCONNECT) == FRONTEND_DISCONNECT) {
            std::thread background = std::thread(
                  [this, f](RpcMessage&& msg) {
                      if (f->connect()) {
                          {
                              lock_guard<RWSpinLock> l(_spin_lock);
                              add_frontend_event(f.get());
                          }
                          f->send_msg(std::move(msg));
                      } else {
                          send_request(std::move(msg), false);
                      }
                  },
                  std::move(msg));
            background.detach();
        } else {
            f->send_msg_nonblock(std::move(msg));
        }
    } else {
        if ((f->state() & FRONTEND_DISCONNECT) == FRONTEND_DISCONNECT) {
            if (f->connect()) {
                {
                    lock_guard<RWSpinLock> l(_spin_lock);
                    add_frontend_event(f.get());
                }
                f->send_msg(std::move(msg));
            } else {
                send_request(std::move(msg), false);
            }
        }
        f->send_msg(std::move(msg));
    }
    return ret;
}

void RpcContext::send_response(RpcMessage&& resp, bool nonblcok) {
    std::shared_ptr<FrontEnd> f = nullptr;
    auto dest_rank = resp.head()->dest_rank;
    f = get_server_frontend_by_rank(dest_rank);
    if (!f) {
        SLOG(WARNING) << "no server frontend";
        return;
    }
    if (f->state() & FRONTEND_DISCONNECT) {
        SLOG(WARNING) << "no server frontend";
        return;
    }

    if (nonblcok) {
        f->send_msg_nonblock(std::move(resp));
    } else {
        f->send_msg(std::move(resp));
    }
}

/*
 * 瞎写的，目前是随机选择一个
 * 如果没有，那么遍历所有的，找到
 * 一个可用的，未来维护一个专门的数据结构，
 * 保存可用的
 */
std::shared_ptr<FrontEnd> RpcContext::get_client_frontend_by_rpc_id(
      int rpc_id) {
    //shared_lock_guard<RWSpinLock> l(_spin_lock);
    auto it1 = _rpc_server_frontend.find(rpc_id);
    if (it1 == _rpc_server_frontend.end()) {
        return nullptr;
    }
    const auto& v = it1->second;
    auto ret = v[rand() % v.size()];
    if (ret->available()) {
        return ret;
    }
    for (const auto& i : v) {
        if (i->available()) {
            return i;
        }
    }
    return nullptr;
}

std::shared_ptr<FrontEnd> RpcContext::get_client_frontend_by_sid(int rpc_id,
      int server_id) {
    auto it1 = _rpc_server_info.find(rpc_id);
    if (it1 == _rpc_server_info.end()) {
        SLOG(WARNING) << "no rpc service " << rpc_id;
        return nullptr;
    }
    auto it2 = it1->second.find(server_id);
    if (it2 == it1->second.end()) {
        SLOG(WARNING) << "no rpc service server " << rpc_id << " " << server_id;
        return nullptr;
    }
    return get_client_frontend_by_rank(it2->second->global_rank);
}

std::shared_ptr<FrontEnd> RpcContext::get_server_frontend_by_rank(
      comm_rank_t rank) {
    auto it = _server_sockets.find(rank);
    if (it == _server_sockets.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

void RpcContext::handle_message_event(int fd) {
    auto func = [this](RpcMessage&& msg) {
        if (msg.head()->dest_dealer == -1) {
            push_request(RpcRequest(std::move(msg)));
        } else {
            push_response(RpcResponse(std::move(msg)));
        }
    };
    shared_lock_guard<RWSpinLock> l(_spin_lock);
    auto it = _fd_map.find(fd);
    if (it == _fd_map.end()) {
        SLOG(WARNING) << "fuck!!!!";
        return;
    }
    auto f = it->second;
    {
        bool ret = f->handle_event(fd, func);
        if (!ret) {
            _spin_lock.unlock_shared();
            _spin_lock.lock();
            // TODO 仔细想想要不要只删除server end
            // client端的epipe只删除事件
            remove_frontend(f);
            _spin_lock.downgrade();
        }
    }
}

std::vector<CommInfo> RpcContext::get_comm_info() {
    shared_lock_guard<RWSpinLock> l(_spin_lock);
    std::vector<CommInfo> ret;
    ret.reserve(_server_sockets.size());
    for (auto& i : _server_sockets) {
        ret.push_back(i.second->info());
    }
    return ret;
}

void RpcContext::update_comm_info(const std::vector<CommInfo>& list) {
    std::set<CommInfo> set(list.begin(), list.end());
    std::vector<std::shared_ptr<FrontEnd>> to_del;
    std::vector<CommInfo> to_add;
    lock_guard<RWSpinLock> l(_spin_lock);
    for (auto& i : _server_sockets) {
        auto& f = i.second;
        if (set.count(f->info()) == 0) {
            to_del.push_back(f);
        }
    }
    for (auto ptr : to_del) {
        remove_frontend(ptr.get());
    }
    for (const auto& comm_info : list) {
        if (_server_sockets.count(comm_info.global_rank) == 0) {
            to_add.push_back(comm_info);
        }
    }
    for (const auto& comm_info : to_add) {
        auto f = std::make_shared<FrontEnd>();
        f->_ctx = this;
        f->_info = comm_info;
        f->is_client_socket() = true;
        f->_is_use_rdma = _is_use_rdma;
        _client_sockets.emplace(comm_info.global_rank, f);
    }
}

/*
 * 暴力到自己不想看
 */
void RpcContext::update_service_info(const std::vector<RpcServiceInfo>& list) {
    {
        lock_guard<std::mutex> lk(_rpc_mu);
        lock_guard<RWSpinLock> l(_spin_lock);
        _rpc_info.clear();
        _rpc_server_info.clear();
        _rpc_server_frontend.clear();
        auto service_list = list;
        for (auto& service_info: service_list) {
            int num = 0;
            for (auto& server_info: service_info.servers) {
                if (_client_sockets.count(server_info.global_rank)) {
                    service_info.servers[num++] = server_info;
                }
            }
            service_info.servers.resize(num);
        }
        for (const auto& info : service_list) {
            _rpc_info.emplace(info.rpc_service_name, info);
        }
        for (const auto& pr : _rpc_info) {
            const auto& rpc_info = pr.second;
            for (const auto& server_info : rpc_info.servers) {
                _rpc_server_info[rpc_info.rpc_id][server_info.server_id]
                    = (ServerInfo*)&server_info;
                _rpc_server_frontend[rpc_info.rpc_id].push_back(
                      _client_sockets[server_info.global_rank]);
            }
        }
        /* unique all frontend，但是觉得没必要
        for (auto& pr : _rpc_server_frontend) {
            auto& v = pr.second;
            std::sort(v.begin(), v.end());
            auto last = std::unique(v.begin(), v.end());
            v.erase(last, v.end());
        }*/
    }
    _rpc_waiter.notify_all(); // Allow send/receive threads to run.
}

void RpcContext::wait(const std::function<bool(RpcContext*)>& func) {
    std::unique_lock<std::mutex> lk(_rpc_mu); // RAII, no need for unlock.
    auto not_paused = [this, func]() { return func(this); };
    _rpc_waiter.wait(lk, not_paused);
}

void RpcContext::accept() {
    BinaryArchive ar;
    std::string info;
    auto f = std::make_shared<FrontEnd>();
    f->_ctx = this;
    f->_socket = _acceptor->accept();
    f->set_state(FRONTEND_CONNECT);
    if (!f->_socket || !f->_socket->accept(info)) {
        return;
    }
    ar.set_read_buffer((char*)info.data(), info.size());
    CommInfo comm_info;
    uint16_t magic = -1;
    ar >> magic >> f->_info;
    SCHECK(magic == 0);
    ar.release();
    SLOG(INFO) << "accept from " << f->info();
    lock_guard<RWSpinLock> l(_spin_lock);
    f->_is_client_socket = false;
    _server_sockets.emplace(f->info().global_rank, f);
    add_frontend_event(f.get());
}

bool RpcContext::get_rpc_service_info(const std::string rpc_name,
      RpcServiceInfo& info) {
    shared_lock_guard<RWSpinLock> _(_spin_lock);
    auto it = _rpc_info.find(rpc_name);
    if (it == _rpc_info.end()) {
        return false;
    }
    info = it->second;
    return true;
}

/*
 * only for proxy
 * 假设外部已经抢到读锁
 */
void RpcContext::push_request(RpcRequest&& req) {
    int rpc_id = req.head().rpc_id;
    auto it = _server_backend.find(rpc_id);
    // XXX 如果没有找到server，那么先扔掉，后面想办法回复一个默认的resp
    if (it == _server_backend.end()) {
        SLOG(WARNING)
              << "recv request, but no such service. Drop it. "
              << " rpc_id is " << req.head().rpc_id
              << " sid is " << req.head().sid;
        return;
    }
    auto fq = it->second;
    auto dealer = fq->next(req.head().sid);
    if (!dealer) {
        if (!fq->push_request(req.head().sid, std::move(req))) {
            SLOG(WARNING)
                << "recv request, but no such server. Drop it. "
                << " rpc_id is " << req.head().rpc_id
                << " sid is " << req.head().sid;
        }
    } else {
        dealer->push_request(std::move(req));
    }
    
}

/*
 * 假设外部已经抢到读锁
 */
void RpcContext::push_response(RpcResponse&& resp) {
    auto it = _client_backend.find(resp.head().dest_dealer);
    if (it != _client_backend.end()) {
        auto dealer = it->second;
        if (dealer) {
            dealer->push_response(std::move(resp));
            return;
        }
    }
    SLOG(WARNING) << "recv resp, but dealer has been finalized. Drop "
        "it. rpc_id is "
        << resp.head().rpc_id;
}

void RpcContext::add_frontend_event(FrontEnd* f) {
    SCHECK(f->state() == FRONTEND_CONNECT) << f->state();
    if (!f->_socket->fds().empty()) {
        static int idx = 0;
        f->_epfd = _epfds[idx % _io_thread_num];
        ++idx;
        for (int fd : f->_socket->fds()) {
            add_event(fd, f->_epfd, true);
            _fd_map.emplace(fd, f);
        }
    }
}

void RpcContext::remove_frontend_event(FrontEnd* f) {
    for (auto& fd : f->_socket->fds()) {
        del_event(fd, f->_epfd);
    }
}

void RpcContext::remove_frontend(FrontEnd* f) {
    remove_frontend_event(f);
    for (auto& fd : f->_socket->fds()) {
        _fd_map.erase(fd);
    }
    comm_rank_t rank = f->_info.global_rank;
    if (f->_is_client_socket) {
        _client_sockets.erase(rank);
    } else {
        _server_sockets.erase(rank);
    }
}

void RpcContext::add_event(int fd, int epfd, bool edge_trigger) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (edge_trigger) {
        // event.events |= EPOLLET;
    }
    //PSCHECK(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == 0) << fd << " " << errno;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    if (ret != 0) {
        PSLOG(WARNING) << "epoll ctl error";
    }
    ++_n_events;
}

void RpcContext::del_event(int fd, int epfd) {
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == 0) {
        --_n_events;
    } else if (errno == ENOENT) {
        PSLOG(WARNING) << "no such event";
    } else {
        PSLOG(FATAL) << errno;
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4

