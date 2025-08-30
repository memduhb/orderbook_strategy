// test_strategy_sim.cpp
#include "orderbook.h"
#include "strategy.h"
#include "types/event.h"

#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iomanip>

// ----- pretty book snapshot (top-of-book only) -----
static void print_top(const Orderbook& ob) {
    if (!ob.has_top()) {
        std::cout << "BOOK: (no top)\n";
        return;
    }
    Price b = ob.best_bid_price();
    Quantity bq = ob.best_bid_quantity();
    Price a = ob.best_ask_price();
    Quantity aq = ob.best_ask_quantity();
    long spr = static_cast<long>(a) - static_cast<long>(b);
    std::cout << "BOOK: bid " << b << " x " << bq
              << " | ask " << a << " x " << aq
              << " | spr=" << spr << "\n";
}

// ----- detailed orderbook snapshot -----
static void print_topN(const Orderbook& ob, size_t N, uint64_t ns = 0) {
    std::vector<std::pair<Price, Quantity>> bids, asks;
    ob.snapshot_n(N, bids, asks);

    std::cout << "---- SNAPSHOT"
              << " ns=" << ns
              << " ----\n";

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
                  << " | spread=" << (ob.best_ask_price() - ob.best_bid_price()) << "\n";
    }
    std::cout << "------------------------------\n";
}

