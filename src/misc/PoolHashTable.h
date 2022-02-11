#ifndef PARADIGM4_PICO_CORE_POOL_HASH_TABLE_H
#define PARADIGM4_PICO_CORE_POOL_HASH_TABLE_H

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <vector>

#include "Archive.h"

namespace paradigm4 {
namespace pico {
namespace core {

template<class T>
struct is_trivially_movable: std::is_trivially_copyable<T> {};

template<class Key, class Mapped, class Hash, class Pred>
class PairItemType {
    using inner_value_type = std::pair<Key, Mapped>;
    alignas(alignof(inner_value_type)) struct item_buffer_t {
        char data[sizeof(inner_value_type)];
    };

public:
    using key_type = Key;
    using mapped_type = Mapped;
    using const_key_reference = const key_type&;
    using mapped_reference = mapped_type&;
    using const_mapped_reference = const mapped_type&;
    
    using value_type = std::pair<const Key, Mapped>;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = const value_type*;
    using const_reference = const value_type&;

    char* temp_item() {
        return _temp_item.data;
    }
    
    size_t type_size()const {
        return sizeof(inner_value_type);
    }

    size_t type_align()const {
        return alignof(inner_value_type);
    }

    bool is_trivially_movable()const {
        return core::is_trivially_movable<key_type>::value &&
               core::is_trivially_movable<mapped_type>::value;
    }

    template<class... Args>
    void construct(char* item, Args&&... args)const {
        inner_value_type* value = reinterpret_cast<inner_value_type*>(item);
        new (value) inner_value_type(std::forward<Args>(args)...);
    }

    template<class... Args>
    void construct_with_key(char* item, const_key_reference key, Args&&... args)const {
        inner_value_type* value = reinterpret_cast<inner_value_type*>(item);
        new (value) inner_value_type(key, mapped_type(std::forward<Args>(args)...));
    }

    template<class... Args>
    void construct_with_key(char* item, key_type&& key, Args&&... args)const {
        inner_value_type* value = reinterpret_cast<inner_value_type*>(item);
        new (value) inner_value_type(std::move(key), mapped_type(std::forward<Args>(args)...));
    }

    void destruct(char* item)const {
        inner_value_type* value = reinterpret_cast<inner_value_type*>(item);
        value->inner_value_type::~inner_value_type();
    }

    void move_from(char* item, char* other)const {
        if (is_trivially_movable()) {
            *reinterpret_cast<item_buffer_t*>(item) = *reinterpret_cast<item_buffer_t*>(other);
        } else {
            construct(item, std::move(*reinterpret_cast<inner_value_type*>(other)));
            destruct(other);           
        }
    }

    pointer get_pointer(char* item)const {
        return reinterpret_cast<pointer>(item);
    }

    reference get_reference(char* item)const {
        return *get_pointer(item);
    }

    const_key_reference get_key(char* item)const {
        return get_reference(item).first;
    }

    mapped_reference get_mapped(char* item)const {
        return get_reference(item).second;
    }

    template<class Serializer>
    void move_serialize(char* item, Serializer& serializer)const {
        serializer << std::move(*reinterpret_cast<inner_value_type*>(item));
    }

    template<class Serializer>
    void move_deserialize(char* item, Serializer& serializer)const {
        construct(item);
        serializer >> *reinterpret_cast<inner_value_type*>(item);
    }

    bool key_eq(const_key_reference a, const_key_reference b)const {
        return _key_eq(a, b);
    }

    size_t hash_function(const_key_reference key)const {
        return _hash_function(key);
    }

private:
    Pred _key_eq;
    Hash _hash_function;
    item_buffer_t _temp_item;
};

template<class T>
class HashTableFileSerializer {
public:
    HashTableFileSerializer() {}

    HashTableFileSerializer(const HashTableFileSerializer&) {}

    ~HashTableFileSerializer() {
        reset();
    }

    HashTableFileSerializer& operator=(const HashTableFileSerializer& other) {
        reset();
    }

