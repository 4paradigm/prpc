// An extensible implementatin of HashTable, APIs are similar to std::unordered_map.Due to amortized
// rehashing trick (i.e. incremental resizing + linear hashing trick), it's more efficient than 
// std::unordered_map. And user can control the bucket growth-rate of the HashTable by setting the
// max_load_factor() to balance the memory-efficiency and the lookup/insert efficiency. 
// Disadvantages:
//      1. Not so memory-efficient as Google SparseHash, but user can control it.
//      2. Iteration of the HashTable is slow, because all the buckets including empty ones should
//         be examed.
//      3. emplace_hint / user-defined Allocator APIs are removed, which can be optimized later.
//      4. another possible improvement is to add KEY order to improve the efficiency, if the
//         comparasion and HASH(KEY) opertion is cheap, e.g. KEY = uint64_t
#ifndef PARADIGM4_PICO_TABLE_HASH_TABLE_H
#define PARADIGM4_PICO_TABLE_HASH_TABLE_H

#include <utility>
#include <memory>
#include <initializer_list>
#include <memory>
#include "pico_memory.h"
#include "pico_log.h"
#include "VirtualObject.h"

namespace paradigm4 {
namespace pico {

template<typename T, class = decltype(std::declval<T>() < std::declval<T>())>
std::true_type is_less_than_comparable_test(const T&);
std::false_type is_less_than_comparable_test(...);

/*!
 * \brief check < is usable for T
 */
template<typename T> using IsLessThanComparable
    = decltype(is_less_than_comparable_test(std::declval<T>()));

/*!
 * \brief Entry struct define instance in hashtable
 */
template<class KEY, class T>
struct HashTableEntry : public std::pair<const KEY, T> {
    size_t hash_code = 0;
    HashTableEntry<KEY, T>* next = nullptr;
    using std::pair<const KEY, T>::pair;
    //uncomment next line to support mac os.
    //constexpr HashTableEntry(const std::pair<const KEY, T>& p) :std::pair<const KEY, T>(p){}
    HashTableEntry(const std::pair<const KEY, T>& p) : std::pair<const KEY, T>(p) {}
    HashTableEntry(std::pair<const KEY, T>&& p) : std::pair<const KEY, T>(std::move(p)) {}
};


template<
    class KEY,
    class T,
    class HASH = std::hash<KEY>,
    class ALLOCATOR = PicoAllocator<HashTableEntry<KEY, T>>>
class HashTable {
public:
    typedef KEY                                key_type;
    typedef T                                  mapped_type;
    typedef std::pair<const KEY, T>            value_type;
    typedef size_t                             size_type;
    typedef HashTable<KEY, T, HASH, ALLOCATOR> self_type;
    typedef HASH                               hasher;
    typedef typename ALLOCATOR::template rebind<HashTableEntry<KEY, T>>::other allocator_type;

public:
    typedef HashTableEntry<key_type, mapped_type> Entry;
    typedef std::allocator_traits<allocator_type> allocator_helper;

public:
    /*!
     * \brief iterator struct like iterator in unordered_map.
     * defined operator ++it, it++, *it, ->, ==, !=
     */
    struct iterator {
        self_type* table = nullptr;
        Entry* entry = nullptr;
        iterator() = default;

        iterator(self_type* table, Entry* entry) : table(table), entry(entry) {}

        value_type& operator*() const {
            return *entry;
        }

        value_type* operator->() const {
            return entry;
        }

