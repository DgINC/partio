//
// Created by root on 4/21/26.
//

#include "Arena.hpp"

void * Arena::do_allocate(const std::size_t bytes, const std::size_t alignment) {
    return this->raw_allocate(bytes, alignment);
}

void Arena::do_deallocate(void *p, std::size_t bytes, std::size_t alignment) {
}

bool Arena::do_is_equal(const memory_resource &other) const noexcept {
    return this == &other;
}

Arena::~Arena() {
    while (head) {
        std::unique_ptr<Chunk> next_chunk = std::move(head->next);
        head.reset();
        head = std::move(next_chunk);
    }
}