    HashTableFileSerializer& operator<<(T&& value) {
        if (!_reading) {
            SCHECK(_file == nullptr);
            _file = std::tmpfile();
            _ar.reset(_file);
            _reading = true;
        }
        _ar << std::move(value);
        return *this;
    }

    HashTableFileSerializer& operator>>(T& value) {
        SCHECK(_file);
        if (_reading) {
            std::rewind(_file);
            _reading = false;
        }
        _ar >> value;
        return *this;
    }

    void reset() {
        _reading = false;
        if (_file) {
            std::fclose(_file);
            _file = nullptr;
        }
        _ar.reset();
    }

private:
    bool _reading = false;
    FILE* _file = nullptr;
    BinaryFileArchive _ar;
};

template<class T>
class HashTableMemorySerializer {
public:
    HashTableMemorySerializer() {}

    HashTableMemorySerializer(const HashTableMemorySerializer&) {}

    ~HashTableMemorySerializer() {
        reset();
    }

    HashTableMemorySerializer& operator=(const HashTableMemorySerializer& other) {
        reset();
    }

    HashTableMemorySerializer& operator<<(T&& value) {
        _que.push_back(std::move(value));
        return *this;
    }

    HashTableMemorySerializer& operator>>(T& value) {
        value = std::move(_que.front());
        _que.pop_front();
        return *this;
    }

    void reset() {
        _que.clear();
        _que.shrink_to_fit();
    }

private:
    std::deque<T> _que;
};

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

    void remap(size_t size) {
        // pico_free(_data);
        // _data = reinterpret_cast<char*>(pico_malloc(size));
        // return;
        if (size) {
            size_t page = getpagesize();
            size = (size + page - 1) / page * page;
            if (_size) {
                void* p = mremap(_data, _size, size, MREMAP_MAYMOVE);
                PCHECK(p != MAP_FAILED);
                _data = reinterpret_cast<char*>(p);
                _size = size;
            } else {
                void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                PCHECK(p != MAP_FAILED);
                _data = reinterpret_cast<char*>(p);
                _size = size;
            }
        } else {
            if (_size) {
                PCHECK(munmap(_data, _size) == 0);
                _data = nullptr;
                _size = 0;
            }
        }
    }

    char* data()const {
        return _data;
    }

private:
    char* _data = nullptr;
    size_t _size = 0;
};

class FastModTable {
    template<size_t mod>
    static size_t fast_mod(size_t hash) {
        return hash % mod;
    }

    template<size_t mod, bool next = mod < std::numeric_limits<size_t>::max() / 2>
    struct AddFastMod;

    template<size_t mod>
    struct AddFastMod<mod, true> {
        static void add_fast_mod(std::map<size_t, size_t(*)(size_t)>& fast_mods) {
            fast_mods[mod] = fast_mod<mod>;
            AddFastMod<size_t(mod * 1.05 + 1)>::add_fast_mod(fast_mods);
        }
    };

    template<size_t mod>
    struct AddFastMod<mod, false> {
        static void add_fast_mod(std::map<size_t, size_t(*)(size_t)>& fast_mods) {
            fast_mods[mod] = fast_mod<mod>;
        }
    };

    FastModTable() {
        AddFastMod<1>::add_fast_mod(fast_mods);
    }

public:
    static const FastModTable& singleton() {
        static FastModTable fast_mod_table;
        return fast_mod_table;
    }

    std::map<size_t, size_t(*)(size_t)> fast_mods;
};

class ArrayHashSpace {
    using double_size_t = __uint128_t;
    static constexpr int BITS = 8 * sizeof(size_t);
public:
    typedef size_t offset_type;

    ArrayHashSpace() {}
    ArrayHashSpace(const ArrayHashSpace& other) {
        *this = other;
    }

    ArrayHashSpace& operator=(const ArrayHashSpace& other) {
        _mm.remap(0);
        set_space(other.node_size(), other.num_nodes());
        return *this;
    }