        iterator& operator++() {
            if (entry->next != nullptr) {
                entry = entry->next;
            } else {
                size_t i = table->find_first_nonempty(table->hash_to_bucket(entry->hash_code) + 1);
                if (i >= table->bucket_count()) {
                    entry = nullptr;
                } else {
                    entry = table->bucket(i);
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator it(*this);
            ++(*this);
            return it;
        }

        bool operator==(const iterator& it) const {
            return entry == it.entry;
        }

        bool operator!=(const iterator& it) const {
            return entry != it.entry;
        }
    };

    /*!
     * \brief const_iterator struct mostly same as iterator, except return a const value at operator *, ->
     *  const_iterator can be construct with iterator
     */
    struct const_iterator {
        const self_type* table = nullptr;
        const Entry* entry = nullptr;
        const_iterator() = default;

        const_iterator(const iterator& it) : table(it.table), entry(it.entry) {}

        const_iterator(const self_type* table, const Entry* entry) : table(table), entry(entry) {}

        const value_type& operator*() const {
            return *entry;
        }

        const value_type* operator->() const {
            return entry;
        }

        const_iterator& operator++() {
            if (entry->next != nullptr) {
                entry = entry->next;
            } else {
                size_t i = table->find_first_nonempty(table->hash_to_bucket(entry->hash_code) + 1);
                if (i >= table->bucket_count()) {
                    entry = nullptr;
                } else {
                    entry = table->bucket(i);
                }
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator it(*this);
            ++(*this);
            return it;
        }

        friend bool operator==(const const_iterator& it1, const const_iterator& it2) {
            return it1.entry == it2.entry;
        }

        friend bool operator!=(const const_iterator& it1, const const_iterator& it2) {
            return it1.entry != it2.entry;
        }
    };

    /*!
     * \brief local_iterator struct same as iterator, but do not use bucket when ++
     */
    struct local_iterator {
        self_type* table = nullptr;
        Entry* entry = nullptr;

        local_iterator() = default;

        local_iterator(self_type* table, Entry* entry) : table(table), entry(entry) {}

        value_type& operator*() const {
            return *entry;
        }

        value_type* operator->() const {
            return entry;
        }

        local_iterator& operator++() {
            entry = entry->next;
            return *this;
        }

        local_iterator operator++(int) {
            local_iterator it(*this);
            ++(*this);
            return it;
        }

        bool operator==(const local_iterator& it) const {
            return entry == it.entry;
        }

        bool operator!=(const local_iterator& it) const {
            return entry != it.entry;
        }
    };

    /*!
     * \brief same as local iterator, used friend func to speed up
     */
    struct const_local_iterator {
        const self_type* table = nullptr;
        const Entry* entry = nullptr;
        const_local_iterator() = default;

        const_local_iterator(const local_iterator& it) : table(it.table), entry(it.entry) {}

        const_local_iterator(const self_type* table, const Entry* entry)
            : table(table), entry(entry) {}

        const value_type& operator*() const {
            return *entry;
        }

        const value_type* operator->() const {
            return entry;
        }

        const_local_iterator& operator++() {
            entry = entry->next;
            return *this;
        }

        const_local_iterator operator++(int) {
            const_local_iterator it(*this);
            ++(*this);
            return it;
        }

        friend bool operator==(const const_local_iterator& x, const const_local_iterator& y) {
            return x.entry == y.entry;
        }

        friend bool operator!=(const const_local_iterator& x, const const_local_iterator& y) {
            return x.entry != y.entry;
        }
    };

    /*!
     * \brief construct HashTable with capacity size of Entry
     */
    explicit HashTable(size_type capacity) {
        _new_buckets.reset(new Entry*[capacity]);
        _new_buckets[0] = nullptr;
        _bucket_count = 1;
        _bucket_capacity = capacity;
        _mask = 0;
    }

    /*!
     * \brief default constructer with size 1
     */
    HashTable() : HashTable(1) {}

    /*!
     * \brief construct from exisit HashTable
     */
    template<class InputIt>
    HashTable(InputIt first, InputIt last, size_type capacity = 1) : HashTable(capacity) {
        insert(first, last);
    }

    HashTable(std::initializer_list<value_type> init, size_type capacity = 1)
        : HashTable(init.begin(), init.end(), capacity) {
    }

    HashTable(const self_type& other) : HashTable() {
        insert(other.begin(), other.end());
    }

    HashTable(self_type&& other) : HashTable() {
        swap(other);
    }

    ~HashTable() {
        clear();
    }

    /*!
     * \brief operator = copy other Hashtable or list to self with same content
     *  clear other when it is not a const HashTable
     */
    self_type& operator=(const self_type& other) {
        clear();
        insert(other.begin(), other.end());
        return *this;
    }

    self_type& operator=(self_type&& other) {
        clear();
        swap(other);
        self_type empty;
        other.swap(empty);
        return *this;
    }

    self_type& operator=(std::initializer_list<value_type> ilist) {
        clear();
        insert(ilist.begin(), ilist.end());
        return *this;
    }

    bool emtpy() const {
        return _size == 0;
    }

    size_type size() const {
        return _size;
    }

    size_type bucket_count() const {
        return _bucket_count;
    }

    size_type bucket_capacity() const {
        return _bucket_capacity;
    }

    /*!
     * \brief get bucket number of hash_code
     */
    size_type hash_to_bucket(size_t hash_code) const {
        size_type i = hash_code & _mask;
        if (i < _bucket_count) {
            return i;
        }
        return hash_code & (_mask >> 1);
    }

    /*!
     * \brief get label of first none empty bucket
     */
    size_type find_first_nonempty(size_t i) const {
        while (i < _bucket_count && bucket(i) == nullptr) {
            ++i;
        }
        return i;
    }

    /*!
     * \brief get i th element in bucket
     */
    Entry*& bucket(size_t i) {
        if (i < _new_begin) {
            return _old_buckets[i];
        }
        return _new_buckets[i];
    }

    Entry* const& bucket(size_t i) const {
        if (i < _new_begin) {
            return _old_buckets[i];
        }
        return _new_buckets[i];
    }

    /*!
     * \brief get value of key, raise exception when can not find
     */
    T& at(const KEY& key) {
        Entry*& entry = find_entry(key);
        if (entry != nullptr) {
            return entry->second;
        }
        throw std::out_of_range("key not found");
    }

    const T& at(const KEY& key) const {
        Entry* const& entry = find_entry(key);
        if (entry != nullptr) {
            return entry->second;
        }
        throw std::out_of_range("key not found");
    }

    /*!
     * \brief operator[] same as .at(), create new entry when key can not find
     */
    T& operator[](const KEY& key) {
        size_type hash_code;
        Entry*& entry = find_entry(key, hash_code);
        if (entry != nullptr) {
            return entry->second;
        }
        Entry* new_entry = allocator_helper::allocate(_allocator, 1);
        allocator_helper::construct(_allocator, new_entry, key, T());
        new_entry->next = nullptr;
        new_entry->hash_code = hash_code;
        entry = new_entry;
        insert_postprocess();
        return new_entry->second;
    }

    T& operator[](KEY&& key) {
        size_type hash_code;
        Entry*& entry = find_entry(key, hash_code);
        if (entry != nullptr) {
            return entry->second;
        }
        Entry* new_entry = allocator_helper::allocate(_allocator, 1);
        allocator_helper::construct(_allocator, new_entry, std::move(key), T());
        new_entry->next = nullptr;
        new_entry->hash_code = hash_code;
        entry = new_entry;
        insert_postprocess();
        return new_entry->second;
    }

    /*!
     * \brief get value of key, return nullptr when con not find
     */
    iterator find(const KEY& key) {
        Entry*& entry = find_entry(key);
        return {this, entry};
    }

    const_iterator find(const KEY& key) const {
        Entry* const& entry = find_entry(key);
        return {this, entry};
    }

    /*!
     * \brief return first element in table, nullptr when table is empty
     */
    iterator begin() {
        size_t i = find_first_nonempty(0);
        if (i >= _bucket_count) {
            return {this, nullptr};
        }
        return {this, bucket(i)};
    }

    const_iterator begin() const {
        size_t i = find_first_nonempty(0);
        if (i >= _bucket_count) {
            return {this, nullptr};
        }
        return {this, bucket(i)};
    }

    /*!
     * \brief same with begin()
     */
    const_iterator cbegin() const {
        return begin();
    }

    /*!
     * \brief return nullptr
     */
    iterator end() {
        return {this, nullptr};
    }

    const_iterator end() const {
        return {this, nullptr};
    }

    /*!
     * \brief same with end()
     */
    const_iterator cend() const {
        return {this, nullptr};
    }

    /*!
     * \brief delete all instance in hashtable
     */
    void clear() {
        for (size_t i = 0; i < _bucket_count; ++i) {
            Entry* entry = bucket(i);
            while (entry != nullptr) {
                Entry* next = entry->next;
                allocator_helper::destroy(_allocator, entry);
                allocator_helper::deallocate(_allocator, entry, 1);
                entry = next;
                --_size;
            }
            bucket(i) = nullptr;
        }
        SCHECK(_size == 0);
        _new_begin = 0;
        _bucket_count = 1;
        _new_buckets[0] = nullptr;
        _mask = 0;
        _old_buckets.reset();
    }

    /*!
     * \brief insert value into hashtable
     * \return if key already exisit, return its pointer and false,
     *         else return new entry pointer and true
     */
    std::pair<iterator, bool> insert(const value_type& value) {
        size_type hash_code;
        Entry*& entry = find_entry(value.first, hash_code);
        if (entry != nullptr) {
            return {{this, entry}, false};
        }
        Entry* new_entry = allocator_helper::allocate(_allocator, 1);
        allocator_helper::construct(_allocator, new_entry, value);
        new_entry->next = nullptr;
        new_entry->hash_code = hash_code;
        entry = new_entry;
        insert_postprocess();
        return {{this, new_entry}, true};
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        size_type hash_code;
        Entry*& entry = find_entry(value.first, hash_code);
        if (entry != nullptr) {
            return {{this, entry}, false};
        }
        Entry* new_entry = allocator_helper::allocate(_allocator, 1);
        allocator_helper::construct(_allocator, new_entry, std::move(value));
        new_entry->next = nullptr;
        new_entry->hash_code = hash_code;
        entry = new_entry;
        insert_postprocess();
        return {{this, new_entry}, true};
    }

    void insert(std::initializer_list<value_type> ilist) { // this won't fail, even the key exists
        insert(ilist.begin(), ilist.end());
    }

    template<class InputIt>
    void insert(InputIt first, InputIt last) { // this won't fail, even the key exists
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    /*!
     * \brief same with insert, but input args are not self_value
     */
    template<class... ARGS>
    std::pair<iterator, bool> emplace(ARGS&&... args) {
        Entry* entry = allocator_helper::allocate(_allocator, 1);
        allocator_helper::construct(_allocator, entry, std::forward<ARGS>(args)...);
        size_type hash_code;
        Entry*& found_entry = find_entry(entry->first, hash_code);
        if (found_entry != nullptr) {
            allocator_helper::destroy(_allocator, entry);
            allocator_helper::deallocate(_allocator, entry, 1);
            return {{this, found_entry}, false};
        }
        entry->next = nullptr;
        entry->hash_code = hash_code;
        found_entry = entry;
        insert_postprocess();
        return {{this, entry}, true};
    }

    /*!
     * \brief erase element from hashtable
     */
    void erase(const_iterator pos) {
        Entry*& entry = find_entry(pos->first);
        SCHECK(entry != nullptr);
        Entry* next = entry->next;
        allocator_helper::destroy(_allocator, entry);
        allocator_helper::deallocate(_allocator, entry, 1);
        entry = next;
        --_size;
        move_bucket();
    }

    void erase(const_iterator first, const_iterator last) {
        const_iterator it = first;
        while (it != last) {
            ++first;
            erase(it);
            it = first;
        }
    }

    size_type erase(const key_type& key) {
        Entry*& entry = find_entry(key);
        if (entry == nullptr) {
            return 0;
        }
        Entry* next = entry->next;
        allocator_helper::destroy(_allocator, entry);
        allocator_helper::deallocate(_allocator, entry, 1);
        entry = next;
        --_size;
        move_bucket();
        return 1;
    }

    /*!
     * \brief swap self with other
     */
    void swap(self_type& other) {
        std::swap(_size, other._size);
        std::swap(_mask, other._mask);
        std::swap(_bucket_count, other._bucket_count);
        std::swap(_bucket_capacity, other._bucket_capacity);
        std::swap(_new_begin, other._new_begin);
        std::swap(_max_load_factor, other._max_load_factor);
        std::swap(_allocator, other._allocator);
        _new_buckets.swap(other._new_buckets);
        _old_buckets.swap(other._old_buckets);
    }

    float load_factor() const {
        return _size / (1.0f * _bucket_count);
    }

    float max_load_factor() const {
        return _max_load_factor;
    }

    void max_load_factor(float ml) {
        _max_load_factor = ml;
    }

    /*！
     * \brief resize bucket, point _new_buckets to the end of old bucket.
     *  make new bucket double size of old bucket, each time exec move_bucket move one
     *  ele in position _new_begin from old to new
     */
    void reserve(size_type size) {
        if (size <= _bucket_capacity) {
            return;
        }
        SCHECK(_new_begin == 0);
        _old_buckets.reset(new Entry*[size]);
        _new_buckets.swap(_old_buckets);
        _new_begin = _bucket_count;
        _bucket_capacity = size;
    }

    // local iterators
    local_iterator begin(size_type n) {
        if (n < _bucket_count) {
            return {this, bucket(n)};
        }
        return {this, nullptr};
    }

    const_local_iterator begin(size_type n) const {
        if (n < _bucket_count) {
            return {this, bucket(n)};
        }
        return {this, nullptr};
    }

    const_local_iterator cbegin(size_type n) const {
        return begin(n);
    }

    local_iterator end(size_type) {
        return {this, nullptr};
    }

    const_local_iterator end(size_type) const {
        return {this, nullptr};
    }

    const_local_iterator cend(size_type) const {
        return {this, nullptr};
    }

    void dump() const {
        SLOG(INFO) << "***********";
        for (size_t i = 0; i < _bucket_count; ++i) {
            SLOG(INFO) << "--------------";
            SLOG(INFO) << "| " << i << " | " << bucket(i) << " |";
            Entry* entry = bucket(i);
            while (entry != nullptr) {
                SLOG(INFO) << "->| " << entry << " (" << entry->first << "," << entry->second << ","
                    << entry->hash_code << "," << (entry->hash_code & _mask)
                    << ") | " << entry->next << " |";
                entry = entry->next;
            }
            SLOG(INFO) << "--------------";
        }
        SLOG(INFO) << "***********";
        dump_stats();
    }

    void dump_stats() const {
        SLOG(INFO) << "size: " << _size;
        SLOG(INFO) << "non-empty buckets: " << nonempty_bucket_num();
        SLOG(INFO) << "bucket: " << _bucket_count << "/" << _bucket_capacity;
        SLOG(INFO) << "load factor: " << load_factor() * 100 << "%";
        SLOG(INFO) << "max load factor: " << _max_load_factor;
    }

    size_type nonempty_bucket_num() const { // not efficient o(bucket_count)
        size_type count = 0;
        for (size_type i = 0; i < _bucket_count; ++i) {
            if (bucket(i) != nullptr) {
                ++count;
            }
        }
        return count;
    }

private:

    /*!
     * \brief get hash_code of key, and calc init bucket label, check from it one by one
     */
    Entry*& find_entry(const KEY& key) {
        size_type hash_code = _hash(key);
        size_type i = hash_to_bucket(hash_code);
        Entry** ptr = &bucket(i);
        while (*ptr != nullptr) {
            if ((*ptr)->hash_code == hash_code && (*ptr)->first == key) {
                return *ptr;
            }
            ptr = &(*ptr)->next;
        }
        return *ptr;
    }

    Entry* const& find_entry(const KEY& key) const {
        size_type hash_code = _hash(key);
        size_type i = hash_to_bucket(hash_code);
        Entry* const* ptr = &bucket(i);
        while (*ptr != nullptr) {
            if ((*ptr)->hash_code == hash_code && (*ptr)->first == key) {
                return *ptr;
            }
            ptr = &(*ptr)->next;
        }
        return *ptr;
    }

    Entry*& find_entry(const KEY& key, size_type& hash_code) {
        hash_code = _hash(key);
        size_type i = hash_to_bucket(hash_code);
        Entry** ptr = &bucket(i);
        while (*ptr != nullptr) {
            if ((*ptr)->hash_code == hash_code && (*ptr)->first == key) {
                return *ptr;
            }
            ptr = &(*ptr)->next;
        }
        return *ptr;
    }

    /*!
     * \brief increase bucket used num when size is large,
     *  move orgin ele into new bucket
     */
    void add_bucket() {
        if (_bucket_count >= _bucket_capacity) {
            reserve(_bucket_capacity * 2);
        }
        if ((_bucket_count & (_bucket_count - 1)) == 0) { // pow of 2
            _mask = _bucket_count * 2 - 1;
        }
        // rehashing here
        size_t to_i = _bucket_count++;
        size_t from_i = to_i & (_mask >> 1);
        Entry** src = &bucket(from_i);
        Entry** dest = &bucket(to_i);
        *dest = nullptr;
        while ((*src) != nullptr) {
            if (((*src)->hash_code & _mask) == to_i) {
                *dest = *src;
                *src = (*src)->next;
                dest = &(*dest)->next;
                *dest = nullptr;
            } else {
                src = &(*src)->next;
            }
        }
    }

    /*!
     * \brief move one ele from old bucket to new bucket
     */
    void move_bucket() {
        if (_new_begin > 0) {
            --_new_begin;
            _new_buckets[_new_begin] = _old_buckets[_new_begin];
            if (_new_begin == 0) {
                _old_buckets.reset();
            }
        }
    }

    /*!
     * \brief exec move bucket, exec this func in each insert to reduce time consume
     */
    void insert_postprocess() {
        ++_size;
        if (_size > _max_load_factor * _bucket_count) {
            add_bucket();
            move_bucket();
            add_bucket();
        }
        move_bucket();
    }

private:
    hasher _hash;
    size_type _size = 0;
    size_type _mask = 0;
    size_type _bucket_count = 0;
    size_type _bucket_capacity = 0;
    size_type _new_begin = 0; // the start postion of new buckets
    float _max_load_factor = 0.75f;
    std::unique_ptr<Entry*[]> _new_buckets;
    std::unique_ptr<Entry*[]> _old_buckets;
    allocator_type _allocator;
};

} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_TABLE_HASH_TABLE_H
