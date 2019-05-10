//
// Created by 孙承根 on 2017/12/29.
//

#include "pico_memory.h"
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include "Monitor.h"

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <malloc/malloc.h>

#define HAVE_TASKINFO 1

#endif

#ifdef __linux__
#include <features.h>
#include <linux/version.h>
#include <malloc.h>
#define HAVE_PROC_STAT 1
#define HAVE_PROC_MAPS 1
#define HAVE_PROC_SMAPS 1
#define HAVE_PROC_SOMAXCONN 1
#endif

/* Explicitly override malloc/free etc when using tcmalloc. */
#if defined(USE_TCMALLOC)
#define SYS_malloc(size) tc_malloc(size)
#define SYS_calloc(count, size) tc_calloc(count, size)
#define SYS_realloc(ptr, size) tc_realloc(ptr, size)
#define SYS_free(ptr) tc_free(ptr)
#elif defined(USE_JEMALLOC)
#include "jemalloc/jemalloc.h"
#define JEMALLOC_WRAPPER(func) je_##func
#define SYS_malloc(size) JEMALLOC_WRAPPER(malloc)(size)
#define SYS_calloc(count, size) JEMALLOC_WRAPPER(calloc)(count, size)
#define SYS_realloc(ptr, size) JEMALLOC_WRAPPER(realloc)(ptr, size)
#define SYS_valloc(size) JEMALLOC_WRAPPER(valloc)(size)

#define SYS_free(ptr) JEMALLOC_WRAPPER(free)(ptr)
#define SYS_mallocx(size, flags) JEMALLOC_WRAPPER(mallocx)(size, flags)
#define SYS_dallocx(ptr, flags) JEMALLOC_WRAPPER(dallocx)(ptr, flags)
#define SYS_memalign(align, size) JEMALLOC_WRAPPER(aligned_alloc)(align, size)
#define SYS_malloc_usable_size(ptr) JEMALLOC_WRAPPER(malloc_usable_size)(ptr)
volatile extern size_t JEMALLOC_WRAPPER(allocated_size);
extern size_t JEMALLOC_WRAPPER(dallocated_size);
extern size_t JEMALLOC_WRAPPER(mmap_size);
extern size_t JEMALLOC_WRAPPER(committed_size);
volatile extern size_t JEMALLOC_WRAPPER(max_alloc_size);
#else
#define SYS_malloc(size) ::malloc(size)
#define SYS_valloc(size) ::valloc(size)
#define SYS_calloc(count, size) ::calloc(count, size)
#define SYS_realloc(ptr, size) ::realloc(ptr, size)
#define SYS_free(ptr) ::free(ptr)
#ifdef __linux__

#define SYS_memalign(align, size) ::memalign(align, size)
#define SYS_malloc_usable_size(ptr) ::malloc_usable_size(ptr)
#endif
#ifdef __APPLE__
inline void* SYS_memalign(size_t align,size_t size) {
    void * res;
    ::posix_memalign(&res,align, size);
    return res;
}
#define SYS_malloc_usable_size(ptr) ::malloc_size(ptr)
#endif
#ifdef OVERRIDE_SYSTEM_MALLOC
#undef OVERRIDE_SYSTEM_MALLOC
#endif
#ifdef __APPLE__

#endif

