#ifndef PARADIGM4_PICO_CORE_LRUCACHE_H
#define PARADIGM4_PICO_CORE_LRUCACHE_H

#include <atomic>
#include <ctime>
#include <memory>
#include <unordered_map>

#include "SpinLock.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {


template<typename VALUE>
struct default_value_memory_usage_fn {
    size_t operator()(const VALUE& value) const {
        return sizeof(VALUE);
    }
};

// 线程安全的 LRUCache，max_item_count 是元素数量最大限制，max_item_memory_usage 是元素 memory usage 最大限制, 传入 0 表示没有限制。
template <typename KEY,
    typename VALUE,
    typename VALUE_MEMORY_USAGE_FN = default_value_memory_usage_fn<VALUE>,
    typename HASH = std::hash<KEY>,
    typename KEYEQUAL = std::equal_to<KEY>,
    typename ALLOCATOR = std::allocator<std::pair<const KEY, VALUE>>>
class LRUCache {
private:
    struct ListNode {
        ListNode() : _value_memory_usage(0), _prev(nullptr), _next(nullptr) {}

        explicit ListNode(const KEY& key) : _key(key), _value_memory_usage(0), _prev(nullptr), _next(nullptr) {}

        KEY _key;
        size_t _value_memory_usage;
        ListNode* _prev;
        ListNode* _next;
    };

    struct HashMapValue {
        HashMapValue() : _node(nullptr) {
            _access_count = std::make_shared<std::atomic<int>>(0);
        }

        HashMapValue(const VALUE& value, ListNode* node) : _value(value), _node(node) {
            _access_count = std::make_shared<std::atomic<int>>(0);
            _insert_timestamp_sec = std::time(nullptr);
        }

        VALUE _value;
        ListNode* _node;
        std::shared_ptr<std::atomic<int>> _access_count;
        int64_t _insert_timestamp_sec;
    };

public:
    explicit LRUCache(size_t max_item_count):
         _max_item_count(max_item_count), _max_item_memory_usage(0),
         _item_ttl_sec(0), _item_max_access_count(0) {}

    LRUCache(size_t max_item_count, size_t max_item_memory_usage,
             size_t item_ttl_sec, size_t item_max_access_count): 
        _max_item_count(max_item_count), _max_item_memory_usage(max_item_memory_usage),
         _item_ttl_sec(item_ttl_sec), _item_max_access_count(item_max_access_count) {}


    bool find(const KEY& key, VALUE& value) {
        shared_lock_guard<RWSpinLock> _(_lock);
        HashMapValue hash_value_accessor;
        auto it = _hashmap.find(key);
        if (it == _hashmap.end()) {
            return false;
        }
        hash_value_accessor = it->second;
        value = hash_value_accessor._value;
        ++(*(hash_value_accessor._access_count));
        if (_item_outdated(hash_value_accessor._access_count->load(), hash_value_accessor._insert_timestamp_sec)) {
            upgrade_guard<RWSpinLock> __(_lock);
            auto iter = _hashmap.find(key);
            if (iter != _hashmap.end() && _item_outdated(iter->second._access_count->load(), iter->second._insert_timestamp_sec)) {
                _evict(iter->second._node);
            }
        } else {
            bool upgraded = _lock.try_upgrade();
            if (upgraded) {
                ListNode* node = hash_value_accessor._node;
                _delink_node(node);
                _push_node_to_front(node);
                _lock.downgrade();
            }
        }
        return true;
    }

    void insert(const KEY& key, const VALUE& value) {
        lock_guard<RWSpinLock> _(_lock);
        ListNode* node = nullptr;
        auto it = _hashmap.find(key);
        if (it != _hashmap.end()) {
            node = it->second._node;
        } else {
            node = new ListNode(key);
        }
        if (_need_to_evict()) {
            _evict(_tail);
        }

        node->_value_memory_usage = VALUE_MEMORY_USAGE_FN()(value);
        HashMapValue hash_value_accessor(value, node);
        _hashmap.emplace(key, std::move(hash_value_accessor));
        _delink_node(node);
        _push_node_to_front(node);
        _item_count += 1;
        _item_memory_usage += node->_value_memory_usage;

        if (_need_to_evict()) {
            _evict(_tail);
        }
    }

    void clear() {
        lock_guard<RWSpinLock> _(_lock);
        _hashmap.clear();
        ListNode* node = _head;
        while(node != nullptr) {
            ListNode* cur = node;
            node = node->_next;
            delete cur;
        }
        _item_memory_usage = 0;
        _item_count = 0;
        _head = _tail = nullptr;
    }

    size_t item_count() {
        shared_lock_guard<RWSpinLock> _(_lock);
        return _item_count;
    }

    size_t item_memory_usage() {
        shared_lock_guard<RWSpinLock> _(_lock);
        return _item_memory_usage;
    }

private:
    void _evict(ListNode* node) {
        if (node == nullptr) {
            return;
        }
        _delink_node(node);
        _item_count -= 1;
        _item_memory_usage -= node->_value_memory_usage;
        _hashmap.erase(node->_key);
        delete node;
    }

    bool _item_outdated(size_t current_access_count, size_t insert_timestamp_sec) {
        // SLOG(INFO) << "current access count: " << current_access_count;
        // SLOG(INFO) << "max access count: " << _item_max_access_count;
        return (_item_max_access_count != 0 && current_access_count >= _item_max_access_count) ||
               (_item_ttl_sec != 0 && (int64_t(std::time(nullptr)) - insert_timestamp_sec) >= _item_ttl_sec);
    }

    // max_item_count 和 max_item_memory_usage 不是严格的限制。
    // 正好在边界时不会触发 evict，所以实际 size 可能大于限制。
    bool _need_to_evict() {
        // SLOG(INFO) << "max_item_count: " << _max_item_count;
        // SLOG(INFO) << "current item count: " << _item_count;
        // SLOG(INFO) << "max memory usage: " << _max_item_memory_usage;
        // SLOG(INFO) << "current memory usage: " << _item_memory_usage;
        return (_max_item_count !=0 && _item_count > _max_item_count) || 
            (_max_item_memory_usage !=0 && _item_memory_usage > _max_item_memory_usage);
    }

    void _delink_node(ListNode* node) {
        if (_tail == node) {
            _tail = _tail->_prev;
        }
        if (_head == node) {
            _head = _head->_next;
        }
        ListNode* prev = node->_prev;
        ListNode* next = node->_next;
        if (prev != nullptr) {
            prev->_next = next;
        }
        if (next != nullptr) {
            next->_prev = prev;
        }
        node->_prev = nullptr;
        node->_next = nullptr;
    }

    void _push_node_to_front(ListNode* node) {
        if (_head != nullptr) {
            _head->_prev = node;
        }
        node->_prev = nullptr;
        node->_next = _head;
        _head = node;
        if (_tail == nullptr) {
            _tail = _head;
        }
    }
private:
    RWSpinLock _lock;
    std::unordered_map<KEY, HashMapValue, HASH, KEYEQUAL,
          typename std::allocator_traits<ALLOCATOR>::template rebind_alloc<std::pair<const KEY, HashMapValue>> > _hashmap;

    ListNode* _head = nullptr;
    ListNode* _tail = nullptr;

    size_t _max_item_count;
    size_t _max_item_memory_usage;
    size_t _item_ttl_sec;
    size_t _item_max_access_count;
    size_t _item_count = 0;
    size_t _item_memory_usage = 0;
};


}  // namespace core
}  // namespace pico
}  // namespace paradigm4


#endif // PARADIGM4_PICO_CORE_LRUCACHE_H