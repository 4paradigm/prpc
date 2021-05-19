//
// Created by sun on 2018/8/17.
//
#ifndef PICO_ArenaALLOCATOR_H
#define PICO_ArenaALLOCATOR_H
#include <memory>
#include <jemalloc/jemalloc.h>
//#include "macro.h"
#include "pico_memory.h"

namespace paradigm4 {
namespace pico {
namespace core {
class Arena {
public:
    // To be used as IOBuf::FreeFunction, userData should be set to
    // reinterpret_cast<void*>(getFlags()).
    static void deallocate(void* p, void* userData);

    explicit Arena();

    void* allocate(size_t size);
    void* reallocate(void* p, size_t size);
    void deallocate(void* p, size_t = 0);

    unsigned getArenaIndex() const {
        return arena_index_;
    }

    int getFlags() const {
        return flags_;
    }

protected:
    static bool check_in(void*);
    static extent_hooks_t original_hooks_;
    static extent_alloc_t* original_alloc_;
    static extent_dalloc_t* original_dalloc_;

    bool extend_and_setup_arena();
    void update_hook(extent_hooks_t* new_hooks);

    unsigned arena_index_{0};
    int flags_{0};
};

template <class ArenaType, class T, bool NoExcept = false>
class ArenaAllocator : public std::allocator<T> {
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    static ArenaType arena;

    template <class U>
    struct rebind {
        typedef ArenaAllocator<ArenaType, U, NoExcept> other;
    };

    ArenaAllocator() : std::allocator<T>() {}

    ArenaAllocator(const ArenaAllocator& other) : std::allocator<T>(other) {}

    template <class U, bool NoExcept2>
    ArenaAllocator(const ArenaAllocator<ArenaType, U, NoExcept2>& other)
        : std::allocator<T>(other) {}

    ~ArenaAllocator() {}

    pointer allocate(size_type num, const void* /*hint*/ = 0) {
        void* ptr = arena.allocate(num * sizeof(T));
        if (likely(ptr != nullptr)) return (pointer)ptr;

        return (pointer)handle_OOM(num * sizeof(T), NoExcept);
    }

    void deallocate(pointer p, size_type num) {
        arena.deallocate(p,num* sizeof(T));
    }
};

template <class ArenaType, class T, bool NoExcept>
ArenaType ArenaAllocator<ArenaType, T, NoExcept>::arena;

class RpcArena: public Arena{
public:
    RpcArena();
    static void* alloc(extent_hooks_t* extent, void* new_addr, size_t size,
        size_t alignment, bool* zero, bool* commit, unsigned arena_ind);

    static bool dalloc(extent_hooks_t* extent_hooks, void* addr, size_t size, bool committed,
        unsigned arena_ind);
};
#ifdef USE_RDMA
template <class T, bool NoExcept = false>
using RpcAllocator = ArenaAllocator<RpcArena, T, NoExcept>;
#else
template <class T, bool NoExcept = false>
using RpcAllocator = PicoAllocator<T, NoExcept>;
#endif


} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PICO_ArenaALLOCATOR_H
