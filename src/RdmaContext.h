#ifndef PARADIGM4_PICO_COMMON_RDMA_CONTEXT_H
#define PARADIGM4_PICO_COMMON_RDMA_CONTEXT_H
#ifdef USE_RDMA
#include "PicoJsonNode.h"
#include "SpinLock.h"
#include "flags.h"
#include <map>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

namespace paradigm4 {
namespace pico {
namespace core {

struct RdmaConfig {
    std::string ib_devname;
    int gid_index;
    int ib_port;
    int traffic_class;
    int sl;
    int mtu;
    int pkey_index;
    int min_rnr_timer;
    int retry_cnt;
    int timeout;
};

struct mr_t {
    // 没有内存所有权，只有mr所有权
    char* start;
    size_t len;
    struct ibv_mr* mr;
    size_t cnt;
};

class RdmaContext {
public:
    int timeout() {
        return _timeout;
    }

    int rnr_retry() {
        return _rnr_retry;
    }

    int retry_cnt() {
        return _retry_cnt;
    }

    int min_rnr_timer() {
        return _min_rnr_timer;
    }

    int pkey_index() {
        return _pkey_index;
    }

    int ib_port() {
        return _ib_port;
    }

    ibv_mtu mtu() {
        switch (_mtu) {
        case 256:
            return IBV_MTU_256;
        case 512:
            return IBV_MTU_512;
        case 1024:
            return IBV_MTU_1024;
        case 2048:
            return IBV_MTU_2048;
        case 4096:
            return IBV_MTU_4096;
        default:
            SLOG(FATAL) << " Invalid MTU - " << _mtu
                          << ". Please choose mtu from "
                             "{256,512,1024,2048,4096}.";
            return IBV_MTU_1024;
        }
    }

    int sl() {
        return _sl;
    }

    ibv_gid gid() {
        return _gid;
    }

    int gid_index() {
        return _gid_index;
    }

    int traffic_class() {
        return _traffic_class;
    }

    ibv_context* ib_ctx() {
        return _ib_ctx;
    }

    ibv_pd* pd() {
        return _pd;
    }

    int max_reads() {
        return _max_reads;
    }

    ibv_port_attr query_port_attr() {
        ibv_port_attr port_attr;
        PSCHECK(ibv_query_port(_ib_ctx, _ib_port, &port_attr) == 0)
              << "Unable to query port attributes";
        return port_attr;
    }

    ibv_device_attr query_device() {
        struct ibv_device_attr attr;
        PSCHECK(ibv_query_device(_ib_ctx, &attr) == 0);
        return attr;
    }

    void from_rdma_config(const RdmaConfig& config) {
        _ib_devname = config.ib_devname;
        _gid_index = config.gid_index;
        _ib_port = config.ib_port;
        _traffic_class = config.traffic_class;
        _sl = config.sl;
        _mtu = config.mtu;
        _pkey_index = config.pkey_index;
        _min_rnr_timer = config.min_rnr_timer;
        _retry_cnt = config.retry_cnt;
        _timeout = config.timeout;
    }
/*
    void from_json_node(const PicoJsonNode& node) { 
        if (!node.at("enable").try_as<bool>(_enabled)) {
            SLOG(FATAL) << "endable required.";
        }
        if (!_enabled) {
            return;
        }
        _enabled = config.enabled;
        if (!node.at("ib_devname").try_as<std::string>(_ib_devname)) {
            SLOG(FATAL) << "ib_devname required.";
        }
        if (!node.at("gid_index").try_as<int>(_gid_index)) {
            SLOG(FATAL) << "gid required.";
        }
       if (!node.at("ib_port").try_as<int>(_ib_port)) {
           SLOG(FATAL) << "ib_port required.";
        }
        if (!node.at("traffic_class").try_as<int>(_traffic_class)) {
            _traffic_class = 4;
            SLOG(WARNING) << "use defualt traffic_class : " << _traffic_class;
        }
        if (!node.at("sl").try_as<int>(_sl)) {
            _sl = 4;
            SLOG(WARNING) << "use defualt sl : " << _sl;
        }
       if (!node.at("mtu").try_as<int>(_mtu)) {
           _mtu = 1024;
            SLOG(WARNING) << "use defualt mtu : " << _mtu;
        }
        if (!node.at("pkey_index").try_as<int>(_pkey_index)) {
            _pkey_index = 0;
            SLOG(WARNING) << "use defualt pkey_index : " << _pkey_index;
        }
        if (!node.at("min_rnr_timer").try_as<int>(_min_rnr_timer)) {
            _min_rnr_timer = 12;
            SLOG(WARNING) << "use defualt min_rnr_timer : " << _min_rnr_timer;
        }
        if (!node.at("retry_cnt").try_as<int>(_retry_cnt)) {
            _retry_cnt = 7;
            SLOG(WARNING) << "use defualt retry_cnt : " << _retry_cnt;
        }
        if (!node.at("timeout").try_as<int>(_timeout)) {
            _timeout = 12;
            SLOG(WARNING) << "use defualt timeout : " << _timeout;
        }
    }
*/
    void initialize(const RdmaConfig& config) {
        SCHECK(!_initialized);
        //PicoJsonNode node;
        //node.load(FLAGS_rdma_config_str);
        from_rdma_config(config); 
        SLOG(INFO) << "rdma enabled.";
        // 查找rdma device
        int num_of_device;
        struct ibv_device **dev_list;
        struct ibv_device* ib_dev = NULL;
        dev_list = ibv_get_device_list(&num_of_device);
        SCHECK(num_of_device > 0) << "Did not detect devices";
        for (; (ib_dev = *dev_list); ++dev_list) {
            if (!strcmp(ibv_get_device_name(ib_dev), _ib_devname.c_str())) {
                break;
            }
        }
        if (!ib_dev) {
            SLOG(FATAL) << "IB device " << _ib_devname << " not found";
        }
        SLOG(INFO) << "use device name : " << ibv_get_device_name(ib_dev);
        _ib_ctx = ibv_open_device(ib_dev);

        ibv_device_attr attr = query_device();
        _max_reads = attr.max_qp_rd_atom;

        ibv_port_attr port_attr = query_port_attr();
        SCHECK(port_attr.link_layer == IBV_LINK_LAYER_ETHERNET) << "not RoCE.";

        _pd = ibv_alloc_pd(_ib_ctx);
        PSCHECK(_pd) << "Couldn't allocate PD";

        ibv_query_gid(_ib_ctx, _ib_port, _gid_index, &_gid);
        _initialized = true;
    }

