#ifndef PARADIGM4_PICO_CORE_MASTER_CLIENT_H
#define PARADIGM4_PICO_CORE_MASTER_CLIENT_H

#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "zookeeper/zookeeper.h"

#include "Archive.h"
#include "AsyncReturn.h"
#include "AsyncWatcher.h"
#include "Master.h"
#include "RpcChannel.h"
#include "SpinLock.h"
#include "TcpSocket.h"
#include "pico_lexical_cast.h"

namespace paradigm4 {
namespace pico {
namespace core {

class WatcherTable {
public:
    struct Watcher {
        std::function<void()> callback;
    };

    class WatcherHandle {
        friend WatcherTable;

    private:
        WatcherTable* _table = nullptr;
        std::string _key = "";
        std::list<std::shared_ptr<Watcher>>::iterator _watcher;
    };

    ~WatcherTable();
    void invoke(const std::string& key);
    WatcherHandle insert(const std::string& key, std::function<void()> callback);
    void erase(WatcherHandle handle);

private:
    std::mutex _mu;
    std::unordered_map<std::string, std::list<std::shared_ptr<Watcher>>> _mp;
};

typedef WatcherTable::WatcherHandle WatcherHandle;

class AsyncWatcher;

class MasterClient {
public:
    MasterClient(const std::string& root_path);
    virtual ~MasterClient();

    virtual bool initialize();
    virtual void finalize();
    virtual std::string endpoint() = 0;
    virtual bool connected() = 0;
    virtual bool reconnect() = 0;

    void initialize_paths();
    void clear_master();
    void set_task_ready();
    bool get_task_ready();
    bool set_task_failed(const std::string& message);
    bool get_task_failed(std::string& message);
    bool add_task_node(comm_rank_t g_rank, const std::string& role);
    bool del_task_node(comm_rank_t g_rank);
    bool get_task_node(const std::string& role, std::vector<comm_rank_t>&);

    void register_node(const CommInfo& info);
    bool get_comm_info(std::vector<CommInfo>&);
    bool get_comm_info(comm_rank_t g_rank, CommInfo&);

    void register_rpc_service(const std::string& rpc_service_api,
          const std::string& rpc_name,
          int& rpc_id);

    bool deregister_rpc_service(const std::string& rpc_service_api,
          const std::string& rpc_name);

    bool del_rpc_service_info(const std::string& rpc_service_api,
          const std::string& rpc_name);

    // XXX
    bool register_server(const std::string& rpc_service_api,
          const std::string& rpc_name,
          int global_rank,
          int& rpc_id,
          int& server_id);

    // XXX
    bool deregister_server(const std::string& rpc_service_api,
          const std::string& rpc_name,
          int server_id);

    // XXX
    bool get_rpc_service_info(const std::string& rpc_service_api,
          std::vector<RpcServiceInfo>& out);
    bool get_rpc_service_info(const std::string& rpc_service_api,
          const std::string& rpc_name,
          RpcServiceInfo& out);
    WatcherHandle watch_rpc_service_info(const std::string& rpc_service_api,
          std::function<void()>);
    WatcherHandle watch_node(std::function<void()>);

    size_t generate_id(const std::string& key);
    void reset_generate_id(const std::string& key);

    void wait_task_ready();
    WatcherHandle watch_task_fail(std::function<void(const std::string&)>);
    WatcherHandle watch_task_node(AsyncWatcher&);
    WatcherHandle watch_comm_node(AsyncWatcher&);

    void alloc_role_rank(const std::string& role,
          size_t role_num,
          comm_rank_t g_rank,
          comm_rank_t& r_rank,
          std::vector<comm_rank_t>& all);

    void barrier(const std::string& barrier_name, size_t number);
    void acquire_lock(const std::string& lock_name);
    void release_lock(const std::string& lock_name);

    bool add_context(int32_t storage_id, const std::string& context);
    bool set_context(int32_t storage_id, const std::string& context);
    bool get_context(int32_t storage_id, std::string& context);
    bool delete_storage(int32_t storage_id);
    std::vector<int32_t> get_storage_list();

    bool add_model(const std::string& name, const std::string& model);
    bool set_model(const std::string& name, const std::string& model);
    bool get_model(const std::string& name, std::string& model);
    bool del_model(const std::string& name);
    std::vector<std::string> get_model_names();
    WatcherHandle watch_model(const std::string& name, std::function<void()>);

    void cancle_watch(WatcherHandle);


