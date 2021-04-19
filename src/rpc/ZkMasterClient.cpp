#include "MasterClient.h"

#include <algorithm>

#include "PicoJsonNode.h"
#include "pico_lexical_cast.h"

namespace paradigm4 {
namespace pico {
namespace core {

ZkMasterClient::ZkMasterClient(const std::string& root_path, 
      const std::string& hosts, 
      int recv_timeout_ms,
      int disconnect_timeout_sec) : MasterClient(root_path) {
    _hosts = hosts;
    _disconnect_timeout_sec = disconnect_timeout_sec;
    _recv_timeout = recv_timeout_ms;
}

ZkMasterClient::~ZkMasterClient() {}

bool ZkMasterClient::initialize() {
    {
        std::unique_lock<std::mutex> lock(_mu);
        SLOG(INFO) << "zk master client initialize";
        _zh = zookeeper_init(
                _hosts.c_str(), handle_event_wrapper, _recv_timeout, nullptr, (void*)this, 0);
        _cv.wait_for(lock, std::chrono::milliseconds(_recv_timeout));
        if (_zh == NULL || !_connected) {
            SLOG(WARNING) << "fail to init zk handler with hosts \"" << _hosts
                            << "\", recv_timeout " << _recv_timeout;
            return false;
        }
        int fd, interest;
        struct timeval tv;
        sockaddr_in local_addr;
        socklen_t len = sizeof(sockaddr_in);
        zookeeper_interest(_zh, &fd, &interest, &tv);
        PSCHECK(getsockname(fd, (struct sockaddr*)&local_addr, &len) == 0);
        _endpoint = inet_ntoa(local_addr.sin_addr);
        _endpoint += ":" + std::to_string(ntohs(local_addr.sin_port));
        SLOG(INFO) << "zk master client initialized";
    }
    return MasterClient::initialize();
}

void ZkMasterClient::finalize() {
    MasterClient::finalize();
    SLOG(INFO) << "zk master client finalize";
    {
        std::lock_guard<std::mutex> _(_connected_mu);
        _connected = false;
    }
    std::unique_lock<std::mutex> lock(_mu);
    if (_zh) {
        zookeeper_close(_zh);
        _zh = NULL;
    }
    SLOG(INFO) << "zk master client finalized";
}

std::string ZkMasterClient::endpoint() {
    std::unique_lock<std::mutex> lock(_mu);
    return _endpoint;
}

bool ZkMasterClient::reconnect() {
    std::unique_lock<std::mutex> lock(_mu);
    if (_zh != NULL) {
        zookeeper_close(_zh);
    }
    _zh = zookeeper_init(
            _hosts.c_str(), handle_event_wrapper, _recv_timeout, nullptr, (void*)this, 0);
    _cv.wait_for(lock, std::chrono::milliseconds(_recv_timeout));
    if (_zh == NULL || !_connected) {
        SLOG(WARNING) << "fail to init zk handler with hosts \"" << _hosts
                        << "\", recv_timeout " << _recv_timeout;
        return false;
    }
    return true;
}

bool ZkMasterClient::connected() {
    return _connected;
}


MasterStatus ZkMasterClient::master_gen(const std::string& path,
      const std::string& value, std::string& gen, bool ephemeral) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    std::string x = path + "/_";
    int flags = ZOO_SEQUENCE;
    flags |= ephemeral ? ZOO_EPHEMERAL : 0;
    std::vector<char> buffer(path.size() + 100);
    int ret = zoo_create(_zh, x.c_str(),
            value.c_str(), value.size() + 1, &ZOO_OPEN_ACL_UNSAFE, 
            flags, buffer.data(), buffer.size());
    if (ret != ZOK) {
        return check_zk_add(ret);
    }
    gen = buffer.data();
    SCHECK(gen.size() > path.size());
    SCHECK(gen.substr(0, path.size()) == path);
    gen = gen.substr(path.size() + 1);
    return MasterStatus::OK;
}


MasterStatus ZkMasterClient::master_add(const std::string& path, const std::string& value, bool ephemeral) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    int flags = ephemeral ? ZOO_EPHEMERAL : 0;
    return check_zk_add(zoo_create(_zh, path.c_str(), 
          value.c_str(), value.size(), &ZOO_OPEN_ACL_UNSAFE, 
          flags, nullptr, 0));
}


MasterStatus ZkMasterClient::master_set(const std::string& path, const std::string& value) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    return check_zk_set(zoo_set(_zh, path.c_str(), value.c_str(), value.size(), -1));
}


MasterStatus ZkMasterClient::master_get(const std::string& path, std::string& value) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    Stat stat;
    std::vector<char> buffer;

    int len = 0;
    stat.dataLength = 100;
    while (len != stat.dataLength) {
        len = stat.dataLength;
        buffer.resize(len + 1);
        MasterStatus ret = check_zk_set(zoo_get(
              _zh, path.c_str(), 1, buffer.data(), &len, &stat));
        if (ret != MasterStatus::OK) {
            return ret;
        }
    }
    value = buffer.data();
    return MasterStatus::OK;
}


MasterStatus ZkMasterClient::master_get(const std::string& path) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    Stat stat;
    return check_zk_set(zoo_exists(_zh, path.c_str(), 1, &stat));
}

MasterStatus ZkMasterClient::master_del(const std::string& path) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    return check_zk_del(zoo_delete(_zh, path.c_str(), -1));
}

