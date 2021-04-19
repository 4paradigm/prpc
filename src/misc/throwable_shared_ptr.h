#ifndef PARADIGM4_PICO_CORE_THROWABLE_SHARED_PTR_H
#define PARADIGM4_PICO_CORE_THROWABLE_SHARED_PTR_H

#include <algorithm>
#include <cstddef>
#include <functional>

// can be replaced by other error mechanism
#include <cassert>
#define SHARED_ASSERT(x) assert(x)

namespace paradigm4 {
namespace pico {
namespace core {

class shared_ptr_count {
public:
    shared_ptr_count() : pn(NULL) {}
    shared_ptr_count(const shared_ptr_count& count) : pn(count.pn) {}

    shared_ptr_count& operator=(const shared_ptr_count& count) = default;

    void swap(shared_ptr_count& lhs) {
        std::swap(pn, lhs.pn);
    }

    long use_count() const {
        long count = 0;
        if (NULL != pn) {
            count = *pn;
        }
        return count;
    }

    template <class U>
    void acquire(U* p, std::function<void(U*)> deleter) noexcept(false) {
        if (NULL != p) {
            if (NULL == pn) {
                try {
                    pn = new long(1); // may throw std::bad_alloc
                } catch (std::bad_alloc&) {
                    deleter(p);
                    throw; // rethrow the std::bad_alloc
                }
            } else {
                ++(*pn);
            }
        }
    }

    template <class U>
    void release(U* p, std::function<void(U*)> deleter) noexcept(false) {
        if (NULL != pn) {
            --(*pn);
            if (0 == *pn) {
                deleter(p);
                delete pn;
            }
            pn = NULL;
        }
    }

public:
    long* pn;
};

/**
 * @brief minimal implementation of smart pointer, a subset of the C++11 std::shared_ptr or
 * boost::shared_ptr. Specially, it allows throw in deconstruction, which is forbidden by
 * std::shared_ptr.
 */
template <class T>
class shared_ptr {
public:
    typedef T element_type;

    shared_ptr() : px(NULL), pn() {}

    shared_ptr(T* p, std::function<void(T* p)> deleter = shared_ptr<T>::default_deleter)
        : pn(), _deleter(deleter) {
        acquire(p);
    }

    shared_ptr(const shared_ptr& ptr) : pn(ptr.pn), _deleter(ptr._deleter) {
        SHARED_ASSERT((NULL == ptr.px) || (0 != ptr.pn.use_count()));

        acquire(ptr.px);
    }

    shared_ptr(shared_ptr&& ptr) {
        swap(ptr);
    }

    shared_ptr& operator=(const shared_ptr& ptr) noexcept(false) {
        pn = ptr.pn;
        _deleter = ptr._deleter;
        SHARED_ASSERT((NULL == ptr.px) || (0 != ptr.pn.use_count()));
        acquire(ptr.px);
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& ptr) noexcept(false) {
        swap(ptr);
        return *this;
    }

    shared_ptr& operator=(T* ptr) noexcept(false) {
        _deleter = shared_ptr<T>::default_deleter;
        acquire(ptr);
    }

    inline ~shared_ptr() noexcept(false) {
        release();
    }

    inline void reset() noexcept(false) {
        release();
    }

    void reset(T* p, std::function<void(T*)> deleter = shared_ptr<T>::default_deleter) {
        SHARED_ASSERT((NULL == p) || (px != p));
        release();
        _deleter = deleter;
        acquire(p);
    }

    void swap(shared_ptr& lhs) noexcept(false) {
        std::swap(px, lhs.px);
        pn.swap(lhs.pn);
        _deleter.swap(lhs._deleter);
    }

    inline operator bool() noexcept(false) {
        return (0 < pn.use_count());
    }

    inline bool unique() noexcept(false) {
        return (1 == pn.use_count());
    }

    long use_count() noexcept(false) {
        return pn.use_count();
    }

    inline T& operator*() noexcept(false) {
        SHARED_ASSERT(NULL != px);
        return *px;
    }

    inline T* operator->() const noexcept(false) {
        SHARED_ASSERT(NULL != px);
        return px;
    }

    inline T* get(void) const noexcept(false) {
        return px;
    }

    inline operator T*() {
        return px;
    }

private:
    static void default_deleter(T* p) {
        delete p;
    }
    /// @brief acquire/share the ownership of the px pointer, initializing the reference
    /// counter
    inline void acquire(T* p) {
        pn.acquire(p, _deleter);
        px = p;
    }

    /// @brief release the ownership of the px pointer, destroying the object when appropriate
    inline void release(void) noexcept(false) {
        pn.release(px, _deleter);
        px = NULL;
    }

private:
    // This allow pointer_cast functions to share the reference counter between different
    // shared_ptr types
    template <class U>
    friend class shared_ptr;

private:
    T* px = nullptr;
    shared_ptr_count pn;
    std::function<void(T*)> _deleter;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_THROWABLE_SHARED_PTR_H
