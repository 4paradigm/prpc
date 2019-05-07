#ifndef PARADIGM4_PICO_COMMON_SPSCQUEUE_H
#define PARADIGM4_PICO_COMMON_SPSCQUEUE_H

#include <atomic>
#include <iostream>
#include <memory>

namespace paradigm4 {
namespace pico {

template <typename T>
class SpscQueue {
public:
    SpscQueue() {
        Node* n = new Node();
        n->next = 0;
        _tail = _head = _first = _tailcopy = n;
    }

    ~SpscQueue() {
        Node* n = _first;
        do {
            Node* next = n->next;
            delete n;
            n = next;
        } while (n);
    }

    void push(T&& v) {
        Node* n = alloc_node(std::move(v));
        Node* h = _head.load(std::memory_order_relaxed);
        h->next.store(n, std::memory_order_release);
        _head.store(n, std::memory_order_relaxed);
    }

    bool pop(T& v) {
        Node* t = _tail.load(std::memory_order_relaxed);
        Node* next = t->next.load(std::memory_order_acquire);
        if (next) {
            v = std::move(next->v);
            _tail.store(next, std::memory_order_release);
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
        Node(T&& v) : next(nullptr), v(std::move(v)) {}
        Node() : next(nullptr) {}
        std::atomic<Node*> next;
        T v;
    };

    // consumer part
    std::atomic<Node*> _tail;

    char _cache_line_pad[64];

    // producer part
    std::atomic<Node*> _head;
    std::atomic<Node*> _first;
    Node* _tailcopy;

    Node* alloc_node(T&& v) {
        Node* n = _first.load(std::memory_order_relaxed);
        Node* next = n->next.load(std::memory_order_relaxed);
        if (n != _tailcopy) {
            _first.store(next, std::memory_order_relaxed);
            n->next.store(nullptr, std::memory_order_relaxed);
            n->v = std::move(v);
            return n;
        }
        _tailcopy = _tail.load(std::memory_order_acquire);
        if (n != _tailcopy) {
            _first.store(next, std::memory_order_relaxed);
            n->next.store(nullptr, std::memory_order_relaxed);
            n->v = std::move(v);
            return n;
        }
        return new Node(std::move(v));
    }
};

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_SPSCQUEUE_H