MasterStatus ZkMasterClient::master_sub(const std::string& path, std::vector<std::string>& children) {
    std::lock_guard<std::mutex> lock(_mu);
    if (!_connected) {
        return MasterStatus::DISCONNECTED;
    }
    struct String_vector data;
    data.count = 0;
    data.data = NULL;
    MasterStatus ret = check_zk_set(
          zoo_get_children(_zh, path.c_str(), 1, &data));
    if (ret != MasterStatus::OK) {
        return ret;
    }
    children.clear();
    for (int32_t i = 0; i < data.count; i++) {
        children.emplace_back(data.data[i]);
    }
    std::sort(children.begin(), children.end());
    return MasterStatus::OK;
}






MasterStatus ZkMasterClient::check_zk_add(int ret) {
    if (ret == ZOK) {
        return MasterStatus::OK;
    }
    if (ret == ZNONODE || ret == ZNOCHILDRENFOREPHEMERALS) {
        return MasterStatus::PATH_FAILED;
    }
    if (ret == ZNODEEXISTS) {
        return MasterStatus::NODE_FAILED;
    }
    if (ret == ZCONNECTIONLOSS || ret == ZSESSIONEXPIRED) {
        return MasterStatus::DISCONNECTED;
    }
    return MasterStatus::ERROR;
}

MasterStatus ZkMasterClient::check_zk_del(int ret) {
    if (ret == ZOK) {
        return MasterStatus::OK;
    }
    if (ret == ZNONODE) {
        return MasterStatus::NODE_FAILED;
    }
    if (ret == ZNOTEMPTY) {
        return MasterStatus::PATH_FAILED;
    }
    if (ret == ZCONNECTIONLOSS || ret == ZSESSIONEXPIRED) {
        return MasterStatus::DISCONNECTED;
    }
    return MasterStatus::ERROR; 
}

MasterStatus ZkMasterClient::check_zk_set(int ret) {
    if (ret == ZOK) {
        return MasterStatus::OK;
    }
    if (ret == ZNONODE) {
        return MasterStatus::NODE_FAILED;
    }
    if (ret == ZCONNECTIONLOSS || ret == ZSESSIONEXPIRED) {
        return MasterStatus::DISCONNECTED;
    }
    return MasterStatus::ERROR; 
}


int ZkMasterClient::session_timeout_ms() {
    return zoo_recv_timeout(_zh);
}


/* zookeeper state constants, copyed from zookeeper/src/c/src/zk_adapter.h*/
#define EXPIRED_SESSION_STATE_DEF -112
#define AUTH_FAILED_STATE_DEF -113
#define CONNECTING_STATE_DEF 1
#define ASSOCIATING_STATE_DEF 2
#define CONNECTED_STATE_DEF 3
#define NOTCONNECTED_STATE_DEF 999

/* zookeeper event type constants, copyed from zookeeper/src/c/src/zk_adapter.h */
#define CREATED_EVENT_DEF 1
#define DELETED_EVENT_DEF 2
#define CHANGED_EVENT_DEF 3
#define CHILD_EVENT_DEF 4
#define SESSION_EVENT_DEF -1
#define NOTWATCHING_EVENT_DEF -2

const char* ZkMasterClient::state2string(int state) {
    switch (state) {
    case 0:
        return "ZOO_CLOSED_STATE";
    case EXPIRED_SESSION_STATE_DEF:
        return "ZOO_EXPIRED_SESSION_STATE";
    case AUTH_FAILED_STATE_DEF:
        return "ZOO_AUTH_FAILED_STATE";
    case CONNECTING_STATE_DEF:
        return "ZOO_CONNECTING_STATE";
    case ASSOCIATING_STATE_DEF:
        return "ZOO_ASSOCIATING_STATE";
    case CONNECTED_STATE_DEF:
        return "ZOO_CONNECTED_STATE";
    case NOTCONNECTED_STATE_DEF:
        return "ZOO_NOTCONNECTED_STATE";
    }
    return "INVALID_STATE";
}

const char* ZkMasterClient::event2string(int ev) {
    switch (ev) {
    case 0:
        return "ZOO_ERROR_EVENT";
    case CREATED_EVENT_DEF:
        return "ZOO_CREATED_EVENT";
    case DELETED_EVENT_DEF:
        return "ZOO_DELETED_EVENT";
    case CHANGED_EVENT_DEF:
        return "ZOO_CHANGED_EVENT";
    case CHILD_EVENT_DEF:
        return "ZOO_CHILD_EVENT";
    case SESSION_EVENT_DEF:
        return "ZOO_SESSION_EVENT";
    case NOTWATCHING_EVENT_DEF:
        return "ZOO_NOTWATCHING_EVENT";
    }
    return "INVALID_EVENT";
} 


void ZkMasterClient::handle_event_wrapper(zhandle_t* zh, int type, int state, const char* path, void*) {
    ZkMasterClient* client = (ZkMasterClient*)zoo_get_context(zh);
    if (client) {
        client->handle_event(type, state, path);
    }
}

void ZkMasterClient::handle_event(int type, int state, const char* path) {
    // SLOG(INFO) << "Zookeeper event with type " << event2string(type) << ", state "
    //             << state2string(state) << ", node \"" << path << "\"";
    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            std::lock_guard<std::mutex> lock(_mu);
            _connected = true;
            _cv.notify_one();
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            std::lock_guard<std::mutex> lock(_mu);
            _connected = false;
            SLOG(FATAL) << "session has expired";
        }
    } else if (type == ZOO_CREATED_EVENT || 
          type == ZOO_DELETED_EVENT || 
          type == ZOO_CHANGED_EVENT || 
          type == ZOO_CHILD_EVENT) {
        std::lock_guard<std::mutex> _(_connected_mu);
        if (_connected) {
            std::vector<std::string> tmp;
            master_get(path);
            master_sub(path, tmp);
            notify_watchers(path);
        }
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4
