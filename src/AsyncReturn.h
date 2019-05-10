#ifndef PARADIGM4_PICO_COMMON_ASYNC_RETURN_H
#define PARADIGM4_PICO_COMMON_ASYNC_RETURN_H

#include <condition_variable>
#include <memory>
#include <mutex>

namespace paradigm4 {
namespace pico {
namespace core {

template <class T>
class AsyncReturnObject {
public:
    void reset() {
        std::lock_guard<std::mutex> mlock(_mutex);
        _ret.reset();
        _returned = false;
    }

    std::shared_ptr<T> wait() {
        std::unique_lock<std::mutex> mlock(_mutex);
        _cond.wait(mlock, [this] { return _returned; });
        return _ret;
    }

    void notify(std::shared_ptr<T> ret) {
        std::lock_guard<std::mutex> mlock(_mutex);
        _returned = true;
        _ret = ret;
        _cond.notify_one();
    }

private:
    bool _returned = false;
    std::shared_ptr<T> _ret;
    std::mutex _mutex;
    std::condition_variable _cond;
};

/*!
 * \brief class AsyncReturn used to mark a function has returned or not.
 *  all inner function need get lock first to access vairable _returned.
 *  this class is used when a function call another function and get its
 *  return state later.
 */
template <class T>
class AsyncReturnV {
public:
    AsyncReturnV() {
        _ret.reset(new AsyncReturnObject<T>());
    }

    virtual ~AsyncReturnV() = default;

    void reset() {
        _ret->reset();
    }

    std::shared_ptr<T> wait() {
        return _ret->wait();
    }

    void notify(std::shared_ptr<T> ret = std::shared_ptr<T>()) {
        _ret->notify(ret);
    }

private:
    std::shared_ptr<AsyncReturnObject<T>> _ret;
};

typedef AsyncReturnV<void> AsyncReturn;

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_ASYNC_RETURN_H
