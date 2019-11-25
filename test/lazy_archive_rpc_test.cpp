#include <cstdlib>
#include <memory>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "LazyArchive.h"
#include "RpcService.h"

namespace paradigm4 {
namespace pico {
namespace core {

const int kMaxRetry = 10;
const int REPEAT = 10;
const int ALIGN = 32;
struct TestValue {
    char tmp[ALIGN] = {0};
} __attribute__((aligned(ALIGN)));

class mystring {
public:
    mystring(size_t size = 0) {
        _size = size / sizeof(TestValue) + 1;
        _data = reinterpret_cast<TestValue*>(pico_memalign(ALIGN, _size * ALIGN));
        _hold = std::shared_ptr<void>(reinterpret_cast<void*>(_data), pico_free);
        memset(static_cast<void*>(_data), 0, _size * ALIGN);
    }
    mystring(const mystring& s) {
        *this = s;
    }
    mystring& operator=(const mystring& s) {
        _size = s._size;
        _data = reinterpret_cast<TestValue*>(pico_memalign(ALIGN, _size * ALIGN));
        _hold = std::shared_ptr<void>(reinterpret_cast<void*>(_data), pico_free);
        memcpy(static_cast<void*>(_data), static_cast<void*>(s._data), _size * ALIGN);
        return *this;
    }
    mystring(mystring&&) = default;
    mystring& operator=(mystring&&) = default;
    char* c_str() {
        return reinterpret_cast<char*>(_data);
    }
    const char* c_str()const {
        return reinterpret_cast<const char*>(_data);
    }
    friend void pico_serialize(ArchiveWriter& ar, 
          SharedArchiveWriter& shared, mystring& s) {
        ar << s._size;
        shared.put_shared_uncheck(s._data, s._size);
    }
    friend void pico_deserialize(ArchiveReader& ar, 
          SharedArchiveReader& shared, mystring& s) {
        ar >> s._size;
        data_block_t own;
        shared.get_shared_uncheck(s._data, s._size, own);
        s._hold = std::make_shared<data_block_t>(std::move(own));
    };
    friend bool operator==(const mystring& a, const mystring& b) {
        //EXPECT_EQ(reinterpret_cast<uintptr_t>(a.c_str()) % ALIGN, 0);
        //EXPECT_EQ(reinterpret_cast<uintptr_t>(b.c_str()) % ALIGN, 0);
        return strcmp(a.c_str(), b.c_str()) == 0;
    }
    friend bool operator!=(const mystring& a, const mystring& b) {
        return !(a == b);
    }
private:
    size_t _size;
    TestValue* _data;
    std::shared_ptr<void> _hold;
};

void server_run(RpcService* rpc, const std::string& rpc_name) {
    auto server = rpc->create_server(rpc_name);
    auto dealer = server->create_dealer();
    std::vector<int> a;
    std::vector<double> b;
    std::string c;
    comm_rank_t d;
    std::pair<double, std::vector<int>> p;
    std::map<int32_t, std::vector<std::pair<int32_t, std::vector<float>>>> x;
    std::vector<decltype(x)> y, Y(REPEAT);
    std::vector<mystring> z, Z(REPEAT);
    RpcRequest request;
    while (dealer->recv_request(request)) {
        if (request.archive().readable_length() == 0) {
            break;
        }
        request.lazy() >> a >> b >> c >> d >> p >> x >> y >> z;
        for (int i = 0; i < REPEAT; i++) {
            request.lazy() >> Y[i] >> Z[i];
        }
        RpcResponse response(request);
        response.archive().write_raw_uncheck(
              request.archive().cursor(), request.archive().readable_length());
        response.lazy() << std::move(a) << std::move(b) << std::move(c)
                        << std::move(d) << std::move(p) << std::move(x)
                        << std::move(y) << std::move(z);
        for (int i = 0; i < REPEAT; i++) {
            response.lazy() << std::move(Y[i]) << std::move(Z[i]);
        }
        dealer->send_response(std::move(response));
    }
    dealer.reset();
};

void client_run(RpcService* rpc, const std::string& rpc_name) {
    auto client = rpc->create_client(rpc_name, 2);
    auto dealer = client->create_dealer();
    for (int k = 0; k < kMaxRetry; ++k) {
        std::vector<int> a = {1, 2, 3, 4}, a0 = a, a1;
        std::vector<double> b = {1.1, 2.1, 3.1, 4.1}, b0 = b, b1;
        std::string c = "dsagfgdbqotnlq", c0 = c, c1 = c;
        comm_rank_t d = 0, d0 = d, d1;
        std::pair<double, std::vector<int>> p = make_pair(d, a), p0 = p, p1;
        std::map<int32_t, std::vector<std::pair<int32_t, std::vector<float>>>> x = {
                {100, {
                        {110, {111, 112, 113}},
                        {120, {121}},
                        {130, {121}},
                }},
                {200, {
                        {210, {211, 212, 213}},
                        {220, {}},
                        {230, {231, 232, 233, 234, 235}},
                }},
                {300, {}},
            }, x0 = x, x1;
        std::vector<decltype(x)> y(REPEAT, x), y0 = y, y1;
        std::vector<decltype(x)> Y = y, Y0 = y, Y1(REPEAT);
        std::vector<mystring> z, z0, z1;
        for (int i = 0; i < REPEAT; i++) {
            mystring s(i);
            for (int j = 0; j < i; j++) {
                s.c_str()[j] = 'a' + (j * i) % 26;
            }
            z.push_back(s);
            z0.push_back(s);
        }
        std::vector<mystring> Z = z, Z0 = Z, Z1(REPEAT);

        RpcRequest request;
        request.lazy() << std::move(a) << std::move(b) << std::move(c)
                       << std::move(d) << std::move(p) << std::move(x)
                       << std::move(y) << std::move(z);
        for (int i = 0; i < REPEAT; i++) {
            request.lazy() << std::move(Y[i]) << std::move(Z[i]);
        }
        for (int i = -1; i < k / 10; i++) {
            request << i;
        }
        RpcResponse response = dealer->sync_rpc_call(std::move(request));
        response.lazy() >> a1 >> b1 >> c1 >> d1 >> p1 >> x1 >> y1 >> z1;
        for (int i = 0; i < REPEAT; i++) {
            response.lazy() >> Y1[i] >> Z1[i];
        }
        for (int i = -1; i < k / 10; i++) {
            int j;
            response >> j;
            EXPECT_EQ(i, j);
        }

        EXPECT_EQ(a0, a1);
        EXPECT_EQ(b0, b1);
        EXPECT_EQ(c0, c1);
        EXPECT_EQ(d0, d1);
        EXPECT_EQ(p0, p1);
        EXPECT_EQ(x0, x1);
        EXPECT_EQ(y0, y1);
        EXPECT_EQ(z0, z1);
        for (int i = 0; i < REPEAT; i++) {
            EXPECT_EQ(Y0[i], Y1[i]);
            EXPECT_EQ(Z0[i], Z1[i]);
        }
    }
    dealer.reset();
};

TEST(LazyArchive, rpc) {
    Master master("127.0.0.1");
    master.initialize();
    auto master_ep = master.endpoint();

    TcpMasterClient mc1(master_ep), mc2(master_ep);
    mc1.initialize();
    mc2.initialize();

    RpcService rpc1, rpc2;
    RpcConfig rpc_config;
    rpc_config.protocol = "tcp";
    rpc_config.bind_ip = "127.0.0.1";
    rpc_config.io_thread_num = 1;
    rpc1.initialize(&mc1, rpc_config);
    rpc2.initialize(&mc2, rpc_config);

    std::string rpc_name = "test_lazy_archive_rpc";
    std::thread s1, s2, c1, c2;
    s1 = std::thread(server_run, &rpc1, rpc_name);
    s2 = std::thread(server_run, &rpc2, rpc_name);
    c1 = std::thread(client_run, &rpc1, rpc_name);
    c2 = std::thread(client_run, &rpc2, rpc_name);

    c1.join();
    c2.join();

    RpcRequest req1, req2;
    req1.head().sid = 0;
    req2.head().sid = 1;
    rpc1.create_client(rpc_name)->create_dealer()->send_request(std::move(req2));
    rpc2.create_client(rpc_name)->create_dealer()->send_request(std::move(req1));

    s1.join();
    s2.join();
    rpc1.finalize();
    rpc2.finalize();
    mc1.finalize();
    mc2.finalize();
    master.exit();
    master.finalize();

}


} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
