#ifndef PARADIGM4_PICO_LAZY_ARCHIVE_H
#define PARADIGM4_PICO_LAZY_ARCHIVE_H

#include <typeindex>
#include "Archive.h"
#include "ThreadedTimer.h"

#ifdef USE_RDMA
#include "common/include/RdmaContext.h"
#endif

namespace paradigm4 {
namespace pico {

struct data_block_t {
    char* data;
    uint32_t length;
    std::function<void(void*)> deleter = [](void*) {};
#ifdef USE_RDMA
    uint32_t lkey;
#endif
    typedef RpcAllocator<char> allocator_type;

    static allocator_type allocator() {
        static allocator_type ins;
        return ins;
    }

    data_block_t(const data_block_t&) = delete;
    data_block_t& operator=(const data_block_t&) = delete;

    data_block_t(data_block_t&& o) {
        *this = std::move(o);
    }

    data_block_t& operator=(data_block_t&& o) {
        length = o.length;
        deleter = std::move(o.deleter);
#ifdef USE_RDMA
        lkey = o.lkey;
        o.lkey = 0;
#endif
        data = o.data;
        o.data = nullptr;
        o.length = 0;
        o.deleter = [](void*) {};
        return *this;
    }

    data_block_t() {
        data = nullptr;
        length = 0;
    }

    // 有所有权
    data_block_t(uint32_t len) {
        length = len;
        if (len == 0) {
            data = nullptr;
            return;
        }
        data = (char*)allocator().allocate(len);
        deleter = [](void* data) { allocator().deallocate((char*)data, 1); };
#ifdef USE_RDMA
        auto mr
              = RdmaContext::singleton().get(data, length);
        if (mr) {
            lkey = mr->mr->lkey;
        }
#endif
    }

    // 没有所有权
    data_block_t(char* data, uint32_t length) : data(data), length(length) {}

    ~data_block_t() {
        deleter(data);
    }
};

class ArchiveReader {
public:
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;

    template<class T> 
    ArchiveReader& operator>>(T& value) {
        _ar >> value;
        return *this;
    }
    bool is_exhausted() {
        return _ar.is_exhausted();
    }

private:
    ArchiveReader() {}
    ArchiveReader(ArchiveReader&& other): _ar(std::move(other._ar)) {}
    ArchiveReader& operator=(ArchiveReader&& other) {
        _ar.release();
        _ar = std::move(other._ar);
        return *this;
    }
    ~ArchiveReader() {
        _ar.release();
    }
    BinaryArchive _ar;
    friend class LazyArchive;
};

class ArchiveWriter {
public:
    ArchiveWriter(const ArchiveWriter&) = delete;
    ArchiveWriter& operator=(const ArchiveWriter&) = delete;
    ~ArchiveWriter() {}
    template<class T>
    ArchiveWriter& operator<<(const T& value) {
        _ar << value;
        return *this;
    }
private:
    BinaryArchive& _ar;
    ArchiveWriter(BinaryArchive& ar): _ar(ar) {}
    friend class LazyArchive;
};

class SharedArchiveReader {
public:
    SharedArchiveReader(const SharedArchiveReader&) = delete;
    SharedArchiveReader& operator=(const SharedArchiveReader&) = delete;

    template <class T>
    std::enable_if_t<std::is_trivially_copyable<T>::value>
          get_shared_uncheck(T*& p, size_t& size, std::unique_ptr<data_block_t>& own) {
        size = _data[_pos].length / sizeof(T);
        SCHECK(_data[_pos].length == size * sizeof(T));
        p = reinterpret_cast<T*>(_data[_pos].data);

        own = std::make_unique<data_block_t>(std::move(_data[_pos]));
        ++_pos;
    }

    bool is_exhausted() {
        return _pos == _data.size();
    }
private:
    SharedArchiveReader() {}
    SharedArchiveReader(SharedArchiveReader&&) = default;
    SharedArchiveReader& operator=(SharedArchiveReader&&) = default;
    pico::vector<data_block_t> _data;
    size_t _pos = 0;
    friend class LazyArchive;
};

class SharedArchiveWriter {
public:
    SharedArchiveWriter(const SharedArchiveWriter&) = delete;
    SharedArchiveWriter& operator=(const SharedArchiveWriter&) = delete; 

