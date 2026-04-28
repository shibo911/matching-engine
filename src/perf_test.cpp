#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <list>

using namespace std;

enum class Side : uint8_t { Buy, Sell };

struct alignas(8) Order {
    Order* next = nullptr;
    Order* prev = nullptr;
    uint64_t id = 0;
    uint32_t price = 0;
    uint32_t quantity = 0;
    Side side = Side::Buy;
};

// --- Naive Engine ---
class NaiveEngine {
public:
    void add_order(const Order& order) {
        if (order.side == Side::Buy) bids_[order.price].push_back(order);
        else asks_[order.price].push_back(order);
        order_map_[order.id] = order; 
    }
    void cancel_order(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        if (it == order_map_.end()) return;
        Order order = it->second;
        order_map_.erase(it);
        if (order.side == Side::Buy) {
            auto& level = bids_[order.price];
            level.remove_if([=](const Order& o) { return o.id == order_id; });
            if (level.empty()) bids_.erase(order.price);
        } else {
            auto& level = asks_[order.price];
            level.remove_if([=](const Order& o) { return o.id == order_id; });
            if (level.empty()) asks_.erase(order.price);
        }
    }
private:
    map<uint32_t, list<Order>> bids_;
    map<uint32_t, list<Order>> asks_;
    map<uint64_t, Order> order_map_;
};

// --- Optimized Engine (Simplified for Linux GCC11 without pmr) ---
struct IntrusiveList {
    Order* head_ = nullptr;
    Order* tail_ = nullptr;
    void push_back(Order* o) {
        o->next = nullptr;
        o->prev = tail_;
        if (tail_) tail_->next = o;
        else head_ = o;
        tail_ = o;
    }
    void remove(Order* o) {
        if (o->prev) o->prev->next = o->next;
        else head_ = o->next;
        if (o->next) o->next->prev = o->prev;
        else tail_ = o->prev;
    }
};

struct PriceLevel {
    IntrusiveList orders;
    uint64_t volume = 0;
};

class OptimizedEngine {
public:
    OptimizedEngine(uint32_t min_p, uint32_t max_p) : min_p_(min_p), max_p_(max_p), levels(max_p - min_p + 1) {}
    
    void add_order(Order* o) {
        levels[o->price - min_p_].orders.push_back(o);
    }
    void cancel_order(Order* o) {
        levels[o->price - min_p_].orders.remove(o);
    }
private:
    uint32_t min_p_, max_p_;
    vector<PriceLevel> levels;
};

std::vector<Order> generate_orders(size_t count) {
    std::vector<Order> orders(count);
    for (size_t i = 0; i < count; ++i) {
        orders[i].id = i + 1;
        orders[i].price = 15000 + (i % 100);
        orders[i].quantity = 100;
        orders[i].side = (i % 2 == 0) ? Side::Buy : Side::Sell;
    }
    return orders;
}

int main(int argc, char** argv) {
    string mode = argv[1];
    size_t count = 50000;
    auto orders = generate_orders(count);

    if (mode == "optimized") {
        OptimizedEngine engine(10000, 30000);
        vector<Order*> cancel_map(count * 2, nullptr);
        vector<Order> pool = orders; 
        
        vector<Order*> ptrs(count);
        for(size_t i=0; i<count; ++i) {
            ptrs[i] = &pool[i];
            cancel_map[ptrs[i]->id] = ptrs[i];
            engine.add_order(ptrs[i]);
        }
        for(size_t i=0; i<count; ++i) {
            engine.cancel_order(cancel_map[ptrs[i]->id]);
        }
    } else {
        NaiveEngine engine;
        for(size_t i=0; i<count; ++i) engine.add_order(orders[i]);
        for(size_t i=0; i<count; ++i) engine.cancel_order(orders[i].id);
    }
    return 0;
}
