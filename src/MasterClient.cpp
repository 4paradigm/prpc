
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/lexical_cast.hpp>

#include "AsyncWatcher.h"
#include "MasterClient.h"
#include "pico_lexical_cast.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

constexpr int DCLIENT = 2;

WatcherTable::~WatcherTable() {
    std::lock_guard<std::mutex> lk(_mu);
    SCHECK(_mp.empty());
}

WatcherHandle WatcherTable::insert(const std::string& key, std::function<void()> callback) {
    std::lock_guard<std::mutex> lk(_mu);
    auto watcher = std::make_shared<Watcher>();
    watcher->callback = callback;
    auto& list = _mp[key];
    WatcherHandle handle;
    handle._table = this;
    handle._key = key;
    handle._watcher = list.emplace(list.end(), watcher);
    BLOG(DCLIENT) << "insert one callback of " << key;
    return handle;
}

void WatcherTable::erase(WatcherHandle handle) {
    std::lock_guard<std::mutex> lk(_mu);
    SCHECK(handle._table == this);
    auto it = _mp.find(handle._key);
    SCHECK(it != _mp.end());
    auto watcher = (*handle._watcher);
    {
        std::lock_guard<std::mutex> lkcb(watcher->mutex);
        it->second.erase(handle._watcher);
    }
    BLOG(DCLIENT) << "erase one callback of " << handle._key;
    if (it->second.empty()) {
        _mp.erase(it);
        BLOG(DCLIENT) << "all callback of " << handle._key << " erased";
    }
}

void WatcherTable::invoke(const std::string& key) {
    std::list<std::shared_ptr<Watcher>> watchers;
    std::lock_guard<std::mutex> lk(_mu);
    {
        auto it = _mp.find(key);
        if (it != _mp.end()) {
            watchers = it->second;
        }
    }
    for (auto watcher: watchers) {
        std::lock_guard<std::mutex> lkcb(watcher->mutex);
        watcher->callback();
    }
}

MasterClient::MasterClient(const std::string& root_path) {
    _root_path = root_path;
    if (_root_path.empty()) {
        _root_path = "/";
    } else {
        if (_root_path[0] != '/') {
            _root_path = '/' + _root_path;
        }
        if (_root_path.back() != '/') {
            _root_path = _root_path + '/';
        } 
    }
}

MasterClient::~MasterClient() {}

bool MasterClient::initialize() {
    SLOG(INFO) << "master client initialize";
    std::vector<std::string> segs;
    boost::split(segs, _root_path, boost::is_any_of("/"));
    std::string cur = "";
    for (auto& seg: segs) {
        if (seg != "") {
            cur += '/' + seg;
            tree_node_add(cur);
        }
    }
    tree_node_add(_root_path + PATH_NODE);
    tree_node_add(_root_path + PATH_TASK_STATE);
    tree_node_add(_root_path + PATH_RPC);
    tree_node_add(_root_path + PATH_GENERATE_ID);
    tree_node_add(_root_path + PATH_LOCK);
    tree_node_add(_root_path + PATH_BARRIER);
    tree_node_add(_root_path + PATH_CONTEXT);
    tree_node_add(_root_path + PATH_MODEL);
    SLOG(INFO) << "master client initialized";
    return true;
}

void MasterClient::finalize() {
}

void MasterClient::clear_master() {
    std::string p = _root_path;
    p.pop_back();
    tree_clear_path(p);
}

void MasterClient::set_task_ready() {
    SCHECK(tree_node_add(_root_path + PATH_TASK_STATE + "/ready"));
}

bool MasterClient::set_task_failed(const std::string& message) {
    return tree_node_add(_root_path + PATH_TASK_STATE + "/fail", message);
}

bool MasterClient::get_task_ready() {
    return tree_node_get(_root_path + PATH_TASK_STATE + "/ready");
}

bool MasterClient::get_task_failed(std::string& message) {
    return tree_node_get(_root_path + PATH_TASK_STATE + "/fail", message);
}

bool MasterClient::add_task_node(comm_rank_t g_rank, const std::string& role) {
    tree_node_add(_root_path + PATH_TASK_STATE + "/node");
    return tree_node_add(_root_path + PATH_TASK_STATE + "/node/" + std::to_string(g_rank), role);
}

bool MasterClient::del_task_node(comm_rank_t g_rank) {
    return tree_node_del(_root_path + PATH_TASK_STATE + "/node/" + std::to_string(g_rank));
}

