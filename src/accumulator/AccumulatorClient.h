#ifndef PARADIGM4_PICO_CORE_ACCUMULATORCLIENT_H
#define PARADIGM4_PICO_CORE_ACCUMULATORCLIENT_H

#include "RpcService.h"
#include "AccumulatorManager.h"
#include "AccumulatorServer.h"
#include "Aggregator.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*
 * 目前全部发送到第一个创建的server
 */
class AccumulatorClient : public NoncopyableObject {
public:
    static AccumulatorClient& singleton() {
        static AccumulatorClient ac;
        return ac;
    }

    bool has_error(RpcResponse& resp) {
        return resp.error_code() != RpcErrorCodeType::SUCC;
    }

    std::string get_error(RpcResponse& resp);

    /*!
     * \brief get a agg of $name from server
     */
    template<class AGG>
    bool read(const std::string name, AGG& ret) {
        static_assert(std::is_base_of<AggregatorBase, AGG>::value, 
                "not inherit from AggregatorBase");
        std::string type_name;
        if (!AccumulatorManager::singleton().get_typename_by_name(name, type_name)) {
            SLOG(WARNING) << "Get typename of accumulator failed, name=" << name;
            return false;
        }
        return inner_read(name, type_name, &ret);
    }

    /*!
     * \brief get a agg of $name from server
     */
    bool read(const std::string& name, std::unique_ptr<AggregatorBase>& ret);

    /*!
     * \brief merge agg to local map, flag pending aggs to be flush
     */
    template<class AGG>
    bool write(const std::string name, const AGG& agg) {
        static_assert(std::is_base_of<Aggregator<typename AGG::value_type, AGG>, AGG>::value, 
                "not inherit from Aggregator");
        
        size_t id;
        {
            std::lock_guard<std::mutex> lock(_client_mutex);
            auto fnd_iter = _umap_name2id.find(name);
            if (fnd_iter == _umap_name2id.end()) {
                return false;
            }
            id = fnd_iter->second;
        }
        
        std::lock_guard<std::mutex> lock(_write_mutex);
        if (_writer_closed) {
            return false;
        }

        static_cast<AGG*>(
                std::get<0>(_pending_aggs[_pending_aggs_idx][id]).get())->merge_aggregator(agg);
        _pending_aggs_flag[_pending_aggs_idx][id] = true;
        _write_pending_cnt[_pending_aggs_idx]++;
        _send_cond.notify_all();

        return true;
    }

    /*!
     * \brief when agg is not const, write std::move(agg) to map
     */
    template<class AGG>
    bool write(const std::string name, AGG&& agg) {
         static_assert(std::is_base_of<Aggregator<typename AGG::value_type, AGG>, AGG>::value, 
                "not inherit from Aggregator");
        
        size_t id;
        {
            std::lock_guard<std::mutex> lock(_client_mutex);
            auto fnd_iter = _umap_name2id.find(name);
            if (fnd_iter == _umap_name2id.end()) {
                return false;
            }
            id = fnd_iter->second;
        }
        
        std::lock_guard<std::mutex> lock(_write_mutex);
        if (_writer_closed) {
            return false;
        }

        static_cast<AGG*>(std::get<0>(
                    _pending_aggs[_pending_aggs_idx][id]).get())->merge_aggregator(std::move(agg));
        _pending_aggs_flag[_pending_aggs_idx][id] = true;
        _write_pending_cnt[_pending_aggs_idx]++;
        _send_cond.notify_all();

        return true;
    }

    /*!
     * \brief reset agg in server
     */
    template<class AGG>
    bool reset(const std::string name) {
        static_assert(std::is_base_of<Aggregator<typename AGG::value_type, AGG>, AGG>::value, 
                "not inherit from Aggregator");
        // request fmt: C agg_name agg_typename
        // response fmt:
        RpcRequest request;
        std::string type_name;
        if (!AccumulatorManager::singleton().get_typename_by_name(name, type_name)) {
            SLOG(WARNING) << "Get typename of accumulator failed, name=" << name;
            return false;
        }
        request << ACCUMULATOR_SERVICE_API_RESET << name << type_name;
        RpcResponse response = _dealer->sync_rpc_call(std::move(request));
        bool is_succ = !has_error(response);
        if (!is_succ) {
            SLOG(WARNING) << "Reset accumulator failed, name=" << name
                          << ", msg=" << get_error(response);
        }
        return is_succ;
    }

