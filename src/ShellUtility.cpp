#include <fcntl.h>
#include <istream>
#include <memory>
#include <sched.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "ShellUtility.h"

namespace paradigm4 {
namespace pico {
namespace core {

std::string ShellUtility::default_shell_bin = "/bin/bash";
std::string ShellUtility::default_shell_arg = "-c";

void ShellUtility::set_default_shell(
      const std::string& shell_bin,
      const std::string& shell_arg) {
    default_shell_bin = shell_bin;
    default_shell_arg = shell_arg; 
}

std::string ShellUtility::execute_tostring(const std::string& cmd) {
    shared_ptr<FILE> file = execute(cmd);
    if (file == nullptr) {
        return "";
    }
    FileLineReader reader;
    char* line = reader.getdelim(file.get(), '\0');
    return line == nullptr ? "" : line;
}

popen_result_t ShellUtility::inner_popen(const std::string& cmd,
      const std::string& mode) {
    popen_result_t ret;
    ret.result = bipopen(cmd, default_shell_bin, default_shell_arg);
    ret.mode = boost::algorithm::trim_copy(mode);
    SCHECK(ret.mode != "") << "popen mode cannot be empty";

    if (ret.mode == "r") {
        fclose(ret.result.fp_write);
        ret.result.fp_write = nullptr;
        ret.file = ret.result.fp_read;
    } else if (ret.mode == "w") {
        fclose(ret.result.fp_read);
        ret.result.fp_read = nullptr;
        ret.file = ret.result.fp_write;
    } else {
        SLOG(FATAL) << "invalid popen mode: " << ret.mode;
    }

    return ret;
}

int ShellUtility::inner_pclose(popen_result_t& r) {
    if (r.mode == "r") {
        fclose(r.result.fp_read);
        r.result.fp_read = nullptr;
    } else if (r.mode == "w") {
        fclose(r.result.fp_write);
        r.result.fp_write = nullptr;
    } else {
        SLOG(FATAL) << "invalid popen mode: " << r.mode;
    }
    r.file = nullptr;

    return bipclose(r.result);
}

shared_ptr<FILE> ShellUtility::open(const std::string& cmd,
      const std::string& mode,
      bool is_pipe) {
    SLOG(INFO) << "opening " << (is_pipe ? "pipe: \"" : "file: \"") << cmd
               << "\" mode : " << mode;
    FILE* file = nullptr;
    if (!is_pipe) {
        file = fopen(cmd.c_str(), mode.c_str());
        if (file == nullptr) {
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_FILE_OPEN))
                  << "fail to open " << cmd << " mode : " << mode;
        }
        return shared_ptr<FILE>(file, [=](FILE* p) {
            if (p != nullptr) {
                int ret = fclose(p);
                ECHECK(ret == 0, PICO_CORE_ERRCODE(FS_FILE_CLOSE))
                      << "closing file \"" << cmd << "\" mode : " << mode << " failed with "
                      << ret << " . " << strerror(ret);
                SLOG(INFO) << "file \"" << cmd << "\" mode : " << mode << " closed with "
                           << ret << " . " << strerror(ret);
            }
        });
    } else {
        std::string pipe_cmd = cmd;
        popen_result_t ret = inner_popen(pipe_cmd, mode);
        return shared_ptr<FILE>(ret.file, [=](FILE* p) mutable {
            if (p != nullptr) {
                bool eof = feof(p);
                int error = inner_pclose(ret);
                if (eof) {
                    PECHECK(error == 0, PICO_CORE_ERRCODE(FS_PIPE_CLOSE))
                        << "closing pipe \"" << cmd << "\" mode : " << mode << " failed with "
                        << error << " . " << strerror(error);
                }
                if (error == 0) {
                    SLOG(INFO) << "pipe \"" << cmd << "\" mode : " << mode << " closed with "
                        << error << " . " << strerror(error);               
                } else {
                    SLOG(WARNING) << "pipe \"" << cmd << "\" mode : " << mode << " interrupted with "
                        << error << " . " << strerror(error);
                }
            }
        });
    }
}

FileSystemType ShellUtility::fs_type(const std::string& path_str,
      std::string& prefix,
      std::string& name) {
    std::string path = path_str;
    boost::algorithm::trim_if(path, boost::algorithm::is_any_of(" \n\r\t"));
    size_t name_pos = 0;
    FileSystemType type;
    if (strncmp(path.c_str(), "hdfs://", 7) == 0) {
        type = FileSystemType::HDFS;
        name_pos = 0;
    } else if (strncmp(path.c_str(), "s3://", 5) == 0) {
        type = FileSystemType::HDFS;
        name_pos = 0;
    } else if (strncmp(path.c_str(), "s3n://", 6) == 0) {
        type = FileSystemType::HDFS;
        name_pos = 0;
    } else if (strncmp(path.c_str(), "s3a://", 6) == 0) {
        type = FileSystemType::HDFS;
        name_pos = 0;
    } else if (strncmp(path.c_str(), "mem://", 6) == 0) {
        type = FileSystemType::MEM;
        name_pos = 6;
    } else if (strncmp(path.c_str(), "file://", 7) == 0) {
        type = FileSystemType::LOCAL;
        name_pos = 7;
    } else if (strncmp(path.c_str(), "kafka://", 8) == 0) {
        type = FileSystemType::KAFKA;
        name_pos = 8;
    } else if (path.find("://") == path.npos) {
        type = FileSystemType::LOCAL;
        name_pos = 0;
    } else {
        name_pos = path.find("://") + 3;
        type = FileSystemType::UNKNOWN;
    }
    prefix = path_str.substr(0, name_pos);
    name = path_str.substr(name_pos);
    return type;
}

std::string ShellUtility::fs_type_desc(FileSystemType type) {
    switch (type) {
        case FileSystemType::HDFS:
            return "hdfs://|s3://|s3n:|s3a://";
        case FileSystemType::LOCAL:
            return "|file://";
        case FileSystemType::MEM:
            return "mem://";
        case FileSystemType::KAFKA:
            return "kafka://";
        default:
            return "unknown";
    }
}

FileExtType ShellUtility::ext_type(const std::string& path_str) {
    std::string path = path_str;
    boost::algorithm::trim_if(path, boost::algorithm::is_any_of(" \n\r\t"));
    const char* end = path.c_str() + path.length();
    if (path.length() > 3 && strncmp(end - 3, ".gz", 3) == 0) {
        return FileExtType::GZ;
    } else if (path.length() > 3 && strncmp(end - 3, ".pz", 3) == 0) {
        return FileExtType::PZ;
    } else if (path.length() > 7 && strncmp(end - 7, ".snappy", 7) == 0) {
        return FileExtType::SNAPPY;
    } else {
        return FileExtType::OTHER;
    }
}

shared_ptr<FILE> ShellUtility::open_write(const std::string& path,
      const std::string& pipe_cmd,
      const std::string& hdp) {
    FileSystemType type = fs_type(path);
    std::string cmd;
    bool is_pipe = false;
    switch (type) {
    case FileSystemType::LOCAL: {
        if (pipe_cmd != "") {
            cmd = pipe_cmd + " > " + path;
            is_pipe = true;
        } else {
            cmd = path;
        }
    } break;
    case FileSystemType::HDFS: {
        std::string put_cmd = hdp + " -put - " + path;
        is_pipe = true;
        if (pipe_cmd != "") {
            cmd = pipe_cmd;
            add_pipecmd(cmd, is_pipe, put_cmd);
        } else {
            cmd = put_cmd;
        }
    } break;
    default:
        ELOG(FATAL, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
              << "unkown file system of [" << path << "]";
    }
    return open(cmd, "w", is_pipe);
}

shared_ptr<FILE> ShellUtility::open_read(const std::string& path,
      const std::string& pipe_cmd,
      const std::string& hdp) {
    FileSystemType type = fs_type(path);
    FileExtType ext = ext_type(path);
    std::string cmd;
    bool is_pipe = false;
    switch (type) {
    case FileSystemType::LOCAL: {
        if (pipe_cmd != "") {
            cmd = path;
            add_pipecmd(cmd, is_pipe, pipe_cmd);
            is_pipe = true;
        } else {
            cmd = path;
        }
    } break;
    case FileSystemType::HDFS: {
        std::string read_cmd;
        switch (ext) {
        case FileExtType::PZ:
        case FileExtType::GZ:
        case FileExtType::SNAPPY:
            read_cmd = hdp + " -text " + path;
            break;
        default:
            read_cmd = hdp + " -cat " + path;
        }
        is_pipe = true;
        if (pipe_cmd != "") {
            cmd = read_cmd;
            add_pipecmd(cmd, is_pipe, pipe_cmd);
        } else {
            cmd = read_cmd;
        }
    } break;
    default:
        ELOG(FATAL, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
              << "unkown file system of [" << path << "]";
    }
    return open(cmd, "r", is_pipe);
}


std::vector<std::string> pico::ShellUtility::get_file_list(
      const std::vector<std::string>& reg_path) {
    std::vector<std::string> file_list;
    for (auto& path : reg_path) {
        std::vector<std::string> tmp = get_file_list(path);
        core::vector_move_append<std::string>(file_list, tmp);
    }
    return file_list;
}

std::vector<std::string> ShellUtility::get_file_list(const std::string& reg_path) {
    std::string path
          = boost::algorithm::trim_copy_if(reg_path, boost::algorithm::is_any_of("\t\r\n "));
    if (path == "") {
        RLOG(WARNING) << "path is empty";
        return {};
    }
    auto pipe = execute(format_string("find %s -type f", path.c_str()));
    RCHECK(pipe != nullptr) << "find " << reg_path << " -type f failed.";
    FileLineReader reader;
    char* file_name = nullptr;
    std::vector<std::string> file_list;
    while ((file_name = reader.getline(pipe.get()))) {
        file_list.push_back(file_name);
    }
    if (file_list.size() == 0) {
        RLOG(WARNING) << "there's no file in path \"" << reg_path << "\"";
    }
    return file_list;
}

std::vector<std::string> ShellUtility::get_hdfs_file_list(const std::string& hadoop_bin,
      const std::string& reg_path) {
    // remove trailing and leading \t\r\n space from reg_path
    std::string path
          = boost::algorithm::trim_copy_if(reg_path, boost::algorithm::is_any_of("\t\r\n "));
    if (path == "") {
        RLOG(WARNING) << "path is empty";
        return {};
    }
    auto pipe = execute(
          format_string("%s -ls %s | grep -v ^Found | awk '{print $NF}'", hadoop_bin, path));
    RCHECK(pipe != nullptr) << hadoop_bin << " -ls " << reg_path << " failed.";
    FileLineReader reader;
    char* file_name = nullptr;
    std::vector<std::string> file_list;
    while ((file_name = reader.getline(pipe.get()))) {
        file_list.push_back(file_name);
    }
    if (file_list.size() == 0) {
        RLOG(WARNING) << "there's no file in path \"" << reg_path << "\"";
    }
    return file_list;
}

std::vector<std::string> pico::ShellUtility::get_hdfs_file_list(const std::string& hadoop_bin,
      const std::vector<std::string>& reg_path) {
    std::vector<std::string> file_list;
    for (auto& path : reg_path) {
        std::vector<std::string> tmp = get_hdfs_file_list(hadoop_bin, path);
        core::vector_move_append<std::string>(file_list, tmp);
    }

    return file_list;
}

          
void ShellUtility::add_pipecmd(std::string& origin_cmd,
      bool& is_pipe,
      const std::string& pipe_cmd) {
    std::string ipipe_cmd
          = boost::algorithm::trim_copy_if(pipe_cmd, boost::algorithm::is_any_of("\t\r\n "));
    if (ipipe_cmd == "") {
        return;
    }
    if (!is_pipe) {
        origin_cmd = format_string("( %s ) < \"%s\"", ipipe_cmd.c_str(), origin_cmd.c_str());
        is_pipe = true;
    } else {
        origin_cmd = format_string("%s | %s", origin_cmd.c_str(), ipipe_cmd.c_str());
    }
}

std::string ShellUtility::read_file_to_string(FILE* ptr) {
    FileLineReader reader;
    std::string out_str = "";

    while (reader.getline(ptr, false)) {
        out_str.append(reader.buffer(), reader.size());
    }
    return out_str;
}

std::string ShellUtility::read_file_to_string(const std::string& file,
      const std::string& pipe_cmd,
      const std::string& hadoop) {
    shared_ptr<FILE> ptr = open_read(file, pipe_cmd, hadoop);
    return read_file_to_string(ptr.get());
}

void ShellUtility::write_string_to_file(FILE* ptr, const std::string& str) {
    fprintf(ptr, "%s", str.c_str());
}

void ShellUtility::write_string_to_file(const std::string& file,
    const std::string& str,
    const std::string& pipe_cmd,
    const std::string& hadoop) {
    shared_ptr<FILE> ptr = open_write(file, pipe_cmd, hadoop);
    write_string_to_file(ptr.get(),str);
}

bipopen_result_t ShellUtility::bipopen(const std::string& command,
      std::string shell_bin,
      std::string shell_arg) {
    if (shell_bin == "") {
        shell_bin = default_shell_bin;
    }
    if (shell_arg == "") {
        shell_arg = default_shell_arg;
    }

    std::lock_guard<std::mutex> mlock(global_fork_mutex());
    bipopen_result_t ret;
    int fd_read[2];
    int fd_write[2];
    ret.command = boost::algorithm::trim_copy(command);
    ret.shell_bin = boost::algorithm::trim_copy(shell_bin);
    ret.shell_arg = boost::algorithm::trim_copy(shell_arg);
    SCHECK(ret.command != "") << "command is empty";
    SCHECK(ret.shell_bin != "") << "Shell Bin cannot be emtpy";
#ifdef __APPLE__
    PSCHECK(pipe(fd_read) == 0) << "create read pipe failed";
    fcntl(fd_read[0], O_CLOEXEC);
    fcntl(fd_read[1], O_CLOEXEC);
    PSCHECK(pipe(fd_write) == 0) << "create write pipe failed";
    fcntl(fd_write[0], O_CLOEXEC);
    fcntl(fd_write[1], O_CLOEXEC);
#else
    PSCHECK(pipe2(fd_read, O_CLOEXEC) == 0) << "create read pipe failed";
    PSCHECK(pipe2(fd_write, O_CLOEXEC) == 0) << "create write pipe failed";
#endif
    ret.pid = vfork();

    PECHECK(ret.pid >= 0, PICO_CORE_ERRCODE(COMM_FORK_PROC)) << "fork failed";
    if (ret.pid == 0) { //child process
        PSCHECK(prctl(PR_SET_PDEATHSIG, SIGHUP) == 0) << "set process control failed.";
        PSCHECK(setpgid(0, 0) == 0) << "set pgid failed";
        if (STDOUT_FILENO != fd_read[1]) {
            SCHECK(dup2(fd_read[1], STDOUT_FILENO) != -1) << "dup2 failed.";
            close(fd_read[1]);
        }

        close(fd_read[0]);

        if (STDIN_FILENO != fd_write[0]) {
            SCHECK(dup2(fd_write[0], STDIN_FILENO) != -1) << "dup2 failed.";
            close(fd_write[0]);
        }

        close(fd_write[1]);

        if (ret.shell_arg != "") {
            execl(ret.shell_bin.c_str(),
                  ret.shell_bin.c_str(),
                  ret.shell_arg.c_str(),
                  ret.command.c_str(),
                  nullptr);
        } else {
            execl(ret.shell_bin.c_str(), ret.shell_bin.c_str(), ret.command.c_str(), nullptr);
        }

        _exit(errno);
    } else {
        close(fd_read[1]);
        close(fd_write[0]);

        ret.fp_read = fdopen(fd_read[0], "r");
        SCHECK(ret.fp_read != nullptr) << "fdopen failed.";

        ret.fp_write = fdopen(fd_write[1], "w");
        SCHECK(ret.fp_write != nullptr) << "fdopen failed.";
    }
    BLOG(1) << "bipopen " << ret.gen_full_command() << " success.";
    return ret;
}

int ShellUtility::bipclose(bipopen_result_t& r) { // won't close FILE*
    std::lock_guard<std::mutex> mlock(global_fork_mutex());
    int ret_state = -1, result = -1;
    do {
        //kill(r.pid, SIGKILL);
        SCHECK(waitpid(r.pid, &ret_state, 0) >= 0) << "waitpid failed";
        if (WIFEXITED(ret_state)) {
            result = WEXITSTATUS(ret_state);
            BLOG(1) << "bipipe " << r.gen_full_command() << " closed with: " << result
                       << " . " << strerror(result);
        } else if (WIFSIGNALED(ret_state)) {
            RLOG(WARNING) << "bipipe " << r.gen_full_command()
                          << " signaled by: " << strsignal(WTERMSIG(ret_state));
            result = WTERMSIG(ret_state);
        } else if (WIFSTOPPED(ret_state)) {
            RLOG(WARNING) << "bipipe " << r.gen_full_command()
                          << " stopped by signal: " << strsignal(WSTOPSIG(ret_state));
        } else if (WIFCONTINUED(ret_state)) {
            RLOG(WARNING) << "bipipe " << r.gen_full_command() << " continued.";
        }
    } while (!WIFEXITED(ret_state) && !WIFSIGNALED(ret_state));

    return result;
}

} // namespace core
} // namespace pico
} // namespace paradigm4
