#ifndef PARADIGM4_PICO_COMMON_RPC_MESSAGE_H
#define PARADIGM4_PICO_COMMON_RPC_MESSAGE_H

#include "ThreadedTimer.h"
#include "Archive.h"
#include "LazyArchive.h"
#include "common.h"
#include "RdmaContext.h"

namespace paradigm4 {
namespace pico {

typedef int16_t RpcErrorCodeType;

/*!
 * \brief rpc message info head,
 * must be trival type
 */
struct rpc_head_t {
    uint32_t body_size;
    comm_rank_t src_rank = -1;
    comm_rank_t dest_rank = -1;
    int32_t src_dealer = -1;
    int32_t dest_dealer = -1;
    int32_t rpc_id = -1;
    int32_t sid = -1;
    uint32_t extra_block_count = 0;
    uint32_t extra_block_length = 0;
    RpcErrorCodeType err_no = 0;
    int32_t error_code = 0;

    size_t msg_size() {
        return sizeof(rpc_head_t) + extra_block_length + body_size;
    }

    friend std::ostream& operator<< (std::ostream& stream, const rpc_head_t& head) {
        stream << "rpc_head[src(rank:dealer):(" << head.src_rank << ":" << head.src_dealer
               << "), dest(rank:dealer):(" << head.dest_rank << ":" << head.dest_dealer
               << "), rpc_id:" << head.rpc_id
               << ", size:" << head.body_size << "]";
        return stream;
    }
};

class RpcRequest;
class RpcResponse;

//static const size_t MAX_BLOCK_ALIGN = 64;
// 小于8k的消息依然copy，否则zero copy
static const size_t MIN_ZERO_COPY_SIZE = 8 * 1024;

class RpcMessage {
public:
    RpcMessage() = default;


    RpcMessage(RpcMessage&& o) {
        *this = std::move(o);
    }

    RpcMessage& operator=(RpcMessage&& o) = default;
       
    RpcMessage(const RpcMessage&) = delete;
    RpcMessage& operator=(const RpcMessage&) = delete;

    // recv时的构造,只有proxy线程调用
    // 此时msg并不能使用
    // head | archive | block_size array
    RpcMessage(char* start, const std::shared_ptr<char>& buffer)
        : _start(start), _buffer(buffer) {
        _data.reserve(head()->extra_block_count);
        char* cur = extra();
        data_block_t* cur_lazy_meta = lazy_meta();
        for (size_t i = 0; i < head()->extra_block_count; ++i) {
            auto len = cur_lazy_meta[i].length;
            if (len < MIN_ZERO_COPY_SIZE) {
                _data.emplace_back(cur, len);
                cur += len;
            } else {
                ++_pending_block_cnt;
                _data.emplace_back(len);
            }
        }
    }

    RpcMessage(RpcRequest&&);
    RpcMessage(RpcResponse&&);

    RpcMessage(const rpc_head_t& h) {
        BinaryArchive ar;
        ar.write_raw(&h, sizeof(h));
        _start = ar.buffer();
        _buffer = ar.release_shared();
    }

    rpc_head_t* head() {
        return reinterpret_cast<rpc_head_t*>(_start);
    }

    data_block_t* lazy_meta() {
        return reinterpret_cast<data_block_t*>(
              _start + sizeof(rpc_head_t) + head()->body_size);
    }

    char* extra() {
        return _start + sizeof(rpc_head_t) + head()->body_size
               + head()->extra_block_count * sizeof(data_block_t);
    }

    /*
     * 必须先调完send，再调zero copy，
     * 否则会导致zero copy的block到了，msg没到
     * 对端无法接收
     */
    template <class F, class F2>
    bool send(bool more, F send, F2 send_zero_copy) {
        size_t n = _data.size();
        if (!send(_start,
                  sizeof(rpc_head_t) + head()->body_size,
                  !_data.empty() || more)) {
            return false;
        }
        if (!_data.empty()) {
            static thread_local std::vector<std::pair<char*, size_t>> tmp1, tmp2;
            tmp1.clear();
            tmp2.clear();

            for (size_t i = 0; i < n; ++i) {
                if (_data[i].length < MIN_ZERO_COPY_SIZE) {
                    if (_data[i].length) {
                        tmp1.emplace_back(_data[i].data, _data[i].length);
                    }
                } else {
                    tmp2.emplace_back(_data[i].data, _data[i].length);
                }
            }
            char* ptr = reinterpret_cast<char*>(_data.data());
            if (!send(ptr,
                      _data.size() * sizeof(_data[0]),
                      !tmp1.empty() || more)) {
                return false;
            }

            for (size_t i = 0; i < tmp1.size(); ++i) {
                if (!send(tmp1[i].first,
                      tmp1[i].second,
                      (i + 1 < tmp1.size()) || more)) {
                    return false;
                }
            }

            for (size_t i = 0; i < tmp2.size(); ++i) {
                if (!send_zero_copy(tmp2[i].first,
                      tmp2[i].second,
                      (i + 1 < tmp2.size()) || more)) {
                    return false;
                }
            }
        }
        return true;
    }