bool MasterClient::get_task_node(const std::string& role, std::vector<comm_rank_t>& g_rank) {
    std::vector<std::string> ranks;
    if (!tree_node_sub(_root_path + PATH_TASK_STATE + "/node", ranks)) {
        return false;
    }
    g_rank.clear();
    g_rank.reserve(ranks.size());
    for (const auto& rank : ranks) {
        std::string rank_role;
        if (role == "") {
            g_rank.push_back(check_stoi(rank));
        } else if (tree_node_get(_root_path + PATH_TASK_STATE + "/node/" + rank, rank_role)) {
            if (rank_role == role || role == "") {
                g_rank.push_back(check_stoi(rank));
            }
        }
    }
    return true;
}

void MasterClient::register_node(const CommInfo& info) {
    std::string rank = std::to_string(info.global_rank);
    SCHECK(tree_node_add(_root_path + PATH_NODE + "/" + rank, info.to_json_str(), true));
    SLOG(INFO) << "register node " << info;
}

bool MasterClient::get_comm_info(comm_rank_t g_rank, CommInfo& info) {
    std::string rank = std::to_string(g_rank);
    std::string str;
    bool ret = tree_node_get(_root_path + PATH_NODE +  "/" + rank, str);
    info.from_json_str(str);
    return ret;
}

bool MasterClient::get_comm_info(std::vector<CommInfo>& info) {
    info.clear();
    std::vector<std::string> ranks;
    tree_node_sub(_root_path + PATH_NODE, ranks);
    info.reserve(ranks.size());
    for (const auto& rank : ranks) {
        CommInfo tmp;
        if (get_comm_info(check_stoi(rank), tmp)) {
            info.push_back(tmp);
        }
    }
    return true;
}

void MasterClient::wait_task_ready() {
    AsyncWatcher watcher;
    WatcherHandle handle = tree_watch(
          _root_path + PATH_TASK_STATE, [&watcher]() { watcher.notify(); });
    watcher.wait([this](){ return tree_node_get(_root_path + PATH_TASK_STATE + "/ready"); });
    cancle_watch(handle);
}

WatcherHandle MasterClient::watch_task_fail(std::function<void(const std::string&)> cb) {
    SCHECK(cb);
    std::string path = _root_path + PATH_TASK_STATE + "/fail";
    WatcherHandle handle = tree_watch(path, [this, path, cb](){
        std::string message;
        if (tree_node_get(path, message)) {
            cb(message);
        }
    });
    std::string message;
    if (tree_node_get(path, message)) {
        cb(message);
    }
    return handle;
}

WatcherHandle MasterClient::watch_task_node(AsyncWatcher& watcher) {
    return tree_watch(_root_path + PATH_TASK_STATE + "/node", [&watcher]() { watcher.notify(); });
}

WatcherHandle MasterClient::watch_comm_node(AsyncWatcher& watcher) {
    return tree_watch(_root_path + PATH_NODE, [&watcher]() { watcher.notify(); });
}

void MasterClient::alloc_role_rank(const std::string& role,
      size_t role_num,
      comm_rank_t global_rank,
      comm_rank_t& role_rank,
      std::vector<comm_rank_t>& all) {
    std::string key = "alloc_role_rank_" + role;
    std::string path = _root_path + key;
    tree_clear_path(path);
    barrier(key, role_num);
    tree_node_add(path);
    SCHECK(tree_node_add(path + '/' + std::to_string(global_rank)));
    barrier(key, role_num);
    all.resize(role_num);
    std::vector<std::string> children;
    tree_node_sub(path, children);
    SCHECK(children.size() == role_num);
    for (size_t i = 0; i < role_num; ++i) {
        comm_rank_t rank = static_cast<comm_rank_t>(check_stoi(children[i]));
        if (rank == global_rank) {
            role_rank = i;
        }
        all[i] = rank;
    }
    barrier(key, role_num);
    SLOG(INFO) << "get role rank : " << role_rank;
}

