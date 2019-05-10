#ifndef PARADIGM4_PICO_COMMON_MPSCQUEUE_H
#define PARADIGM4_PICO_COMMON_MPSCQUEUE_H

#include <atomic>
#include <utility>
#include "pico_memory.h"
namespace paradigm4 {
namespace pico {
namespace core {

template <typename T>
class MpscQueue {
public:
    MpscQueue() {
        Node* n = new_node();
        _tail = _head = n;
    }

    ~MpscQueue() {
        Node* n = _head;
        do {
            Node* next = n->next;
            delete_node(n);
            n = next;
        } while (n);
    }

    void push(T&& v) {
        Node* n = new_node(std::move(v));
        Node* prev_head = _head.exchange(n, std::memory_order_acq_rel);
        prev_head->next.store(n, std::memory_order_release);
    }

    bool pop(T& v) {
        Node* t = _tail.load(std::memory_order_relaxed);
        Node* next = t->next.load(std::memory_order_acquire);
        if (next) {
            v = std::move(next->v);
            _tail.store(next, std::memory_order_release);
            delete_node(t);
            return true;
        } else {
            return false;
        }
    }

    T* top() {
        Node* t = _tail.load(std::memory_order_relaxed);
        Node* next = t->next.load(std::memory_order_acquire);
        if (next) {
            return &next->v;
        } else {
            return nullptr;
        }
    }

private:
    struct alignas(8) Node {
        Node(T&& v) : v(std::move(v)), next(nullptr) {}
        Node() : next(nullptr) {}

        T v;
        std::atomic<Node*> next;
    };

    template <typename ... Args>
    inline Node* new_node(Args&&... args){
        return pico_new<Node>(std::forward<Args>(args)...);
    }

    inline void delete_node(Node* p){
        pico_delete(p);
    }

    // consumer part
    std::atomic<Node*> _tail;

    char _cache_line_pad[64];

    // producer part
    std::atomic<Node*> _head;
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_MPSCQUEUE_H
