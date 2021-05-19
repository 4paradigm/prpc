#ifndef PARADIGM4_PICO_CORE_ACCUMULATORSERVER_H
#define PARADIGM4_PICO_CORE_ACCUMULATORSERVER_H

#include "macro.h"
#include "AggregatorFactory.h"
#include "Aggregator.h"
#include "RpcService.h"

namespace paradigm4 {
namespace pico {
namespace core {

const char* const ACCUMULATOR_SERVICE_API = "_ACCUMULATOR_SERVICE_API";
const char ACCUMULATOR_SERVICE_API_READ = 'R';
const char ACCUMULATOR_SERVICE_API_WRITE = 'W';
const char ACCUMULATOR_SERVICE_API_RESET = 'C';
const char ACCUMULATOR_SERVICE_API_ERASE = 'E';
const char ACCUMULATOR_SERVICE_API_ERASE_ALL = 'A';
const char ACCUMULATOR_SERVICE_API_WAIT_EMPTY = 'I';
const char ACCUMULATOR_SERVICE_API_START = '0';
const char ACCUMULATOR_SERVICE_API_STOP = '1';

struct AccumulatorConfig {
    std::string report_accumulator_json_path = "";
    size_t report_interval_in_sec = 30u;
    bool report_accumulator_pretty = true;
    size_t max_num_of_snapshots_saved = 30u;
};

class AccumulatorServer : public NoncopyableObject {
public:

    static AccumulatorServer& singleton() {
        static AccumulatorServer as;
        return as;
    }

    /*!
     * \brief get server id, lock thread, register rpc service, init process request thread
     */
    void initialize(RpcService* rpc);

    void finalize();
    void wait_empty();

    /*!
     * \brief get name, aggregator pair in string form, sorted by name
     */
    std::vector<std::pair<std::string, std::string>> generate_output_info();

    /*!
     * \brief convert output info vector to json str
     */
    static std::string generate_json_str(
            const std::vector<std::pair<std::string, std::string>>& output_info,
            bool pretty = true);

    /*!
     * \brief get all agg value in accumulator server in PicoJsonNode form
     */
    static PicoJsonNode generate_pico_json_node(
            const std::vector<std::pair<std::string, std::string>>& output_info);

private:
    AccumulatorServer();

    void process_request();

    /*!
     * \brief request to get a certain key of agg
     */
    void process_read_request(RpcRequest& request);

    /*!
     * \brief request to reset a key
     */
    void process_reset_request(RpcRequest& request);

    /*!
     * \brief merge request agg to server map
     */
    void process_write_request(RpcRequest& request);

    void process_erase_request(RpcRequest& request);

    void process_erase_all_request(RpcRequest& request);

    /*!
     * \brief start a new acc client
     */
    void process_start_request(RpcRequest& request);

    /*!
     * \brief stop a acc client, if all stoped, close request channel
     */
    void process_stop_request(RpcRequest& request);

    /*!
     * \brief get agg from map, if not exist, insert a new agg
     */
    AggregatorBase* get_agg_from_pool(const std::string type_name);

private:
    bool _started;
    comm_rank_t _client_count;

    std::thread _process_request_thread;
 
    RpcService* _rpc = nullptr;
    std::unique_ptr<RpcServer> _rpc_server;
    std::shared_ptr<Dealer> _dealer;
    std::mutex _mutex;

    std::unordered_map<std::string, std::pair<std::string, std::unique_ptr<AggregatorBase>>>
            _umap_name2aggs;
    std::unordered_map<std::string, std::unique_ptr<AggregatorBase>> _agg_pool;


};


}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_ACCUMULATORSERVER_H
