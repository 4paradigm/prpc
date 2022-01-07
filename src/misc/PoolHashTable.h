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


template<class Key, class Mapped, class Hash, class Pred>
class PairItemType {
    using inner_value_type = std::pair<Key, Mapped>;
public:
    using key_type = Key;
    using mapped_type = Mapped;
    using key_reference = const key_type&;
    using mapped_reference = mapped_type&;
    using const_mapped_reference = const mapped_type&;
    
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

    template<class... Args>
    void construct(char* item, Args&&... args)const {
        value_type* value = reinterpret_cast<value_type*>(item);
        new (value) value_type(std::forward<Args>(args)...);
    }

    void destruct(char* item)const {
        value_type* value = reinterpret_cast<value_type*>(item);
        value->value_type::~value_type();
    }

    void move_construct(char* item, char* other)const {
        construct(item, std::move(*reinterpret_cast<value_type*>(other)));
        destruct(other);
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

    template<class Serializer>
    void move_serialize(char* item, Serializer& serializer)const {
        serializer << *reinterpret_cast<inner_value_type*>(item);
    }

    template<class Serializer>
    void move_deserialize(char* item, Serializer& serializer)const {
        construct(item);
        serializer >> *reinterpret_cast<inner_value_type*>(item);
    }

    bool key_eq(key_reference a, key_reference b)const {
        return _key_eq(a, b);
    }

    size_t hash_function(key_reference key)const {
        return _hash_function(key);
    }

private:
    Pred _key_eq;
    Hash _hash_function;
};

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

    template<class T>
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

    template<class T>
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
    typedef size_t size_type;

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
        _max_offset = other._max_offset;
        _node_size = other._node_size;
        _num_nodes = other._num_nodes;
        other._max_offset = 0;
        other._node_size = 0;
        other._num_nodes = 0;
        return *this;
    }

    void set_space(size_type node_size, size_type num_nodes) {
        _mm.remap(node_size * (num_nodes + 1));
        _node_size = node_size;
        _num_nodes = num_nodes;
        _max_offset = num_nodes * node_size;
        
        size_t n = 0;
        while ((size_t(1) << (n + 1)) <= _num_nodes) {
            ++n;
        }
        _mask = (size_t(1) << n) - 1;
        if (1.0 * _mask / _num_nodes < 0.91) {
            auto it = FastModTable::singleton().fast_mods.upper_bound(_num_nodes);
            SCHECK(it != FastModTable::singleton().fast_mods.begin());
            --it;
            _fast_mod = it->second;
        } else {
            _fast_mod = nullptr;
        }
    }

    size_type expand_space()const {
        return _num_nodes * 7 / 6 + 1;
    }

    size_type num_nodes()const {
        return _num_nodes;
    }

    size_type node_size()const {
        return _node_size;
    }

    size_type find_offset(size_t hash)const {
        return ((hash & _mask) + 1) * _node_size;
        // if (_fast_mod) {
        //     return (_fast_mod(hash) + 1) * _node_size;
        // } else {
        //     return ((hash & _mask) + 1) * _node_size;
        // }
    }

    // [1 * _node_size, 2 * _node_size, 3 * _node_size, ..., _max_offset, 0)
    size_type probe_offset(size_type offset)const {
        return offset < _max_offset ? offset + _node_size : 0;
    }

    char* operator[](size_type offset)const {
        return _mm.data() + offset;
    }

protected:
    mutable bool _warned = false;

    MemoryMap _mm;
    size_type _mask = 0;
    size_type(*_fast_mod)(size_t) = nullptr;
    size_type _max_offset = 0;
    size_type _node_size = 0;
    size_type _num_nodes = 0;

private:
    template<size_t mod>
    static size_t fast_mod(size_t hash) {
        return hash % mod;
    }

    template<size_t mod, bool next = mod < std::numeric_limits<size_t>::max() / 2>
    struct AddFastMod;

    template<size_t mod>
    struct AddFastMod<mod, true> {
        static void add_fast_mod(std::map<size_t, size_type(*)(size_t)>& fast_mods) {
            SLOG(INFO) << mod;
            fast_mods[mod] = fast_mod<mod>;
            AddFastMod<size_t(mod * 17.0 / 16.0 + 1)>::add_fast_mod(fast_mods);
        }
    };

    template<size_t mod>
    struct AddFastMod<mod, false> {
        static void add_fast_mod(std::map<size_t, size_type(*)(size_t)>& fast_mods) {
            fast_mods[mod] = fast_mod<mod>;
        }
    };
};