void MasterClient::barrier(const std::string& barrier_name, size_t number) {
    std::string base_path = _root_path + PATH_BARRIER + '/' + barrier_name;
    std::string node_path = base_path + "/node";
    std::string ready_path = base_path + "/ready";
    
    AsyncWatcher watcher;
    WatcherHandle handle = tree_watch(ready_path, [&watcher](){ watcher.notify(); });
    watcher.wait([this, &ready_path](){ return !tree_node_get(ready_path); });

    tree_node_add(base_path);
    tree_node_add(node_path);
    std::string gen = tree_node_gen(node_path, "", true);
    SCHECK(!gen.empty());
    
    std::vector<std::string> children;
    SCHECK(tree_node_sub(node_path, children));
    if (children.size() == number && gen == children.back()) {
        WatcherHandle h2 = tree_watch(node_path, [&watcher](){ watcher.notify(); });
        SCHECK(tree_node_add(ready_path, "", true));
        watcher.wait([this, &node_path, &children](){ 
            SCHECK(tree_node_sub(node_path, children));
            return children.size() == 1;
        });
        cancle_watch(h2);
        SCHECK(tree_node_del(node_path + '/' + gen));
        SCHECK(tree_node_del(ready_path));
    } else {
        watcher.wait([this, &ready_path](){ return tree_node_get(ready_path); });
        SCHECK(tree_node_del(node_path + '/' + gen));
    }
    cancle_watch(handle);
}

void MasterClient::acquire_lock(const std::string& lock_name) {
    std::string lock_path = _root_path + PATH_LOCK + '/' + lock_name;

    tree_node_add(lock_path);
    std::string gen = tree_node_gen(lock_path, "", true);
    SCHECK(!gen.empty());
    
    AsyncWatcher watcher;
    WatcherHandle handle = tree_watch(lock_path, [&watcher](){ watcher.notify(); });
    watcher.wait([this, &gen, &lock_path](){
        std::vector<std::string> children;
        SCHECK(tree_node_sub(lock_path, children));
        return gen == children[0];
    });
    cancle_watch(handle);

    std::unique_lock<std::mutex> lck(_client_mtx);
    SCHECK(_acquired_lock.emplace(lock_name, lock_path + '/' + gen).second);
}

void MasterClient::release_lock(const std::string& lock_name) {
    std::unique_lock<std::mutex> lck(_client_mtx);
    auto it = _acquired_lock.find(lock_name);
    SCHECK(it != _acquired_lock.end()) << lock_name;
    SCHECK(tree_node_del(it->second));
    _acquired_lock.erase(it);
}

bool MasterClient::add_context(int32_t storage_id, const std::string& context) {
    return tree_node_add(_root_path + PATH_CONTEXT + '/' + std::to_string(storage_id), context);
}

bool MasterClient::set_context(int32_t storage_id, const std::string& context) {
    return tree_node_set(_root_path + PATH_CONTEXT + '/' + std::to_string(storage_id), context);
}

bool MasterClient::get_context(int32_t storage_id, std::string& context) {
    return tree_node_get(_root_path + PATH_CONTEXT + '/' + std::to_string(storage_id), context);
}

bool MasterClient::delete_storage(int32_t storage_id) {
    return tree_node_del(_root_path + PATH_CONTEXT + '/' + std::to_string(storage_id));
}

std::vector<int32_t> MasterClient::get_storage_list() {
    std::vector<int32_t> storage_list;
    std::vector<std::string> storage_ids;
    SCHECK(tree_node_sub(_root_path + PATH_CONTEXT, storage_ids));
    for (auto& storage_id: storage_ids) {
        storage_list.push_back(check_stoi(storage_id));
    }
    return storage_list;
}

bool MasterClient::add_model(const std::string& name, const std::string& model) {
    return tree_node_add(_root_path + PATH_MODEL + '/' + name, model);
}

bool MasterClient::set_model(const std::string& name, const std::string& model) {
    return tree_node_set(_root_path + PATH_MODEL + '/' + name, model);
}

bool MasterClient::get_model(const std::string& name, std::string& model) {
    return tree_node_get(_root_path + PATH_MODEL + '/' + name, model);
}

bool MasterClient::del_model(const std::string& name) {
    return tree_node_del(_root_path + PATH_MODEL + '/' + name);
}

std::vector<std::string> MasterClient::get_model_names() {
    std::vector<std::string> names;
    SCHECK(tree_node_sub(_root_path + PATH_MODEL, names));
    return names;
}

WatcherHandle MasterClient::watch_model(const std::string& name, std::function<void()> cb) {
    return tree_watch(_root_path + PATH_MODEL + '/' + name, cb);
}

