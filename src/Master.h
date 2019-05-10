#ifndef PARADIGM4_PICO_COMMON_MASTER_H
#define PARADIGM4_PICO_COMMON_MASTER_H

#include <utility>
#include <cstddef>
#include <poll.h>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Archive.h"
#include "TcpSocket.h"
#include "common.h"
#include "defs.h"

namespace paradigm4 {
namespace pico {
namespace core {

constexpr int WATCHER_NOTIFY_RPC_ID = -1;

enum class MasterStatus {
    OK = 0,
    NODE_FAILED = 1,
    PATH_FAILED = 2,
    DISCONNECTED = 3,
    ERROR = 4
};
PICO_ENUM_SERIALIZATION(MasterStatus, int8_t);

enum class PicoMasterReqType : int8_t {
    MASTER_GEN,
    MASTER_ADD,
    MASTER_DEL,
    MASTER_GET,
    MASTER_SET,
    MASTER_SUB,
    MASTER_EXIT,
    MASTER_CLIENT_FINALIZE,
};
PICO_ENUM_SERIALIZATION(PicoMasterReqType, int8_t);

struct CommInfo {
    comm_rank_t global_rank = -1;
    std::string endpoint;

    PICO_SERIALIZATION(global_rank, endpoint);

    bool operator<(const CommInfo& o) const {
        return global_rank < o.global_rank;
    }

    bool operator==(const CommInfo& o) const {
        return global_rank == o.global_rank;
    }

    void to_json_node(PicoJsonNode& node) const;
    std::string to_json_str() const;
    void from_json_node(const PicoJsonNode& node);
    void from_json_str(const std::string& str);
    friend std::ostream& operator<<(std::ostream& stream,
          const CommInfo& comm_info);
};

struct ServerInfo {
    int server_id;
    comm_rank_t global_rank;
    PICO_SERIALIZATION(server_id, global_rank);
};

struct RpcServiceInfo {
    std::string rpc_service_name;
    int rpc_id;
    std::vector<ServerInfo> servers;

    PICO_SERIALIZATION(rpc_service_name, rpc_id, servers);
    void to_json_node(PicoJsonNode& node) const;
    std::string to_json_str() const;
    void from_json_node(const PicoJsonNode& node);
    void from_json_str(const std::string& str);
    friend std::ostream& operator<<(std::ostream& stream,
          const RpcServiceInfo& comm_info);
};

bool master_check_valid_path(const std::string& path);

class Master {
public:
    Master(std::string wrapper_ip) : _bind_ip(std::move(wrapper_ip)) {}

    void initialize();
    void finalize();
    void exit();
    const std::string& endpoint() const {
        return _ep;
    }

private:
    void serving();
    std::vector<int> poll();
    void disconnect_clear_data(TcpSocket* fd);

    void notify_watchers(const std::string& path);
    void handle_op(TcpSocket* tcp_socket,
          RpcRequest& req,
          RpcResponse& resp,
          bool& exit);
    void master_gen(TcpSocket* tcp_socket, RpcRequest& req, RpcResponse& resp);
    void master_add(TcpSocket* tcp_socket, RpcRequest& req, RpcResponse& resp);
    void master_del(RpcRequest& req, RpcResponse& resp);
    void master_set(RpcRequest& req, RpcResponse& resp);
    void master_get(RpcRequest& req, RpcResponse& resp);
    void master_sub(RpcRequest& req, RpcResponse& resp);
    struct MasterNode {
        TcpSocket* owner = nullptr;
        std::string value;
        std::set<std::string> sub;
    };

    std::string _bind_ip, _ep;
    std::thread _th;
    std::unordered_map<std::string, int> _gen_id;
    std::unordered_map<std::string, MasterNode> _path;
    std::unique_ptr<TcpAcceptor> _tcp_acceptor;
    /* The hasmap that stores <fd, TcpSocket> pair */
    std::unordered_map<int, std::unique_ptr<TcpSocket>> _fd_socket_map;
    std::unordered_map<TcpSocket*, RpcRequest> _watchers;
    std::vector<pollfd> _fds;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_MASTER_H