template<class ItemType, class Serializer, class HashSpace>
class PoolHashTable {
    using key_reference = typename ItemType::key_reference;
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
    using size_type = typename HashSpace::size_type;

    PoolHashTable(ItemType item_type = ItemType(), 
            Serializer serializer = Serializer(), 
            HashSpace hash_space = HashSpace())
          : _item_type(item_type), 
            _serializer(serializer), 
            _hash_space(std::move(hash_space)) {
        size_t type_align = std::max(size_type(4), item_type.type_align());
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

    PoolHashTable& operator=(PoolHashTable&& other) {
        this->~PoolHashTable();
        new (this) PoolHashTable(std::move(other));
        return *this;
    }

    PoolHashTable& operator=(const PoolHashTable& other) {
        clear();
        reserve(other.size());
        for (auto& value: other) {
            insert(value);
        }
        return *this;
    }

    ~PoolHashTable() {
        clear();
    }

    class const_iterator {
        friend PoolHashTable;
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
            return _table->get_pointer(_offset);
        }

        reference operator*()const {
            return _table->get_reference(_offset);
        }

        bool operator==(const_iterator other)const {
            return _offset == other._offset;
        }

        bool operator!=(const_iterator other)const {
            return _offset != other._offset;
        }

    private:
        const PoolHashTable* _table = nullptr;
        size_type _offset = 0;
    };

    class iterator: public const_iterator {
        friend PoolHashTable;
        using const_iterator::const_iterator;

    public:
        using value_type = typename ItemType::value_type;
        using reference = typename ItemType::reference;
        using pointer = typename ItemType::pointer;

        iterator& operator++() {
            this->_offset = this->_table->get_iterator_next(this->_offset);
            return *this;
        }

        iterator operator++(int) {
            const_iterator temp = *this;
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

    float max_load_factor()const {
        return _max_load_factor;
    }

    void max_load_factor(float factor) {
        if (factor < 0.1) {
            factor = 0.1;
        }
        _max_load_factor = std::min(1.0f, factor);
        _max_num_items = std::min(_hash_space.num_nodes(), 
              size_type(_max_load_factor * _hash_space.num_nodes()));
    }

    void rehash(size_type n) {
        char* temp_item = reinterpret_cast<char*>(alloca(_node_size));
        if (n > _hash_space.num_nodes()) {
            size_type count = size();
            // HEAD first, because HEAD may be access more frequently.
            size_type offset = _hash_space.probe_offset(0);
            while (offset) {
                if (get_node(offset)->is_head()) {
                    _item_type.move_serialize(get_item(offset), _serializer);
                }
                offset = _hash_space.probe_offset(offset);
            }

            offset = _hash_space.probe_offset(0);
            while (offset) {
                if (!get_node(offset)->is_head()) {
                    _item_type.move_serialize(get_item(offset), _serializer);
                }
                offset = _hash_space.probe_offset(offset);
            }

            clear();
            _hash_space.set_space(_node_size, n);
            max_load_factor(_max_load_factor);
            initialize_nodes();
            for (size_type i = 0; i < count; ++i) {
                _item_type.move_deserialize(temp_item, _serializer);
                key_reference key = _item_type.get_key(temp_item);
                std::pair<size_type, bool> itr = inner_find_to_emplace(key);
                _item_type.move_construct(get_item(itr.first), temp_item);
            }
            _serializer.reset();
            
        }
    }

    void reserve(size_type n) {
        return rehash(n / _max_load_factor);
    }

    void clear() {
        for (iterator it = begin(); it != end(); ++it) {
            _item_type.destruct(get_item(it._offset));
        }
        initialize_nodes();
    }

    iterator find(key_reference key) {
        return get_iterator(inner_find(key));
    }

    const_iterator find(key_reference key)const {
        return get_iterator(inner_find(key));
    }

    mapped_reference at(key_reference key) {
        size_type offset = inner_find(key);
        SCHECK(offset);
        return get_mapped(offset);
    }

    const_mapped_reference at(key_reference key)const {
        size_type offset = inner_find(key);
        SCHECK(offset);
        return get_mapped(offset);
    }

    size_t erase(key_reference key) {
        iterator it = find(key);
        if (it != end()) {
            erase(it);
            return 1;
        }
        return 0;
    }

    iterator erase(iterator it) {
        size_type offset = it._offset;
        SCHECK(offset && !get_node(offset)->is_empty());
        size_type next = get_node(offset)->get_next();
        if (next) {
            _item_type.destruct(get_item(offset));
            size_type prev = 0;
            while (next) {
                _item_type.move_construct(get_item(offset), get_item(next));
                prev = offset;
                offset = next;
                next = get_node(next)->get_next();
            }
            deallocate_node(offset, probe_node(offset));
            get_node(prev)->set_next(0);
        } else {
            ++it;
            size_type head = get_hash_head(get_key(offset));
            _item_type.destruct(get_item(offset));
            if (offset != head) {
                size_type prev = find_node_before(head, offset);
                get_node(prev)->set_next(0);
            }
            deallocate_node(offset, probe_node(offset));
        }
        return it;
    }

    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        char* temp_item = reinterpret_cast<char*>(alloca(_node_size));
        _item_type.construct(temp_item, std::forward<Args>(args)...);
        key_reference key = _item_type.get_key(temp_item);
        std::pair<size_type, bool> itr = inner_find_to_emplace(key);
        if (itr.second) {
            _item_type.move_construct(get_item(itr.first), temp_item);
        }
        return {get_iterator(itr.first), itr.second};
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_reference key, Args&&... args) {
        std::pair<size_type, bool> itr = inner_find_to_emplace(key);
        if (itr.second) {
            _item_type.construct(get_item(itr.first), 
                  key, mapped_type(std::forward<Args>(args)...));
        }
        return {get_iterator(itr.first), itr.second};
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        std::pair<size_type, bool> itr = inner_find_to_emplace(key);
        if (itr.second) {
            _item_type.construct(get_item(itr.first), 
                  std::move(key), mapped_type(std::forward<Args>(args)...));
        }
        return {get_iterator(itr.first), itr.second};
    }

