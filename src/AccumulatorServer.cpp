#include "AccumulatorServer.h"
#include "pico_http.h"
#include "pico_log.h"
#include "Monitor.h"
#include "FileSystem.h"

namespace paradigm4 {
namespace pico {
namespace core {

void AccumulatorServer::initialize(core::RpcService* rpc) {
    _rpc = rpc;
    SCHECK(!_started) << "AccumulatorServer already initialized.";
    std::lock_guard<std::mutex> lock(_mutex);
    _rpc_server = _rpc->create_server(ACCUMULATOR_SERVICE_API);
    _dealer = _rpc_server->create_dealer();
    _process_request_thread = std::thread(&AccumulatorServer::process_request, this);
    _started = true;
}

void AccumulatorServer::finalize() {
    SCHECK(_started) << "AccumulatorServer already finalized.";
    std::unique_lock<std::mutex> lock(_mutex);
    _started = false;
    lock.unlock();
    _dealer->terminate();
    if (_process_request_thread.joinable()) {
        _process_request_thread.join();
    }
    _dealer.reset();
    _rpc_server.reset();
}

void AccumulatorServer::wait_empty() {
    if (!_started) {
        return;
    }
    RpcRequest request;
    request << ACCUMULATOR_SERVICE_API_WAIT_EMPTY;
    _rpc->create_client(ACCUMULATOR_SERVICE_API)
          ->create_dealer()
          ->sync_rpc_call(std::move(request));
}

std::vector<std::pair<std::string, std::string>> AccumulatorServer::generate_output_info() {
    std::vector<std::pair<std::string, std::string>> output_info;
    {
        std::unique_lock<std::mutex> mlock(_mutex);
        std::for_each(_umap_name2aggs.begin(),
              _umap_name2aggs.end(),
              [&](decltype(_umap_name2aggs)::value_type& item) {
                  auto key = item.first;
                  auto value = item.second.second.get();
                  std::string val_str;
                  SCHECK(value != nullptr);
                  ;
                  if (!value->try_to_string(val_str)) {
                      val_str = "N/A";
                  }
                  output_info.push_back({std::move(key), std::move(val_str)});
              });
    }
    std::sort(output_info.begin(),
          output_info.end(),
          [](std::pair<std::string, std::string> a, std::pair<std::string, std::string> b) {
              return a.first < b.first;
          });
    return output_info;
}

std::string AccumulatorServer::generate_json_str(
      const std::vector<std::pair<std::string, std::string>>& output_info,
      bool pretty) {
    PicoJsonNode node;
    for (auto item : output_info) {
        node.add(item.first, item.second);
    }
    PicoJsonNode root;
    root.add("PicoAccumulator", node);
    std::string json_str;
    SCHECK(root.save(json_str, pretty));
    return json_str;
}

PicoJsonNode AccumulatorServer::generate_pico_json_node(
      const std::vector<std::pair<std::string, std::string>>& output_info) {
    PicoJsonNode node;
    for (auto item : output_info) {
        node.add(item.first, item.second);
    }
    return node;
}

AccumulatorServer::AccumulatorServer() {
    _started = false;
    _client_count = 0;
}

void AccumulatorServer::process_request() {
    RpcRequest request;
    char op = '\0';
    std::vector<RpcRequest> wait_empty_reqs;
    for (;;) {
        while (_dealer->recv_request(request, wait_empty_reqs.empty() ? -1 : 0)) {
            request >> op;
            std::lock_guard<std::mutex> lock(_mutex);
            switch (op) {
            case ACCUMULATOR_SERVICE_API_READ:
                process_read_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_WRITE:
                process_write_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_RESET:
                process_reset_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_ERASE:
                process_erase_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_ERASE_ALL:
                process_erase_all_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_START:
                process_start_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_STOP:
                process_stop_request(request);
                break;
            case ACCUMULATOR_SERVICE_API_WAIT_EMPTY:
                wait_empty_reqs.push_back(std::move(request));
                break;
            }
            SCHECK(request.archive().is_exhausted());
        }
        if (wait_empty_reqs.empty()) {
            break;
        } else {
            for (auto& req : wait_empty_reqs) {
                _dealer->send_response(RpcResponse(req));
            }
            wait_empty_reqs.clear();
        }
    }
}

void AccumulatorServer::process_read_request(RpcRequest& request) {
    std::string name;
    std::string type_name;
    request >> name >> type_name;

    auto fnd_iter = _umap_name2aggs.find(name);
    if (fnd_iter == _umap_name2aggs.end()) {
        RpcResponse response(request);
        response.set_error_code(RpcErrorCodeType::ENOTFOUND);
        _dealer->send_response(std::move(response));
        return;
    }

    SCHECK(type_name == fnd_iter->second.first)
          << "Aggregator type mismatch, name=" << name
          << ", expect type=" << fnd_iter->second.first << ", real type=" << type_name;
    SCHECK(fnd_iter->second.second.get() != nullptr) << "Aggregator is null, name=" << name;

    RpcResponse response(request);
    fnd_iter->second.second.get()->serialize(response.archive());
    _dealer->send_response(std::move(response));
}

void AccumulatorServer::process_reset_request(RpcRequest& request) {
    std::string name;
    std::string type_name;
    request >> name >> type_name;

    auto fnd_iter = _umap_name2aggs.find(name);
    if (fnd_iter == _umap_name2aggs.end()) {
        RpcResponse response(request);
        response.set_error_code(RpcErrorCodeType::ENOTFOUND);
        _dealer->send_response(std::move(response));
        return;
    }

    SCHECK(type_name == fnd_iter->second.first)
          << "Aggregator type mismatch, name=" << name
          << ", expect type=" << fnd_iter->second.first << ", real type=" << type_name;
    SCHECK(fnd_iter->second.second.get() != nullptr) << "Aggregator is null, name=" << name;

    RpcResponse response(request);
    fnd_iter->second.second.get()->init();
    _dealer->send_response(std::move(response));
}

void AccumulatorServer::process_write_request(RpcRequest& request) {
    size_t count;
    std::string name;
    std::string type_name;
    request >> count;
    for (size_t i = 0; i < count; i++) {
        request >> name >> type_name;
        auto fnd_iter = _umap_name2aggs.find(name);
        if (fnd_iter == _umap_name2aggs.end()) {
            AggregatorBase* new_agg_ptr = static_cast<AggregatorBase*>(
                  AggregatorFactory::singleton().create(type_name));
            SCHECK(new_agg_ptr != nullptr) << "Cannot create aggregator, type=" << type_name;
            new_agg_ptr->init();
            new_agg_ptr->deserialize(request.archive());
            _umap_name2aggs[name]
                  = std::make_pair(type_name, std::unique_ptr<AggregatorBase>(new_agg_ptr));
            // LOG(INFO) << "Create aggregator, name=" << name << ", type=" << type_name;
        } else {
            AggregatorBase* delta_agg_ptr = get_agg_from_pool(type_name);
            delta_agg_ptr->init();
            delta_agg_ptr->deserialize(request.archive());
            SCHECK(type_name == fnd_iter->second.first)
                  << "Aggregator type mismatch, name=" << name
                  << ", expect type=" << fnd_iter->second.first << ", real type=" << type_name;
            SCHECK(fnd_iter->second.second.get() != nullptr)
                  << "Aggregator is null, name=" << name;
            fnd_iter->second.second.get()->merge_aggregator(delta_agg_ptr);
            // LOG(INFO) << "Merge aggregator, name=" << name << ", type=" << type_name;
        }
    }
    RpcResponse response(request);
    _dealer->send_response(std::move(response));
}

void AccumulatorServer::process_erase_request(RpcRequest& request) {
    std::vector<std::string> names;
    request >> names;
    for (size_t i = 0; i < names.size(); i++) {
        _umap_name2aggs.erase(names[i]);
    }
    RpcResponse response(request);
    _dealer->send_response(std::move(response));
}

void AccumulatorServer::process_erase_all_request(RpcRequest& request) {
    _umap_name2aggs.clear();
    RpcResponse response(request);
    _dealer->send_response(std::move(response));
}

void AccumulatorServer::process_start_request(RpcRequest& request) {
    //comm_rank_t client_id;
    //request >> client_id;
    ++_client_count;
    RpcResponse response(request);
    _dealer->send_response(std::move(response));
    // LOG(INFO) << "AccumulatorClient started, client_id=" << client_id;
}

void AccumulatorServer::process_stop_request(RpcRequest& request) {
    //comm_rank_t client_id;
    //request >> client_id;
    SCHECK(_client_count > 0) << "No client exists while stopping";
    --_client_count;
    RpcResponse response(request);
    _dealer->send_response(std::move(response));
}

AggregatorBase* AccumulatorServer::get_agg_from_pool(const std::string type_name) {
    auto fnd_iter = _agg_pool.find(type_name);
    if (fnd_iter == _agg_pool.end()) {
        AggregatorBase* agg
              = static_cast<AggregatorBase*>(AggregatorFactory::singleton().create(type_name));
        SCHECK(agg != nullptr) << "Cannot create aggregator, type=" << type_name;
        auto ret = _agg_pool.insert(
              std::make_pair(type_name, std::unique_ptr<AggregatorBase>(agg)));
        fnd_iter = ret.first;
    }
    return fnd_iter->second.get();
}

}
} // namespace pico
} // namespace paradigm4
