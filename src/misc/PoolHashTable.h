#ifndef PARADIGM4_PICO_CORE_INCLUDE_BLOCK_ALLOCATOR_H
#define PARADIGM4_PICO_CORE_INCLUDE_BLOCK_ALLOCATOR_H

#include "pico_log.h"
#include "VirtualObject.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <vector>

namespace paradigm4 {
namespace pico {
namespace core {

class MemoryMap {
public:
    MemoryMap() {}

    ~MemoryMap() {
        remap(0);
    }

    MemoryMap(const MemoryMap&)=delete;
    MemoryMap& operator=(const MemoryMap&)=delete;

    MemoryMap(MemoryMap&& other) {
        *this = std::move(other);
    }

    MemoryMap& operator=(MemoryMap&& other) {
        remap(0);
        _data = other._data;
        _size = other._size;
        other._data = 0;
        other._size = 0;
    }


    bool remap(size_t size) {
        if (size) {
            size_t page = getpagesize();
            size = (size + page - 1) / page * page;
            if (_size) {
                void* p = mremap(p, _size, size, MREMAP_MAYMOVE);
                if (p == MAP_FAILED) {
                    return false;
                }
                _data = reinterpret_cast<char*>(p);
                _size = size;
            } else {
                void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (p == MAP_FAILED) {
                    return false;
                }
                _data = reinterpret_cast<char*>(p);
                _size = size;
            }
        } else {
            if (_size) {
                if (!munmap(_data, _size)) {
                    return false;
                }
                _data = nullptr;
                _size = 0;
            }
        }
        return true;
    }

    char* data() {
        return _data;
    }

private:
    char* _data = nullptr;
    size_t _size = 0;
};

class ArrayHashSpace {
    typedef size_t size_type;
    ArrayHashSpace() {}

    ArrayHashSpace(ArrayHashSpace&& other) {
        *this = std::move(other);
    }

    ArrayHashSpace& operator=(ArrayHashSpace&& other) {
        _mm = std::move(other._mm);
        _max_offset = other._max_offset;
        _item_size = other._item_size;
        _num_items = other._num_items;
        other._max_offset = 0;
        other._item_size = 0;
        other._num_items = 0;
    }

    void set_space(size_type item_size, size_type num_items) {
        SCHECK(_mm.remap(item_size * (num_items + 1)));
        _item_size = item_size;
        _num_items = num_items;
        _max_offset = num_items * item_size;
    }

    size_type num_items()const {
        return _num_items;
    }

    size_type item_size()const {
        return _item_size;
    }

    size_type find_offset(size_type hash) {
        return (hash % _num_items + 1) * _item_size;
    }

    // [1 * _item_size, 2 * _item_size, 3 * _item_size, ..., _max_offset, 0)
    size_type probe_offset(size_type offset) {
        return offset < _max_offset ? offset + _item_size : 0;
    }

    char* operator[](size_type offset) {
        return _mm.data() + offset;
    }

private:
    MemoryMap _mm;
    size_type _max_offset = 0;
    size_type _item_size = 0;
    size_type _num_items = 0;
};

template<class Key, class Mapped, class Hash>
class PairItemType {
    using key_type = Key;
    using mapped_type = Mapped;
    using key_reference = const key_type&;
    using mapped_reference = mapped_type&;
    
    using value_type = std::pair<const Key, Mapped>;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = const value_type*;
    using const_reference = const value_type&;
    
    size_t type_size()const {
        return sizeof(value_type);
    }

    size_t type_align()const {
        return alignof(value_type);
    }

    size_t serialize_size(char*)const {
        return sizeof(value_type);
    }

    void move_serialize(char* item, char* snapshot)const {
        value_type* value = reinterpret_cast<value_type*>(item);
        construct(snapshot, value);
        destory(item);
    }

    void move_deserialize(char* item, char* snapshot)const {
        value_type* value = reinterpret_cast<value_type*>(snapshot);
        construct(value, item);
        destory(snapshot);
    }

    template<class... Args>
    void construct(char* item, Args&&... args)const {
        value_type* value = reinterpret_cast<value_type*>(item);
        new (value) value_type(std::forward<Args&&>(args)...);
    }

    void destory(char* item)const {
        value_type* value = reinterpret_cast<value_type*>(item);
        value->value_type::~value_type();
    }

    pointer get_pointer(char* item)const {
        return reinterpret_cast<pointer>(item);
    }

    reference get_reference(char* item)const {
        return *get_pointer(item);
    }

    key_reference get_key(char* item)const {
        return get_reference(item).first;
    }

    mapped_reference get_mapped(char* item)const {
        return get_reference(item).second;
    }

    bool equal(key_reference a, key_reference b)const {
        return a == b;
    }

    void hash(key_reference key)const {
        return _hash(key);
    }

private:
    Hash _hash;
};

template<class ItemType, class HashSpace = ArrayHashSpace>
class PoolHashTable {
public:
    using key_type = typename ItemType::key_type;
    using mapped_type = typename ItemType::mapped_type;
    using key_reference = typename ItemType::key_reference;
    using mapped_reference = typename ItemType::mapped_reference;
    
    using value_type = typename ItemType::value_type;
    using pointer = typename ItemType::pointer;
    using reference = typename ItemType::reference;
    using const_pointer = typename ItemType::const_pointer;
    using const_reference = typename ItemType::const_reference;

    using value_type = typename ItemType::value_type;
    using size_type = typename ClosedSpace::size_type;