bool MasterClient::get_rpc_service_info(const std::string& rpc_service_api,
      std::vector<RpcServiceInfo>& out) {
    std::vector<std::string> names;
    out.clear();
    if (!tree_node_sub(_root_path + PATH_RPC + '/' + rpc_service_api, names)) {
        return false;
    }
    for (const auto& name : names) {
        RpcServiceInfo info;
        if (!get_rpc_service_info(rpc_service_api, name, info)) {
            return false;
        }
        out.push_back(std::move(info));
    }
    return true;
}

bool MasterClient::get_rpc_service_info(const std::string& rpc_service_api,
      const std::string& rpc_name,
      RpcServiceInfo& out) {
    std::string path = _root_path + PATH_RPC + '/' + rpc_service_api + '/' + rpc_name;
    std::string rpc_id;
    if (!tree_node_get(path, rpc_id)) {
        return false;
    }
    out.rpc_id = check_stoi(rpc_id);
    std::vector<std::string> sids;
    if (!tree_node_sub(path, sids)) {
        return false;
    }
    out.servers.clear();
    out.rpc_service_name = rpc_name;
    for (const auto& sid : sids) {
        std::string g_rank;
        if (!tree_node_get(path + '/' + sid, g_rank)) {
            continue;
        }
        out.servers.push_back(
              {check_stoi(sid), static_cast<comm_rank_t>(check_stoi(g_rank))});
    }
    return true;
}

bool MasterClient::del_rpc_service_info(const std::string& rpc_service_api,
      const std::string& rpc_name) {
    std::string path = _root_path + PATH_RPC + '/' + rpc_service_api + '/' + rpc_name;
    if (tree_node_get(path)) {
        tree_clear_path(path);
        return true;
    } else {
        return false;
    }
}

WatcherHandle MasterClient::watch_rpc_service_info(
      const std::string& rpc_service_api,
      std::function<void()> cb) {
    SCHECK(cb);
    std::string path = _root_path + PATH_RPC + '/' + rpc_service_api;
    tree_node_add(path);
    WatcherHandle handle = tree_watch(path, cb);
    return handle;
}

void MasterClient::register_rpc_service(const std::string& rpc_service_api,
      const std::string& rpc_name,
      int& rpc_id) {
    std::string rpc_key = rpc_service_api + "$" + rpc_name;
    acquire_lock(rpc_key);
    std::string path =  _root_path + PATH_RPC + '/' + rpc_service_api;
    std::string info_str;
    tree_node_add(path);
    path += '/' + rpc_name;
    if (!tree_node_get(path, info_str)) {
        rpc_id = generate_id(rpc_service_api);
        tree_node_add(path, std::to_string(rpc_id));
    } else {
        rpc_id = check_stoi(info_str);
    }
    SLOG(INFO) << "register service :  " << rpc_service_api << " " 
              << rpc_name << " " << rpc_id;
    release_lock(rpc_key);
}

bool MasterClient::deregister_rpc_service(const std::string& rpc_service_api,
      const std::string& rpc_name) {
    std::string rpc_key = rpc_service_api + "$" + rpc_name;
    acquire_lock(rpc_key);
    bool ret = del_rpc_service_info(rpc_service_api, rpc_name);
    release_lock(rpc_key);
    return ret;
}


bool MasterClient::register_server(const std::string& rpc_service_api,
      const std::string& rpc_name,
      int global_rank,
      int& rpc_id,
      int& server_id) {
    register_rpc_service(rpc_service_api, rpc_name, rpc_id);
    SLOG(INFO) << "register server :  " << rpc_service_api << " " 
              << rpc_name << " " << rpc_id << " " << global_rank;
    std::string rpc_key = rpc_service_api + "$" + rpc_name;
    if (server_id == -1) {
        server_id = generate_id(rpc_key);
    }
    std::string path = _root_path + PATH_RPC + '/' + rpc_service_api + '/'
                       + rpc_name + '/' + std::to_string(server_id);
    if (!tree_node_add(path, std::to_string(global_rank), true)) {
        return false;
    }
    return true;
}

bool MasterClient::deregister_server(const std::string& rpc_service_api,
      const std::string& rpc_name,
      int server_id) {
    std::string path = _root_path + PATH_RPC + '/' + rpc_service_api + '/'
                       + rpc_name + '/' + std::to_string(server_id);
    return tree_node_del(path);
}