// ----- event factories -----
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
    e.quantity = qty; // no price in spec
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
    const OrderbookId BOOK = 123;      // toy book id
    const Quantity LOT = 1000;         // seed sizes
    const Price    TICK = 10;          // 1 tick in kuruş
    const Price    BID0 = 100;         // start best bid at 100
    const Price    ASK0 = 110;         // start best ask at 110 (tight: spread = tick)

    Orderbook ob;
    Strategy  strat(BOOK, /*order_qty=*/100, /*max_pos=*/500, /*min_pos=*/0);

    // batching state
    uint64_t batch_ns = 0;
    bool have_batch = false;
    std::vector<Event> ns_batch;
    ns_batch.reserve(32);

    auto flush_batch = [&](uint64_t ns){
        if (!have_batch) return;

        // summary header
        std::cout << "\n=== BATCH ns=" << ns << " (" << ns_batch.size() << " events) ===\n";
        for (const auto& ev : ns_batch) {
            std::cout << "[MSG] ns=" << ev.nanosec << " type=";
            switch (ev.type) {
                case MessageType::OrderbookState:
                    std::cout << "STATE state=" << ev.orderbook_state; break;
                case MessageType::AddOrder:
                    std::cout << "ADD id=" << ev.order_id << " side=" << (ev.side==Side::Buy?"B":"S")
                              << " qty=" << ev.quantity << " px=" << ev.price; break;
                case MessageType::ExecuteOrder:
                    std::cout << "EXEC id=" << ev.order_id << " side=" << (ev.side==Side::Buy?"B":"S")
                              << " qty=" << ev.quantity; break; // no price
                case MessageType::DeleteOrder:
                    std::cout << "DEL id=" << ev.order_id << " side=" << (ev.side==Side::Buy?"B":"S"); break;
                default: std::cout << "OTHER"; break;
            }
            std::cout << "\n";
        }

        // let strategy react to THIS ns (prev snapshot vs current top, phantom filter, etc.)
        strat.on_batch(ns, ob, ns_batch);

        // after strategy acts, print top + state + pos/pnl
        print_top(ob);
        std::cout << "STRAT: pos=" << strat.position()
                  << " pnl=" << strat.realized_pnl()
                  << " open=" << (ob.trading_open() ? "Y" : "N")
                  << "\n";

        ns_batch.clear();
        have_batch = false;
    };

    auto push = [&](const Event& ev) {
        // boundary BEFORE store/apply
        if (!have_batch) { batch_ns = ev.nanosec; have_batch = true; }
        else if (ev.nanosec != batch_ns) { flush_batch(batch_ns); batch_ns = ev.nanosec; have_batch = true; }

        ob.apply(ev);
        ns_batch.push_back(ev);
    };

    uint64_t ns = 100;

    // --- MARKET OPEN ---
    std::cout << "\n=== MARKET OPEN ===" << std::endl;
    push(make_state(BOOK, "P_SUREKLI_ISLEM", ns)); ns += 10;

    // ------------------------------------------------------------------
    // SCENARIO 1: Seed tight 100/110 with depth (never 0 or huge spread)
    // ------------------------------------------------------------------
    std::cout << "\n=== SCENARIO 1: INITIAL TIGHT (100/110) ===" << std::endl;
    // bids: 100, 90
    push(make_add(BOOK, /*id*/1000, Side::Buy, 100, LOT, 1, 1, ns));
    push(make_add(BOOK, /*id*/1001, Side::Buy,  90, LOT, 1, 2, ns));
    push(make_add(BOOK, /*id*/1002, Side::Buy,  80, LOT, 1, 3, ns));
    // asks: 110, 120
    push(make_add(BOOK, /*id*/2000, Side::Sell, 110, LOT, 1, 1, ns));
    push(make_add(BOOK, /*id*/2001, Side::Sell, 120, LOT, 1, 2, ns));
    push(make_add(BOOK, /*id*/2002, Side::Sell, 130, LOT, 1, 3, ns));
    ns += 10; flush_batch(batch_ns); // tight: 100/110
    print_topN(ob, 3, ns);

    // ---------------------------------------------------------
    // SCENARIO 2: Vanished ASK -> GAP (100/120) -> BUY @110
    // ---------------------------------------------------------
    std::cout << "\n=== SCENARIO 2: VANISHED ASK -> BUY @110 ===" << std::endl;
    // remove ask@110 (2000); next best ask is 120 -> gap 20
    push(make_exec(BOOK, 2000, Side::Sell, LOT, ns));
    ns += 10; flush_batch(batch_ns); // gap: 100/120 (BUY fires at 110)
    print_topN(ob, 3, ns);

    // --------------------------------------------------
    // SCENARIO 3: Retighten back to 100/110 (no trade)
    // --------------------------------------------------
    std::cout << "\n=== SCENARIO 3: RETIGHTEN to 100/110 ===" << std::endl;
    push(make_add(BOOK, /*id*/2003, Side::Sell, 110, LOT, 2, 1, ns));
    ns += 10; flush_batch(batch_ns); // tight: 100/110
    print_topN(ob, 3, ns);

    // -------------------------------------------------------------------
    // SCENARIO 4: Step up cleanly to tight 110/120 (no zero spread)
    // same ns: remove ask@110 and add bid@110 -> final 110/120 tight
    // -------------------------------------------------------------------
    std::cout << "\n=== SCENARIO 4: STEP to 110/120 (tight) ===" << std::endl;
    push(make_exec(BOOK, 2003, Side::Sell, LOT, ns));        // remove ask@110
    push(make_add(BOOK, /*id*/1003, Side::Buy, 110, LOT, 3, 1, ns)); // raise bid to 110
    // ask@120 (2001) already exists, so final = 110/120 (tight)
    ns += 10; flush_batch(batch_ns);    
    print_topN(ob, 3, ns);

    // -------------------------------------------------------------------
    // SCENARIO 5: Step up to tight 120/130 (no zero spread)
    // same ns: add bid@120, remove ask@120, add ask@130 -> final 120/130
    // -------------------------------------------------------------------
    std::cout << "\n=== SCENARIO 5: STEP to 120/130 (tight) ===" << std::endl;
    push(make_add (BOOK, /*id*/1004, Side::Buy, 120, LOT, 4, 1, ns)); // bid 120 appears
    push(make_exec(BOOK, 2001,          Side::Sell, LOT, ns));        // remove ask 120
    push(make_add (BOOK, /*id*/2004, Side::Sell, 140, LOT, 4, 2, ns)); // ask 140 appears
    ns += 10; flush_batch(batch_ns); // tight: 120/130
    print_topN(ob, 3, ns);

    // ---------------------------------------------------------
    // SCENARIO 6: Vanished BID -> GAP (110/130) -> SELL @120
    // ---------------------------------------------------------
    std::cout << "\n=== SCENARIO 6: VANISHED BID -> SELL @120 ===" << std::endl;
    push(make_exec(BOOK, 1004, Side::Buy, LOT, ns)); // remove bid@120; next bid is 110
    ns += 10; flush_batch(batch_ns); // gap: 110/130 (SELL fires at 120)
    print_topN(ob, 3, ns);

    // -----------------------------------------------------
    // SCENARIO 7: Retighten back to 120/130 (no trade)
    // -----------------------------------------------------
    std::cout << "\n=== SCENARIO 7: RETIGHTEN to 120/130 ===" << std::endl;
    push(make_add(BOOK, /*id*/1005, Side::Buy, 120, LOT, 5, 1, ns));
    ns += 10; flush_batch(batch_ns); // tight: 120/130
    print_topN(ob, 3, ns);

    // ----------------------------------------------------------------
    // SCENARIO 8: Phantom batch (same ns Exec+Add on ask@130) -> skip
    // final is tight 120/130 → no gap → no trade
    // ----------------------------------------------------------------
    std::cout << "\n=== SCENARIO 8: PHANTOM (Exec+Add same ns) ===" << std::endl;
    push(make_exec(BOOK, 2002, Side::Sell, LOT, ns));                  // remove ask@130
    push(make_add (BOOK, /*id*/2005, Side::Sell, 130, LOT, 6, 1, ns)); // re-add ask@130
    ns += 10; flush_batch(batch_ns); // stays tight 120/130
    print_topN(ob, 3, ns);


    // -------------------------------------------------------------
    // SCENARIO 9: Prepare deeper ask 140 (doesn't change the top)
    // -------------------------------------------------------------
    std::cout << "\n=== SCENARIO 9: PREPARE ask@140 depth ===" << std::endl;
    push(make_add(BOOK, /*id*/2006, Side::Sell, 150, LOT, 7, 1, ns));
    ns += 10; flush_batch(batch_ns); // still tight 120/130
    print_topN(ob, 3, ns);

    // -------------------------------------------------------------
    // SCENARIO 10: Vanished ASK -> GAP (120/140) -> BUY @130
    // -------------------------------------------------------------
    std::cout << "\n=== SCENARIO 10: VANISHED ASK -> BUY @130 ===" << std::endl;
    push(make_exec(BOOK, 2005, Side::Sell, LOT, ns)); // remove ask@130; next ask is 140
    ns += 10; flush_batch(batch_ns); // gap: 120/140 (BUY fires at 130)   
    print_topN(ob, 3, ns);

    // -------------------------------------------------------
    // SCENARIO 11: Retighten to 120/130 (no trade)
    // -------------------------------------------------------
    std::cout << "\n=== SCENARIO 11: RETIGHTEN to 120/130 ===" << std::endl;
    push(make_add(BOOK, /*id*/2007, Side::Sell, 130, LOT, 8, 1, ns));
    ns += 10; flush_batch(batch_ns); // tight: 120/130
    print_topN(ob, 3, ns);

    // ------------------------------------------------------------------
    // SCENARIO 12: Step up to tight 130/140 (no zero spread)
    // same ns: add bid@130 and remove ask@130 → final 130/140
    // ------------------------------------------------------------------
    std::cout << "\n=== SCENARIO 12: STEP to 130/140 (tight) ===" << std::endl;
    push(make_add (BOOK, /*id*/1006, Side::Buy, 130, LOT, 9, 1, ns)); // bid 130 appears
    push(make_exec(BOOK, 2007,          Side::Sell, LOT, ns));        // remove ask 130
    // ask@140 already exists (2005)
    ns += 10; flush_batch(batch_ns); // tight: 130/140
    print_topN(ob, 3, ns);

    // ------------------------------------------------------------------
    // SCENARIO 12: Step up to tight 140/150 (no zero spread)
    // same ns: add bid@130 and remove ask@130 → final 130/140
    // ------------------------------------------------------------------
    std::cout << "\n=== SCENARIO 12: STEP to 130/140 (tight) ===" << std::endl;
    push(make_add (BOOK, /*id*/1007, Side::Buy, 140, LOT, 9, 1, ns)); // bid 140 appears
    push(make_exec(BOOK, 2004,          Side::Sell, LOT, ns));        // remove ask 140
    // ask@140 already exists (2005)
    ns += 10; flush_batch(batch_ns); // tight: 130/140
    print_topN(ob, 3, ns);

    // We expect final position to be 100 and final pnl to be 2000
    // --- MARKET CLOSE ---
    std::cout << "\n=== MARKET CLOSE ===" << std::endl;
    push(make_state(BOOK, "P_MARJ_YAYIN_KAPANIS", ns)); ns += 10;
    flush_batch(batch_ns);

    std::cout << "\n[SIM DONE] final pos=" << strat.position()
              << " pnl=" << strat.realized_pnl() << "\n";

}
