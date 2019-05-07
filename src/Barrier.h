#ifndef PARADIGM4_PICO_COMMON_BARRIER_H
#define PARADIGM4_PICO_COMMON_BARRIER_H

#include <thread>

#include "Channel.h"
#include "PicoLearnerContext.h"
#include "RpcService.h"
#include "defs.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {

/*!
 * \brief barrier entity
 * \param total_count  total num of barrier need to be exec
 * \param local_count_avail_num  num of container already run barrier
 * \param local_count_sum total num of time barrier has been exec
 */
struct barrier_item_t {
    size_t total_count = 0;
    int32_t local_count_avail_num = 0;
    std::vector<size_t> local_counts;
    size_t local_count_sum = 0;
    std::vector<RpcRequest> requests;

    barrier_item_t() {
        int32_t comm_size = pico_comm_size();
        total_count = 0;
        local_count_avail_num = 0;
        local_counts.resize(comm_size, 0);
        local_count_sum = 0;
        requests.reserve(comm_size);
    }
};

struct count_item_t {
    size_t total_count = 0;
    size_t local_count_sum = 0;
    std::vector<RpcRequest> requests;
};

struct reduce_item_t {
    size_t total_count = 0;
    size_t local_count_sum = 0;
    std::vector<RpcRequest> requests;
    BinaryArchive data_sum;
};

enum BarrierType : int8_t {
    BARRIER = 0,
    COUNT   = 1,
    AGGREGATE = 2
};
PICO_ENUM_SERIALIZATION(BarrierType, int8_t);

/*!
 * \brief class Barrier store all barriers as server, it initialized when server
 */
class Barrier {
public:
    static Barrier& singleton() {
        static Barrier barrier;
        return barrier;
    }

    /*!
     * \brief if current node is barrier root, start listening thread, send msg to other nodes
     *  if cur node is not root, get default request of it and response
     */
    void initialize() {
        _comm_size = pico_comm_size();
        _comm_rank = pico_comm_rank();
        if (_comm_rank == BARRIER_ROOT) {
            _rpc_server = pico_rpc_service().create_server(BARRIER_SERVICE_API);
            _server_dealer = _rpc_server->create_dealer();
            _server_thread = std::thread(&Barrier::listening, this);
        }
        MasterClient* client = pico_master_client();
        //client->barrier(BARRIER_SERVICE_API, pico_comm_size());
        //pico_rpc_service().update_ctx();
        _rpc_client = pico_rpc_service().create_client(BARRIER_SERVICE_API, 1);
        _client_dealer = _rpc_client->create_dealer();
        client->barrier(BARRIER_SERVICE_API, pico_comm_size());
    }

    /*!
     * \brief close root barrier channel, return when server listening return
     */
    void finalize() {
        if (_comm_rank == BARRIER_ROOT) {
            _server_dealer->terminate();
            _server_thread.join();
            _server_dealer.reset();
            _rpc_server.reset();
        }
        _client_dealer.reset();
        _rpc_client.reset();
    }

    /*!
     * \brief send request to barrier root node, after all node send request, root node
     *  response and wait() func continue to run next line of code
     */
    void wait(const std::string& barrier_name, size_t count = 1u) {
        if (_comm_size == 1 && count == 1u) {
            return;
        }
        RpcRequest request;
        request << BarrierType::BARRIER << barrier_name << _comm_rank << count;
        RpcResponse resp;
        _client_dealer->send_request(std::move(request));
        _client_dealer->recv_response(resp);
        std::string name;
        resp >> name;
        SCHECK(name == barrier_name) << "name:" << name << " barrier_name:" << barrier_name;
    }

    void count(const std::string& barrier_name, size_t tot_count, size_t count) {
        if (count == tot_count) return;
        RpcRequest request;
        request << BarrierType::COUNT << barrier_name << _comm_rank << tot_count << count;
        RpcResponse resp;
        std::string name;
        size_t local_count = 0;
        resp >> name >> local_count;
        SCHECK(name == barrier_name) << "name:" << name << " barrier_name:" << barrier_name;
    }

