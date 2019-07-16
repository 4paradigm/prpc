//
// Created by 孙承根 on 2017/12/29.
//

#ifndef PICO_MEMORY_H
#define PICO_MEMORY_H

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include "gflags/gflags.h"
#include "pico_log.h"
#include "common.h"
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif // likely

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif // unlikely
extern "C" {
void* pico_malloc(size_t size);
void* pico_valloc(size_t size);
void* pico_calloc(size_t n, size_t size);
void* pico_realloc(void* ptr, size_t size);
void* pico_memalign(size_t align, size_t size);
int pico_posix_memalign(void** res, size_t align, size_t size);
void pico_free(void* ptr);
void pico_gc();
void pico_gc_pmem();
void pico_gc_vmem();
size_t pico_malloc_size(void* ptr);
void pico_memstats(std::ostream& out);
} // end extern C

#define pico_print_memstats()      \
    do {                           \
        pico_memstats(SLOG(INFO)); \
    } while (0)

namespace paradigm4 {
namespace pico {
namespace core {
class Memory {
public:
    // init
    void initialize();
    void finalize();
    // stack
    size_t get_max_stack_size();

    // pmem
    size_t get_total_pmem();
    size_t get_used_pmem();
    void set_max_pmem(size_t size);
    size_t get_max_pmem();

    // vmem
    void set_max_vmem(size_t size);
    size_t get_max_vmem();
    size_t get_used_vmem();

    // managed vmem
    void set_max_managed_vmem(size_t size);
    size_t get_max_managed_vmem();
    size_t get_managed_vmem();


    //
    void memstats(std::ostream& out);

    constexpr static Memory& singleton() {
        return *reinterpret_cast<Memory*>(0);
    };

private:
    Memory() {}
};

inline Memory& pico_mem() { return Memory::singleton(); }
void* handle_OOM(std::size_t size, bool nothrow);

template <bool IsNoExcept>
void* newImpl(std::size_t size) noexcept(IsNoExcept) {
    void* ptr = pico_malloc(size);
    if (likely(ptr != nullptr))
        return ptr;

    return handle_OOM(size, IsNoExcept);
}

template <typename Allocator>
class AllocatorDeleter {
public:
    template <typename T>
    void operator()(T* p) {
        Allocator().deallocate(p, 0);
    }
};

template <class T, bool NoExcept = false>
class PicoAllocator : public std::allocator<T> {
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    template <class U>
    struct rebind {
        typedef PicoAllocator<U> other;
    };

    PicoAllocator() : std::allocator<T>() {}

    PicoAllocator(const PicoAllocator& other) : std::allocator<T>(other) {}

    template <class U>
    PicoAllocator(const PicoAllocator<U>& other) : std::allocator<T>(other) {}

    ~PicoAllocator() {}

    pointer allocate(size_type num, const void* /*hint*/ = 0) {
        return static_cast<pointer>(newImpl<NoExcept>(num * sizeof(T)));
    }
    pointer reallocate(pointer p, size_type n) {
        return static_cast<pointer>(pico_realloc(p, n * sizeof(value_type)));
    }

    void deallocate(pointer p, size_type num = 0) {
        (void)(num);
        pico_free(p);
    }
};

template <int id>
class MemPool {
public:
    typedef std::atomic_size_t allocated_size_t;
    typedef volatile size_t max_size_t;

    template <class T, int PoolId, bool NoExcept>
    friend class PicoLimitedAllocator;
    static size_t current_size() { return _allocated_size; }
    static void set_max_size(size_t size) { _max_size = size; }
    static size_t get_max_size() { return _max_size; }
    // private:
    static allocated_size_t _allocated_size;
    static max_size_t _max_size;
};
template <int id>
typename MemPool<id>::max_size_t MemPool<id>::_max_size =
    std::numeric_limits<size_t>::max();
template <int id>
typename MemPool<id>::allocated_size_t MemPool<id>::_allocated_size;

template <class T, int PoolId = 0, bool NoExcept = false>
class PicoLimitedAllocator : public std::allocator<T> {
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef MemPool<PoolId> pool_t;

    static size_t current_size() { return pool_t::current_size(); }
    static void set_max_size(size_t size) { pool_t::set_max_size(size); }
    static size_t get_max_size(size_t size) { return pool_t::get_max_size(); }
    template <class U>
    struct rebind {
        typedef PicoLimitedAllocator<U> other;
    };

    PicoLimitedAllocator() : std::allocator<T>() {}

    PicoLimitedAllocator(const PicoLimitedAllocator& other) : std::allocator<T>(other) {}

    template <class U>
    PicoLimitedAllocator(const PicoLimitedAllocator<U>& other)
        : std::allocator<T>(other) {}

    ~PicoLimitedAllocator() {}