    ArrayHashSpace(ArrayHashSpace&& other) {
        *this = std::move(other);
    }

    ArrayHashSpace& operator=(ArrayHashSpace&& other) {
        _mm = std::move(other._mm);
        _mask = other._mask;
        _fast_mod = other._fast_mod;
        _max_offset = other._max_offset;
        _node_size = other._node_size;
        _num_nodes = other._num_nodes;
        other._max_offset = 0;
        other._node_size = 0;
        other._num_nodes = 0;
        return *this;
    }

    void set_space(offset_type node_size, offset_type num_nodes) {
        _mm.remap(node_size * (num_nodes + 1));
        _node_size = node_size;
        _num_nodes = num_nodes;
        _max_offset = num_nodes * node_size;
        
        size_t n = 0;
        while ((size_t(1) << (n + 1)) <= _num_nodes) {
            ++n;
        }
        _mask = (size_t(1) << n) - 1;
        auto it = FastModTable::singleton().fast_mods.upper_bound(_num_nodes);
        SCHECK(it != FastModTable::singleton().fast_mods.begin());
        --it;
        _fast_mod = it->second;
        // if (1.0 * _mask / _num_nodes < 0.91) {
        //     _fast_mod = nullptr;
        // }
    }

    offset_type expand_space()const {
        double ratio = 1.0 * _num_nodes / _mask;
        if (_mask && ratio > 1.4 && ratio < 1.8) {
            return (_mask + 1) * 2;
        }
        return std::max(offset_type(_num_nodes * (1.0 * 45 / 32)), _num_nodes + 1);
    }

    offset_type num_nodes()const {
        return _num_nodes;
    }

    offset_type node_size()const {
        return _node_size;
    }

    offset_type find_offset(size_t hash)const {
        // return (_fast_mod(hash) + 1) * _node_size;
        return ((hash & _mask) + 1) * _node_size;
        // if (_fast_mod) {
        //     return (_fast_mod(hash) + 1) * _node_size;
        // } else {
        //     return ((hash & _mask) + 1) * _node_size;
        // }
    }

    // [1 * _node_size, 2 * _node_size, 3 * _node_size, ..., _max_offset, 0)
    offset_type probe_offset(offset_type offset)const {
        return offset < _max_offset ? offset + _node_size : 0;
    }

    char* operator[](offset_type offset)const {
        return _mm.data() + offset;
    }

protected:
    mutable bool _warned = false;

    MemoryMap _mm;
    offset_type _mask = 0;
    offset_type(*_fast_mod)(size_t) = nullptr;
    offset_type _max_offset = 0;
    offset_type _node_size = 0;
    offset_type _num_nodes = 0;

private:
    template<size_t mod>
    static size_t fast_mod(size_t hash) {
        return hash % mod;
    }

    template<size_t mod, bool next = mod < std::numeric_limits<size_t>::max() / 2>
    struct AddFastMod;

    template<size_t mod>
    struct AddFastMod<mod, true> {
        static void add_fast_mod(std::map<size_t, offset_type(*)(size_t)>& fast_mods) {
            SLOG(INFO) << mod;
            fast_mods[mod] = fast_mod<mod>;
            AddFastMod<size_t(mod * 17.0 / 16.0 + 1)>::add_fast_mod(fast_mods);
        }
    };

    template<size_t mod>
    struct AddFastMod<mod, false> {
        static void add_fast_mod(std::map<size_t, offset_type(*)(size_t)>& fast_mods) {
            fast_mods[mod] = fast_mod<mod>;
        }
    };
};

template<class ItemType, class Serializer, class HashSpace>
class PoolHashTable {
    using const_key_reference = typename ItemType::const_key_reference;
    using mapped_reference = typename ItemType::mapped_reference;
    using const_mapped_reference =  typename ItemType::const_mapped_reference;

public:
    using key_type = typename ItemType::key_type;
    using mapped_type = typename ItemType::mapped_type;
    using value_type = typename ItemType::value_type;
    using pointer = typename ItemType::pointer;
    using reference = typename ItemType::reference;
    using const_pointer = typename ItemType::const_pointer;
    using const_reference = typename ItemType::const_reference;
    using offset_type = typename HashSpace::offset_type;
    using size_type = size_t;