    PoolHashTable(ItemType item_type = ItemType(), HashSpace hash_space = HashSpace())
          : _item_type(item_type), _hash_space(hash_space) {
            size_t type_align = std::max(4, item_type.type_align());
            size_t type_size = (item_type.type_size() + type_align - 1) / type_align * type_align;
            _item_offset = type_align;
            _node_size = type_size + type_align;
        }

    class const_iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = const typename ItemType::value_type;
        using reference = typename ItemType::const_reference;
        using pointer = typename ItemType::const_pointer;

        const_iterator(const PoolHashTable* table, size_type offset)
            : _table(table), _offset(offset) {}

    public:
        const_iterator& operator++() {
            _offset = this->_table->get_iterator_next(_offset);
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++*this;
            return temp; 
        }

        pointer operator->()const {
            return this->_table->get_pointer(_offset);
        }

        reference operator*()const {
            return this->_table->get_reference(_offset);
        }

        bool operator==(const_iterator other) {
            return _offset == other._offset;
        } 

    private:
        const PoolHashTable* _table = nullptr;
        size_type _offset = 0;
    };

    class iterator: public const_iterator {
    public:
        using value_type = typename ItemType::value_type;
        using reference = typename ItemType::reference;
        using pointer = typename ItemType::pointer;

        iterator& operator++() {
            _offset = this->_table->get_iterator_next(_offset);
            return *this;
        }

        iterator operator++(int) {
            const_iterator temp = *this;
            ++*this;
            return temp; 
        }

        pointer operator->()const {
            return this->_table->get_pointer(_offset);
        }

        reference operator*()const {
            return this->_table->get_reference(_offset);
        }
    };

    const_iterator find(key_reference key)const {
        size_type hash = _item_type.hash(key);
        size_type offset = _hash_space.find(hash);
        PoolNode* node = get_node(offset);
        while (offset && _item_type.equal(get_key(node), key)) {
            offset = node->next & ~MASK;
            node = get_node(offset);
        }
        return get_iterator(offset);
    }

    const_iterator begin()const {
        return get_iterator(get_iterator_next(0));
    }

    const_iterator end()const {
        return get_iterator(0);
    }

    iterator begin() {
        return get_iterator(get_iterator_next(0));
    }

    iterator end() {
        return get_iterator(0);
    }

private:
    iterator insert_no_resize(char* item) {
        key_reference key = _item_type.get_key(item);
        size_type hash = _item_type.hash(key);
        size_type offset = _hash_space.find(hash);
        PoolNode* node = get_node(offset);
        if (node->next & MASK == EMPTY) {
            
        } else if () {

        } else {

        }
    }

	static constexpr size_type EMPTY = 0;
    static constexpr size_type USED = 1;
	static constexpr size_type HEAD = 2;
    static constexpr size_type MASK = 3;
    static constexpr size_type PROBE_COUNT = 128 / (sizeof(T) + 16);

    struct PoolNode {
        size_type next;
        size_type prev;
    };

    iterator get_iterator(size_type offset)const {
        return iterator(this, offset);
    }

    size_type get_iterator_next(size_type offset)const {
        offset = _space.probe_offset(offset);
        while (offset && (get_node(offset)->next & MASK) == EMPTY) {
            offset = _space.probe_offset(offset);
        }
        return offset;
    }

    size_type get_hash_head(size_type offset) {
        return _space.find_offset(hash)
    }

    pointer get_pointer(size_type offset)const {
        return _item_type.get_pointer(get_item(get_node(_offset)));
    }

    reference get_reference(size_type offset)const {
        return _item_type.get_reference(get_item(get_node(_offset)));
    }

    char* get_item(PoolNode* node)const {
        return reinterpret_cast<char*>(item) + _item_offset;
    }

    key_reference get_key(PoolNode* node)const {
        return _item_type.get_key(get_item(node));
    }

    PoolNode* get_node(size_type offset)const {
        return reinterpret_cast<PoolNode*>(_space[offset]);
    }

    void initialize_nodes() {
        get_node(0)->next = get_node(0)->prev = 0;
        size_type current = _space.probe_offset(0);
        while (current) {
            inner_free_node(current);
            current = _space.probe_offset(0);
        }
    }

    template<class... Args>
    void new_node(size_type offset, Args&&... args) {
        PoolNode* node = get_node(offset);
        SCHECK(offset != 0 && (node->next & MASK) == EMPTY);
        get_node(node->next)->prev = node->prev;
        get_node(node->prev)->next = node->next;
        _item_type.construct(get_item(node), std::forward<Args&&>(args)...);
    }

    void delete_node(size_type offset) {
        PoolNode* node = get_node(offset);
        SCHECK(offset != 0 && (node->next & MASK) != EMPTY);
        _item_type.destruct(get_item(node));
        size_type next = 0;
        size_type probe = _space.probe_offset(offset);
        for (size_t i = 0; probe && i < PROBE_COUNT; ++i) {
            if (get_node(probe)->next & MASK) {
                next = probe;
            }
            probe = _space.probe_offset(probe);
        }
        inner_free_node(offset, next);
    }

    void inner_free_node(size_type current, size_type next) {
        size_type prev = get_node(next)->prev;
        get_node(current)->next = next;
        get_node(current)->prev = prev;
        get_node(next)->prev = current;
        get_node(prev)->next = current;
    }

    ItemType _item_type;
    HashSpace _hash_space;

    size_t _item_offset = 0;
    size_t _node_size = 0;
};

}
}
}

#endif
