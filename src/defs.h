#ifndef PARADIGM4_PICO_COMMON_DEFS_H
#define PARADIGM4_PICO_COMMON_DEFS_H

#include <cstddef>
#include <cstdint>

namespace paradigm4 {
namespace pico {

/*!
 * \brief const value
 */

const size_t MAX_IP_PORT_STRLEN = 1024;
const char* const WRAPPER_IP_STR = "wrapper_ip";
const char* const WRAPPER_PORT_STR = "wrapper_port";
const char* const COMM_RANK_STR = "comm_rank";
const char* const COMM_SIZE_STR = "comm_size";
const char* const COMM_CODE_STR = "comm_code";
const char* const CONFIG_FILE_STR = "config_file";
const char* const CONFIG_JSON_STR = "config_json";
//const char* const CONNECTION_CONFIG_STR = "connection_configure";
const char* const MASTER_EP_STR = "master_endpoint";
const char* const PSERVER_NUM_STR = "pserver_num";
const char* const LEARNER_NUM_STR = "learner_num";
const char* const LOG_REPORT_URI_STR = "log_report_uri";
const size_t ARCHIVE_DEFAULT_SIZE = 1024;

const int32_t BARRIER_ROOT = 0;
const char* const BARRIER_SERVICE_API = "_PICO_BARRIER_API_";
const uint32_t MURMURHASH_SEED = 142857;

const char* const ACCUMULATOR_SERVICE_API = "_ACCUMULATOR_SERVICE_API";
const char ACCUMULATOR_SERVICE_API_READ = 'R';
const char ACCUMULATOR_SERVICE_API_WRITE = 'W';
const char ACCUMULATOR_SERVICE_API_RESET = 'C';
const char ACCUMULATOR_SERVICE_API_ERASE = 'E';
const char ACCUMULATOR_SERVICE_API_ERASE_ALL = 'A';
const char ACCUMULATOR_SERVICE_API_WAIT_EMPTY = 'I';
const char ACCUMULATOR_SERVICE_API_START = '0';
const char ACCUMULATOR_SERVICE_API_STOP = '1';
const char* const PROGRESS_ACCUMULATOR_NAME = "progress";

const char* const INPUT_STRATEGY_SERVICE_API = "_INPUT_STRATEGY_SERVICE_API";

const char* const RUNNING_ENV_pico_version_code = "Pico Version Code";
const char* const RUNNING_ENV_pico_comm_rank = "Rank of Dumping Meatadata Node";
const char* const RUNNING_ENV_pico_comm_size = "Pico Communication Size";
const char* const RUNNING_ENV_pico_comm_code = "Pico Communication Code";
const char* const RUNNING_ENV_init_local_time = "Pico Initialize Local Time";
const char* const RUNNING_ENV_init_UTC_time = "Pico Initialize UTC Time";
const char* const RUNNING_ENV_dump_local_time = "Pico Dump Metadata Local Time";
const char* const RUNNING_ENV_dump_UTC_time = "Pico Dump Metadata UTC Time";
const char* const RUNNING_ENV_pico_master_snapshot = "Pico Master Snapshot";

const char* const EXECUTION_INFO_running_env = "RunningEnvironment";
const char* const EXECUTION_INFO_config = "Configure";
const char* const EXECUTION_INFO_app_config = "ApplicationConfigure";
const char* const EXECUTION_INFO_framework_config = "FrameworkConfigure";
const char* const EXECUTION_INFO_env_config = "EnvConfigure";
const char* const EXECUTION_INFO_accumulator = "Accumulator";
const char* const EXECUTION_INFO_accumulator_snapshots = "AccumulatorSnapshots";
const char* const EXECUTION_INFO_execution_info = "ExecutionInfomation";

const char* const MODEL_INFO_model_version = "ModelVersion";
const char* const DEFAULT_TIME_STR_FORMAT = "%c %Z";

const char* const METADATA_CONTEXT_ds_train = "ds_train";

// Regex Expressions
const char* const REGEX_NON_EMPTY = R"(^.+$)";
const char* const REGEX_NON_EMPTY_PREFIX = R"(^([\w\.-])+$)";
const char* const REGEX_ENABLED_SLOTS
      = R"((^(\d+|\d+\:\d+)?$|^(\d+|\d+\:\d+)(\,(\d+|\d+\:\d+))+$))";
const char* const REGEX_IPV4
      = R"(^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$)";
const char* const REGEX_CACHE_URI
      = R"(^(((([bt]\-file)|((snappy|zlib|lz4)\-(file|mem))|(mem)):\/\/[^\s]*)|)$)";
const char* const REGEX_NONEMPTY_CACHE_URI
      = R"(^(([bt]\-file)|((snappy|zlib|lz4)\-(file|mem))|(mem)):\/\/[^\s]*$)";
const char* const REGEX_BINARY_CACHE_URI
      = R"(^((b\-file)|((snappy|zlib|lz4)\-(file|mem))|(mem)):\/\/[^\s]*$)";
const char* const REGEX_INPUT_PATH_URI = R"(^(\S)+$)";

typedef int16_t comm_rank_t;
const comm_rank_t EMPTY_COMM_RANK = -1;

const char* const PSERVER_LOCK_NAME = "PSERVER_LOCK";
const char* const CONNECTION_BARRIER = "CONNECTION_BARRIER";

const char* const PSERVER_C2S_RPC_NAME = "pserver_c2s_rpc_api";
const char* const PSERVER_S2S_RPC_NAME = "pserver_s2s_rpc_api";
const char* const CONTROLLER_RPC_NAME = "controller_rpc_api";

} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_DEFS_H
