#ifndef PARADIGM4_PICO_COMMON_FILE_SYSTEM_H
#define PARADIGM4_PICO_COMMON_FILE_SYSTEM_H

#include <vector>
#include <string>
#include "URIConfig.h"

namespace paradigm4 {
namespace pico {
namespace core {

const char* const URI_HADOOP_BIN = "hadoop_bin";

/*!
 * \brief shell cmd deal with file
 */
class FileSystem {
public:
    FileSystem() = delete;

    static std::string default_hadoop_bin();

    static void set_default_hadoop_bin(const std::string&);

    static std::string current_hadoop_bin(const std::string& hb) {
        return hb == "" ? default_hadoop_bin() : hb;
    }

    static std::vector<std::string> get_file_list(const std::string& path,
            const std::string& hb = "");

    static std::vector<std::string> get_file_list(const URIConfig& uri);

    static void mkdir_p(const std::string& path, const std::string& hb = "");

    static void mkdir_p(const URIConfig& uri);

    static bool create_output_dir(const std::string& path, const std::string& hb = "");

    static bool create_output_dir(const URIConfig& uri);

    static void rmr(const std::string& path, const std::string& hb = "");

    static void rmr(const URIConfig& uri);

    static void rmrf(const std::string& path, const std::string& hb = "");

    static void rmrf(const URIConfig& uri);

    static bool exists(const std::string& path, const std::string& hb = "");

    static bool exists(const URIConfig& uri);

    static bool is_directory(const std::string& path, const std::string& hb = "");

    static bool is_file(const std::string& path, const std::string& hb = "");
    
    static void mv(const std::string& src_path, const std::string& dst_path, const std::string& hb = "");
    
    // equiv to: rm -rf dst_path; mv src_path dst_path
    static void mvf(const std::string& src_path, const std::string& dst_path, const std::string& hb = "");

    static void copy(const std::string& src_path, const std::string& dst_path, const std::string& hb = "");

private:
    static std::string static_default_hadoop_bin;

};

}
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_FILE_SYSTEM_H
