#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <utility>
#include <memory_resource>

template <typename T>
concept HasArenaSafeTag = requires {
    { T::is_arena_safe } -> std::convertible_to<bool>;
} && T::is_arena_safe;

template <typename T>
concept ArenaSafe = std::is_trivially_destructible_v<T> || HasArenaSafeTag<T>;

class Arena : public std::pmr::memory_resource {
    struct Chunk {
        std::unique_ptr<std::byte[]> data;
        size_t capacity;
        size_t offset;
        std::unique_ptr<Chunk> next;

        explicit Chunk(const size_t size) : capacity(size), offset(0) {
            data = std::make_unique<std::byte[]>(size);
        }
    };

    std::unique_ptr<Chunk> head;
    size_t chunk_size;

    static size_t align_forward(const size_t ptr, const size_t alignment) {
        return ptr + alignment - 1 & ~(alignment - 1);
    }

protected:
    void * do_allocate(std::size_t bytes, std::size_t alignment) override;

    void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;

    bool do_is_equal(const memory_resource &other) const noexcept override;

public:
    explicit Arena(const size_t default_chunk_size = 1024 * 1024)
        : chunk_size(default_chunk_size) {
        head = std::make_unique<Chunk>(chunk_size);
    }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* raw_allocate(const size_t size, const size_t alignment) {
        size_t current_ptr = reinterpret_cast<size_t>(head->data.get()) + head->offset;
        size_t aligned_ptr = align_forward(current_ptr, alignment);
        size_t padding = aligned_ptr - current_ptr;

        if (head->offset + padding + size > head->capacity) {
            size_t new_size = std::max(chunk_size, size);
            auto new_chunk = std::make_unique<Chunk>(new_size);

            new_chunk->next = std::move(head);
            head = std::move(new_chunk);

            current_ptr = reinterpret_cast<size_t>(head->data.get());
            aligned_ptr = align_forward(current_ptr, alignment);
            padding = aligned_ptr - current_ptr;
        }

        head->offset += padding + size;
        return reinterpret_cast<void*>(aligned_ptr);
    }

    template <ArenaSafe T, typename... Args>
    T* make(Args&&... args) {
        void* ptr = do_allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }

    ~Arena() override;
};