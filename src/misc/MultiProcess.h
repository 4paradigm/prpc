
#ifndef PARADIGM4_PICO_CORE_MULTIPROCESS_H
#define PARADIGM4_PICO_CORE_MULTIPROCESS_H

#include <cstdint>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include "pico_log.h"
#include "FileSystem.h"

namespace paradigm4 {
namespace pico {
namespace core {

// only for unit test
struct MultiProcess {
public:
    MultiProcess(size_t num_process, const std::string& root = "./.unittest_tmp/multi_process") {
        google::InstallFailureSignalHandler(); // 防止进程挂了都看不出来
        static char buffer[10240];
        SCHECK(getcwd(buffer, sizeof(buffer)) != nullptr) << errno;
        _cwd = buffer;
        _root = root;
        for (size_t i = 1; i < num_process; ++i) {
            SLOG(INFO) << "begin fork " << i << ' ' << num_process;
            pid_t pid = fork();
            SCHECK(pid >= 0);
            if (pid == 0) {
                SLOG(INFO) << "end fork " << i << ' ' << num_process << ' ' << ::getpid();
                _pids.clear();
                _index = i;
                break;
            } else {
                _pids.push_back(pid);
            }
        }
        if (_index == 0) {
            _wait_thread = std::thread(waiting, _pids);
        }
        if (!_root.empty()) {
            std::string workdir = core::format_string("%s/%d-%d", _root, _index, ::getpid());
            core::FileSystem::mkdir_p(core::URIConfig(workdir));
            SCHECK(chdir(workdir.c_str()) == 0);
        }
        SCHECK(getcwd(buffer, sizeof(buffer)) != nullptr) << errno;
        SLOG(INFO) << buffer;
    }

    MultiProcess(MultiProcess&& proc) = default;
    MultiProcess& operator=(MultiProcess&&) = default;

    MultiProcess(const MultiProcess& proc) = delete;
    MultiProcess& operator=(const MultiProcess&) = delete;

    ~MultiProcess() {
        if (_index == 0) {
            _wait_thread.join();
        } else {
            exit(0);
        }
        if (!_root.empty()) {
            SCHECK(chdir(_cwd.c_str()) == 0);
            core::FileSystem::rmrf(_root);
        }
    }

    size_t process_index()const {
        return _index;
    }

    static void waiting(const std::vector<pid_t>& pids) {
        for (pid_t pid: pids) {
            int ret_state = -1;
            SCHECK(waitpid(pid, &ret_state, 0) >= 0);
            SCHECK(WIFEXITED(ret_state));
        }
    }

    std::thread _wait_thread;

    size_t _index = 0;
    std::vector<pid_t> _pids;
    std::string _root;
    std::string _cwd;
};

}
}
}

#endif