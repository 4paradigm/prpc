
#include "pico_log.h"
#include "error_code.h"
#include "ShellUtility.h"
#include "URIConfig.h"
#include "FileSystem.h"
#include "MemoryStorage.h"

namespace paradigm4 {
namespace pico {
namespace core {


std::string FileSystem::static_default_hadoop_bin = "";

std::string FileSystem::default_hadoop_bin() {
    return static_default_hadoop_bin;
}

void FileSystem::set_default_hadoop_bin(const std::string& value) {
    static_default_hadoop_bin = value;
}


std::vector <std::string> FileSystem::get_file_list(const std::string& path,
                                                          const std::string& hb) {
    URIConfig uri(path);
    uri.config().set_val(URI_HADOOP_BIN, hb);
    return get_file_list(uri);
}

std::vector <std::string> FileSystem::get_file_list(const URIConfig& uri) {
    std::string path = uri.name();
    std::string hb = uri.config().get_with_default<std::string>(URI_HADOOP_BIN, "");
    switch(uri.storage_type()) {
        case FileSystemType::LOCAL:
            return ShellUtility::get_file_list(path);
            break;
        case FileSystemType::HDFS:
            return ShellUtility::get_hdfs_file_list(current_hadoop_bin(hb), path);
            break;
        case FileSystemType::MEM: {
            std::vector<std::string> ret;
            MemoryStorage::singleton().list(path, ret);
            return ret;
            break;
        }
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << path;
            return {};
    }
}

void FileSystem::mkdir_p(const URIConfig& uri) {
    std::string path = uri.name();
    std::string hb = uri.config().get_with_default<std::string>(URI_HADOOP_BIN, "");
    switch(uri.storage_type()) {
        case FileSystemType::LOCAL:
            ShellUtility::execute("mkdir -p " + path);
            break;
        case FileSystemType::HDFS:
            ShellUtility::execute(current_hadoop_bin(hb) + " -mkdir -p " + path);
            break;
        case FileSystemType::MEM:
            MemoryStorage::singleton().mkdir_p(path);
            break;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << uri.uri();
            return;
    }
}

void FileSystem::mkdir_p(const std::string& path, const std::string& hb) {
    URIConfig uri(path);
    uri.config().set_val(URI_HADOOP_BIN, hb);
    mkdir_p(uri);
}

bool FileSystem::create_output_dir(const URIConfig& uri) {
    if (exists(uri)) {
        ELOG(WARNING, PICO_CORE_ERRCODE(FS_ITEM_EXISTS))
            << "Path \"" << uri.uri() << "\" already exists, cannot be used as output dir.";
        return false;
    }
    mkdir_p(uri);
    return true;
}

bool FileSystem::create_output_dir(const std::string& path, const std::string& hb) {
    URIConfig uri(path);
    uri.config().set_val(URI_HADOOP_BIN, hb);
    return create_output_dir(uri);
}

void FileSystem::rmr(const URIConfig& uri) {
    std::string path = uri.name();
    std::string hb = uri.config().get_with_default<std::string>(URI_HADOOP_BIN, "");
    switch(uri.storage_type()) {
        case FileSystemType::LOCAL:
            ShellUtility::execute("rm -r " + path);
            break;
        case FileSystemType::HDFS:
            ShellUtility::execute(current_hadoop_bin(hb) + " -rm -r " + path);
            break;
        case FileSystemType::MEM:
            MemoryStorage::singleton().rmr(path);
            break;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << uri.uri();
            return;
    }
}

void FileSystem::rmrf(const URIConfig& uri) {
    std::string path = uri.name();
    std::string hb = uri.config().get_with_default<std::string>(URI_HADOOP_BIN, "");
    switch(uri.storage_type()) {
        case FileSystemType::LOCAL:
            ShellUtility::execute("rm -rf " + path);
            break;
        case FileSystemType::HDFS:
            ShellUtility::execute(current_hadoop_bin(hb) + " -rm -r -f " + path);
            break;
        case FileSystemType::MEM:
            MemoryStorage::singleton().rmr(path);
            break;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << uri.uri();
            return;
    }

}

bool FileSystem::exists(const URIConfig& uri) {
    std::string path = uri.name();
    std::string hb = uri.config().get_with_default<std::string>(URI_HADOOP_BIN, "");
    switch(uri.storage_type()) {
        case FileSystemType::LOCAL:
            return std::stoi(ShellUtility::execute_tostring("test -e " + path + "; echo $?"))
                == 0 ? true : false;
        case FileSystemType::HDFS:
            return std::stoi(ShellUtility::execute_tostring(current_hadoop_bin(hb) 
                        + " -test -e " + path + "; echo $?")) 
                == 0 ? true : false;
        case FileSystemType::MEM:
            return MemoryStorage::singleton().exists(path);
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << uri.uri();
            return false;
    }
}

bool FileSystem::is_file(const std::string& path, const std::string& hb) {
    switch(ShellUtility::fs_type(path)) {
        case FileSystemType::LOCAL:
            return std::stoi(ShellUtility::execute_tostring("test -f " + path + "; echo $?"))
                == 0 ? true : false;
        case FileSystemType::HDFS:
            return std::stoi(ShellUtility::execute_tostring(current_hadoop_bin(hb)
                        + " -test -f " + path + "; echo $?")) 
                == 0 ? true : false;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << path;
            return false;
    }

}

bool FileSystem::is_directory(const std::string& path, const std::string& hb) {
    switch(ShellUtility::fs_type(path)) {
        case FileSystemType::LOCAL:
            return std::stoi(ShellUtility::execute_tostring("test -d " + path + "; echo $?"))
                == 0 ? true : false;
        case FileSystemType::HDFS:
            return std::stoi(ShellUtility::execute_tostring(current_hadoop_bin(hb)
                        + " -test -d " + path + "; echo $?")) 
                == 0 ? true : false;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << path;
            return false;
    }

}

void FileSystem::mv(const std::string& src_path,
                          const std::string& dst_path,
                          const std::string& hb) {
    auto src_type = ShellUtility::fs_type(src_path);
    auto dst_type = ShellUtility::fs_type(dst_path);
    RCHECK(src_type == dst_type) << "file type mismatch between source ["
                                 << src_path << "] and destination [" << dst_path << "] for mv operator";
    switch(src_type) {
        case FileSystemType::LOCAL:
            ShellUtility::execute("mv " + src_path + " " + dst_path);
            break;
        case FileSystemType::HDFS:
            ShellUtility::execute(current_hadoop_bin(hb) + " -mv " + src_path + " " + dst_path);
            break;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE))
                << "unkown fs_type of " << src_path << " and " << dst_path;
            return;
    }

}

void FileSystem::mvf(const std::string& src_path,
                           const std::string& dst_path,
                           const std::string& hb) {
    auto src_type = ShellUtility::fs_type(src_path);
    auto dst_type = ShellUtility::fs_type(dst_path);
    RCHECK(src_type == dst_type) << "file type mismatch between source ["
                                 << src_path << "] and destination [" << dst_path << "] for mvf operator";
    switch(src_type) {
        case FileSystemType::LOCAL:
            ShellUtility::execute("rm -rf " + dst_path + "; mv " + src_path + " " + dst_path);
            break;
        case FileSystemType::HDFS:
            ShellUtility::execute(current_hadoop_bin(hb) + " -rm -r -f " + dst_path + "; "
                + current_hadoop_bin(hb) + " -mv " + src_path + " " + dst_path);
            break;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE)) << "unkown fs_type of " << src_path << " and " << dst_path;
            return;
    }

}

