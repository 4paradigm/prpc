#ifndef PARADIGM4_PICO_CORE_ACCUMULATORMANAGER_H
#define PARADIGM4_PICO_CORE_ACCUMULATORMANAGER_H

#include <mutex>
#include "AggregatorFactory.h"
#include "VirtualObject.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief class AccumulatorManager include a variable name(string) to id map
 *  use lock when access to map
 */
class AccumulatorManager : public core::NoncopyableObject {
public:

    static AccumulatorManager& singleton() {
        return am;
    }

    /*!
     * \brief get id of $name in map, alloc new id if $name not exisit
     */
    template<class AGG>
    void alloc_accumulator_id(const std::string name, size_t* id) {
        SCHECK(id != nullptr) << "id is nullptr";
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string type_name = AggregatorFactory::singleton().get_type_name<AGG>();
        
        auto fnd_iter = _umap_name2id.find(name);
        if (fnd_iter == _umap_name2id.end()) {
            *id = _vec_name_type.size();
            _umap_name2id[name] = *id;
            _vec_name_type.push_back(std::make_pair(name, type_name));
        } else {
            size_t tmp_id = fnd_iter->second;
            SCHECK(type_name == _vec_name_type[tmp_id].second) 
                    << "aggregator type mismatch for " << name 
                    << ", old type: " << _vec_name_type[tmp_id].second 
                    << ", new type: " << type_name;
            *id = tmp_id;
        }
    }

    bool get_id_by_name(const std::string& name, size_t& id) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto fnd_iter = _umap_name2id.find(name);
        if (fnd_iter == _umap_name2id.end()) {
            return false;
        }
        id = fnd_iter->second;
        return true;
    }

    size_t get_id_by_name(const std::string& name) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto fnd_iter = _umap_name2id.find(name);
        SCHECK(fnd_iter != _umap_name2id.end()) << "Cannot find id for accumulator " << name;
        return fnd_iter->second;
    }

    /*!
     * \brief get type of variable $name
     */
    bool get_typename_by_name(const std::string& name, std::string& type_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto fnd_iter = _umap_name2id.find(name);
        if (fnd_iter == _umap_name2id.end()) {
            return false;
        }
        type_name = _vec_name_type[fnd_iter->second].second;
        return true;
    }

    const std::string& get_typename_by_name(const std::string& name) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto fnd_iter = _umap_name2id.find(name);
        SCHECK(fnd_iter != _umap_name2id.end()) 
            << "Cannot find typename for accumulator " << name;
        return _vec_name_type[fnd_iter->second].second;
    }

    bool get_typename_by_id(const size_t id, std::string& type_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (id >= _vec_name_type.size()) {
            return false;
        }
        type_name = _vec_name_type[id].second;
        return true;
    }

    const std::string& get_typename_by_id(const size_t id) {
        std::lock_guard<std::mutex> lock(_mutex);
        SCHECK(id < _vec_name_type.size()) << "Accumulator id " << id << " exceeded";
        return _vec_name_type[id].second;
    }

    bool get_name_by_id(const size_t id, std::string& name) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (id >= _vec_name_type.size()) {
            return false;
        }
        name = _vec_name_type[id].first;
        return true;
    }

    const std::string& get_name_by_id(const size_t id) {
        std::lock_guard<std::mutex> lock(_mutex);
        SCHECK(id < _vec_name_type.size()) << "Accumulator id " << id << " exceeded";
        return _vec_name_type[id].first;
    }

    
private:
    static AccumulatorManager am;
    std::mutex _mutex;
    std::vector<std::pair<std::string, std::string>> _vec_name_type;
    std::unordered_map<std::string, size_t> _umap_name2id;

    AccumulatorManager() {
        _vec_name_type.clear();
        _umap_name2id.clear();
    }
    
};

}
} // pico
} // paradigm4
#endif // PARADIGM4_PICO_COMMON_ACCUMULATORMANAGER_H