    PoolHashTable(ItemType item_type = ItemType(), 
            Serializer serializer = Serializer(), 
            HashSpace hash_space = HashSpace())
          : _item_type(item_type), 
            _serializer(serializer), 
            _hash_space(std::move(hash_space)) {
        size_t type_align = std::max(std::max(size_t(8), sizeof(offset_type)), item_type.type_align());
        size_t type_size = (item_type.type_size() + type_align - 1) / type_align * type_align;
        _item_offset = type_align;
        _node_size = type_size + type_align;
        _hash_space.set_space(_node_size, 1);
        initialize_nodes();
    }

    PoolHashTable(PoolHashTable&& other)
        : _item_type(other._item_type),
          _serializer(other._serializer),
          _hash_space(std::move(other._hash_space)),
          _max_load_factor(other._max_load_factor),
          _num_items(other._num_items),
          _max_num_items(other._max_num_items),
          _item_offset(other._item_offset),
          _node_size(other._node_size) {
        other._hash_space.set_space(other._node_size, 1);
        other.initialize_nodes();
        other._num_items = other._max_num_items = 0;
    }

    PoolHashTable(const PoolHashTable& other)
        : _item_type(other._item_type),
          _serializer(other._serializer),
          _hash_space(other._hash_space),
          _max_load_factor(other._max_load_factor),
          _max_num_items(other._max_num_items),
          _item_offset(other._item_offset),
          _node_size(other._node_size) {
        initialize_nodes(); // clear the may be copyed _hash_space
        reserve(other.size());
        for (auto& value: other) {
            insert(value);
        }
    }

    PoolHashTable& operator=(PoolHashTable other) {
        this->~PoolHashTable();
        new (this) PoolHashTable(std::move(other));
        return *this;
    }

    ~PoolHashTable() noexcept {
        clear();
    }

    class const_iterator {
        friend PoolHashTable;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = const typename ItemType::value_type;
        using reference = typename ItemType::const_reference;
        using pointer = typename ItemType::const_pointer;

        const_iterator(const PoolHashTable* table, offset_type head, offset_type offset)
            : _table(table), _head(head), _offset(offset) {}

    public:
        const_iterator() {}

        const_iterator& operator++() {
            SCHECK(_offset);
            _offset = _table->get_node(_offset)->get_next();
            if (!_offset) {
                _offset = _head = _table->get_next_head(_head);
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++*this;
            return temp; 
        }

        pointer operator->()const {
            return _table->get_pointer(_offset);
        }

        reference operator*()const {
            return _table->get_reference(_offset);
        }

        // not compare head
        bool operator==(const_iterator other)const {
            return _offset == other._offset && _table == other._table;
        }

        bool operator!=(const_iterator other)const {
            return _offset != other._offset || _table != other._table;
        }

    private:
        const PoolHashTable* _table = nullptr;
        offset_type _head = 0;
        offset_type _offset = 0;
    };

    class iterator: public const_iterator {
        friend PoolHashTable;
        using const_iterator::const_iterator;

    public:
        using value_type = typename ItemType::value_type;
        using reference = typename ItemType::reference;
        using pointer = typename ItemType::pointer;

        iterator& operator++() {
            const_iterator::operator++();
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++*this;
            return temp; 
        }

        pointer operator->()const {
            return this->_table->get_pointer(this->_offset);
        }

        reference operator*()const {
            return this->_table->get_reference(this->_offset);
        }
    };

    size_t size()const {
        return _num_items;
    }

    size_t bucket_count()const {
        return _hash_space.num_nodes();
    }

    bool empty()const {
        return size() == 0;
    }

    double load_factor()const {
        return 1.0 * size() / bucket_count();
    }

    double max_load_factor()const {
        return _max_load_factor;
    }

