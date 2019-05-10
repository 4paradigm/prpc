//
// Created by sun on 2018/3/6.
//

#ifndef PICO_MONITOR_H
#define PICO_MONITOR_H

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

class Monitor {
public:
    typedef uint64_t monitor_id;
    typedef std::function<void()> handler_type;
    typedef std::chrono::steady_clock Clock;
    typedef std::chrono::time_point<Clock> Timestamp;
    typedef std::chrono::milliseconds Duration;
    enum Status { NOT_FOUND, RUNNING, PENDING, DONE };
    class Waiter {
        std::function<bool(void)> wait_func;

    public:
        Waiter() {}
        template <typename F>
        Waiter(F&& f) : wait_func(std::forward<F>(f)) {}
        bool wait() {
            if (wait_func == nullptr)
                return true;
            bool ret  = wait_func();
            wait_func = nullptr;
            return ret;
        }
    };

    struct Event {
        Event(monitor_id id = 0) : id(id), running(false) {}

        template <typename FUNC>
        Event(monitor_id id, const std::string& name, Timestamp next, Duration period,
            FUNC&& handler) noexcept
            : id(id),
              next(next),
              period(period),
              handler(std::forward<FUNC>(handler)),
              running(false),
              name(name) {}

        Event(Event const& r) = delete;

        Event(Event&& r) noexcept
            : id(r.id),
              next(r.next),
              period(r.period),
              handler(std::move(r.handler)),
              running(r.running),
              name(std::move(r.name)) {}

        Event& operator=(Event const& r) = delete;

        Event& operator=(Event&& r) {
            if (this != &r) {
                id      = r.id;
                next    = r.next;
                period  = r.period;
                handler = std::move(r.handler);
                running = r.running;
                name    = std::move(r.name);
            }
            return *this;
        }

        monitor_id id;
        Timestamp next;
        Duration period;
        handler_type handler;
        bool running;
        bool canceled = false;
        std::string name;
    };

public:
    static Monitor& singleton() {
        static Monitor monitor;
        return monitor;
    }

    ~Monitor() {
        ScopedLock lock(_mutex);
        for (auto& event : _active_events) {
            if (event.second.period.count() > 0) {
                SLOG(FATAL) << "periodic task name=" << event.second.name
                            << " is still active, please destroy it first";
            }
        }
        _done = true;
        _wake_up.notify_all();
        lock.unlock();
        _worker.join();
    }

    template <typename FUNC>
    monitor_id submit(
        const std::string& name, uint64_t msFromNow, uint64_t msPeriod, FUNC&& handler) {
        return submitImpl(Event(0, name, Clock::now() + Duration(msFromNow),
            Duration(msPeriod), std::forward<FUNC>(handler)));
    }

    Status query(monitor_id id) {
        ScopedLock lock(_mutex);
        auto i = _active_events.find(id);
        if (i == _active_events.end()) {
            if (id < _next_id)
                return DONE;
            return NOT_FOUND;
        }
        if (i->second.running) {
            return RUNNING;
        }
        return PENDING;
    }

    Waiter destroy(monitor_id id) {
        ScopedLock lock(_mutex);
        auto i = _active_events.find(id);
        if (i == _active_events.end())
            return [] { return false; };
        if (i->second.running) {
            // A callback is in progress for this Event,
            // so flag it for deletion in the _worker
            i->second.canceled = true;
        } else {
            _event_queue.erase(std::ref(i->second));
            _active_events.erase(i);
        }

        _wake_up.notify_all();
        return [id, this] {
            wait_complete(id);
            return true;
        };
    }

    Waiter destroy_with_additional_run(monitor_id id, size_t delay = 0) {
        ScopedLock lock(_mutex);
        auto i = _active_events.find(id);
        if (i == _active_events.end())
            return [] { return false; };

        if (i->second.running) {
            // A callback is in progress for this Event,
            // so flag it for deletion in the _worker
            i->second.canceled = true;
            _wake_up.wait(lock, [this, id] {
                return _active_events.find(id) == _active_events.end();

            });
        } else {
            _event_queue.erase(std::ref(i->second));
            _active_events.erase(i);
        }
        lock.unlock();
        auto new_id = submit(i->second.name, delay, 0, i->second.handler);
        return [new_id, this] {
            wait_complete(new_id);
            return true;
        };
    }
    void wait_complete(monitor_id id) {
        ScopedLock lock(_mutex);
        _wake_up.wait(lock, [this, id] {
            return _active_events.find(id) == _active_events.end();

        });
    }

private:
    Monitor() : _next_id(1), _event_queue(), _done(false) {
        ScopedLock lock(_mutex);
        _worker = std::thread(std::bind(&Monitor::thread_run, this));
    }
    Monitor::monitor_id submitImpl(Event&& item) {
        ScopedLock lock(_mutex);
        item.id   = _next_id++;
        auto iter = _active_events.emplace(item.id, std::move(item));
        _event_queue.insert(iter.first->second);
        _wake_up.notify_all();
        return item.id;
    }

    void thread_run() {
        ScopedLock lock(_mutex);

        while (!_done || !_event_queue.empty()) {
            if (_event_queue.empty()) {
                // Wait (forever) for work
                _wake_up.wait(lock);
            } else {
                auto firstEvent = _event_queue.begin();
                Event& instance = *firstEvent;
                auto now        = Clock::now();
                if (now >= instance.next) {
                    _event_queue.erase(firstEvent);

                    // Mark it as running to handle racing destroy
                    instance.running = true;

                    // Call the handler
                    lock.unlock();
                    instance.handler();
                    lock.lock();
                    instance.running = false;

                    if (instance.canceled) {
                        // Running was set to false, destroy was called
                        // for this Event while the callback was in progress
                        // (this thread was not holding the lock during the callback)
                        _active_events.erase(instance.id);
                        _wake_up.notify_all();
                    } else {
                        // If it is periodic, schedule a new one
                        if (instance.period.count() > 0) {
                            instance.next = instance.next + instance.period;
                            _event_queue.insert(instance);
                        } else {
                            _active_events.erase(instance.id);
                            _wake_up.notify_all();
                        }
                    }
                } else {
                    // Wait until the monitor is ready or a monitor creation notifies
                    _wake_up.wait_until(lock, instance.next);
                }
            }
        }
    }

private:
    // Comparison functor to sort the monitor "_event_queue" by Event::next
    struct NextActiveComparator {
        bool operator()(const Event& a, const Event& b) const {
            return a.next < b.next;
        }
    };
    typedef std::unique_lock<std::mutex> ScopedLock;
    typedef std::unordered_map<monitor_id, Event> EventMap;
    // EventQueue is a set of references to Event objects, sorted by next
    typedef std::reference_wrapper<Event> QueueValue;
    typedef std::multiset<QueueValue, NextActiveComparator> EventQueue;
    std::mutex _mutex;
    std::condition_variable _wake_up;
    monitor_id _next_id;
    EventMap _active_events;
    EventQueue _event_queue;
    std::thread _worker;
    bool _done;
};

inline Monitor& pico_monitor() {
    return Monitor::singleton();
}

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PICO_MONITOR_H