void FileSystem::copy(const std::string& src_path,
                            const std::string& dst_path,
                            const std::string& hb) {
    auto src_type = ShellUtility::fs_type(src_path);
    auto dst_type = ShellUtility::fs_type(dst_path);
    RCHECK(src_type == dst_type) << "file type mismatch between source ["
                                 << src_path << "] and destination [" << dst_path << "] for mvf operator";
    switch(src_type) {
        case FileSystemType::LOCAL:
            ShellUtility::execute("rm -rf " + dst_path + "; cp -r " + src_path + " " + dst_path);
            break;
        case FileSystemType::HDFS:
            ShellUtility::execute(current_hadoop_bin(hb) + " -rm -r -f " + dst_path + "; "
                + current_hadoop_bin(hb) + " -cp " + src_path + " " + dst_path);
            break;
        default:
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_UNKNOWN_TYPE)) << "unkown fs_type of " << src_path << " and " << dst_path;
            return;
    }
}

bool FileSystem::exists(const std::string& path, const std::string& hb) {
    URIConfig uri(path);
    uri.config().set_val(URI_HADOOP_BIN, hb);
    return exists(uri);
}

void FileSystem::rmr(const std::string& path, const std::string& hb) {
    URIConfig uri(path);
    uri.config().set_val(URI_HADOOP_BIN, hb);
    rmr(uri);
}

void FileSystem::rmrf(const std::string& path, const std::string& hb) {
    URIConfig uri(path);
    uri.config().set_val(URI_HADOOP_BIN, hb);
    rmrf(uri);
}

}
} // namespace pico
} // namespace paradigm4

