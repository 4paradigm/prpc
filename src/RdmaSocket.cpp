#ifdef USE_RDMA
#include "RdmaSocket.h"
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/eventfd.h>


namespace paradigm4 {
namespace pico {
namespace core {

bool RdmaSocket::accept(std::string& info) {

    int64_t meta[2];
    if (retry_eintr_call(::recv,
              _fd,
              (char*)&meta[0],
              sizeof(meta),
              MSG_NOSIGNAL | MSG_WAITALL)
          != sizeof(meta)) {
        return false;
    }
    int64_t magic = meta[0];
    int64_t len = meta[1];
    if (magic != 0) {
        SLOG(INFO) << "magic not correct. " << magic;
        PSCHECK(::close(_fd) == 0);
        return false;
    }

    info.resize(len);
    if (retry_eintr_call(
              ::recv, _fd, (char*)info.data(), len, MSG_NOSIGNAL | MSG_WAITALL)
          != len) {
        return false;
    }

    if (retry_eintr_call(::send, _fd, &magic, 1, 0) != 1) {
        return false;
    }

    init();
    //PSCHECK(::close(_fd) == 0);
    return true;
}

ssize_t RdmaSocket::recv_nonblock(char* ptr, size_t size) {
    auto& buffer = _recv_buffer[_recv_id];
    int ret = buffer.tail() - _recv_cursor;
    if (ret == 0) {
        errno = EAGAIN;
        return -1;
    }
    if (ret > (int)size) {
        ret = size;
    }
    std::memcpy(ptr, _recv_cursor, ret);
    _recv_cursor += ret;
    return ret;
}

int ack_num(uint32_t imm_data) {
    return imm_data >> 16;
}

int read_complete_num(uint32_t imm_data) {
    return imm_data & 0xFFFF;
}

void add_ack_num(uint32_t& imm_data, int cnt = 1) {
    imm_data += (cnt << 16);
}

void add_read_complete_num(uint32_t& imm_data, int cnt = 1) {
    imm_data += cnt;
}

void RdmaSocket::post_read() {
    if (_pending_read_wrs.empty() || _uncomplete_read_cnt == BNUM) {
        return;
    }
    for (auto it = _pending_read_wrs.begin(); it != _pending_read_wrs.end();) {
        auto next = it + 1;
        it->first.sg_list = &it->second;
        ++_uncomplete_read_cnt;
        //SLOG(INFO) << "post read : " << _uncomplete_read_cnt;
        if (_uncomplete_read_cnt == BNUM || next == _pending_read_wrs.end()) {
            it->first.next = NULL;
            ibv_send_wr* bad_wr;
            PSCHECK(ibv_post_send(_qp, &_pending_read_wrs.front().first, &bad_wr)
                   == 0);
            _pending_read_wrs.erase(_pending_read_wrs.begin(), next);
            break;
        }
        it->first.next = &(next->first);
        it = next;
    }
}

bool RdmaSocket::handle_in_event(std::function<void(RpcMessage&&)> pass) {
    void* context;
    struct ibv_cq* cq;
    struct ibv_wc wc_arr[BNUM * 4];
    std::memset(wc_arr, 0, sizeof(wc_arr));
    PSCHECK(ibv_get_cq_event(_recv_cq_channel, &cq, &context) == 0);
    PSCHECK(ibv_req_notify_cq(_recv_cq, 0) == 0);
    int64_t ctl_msg_cnt = 0;
    uint32_t ack_cnt = 0;

    auto func = [this, pass](RpcMessage&& msg) {
        if (msg._pending_block_cnt == 0) {
            pass(std::move(msg));
        } else {
            std::vector<ibv_send_wr> wrs;
            wrs.reserve(msg._pending_block_cnt);
            std::vector<ibv_sge> sges;
            sges.reserve(msg._pending_block_cnt);
            for (size_t i = 0; i < msg._data.size(); ++i) {
                auto& block = msg._data[i];
                //SLOG(INFO) << "one block is " << block.length;
                if (block.length >= MIN_ZERO_COPY_SIZE) {
                    //SLOG(INFO) << "append one. " << block.length;
                    _pending_read_wrs.emplace_back();
                    auto& wr = _pending_read_wrs.back().first;
                    auto& sge = _pending_read_wrs.back().second;
                    sge.addr = (uintptr_t)block.data;
                    sge.length = block.length;
                    sge.lkey = block.lkey;
                    wr.wr.rdma.remote_addr = (uintptr_t)msg.lazy_meta()[i].data;
                    wr.wr.rdma.rkey = msg.lazy_meta()[i].lkey;
                    wr.num_sge = 1;
                    wr.opcode = IBV_WR_RDMA_READ;
                    wr.send_flags = IBV_SEND_SIGNALED;
                    wr.sg_list = &sge;
                }
            }
            _recving_msgs.push_back(std::move(msg));
        }
    };

    while (true) {
        int ret = ibv_poll_cq(_recv_cq, BNUM * 4, wc_arr);
        PSCHECK(ret >= 0);
        for (int i = 0; i < ret; ++i) {
            ibv_wc& wc = wc_arr[i];
            if (wc.status != IBV_WC_SUCCESS) {
                SLOG(WARNING)
                      << "ibv_poll_cq failed. " << ibv_wc_status_str(wc.status);
                RdmaContext::singleton().print();
                return false;
            }
            // SLOG(INFO) << "in event : " << wc.byte_len << " " << wc.imm_data
            // << " " << ack_num(wc.imm_data) << " " <<
            // read_complete_num(wc.imm_data);
            if (wc.byte_len > 0) {
                _recv_buffer[wc.wr_id].cursor = wc.byte_len;
                _recv_cursor = _recv_buffer[wc.wr_id].head();
                _recv_id = wc.wr_id;
                try_recv_msgs(func);
                add_ack_num(ack_cnt, 1);
            }
            ctl_msg_cnt += ack_num(wc.imm_data);
            for (int j = 0; j < read_complete_num(wc.imm_data); ++j) {
                std::unique_ptr<paradigm4::pico::core::RdmaSocket::msg_mr_t> t;
                //SLOG(INFO) << "read ok";
                _sending_msgs.pop(t);
                //SLOG(INFO) << "read ok";
            }
            post_recv(wc.wr_id);
        }
        if (ret < BNUM * 4) {
            break;
        }
    }
    _ack_cnt.fetch_add(ack_cnt, std::memory_order_relaxed);
    if (ctl_msg_cnt > 0) {
        if (_post_send_blocker_cnt.fetch_add(
                  ctl_msg_cnt, std::memory_order_acq_rel)
              == -1) {
            ctl_msg_cnt = 1;
            PSCHECK(
                  ::write(_post_send_blocker, &ctl_msg_cnt, sizeof(ctl_msg_cnt))
                  == sizeof(ctl_msg_cnt));
        }
    }
    ibv_ack_cq_events(_recv_cq, 1);
    post_read();
    send_ack();
    return true;
}

bool RdmaSocket::handle_out_event(std::function<void(RpcMessage&&)> pass) {
    void* context;
    struct ibv_cq* cq;
    struct ibv_wc wc_arr[BNUM * 4];
    PSCHECK(ibv_get_cq_event(_send_cq_channel, &cq, &context) == 0);
    PSCHECK(cq == _send_cq) << (void*) cq << " " << (void*) _send_cq;
    PSCHECK(ibv_req_notify_cq(_send_cq, 0) == 0);
    uint32_t ack_cnt = 0;

    while (true) {
        int ret = ibv_poll_cq(_send_cq, BNUM * 4, wc_arr);
        PSCHECK(ret >= 0);
        for (int i = 0; i < ret; ++i) {
            ibv_wc& wc = wc_arr[i];
            if (wc.status != IBV_WC_SUCCESS) {
                SLOG(WARNING)
                      << "ibv_poll_cq failed. " << ibv_wc_status_str(wc.status);
                RdmaContext::singleton().print();
                return false;
            }
            if (wc.opcode == IBV_WC_RDMA_READ) {
                --_uncomplete_read_cnt;
                //SLOG(INFO) << "read succ. "
                //           << _recving_msgs.front()._pending_block_cnt << " "
                //           << _uncomplete_read_cnt;
                if (--_recving_msgs.front()._pending_block_cnt == 0) {
                    pass(std::move(_recving_msgs.front()));
                    _recving_msgs.pop_front();
                    ++_read_complete_num;
                    //SLOG(INFO) << "herererer : " << _read_complete_num;
                    //add_read_complete_num(ack_cnt, 1);
                }
            } else if (wc.wr_id == UINT64_MAX) {
                //SLOG(INFO) << "ack succ.";
                --_uncomplete_ack_cnt;
            } else {
                //SLOG(INFO) << "post send succ.";
                //SCHECK(_get_send_buffer_blocker[wc.wr_id].load()) << wc.wr_id;
                _get_send_buffer_blocker[wc.wr_id].store(
                      false, std::memory_order_release);
            }
        }
        if (ret < BNUM * 4) {
            break;
        }
    }
    _ack_cnt.fetch_add(ack_cnt, std::memory_order_relaxed);
    ibv_ack_cq_events(_send_cq, 1);
    post_read();
    send_ack();
    return true;
}


bool RdmaSocket::handle_event(int fd, std::function<void(RpcMessage&&)> func) {
    if (fd == _recv_cq_channel->fd) { // recv complete.
        return handle_in_event(func);
    } else if (fd == _send_cq_channel->fd) {
        return handle_out_event(func);
    } else if (fd == _fd) {
        SLOG(WARNING) << "handle unexpected tcp socket event, maybe epipe.";
        return false;
    } else {
        SLOG(FATAL) << "unknown file desc. " << fd;
    }
    return false;
}

void RdmaSocket::send_ack() {
    // on the fly的ack不能太多
    if (_uncomplete_ack_cnt >= BNUM / 2 + 1) {
        return;
    }
    struct ibv_send_wr wr;
    struct ibv_send_wr* bad_wr;
    memset(&wr, 0, sizeof(wr));

    wr.imm_data = _ack_cnt.exchange(0, std::memory_order_relaxed);
    // 如果只有完成的read，没有其他的ack信息，则不发送ack，否则会把cq撑满
    if (wr.imm_data == 0 ) {
        return;
    }
    add_read_complete_num(wr.imm_data, _read_complete_num);
    _read_complete_num = 0;
    wr.wr_id = UINT64_MAX;
    wr.sg_list = nullptr;
    wr.num_sge = 0;
    wr.opcode = IBV_WR_SEND_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
    //SLOG(INFO) << "send ack : " << ack_num(wr.imm_data)
    //           << " read ok num : " << read_complete_num(wr.imm_data);
    PSCHECK(ibv_post_send(_qp, &wr, &bad_wr) == 0);
    ++_uncomplete_ack_cnt;
}

int RdmaSocket::send(const char* buffer, uint32_t size, bool more) {
    int ret = 0;
    buffer_t* b = nullptr;
    while (size) {
        b = get_send_buffer();
        int nbytes = std::min(size, BSIZE - b->cursor);
        std::memcpy(&b->data[b->cursor], buffer + ret, nbytes);
        b->cursor += nbytes;
        ret += nbytes;
        size -= nbytes;
        if (BSIZE - b->cursor == 0) {
            post_send();
        }
    }
    if (b && b->cursor && !more) {
        post_send();
    }
    return ret;
}

RdmaSocket::buffer_t* RdmaSocket::get_send_buffer() {
    // BNUM大于线程数的时候，百分百可以acquire到
    for (;;) {
        if (!_get_send_buffer_blocker[_send_id].load(
                  std::memory_order_acquire)) {
            break;
        }
    }
    return &_send_buffer[_send_id];
}

void RdmaSocket::post_recv(int i) {
    struct ibv_sge sge;
    sge.addr = (uint64_t)_recv_buffer[i].head();
    sge.length = BSIZE;
    sge.lkey = _recv_buffer[i].mr->lkey;
    struct ibv_recv_wr wr, *bad;
    wr.wr_id = i;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    PSCHECK(ibv_post_recv(_qp, &wr, &bad) == 0);
}

void RdmaSocket::post_send() {
    if (_send_buffer[_send_id].cursor == 0) {
        return;
    }
    // 等待可以发送
    int ret = 0;
    _get_send_buffer_blocker[_send_id].store(true, std::memory_order_relaxed);
    struct ibv_send_wr wr;
    struct ibv_send_wr* bad_wr = nullptr;

    struct ibv_sge sge;
    sge.addr = (uint64_t)_send_buffer[_send_id].head();
    sge.length = _send_buffer[_send_id].cursor;
    _send_buffer[_send_id].cursor = 0;
    sge.lkey = _send_buffer[_send_id].mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id = _send_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.imm_data = _ack_cnt.exchange(0, std::memory_order_relaxed);
    wr.opcode = IBV_WR_SEND_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
    ret = _post_send_blocker_cnt.fetch_add(-1, std::memory_order_acq_rel);
    if (ret == 0) {
        int64_t _;
        PSCHECK(::read(_post_send_blocker, &_, sizeof(_)) == sizeof(_));
    }
    PCHECK(ibv_post_send(_qp, &wr, &bad_wr) == 0 && bad_wr == nullptr);
    _send_id = (_send_id + 1) % BNUM;
}


void RdmaSocket::init() {
    _ib_ctx = RdmaContext::singleton().ib_ctx();
    _pd = RdmaContext::singleton().pd();
    _send_cq_channel = ibv_create_comp_channel(_ib_ctx);
    PSCHECK(_send_cq_channel);
    _recv_cq_channel = ibv_create_comp_channel(_ib_ctx);
    PSCHECK(_recv_cq_channel);
    PSCHECK(fcntl(_recv_cq_channel->fd, F_SETFD, O_NONBLOCK) == 0);
    PSCHECK(fcntl(_send_cq_channel->fd, F_SETFD, O_NONBLOCK) == 0);
    _send_cq = ibv_create_cq(_ib_ctx, BNUM * 4, NULL, _send_cq_channel, 0);
    PSCHECK(_send_cq);
    _recv_cq = ibv_create_cq(_ib_ctx, BNUM * 4, NULL, _recv_cq_channel, 0);
    PSCHECK(_recv_cq);
    struct ibv_qp_init_attr attr;
    memset(&attr, 0, sizeof(struct ibv_qp_init_attr));
    attr.qp_context = NULL;
    attr.send_cq = _send_cq;
    attr.recv_cq = _recv_cq;
    attr.srq = NULL;
    attr.cap.max_send_wr = BNUM * 4;
    attr.cap.max_send_sge = 1;
    attr.cap.max_inline_data = 0;
    attr.cap.max_recv_wr = BNUM * 4;
    attr.cap.max_recv_sge = 1;
    attr.sq_sig_all = 1;
    attr.qp_type = IBV_QPT_RC;
    _qp = ibv_create_qp(_pd, &attr);
    PSCHECK(_qp);
    init_mr();
    init_qp();
    for (int i = 0; i < BNUM * 4; ++i) {
        post_recv(i);
    }
   PSCHECK(ibv_req_notify_cq(_recv_cq, 0) == 0);
   PSCHECK(ibv_req_notify_cq(_send_cq, 0) == 0);
}

void RdmaSocket::init_mr() {
    _post_send_blocker = eventfd(0, EFD_SEMAPHORE | EFD_CLOEXEC);
    _post_send_blocker_cnt = BNUM;
    std::memset(_recv_buffer, 0, sizeof(_recv_buffer));
    std::memset(_send_buffer, 0, sizeof(_send_buffer));
    for (int i = 0; i < BNUM; ++i) {
        _get_send_buffer_blocker[i] = false;
    }
    for (int i = 0; i < BNUM; ++i) {
        _send_buffer[i].mr = ibv_reg_mr(_pd,
              _send_buffer[i].head(),
              BSIZE,
              IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ
                    | IBV_ACCESS_REMOTE_WRITE);
        PSCHECK(_send_buffer[i].mr) << "rdma_reg_read failed.";
    }

    for (int i = 0; i < BNUM * 4; ++i) {
        _recv_buffer[i].mr = ibv_reg_mr(_pd,
              _recv_buffer[i].head(),
              BSIZE,
              IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ
                    | IBV_ACCESS_REMOTE_WRITE);
        PSCHECK(_recv_buffer[i].mr) << "rdma_reg_write failed.";
    }
}

void RdmaSocket::init_qp() {
    {
        int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT
                    | IBV_QP_ACCESS_FLAGS;
        struct ibv_qp_attr attr;
        memset(&attr, 0, sizeof(struct ibv_qp_attr));
        attr.qp_state = IBV_QPS_INIT;
        attr.pkey_index = RdmaContext::singleton().pkey_index();
        attr.port_num = RdmaContext::singleton().ib_port();
        attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE
                               | IBV_ACCESS_REMOTE_READ;
        PSCHECK(ibv_modify_qp(_qp, &attr, flags) == 0);
    }

    // XXX 交换信息 写的比较糊，丢包buffer满就死了。
    // 但是前面交换meta信息都成功了，这个死不了应该
    int qpn = _qp->qp_num;
    ibv_gid gid = RdmaContext::singleton().gid();
    SLOG(INFO) << "local info : " << qpn << " " << gid.global.subnet_prefix << " " << gid.global.interface_id;
    PSCHECK(retry_eintr_call(::send, _fd, &qpn, sizeof(qpn), MSG_NOSIGNAL) == sizeof(qpn));
    PSCHECK(retry_eintr_call(::send, _fd, &gid, sizeof(gid), MSG_NOSIGNAL) == sizeof(gid));
    PSCHECK(retry_eintr_call(::recv, _fd, &qpn, sizeof(qpn), MSG_WAITALL) == sizeof(qpn));
    PSCHECK(retry_eintr_call(::recv, _fd, &gid, sizeof(gid), MSG_WAITALL) == sizeof(gid));
    SLOG(INFO) << "remote info : " << qpn << " " << gid.global.subnet_prefix << " " << gid.global.interface_id;
    {
        struct ibv_qp_attr attr;
        memset(&attr, 0, sizeof(struct ibv_qp_attr));

        int flags = IBV_QP_STATE;
        attr.qp_state = IBV_QPS_RTR;
        attr.ah_attr.src_path_bits = 0;
        attr.ah_attr.port_num = RdmaContext::singleton().ib_port();

        attr.ah_attr.dlid = 0; // 不支持跨subnet
        attr.ah_attr.sl = RdmaContext::singleton().sl();

        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.dgid = gid; // dest gid
        attr.ah_attr.grh.sgid_index = RdmaContext::singleton().gid_index();
        attr.ah_attr.grh.hop_limit = 1;// 包不会跨subnet
        attr.ah_attr.grh.traffic_class = RdmaContext::singleton().traffic_class();
        attr.path_mtu = RdmaContext::singleton().mtu();

        attr.dest_qp_num = qpn; // dest qpn
        attr.rq_psn = 0; // dest psn 这里大家都从0开始

        flags |= (IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN
                  | IBV_QP_RQ_PSN);

        attr.max_dest_rd_atomic = RdmaContext::singleton().max_reads();
        attr.min_rnr_timer = RdmaContext::singleton().min_rnr_timer();
        flags |= (IBV_QP_MIN_RNR_TIMER | IBV_QP_MAX_DEST_RD_ATOMIC);
        PSCHECK(ibv_modify_qp(_qp, &attr, flags) == 0);
    }

    {
        int flags = IBV_QP_STATE;
        struct ibv_qp_attr attr;
        memset(&attr, 0, sizeof(struct ibv_qp_attr));
        attr.qp_state = IBV_QPS_RTS;
        flags |= IBV_QP_SQ_PSN;
        attr.sq_psn = 0;
        attr.timeout = RdmaContext::singleton().timeout();
        attr.retry_cnt = RdmaContext::singleton().retry_cnt();
        attr.rnr_retry = RdmaContext::singleton().rnr_retry();
        attr.max_rd_atomic = RdmaContext::singleton().max_reads();
        flags |= (IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY
                  | IBV_QP_MAX_QP_RD_ATOMIC);
        PSCHECK(ibv_modify_qp(_qp, &attr, flags) == 0);
    }
}

bool RdmaSocket::connect(const std::string& endpoint, const std::string& info,
        int64_t magic) {
    sockaddr_in addr = parse_rpc_endpoint(endpoint);
    int ret = 0;
    auto starttm = std::chrono::steady_clock::now();
    for (int i = 1; ;) {
        ret = retry_eintr_call(
              ::connect, _fd, (sockaddr*)&addr, sizeof(addr));
        if (ret == 0) {
            break;
        }
        auto dur = std::chrono::steady_clock::now() - starttm;
        int sec = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
        if (TcpSocket::_tcp_config.connect_timeout >= 0 && 
                sec >= TcpSocket::_tcp_config.connect_timeout) {
            PSLOG(WARNING) << "connect failed endpoint: " << endpoint
                    << ", exit connect";
            return false;
        }
        if (i < 32) {
            i *= 2;
        }
        PSLOG(INFO) << "connect failed endpoint: " << endpoint
                    << " , sleep for " << i << " seconds.";
        ::sleep(i);
    }
    int64_t meta[2] = {magic, (int64_t)info.length()};
    ret = retry_eintr_call(::send, _fd, (const char*)&meta[0], sizeof(meta), MSG_NOSIGNAL);
    if (ret != sizeof(meta)) {
        PSLOG(INFO) << "connect failed endpoint and ret is : " << endpoint << " " << ret;
        return false;
    }
    ret = retry_eintr_call(::send, _fd, info.data(), info.size(), MSG_NOSIGNAL);
    if (ret != (int)info.size()) {
        PSLOG(INFO) << "connect failed endpoint and ret is : " << endpoint << " " << ret;
        return false;
    }
    char tmp_buffer[10];
    PSCHECK(::recv(_fd, tmp_buffer, 1, MSG_WAITALL) == 1);
    init();
    //PSCHECK(::close(_fd) == 0);
    return true;
}

RdmaSocket::~RdmaSocket() {
    ::close(_fd);
    for (int i = 0; i < BNUM; ++i) {
        ibv_dereg_mr(_send_buffer[i].mr);
    }
    for (int i = 0; i < BNUM * 4; ++i) {
        ibv_dereg_mr(_recv_buffer[i].mr);
    }
    PSCHECK(ibv_destroy_qp(_qp) == 0);
    PSCHECK(ibv_destroy_cq(_send_cq) == 0);
    PSCHECK(ibv_destroy_cq(_recv_cq) == 0);
    PSCHECK(ibv_destroy_comp_channel(_send_cq_channel) == 0);
    PSCHECK(ibv_destroy_comp_channel(_recv_cq_channel) == 0);
}

bool RdmaSocket::send_msg(RpcMessage& msg,
      bool,
      bool more,
      RpcMessage::byte_cursor& it1,
      RpcMessage::byte_cursor& it2) {

    if (!msg._data.empty()) {
        /*
         * 含有zero copy block的消息
         * 需要move到heap上
         */
        std::unique_ptr<RpcMessage> m
              = std::make_unique<RpcMessage>(std::move(msg));
        std::vector<ibv_mr*> mrs;
        auto zero_copy_block_cnt = 0;
        for (auto& block : m->_data) {
            if (block.length >= MIN_ZERO_COPY_SIZE) {
                ++zero_copy_block_cnt;
                auto mr
                      = RdmaContext::singleton().get(block.data, block.length);
                if (mr == nullptr) {
                    SLOG(WARNING) << "unregisterd mr : " << (uint64_t)block.data
                                  << " " << block.length;
                    //RdmaContext::singleton().print();
                    ibv_mr* ib_mr = nullptr;
                    if (block.data == nullptr) {
                        ib_mr = nullptr;
                        block.lkey = 0;
                    } else {
                        ib_mr = ibv_reg_mr(_pd,
                              block.data,
                              block.length,
                              IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ
                                    | IBV_ACCESS_REMOTE_WRITE);
                        PSCHECK(ib_mr);
                        block.lkey = ib_mr->lkey;
                        mrs.push_back(ib_mr);
                    }
                } else {
                    block.lkey = mr->mr->lkey;
                }
            }
        }
        if (zero_copy_block_cnt) {
            std::unique_ptr<msg_mr_t> item = std::make_unique<msg_mr_t>();
            item->msg = std::move(m);
            item->mrs = std::move(mrs);
            item->zero_copy_block_cnt = zero_copy_block_cnt;
            _sending_msgs.push(std::move(item));
        }
    }

    while (it1.has_next()) {
        auto front = it1.head();
        it1.next();
        send(front.first, front.second, it1.has_next() | more);
    }


    while (it2.has_next()) {
        auto front = it2.head();
        it2.next();
        if (front.second < MIN_ZERO_COPY_SIZE) {
            // 目前这个more也是糊的
            send(front.first, front.second, it2.has_next() | more);
        }
    }
    return true;
}

RdmaAcceptor::~RdmaAcceptor() {
    ::close(_fd);
}

bool RdmaAcceptor::bind(const std::string& endpoint) {
    SCHECK(_ep == "") << "already bind on " << _ep;
    sockaddr_in addr = parse_rpc_endpoint(endpoint);
    if (::bind(_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        _ep = endpoint;
        return true;
    } else {
        return false;
    }
}

int RdmaAcceptor::listen(int backlog) {
    return retry_eintr_call(::listen, _fd, backlog);
}

std::unique_ptr<RpcSocket> RdmaAcceptor::accept() {
    sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);
    int fd = ::accept4(_fd, (sockaddr*)&remote_addr, &len, SOCK_CLOEXEC);
    TcpSocket::set_sockopt(fd);
    PSCHECK(fd != -1);
    SLOG(INFO) << "received a tcp connection from "
               << inet_ntoa(remote_addr.sin_addr) << ":"
               << ntohs(remote_addr.sin_port) << " fd is : " << fd;
    return std::make_unique<RdmaSocket>(fd);
}

int RdmaAcceptor::fd() {
    return _fd;
}

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif
