#include "itch_parser.h"
#include "orderbook.h"
#include "strategy.h"
#include "types/event.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

// --- pretty printers ---
static void print_event(const Event& ev) {
    std::cout << "[MSG] ns=" << ev.nanosec << " type=";
    switch (ev.type) {
        case MessageType::OrderbookState:
            std::cout << "STATE book=" << ev.orderbook_id
                      << " state=" << ev.orderbook_state; break;
        case MessageType::AddOrder:
            std::cout << "ADD id=" << ev.order_id
                      << " side=" << (ev.side == Side::Buy ? "B" : "S")
                      << " qty=" << ev.quantity
                      << " px=" << ev.price; break;
        case MessageType::ExecuteOrder:
            std::cout << "EXEC id=" << ev.order_id
                      << " side=" << (ev.side == Side::Buy ? "B" : "S")
                      << " qty=" << ev.quantity; break; // exec has no price in spec
        case MessageType::DeleteOrder:
            std::cout << "DEL id=" << ev.order_id
                      << " side=" << (ev.side == Side::Buy ? "B" : "S"); break;
        default: std::cout << "OTHER"; break;
    }
    std::cout << "\n";
}

static void print_topN(const Orderbook& ob, size_t N,
                       uint64_t ns = 0, int64_t book_id = -1) {
    std::vector<std::pair<Price, Quantity>> bids, asks;
    ob.snapshot_n(N, bids, asks);

    std::cout << "---- SNAPSHOT"
              << " ns=" << ns
              << " book=" << (book_id >= 0 ? book_id : -1)
              << " open=" << (ob.trading_open() ? "Y" : "N")
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
                  << "\n";
    }
    std::cout << "------------------------------\n";
}

int main(int argc, char* argv[]) {
    const OrderbookId TARGET_BOOK = 73616;
    const char* FILE_PATH = "data/itch_data_250815_HI2.dat";
    
    // Check for quiet mode flag
    bool quiet_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet_mode = true;
            break;
        }
    }

    if (!quiet_mode) {
        std::cout << "Opening file: " << FILE_PATH << std::endl;
    }
    std::ifstream file(FILE_PATH, std::ios::binary);
    if (!file) { std::cerr << "Error opening file: " << FILE_PATH << "\n"; return 1; }

    if (!quiet_mode) {
        std::cout << "Creating parser..." << std::endl;
    }
    ItchParser parser(file);
    if (!quiet_mode) {
        std::cout << "Creating orderbook..." << std::endl;
    }
    Orderbook  book;
    if (!quiet_mode) {
        std::cout << "Creating strategy..." << std::endl;
    }
    Strategy   strat(TARGET_BOOK, /*order_qty=*/100, /*max_pos=*/1000, /*min_pos=*/0);

    bool   seen_open = false;
    size_t msgs_total = 0, batches_total = 0;

    // ns-batching state
    uint64_t cur_ns = 0;
    bool have_batch = false;
    std::vector<Event> ns_batch;
    ns_batch.reserve(64);

    auto flush_batch = [&](uint64_t ns){
        if (!have_batch) return;

        ++batches_total;
        if (!quiet_mode) {
            std::cout << "\n=== BATCH ns=" << ns << " (" << ns_batch.size() << " events) ===\n";
            for (const auto& ev : ns_batch) print_event(ev);
        }

        // run strategy after the book has all events for this ns
        strat.on_batch(ns, book, ns_batch);

        // print top-n after each batch
        if (!quiet_mode) {
            print_topN(book, 3, ns, TARGET_BOOK);
        }

        ns_batch.clear();
        have_batch = false;
    };

    if (!quiet_mode) {
        std::cout << "Starting main loop..." << std::endl;
    }
    while (file.good()) {
        auto events = parser.next_packet();
        if (events.empty()) continue;

        for (const auto& ev : events) {
            if (ev.orderbook_id != TARGET_BOOK) continue;

            // log all state messages (esp. close)
            if (ev.type == MessageType::OrderbookState) {
                if (!quiet_mode) {
                    std::cerr << "[STATE] ns=" << ev.nanosec
                              << " state=" << ev.orderbook_state << "\n";
                }
            }

            // detect continuous trading open
            if (!seen_open && ev.type == MessageType::OrderbookState &&
                ev.orderbook_state == "P_SUREKLI_ISLEM") {
                seen_open = true;
                std::cout << "[DAY START] Continuous trading begins.\n";
            }

            // ns boundary handling
            if (!have_batch) { cur_ns = ev.nanosec; have_batch = true; }
            else if (ev.nanosec != cur_ns) { flush_batch(cur_ns); cur_ns = ev.nanosec; have_batch = true; }

            // apply to book (tape order), then collect into this ns batch
            book.apply(ev);
            ns_batch.push_back(ev);
            ++msgs_total;

            // Check for EOD after adding to batch
            if (ev.type == MessageType::OrderbookState && 
                ev.orderbook_state == "P_MARJ_YAYIN_KAPANIS") {
                std::cout << "[DAY END] Market closed.\n";
                flush_batch(cur_ns);
                goto eod_reached;
            }
        }
    }

eod_reached:
    // flush final batch
    flush_batch(cur_ns);

    // explicit EOD settle (mark open pos with last trade price)

    // final summary
    double pnl_tl = static_cast<double>(strat.realized_pnl()) / 1000.0;
    std::cout << "[FINAL] batches=" << batches_total
              << " msgs=" << msgs_total
              << " pos=" << strat.position()
              << " pnl=" << strat.realized_pnl() << " converted to TL: " <<std::fixed << std::setprecision(2) << pnl_tl << " TL)\n";

    // final top-10 snapshot
    if (!quiet_mode) {
        print_topN(book, 5, cur_ns, TARGET_BOOK);
    }
    return 0;
}
