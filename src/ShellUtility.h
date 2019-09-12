#ifndef PARADIGM4_PICO_COMMON_SHELL_UTILITY_H
#define PARADIGM4_PICO_COMMON_SHELL_UTILITY_H

#include <stdio.h>
#include <string>

#include "error_code.h"
#include "pico_log.h"
#include "Archive.h"

namespace paradigm4 {   
namespace pico {
namespace core {

enum class FileSystemType : int8_t {
    UNKNOWN,
    LOCAL, 
    HDFS,
    KAFKA,
    MEM
};

PICO_ENUM_SERIALIZATION(FileSystemType, int8_t);

enum class FileExtType { PZ, GZ, SNAPPY, OTHER };

struct bipopen_result_t {
    FILE* fp_read = nullptr;
    FILE* fp_write = nullptr;
    int pid = -1;
    std::string shell_bin = "";
    std::string shell_arg = "";
    std::string command = "";

    std::string gen_full_command() const {
        return "{" + shell_bin + (shell_arg == "" ? std::string("") : ("}{" + shell_arg))
               + "} [" + command + "]";
    }
};

struct popen_result_t {
    FILE* file = nullptr;
    bipopen_result_t result;
    std::string mode = "";
};

class ShellUtility : public VirtualObject {
public:
    ShellUtility() = delete;

    static void set_default_shell(const std::string& shell_bin, const std::string& shell_arg);
    /*!
     * \brief exec command cmd, return shell output file pointer
     */
    static shared_ptr<FILE> execute(const std::string& cmd) {
        return open(cmd, "r", true);
    }

    /*!
     * \brief exec command cmd, return result in string
     */
    static std::string execute_tostring(const std::string& cmd);

    static popen_result_t inner_popen(const std::string& cmd, const std::string& mode);

    static int inner_pclose(popen_result_t& r);

    static shared_ptr<FILE> open(const std::string& cmd,
          const std::string& mode,
          bool is_pipe = false);

    /*!
     * \brief check current file system, HDFS or local
     */
    static FileSystemType fs_type(const std::string& path_str) {
        std::string prefix;
        std::string name;
        return fs_type(path_str, prefix, name);
    }

    /**
     * @brief get file type, extra protocol prefix, path name
     *
     * @param path_str path name with protocol
     * @param prefix extra protocol prefix, for fs type
     * @param name the name of the actual use
     *
     * @return file type
     */
    static FileSystemType fs_type(const std::string& path_str,
          std::string& prefix,
          std::string& name);

    /**
     * @brief get file type protocol string
     *
     * @param type
     *
     * @return
     */
    static std::string fs_type_desc(FileSystemType type);

    /**
     * @brief check file ext name
     *
     * @param path_str
     *
     * @return ext type
     */
    static FileExtType ext_type(const std::string& path_str);

    /*！
     * \brief exec pipe_cmd and put result in path, return $path's file pointer,
     * just open $path file('w' mode) when pipe_cmd is empty
     * \param hdp 'hdfs dfs'
     */
    static shared_ptr<FILE> open_write(const std::string& path,
          const std::string& pipe_cmd,
          const std::string& hdp = "");

    /*!
     * \brief exec pipe_cmd with path as input, return output file pointer,
     *  just return path file pointer when pipe_cmd is empty
     */
    static shared_ptr<FILE> open_read(const std::string& path,
          const std::string& pipe_cmd,
          const std::string& hdp = "");

    /*!
     * \brief return file list under $reg_path
     */
    static std::vector<std::string> get_file_list(const std::string& reg_path);

    static std::vector<std::string> get_file_list(const std::vector<std::string>& reg_path);
    
    /*!
     * \brief return file list under hdfs path $reg_path
     */
    static std::vector<std::string> get_hdfs_file_list(const std::string& hadoop_bin,
          const std::string& reg_path);

    static std::vector<std::string> get_hdfs_file_list(const std::string& hadoop_bin,
	  	  const std::vector<std::string>& reg_path);
    
    /*!
     * \brief generate cmd to use origin_cmd's output as pipe_cmd's input
     */
    static void add_pipecmd(std::string& origin_cmd,
          bool& is_pipe,
          const std::string& pipe_cmd);

    static std::string read_file_to_string(FILE* ptr);

    /*!
     * \brief run pipe_cmd with file as input, return result as string
     */
    static std::string read_file_to_string(const std::string& file,
          const std::string& pipe_cmd = "",
          const std::string& hadoop = "");

    static void write_string_to_file(FILE* ptr, const std::string& );

    static void write_string_to_file(const std::string& file,
        const std::string& str,
        const std::string& pipe_cmd = "",
        const std::string& hadoop = "");

    static bipopen_result_t bipopen(const std::string& command,
          std::string shell_bin = "",
          std::string shell_arg = "");

    /*!
     * \brief get bipipe state until terminate
     */
    static int bipclose(bipopen_result_t& r);
private:
    static std::string default_shell_bin;
    static std::string default_shell_arg;
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_SHELL_UTILITY_H