    void max_load_factor(double factor) {
        if (factor < 0.1) {
            factor = 0.1;
        }
        _max_load_factor = std::min(1.0, factor);
        _max_num_items = std::min(_hash_space.num_nodes(), 
              offset_type(_max_load_factor * _hash_space.num_nodes()));
    }

    void rehash(size_t num_buckets) {
        num_buckets = std::max(num_buckets, size_t(1));
        num_buckets = std::max(num_buckets, size_t(_num_items / _max_load_factor));
        while (num_buckets * _max_load_factor < _num_items) {
            ++num_buckets;
        }
        if (num_buckets != _hash_space.num_nodes()) {
            if (num_buckets > _hash_space.num_nodes() && _item_type.is_trivially_movable()) {
                inner_remap_rehash(num_buckets);
            } else {
                inner_serialize_rehash(num_buckets);
            }
        }
    }

    void reserve(offset_type n) {
        if (n > _max_num_items) {
            size_t num_buckets = n / _max_load_factor;
            while (num_buckets * _max_load_factor < n) {
                ++num_buckets;
            }
            if (num_buckets > _hash_space.num_nodes()) {
                rehash(std::max(num_buckets, _hash_space.expand_space()));
            }
        }
    }

    void clear() {
        for (iterator it = begin(); it != end(); ++it) {
            _item_type.destruct(get_item(it._offset));
        }
        initialize_nodes();
    }

    iterator find(const_key_reference key) {
        return inner_find(key);
    }

    const_iterator find(const_key_reference key)const {
        return inner_find(key);
    }

    size_t count(const_key_reference key)const {
        return find(key)._offset != 0;
    }

    mapped_reference at(const_key_reference key) {
        iterator it = inner_find(key);
        SCHECK(it._offset);
        return get_mapped(it._offset);
    }

    const_mapped_reference at(const_key_reference key)const {
        iterator it = inner_find(key);
        SCHECK(it._offset);
        return get_mapped(it._offset);
    }

    size_t erase(const_key_reference key) {
        iterator it = find(key);
        if (it._offset) {
            erase(it);
            return 1;
        }
        return 0;
    }

    iterator erase(iterator it) {
        SCHECK(this == it._table);
        offset_type offset = it._offset;
        SCHECK(offset && !get_node(offset)->is_empty());
        offset_type next = get_node(offset)->get_next();
        if (next) {
            _item_type.destruct(get_item(offset));
            offset_type prev = 0;
            while (next) {
                _item_type.move_from(get_item(offset), get_item(next));
                prev = offset;
                offset = next;
                next = get_node(next)->get_next();
            }
            deallocate_node(offset, probe_node(offset));
            get_node(prev)->set_next(0);
        } else {
            ++it;
            offset_type head = get_hash_head(get_key(offset));
            _item_type.destruct(get_item(offset));
            if (offset != head) {
                offset_type prev = find_node_before(head, offset);
                get_node(prev)->set_next(0);
            }
            deallocate_node(offset, probe_node(offset));
        }
        return it;
    }

    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        char* temp_item = _item_type.temp_item();
        _item_type.construct(temp_item, std::forward<Args>(args)...);
        const_key_reference key = _item_type.get_key(temp_item);
        iterator it = inner_find(key);
        if (likely(it._offset)) {
            _item_type.destruct(temp_item);
            return {it, false};
        }
        return inner_emplace_not_exist(temp_item);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const_key_reference key, Args&&... args) {
        iterator it = inner_find(key);
        if (likely(it._offset)) {
            return {it, false};
        }
        char* temp_item = _item_type.temp_item();
        _item_type.construct_with_key(temp_item, key, std::forward<Args>(args)...);
        return inner_emplace_not_exist(temp_item);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        iterator it = inner_find(key);
        if (likely(it._offset)) {
            return {it, false};
        }
        char* temp_item = _item_type.temp_item();
        _item_type.construct_with_key(temp_item, std::move(key), std::forward<Args>(args)...);
        return inner_emplace_not_exist(temp_item);
    }