#endif
namespace paradigm4 {
namespace pico {
namespace core {

DEFINE_uint64(pmem_monitor_interval_ms, 4000, "interval of pmem/rss monitor (ms)");
DEFINE_uint64(max_pmem_mb, -1, "max physical memory (mb), -1 means unlimited");
DEFINE_uint64(max_vmem_mb, -1, "max virtual memory (mb), -1 means unlimited");
DEFINE_double(vmem_ratio, 2.1, "ratio of vmem and pmem");
DEFINE_bool(log_memstats, false, "print memstats");


/* Returns the size of physical memory (RAM) in bytes.
 * It looks ugly, but this is the cleanest way to achive cross platform results.
 * Cleaned up from:
 *
 * http://nadeausoftware.com/articles/2012/09/c_c_tip_how_get_physical_memory_size_system
 *
 * Note that this function:
 * 1) Was released under the following CC attribution license:
 *    http://creativecommons.org/licenses/by/3.0/deed.en_US.
 * 2) Was originally implemented by David Robert Nadeau.
 * 3) Was modified for Redis by Matt Stancliff.
 * 4) This note exists in order to comply with the original license.
 */
size_t Memory::get_total_pmem() {
#if defined(__unix__) || defined(__unix) || defined(unix) || \
    (defined(__APPLE__) && defined(__MACH__))
#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
    int mib[2];
    mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
    mib[1] = HW_MEMSIZE; /* OSX. --------------------- */
#elif defined(HW_PHYSMEM64)
    mib[1] = HW_PHYSMEM64; /* NetBSD, OpenBSD. --------- */
#endif
    int64_t size = 0; /* 64-bit */
    size_t len   = sizeof(size);
    if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
        return (size_t)size;
    return 0L; /* Failed? */

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    /* FreeBSD, Linux, OpenBSD, and Solaris. -------------------- */
    return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGESIZE);

#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
    /* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
    int mib[2];
    mib[0]            = CTL_HW;
#if defined(HW_REALMEM)
    mib[1]            = HW_REALMEM; /* FreeBSD. ----------------- */
#elif defined(HW_PYSMEM)
    mib[1] = HW_PHYSMEM; /* Others. ------------------ */
#endif
    unsigned int size = 0;          /* 32-bit */
    size_t len        = sizeof(size);
    if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
        return (size_t)size;
    return 0L; /* Failed? */
#else
    return 0L; /* Unknown method to get the data. */
#endif
#else
    return 0L; /* Unknown OS. */
#endif
}
size_t Memory::get_used_pmem() {
#ifdef HAVE_TASKINFO
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
        return 0;
    task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

    return t_info.resident_size;
#elif HAVE_PROC_STAT
    static size_t used_pmem = 0;
    FILE* statm             = fopen("/proc/self/statm", "r");
    if (statm == nullptr) {
        return used_pmem;
    }
    size_t vm = 0, rss = 0, share = 0, text = 0, lib = 0, data = 0, dt = 0;
    fscanf(statm, "%lu%lu%lu%lu%lu%lu%lu", &vm, &rss, &share, &text, &lib, &data, &dt);
    fclose(statm);
    int page  = sysconf(_SC_PAGESIZE);
    used_pmem = rss * page;
    return used_pmem;
#else
#error ""
#endif
}
size_t Memory::get_used_vmem() {
#ifdef HAVE_TASKINFO
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
        return 0;
    task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
    return t_info.virtual_size;
#elif HAVE_PROC_STAT
    static size_t used_vmem = 0;
    FILE* statm             = fopen("/proc/self/statm", "r");
    if (statm == nullptr) {
        return used_vmem;
    }
    size_t vm = 0, rss = 0, share = 0, text = 0, lib = 0, data = 0, dt = 0;
    fscanf(statm, "%lu%lu%lu%lu%lu%lu%lu", &vm, &rss, &share, &text, &lib, &data, &dt);
    fclose(statm);
    int page  = sysconf(_SC_PAGESIZE);
    used_vmem = vm * page;
    return used_vmem;
#else
#error "not support"
#endif
}
static size_t max_vmem_ = static_cast<size_t>(-1);
size_t Memory::get_max_vmem() {
    //    struct rlimit limit;
    //    getrlimit(RLIMIT_AS, &limit);
    //    return limit.rlim_cur;
    return max_vmem_;
}
void Memory::set_max_vmem(size_t size) {
    //    struct rlimit limit;
    //    limit.rlim_cur = size;
    //    setrlimit(RLIMIT_AS, &limit);
    max_vmem_ = size;
}

size_t Memory::get_max_stack_size() {
    static size_t s = [] {
        struct rlimit limit;
        getrlimit(RLIMIT_STACK, &limit);
        return limit.rlim_cur;
    }();
    return s;
}

size_t Memory::get_max_managed_vmem() {
#ifdef USE_JEMALLOC
    return JEMALLOC_WRAPPER(max_alloc_size);
#else
//    SLOG(ERROR) << "not support";
    return get_max_vmem();
#endif
}
void Memory::set_max_managed_vmem(size_t size) {
#ifdef USE_JEMALLOC
    JEMALLOC_WRAPPER(max_alloc_size) = size;
#else
    set_max_vmem(size);
    //SLOG(ERROR) << "not support";
#endif
}
size_t Memory::get_managed_vmem() {
#ifdef USE_JEMALLOC
    return JEMALLOC_WRAPPER(allocated_size);
#else
    return get_used_vmem();
#endif
}

static size_t max_pmem_ = static_cast<size_t>(-1);
void Memory::set_max_pmem(size_t max_pmem) {
    max_pmem_         = max_pmem;
    size_t cached_rss = get_used_pmem();
    size_t rest_rss   = cached_rss < max_pmem ? (max_pmem - cached_rss) : 0;
    if (rest_rss < 10 * 1024 * 1024) {
        pico_gc();
        cached_rss = get_used_pmem();
        rest_rss   = cached_rss < max_pmem ? (max_pmem - cached_rss) : 0;
    }
    size_t rest_vmem = static_cast<size_t>(rest_rss * FLAGS_vmem_ratio);
    if (get_max_vmem() != static_cast<size_t>(-1)) {
        size_t rest_vmem2 =
            get_max_vmem() > get_used_vmem() ? get_max_vmem() - get_used_vmem() : 0;
        rest_vmem = std::min(rest_vmem, rest_vmem2);
    }
    set_max_managed_vmem(rest_vmem + get_managed_vmem());
}
size_t Memory::get_max_pmem() {
    return max_pmem_;
}

static Monitor::monitor_id pmem_monitor_id = 0;

void Memory::initialize() {
    if (FLAGS_max_vmem_mb != static_cast<size_t>(-1)) {
        size_t max_vmem = FLAGS_max_vmem_mb * 1024 * 1024;
        set_max_vmem(max_vmem);
    }
    if (FLAGS_pmem_monitor_interval_ms > 0 && FLAGS_max_pmem_mb > 0 &&
        FLAGS_max_pmem_mb != static_cast<size_t>(-1))
        pmem_monitor_id = pico_monitor().submit(
            "memory_monitor", 0, FLAGS_pmem_monitor_interval_ms, [=] {
                size_t max_pmem = FLAGS_max_pmem_mb * 1024 * 1024;
                set_max_pmem(max_pmem);
                if (FLAGS_log_memstats) {
                    memstats(SLOG(INFO));
                }
            });
}

void Memory::finalize() {
    pico_monitor().destroy(pmem_monitor_id);
}


std::string pretty_bytes(size_t size) {
    if (size == static_cast<size_t>(-1))
        return "inf";
    const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    uint s              = 0; // which suffix to use
    double count        = size;
    while (count >= 1024 && s < 8) {
        s++;
        count /= 1024;
    }
    char buf[32];
    if (count - floor(count) == 0.0)
        sprintf(buf, "%d%s", (int)count, units[s]);
    else
        sprintf(buf, "%.1f%s", count, units[s]);

    return std::string(buf);
}
void Memory::memstats(std::ostream& out) {
    out << "vmem: " << pretty_bytes(get_used_vmem()) << "/"
        << pretty_bytes(get_max_vmem()) << ", pmem: " << pretty_bytes(get_used_pmem())
        << "/" << pretty_bytes(get_max_pmem())
        << ", managed_vmem: " << pretty_bytes(get_managed_vmem()) << "/"
        << pretty_bytes(get_max_managed_vmem());
}
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

void* handle_OOM(std::size_t size, bool nothrow) {
    void* ptr = nullptr;

    while (ptr == nullptr) {
        std::new_handler handler;
        // GCC-4.8 and clang 4.0 do not have std::get_new_handler.
        {
            static std::mutex mtx;
            std::lock_guard<std::mutex> lock(mtx);

            handler = std::set_new_handler(nullptr);
            std::set_new_handler(handler);
        }
        if (handler == nullptr)
            break;

        try {
            handler();
        } catch (const std::bad_alloc&) {
            break;
        }

        ptr = pico_malloc(size);
    }
    //    SCHECK(ptr!= nullptr);

    if (ptr == nullptr && !nothrow)
        std::__throw_bad_alloc();
    return ptr;
}
} // namespace core
} // namespace pico
} // namespace paradigm4