size_t MasterClient::generate_id(const std::string& key) {
    std::string path = _root_path + PATH_GENERATE_ID + '/' + key;
    tree_node_add(path);
    std::string gen = tree_node_gen(path, "", true);
    SCHECK(gen.size() > 1);
    return stoi(gen.substr(1));
}


void MasterClient::reset_generate_id(const std::string& key) {
    std::string path = _root_path + PATH_GENERATE_ID + '/' + key;
    tree_clear_path(path);
}

void MasterClient::cancle_watch(WatcherHandle handle) {
    _table.erase(handle);
}

void MasterClient::notify_watchers(const std::string& path) {
    BLOG(DCLIENT) << "master handle event of path " << path;
    std::vector<std::string> segs;
    boost::split(segs, path, boost::is_any_of("/"));
    std::string cur = "";
    for (auto& seg: segs) {
        if (seg != "") {
            cur += '/' + seg;
            _table.invoke(cur);
        }
    }
}

#define RETRY_MASTER_METHOD(method, params...)                                  \
    SCHECK(master_check_valid_path(path)) << path;                              \
    MasterStatus status;                                                        \
    do {                                                                        \
        status = method(params);                                                \
        SCHECK(status != MasterStatus::ERROR);                                  \
    } while (status == MasterStatus::DISCONNECTED);                             \
    BLOG(DCLIENT) << #method << " " << path << ": " << (int)status;             \
    return status == MasterStatus::OK;                                          \


std::string MasterClient::tree_node_gen(const std::string& path, const std::string& value, bool ephemeral) {
    SCHECK(master_check_valid_path(path)) << path;
    std::string gen;
    MasterStatus status;                   
    do {                             
        status = master_gen(path, value, gen, ephemeral);       
        SCHECK(status != MasterStatus::ERROR);
        BLOG(DCLIENT) << "master_gen" << " " << path << ": " << (int)status;
    } while (status == MasterStatus::DISCONNECTED);
    return gen;
}

void MasterClient::tree_clear_path(const std::string& path) {
    std::vector<std::string> children;
    while (tree_node_sub(path, children)) {
        for (auto& child: children) {
            tree_clear_path(path + '/' + child);
        }
        tree_node_del(path);
    }
}
bool MasterClient::tree_node_add(const std::string& path, const std::string& value, bool ephemeral) {
    RETRY_MASTER_METHOD(master_add, path, value, ephemeral);
}
bool MasterClient::tree_node_set(const std::string& path, const std::string& value) {
    RETRY_MASTER_METHOD(master_set, path, value);
}
bool MasterClient::tree_node_get(const std::string& path, std::string& value) {
    RETRY_MASTER_METHOD(master_get, path, value);  
}
bool MasterClient::tree_node_get(const std::string& path) {
    RETRY_MASTER_METHOD(master_get, path);  
}
bool MasterClient::tree_node_del(const std::string& path) {
    RETRY_MASTER_METHOD(master_del, path); 
}
bool MasterClient::tree_node_sub(const std::string& path, std::vector<std::string>& children) {
    RETRY_MASTER_METHOD(master_sub, path, children);   
}
WatcherHandle MasterClient::tree_watch(const std::string& path, std::function<void()> cb) {
    SCHECK(master_check_valid_path(path)) << path;
    auto ret = _table.insert(path, cb);

    // 对于zkmaster只会在tree_node_get和tree_node_sub的同时注册一次性的监听。
    // 如果cb中有tree_node_get tree_node_sub，则当对应的内容发生变化时仍会调用cb，否则有可能不会再调用cb。
    // 理论上如果不调用get|sub，就说明client不需要相应的内容，那么即使发生了变化，也没有必要再通知client。
    // 所以如果某个程序依赖于永久监听，那么这个程序应该是不严谨的，很可能存在一些隐蔽的问题。
    std::vector<std::string> children;
    tree_node_get(path);
    tree_node_sub(path, children);
    return ret;
}

const char* MasterClient::PATH_NODE = "_node_";
const char* MasterClient::PATH_TASK_STATE = "_task_state_";
const char* MasterClient::PATH_GENERATE_ID = "_id_gen_";
const char* MasterClient::PATH_LOCK = "_lock_";
const char* MasterClient::PATH_BARRIER = "_barrier_";
const char* MasterClient::PATH_RPC = "_rpc_";
const char* MasterClient::PATH_CONTEXT = "_context_";
const char* MasterClient::PATH_MODEL = "_model_";

} // namespace core
} // namespace pico
} // namespace paradigm4
