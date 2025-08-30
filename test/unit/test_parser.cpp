// test_parser.cpp
#include "itch_parser.h"
#include "types/event.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <map>

// pretty printer for events
static void print_event(const Event& ev) {
    std::cout << "[EVENT] ns=" << ev.nanosec << " type=";
    switch (ev.type) {
        case MessageType::OrderbookState:
            std::cout << "STATE book=" << ev.orderbook_id
                      << " state='" << ev.orderbook_state << "'"; 
            break;
        case MessageType::AddOrder:
            std::cout << "ADD id=" << ev.order_id
                      << " book=" << ev.orderbook_id
                      << " side=" << (ev.side == Side::Buy ? "B" : "S")
                      << " qty=" << ev.quantity
                      << " px=" << ev.price
                      << " rank_seq=" << ev.ranking_seq_num
                      << " rank_time=" << ev.ranking_time; 
            break;
        case MessageType::ExecuteOrder:
            std::cout << "EXEC id=" << ev.order_id
                      << " book=" << ev.orderbook_id
                      << " side=" << (ev.side == Side::Buy ? "B" : "S")
                      << " qty=" << ev.quantity; 
            break;
        case MessageType::DeleteOrder:
            std::cout << "DEL id=" << ev.order_id
                      << " book=" << ev.orderbook_id
                      << " side=" << (ev.side == Side::Buy ? "B" : "S"); 
            break;
        default: 
            std::cout << "OTHER"; 
            break;
    }
    std::cout << "\n";
}

// statistics tracking
struct ParserStats {
    size_t total_packets = 0;
    size_t total_events = 0;
    size_t state_events = 0;
    size_t add_events = 0;
    size_t exec_events = 0;
    size_t del_events = 0;
    size_t other_events = 0;
    size_t target_book_events = 0;
    
    void print() const {
        std::cout << "\n=== PARSER STATISTICS ===\n";
        std::cout << "Total packets processed: " << total_packets << "\n";
        std::cout << "Total events: " << total_events << "\n";
        std::cout << "  - State events: " << state_events << "\n";
        std::cout << "  - Add events: " << add_events << "\n";
        std::cout << "  - Exec events: " << exec_events << "\n";
        std::cout << "  - Del events: " << del_events << "\n";
        std::cout << "  - Other events: " << other_events << "\n";
        std::cout << "Target book events: " << target_book_events << "\n";
        std::cout << "========================\n";
    }
};

int main() {
    const char* FILE_PATH = "data/itch_data_250815_HI2.dat";
    const OrderbookId TARGET_BOOK = 73616;  // Same as in main.cpp
    
    std::cout << "Testing ITCH Parser with file: " << FILE_PATH << std::endl;
    
    // Open the data file
    std::ifstream file(FILE_PATH, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open file " << FILE_PATH << std::endl;
        return 1;
    }
    
    // Create parser
    ItchParser parser(file);
    ParserStats stats;
    
    // Track some interesting events for detailed inspection
    std::vector<Event> interesting_events;
    const size_t MAX_INTERESTING = 20;
    
    std::cout << "\n=== PARSING PACKETS ===" << std::endl;
    
    // Parse packets and collect statistics
    size_t packet_count = 0;
    while (file.good()) {
        auto events = parser.next_packet();
        if (events.empty()) {
            if (file.eof()) {
                std::cout << "Reached end of file after " << packet_count << " packets\n";
                break;
            } else {
                std::cout << "No events in packet " << packet_count << " (possibly corrupted)\n";
                continue;
            }
        }
        
        ++stats.total_packets;
        stats.total_events += events.size();
        
        // Print only events for target book
        bool has_target_events = false;
        for (const auto& ev : events) {
            if (ev.orderbook_id == TARGET_BOOK) {
                if (!has_target_events) {
                    std::cout << "\n--- Packet " << packet_count << " (target book events) ---\n";
                    has_target_events = true;
                }
                print_event(ev);
            }
        }
        
        // Collect statistics and interesting events
        for (const auto& ev : events) {
            switch (ev.type) {
                case MessageType::OrderbookState: ++stats.state_events; break;
                case MessageType::AddOrder: ++stats.add_events; break;
                case MessageType::ExecuteOrder: ++stats.exec_events; break;
                case MessageType::DeleteOrder: ++stats.del_events; break;
                default: ++stats.other_events; break;
            }
            
            // Track target book events
            if (ev.orderbook_id == TARGET_BOOK) {
                ++stats.target_book_events;
                if (interesting_events.size() < MAX_INTERESTING) {
                    interesting_events.push_back(ev);
                }
            }
        }
        
        ++packet_count;
        

    }
    
    // Print statistics
    stats.print();
    
    // Print interesting events from target book
    if (!interesting_events.empty()) {
        std::cout << "\n=== TARGET BOOK EVENTS (Book " << TARGET_BOOK << ") ===\n";
        for (const auto& ev : interesting_events) {
            print_event(ev);
        }
    }
    
    // Test edge cases and error handling
    std::cout << "\n=== TESTING EDGE CASES ===" << std::endl;
    
    // Test with empty file
    std::cout << "Testing empty file handling..." << std::endl;
    std::stringstream empty_stream;
    ItchParser empty_parser(empty_stream);
    auto empty_events = empty_parser.next_packet();
    std::cout << "Empty file returned " << empty_events.size() << " events (expected 0)\n";
    
    // Test with corrupted data (short header)
    std::cout << "Testing corrupted data handling..." << std::endl;
    std::stringstream corrupted_stream;
    corrupted_stream.write("SHORT", 5);  // Too short for MoldUDP64 header
    ItchParser corrupted_parser(corrupted_stream);
    auto corrupted_events = corrupted_parser.next_packet();
    std::cout << "Corrupted data returned " << corrupted_events.size() << " events (expected 0)\n";
    
    std::cout << "\n[TEST_PARSER DONE] Successfully tested ITCH parser\n";
    return 0;
}