    std::string tree_node_gen(std::string path,
          const std::string& value = "",
          bool ephemeral = false);
    void tree_clear_path(std::string path);
    bool tree_node_add(std::string path,
          const std::string& value = "",
          bool ephemeral = false);
    bool tree_node_set(std::string path, const std::string& value);
    bool tree_node_get(std::string path, std::string& value);
    bool tree_node_get(std::string path);
    bool tree_node_del(std::string path);
    bool tree_node_sub(std::string path,
          std::vector<std::string>& children);
    WatcherHandle tree_watch(std::string path, std::function<void()>);


protected:
    void notify_watchers(const std::string& path);
    virtual MasterStatus master_gen(const std::string& path,
          const std::string& value,
          std::string& gen,
          bool ephemeral)
          = 0;
    virtual MasterStatus master_add(const std::string& path,
          const std::string& value,
          bool ephemeral)
          = 0;
    virtual MasterStatus master_set(const std::string& path,
          const std::string& value)
          = 0;
    virtual MasterStatus master_get(const std::string& path, std::string& value)
          = 0;
    virtual MasterStatus master_get(const std::string& path) = 0;
    virtual MasterStatus master_del(const std::string& path) = 0;
    virtual MasterStatus master_sub(const std::string& path,
          std::vector<std::string>& children)
          = 0;

private:
    std::string _root_path;
    WatcherTable _table;

    std::mutex _client_mtx;
    std::unordered_map<std::string, std::string> _acquired_lock;

    static const std::string PATH_NODE;
    static const std::string PATH_TASK_STATE;
    static const std::string PATH_GENERATE_ID;
    static const std::string PATH_LOCK;
    static const std::string PATH_BARRIER;
    static const std::string PATH_RPC;
    static const std::string PATH_CONTEXT;
    static const std::string PATH_MODEL;
};

class ZkMasterClient : public MasterClient {
public:
    ZkMasterClient(const std::string& root_path,
          const std::string& hosts,
          int recv_timeout_ms,
          int disconnect_timeout_sec);
    ~ZkMasterClient() override;

    bool initialize() override;
    void finalize() override;

    std::string endpoint() override;
    bool connected() override;
    bool reconnect() override;

protected:
    virtual MasterStatus master_gen(const std::string& path,
          const std::string& value,
          std::string& gen,
          bool ephemeral);
    virtual MasterStatus master_add(const std::string& path,
          const std::string& value,
          bool ephemeral);
    virtual MasterStatus master_set(const std::string& path,
          const std::string& value);
    virtual MasterStatus master_get(const std::string& path,
          std::string& value);
    virtual MasterStatus master_get(const std::string& path);
    virtual MasterStatus master_del(const std::string& path);
    virtual MasterStatus master_sub(const std::string& path,
          std::vector<std::string>& children);

private:
    static void handle_event_wrapper(zhandle_t* zh,
          int type,
          int state,
          const char* path,
          void* watcher_ctx);
    void handle_event(int type, int state, const char* path);
    MasterStatus check_zk_add(int ret);
    MasterStatus check_zk_del(int ret);
    MasterStatus check_zk_set(int ret);
    const char* state2string(int state);
    const char* event2string(int ev);

    std::string _hosts;
    int32_t _recv_timeout;
    int32_t _disconnect_timeout_sec;
    std::mutex _connected_mu;
    bool _connected = false;
    std::string _endpoint;
    zhandle_t* _zh = nullptr;

    std::mutex _mu;
    std::condition_variable _cv;
};

class TcpMasterClient : public MasterClient {
public:
    TcpMasterClient(const std::string& master_ep, const std::string& root_path = "root");
    ~TcpMasterClient() override;

    bool initialize() override;
    void finalize() override;

    std::string endpoint() override;
    bool connected() override;
    bool reconnect() override;

protected:
    virtual MasterStatus master_gen(const std::string& path,
          const std::string& value,
          std::string& gen,
          bool ephemeral);
    virtual MasterStatus master_add(const std::string& path,
          const std::string& value,
          bool ephemeral);
    virtual MasterStatus master_set(const std::string& path,
          const std::string& value);
    virtual MasterStatus master_get(const std::string& path,
          std::string& value);
    virtual MasterStatus master_get(const std::string& path);
    virtual MasterStatus master_del(const std::string& path);
    virtual MasterStatus master_sub(const std::string& path,
          std::vector<std::string>& children);

private:
    void listening();
    void run_cb();
    AsyncReturnV<RpcResponse> send_request(RpcRequest&& msg);

    std::unique_ptr<TcpSocket> _tcp_socket;
    std::string _master_ep;
    int _exit_fd;
    std::mutex _lk;

    std::thread _listening_th;

    RpcChannel<std::function<void()>> _cb_ch;
    std::thread _cb_th;

    std::unordered_map<int, AsyncReturnV<RpcResponse>> _as_ret;
    std::atomic<int32_t> _id_gen;
};

class MasterUniqueLock {
public:
    MasterUniqueLock(MasterClient* client, const std::string& lock_name)
        : _client(client), _lock_name(lock_name) {
        _client->acquire_lock(_lock_name);
    }

    MasterUniqueLock(std::shared_ptr<MasterClient> client,
          const std::string& lock_name)
        : _client(client.get()), _lock_name(lock_name) {
        _client->acquire_lock(_lock_name);
    }

    ~MasterUniqueLock() {
        _client->release_lock(_lock_name);
    }

private:
    MasterClient* _client;
    std::string _lock_name;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_MASTER_CLIENT_H
