#include <csignal>
#include <sys/prctl.h>

#include "Master.h"

using namespace paradigm4::pico::core;

DEFINE_string(endpoint, "127.0.0.1", "ip or ip:port");

void show_flags_info() {
    std::vector<google::CommandLineFlagInfo> flag_infos;
    google::GetAllFlags(&flag_infos);
    std::sort(flag_infos.begin(), flag_infos.end(), [](const auto& a, const auto& b) {
        bool ret = a.is_default < b.is_default;
        if (a.is_default == b.is_default) {
            ret = a.name < b.name;
        }
        return ret;
    });
    size_t max_name_length = 0;
    for (auto& info : flag_infos) {
        max_name_length = std::max(info.name.length(), max_name_length);
    }

    SLOG(INFO) << "Program Configure(By Flags):";
    for (auto& info : flag_infos) {
        std::string name = std::string(max_name_length - info.name.length(), ' ') + info.name;
        if (info.is_default) {
            SLOG(INFO) << "(Default)    " << name << " : " << info.current_value;
        } else {
            SLOG(INFO) << "(UserSet)    " << name << " : " << info.current_value;
        }
    }
}

std::function<void(int)> sigint_handler;
void signal_handler(int sig) {
    sigint_handler(sig);
}

int main(int argc, char* argv[]) {
    // prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);

    //    std::signal(SIGINT, signal_handler);
    //    std::signal(SIGTERM, signal_handler);

    google::InstallFailureSignalHandler();
    google::InitGoogleLogging(argv[0]);
    google::AllowCommandLineReparsing();
    google::ParseCommandLineFlags(&argc, &argv, false);
    //FLAGS_logtostderr = 1;
    //LogReporter::initialize();
    //Configure config;

    //if (FLAGS_config_file != "") {
        //RCHECK(config.load_file(FLAGS_config_file)) << "load configure failed.";
    //}
    //RCHECK(pico_framework_configure().load_config(config["framework"]))
          //<< "load framework configure failed.";
    //show_flags_info();
    //SLOG(INFO) << "Pico Framework related configure:\n"
               //<< "framework :\n"
               //<< pico_framework_configure().value_info("    ");

    //std::string conn_config_str = FLAGS_connection_configure;
    //ConnectionConfig conn_config;
    //conn_config.from_json_str(conn_config_str);

    //std::string jstr;
    //conn_config.to_json_node().save(jstr);
    //SLOG(INFO) << "Connection Configure\n" << jstr;

    Master master(FLAGS_endpoint);
    master.initialize();
    sigint_handler = [&master](int) { master.exit(); };
    master.finalize();

    return 0;
}