    // XXX 希望有所有权吗
    template <class T>
    std::enable_if_t<std::is_trivially_copyable<T>::value>    
    put_shared_uncheck(T* p, size_t size) {
        uint32_t length = size * sizeof(T);
        _data.push_back({reinterpret_cast<char*>(p), length});
    }

private:
    pico::vector<data_block_t>& _data;
    SharedArchiveWriter(pico::vector<data_block_t>& data): _data(data) {}
    friend class LazyArchive;
};

class LazyArchive {
public:
    LazyArchive() : _meta_ar(true) {}
    LazyArchive(const LazyArchive&) = delete;
    LazyArchive& operator=(const LazyArchive&) = delete;

    LazyArchive(LazyArchive&& o) = default;

    LazyArchive& operator=(LazyArchive&& o) = default;

    template<class T, class =
          decltype(pico_serialize(
          std::declval<ArchiveWriter&>(), 
          std::declval<SharedArchiveWriter&>(), 
          std::declval<T&>()))>
    std::enable_if_t<!std::is_reference<T>::value, LazyArchive&>
    operator<<(T&& value) {
        _lazy.push_back(make_lazy(std::move(value)));
        return *this;
    }

    template<class T, class =
          decltype(pico_deserialize(
          std::declval<ArchiveReader&>(), 
          std::declval<SharedArchiveReader&>(), 
          std::declval<T&>()))>
    LazyArchive& operator>>(T& value) {
        if (_ar.is_exhausted() && _shared.is_exhausted()) {
            SCHECK(_cur < _lazy.size()); 
            _lazy[_cur]->get(&value, typeid(T));
            ++_cur;
        } else {
            pico_deserialize(_ar, _shared, value);
        }
        return *this;
    }

    void attach(pico::vector<data_block_t>&& data) {
        if (!data.empty()) {
            data_block_t& ar_block = data.back();
            _ar._ar.set_read_buffer(ar_block.data, ar_block.length);
        }
        _shared._data = std::move(data);
        _shared._pos = 0;
        _lazy.clear();
        _cur = 0;
    }

    void apply(pico::vector<data_block_t>& data) {
        if (!_lazy.empty()) {
            _meta_ar.clear();
            ArchiveWriter arw(_meta_ar);
            SharedArchiveWriter sar(data);
            while (_cur < _lazy.size()) {
                _lazy[_cur]->serialize(arw, sar);
                ++_cur;
            }
            data.push_back({_meta_ar.buffer(), static_cast<uint32_t>(_meta_ar.length())});
        }
    }

private:
    struct LazyBase {
        virtual ~LazyBase() {}
        virtual void get(void* p, const std::type_info& t) = 0;
        virtual void serialize(ArchiveWriter& writer, SharedArchiveWriter& shared_writer) = 0;
    };

    template<class T>
    struct Lazy: LazyBase {
        T value;
        Lazy(T&& value): value(std::move(value)) {}
        ~Lazy() override {}
        void get(void* p, const std::type_info& t) override {
            SCHECK(std::type_index(typeid(T)) == std::type_index(t));
            T& out = *static_cast<T*>(p);
            out = std::move(value);
        }
        virtual void serialize(ArchiveWriter& writer, SharedArchiveWriter& shared_writer) override {
            pico_serialize(writer, shared_writer, value);
        }
    };

    struct LazyDeleter {
        void operator()(LazyBase* p)const {
            p->~LazyBase();
            pico_free(p);
        }
    };

    using LazyPtr = std::unique_ptr<LazyBase, LazyDeleter>;

    template<class T>
    LazyPtr make_lazy(T&& value) {
        void* p = pico_malloc(sizeof(Lazy<T>));
        ::new (p) Lazy<T>(std::forward<T>(value));
        return LazyPtr(static_cast<Lazy<T>*>(p));
    }

