#ifndef PARADIGM4_PICO_COMMON_CONFIGURE_H
#define PARADIGM4_PICO_COMMON_CONFIGURE_H

#include <yaml-cpp/yaml.h>
#include <boost/lexical_cast.hpp>

#include "pico_log.h"
#include "error_code.h"
#include "ShellUtility.h"

namespace paradigm4 {
namespace pico {
namespace core {

class Configure {
public:
    Configure() = default;

    Configure(const YAML::Node& node, const std::string& key) {
        _node.reset(node);
        _key = key;
    }

    Configure(const std::string& filename, const std::string& pipe_cmd = "",
            const std::string& hadoop = "") {
        if (!load_file(filename, pipe_cmd, hadoop)) {
            RLOG(WARNING) << "construct Configure from file failed";
        }
    }

    Configure(const Configure& cfg) {
        _node.reset(cfg._node);
        _key = cfg._key;
    }

    Configure& operator=(const Configure& cfg) {
        _node.reset(cfg._node);
        _key = cfg._key;
        return *this;
    }

    /*!
     * \brief open $filename, if pipe_cmd is not empty, filter $filename content with it
     */
    bool load_file(const std::string& filename, const std::string& pipe_cmd = "",
            const std::string& hadoop = "") {
        shared_ptr<FILE> ptr = ShellUtility::open_read(filename, pipe_cmd, hadoop);
        if (ptr == nullptr) {
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_FILE_OPEN)) 
                << "loading file \"" << filename << "\" pipe_cmd: \""
                << pipe_cmd << "\" hadoop_bin: \"" << hadoop << "\" failed";
            return false;
        }
        return load(ShellUtility::read_file_to_string(ptr.get()));
    }
    
    /*!
     * \brief convert string into yaml node
     */
    bool load(const std::string& input) {
        try {
            _node = YAML::Load(input);
            _key.clear();
        } catch(std::exception& e) {
            ELOG(WARNING, PICO_CORE_ERRCODE(CFG_LOAD)) 
                << "fail to load configure from input:\n" 
                << input << "\n\n std::exception:" << e.what();
            return false;
        }
        return true;
    }

    /*!
     * \brief open $filename in 'w' mode, if pipe_cmd is not empty, run pipe_cmd > filename, then
     *  open filename
     */
    bool save(const std::string& filename, const std::string& pipe_cmd,
            const std::string& hadoop) const {
        shared_ptr<FILE> file = ShellUtility::open_write(filename, pipe_cmd, hadoop);
        if (file == nullptr) {
            ELOG(WARNING, PICO_CORE_ERRCODE(FS_FILE_OPEN)) 
                << "open file \"" << filename << "\" pipe_cmd: \""
                << pipe_cmd << "\" hadoop_bin: \"" << hadoop << "\" failed";
            return false;
        }
        std::string out_str = dump();
        if (fwrite(out_str.c_str(), sizeof(char), out_str.size(), file.get()) != out_str.size()) {
            ELOG(WARNING, PICO_CORE_ERRCODE(CFG_SAVE)) << "write configure to \"" 
                << filename << "\" failed: write file failed";
            return false;
        }
        return true;
    }

    std::string dump() const {
        return YAML::Dump(_node);
    }

    const std::string& key() const {
        return _key;
    }

    std::string& key() {
        return _key;
    }

    const YAML::Node& node() const {
        return _node;
    }

    YAML::Node& node() {
        return _node;
    }

    size_t size() const {
        return _node.size();
    }

    YAML::NodeType::value type() const {
        return _node.Type();
    }

    /*!
     * \brief if yaml node is not defined, return true
     */
    bool operator!() const {
        return !_node.IsDefined();
    }

    /*!
     * \brief check if yaml node has key $key
     */
    bool has(const std::string& key) const {
        return _node.IsDefined() && _node[key].IsDefined();
    }

    bool has(size_t i) const {
        return _node.IsDefined() && _node[i].IsDefined();
    }

    /*!
     * \brief return next level of yaml node
     */
    Configure operator[](size_t i) const {
        std::string newkey = _key + "[" + boost::lexical_cast<std::string>(i) + "]";
        if (!has(i)) {
            SLOG(WARNING) << "unkown configure key: " << newkey;
            return {YAML::Node(), newkey};
        }
        return {_node[i], newkey};
    }

    Configure operator[](const std::string& key) const {
        std::string newkey = _key + "[\"" + key + "\"]";
        if (!has(key)) {
//            SLOG(WARNING) << "unkown configure key: " << newkey;
            return {YAML::Node(), newkey};
        }
        return {_node[key], newkey};
    }

    bool is_valid() const {
        return _node.IsDefined();
    }

    template<class T>
    std::enable_if_t<!std::is_same<Configure, T>::value, bool> try_as(T& val) const {
        try {
            std::string str = _node.as<std::string>();
            return pico_lexical_cast(str, val);
        } catch(std::exception& e) {
            SLOG(WARNING) 
                << "read configure key = {" << _key << "} failed. EXCEPTION:" << e.what();
            return false;
        }
    }

    template<class T>
    std::enable_if_t<std::is_same<Configure, T>::value, bool> try_as(T& val) const {
        val = *this;
        return true;
    }

    template<typename T>
    using IsGloggable = core::IsGloggable<T>;
    template<class T>
    std::enable_if_t<IsGloggable<T>::value, T> get(const std::string& name, const T& dftv) const {
        if (has(name)) {
            return (*this)[name].as<T>();
        } else {
            SLOG(WARNING) << "cannot find the configure item " << _key << "[\""
                         << name << "\"], now set to default value: "
                         << dftv;
            return dftv;
        }
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value, T> get(const std::string& name, const T& dftv) const {
        if (has(name)) {
            return (*this)[name].as<T>();
        } else {
            SLOG(WARNING) << "cannot find the configure item " << _key << "[\""
                         << name << "\"], now set to default value: N/A(NON-LOGGABLE TYPE)";
            return dftv;
        }
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value, T> get(size_t i, const T& deft_val) const {
        if (has(i)) {
            return (*this)[i].as<T>();
        } else {
            SLOG(WARNING) << "cannot find the configure item " << _key << "["
                         << i << "], now set to default value: "
                         << deft_val;
            return deft_val;
        }
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value, T> get(size_t i, const T& deft_val) const {
        if (has(i)) {
            return (*this)[i].as<T>();
        } else {
            SLOG(WARNING) << "cannot find the configure item " << _key << "["
                         << i << "], now set to default value: N/A(NON-LOGGABLE TYPE)";
            return deft_val;
        }
    }

    template<class T>
    std::enable_if_t<!std::is_same<Configure, T>::value, T> as() const {
        try {
            return pico_lexical_cast<T>(_node.as<std::string>());
        } catch (std::exception& e) {
            ELOG(FATAL, PICO_CORE_ERRCODE(CFG_PARSE)) 
                << "read configure key = {" << _key << "} failed. EXCEPTION:" << e.what();
            return T();
        }
    }

    template<class T>
    std::enable_if_t<std::is_same<Configure, T>::value, T> as() const {
        return *this;
    }

    template<class T>
    void set(const std::vector<std::string>& key, const T& val) {
        std::vector<YAML::Node> nodes;
        nodes.reserve(key.size());
        // TODO support array
        nodes.push_back(_node[key[0]]);
        for (size_t i = 1; i < key.size(); ++i) {
            nodes.push_back(nodes.back()[key[i]]);
        }
        nodes.back() = val;
    }

private:
    YAML::Node _node;
    std::string _key;
};

}
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_CONFIGURE_H
