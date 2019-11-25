#ifndef PARADIGM4_PICO_CORE_RDMA_SOCKET_H
#define PARADIGM4_PICO_CORE_RDMA_SOCKET_H
#ifdef USE_RDMA

#include <algorithm>
#include <cstring>
#include <string>

#include "RpcMessage.h"
#include "RpcSocket.h"
#include "MpscQueue.h"
#include "RdmaContext.h"
#include "TcpSocket.h"
#include <fcntl.h>

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

namespace paradigm4 {
namespace pico {
namespace core {

const int BNUM = 8;
const size_t BSIZE = 32 * 1024;

/*
 * imm_data < 0 意味着data里是 pos, lkey，取反后是正常的ack
 * imm_data 前16位表示write完成的数量
 * imm_data 后16位表示ack数
 */
class RdmaSocket : public RpcSocket {
public:

    struct buffer_t {
        uint32_t cursor;
        char __pad__[64];
        std::array<int8_t, BSIZE> data;
        struct ibv_mr* mr;

        char* head() {
            return (char*)(&data[0]);
        }

        char* tail() {
            return head() + cursor;
        }

    };

    struct msg_mr_t {
        core::unique_ptr<RpcMessage> msg;
        core::vector<ibv_mr*> mrs;
        int zero_copy_block_cnt;
        ~msg_mr_t() {
            for (auto& mr : mrs) {
                ibv_dereg_mr(mr);
            }
        }
    };

    RdmaSocket() : RpcSocket() {
        // 用于交换信息
        _fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
        TcpSocket::set_sockopt(_fd);
    }

    RdmaSocket(int fd) : RpcSocket(), _fd(fd) {
        TcpSocket::set_sockopt(_fd);
    }

    ~RdmaSocket();

    bool connect(const std::string& endpoint, const std::string& info, int64_t magic) override;

    std::vector<int> fds() override {
        return {_recv_cq_channel->fd, _send_cq_channel->fd, _fd};
    }

    bool accept(std::string& info) override;

    ssize_t recv_nonblock(char* ptr, size_t size) override;
    char* _recv_cursor;
    int _recv_id;

    void send_ack();

    virtual bool handle_event(int fd,
          std::function<void(RpcMessage&&)>) override;

    /*
     * 直接把msg move给socket了
     * RDMA情形下，不可恢复
     * 这个函数always return true
     * 并且output的it1和it2的has_next永远返回false
     */
    bool send_msg(RpcMessage& msg,
          bool nonblock,
          bool more,
          RpcMessage::byte_cursor& it1,
          RpcMessage::byte_cursor& it2) override;

private:
    bool handle_in_event(std::function<void(RpcMessage&&)>);

    bool handle_out_event(std::function<void(RpcMessage&&)>);

    int send(const char* buffer, size_t size, bool more = false);

    buffer_t* get_send_buffer();

    void post_recv(int i);

    void post_send();

    //void generate_post_write(BinaryArchive& ar);

    void post_read();

    void init();
    void init_mr();
    void init_cq();
    void init_qp();


    ///////////////////////////
    ibv_context* _ib_ctx;
    ibv_pd* _pd;
    ibv_cq* _send_cq;
    ibv_cq* _recv_cq;
    ibv_comp_channel* _send_cq_channel;
    ibv_comp_channel* _recv_cq_channel;
    ibv_qp* _qp;
    int _fd; // tcp connection

    ////////////////////////////
    
    // recv多一倍为了收ctrl msg
    buffer_t _recv_buffer[BNUM * 4];

    int _send_id = 0;
    buffer_t _send_buffer[BNUM];
    //struct ibv_mr* send_mr[BNUM];
    int64_t _uncomplete_ack_cnt = 0;
    int64_t _uncomplete_read_cnt = 0;
    int _post_send_blocker;
    int _read_complete_num = 0;

    // 用于暂时保存zero copy的消息的所有权

    std::deque<RpcMessage> _recving_msgs;
    std::deque<std::pair<ibv_send_wr, ibv_sge>> _pending_read_wrs;
    buffer_t _ack_buffer[BNUM];

    std::atomic<int> _post_send_blocker_cnt;
    char _pad1_[64];
    std::atomic<bool> _get_send_buffer_blocker[BNUM];
    char _pad2_[64];
    std::atomic<int64_t> _ack_cnt;
    char _pad3_[64];
    MpscQueue<core::unique_ptr<msg_mr_t>> _sending_msgs;
};

class RdmaAcceptor : public RpcAcceptor {
public:
    RdmaAcceptor() : RpcAcceptor() {
        _fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
        PSCHECK(_fd != -1);
        _ep = "";
    }

    ~RdmaAcceptor();

    RdmaAcceptor(const RdmaAcceptor&) = delete;
    RdmaAcceptor(RdmaAcceptor&&) = delete;
    RdmaAcceptor& operator=(const RdmaAcceptor&) = delete;
    RdmaAcceptor& operator=(RdmaAcceptor&&) = delete;

    const std::string& endpoint() override {
        return _ep;
    }

    bool bind(const std::string& endpoint) override;

    int listen(int backlog) override;

    std::unique_ptr<RpcSocket> accept() override;

    int fd() override;

private:
    int _fd;
    std::string _ep;
};



} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // USE_RDMA
#endif // PARADIGM4_PICO_CORE_RDMA_SOCKET_H}}}