using paradigm4::pico::core::pico_mem;
void* pico_malloc(size_t size) {
    void* res = SYS_malloc(size);
    if (unlikely(res == nullptr)) {
        pico_gc();
        res = SYS_malloc(size);
    }
    return res;
}
void* pico_valloc(size_t size) {
    void* res = SYS_valloc(size);
    if (unlikely(res == nullptr)) {
        pico_gc();
        res = SYS_valloc(size);
    }
    return res;
}

void* pico_calloc(size_t n, size_t size) {
    void* res = SYS_calloc(n, size);
    if (unlikely(res == nullptr)) {
        pico_gc();
        res = SYS_calloc(n, size);
    }
    return res;
}
void* pico_realloc(void* ptr, size_t size) {
    void* res = SYS_realloc(ptr, size);
    if (unlikely(res == nullptr)) {
        pico_gc();
        res = SYS_realloc(ptr, size);
    }
    return res;
}
void* pico_memalign(size_t align, size_t size) {
    void* res = SYS_memalign(align, size);
    if (unlikely(res == nullptr)) {
        pico_gc();
        res = SYS_memalign(align, size);
    }
    return res;
}
int pico_posix_memalign(void** res, size_t align, size_t size) {
    *res = pico_memalign(align, size);
    return 0;
}

void pico_free(void* ptr) {
    return SYS_free(ptr);
}

void pico_cfree(void* ptr) {
    return ::free(ptr);
}