    std::pair<iterator, bool> insert(const_reference value) {
        return emplace(value);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        return emplace(std::move(value));
    }

    mapped_reference operator[](const_key_reference key) {
        iterator it = inner_find(key);
        if (likely(it._offset)) {
            return get_mapped(it._offset);
        }
        char* temp_item = _item_type.temp_item();
        _item_type.construct_with_key(temp_item, key);
        return get_mapped(inner_emplace_not_exist(temp_item).first._offset);
    }

    mapped_reference operator[](key_type&& key) {
        iterator it = inner_find(key);
        if (likely(it._offset)) {
            return get_mapped(it._offset);
        }
        char* temp_item = _item_type.temp_item();
        _item_type.construct_with_key(temp_item, std::move(key));
        return get_mapped(inner_emplace_not_exist(temp_item).first._offset);
    }

    const_iterator begin()const {
        offset_type first = get_next_head(0);
        return iterator(this, first, first);
    }

    const_iterator end()const {
        return iterator(this, 0, 0);
    }

    iterator begin() {
        offset_type first = get_next_head(0);
        return iterator(this, first, first);
    }

    iterator end() {
        return iterator(this, 0, 0);
    }

    void debug() {
        offset_type offset = 0;
        SLOG(INFO) << "debug hash table:";
        do {
            PoolNode* node = get_node(offset);
            if (node->is_empty()) {
                SLOG(INFO) << offset << '\t' 
                           << (node->next & MASK) << '\t'
                           << node->get_next() << '\t'
                           << node->prev;
            } else {
                SLOG(INFO) << offset << '\t' 
                           << (node->next & MASK) << '\t' 
                           << node->get_next() << '\t' 
                           << get_key(offset) << '\t' 
                           << get_mapped(offset);
            }
            offset = _hash_space.probe_offset(offset);
        } while (offset);
    }

private:
    void inner_remap_rehash(size_t n) {
        SCHECK(n > _hash_space.num_nodes());
        _hash_space.set_space(_node_size, n);
        max_load_factor(_max_load_factor);
        
        get_node(0)->next = get_node(0)->prev = 0;
        offset_type offset = _hash_space.probe_offset(0);
        while (offset) {
            if (get_node(offset)->is_empty()) {
                deallocate_node(offset, 0);
                ++_num_items;
            } else {
                get_node(offset)->next |= REHASH;
            }
            offset = _hash_space.probe_offset(offset);
        }

        offset = _hash_space.probe_offset(0);
        char* temp_item = temp_align(alloca(_node_size * 2)); // _item_type.temp_item may be used
        while (offset) {
            bool next = true;
            if (get_node(offset)->is_rehashing()) {
                offset_type probe = get_hash_head(get_key(offset));
                if (get_node(probe)->is_rehashing()) {
                    get_node(probe)->next = HEAD;
                    if (probe != offset) {
                        _item_type.move_from(temp_item, get_item(probe));
                        _item_type.move_from(get_item(probe), get_item(offset));
                        _item_type.move_from(get_item(offset), temp_item);
                        next = false;
                    }
                } else if (get_node(probe)->is_empty()) {
                    allocate_node(probe);
                    get_node(probe)->next = HEAD;
                    _item_type.move_from(get_item(probe), get_item(offset));
                    deallocate_node(offset, 0);
                }  
            }
            if (next) {
                offset = _hash_space.probe_offset(offset);
            }
        }

        offset = _hash_space.probe_offset(0);
        while (offset) {
            bool next = true;
            if (get_node(offset)->is_rehashing()) {
                offset_type prev = get_hash_head(get_key(offset));
                while (get_node(prev)->get_next()) {
                    prev = get_node(prev)->get_next();
                }
                offset_type probe = probe_node_rehash(prev);
                if (get_node(probe)->is_rehashing()) {
                    get_node(probe)->next = PROBE;
                    get_node(prev)->set_next(probe);
                    if (probe != offset) {
                        _item_type.move_from(temp_item, get_item(probe));
                        _item_type.move_from(get_item(probe), get_item(offset));
                        _item_type.move_from(get_item(offset), temp_item);
                        next = false;
                    }
                } else {
                    allocate_node(probe);
                    get_node(prev)->set_next(probe);
                    _item_type.move_from(get_item(probe), get_item(offset));
                    deallocate_node(offset, 0);
                }
            }
            if (next) {
                offset = _hash_space.probe_offset(offset);
            }
        }
    }

