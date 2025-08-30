// test_orderbook.cpp
#include "orderbook.h"
#include "types/event.h"
#include <iostream>
#include <vector>
#include <cstdint>

// --- snapshot printer (top-N) ---
static void print_topN(const Orderbook& ob, size_t N, uint64_t ns, int64_t book_id) {
    std::vector<std::pair<Price, Quantity>> bids, asks;
    ob.snapshot_n(N, bids, asks);

    std::cout << "\n---- SNAPSHOT ns=" << ns
              << " book=" << book_id
              << " open=" << (ob.trading_open() ? "Y" : "N") << " ----\n";

    std::cout << "BIDS (price, qty):\n";
    for (size_t i = 0; i < bids.size(); ++i)
        std::cout << "  [" << i << "] " << bids[i].first << ", " << bids[i].second << "\n";
    if (bids.empty()) std::cout << "  (none)\n";

    std::cout << "ASKS (price, qty):\n";
    for (size_t i = 0; i < asks.size(); ++i)
        std::cout << "  [" << i << "] " << asks[i].first << ", " << asks[i].second << "\n";
    if (asks.empty()) std::cout << "  (none)\n";

    if (ob.has_top()) {
        std::cout << "BEST: bid " << ob.best_bid_price() << " x " << ob.best_bid_quantity()
                  << " | ask " << ob.best_ask_price() << " x " << ob.best_ask_quantity()
                  << "\n";
    }
    std::cout << "------------------------------\n";
}

// --- helpers to create events ---
static Event make_state(OrderbookId book, const char* state, uint64_t ns) {
    Event e{};
    e.type = MessageType::OrderbookState;
    e.orderbook_id = book;
    e.orderbook_state = state;
    e.nanosec = ns;
    return e;
}
static Event make_add(OrderbookId book, OrderId id, Side s, Price px, Quantity qty,
                      RankingTime rt, RankingSeqNum rsn, uint64_t ns) {
    Event e{};
    e.type = MessageType::AddOrder;
    e.orderbook_id = book;
    e.order_id = id;
    e.side = s;
    e.price = px;
    e.quantity = qty;
    e.ranking_time = rt;
    e.ranking_seq_num = rsn;
    e.nanosec = ns;
    return e;
}
static Event make_exec(OrderbookId book, OrderId id, Side s, Quantity qty, uint64_t ns) {
    Event e{};
    e.type = MessageType::ExecuteOrder;
    e.orderbook_id = book;
    e.order_id = id;
    e.side = s;
    e.quantity = qty;
    e.nanosec = ns;
    return e;
}
static Event make_del(OrderbookId book, OrderId id, Side s, uint64_t ns) {
    Event e{};
    e.type = MessageType::DeleteOrder;
    e.orderbook_id = book;
    e.order_id = id;
    e.side = s;
    e.nanosec = ns;
    return e;
}

int main() {
    const OrderbookId BOOK = 123;
    Orderbook ob;

    uint64_t ns = 1;
    size_t applied = 0;

    auto apply_and_maybe_snapshot = [&](const Event& ev) {
        ob.apply(ev);
        ++applied;
        if ((applied % 10) == 0) {
            print_topN(ob, 10, ns, BOOK);
        }
        ++ns;
    };

    // 1) put the book into continuous trading
    apply_and_maybe_snapshot(make_state(BOOK, "P_SUREKLI_ISLEM", ns));

    // 2) seed with bids 10, 20, 30, … and asks 20, 30, 40, …
    Price mid = 10;        // start price
    Quantity lot = 1000;

    // 10 bid levels: 10, 20, 30, …
    for (int i = 0; i < 10; ++i) {
        Price px = mid + i * 10;
        apply_and_maybe_snapshot(make_add(BOOK, 1000 + i, Side::Buy, px, lot*(i+1),
                                          i+1, i+1, ns));
    }

    // 10 ask levels: 20, 30, 40, …
    for (int i = 0; i < 10; ++i) {
        Price px = mid + (i+1) * 10;
        apply_and_maybe_snapshot(make_add(BOOK, 2000 + i, Side::Sell, px, lot*(i+1),
                                          i+1, i+1, ns));
    }

    // 3) execute half of best ask (id=2000)
    apply_and_maybe_snapshot(make_exec(BOOK, 2000, Side::Sell, lot/2, ns));
    // 4) execute rest of best ask
    apply_and_maybe_snapshot(make_exec(BOOK, 2000, Side::Sell, lot - lot/2, ns));

    // 5) delete a bid level (id=1003)
    apply_and_maybe_snapshot(make_del(BOOK, 1003, Side::Buy, ns));

    // 6) partial + full exec on best bid
    apply_and_maybe_snapshot(make_exec(BOOK, 1000, Side::Buy, lot/3, ns));
    apply_and_maybe_snapshot(make_exec(BOOK, 1000, Side::Buy, lot - lot/3, ns));

    // 7) add fresh liquidity at top ask/bid
    apply_and_maybe_snapshot(make_add(BOOK, 3001, Side::Sell, ob.best_ask_price(), 2500, 99, 1, ns));
    apply_and_maybe_snapshot(make_add(BOOK, 3002, Side::Buy, ob.best_bid_price(), 2500, 99, 2, ns));

    // final snapshot if needed
    if ((applied % 10) != 0) {
        print_topN(ob, 10, ns, BOOK);
    }

    std::cout << "\n[TEST_ORDERBOOK DONE] total_events=" << applied << "\n";
    return 0;
}
