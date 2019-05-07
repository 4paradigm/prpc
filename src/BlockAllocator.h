#ifndef PARADIGM4_PICO_COMMON_INCLUDE_BLOCK_ALLOCATOR_H
#define PARADIGM4_PICO_COMMON_INCLUDE_BLOCK_ALLOCATOR_H

#include <cstdlib>
#include <type_traits>

#include "pico_log.h"

namespace paradigm4 {
namespace pico {

template<class T, size_t* N>
class BlockAllocator {
public:
    typedef T              value_type;
    typedef std::true_type propagate_on_container_move_assignment;
    typedef std::true_type is_always_equal;

public:
    template<class U>
    class rebind {
    public:
        using other = BlockAllocator<U, N>;
    };

public:
    BlockAllocator() = default;

    BlockAllocator(BlockAllocator<T, N>&& other) {
        *this = std::move(other);
    }

    ~BlockAllocator() {
        SCHECK(_elm_num == 0) << "memory leak, " << _elm_num << " elements leaked";
        Block* ptr = _block_head;
        while (ptr != nullptr) {
            Block* new_head = ptr->next;
            pico_free(ptr);
            ptr = new_head;
        }
    }

    BlockAllocator<T, N>& operator=(BlockAllocator<T, N>&& other) {
        _elm_num = other._elm_num;
        _free_head = other._free_head;
        _block_head = other._block_head;
        other._elm_num = 0u;
        other._free_head = nullptr;
        other._block_head = nullptr;
        return *this;
    }

    T* allocate(size_t n) {
        SCHECK(n == 1) << "only one entry allocation is allowed, but n = " << n;
        if (_free_head == nullptr) {
            allocate_new_block();
        }
        T* ret = static_cast<T*>((void*)_free_head);
        _free_head = _free_head->next;
        ++_elm_num;
        return ret;
    }

    void deallocate(T* p, size_t n) {
        SCHECK(_elm_num > 0u);
        SCHECK(n == 1) << "only one entry allocation is allowed, but n = " << n;
        Entry* ent = static_cast<Entry*>((void*)p);
        ent->next = _free_head;
        _free_head = ent;
        --_elm_num;
    }

private:
    union alignas(T) Entry {
        Entry* next;
        char data[sizeof(T)];
    };

    struct Block {
        Block* next;
        Entry entry[];
    };

private:
    void allocate_new_block() {
        size_t malloc_size = sizeof(Block) + sizeof(Entry) * (*N);
        Block* new_block = static_cast<Block*>(pico_malloc(malloc_size));
        new_block->next = _block_head;
        _block_head = new_block;
        _block_head->entry[0].next = _free_head;
        for (size_t i = 1; i < (*N); ++i) {
            _block_head->entry[i].next = &_block_head->entry[i-1];
        }
        _free_head = &_block_head->entry[(*N)-1];
    }

private:
    size_t _elm_num = 0u;
    Entry* _free_head = nullptr;
    Block* _block_head = nullptr;
};

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_INCLUDE_BLOCK_ALLOCATOR_H