    BinaryArchive _meta_ar;
    ArchiveReader _ar;
    SharedArchiveReader _shared;
    pico::vector<LazyPtr> _lazy;
    size_t _cur = 0;
};

template<class T>
std::enable_if_t<IsBinarySerializable<T>::value && !std::is_const<T>::value>
pico_serialize(ArchiveWriter& ar, SharedArchiveWriter&, T& value) {
    ar << value;
}

template<class T>
std::enable_if_t<IsBinarySerializable<T>::value>
pico_deserialize(ArchiveReader& ar, SharedArchiveReader&, T& value) {
    ar >> value;
}

inline void pico_serialize(ArchiveWriter&, SharedArchiveWriter& shared, data_block_t& data) {
    shared.put_shared_uncheck(data.data, data.length);
}

inline void pico_deserialize(ArchiveReader&, SharedArchiveReader& shared, data_block_t& data) {
    char* ptr;
    size_t len;
    std::unique_ptr<data_block_t> own;
    shared.get_shared_uncheck(ptr, len, own);
    data = std::move(*own);
}

inline void pico_serialize(ArchiveWriter&, SharedArchiveWriter& shared, BinaryArchive& ar) {
    shared.put_shared_uncheck(ar.cursor(), ar.readable_length());
}

inline void pico_deserialize(ArchiveReader&, SharedArchiveReader& shared, BinaryArchive& ar) {
    char* ptr;
    size_t len;
    std::unique_ptr<data_block_t> own;
    shared.get_shared_uncheck(ptr, len, own);
    auto deleter = own.get_deleter();
    auto own_ptr = own.release();
    ar.set_read_buffer(ptr, len, [deleter, own_ptr](void*) { deleter(own_ptr); });
}

template<class T>
std::enable_if_t<std::is_trivially_copyable<T>::value && !std::is_same<T, bool>::value>
pico_serialize(ArchiveWriter&, SharedArchiveWriter& shared, std::vector<T>& vec) {
    shared.put_shared_uncheck(vec.data(), vec.size());
}

template<class T>
std::enable_if_t<!std::is_trivially_copyable<T>::value || std::is_same<T, bool>::value>
pico_serialize(ArchiveWriter& ar, SharedArchiveWriter& shared, std::vector<T>& vec) {
    ar << vec.size();
    for (auto& val : vec) {
        pico_serialize(ar, shared, val);
    }
}

template<class T>
std::enable_if_t<std::is_trivially_copyable<T>::value && !std::is_same<T, bool>::value>
pico_deserialize(ArchiveReader&, SharedArchiveReader& shared, std::vector<T>& vec) {
    T* data; 
    size_t size;
    std::unique_ptr<data_block_t> own;
    shared.get_shared_uncheck(data, size, own);
    vec.resize(size);
    memcpy(vec.data(), data, size * sizeof(T));
}

template<class T>
std::enable_if_t<!std::is_trivially_copyable<T>::value || std::is_same<T, bool>::value>
pico_deserialize(ArchiveReader& ar, SharedArchiveReader& shared, std::vector<T>& vec) {
    size_t size;
    ar >> size;
    vec.resize(size);
    for (auto& val : vec) {
        pico_deserialize(ar, shared, val);
    }
}


template<class KEY, class VALUE>
void pico_serialize(ArchiveWriter& ar, SharedArchiveWriter& shared, std::map<KEY, VALUE>& m) {
    ar << m.size();
    for (auto& item : m) {
        ar << item.first;
        pico_serialize(ar, shared, item.second);
    }
}


template<class KEY, class VALUE>
void pico_deserialize(ArchiveReader& ar, SharedArchiveReader& shared, std::map<KEY, VALUE>& m) {
    size_t size;
    ar >> size;
    m.clear();
    for (size_t i = 0; i < size; i++) {
        KEY k;
        ar >> k;
        VALUE v;
        pico_deserialize(ar, shared, v);
        SCHECK(m.emplace(std::move(k), std::move(v)).second);
    }
}

template<class T1, class T2>
void pico_serialize(ArchiveWriter& ar, SharedArchiveWriter& shared, std::pair<T1, T2>& p) {
    pico_serialize(ar, shared, p.first);
    pico_serialize(ar, shared, p.second);
}

template<class T1, class T2>
void pico_deserialize(ArchiveReader& ar, SharedArchiveReader& shared, std::pair<T1, T2>& p) {
    pico_deserialize(ar, shared, p.first);
    pico_deserialize(ar, shared, p.second);
};

} // namespace pico
} // namespace paradigm4

#endif //PARADIGM4_PICO_LAZY_ARCHIVE_H