    static RdmaContext& singleton() {
        static RdmaContext ins;
        return ins;
    }

    mr_t* get(char* start, size_t len) {
        shared_lock_guard<RWSpinLock> _(_lock);
        return get_nonlock(start, len);
    }

    void insert(char* start, size_t len) {
        lock_guard<RWSpinLock> _(_lock);
        return insert_non_lock(start, len);
    }

    bool remove(char* start, size_t size, char*& r_start, size_t& r_size) {
        lock_guard<RWSpinLock> _(_lock);
        return remove_non_lock(start, size, r_start, r_size);
    }

    void print() {
        SLOG(INFO) << "start print rdma mr num : " << _mp.size();
        for (auto& i : _mp) {
            SLOG(INFO) << (uint64_t)i.second.start << " " << i.second.len / 1e6 << " " << i.second.cnt / 1e6;
        }
        SLOG(INFO) << "end print rdma mr num : " << _mp.size();
    }

private:

    mr_t* get_nonlock(char* start, size_t len) {
        if (!_initialized) {
            return nullptr;
        }
        if (_mp.empty()) {
            return nullptr;
        }
        auto it = _mp.lower_bound(uint64_t(start));
        mr_t* ret = nullptr;
        if (it == _mp.end()) {
            return nullptr;
        } else {
            ret = &it->second;
        }
        SCHECK(ret->start <= start);
        if (start + len <= ret->start + ret->len) {
            return ret;
        } else {
            return nullptr;
        }
    }

    void insert_non_lock(char* start, size_t len) {
        if (!_initialized) {
            return;
        }
        SCHECK(_pd) << "not set rdma protection domin.";
        // XXX 这个CHECK有点重，未来考虑删掉
        //SCHECK(get_nonlock(start, len) == nullptr);
        struct ibv_mr* mr = nullptr;
        mr = ibv_reg_mr(_pd,
              start,
              len,
              IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ
                    | IBV_ACCESS_REMOTE_WRITE);
        PSCHECK(mr) << (void*)_pd << " " << (void*)start << " " << len;
        _mp.insert({uint64_t(start),{start, len, mr, len}});
    }

    bool remove_non_lock(char* start, size_t size, char*& r_start, size_t& r_size) {
        if (!_initialized) {
            return true;
        }
        auto mr = get_nonlock(start, size);
        SCHECK(mr);
        mr->cnt -= size;
        if (mr->cnt == 0) {
            r_start = mr->start;
            r_size = mr->len;
            PSCHECK(ibv_dereg_mr(mr->mr) == 0);
            _mp.erase(uint64_t(mr->start));
            return true;
        }
        return false;
    }

    std::map<uint64_t, mr_t, std::greater<uint64_t>> _mp;
    bool _initialized = false;

    std::string _ib_devname;
    int _max_reads;
    ibv_pd* _pd;
    ibv_context* _ib_ctx;
    int _traffic_class = 4;
    int _gid_index = 5; // 目前靠用户设置吧，他们猜的逻辑没看懂
    ibv_gid _gid;       //由 _gid_index query 出来
    int _sl = 4;
    int _mtu;            // 默认使用 active_mtu，可设置
    int _ib_port = 1;    // 需要设置
    int _pkey_index = 0; // 暂时一直用0，应该问题不大
    int _min_rnr_timer = 12;
    int _retry_cnt = 7;
    int _rnr_retry = 7;
    int _timeout = 12;
    RWSpinLock _lock;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // USE_RDMA
#endif // PARADIGM4_PICO_COMMON_RDMA_CONTEXT_H