    friend std::ostream& operator<< (std::ostream& stream, const RpcMessage& msg) {
        stream << *((RpcMessage&)msg).head();
        return stream;
    }

    friend RpcRequest;
    friend RpcResponse;
    void initialize(rpc_head_t&& head, BinaryArchive&& ar, LazyArchive&& lazy);
    void finalize(rpc_head_t& head, BinaryArchive& ar, LazyArchive& lazy);

    char* _start = nullptr;
    std::shared_ptr<char> _buffer;

    pico::vector<data_block_t> _data;
    std::unique_ptr<LazyArchive> _hold;
    int _pending_block_cnt = 0;
};

class RpcRequest {
public:
    friend RpcMessage;
    typedef BinaryArchive::allocator_type allocator_type;
    RpcRequest() {
        _ar.resize(sizeof(_head));
        _ar.set_cursor(_ar.end());
    }

    RpcRequest(const RpcRequest&) = delete;

    RpcRequest& operator=(const RpcRequest&) = delete;

    RpcRequest(comm_rank_t dest_rank) {
        _head.dest_rank = dest_rank;
        _head.dest_dealer = -1;
        _ar.resize(sizeof(_head));
        _ar.set_cursor(_ar.end());
    }

    RpcRequest(RpcMessage&& msg) {
        msg.finalize(_head, _ar, _lazy);
        _msg = std::make_unique<RpcMessage>(std::move(msg));
    }

    RpcRequest(RpcRequest&& req) {
        *this = std::move(req);
    }

    RpcRequest& operator=(RpcRequest&& req) {
        _head = std::move(req._head);
        if (_msg) {
            _ar.release();
        }
        _msg = std::move(req._msg);
        _ar = std::move(req._ar);
        _lazy = std::move(req._lazy);
        return *this;
    }

    ~RpcRequest() {
        if (_msg) {
            _ar.release();
        }
    }

    BinaryArchive& archive() {
        return _ar;
    }

    LazyArchive& lazy() {
        return _lazy;
    }

    rpc_head_t& head() {
        return _head;
    }

    const rpc_head_t& head() const {
        return _head;
    }

    template <class T>
    RpcRequest& operator>>(T& val) {
        _ar >> val;
        return *this;
    }

    template <class T>
    RpcRequest& operator<<(const T& val) {
        _ar << val;
        return *this;
    }

private:
    rpc_head_t _head;
    BinaryArchive _ar;
    LazyArchive _lazy;
    std::unique_ptr<RpcMessage> _msg = nullptr;
};

class RpcResponse {
public:
    friend RpcMessage;
    RpcResponse() = default;

    RpcResponse(const RpcRequest& req) {
        _head.dest_rank = req.head().src_rank;
        _head.dest_dealer = req.head().src_dealer;
        _head.rpc_id = req.head().rpc_id;
        _ar.resize(sizeof(_head));
        _ar.set_cursor(_ar.end());
    }

    RpcResponse(const RpcResponse&) = delete;

    RpcResponse& operator=(const RpcResponse&) = delete;

    RpcResponse(RpcMessage&& msg) {
        msg.finalize(_head, _ar, _lazy);
        _msg = std::make_unique<RpcMessage>(std::move(msg));
    }

    RpcResponse(RpcResponse&& resp) {
        *this = std::move(resp);
    }

    RpcResponse& operator=(RpcResponse&& resp) {
        _head = std::move(resp._head);
        if (_msg) {
            _ar.release();
        }
        _msg = std::move(resp._msg);
        _ar = std::move(resp._ar);
        _lazy = std::move(resp._lazy);
        return *this;
    }

    ~RpcResponse() {
        if (_msg != nullptr) {
            _ar.release();
        }
    }

    BinaryArchive& archive() {
        return _ar;
    }
    LazyArchive& lazy() {
        return _lazy;
    }

    rpc_head_t& head() {
        return _head;
    }

    const rpc_head_t& head() const {
        return _head;
    }

    template <class T>
    RpcResponse& operator>>(T& val) {
        _ar >> val;
        return *this;
    }

    template <class T>
    RpcResponse& operator<<(const T& val) {
        _ar << val;
        return *this;
    }

    void set_error_code(RpcErrorCodeType err) {
        _head.error_code = err;
    }

    RpcErrorCodeType error_code() {
        return _head.error_code;
    }

private:
    rpc_head_t _head;
    BinaryArchive _ar;
    LazyArchive _lazy;
    std::unique_ptr<RpcMessage> _msg = nullptr;
};

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_RPC_MESSAGE_H