    pointer allocate(size_type num, const void* /*hint*/ = 0) {
        if (unlikely(pool_t::_allocated_size + num * sizeof(T) > pool_t::_max_size)) {
            if (!NoExcept)
                std::__throw_bad_alloc();
            return nullptr;
        }
        void* ptr = newImpl<NoExcept>(num * sizeof(T));
        if (likely(ptr != nullptr)) {
            size_t real_size = pico_malloc_size(ptr);
            pool_t::_allocated_size += real_size;
            if (unlikely(pool_t::_allocated_size > pool_t::_max_size)) {
                deallocate(static_cast<pointer>(ptr), num);
                if (!NoExcept)
                    std::__throw_bad_alloc();
                return nullptr;
            }
        }
        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer ptr, size_type /*num*/) {
        size_t real_size = pico_malloc_size(ptr);
        pool_t::_allocated_size -= real_size;
        pico_free(ptr);
    }
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#include <array>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <valarray>
#include <vector>

namespace paradigm4 {
namespace pico {
namespace core {

template <typename T, typename _Alloc = PicoAllocator<T>>
using vector = std::vector<T, _Alloc>;
template <typename T, typename _Alloc = PicoAllocator<T>>
using list = std::list<T, _Alloc>;
template <typename T, typename _Alloc = PicoAllocator<T>>
using deque = std::deque<T, _Alloc>;
template <typename _Key, typename _Tp, typename _Compare = std::less<_Key>,
    typename _Alloc = PicoAllocator<std::pair<const _Key, _Tp>>>
using map           = std::map<_Key, _Tp, _Compare, _Alloc>;
template <typename _Key, typename _Compare = std::less<_Key>,
    typename _Alloc = PicoAllocator<_Key>>
using set           = std::set<_Key, _Compare, _Alloc>;

template <class _Key, class _Tp, class _Hash = std::hash<_Key>,
    class _Pred     = std::equal_to<_Key>,
    class _Alloc    = PicoAllocator<std::pair<const _Key, _Tp>>>
using unordered_map = std::unordered_map<_Key, _Tp, _Hash, _Pred, _Alloc>;

template <class _Value, class _Hash = std::hash<_Value>,
    class _Pred = std::equal_to<_Value>, class _Alloc = PicoAllocator<_Value>>
using unordered_set = std::unordered_set<_Value, _Hash, _Pred, _Alloc>;

template <typename _Tp, typename... _Args>
inline std::shared_ptr<_Tp> make_shared(_Args&&... __args) {
    typedef typename std::remove_const<_Tp>::type _Tp_nc;
    return std::allocate_shared<_Tp>(
        PicoAllocator<_Tp_nc>(), std::forward<_Args>(__args)...);
}

template <typename Alloc>
inline std::shared_ptr<typename std::remove_reference_t<Alloc>::value_type> alloc_shared(Alloc&& alloc, size_t size) {
    typedef std::remove_reference_t<Alloc> alloc_t;
    typedef typename alloc_t::value_type value_type;
    return std::shared_ptr<value_type>(alloc.allocate(size), AllocatorDeleter<alloc_t>(),
        PicoAllocator<value_type>());
}

template <typename _Tp, typename... _Args>
inline _Tp* pico_new(_Args&&... __args) {
    _Tp* p = static_cast<_Tp*>(pico_malloc(sizeof(_Tp)));
    new(p) _Tp(std::forward<_Args>(__args)...);
    return p;
}

template <typename _Tp>
inline void pico_delete(_Tp* p) {
    p->~_Tp();
    pico_free(p);
}

template<class _Tp>
struct PicoDelete {
    PicoDelete() = default;
    template<typename _Up, typename = typename
          std::enable_if<std::is_convertible<_Up*, _Tp*>::value>::type>
    PicoDelete(const PicoDelete<_Up>&) noexcept { }
    void operator()(_Tp* p)const {
        pico_delete(p);
    }
};

template <typename T, typename _Deleter = PicoDelete<T>>
using unique_ptr = std::unique_ptr<T, _Deleter>;

template <typename _Tp, typename... _Args>
inline core::unique_ptr<_Tp> make_unique(_Args&&... __args) {
    return core::unique_ptr<_Tp>(pico_new<_Tp>(std::forward<_Args>(__args)...));
}

// template <class T>
// PicoAllocator<T> & global_pico_allocator(){
//    static PicoAllocator<T> alloca;
//    return alloca;
//}
// template <class T>
// auto & global_pico_deleter(){
//   static  auto d=[](void * p){
//     global_pico_allocator<T>().deallocate(p,0);
//    };
//    return d;
//}
// template<typename T, typename... Args>
// std::unique_ptr<T> make_unique(Args&&... args)
//{
//    typedef std::allocator_traits<PicoAllocator<T>> allocator_helper;
//    T* new_entry = allocator_helper::allocate(global_pico_allocator<T>(), 1);
//    allocator_helper::construct(global_pico_allocator<T>(),new_entry,std::forward<Args>(args)...);
//    return std::unique_ptr<T>(new_entry,global_pico_deleter());
//}
} // namespace core
} // namespace pico
} // namespace paradigm4
//#include "pico_memory.cpp"
#endif // PICO_MEMORY_H
