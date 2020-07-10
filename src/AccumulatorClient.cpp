
#include "AccumulatorClient.h"
#include "AggregatorFactory.h"
#include "Aggregator.h"

namespace paradigm4 {
namespace pico {
namespace core {

std::string AccumulatorClient::get_error(RpcResponse& resp) {
    std::string ret = "";
    switch (resp.error_code()) {
    case RpcErrorCodeType::SUCC:
        break;
    case RpcErrorCodeType::ENOTFOUND:
        ret = "Cannot find aggregator on server";
        break;
    default:
        SLOG(FATAL) << "Unknow Rpc ErrorCode: " << int(resp.error_code());
    }
    return ret;
}

bool AccumulatorClient::read(const std::string& name, 
        std::unique_ptr<AggregatorBase>& ret) {
    std::string type_name;
    if (!AccumulatorManager::singleton().get_typename_by_name(name, type_name)) {
        SLOG(WARNING) << "Get typename of accumulator failed, name=" << name;
        return false;
    }
    AggregatorBase* agg = static_cast<AggregatorBase*>(
            AggregatorFactory::singleton().create(type_name));
    if (!inner_read(name, type_name, agg)) {
        return false;
    }
    ret.reset(agg);
    return true;
}

bool AccumulatorClient::erase_all() {
    // request fmt: A
    // response fmt:
    RpcRequest request;
    request.head().sid = 0;
    request << ACCUMULATOR_SERVICE_API_ERASE_ALL;
    RpcResponse response = _dealer->sync_rpc_call(std::move(request));
    bool is_succ = !has_error(response);
    if (!is_succ) {
        SLOG(WARNING) << "Erase all accumulators failed, msg=" << get_error(response);
    }
    return is_succ;
}

void AccumulatorClient::initialize(RpcService* rpc) {
    _rpc = rpc;
    std::lock_guard<std::mutex> lock(_client_mutex);
    _rpc_client = _rpc->create_client(ACCUMULATOR_SERVICE_API, 1);
    _dealer = _rpc_client->create_dealer();
    SCHECK(!_started) << "AccumulatorClient already initialized.";
    RpcRequest request;
    request.head().sid = 0;
    request << ACCUMULATOR_SERVICE_API_START;
    RpcResponse response = _dealer->sync_rpc_call(std::move(request));
    if (has_error(response)) {
        SLOG(FATAL) << "Initialize AccumulatorClient failed on server side, "
                    << get_error(response);
    }
    _started = true;
    initialize_reader();
    initialize_writer();
}

void AccumulatorClient::finalize() {
    std::lock_guard<std::mutex> lock(_client_mutex);
    SCHECK(_started) << "AccumulatorClient already finalized.";
    finalize_writer();
    finalize_reader();
    _started = false;
    RpcRequest request;
    request.head().sid = 0;
    request << ACCUMULATOR_SERVICE_API_STOP;
    RpcResponse response = _dealer->sync_rpc_call(std::move(request));
    SCHECK(!has_error(response))
          << "Finalize AccumulatorClient failed on server side. " << get_error(response);
    _dealer.reset();
    _rpc_client.reset();
}

void AccumulatorClient::wait_empty() {
    std::lock_guard<std::mutex> lock(_client_mutex);
    SCHECK(_started) << "AccumulatorClient does not initialized.";
    wait_empty_reader();
    wait_empty_writer();
}

AccumulatorClient::AccumulatorClient() {
    _writer_closed = true;
    _started = false;
    _pending_aggs_idx = 0;
    for (size_t i = 0; i < 2; i++) {
        _write_pending_cnt[i] = 0;
        _pending_aggs[i].reserve(1024);
        _pending_aggs[i].clear();
        _pending_aggs_flag[i].reserve(1024);
        _pending_aggs_flag[i].clear();
    }
}

void AccumulatorClient::initialize_writer() {
    std::lock_guard<std::mutex> lock(_write_mutex);
    if (!_writer_closed) {
        return;
    }
    _writer_closed = false;
    _send_cond.notify_all();
    _send_thread = std::thread(&AccumulatorClient::send_thread_procedure, this);
}

void AccumulatorClient::finalize_writer() {
    {
        std::lock_guard<std::mutex> lock(_write_mutex);
        if (_writer_closed) {
            return;
        }
        _writer_closed = true;
        _send_cond.notify_all();
    }

    if (_send_thread.joinable()) {
        _send_thread.join();
    }
}

void AccumulatorClient::wait_empty_writer() {
    std::unique_lock<std::mutex> lock(_write_mutex);
    _send_cond.wait(lock, [this]() {
        return _writer_closed || (_write_pending_cnt[0] == 0 && _write_pending_cnt[1] == 0);
    });
}

void AccumulatorClient::send_thread_procedure() {
    auto dealer = _rpc_client->create_dealer();
    while (true) {
        std::unique_lock<std::mutex> lock(_write_mutex);
        _send_cond.wait(lock, [this]() {
            return _write_pending_cnt[_pending_aggs_idx] > 0 || _writer_closed;
        });

        if (_write_pending_cnt[_pending_aggs_idx] > 0) {
            size_t aggs_idx2send = _pending_aggs_idx;
            _pending_aggs_idx = 1 - _pending_aggs_idx;
            lock.unlock();
            send2server(aggs_idx2send, dealer.get());
            lock.lock();
            _write_pending_cnt[aggs_idx2send] = 0;
            _send_cond.notify_all();
        }

        if (_writer_closed) {
            if (_write_pending_cnt[_pending_aggs_idx] > 0) {
                send2server(_pending_aggs_idx, dealer.get());
                _write_pending_cnt[_pending_aggs_idx] = 0;
                _send_cond.notify_all();
            }
            break;
        }
    }
    dealer.reset();
}

void AccumulatorClient::send2server(size_t idx, Dealer* dealer) {
    // request fmt: W agg_cnt (agg_name agg_typename agg_serialize) ...
    // response fmt:
    RpcRequest request;
    request.head().sid = 0;
    {
        std::lock_guard<std::mutex> lock(_pending_agg_size_mutex);
        size_t sending_cnt = 0;
        for (size_t i = 0; i < _pending_aggs[idx].size(); i++) {
            if (_pending_aggs_flag[idx][i]) {
                sending_cnt++;
            }
        }
        if (0 == sending_cnt) {
            return;
        }
        request << ACCUMULATOR_SERVICE_API_WRITE << sending_cnt;
        for (size_t i = 0; i < _pending_aggs[idx].size(); i++) {
            if (!_pending_aggs_flag[idx][i]) {
                continue;
            }
            AggregatorBase* agg = std::get<0>(_pending_aggs[idx][i]).get();
            std::string name = std::get<1>(_pending_aggs[idx][i]);
            std::string type_name = std::get<2>(_pending_aggs[idx][i]);
            request << name << type_name;
            agg->serialize(request.archive());
            agg->init();
            _pending_aggs_flag[idx][i] = false;
        }
    }
    RpcResponse response = dealer->sync_rpc_call(std::move(request));
    bool is_succ = !has_error(response);
    SCHECK(is_succ) << "Send accumulator to server failed, msg=" << get_error(response);
}

bool AccumulatorClient::inner_read(const std::string& name, 
        const std::string& type_name, AggregatorBase* ret) {
    // request fmt: R agg_name agg_typename
    // response fmt: agg_serialize
    SCHECK(ret != nullptr);
    RpcRequest request;
    request.head().sid = 0;
    request << ACCUMULATOR_SERVICE_API_READ << name << type_name;
    RpcResponse response = _dealer->sync_rpc_call(std::move(request));
    bool is_succ = !has_error(response);
    if (!is_succ) {
        SLOG(WARNING) << "Read accumulator failed, name=" << name
                    << ", msg=" << get_error(response);
        return false;
    }
    ret->init();
    ret->deserialize(response.archive());
    SCHECK(response.archive().is_exhausted());

    return true;
}

}
} // namespace pico
} // namespace paradigm4