size_t pico_malloc_size(void* ptr) {
    return SYS_malloc_usable_size(ptr);
}
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
void pico_gc() {
    pico_gc_pmem();
    pico_gc_vmem();
}
void pico_gc_pmem() {
#ifdef USE_JEMALLOC

    SCHECK(je_mallctl("thread.tcache.flush", NULL, NULL, NULL, 0) == 0);
    SCHECK(je_mallctl(
        "arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".decay", NULL, NULL, NULL, 0) == 0);
    SCHECK(je_mallctl(
        "arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".purge", NULL, NULL, NULL, 0) == 0);
#endif
}
void pico_gc_vmem() {
#ifdef USE_JEMALLOC
//    SCHECK(je_mallctl("arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".destroy_retained", NULL,
//              NULL, NULL, 0) == 0);
#endif
}

void pico_memstats(std::ostream& out) {
    pico_mem().memstats(out);
}
#if OVERRIDE_SYSTEM_NEW
void* operator new(std::size_t size) {
    return paradigm4::pico::core::newImpl<false>(size);
}

void* operator new[](std::size_t size) {
    return paradigm4::pico::core::newImpl<false>(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return paradigm4::pico::core::newImpl<true>(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return paradigm4::pico::core::newImpl<true>(size);
}

void operator delete(void* ptr) noexcept {
    pico_free(ptr);
}

void operator delete[](void* ptr) noexcept {
    pico_free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&)noexcept {
    pico_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    pico_free(ptr);
}
#if __cpp_sized_deallocation >= 201309
/* C++14's sized-delete operators. */
void operator delete(void* ptr, std::size_t /*size*/) noexcept {
    if (unlikely(ptr == nullptr)) {
        return;
    }
    pico_free(ptr);
}

void operator delete[](void* ptr, std::size_t /*size*/) noexcept {
    if (unlikely(ptr == nullptr)) {
        return;
    }
    pico_free(ptr);
}
#endif
#endif

extern "C" {
#if OVERRIDE_SYSTEM_MALLOC
#pragma message(STRINGIFY(__GLIBC__))
#pragma message(STRINGIFY(__GNUC__))

#if defined(__GLIBC__)
//#define ALIAS(pico_fn) __attribute__((alias(#pico_fn), used))
//
//  void* __libc_malloc(size_t size)                ALIAS(pico_malloc);
//  void __libc_free(void* ptr)                     ALIAS(pico_free);
//  void* __libc_realloc(void* ptr, size_t size)    ALIAS(pico_realloc);
//  void* __libc_calloc(size_t n, size_t size)      ALIAS(pico_calloc);
//  void __libc_cfree(void* ptr)                    ALIAS(pico_free);
//  void* __libc_memalign(size_t align, size_t s)   ALIAS(pico_memalign);
//  void* __libc_valloc(size_t size)                ALIAS(pico_valloc);
////  void* __libc_pvalloc(size_t size)             ALIAS(pico_pvalloc);
//  int __posix_memalign(void** r, size_t a, size_t s)  ALIAS(pico_posix_memalign);
//#undef ALIAS
//
//#elif HAVE_MALLOC_H
/* Prototypes for __malloc_hook, __free_hook */
/* Prototypes for our hooks.  */
static void my_init_hook(void);
static void* my_malloc_hook(size_t, const void*);
static void* my_realloc_hook(void*, size_t, const void*);
static void* my_memalign_hook(size_t, size_t, const void*);

static void my_free_hook(void*, const void*);

/* Override initializing hook from the C library. */
void (*volatile __malloc_initialize_hook)(void) = my_init_hook;

static void my_init_hook(void) {
    __malloc_hook   = my_malloc_hook;
    __free_hook     = my_free_hook;
    __realloc_hook  = my_realloc_hook;
    __memalign_hook = my_memalign_hook;
}

static void* my_malloc_hook(size_t size, const void*) {
    return pico_malloc(size);
}
static void* my_realloc_hook(void* ptr, size_t size, const void*) {
    return pico_realloc(ptr, size);
}
static void my_free_hook(void* ptr, const void*) {
    pico_free(ptr);
}
static void* my_memalign_hook(size_t alignment, size_t size, const void*) {
    return pico_memalign(alignment, size);
}
#else
void* malloc(size_t size) ALIAS(pico_malloc);
void* calloc(size_t n, size_t size) ALIAS(pico_calloc);
void* realloc(void* ptr, size_t size) ALIAS(pico_realloc);
void free(void* ptr) ALIAS(pico_free);
int posix_memalign(void** ptr, size_t align, size_t size) ALIAS(pico_posix_memalign);

// void* malloc(size_t size) { return pico_malloc(size); }
// void* calloc(size_t n, size_t size) { return pico_calloc(n, size); }
// void* realloc(void* ptr, size_t size) { return pico_realloc(ptr, size); }
// void free(void* ptr) { return pico_free(ptr); }
// int posix_memalign(void** ptr, size_t align, size_t size) {
//    *ptr = pico_memalign(align, size);
//    return 0;
//}
#endif
#endif
}