    void inner_serialize_rehash(size_t n) {
        offset_type count = size();
        offset_type offset = _hash_space.probe_offset(0);
        while (offset) {
            if (!get_node(offset)->is_empty()) {
                _item_type.move_serialize(get_item(offset), _serializer);
            }
            offset = _hash_space.probe_offset(offset);
        }

        clear();
        _hash_space.set_space(_node_size, n);
        max_load_factor(_max_load_factor);
        initialize_nodes();
        char* temp_item = temp_align(alloca(_node_size * 2));
        for (offset_type i = 0; i < count; ++i) {
            _item_type.move_deserialize(temp_item, _serializer);
            const_key_reference key = _item_type.get_key(temp_item);
            iterator it = inner_allocate_not_exist_no_rehash(key);
            _item_type.move_from(get_item(it._offset), temp_item);
        }
        _serializer.reset();
    }
    
    iterator inner_find(const_key_reference key)const {
        offset_type head = get_hash_head(key);
        offset_type offset = 0;
        if (likely(!get_node(head)->is_empty())) {
            offset = head;
            do {
                if (_item_type.key_eq(get_key(offset), key)) {
                    break;
                }
            } while (offset = get_node(offset)->get_next());
        }
        return iterator(this, head, offset);
    }

    iterator inner_allocate_not_exist_no_rehash(const_key_reference key) {
        offset_type head = get_hash_head(key);
        PoolNode* node = get_node(head);
        if (node->is_empty()) {
            allocate_node(head);
            node->next = HEAD;
            return iterator(this, head, head);
        } else if (node->is_head()) {
            offset_type prev = head;
            while (get_node(prev)->get_next()) {
                prev = get_node(prev)->get_next();
            }
            offset_type probe = probe_node(prev);
            allocate_node(probe);
            get_node(prev)->set_next(probe);
            return iterator(this, head, probe);
        } else {
            offset_type exchange = get_hash_head(get_key(head));

            _temp.clear();
            _temp.reserve(20);
            offset_type prev = head;
            _temp.push_back(prev);
            while (get_node(prev)->get_next()) {
                prev = get_node(prev)->get_next();
                _temp.push_back(prev);
            }
            
            offset_type probe = probe_node(prev);
            allocate_node(probe);
            get_node(prev)->set_next(probe);
            while (!_temp.empty()) {
                _item_type.move_from(get_item(probe), get_item(_temp.back()));
                probe = _temp.back();
                _temp.pop_back();
            }

            exchange = find_node_before(exchange, head);
            get_node(exchange)->set_next(node->get_next());
            node->next = HEAD;
            return iterator(this, head, head);
        }
    }

    std::pair<iterator, bool> inner_emplace_not_exist(char* item) {
        reserve(_num_items + 1);
        const_key_reference key = _item_type.get_key(item);
        iterator it = inner_allocate_not_exist_no_rehash(key);
        _item_type.move_from(get_item(it._offset), item);
        return {it, true};
    }

    offset_type find_node_before(offset_type head, offset_type next) {
        offset_type probe = head;
        while (get_node(probe)->get_next() != next) {
            probe = get_node(probe)->get_next();
        }
        return probe;
    }

    offset_type get_hash_head(const_key_reference key)const {
        return _hash_space.find_offset(_item_type.hash_function(key));
    }

	static constexpr offset_type EMPTY = 0;
    static constexpr offset_type PROBE = 1;
	static constexpr offset_type HEAD = 2;
    static constexpr offset_type REHASH = 3;