    void register_aggregate_function(
            const std::string& name,
            std::function<void(BinaryArchive&, BinaryArchive&)> func) {
        _reduce_func[name] = func;
    }

    void aggregate(const std::string& barrier_name,
                   size_t tot_count, size_t count,
                   const BinaryArchive& data_in,
                   BinaryArchive& data_out,
                   const std::string& func_name) {
        if (count == tot_count) {
            data_out = data_in;
            return;
        }
        RpcRequest request(BARRIER_ROOT);
        request << BarrierType::AGGREGATE << barrier_name << _comm_rank
            << tot_count << count << data_in << func_name;
        auto resp = _client_dealer->sync_rpc_call(std::move(request));
        std::string name;
        resp >> name >> data_out;
        SCHECK(name == barrier_name) << "name:" << name << " barrier_name:" << barrier_name;
    }

private:
    Barrier() = default;

    /*!
     * \brief barrier server func.
     *  continue to get barrier name, rank and count from request, register barrier name if
     *  not exist, calc all kind of count, when count equal to limit, deregister barrier
     *  name from map
     */
    void process_barrier_request(Dealer& dealer, RpcRequest& request) {
        std::string barrier_name;
        comm_rank_t rank = 0;
        size_t count = 0;
        request >> barrier_name >> rank >> count;
        SCHECK(rank < _comm_size)
              << "rank " << rank << " exceeds comm size " << _comm_size;
        auto it = _barrier_map.find(barrier_name);
        if (it == _barrier_map.end()) {
            barrier_item_t item;
            auto p = _barrier_map.emplace(std::move(barrier_name), std::move(item));
            SCHECK(p.second);
            it = p.first;
        }

        barrier_item_t& item = it->second;
        if (item.local_counts[rank] == 0) {
            item.total_count += count;
            item.local_count_avail_num++;
            item.local_counts[rank] = count;
        } else {
            SCHECK(count == item.local_counts[rank]);
        }

        item.local_count_sum++;
        item.requests.push_back(std::move(request));
        SLOG(INFO) << item.local_count_sum;
        if (try_bcast_ack_barrier(item, it->first, dealer)) {
            _barrier_map.erase(it);
        }
    }

    void process_count_request(Dealer& dealer, RpcRequest& request) {
        std::string barrier_name;
        comm_rank_t rank = 0;
        size_t tot_count = 0, count = 0;
        request >> barrier_name >> rank >> tot_count >> count;
        SCHECK(rank < _comm_size)
              << "rank " << rank << " exceeds comm size " << _comm_size;
        auto it = _count_map.find(barrier_name);
        if (it == _count_map.end()) {
            count_item_t item;
            auto p = _count_map.emplace(std::move(barrier_name), std::move(item));
            SCHECK(p.second);
            it = p.first;
        }

        count_item_t& item = it->second;
        if (item.total_count == 0)
            item.total_count = tot_count;
        else
            SCHECK(item.total_count == tot_count);
        item.local_count_sum += count;

        item.requests.push_back(std::move(request));
        if (try_bcast_ack_count(item, it->first, dealer)) {
            _count_map.erase(it);
        }
    }

    void process_aggregate_request(Dealer& dealer, RpcRequest& request) {
        std::string barrier_name;
        comm_rank_t rank = 0;
        size_t tot_count = 0, count = 0;
        request >> barrier_name >> rank >> tot_count >> count;
        SCHECK(rank < _comm_size)
              << "rank " << rank << " exceeds comm size " << _comm_size;
        auto it = _reduce_map.find(barrier_name);
        bool is_new_name = false;
        if (it == _reduce_map.end()) {
            reduce_item_t item;
            auto p = _reduce_map.emplace(std::move(barrier_name), std::move(item));
            SCHECK(p.second);
            it = p.first;
            is_new_name = true;
        }

        reduce_item_t& item = it->second;
        if (item.total_count == 0)
            item.total_count = tot_count;
        else
            SCHECK(item.total_count == tot_count);
        item.local_count_sum += count;

        BinaryArchive data;
        std::string func_name;
        request >> data >> func_name;
        if (is_new_name) item.data_sum = data;
        else {
            SCHECK(_reduce_func.find(func_name) != _reduce_func.end());
            _reduce_func[func_name](data, item.data_sum);
        }

        item.requests.push_back(std::move(request));
        if (try_bcast_ack_aggregate(item, it->first, dealer)) {
            _reduce_map.erase(it);
        }
    }