    std::pair<iterator, bool> insert(const_reference value) {
        return emplace(value);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        return emplace(std::move(value));
    }

    mapped_reference operator[](key_reference key) {
        return get_mapped(try_emplace(key).first._offset);
    }

    mapped_reference operator[](key_type&& key) {
        return get_mapped(try_emplace(std::move(key)).first._offset);
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

    void debug() {
        size_type offset = 0;
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
    size_type inner_find(key_reference key)const {
        size_t hash = _item_type.hash_function(key);
        size_type offset = _hash_space.find_offset(hash);
        if (unlikely(get_node(offset)->is_empty())) {
            return 0;
        } else {
            // 0 is end()
            if (likely(_item_type.key_eq(get_key(offset), key))) {
                return offset;
            }
            offset = get_node(offset)->get_next();
            while (likely(offset)) {
                if (likely(_item_type.key_eq(get_key(offset), key))) {
                    return offset;
                }
                offset = get_node(offset)->get_next();
            }
            return offset;
        }
    }

    std::pair<size_type, bool> inner_find_to_emplace(key_reference key) {
        size_type head = get_hash_head(key);
        PoolNode* node = get_node(head);
        if (node->is_empty()) {
            allocate_node(head);
            node->next = HEAD;
            return {head, true};
        } else if (node->is_head()) {
            size_type prev = head;
            if (_item_type.key_eq(get_key(prev), key)) {
                return {prev, false};
            }
            while (get_node(prev)->get_next()) {
                prev = get_node(prev)->get_next();
                if (_item_type.key_eq(get_key(prev), key)) {
                    return {prev, false};
                }
            }
            size_type probe = probe_node(prev);
            if (probe == 0 || _num_items >= _max_num_items) {
                rehash(_hash_space.expand_space());
                return inner_find_to_emplace(key);
            }
            allocate_node(probe);
            get_node(prev)->set_next(probe);
            return {probe, true};
        } else {
            size_type exchange = get_hash_head(get_key(head));

            _temp.clear();
            _temp.reserve(20);
            size_type prev = head;
            _temp.push_back(prev);
            while (get_node(prev)->get_next()) {
                prev = get_node(prev)->get_next();
                _temp.push_back(prev);
            }
            
            size_type probe = probe_node(prev);
            if (probe == 0 || _num_items >= _max_num_items) {
                _temp.clear();
                rehash(_hash_space.expand_space());
                return inner_find_to_emplace(key);
            }

            allocate_node(probe);
            get_node(prev)->set_next(probe);
            while (!_temp.empty()) {
                _item_type.move_construct(get_item(probe), get_item(_temp.back()));
                probe = _temp.back();
                _temp.pop_back();
            }

            exchange = find_node_before(exchange, head);
            get_node(exchange)->set_next(get_node(head)->get_next());
            node->next = HEAD;
            return {head, true};
        }
    }

    size_type find_node_before(size_type head, size_type next) {
        size_type probe = head;
        while (get_node(probe)->get_next() != next) {
            probe = get_node(probe)->get_next();
        }
        return probe;
    }

    size_type get_hash_head(key_reference key) {
        return _hash_space.find_offset(_item_type.hash_function(key));
    }

	static constexpr size_type EMPTY = 0;
    static constexpr size_type PROBE = 1;
	static constexpr size_type HEAD = 2;
    static constexpr size_type MASK = 3;
    static constexpr size_type PROBE_COUNT = 128;

    struct PoolNode {
        size_type next;
        size_type prev;

        bool is_empty() {
            return (next & MASK) == EMPTY;
        }

        bool is_head() {
            return (next & MASK) == HEAD;
        }

        size_type get_next() {
            return next & ~MASK;
        }

        void set_next(size_type offset) {
            next = next & MASK | offset;
        }
    };

    iterator get_iterator(size_type offset)const {
        return iterator(this, offset);
    }

    size_type get_iterator_next(size_type offset)const {
        offset = _hash_space.probe_offset(offset);
        while (offset && get_node(offset)->is_empty()) {
            offset = _hash_space.probe_offset(offset);
        }
        return offset;
    }

    pointer get_pointer(size_type offset)const {
        return _item_type.get_pointer(get_item(offset));
    }

    reference get_reference(size_type offset)const {
        return _item_type.get_reference(get_item(offset));
    }

    key_reference get_key(size_type offset)const {
        return _item_type.get_key(get_item(offset));
    }

    mapped_reference get_mapped(size_type offset)const {
        return _item_type.get_mapped(get_item(offset));
    }

    char* get_item(size_type offset)const {
        return reinterpret_cast<char*>(get_node(offset)) + _item_offset;
    }

    PoolNode* get_node(size_type offset)const {
        return reinterpret_cast<PoolNode*>(_hash_space[offset]);
    }

    void initialize_nodes() {
        _num_items = _hash_space.num_nodes();
        PoolNode* head = get_node(0);
        head->next = head->prev = 0;
        size_type offset = _hash_space.probe_offset(0);
        while (offset) {
            deallocate_node(offset, 0);
            offset = _hash_space.probe_offset(offset);
        }
    }

    // offset != 0, offset is EMPTY
    void allocate_node(size_type offset) {
        ++_num_items;
        PoolNode* node = get_node(offset);
        get_node(node->next)->prev = node->prev;
        get_node(node->prev)->next = node->next;
        node->next = PROBE;
    }

    void deallocate_node(size_type offset, size_type next) {
        --_num_items;
        size_type prev = get_node(next)->prev;
        get_node(offset)->next = next;
        get_node(offset)->prev = prev;
        get_node(next)->prev = offset;
        get_node(prev)->next = offset;
        get_node(offset)->next = EMPTY;
    }

    size_type probe_node(size_type probe) {
        size_type offset = probe;
        for (size_t i = 0; probe && offset - probe < PROBE_COUNT; ++i) {
            if (get_node(probe)->is_empty()) {
                return probe;
            }
            probe = _hash_space.probe_offset(probe);
        }
        return get_node(0)->prev;
    }

    ItemType _item_type;
    Serializer _serializer;
    HashSpace _hash_space;

    double _max_load_factor = 1.0;
    size_type _num_items = 0;
    size_type _max_num_items = 0;
    size_t _item_offset = 0;
    size_t _node_size = 0;

    std::vector<size_type> _temp;
    char _pad[64];
};

template<class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
using pool_hash_map = PoolHashTable<PairItemType<Key, T, Hash, Pred>, HashTableFileSerializer, ArrayHashSpace>;

}
}
}

#endif
