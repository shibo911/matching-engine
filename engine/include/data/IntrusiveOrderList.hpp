#pragma once
#include "data/Order.hpp"

namespace matching_engine {
namespace data {

// A zero-allocation doubly linked list. Because the pointers are embedded directly 
// inside the Order struct, removing an order from the middle of the queue is 
// an O(1) operation and causes no cache-misses from secondary node lookups.
class IntrusiveOrderList {
public:
    IntrusiveOrderList() = default;

    IntrusiveOrderList(const IntrusiveOrderList&) = delete;
    IntrusiveOrderList& operator=(const IntrusiveOrderList&) = delete;

    // Allow move semantics only for initial vector resizing (e.g., LOB array setup).
    // Moving a populated list is dangerous as it breaks external intrusive pointers,
    // but since we only resize the LOB array ONCE on startup while empty, this is safe.
    IntrusiveOrderList(IntrusiveOrderList&& other) noexcept : head_(other.head_), tail_(other.tail_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
    }
    IntrusiveOrderList& operator=(IntrusiveOrderList&& other) noexcept {
        if (this != &other) {
            head_ = other.head_;
            tail_ = other.tail_;
            other.head_ = nullptr;
            other.tail_ = nullptr;
        }
        return *this;
    }

    // Inserts at the tail. Required for Price-Time priority in the matching engine.
    inline void push_back(Order* order) {
        order->next = nullptr;
        order->prev = tail_;

        if (tail_) {
            tail_->next = order;
        } else {
            head_ = order;
        }
        tail_ = order;
    }

    inline void push_front(Order* order) {
        order->prev = nullptr;
        order->next = head_;

        if (head_) {
            head_->prev = order;
        } else {
            tail_ = order;
        }
        head_ = order;
    }

    // O(1) removal. Bypasses list traversal entirely since the node knows its neighbors.
    inline void remove(Order* order) {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head_ = order->next;
        }

        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail_ = order->prev;
        }

        order->next = nullptr;
        order->prev = nullptr;
    }

    // Pops the head of the list. Heavily utilized during the crossing phase.
    inline Order* pop_front() {
        if (!head_) return nullptr;
        Order* order = head_;
        remove(order);
        return order;
    }

    inline Order* head() const { return head_; }
    inline Order* tail() const { return tail_; }
    inline bool empty() const { return head_ == nullptr; }

private:
    Order* head_ = nullptr;
    Order* tail_ = nullptr;
};

} // namespace data
} // namespace matching_engine
