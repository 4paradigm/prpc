#ifndef PARADIGM4_PICO_CORE_RPC_MESSAGE_H
#define PARADIGM4_PICO_CORE_RPC_MESSAGE_H

#include "Archive.h"
#include "LazyArchive.h"
#include "common.h"
#include "RdmaContext.h"

namespace paradigm4 {
namespace pico {
namespace core {

//typedef int16_t RpcErrorCodeType;

enum RpcErrorCodeType:int16_t {
    SUCC,
    ENOSUCHSERVER = 101,
    ENOSUCHRANK,
    ENOSUCHSERVICE,
    ELOGICERROR,
    EILLEGALMSG,
    ETIMEOUT,
    ENOTFOUND,
    ECONNECTION
};

/*!
 * \brief rpc message info head,
 * must be trival type
 */
struct rpc_head_t {
    uint32_t body_size = 0;
    comm_rank_t src_rank = -1;
    comm_rank_t dest_rank = -1;
    int32_t src_dealer = -1;
    int32_t dest_dealer = -1;
    int32_t rpc_id = -1;
    int32_t sid = -1;
    uint32_t extra_block_count = 0;
    uint32_t extra_block_length = 0;
    RpcErrorCodeType error_code = SUCC;

    size_t msg_size() {
        return sizeof(rpc_head_t) + extra_block_length + body_size;
    }

    friend std::ostream& operator<< (std::ostream& stream, const rpc_head_t& head) {
        stream << "rpc_head[src(rank:dealer):(" << head.src_rank << ":" << head.src_dealer
               << "), dest(rank:dealer):(" << head.dest_rank << ":" << head.dest_dealer
               << "), rpc_id:" << head.rpc_id 
               << ", sid:" << head.sid
               << ", extra_block(count:length):(" << head.extra_block_count 
               << ":" << head.extra_block_length << ")"
               << ", size:" << head.body_size << "]";
        return stream;
    }
};

class RpcRequest;
class RpcResponse;

//static const size_t MAX_BLOCK_ALIGN = 64;
// 小于8k的消息依然copy，否则zero copy
static const size_t MIN_ZERO_COPY_SIZE = 4 * 1024;

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
                _data.emplace_back(len);
	  	        std::memcpy(_data.back().data, cur, len);
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

    struct byte_cursor {
        byte_cursor() = default;
        byte_cursor(const byte_cursor& o) = default;
        byte_cursor(byte_cursor&& o) = default;
        byte_cursor& operator = (const byte_cursor&) = default;
        byte_cursor& operator = (byte_cursor&&) = default;

        byte_cursor(RpcMessage* msg, bool zero_copy) {
            auto& data = msg->_data;
            if (!zero_copy) {
                _cur.emplace_back(
                      msg->_start, sizeof(rpc_head_t) + msg->head()->body_size);
                if (data.size()) {
                    _cur.emplace_back(reinterpret_cast<char*>(data.data()),
                          data.size() * sizeof(data[0]));
                }
            }
            size_t n = data.size();
            for (size_t i = 0; i < n; ++i) {
                if (zero_copy && data[i].length >= MIN_ZERO_COPY_SIZE) {
                    _cur.emplace_back(data[i].data, data[i].length);
                }
                if (!zero_copy && data[i].length < MIN_ZERO_COPY_SIZE) {
                    _cur.emplace_back(data[i].data, data[i].length);
                }
            }
        }

        void reset() {
            _cur.clear();
        }

        bool has_next() {
            return !_cur.empty();
        }

        size_t size() {
            return _cur.size();
        }

        std::pair<char*, size_t> head() {
            auto ret = _cur.front();
            return ret;
        }

        void next() {
            _cur.pop_front();
        }

        void advance(size_t nbytes) {
            SCHECK(_cur.front().second >= nbytes);
            _cur.front().second -= nbytes;
            _cur.front().first += nbytes;
            while (!_cur.empty() && _cur.front().second == 0) {
                _cur.pop_front();
            }
        }

        std::deque<std::pair<char*, size_t>> _cur;
    };

    byte_cursor zero_copy_cursor() {
        return byte_cursor(this, true);
    }

    byte_cursor cursor() {
        return byte_cursor(this, false);
    }

    friend std::ostream& operator<< (std::ostream& stream, const RpcMessage& msg) {
        stream << *((RpcMessage&)msg).head();
        return stream;
    }

    void send_failure() {
        _send_failure_func();
    }

    friend RpcRequest;
    friend RpcResponse;
    friend class TcpSocket;
    friend class RdmaSocket;

    void initialize(rpc_head_t&& head, BinaryArchive&& ar, LazyArchive&& lazy);
    void finalize(rpc_head_t& head, BinaryArchive& ar, LazyArchive& lazy);

    char* _start = nullptr;
    std::shared_ptr<char> _buffer;
    pico::core::vector<data_block_t> _data;
    int _pending_block_cnt = 0;

    std::function<void()> _send_failure_func = [](){};
    std::unique_ptr<LazyArchive> _hold;
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
        req._head = rpc_head_t();
        if (_msg) {
            _ar.release();
        }
        _msg = std::move(req._msg);
        _ar = std::move(req._ar);
        _lazy = std::move(req._lazy);
        return *this;
    }

    void set_send_failure_func(std::function<void(int)> func) {
        _send_failure_func = func;
    }

    void send_failure(int error_code) {
        _send_failure_func(error_code);
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
    std::function<void(int)> _send_failure_func = [](int){};
};

class RpcResponse {
public:
    friend RpcMessage;
    RpcResponse() = default;
   
    RpcResponse(const rpc_head_t& req_head) {
        const auto& hd = req_head;
        _head.dest_rank = hd.src_rank;
        _head.dest_dealer = hd.src_dealer;
        _head.src_rank = hd.dest_rank;
        _head.rpc_id = hd.rpc_id;
        _ar.resize(sizeof(_head));
        _ar.set_cursor(_ar.end());
    }

    RpcResponse(const RpcRequest& req) : RpcResponse(req.head()) {}

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

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_RPC_MESSAGE_H