    /*!
     * \brief DO NOT use erase related functions unless you know what is it.
     */
    bool erase(const std::vector<std::string>& names) {
       // request fmt: E names
       // response fmt:
       RpcRequest request;
       request << ACCUMULATOR_SERVICE_API_ERASE << names;
       RpcResponse response = _dealer->sync_rpc_call(std::move(request));
       bool is_succ = !has_error(response);
       if (!is_succ) {
           SLOG(WARNING) << "Erase accumulator failed, msg=" << get_error(response);
       }
       return is_succ;
    }

    bool erase(std::vector<std::string>&& names) {
        return erase(names);
    }

    bool erase_all();

    /*!
     * \brief register acc client to server, init reader and writer, barrier thread to wait all
     *  thread finished initialized
     */
    void initialize(RpcService* rpc);

    /*!
     * \brief finalize writer and reader, diregister client from server, barrier thread to
     *  wait all thread finished finalize
     */
    void finalize();

    /*!
     * \brief wait reader and writer
     */
    void wait_empty();

    /*!
     * \brief add new agg to map, add its ptr to pending vec, and push false to pending flag
     */
    template<class AGG>
    void add_aggregator(const std::string& name) {
        bool is_need_add;
        
        std::lock_guard<std::mutex> lock(_client_mutex);
        auto fnd_iter = _umap_name2id.find(name);
        is_need_add = (fnd_iter == _umap_name2id.end());
        
        if (is_need_add) {
            std::lock_guard<std::mutex> lock(_pending_agg_size_mutex);
            size_t id = _pending_aggs[0].size();
            std::string type_name = AggregatorFactory::singleton().get_type_name<AGG>();
            for (size_t i = 0; i < 2; i++) {
                AggregatorBase* new_agg_ptr = static_cast<AggregatorBase*>(
                        AggregatorFactory::singleton().create(type_name));
                SCHECK(new_agg_ptr != nullptr)
                        << "Create aggregator " + type_name + " failed.";
                new_agg_ptr->init();
                _pending_aggs[i].push_back(std::make_tuple(
                            std::unique_ptr<AggregatorBase>(new_agg_ptr),
                            name,
                            type_name));
                _pending_aggs_flag[i].push_back(false);
            }
            _umap_name2id[name] = id;
        }
    }

private:
    AccumulatorClient();

    void initialize_reader() {
        return;
    }

    void finalize_reader() {
        return;
    }

    void wait_empty_reader() {
        return;
    }

    /*!
     * \brief start a send thread to check pending vec and flush change to server
     */
    void initialize_writer();

    /*!
     * \brief close writer, wait send thread to return
     */
    void finalize_writer();

    /*!
     * \brief wait write pending vec to be empty
     */
    void wait_empty_writer();

    /*!
     * \brief check pending vec, send change to server, while sending, write change to the other
     *  pending aggs vec
     */
    void send_thread_procedure();

    /*!
     * \brief send pending vec to server
     */
    void send2server(size_t idx, Dealer* dealer);
    
    bool inner_read(const std::string& name, const std::string& type_name, AggregatorBase* ret);

private:

    bool _writer_closed;
    bool _started;

    std::mutex _client_mutex;
    std::mutex _write_mutex;
    std::mutex _pending_agg_size_mutex;
    std::condition_variable _send_cond;
    std::thread _send_thread;
    
    RpcService* _rpc = nullptr;
    std::unique_ptr<RpcClient> _rpc_client;
    std::shared_ptr<Dealer> _dealer; // owner is _send_thread

    std::vector<std::tuple<std::unique_ptr<AggregatorBase>, std::string, std::string>> 
            _pending_aggs[2];
    std::vector<bool> _pending_aggs_flag[2];
    std::unordered_map<std::string, size_t> _umap_name2id;
    size_t _write_pending_cnt[2];
    size_t _pending_aggs_idx;
};

}
} // namespace pico
} // namespace paradigm4

#endif
