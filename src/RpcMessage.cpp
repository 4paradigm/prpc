#include "RpcMessage.h"

namespace paradigm4 {
namespace pico {

void RpcMessage::initialize(rpc_head_t&& head, BinaryArchive&& ar, LazyArchive&& lazy) {
    lazy.apply(_data);
    head.extra_block_count = _data.size();
    head.extra_block_length = sizeof(data_block_t) * _data.size();
    for (const auto& data : _data) {
        // TODO 把这个判断临界值的逻辑拽出来
        if (data.length < MIN_ZERO_COPY_SIZE) {
            head.extra_block_length += data.length;
        }
    }
    head.body_size = ar.length() - sizeof(rpc_head_t);
    _start = ar.buffer();
    *this->head() = head;
    _buffer = ar.release_shared();
    _hold = std::make_unique<LazyArchive>(std::move(lazy));
}

void RpcMessage::finalize(rpc_head_t& head, BinaryArchive& ar, LazyArchive& lazy) {
    SCHECK(_hold == nullptr); 
    head = *this->head();
    ar.set_read_buffer(_start, head.body_size + sizeof(rpc_head_t));
    ar.advance_cursor(sizeof(head));
    lazy.attach(std::move(_data));
}


RpcMessage::RpcMessage(RpcRequest&& req) {
    if (req._msg) {
        *this = std::move(*req._msg);
    } else {
        initialize(std::move(req._head), std::move(req._ar), std::move(req._lazy));
    }
}

RpcMessage::RpcMessage(RpcResponse&& resp) {
    if (resp._msg) {
        *this = std::move(*resp._msg);
    } else {
        initialize(std::move(resp._head), std::move(resp._ar), std::move(resp._lazy));
    }
}
} // namespace pico
} // namespace paradigm4
