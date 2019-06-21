#include "Master.h"
#include <gflags/gflags.h>
#include <poll.h>

namespace paradigm4 {
namespace pico {
namespace core {

constexpr int DMASTER = 2;

void CommInfo::to_json_node(PicoJsonNode& node)const {
    node.add("global_rank", global_rank);
    node.add("endpoint", endpoint);
}

std::string CommInfo::to_json_str()const {
    PicoJsonNode node;
    to_json_node(node);
    std::string s;
    node.save(s);
    return s;
}

void CommInfo::from_json_node(const PicoJsonNode& node) {
    node.at("global_rank").try_as<comm_rank_t>(global_rank);
    node.at("endpoint").try_as<std::string>(endpoint);
}

void CommInfo::from_json_str(const std::string &str) {
    PicoJsonNode node;
    node.load(str);
    from_json_node(node);
}

std::ostream& operator<<(std::ostream& stream, const CommInfo& comm_info) {
    stream << "node[ep: " << comm_info.endpoint
        << ", g_rank: " << comm_info.global_rank;
    return stream;
}

void RpcServiceInfo::to_json_node(PicoJsonNode& node) const {
    node.add("rpc_service_name", rpc_service_name);
    node.add("rpc_id", rpc_id);
    PicoJsonNode arr;
    for (const auto& info : servers) {
        PicoJsonNode n = {{"server_id", info.server_id},
              {"global_rank", info.global_rank}};
        arr.push_back(n);
    }
    node.add("servers", arr);
}

std::string RpcServiceInfo::to_json_str()const {
    PicoJsonNode node;
    to_json_node(node);
    std::string s;
    node.save(s);
    return s;
}

void RpcServiceInfo::from_json_node(const PicoJsonNode& node) {
    node.at("rpc_service_name").try_as<std::string>(rpc_service_name);
    node.at("rpc_id").try_as<int>(rpc_id);
    servers.clear();
    auto jarr = node.at("servers");
    for(auto it = jarr.begin(); it != jarr.end(); it++) {
        ServerInfo s;
        it.value().at("server_id").try_as<int>(s.server_id);
        it.value().at("global_rank").try_as<comm_rank_t>(s.global_rank);
        servers.push_back(s);
    }
}

void RpcServiceInfo::from_json_str(const std::string &str) {
    PicoJsonNode node;
    node.load(str);
    from_json_node(node);
}

std::ostream& operator<<(std::ostream& stream, const RpcServiceInfo& info) {
    stream << info.to_json_str();
    return stream;
}

bool master_check_valid_path(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (path[0] != '/') {
        return false;
    }
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '/') {
            if (i + 1 == path.size() || path[i + 1] == '/') {
                return false;
            }
        }
    }
    return true;
}

void Master::initialize() {
    SLOG(INFO) << "Master initializing...";
    PSCHECK(_tcp_acceptor = std::unique_ptr<TcpAcceptor>(new TcpAcceptor())) << "fail to create tcp acceptor";
    if(_bind_ip.find(':') == std::string::npos) {
        SCHECK(fetch_ip(_bind_ip, &_bind_ip)) << "fetch ip failed";
        if (_tcp_acceptor->bind_on_random_port(_bind_ip) != 0) {
            PSLOG(FATAL) << "tcp_acceptor bind failed! " << _bind_ip;
        }
    } else {
        if (!_tcp_acceptor->bind(_bind_ip)) {
            PSLOG(FATAL) << "tcp_acceptor bind failed! " << _bind_ip;
        }
    }

    _ep = _tcp_acceptor->endpoint();

    int backlog = 20; // this means maximum connection number Acceptor can hold
    _tcp_acceptor->listen(backlog);
    
    SLOG(INFO) << "Master serving thread bind at endpoint: \"" << _ep << "\"";
    _th = std::thread(&Master::serving, this);
}

void Master::finalize() {
    if (_th.joinable()) {
        _th.join();
    }
    SLOG(INFO) << "Master exited";
}

void Master::exit() {
    TcpSocket temp_tcp_socket;
    temp_tcp_socket.connect(_ep, _ep, 0);
    RpcRequest req(-1);
    req << PicoMasterReqType::MASTER_EXIT;
    temp_tcp_socket.send_rpc_message(std::move(req), false);
}

void Master::serving() {
    bool exit = false;
    while (true) {
        for (int fd: poll()) {
            if (fd == _tcp_acceptor->fd()) {
                // Accept
                std::unique_ptr<RpcSocket> rpc_socket = _tcp_acceptor->accept();
                std::unique_ptr<TcpSocket> tcp_socket = static_unique_pointer_cast<TcpSocket>(
                        std::move(rpc_socket));
                std::string info;
                tcp_socket->accept(info);
                int socket_fd = tcp_socket->in_fd();
                RpcRequest fake(-1);
                fake.head().rpc_id = WATCHER_NOTIFY_RPC_ID;
                _watchers.emplace(tcp_socket.get(), std::move(fake));
                _fd_socket_map.insert(std::make_pair(socket_fd, std::move(tcp_socket)));
            } else {
                // Check in TcpSocket Map
                auto it = _fd_socket_map.find(fd);
                SCHECK(it != _fd_socket_map.end());
                TcpSocket* tcp_socket = it->second.get();
                bool socket_alive = tcp_socket->handle_event(fd, 
                      [this, tcp_socket, &exit](RpcMessage&& msg) {
                    RpcRequest req(std::move(msg));
                    RpcResponse resp(req);
                    handle_op(tcp_socket, req, resp, exit);
                    tcp_socket->send_rpc_message(std::move(resp), false);
                });
                if (!socket_alive) {
                    SLOG(INFO) << "Master erases connection fd " << fd;
                    auto it = _fd_socket_map.find(fd);
                    SCHECK(it != _fd_socket_map.end());
                    disconnect_clear_data(it->second.get());
                    _watchers.erase(it->second.get());
                    _fd_socket_map.erase(it);
                }
            }
        }
        if (exit) {
            if (_fd_socket_map.empty()) {
                break;
            } else {
                SLOG(WARNING) << "received exit request but need wait all client exit.";
            }
        }
    }
}

std::vector<int> Master::poll() {
    std::vector<int> fds;
    std::vector<pollfd> pfds;
    pfds.push_back({_tcp_acceptor->fd(), POLLIN | POLLPRI, 0});
    for (auto& fd: _fd_socket_map) {
        pfds.push_back({fd.first, POLLIN | POLLPRI, 0});
    }
    PSCHECK(retry_eintr_call(::poll, pfds.data(), (nfds_t)pfds.size(), -1) != -1);
    for (pollfd& pfd : pfds) {
        if (pfd.revents != 0) {
            fds.push_back(pfd.fd);
        }
    }
    return fds;
}

void Master::disconnect_clear_data(TcpSocket* tcp_socket) {
    SCHECK(tcp_socket != nullptr);
    std::vector<std::string> temp;
    for (auto& p: _path) {
        if (p.second.owner == tcp_socket) {
            temp.push_back(p.first);
        }
    }
    for (auto& path: temp) {
        size_t p = path.find_last_of('/');
        std::string key = path.substr(p + 1);
        std::string parent = path.substr(0, p);
        SCHECK(_path.erase(path));
        SCHECK(_path[parent].sub.erase(key));
        notify_watchers(path);
    }
}    

void Master::handle_op(TcpSocket* tcp_socket, RpcRequest& req, RpcResponse& resp, bool& exit) {
    PicoMasterReqType op = PicoMasterReqType::MASTER_EXIT;
    _path[""];
    req >> op;
    switch (op) {
        case PicoMasterReqType::MASTER_GEN:
            master_gen(tcp_socket, req, resp);
            break;
        case PicoMasterReqType::MASTER_ADD:
            master_add(tcp_socket, req, resp);
            break;
        case PicoMasterReqType::MASTER_DEL:
            master_del(req, resp);
            break;
        case PicoMasterReqType::MASTER_SET:
            master_set(req, resp);
            break;
        case PicoMasterReqType::MASTER_GET:
            master_get(req, resp);
            break;
        case PicoMasterReqType::MASTER_SUB:
            master_sub(req, resp);
            break;
        case PicoMasterReqType::MASTER_EXIT:
            exit = true;
            break;
        case PicoMasterReqType::MASTER_CLIENT_FINALIZE:
            disconnect_clear_data(tcp_socket);
            _watchers.erase(tcp_socket);
            break;
        default:
            SLOG(WARNING) << "irrelavent request type: " << int(op);
    }
}

void Master::notify_watchers(const std::string& path) {
    for (auto& watcher: _watchers) {
        RpcResponse resp(watcher.second);
        resp << path;
        watcher.first->send_rpc_message(std::move(resp), false);
    }
}

void Master::master_gen(TcpSocket* tcp_socket, RpcRequest& req, RpcResponse& resp) {
    bool ephemeral;
    std::string parent;
    std::string value;
    req >> parent >> value >> ephemeral;
    
    if (!master_check_valid_path(parent)) {
        SLOG(WARNING) << "master gen path " << parent << " invalid";
        resp << MasterStatus::ERROR;
        return;
    }
    auto pit = _path.find(parent);
    if (pit == _path.end() || pit->second.owner != nullptr) {
        SLOG(WARNING) << "master gen path " << parent << " not found";
        resp << MasterStatus::NODE_FAILED;
        return;
    }
    TcpSocket* owner = ephemeral ? tcp_socket : nullptr;
    std::string key = std::to_string(_gen_id[parent]++);
    while (key.length() < 10) {
        key = '0' + key;
    }
    key = '_' + key;
    std::string path = parent + "/" + key;
    if (key.length() > 11) {
        SLOG(WARNING) << "master gen path " << path << " over limit";
        resp << MasterStatus::ERROR;
        return;
    }
    if (!_path.emplace(path, MasterNode{owner, value, {}}).second) {
        BLOG(DMASTER) << "master gen path " << path << " exist";
        resp << MasterStatus::PATH_FAILED;
        return;
    }
    SCHECK(pit->second.sub.emplace(key).second);
    BLOG(DMASTER) << "master gen path " << path;
    resp << MasterStatus::OK << key;
    notify_watchers(path);
}

void Master::master_add(TcpSocket* tcp_socket, RpcRequest& req, RpcResponse& resp) {
    bool ephemeral;
    std::string path;
    std::string value;
    req >> path >> value >> ephemeral;

    if (!master_check_valid_path(path)) {
        SLOG(WARNING) << "master add path " << path << " invalid";
        resp << MasterStatus::ERROR;
        return;
    }
    size_t p = path.find_last_of('/');
    std::string key = path.substr(p + 1);
    std::string parent = path.substr(0, p);
    auto pit = _path.find(parent);
    if (pit == _path.end() || pit->second.owner != nullptr) {
        if (pit == _path.end()) {
            BLOG(DMASTER) << "master add path " << path << " parent not found";
        } else {
            BLOG(DMASTER) << "master add path " << path << " parent is ephemeral";    
        }
        resp << MasterStatus::PATH_FAILED;
        return;
    }
    TcpSocket* owner = ephemeral ? tcp_socket : nullptr;
    if (!_path.emplace(path, MasterNode{owner, value, {}}).second) {
        BLOG(DMASTER) << "master add path " << path << " exists";
        resp << MasterStatus::NODE_FAILED;
        return;
    }
    SCHECK(pit->second.sub.insert(key).second);
    BLOG(DMASTER) << "master add path " << path;
    resp << MasterStatus::OK;
    notify_watchers(path);
}

void Master::master_del(RpcRequest& req, RpcResponse& resp) {
    std::string path;
    req >> path;

    if (!master_check_valid_path(path)) {
        SLOG(WARNING) << "master del path " << path << " invalid";
        resp << MasterStatus::ERROR;
        return;
    }
    size_t p = path.find_last_of('/');
    std::string key = path.substr(p + 1);
    std::string parent = path.substr(0, p);
    auto it = _path.find(path);
    if (it == _path.end()) {
        BLOG(DMASTER) << "master del path " << path << " not found";
        resp << MasterStatus::NODE_FAILED;
        return;
    }
    if (!it->second.sub.empty()) {
        BLOG(DMASTER) << "master del path " << path << " has children";
        resp << MasterStatus::PATH_FAILED;
        return;        
    }
    _path.erase(it);
    _gen_id.erase(path);
    auto pit = _path.find(parent);
    SCHECK(pit != _path.end());
    SCHECK(pit->second.sub.erase(key));
    BLOG(DMASTER) << "master del path " << path;
    resp << MasterStatus::OK;
    notify_watchers(path);
}

void Master::master_set(RpcRequest& req, RpcResponse& resp) {
    std::string path;
    std::string value;
    req >> path >> value;

    if (!master_check_valid_path(path)) {
        SLOG(WARNING) << "master set path " << path << " invalid";
        resp << MasterStatus::ERROR;
        return;
    }
    auto it = _path.find(path);
    if (it == _path.end()) {
        BLOG(DMASTER) << "master set path " << path << " not found";
        resp << MasterStatus::NODE_FAILED;
        return;
    }
    it->second.value = value;
    BLOG(DMASTER) << "master del path " << path;
    resp << MasterStatus::OK;
    notify_watchers(path);
}

void Master::master_get(RpcRequest& req, RpcResponse& resp) {
    std::string path;
    req >> path;

    if (!master_check_valid_path(path)) {
        SLOG(WARNING) << "master get path " << path << " invalid";
        resp << MasterStatus::ERROR;
        return;
    }
    auto it = _path.find(path);
    if (it == _path.end()) {
        BLOG(DMASTER) << "master get path " << path << " not found";
        resp << MasterStatus::NODE_FAILED;
        return;
    }
    BLOG(DMASTER) << "master get path " << path;
    resp << MasterStatus::OK << it->second.value;
}

void Master::master_sub(RpcRequest& req, RpcResponse& resp) {
    std::string path;
    req >> path;

    if (!master_check_valid_path(path)) {
        SLOG(WARNING) << "master sub path " << path << " invalid";
        resp << MasterStatus::ERROR;
        return;
    }
    auto it = _path.find(path);
    if (it == _path.end()) {
        BLOG(DMASTER) << "master sub path " << path << " not found";
        resp << MasterStatus::NODE_FAILED;
        return;
    }
    std::vector<std::string> children(it->second.sub.begin(), it->second.sub.end());
    BLOG(DMASTER) << "master sub path " << path;
    resp << MasterStatus::OK << children;
}

const char* RANK_KEY = "RANKER";


} // namespace core
} // namespace pico
} // namespace paradigm4