    void listening() {
        RpcRequest request;
        BarrierType btype = BarrierType::BARRIER;
        while (_server_dealer->recv_request(request)) {
            request >> btype;
            switch (btype) {
                case BarrierType::BARRIER:
                    process_barrier_request(*_server_dealer, request);
                    break;
                case BarrierType::COUNT:
                    process_count_request(*_server_dealer, request);
                    break;
                case BarrierType::AGGREGATE:
                    process_aggregate_request(*_server_dealer, request);
                    break;
                default:
                    SLOG(FATAL) << "unknown barrier type " << btype;
            }
            SCHECK(request.archive().is_exhausted());
        }
        //dealer->finalize();
    }

    /*!
     * \brief check if count equals to limit, if true, means all thread have run barrier and
     *  waiting for response, send response to all thread
     */
    bool try_bcast_ack_barrier(const barrier_item_t& item,
            const std::string& name,  Dealer& dealer) {
        if ((item.local_count_avail_num == _comm_size)
              && (item.total_count == item.local_count_sum)) {
            for (auto& request : item.requests) {
                RpcResponse response(request);
                response << name;
                dealer.send_response(std::move(response));
            }
            return true;
        }
        return false;
    }

    bool try_bcast_ack_count(const count_item_t& item,
          const std::string& name,
          Dealer& dealer) {
        if (item.total_count == item.local_count_sum) {
            for (auto& request : item.requests) {
                RpcResponse response(request);
                response << name;
                dealer.send_response(std::move(response));
            }
            return true;
        }
        return false;
    }

    bool try_bcast_ack_aggregate(const reduce_item_t& item,
            const std::string& name, Dealer& dealer) {
        if (item.total_count == item.local_count_sum) {
            for (auto& request : item.requests) {
                RpcResponse response(request);
                response << name << item.data_sum;
                dealer.send_response(std::move(response));
            }
            return true;
        }
        return false;
    }

private:
    comm_rank_t _comm_rank = 0;
    comm_rank_t _comm_size = 0;
    std::unordered_map<std::string, barrier_item_t> _barrier_map;
    std::unordered_map<std::string, count_item_t> _count_map;
    std::unordered_map<std::string, reduce_item_t> _reduce_map;
    std::unordered_map<std::string,
        std::function<void(BinaryArchive& in, BinaryArchive& out)>> _reduce_func;
    Channel<RpcRequest> _request_chan;
    std::shared_ptr<Dealer> _server_dealer, _client_dealer;
    std::unique_ptr<RpcServer> _rpc_server;
    std::unique_ptr<RpcClient> _rpc_client;
    std::thread _server_thread;
};

/*!
 * \brief wait barrier with $name get all threads' request and send response
 */
inline void pico_barrier(const std::string& name, size_t count = 1) {
    Barrier::singleton().wait(name, count);
}

inline void pico_barrier_count(const std::string& name, size_t tot_count, size_t count) {
    Barrier::singleton().count(name, tot_count, count);
}

inline void pico_barrier_aggregate(
        const std::string& name,
        size_t tot_count,
        size_t count,
        const BinaryArchive& data_in,
        BinaryArchive& data_out,
        const std::string& func_name) {
    Barrier::singleton().aggregate(
            name, tot_count, count, data_in, data_out, func_name);
}

inline void pico_register_barrier_aggregate_function(
        const std::string& name,
        std::function<void(BinaryArchive&, BinaryArchive&)> func) {
    Barrier::singleton().register_aggregate_function(name, func);
}

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_BARRIER_H
