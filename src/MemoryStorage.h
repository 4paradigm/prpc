#ifndef PARADIGM4_PICO_DS_COMMON_MEMORY_STORAGE_H 
#define PARADIGM4_PICO_DS_COMMON_MEMORY_STORAGE_H 

#include <iostream>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <boost/any.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include "common.h"
#include "PicoContext.h"
#include "pico_log.h"
#include "URIConfig.h"

namespace paradigm4 {
namespace pico {

class MemoryStorage {
protected:
    enum OpenStatus {
        CLOSED,
        READ,
        WRITE
    };

    struct tree_t {
        //std::string name;
        boost::any value;
        std::map<std::string, tree_t> nodes;
        OpenStatus status = OpenStatus::CLOSED;
        int read_cnt = 0;
    };
public:
    static MemoryStorage& singleton() {
        static MemoryStorage mst;
        return mst;
    }

    void initialize() {}

    void finalize() {
        std::lock_guard<std::mutex> mlock(_mutex);
        _root = tree_t();
    }

    template<class T>
    T* open_read(const std::string& name) {
        std::lock_guard<std::mutex> mlock(_mutex);
        tree_t* node = get_tree(name);
        if (!node || node->status == OpenStatus::WRITE)
            return nullptr;
        try {
            auto t = boost::any_cast<std::shared_ptr<T>&>(node->value);
            node->status = OpenStatus::READ;
            ++node->read_cnt;
            return t.get();
        } catch (boost::bad_any_cast) {
            SLOG(WARNING) << "momory storage not exist " << name;
        }
        return nullptr;
    }

    template<class T>
    T* open_write(const std::string& name, const std::string& mode = "w") {
        //TODO mode w rw a
        std::shared_ptr<T> t;
        std::lock_guard<std::mutex> mlock(_mutex);
        tree_t* node = create_tree(name);
        if (node == nullptr)
            node = get_tree(name);
        if (node->status != OpenStatus::CLOSED)
            return nullptr;
        if (mode == "w" || node->value.empty()) {
            t = std::make_shared<T>();
            node->value = t;
        }
        node->status = OpenStatus::WRITE;
        return boost::any_cast<std::shared_ptr<T>&>(node->value).get();
    }

    void close(const std::string& name) {
        std::lock_guard<std::mutex> mlock(_mutex);
        tree_t* node = get_tree(name);
        if (!node)
            return;
        if (node->status == OpenStatus::WRITE) {
            node->status = OpenStatus::CLOSED;
        } else if (node->status == OpenStatus::READ) {
            if (!node->read_cnt || !--node->read_cnt)
                node->status = OpenStatus::CLOSED;
        }
    }

    bool exists(const std::string& path) {
        std::lock_guard<std::mutex> mlock(_mutex);
        return get_tree(path) != nullptr;
    }

    bool list(const std::string& path, std::vector<std::string>& files) {
        URIConfig uri(path);
        std::lock_guard<std::mutex> mlock(_mutex);
        auto t = get_tree(uri.name()) ;
        if(t == nullptr)return false;
        files.clear();
        for(auto &p:t->nodes){
            files.push_back(uri.append_uri("/"+p.first));
        }
        return true;
    }

    bool mkdir_p(const std::string& path) {
        std::lock_guard<std::mutex> mlock(_mutex);
        return create_tree(path) != nullptr;
    }

    bool rmr(const std::string& name) {
        std::lock_guard<std::mutex> mlock(_mutex);
        return delete_tree(name);
    }

protected:
    
    tree_t* get_tree(tree_t* root, const std::vector<std::string>& paths) {
        for (auto& p : paths) {
            auto it = root->nodes.find(p);
            if (it == root->nodes.end())
                return nullptr;
            root = &it->second;
        }
        return root;
    }
    
    tree_t* create_tree(tree_t* root, const std::vector<std::string>& paths) {
        for (size_t i = 0; i < paths.size(); ++i) {
            if (i == paths.size() - 1 && root->nodes.find(paths[i]) != root->nodes.end()) {
                return nullptr;
            }
            root = &root->nodes[paths[i]];
        }
        return root;
    }


    std::vector<std::string> split_path(const std::string& path) {
        std::string p = boost::trim_copy_if(path, boost::is_any_of("/\\\r\n\t "));
        std::vector<std::string> paths;
        boost::split(paths, p, boost::is_any_of("/\\"), boost::algorithm::token_compress_on);
        return paths;
    }

    tree_t* get_tree(const std::string &path) {
        auto paths = split_path(path);
        return get_tree(&_root, paths);
    }

    tree_t* create_tree(const std::string& path) {
        auto paths = split_path(path);
        return create_tree(&_root, paths);
    }

    bool delete_tree(const std::string& path) {
        auto paths = split_path(path);
        auto name = paths[paths.size() - 1];
        tree_t* root = &_root;
        if (paths.size() > 1) {
            paths.resize(paths.size() - 1);
            root = get_tree(root, paths);
        }
        if (!root)
            return false;
        auto it = root->nodes.find(name);
        if (it == root->nodes.end())
            return false;
        root->nodes.erase(it);
        return true;
    }

private:
    std::mutex _mutex;
    tree_t _root;
};


}
}

#endif //PARADIGM4_PICO_DS_COMMON_MEMORY_STORAGE_H 
