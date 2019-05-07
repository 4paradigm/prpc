//
// Created by sun on 2018/8/17.
//
#include "common/include/ArenaAllocator.h"
#include <map>
#include <vector>
#include "common/include/pico_log.h"
#define mallctl je_mallctl

#ifdef USE_RDMA
#include "common/include/RpcContext.h"
#endif

/*
 * 每次构造RpcArena的时候，把jemalloc的变量修改
 * 等价于 JE_MALLOC_CONF=retain:false
 */
extern bool je_opt_retain;

namespace paradigm4 {
namespace pico {
extent_hooks_t Arena::original_hooks_;

extent_alloc_t* Arena::original_alloc_ = nullptr;

extent_dalloc_t* Arena::original_dalloc_ = nullptr;

Arena::Arena() {
    if (extend_and_setup_arena()) {
        LOG(INFO) << "Set up arena: " << arena_index_;
    }
}

bool Arena::extend_and_setup_arena() {
    size_t len = sizeof(arena_index_);
    if (auto ret = mallctl("arenas.create", &arena_index_, &len, nullptr, 0)) {
        LOG(FATAL) << "Unable to extend arena: " << (ret);
    }
    flags_ = MALLOCX_ARENA(arena_index_) | MALLOCX_TCACHE_NONE;

    const auto key =
        std::string("arena.") + std::to_string(arena_index_) + ".extent_hooks";
    extent_hooks_t* hooks;
    len = sizeof(hooks);
    // Read the existing hooks
    if (auto ret = mallctl(key.c_str(), &hooks, &len, nullptr, 0)) {
        LOG(FATAL) << "Unable to get the hooks: " << (ret);
    }
    if (original_alloc_ == nullptr) {
        original_alloc_  = hooks->alloc;
        original_dalloc_ = hooks->dalloc;
    } else {
        DCHECK_EQ(original_alloc_, hooks->alloc);
        DCHECK_EQ(original_dalloc_, hooks->dalloc);
    }

    // Set the custom hook
    original_hooks_ = *hooks;

    return true;
}

void Arena::update_hook(extent_hooks_t* new_hooks) {
    const auto key =
        std::string("arena.") + std::to_string(arena_index_) + ".extent_hooks";
    //    extent_hooks_t* new_hooks = &extent_hooks_;
    if (auto ret =
            mallctl(key.c_str(), nullptr, nullptr, &new_hooks, sizeof(new_hooks))) {
        LOG(FATAL) << "Unable to set the hooks: " << (ret);
    }
}

static std::vector<std::pair<size_t, size_t>> allocated_area;

void* Arena::allocate(size_t size) {
    void* ret = je_mallocx(size, flags_);
    return ret;
}

void* Arena::reallocate(void* p, size_t size) {
    return je_rallocx(p, size, flags_);
}

void Arena::deallocate(void* p, size_t) {
    if (p != nullptr) je_dallocx(p, MALLOCX_TCACHE_NONE);
}

void Arena::deallocate(void* p, void* userData) {
    const uint64_t flags = reinterpret_cast<uint64_t>(userData);
    if (p != nullptr) je_dallocx(p, static_cast<int>(flags));
}

bool Arena::check_in(void* ptr) {
    for (auto& p : allocated_area) {
        size_t rr = reinterpret_cast<size_t>(ptr);
        if (rr >= p.first && rr < p.second) return true;
    }
    return false;
}

void* RpcArena::alloc(extent_hooks_t* extent, void* new_addr, size_t size,
    size_t alignment, bool* zero, bool* commit, unsigned arena_ind) {
    void* result =
        original_alloc_(extent, new_addr, size, alignment, zero, commit, arena_ind);
#ifdef USE_RDMA
    RdmaContext::singleton().insert((char*)result, size);
#endif
    //SLOG(INFO) << "rpc alloc in arena " << arena_ind << " size=" << size
    //           << " ret=" << result;
    return result;
}

bool RpcArena::dalloc(extent_hooks_t* extent_hooks, void* addr, size_t size,
    bool committed, unsigned arena_ind) {
    bool ret = true;
    char* raddr = (char*)addr;
    size_t rsize = size;
#ifdef USE_RDMA
    ret = RdmaContext::singleton().remove((char*)addr, size, raddr, rsize);
#endif
    //SLOG(INFO) << "rpc dalloc in arena " << arena_ind << " size=" << size
    //           << " ret=" << addr;
    if (ret) {
        original_hooks_.destroy(extent_hooks, raddr, rsize, committed, arena_ind);
    }
    return false;
}

extent_hooks_t rdma_hook;

RpcArena::RpcArena() {
    je_opt_retain = false;
    rdma_hook       = original_hooks_;
    rdma_hook.alloc = alloc;
    rdma_hook.merge = NULL;
    //rdma_hook.split = NULL;
    rdma_hook.dalloc = dalloc;
    rdma_hook.purge_forced = NULL;
    update_hook(&rdma_hook);
}
} // namespace pico
} // namespace paradigm4