    static constexpr offset_type MASK = 3;
    static constexpr offset_type PROBE_LEN = 128;

    struct PoolNode {
        offset_type next;
        offset_type prev;

        bool is_rehashing() {
            return (next & REHASH) == REHASH;
        }

        bool is_empty() {
            return (next & MASK) == EMPTY;
        }

        bool is_head() {
            return (next & MASK) == HEAD;
        }

        offset_type get_next() {
            return next & ~MASK;
        }

        void set_next(offset_type offset) {
            next = next & MASK | offset;
        }
    };

    offset_type get_next_head(offset_type offset)const {
        offset = _hash_space.probe_offset(offset);
        while (offset && !get_node(offset)->is_head()) {
            offset = _hash_space.probe_offset(offset);
        }
        return offset;
    }

    pointer get_pointer(offset_type offset)const {
        return _item_type.get_pointer(get_item(offset));
    }

    reference get_reference(offset_type offset)const {
        return _item_type.get_reference(get_item(offset));
    }

    const_key_reference get_key(offset_type offset)const {
        return _item_type.get_key(get_item(offset));
    }

    mapped_reference get_mapped(offset_type offset)const {
        return _item_type.get_mapped(get_item(offset));
    }

    char* get_item(offset_type offset)const {
        return reinterpret_cast<char*>(get_node(offset)) + _item_offset;
    }

    PoolNode* get_node(offset_type offset)const {
        return reinterpret_cast<PoolNode*>(_hash_space[offset]);
    }

    void initialize_nodes() {
        _num_items = _hash_space.num_nodes();
        PoolNode* head = get_node(0);
        head->next = head->prev = 0;
        offset_type offset = _hash_space.probe_offset(0);
        while (offset) {
            deallocate_node(offset, 0);
            offset = _hash_space.probe_offset(offset);
        }
    }

    // offset != 0, offset is EMPTY
    void allocate_node(offset_type offset) {
        ++_num_items;
        PoolNode* node = get_node(offset);
        get_node(node->next)->prev = node->prev;
        get_node(node->prev)->next = node->next;
        node->next = PROBE;
    }

    void deallocate_node(offset_type offset, offset_type next) {
        --_num_items;
        offset_type prev = get_node(next)->prev;
        get_node(offset)->next = next;
        get_node(offset)->prev = prev;
        get_node(next)->prev = offset;
        get_node(prev)->next = offset;
    }

    offset_type probe_node(offset_type probe) {
        offset_type offset = probe;
        for (size_t i = 0; probe && offset - probe < PROBE_LEN; ++i) {
            if (get_node(probe)->is_empty()) {
                return probe;
            }
            probe = _hash_space.probe_offset(probe);
        }
        return get_node(0)->prev;
    }

    offset_type probe_node_rehash(offset_type probe) {
        offset_type offset = probe;
        for (size_t i = 0; probe && offset - probe < PROBE_LEN; ++i) {
            if (get_node(probe)->is_empty() || get_node(probe)->is_rehashing()) {
                return probe;
            }
            probe = _hash_space.probe_offset(probe);
        }
        return get_node(0)->prev;
    }

    char* temp_align(void* temp_item) {
        return reinterpret_cast<char*>((
            reinterpret_cast<uintptr_t>(temp_item) + _item_offset) / _item_offset * _item_offset); 
    }

    ItemType _item_type;
    Serializer _serializer;
    HashSpace _hash_space;

    double _max_load_factor = 1.0;
    offset_type _num_items = 0;
    offset_type _max_num_items = 0;
    size_t _item_offset = 0;
    size_t _node_size = 0;

    std::vector<offset_type> _temp;
    char _pad[64];
};

template<class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
using pool_hash_map = PoolHashTable<PairItemType<Key, T, Hash, Pred>, HashTableMemorySerializer<std::pair<Key, T>>, ArrayHashSpace>;

}
}
}

#undef POOL_HASH_MAP_INLINE

#endif
