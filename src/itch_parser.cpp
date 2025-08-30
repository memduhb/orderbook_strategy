#include "itch_parser.h"
#include "util/endian.h"
#include "util/parse_utils.h"
#include <iostream>

namespace {
    constexpr size_t MOLDUDP64_HEADER_SIZE = 20;
    constexpr size_t MAX_MESSAGE_COUNT = 10000;
    constexpr size_t MAX_MESSAGE_LENGTH = 65535;
}

ItchParser::ItchParser(std::istream& in) : in_(in) {
    buffer_.reserve(MAX_MESSAGE_LENGTH);  // pre-allocate buffer once
}

std::vector<Event> ItchParser::next_packet() {
    std::vector<Event> events;

    // read MoldUDP64 header
    char header[MOLDUDP64_HEADER_SIZE];
    in_.read(header, sizeof(header));
    if (!in_) return events; // EOF/short read -> no messages

    const std::string session(header, 10);
    const uint64_t seq_num = endian::read_u64_be(header + 10);
    (void)seq_num; 
    const uint16_t count   = endian::read_u16_be(header + 18);

    // sanity check count (protect against corruption)
    if (count == 0 || count > MAX_MESSAGE_COUNT) {
        std::cerr << "[ITCH] Invalid message count: " << count << "\n";
        return events;
    }

    events.reserve(count);

    // read each length-prefixed message
    for (uint16_t i = 0; i < count; ++i) {
        char lenbuf[2];
        in_.read(lenbuf, 2);
        if (!in_) {
            std::cerr << "[ITCH] Short read on length\n";
            break;
        }

        const uint16_t msg_len = endian::read_u16_be(lenbuf);

        // at least 1 byte for type, max 65535 bytes for payload
        if (msg_len < 1 || msg_len > MAX_MESSAGE_LENGTH) {
            std::cerr << "[ITCH] Invalid message length: " << msg_len << "\n";
            break;
        }
		
		// copy message to buffer
        buffer_.resize(msg_len);
        in_.read(&buffer_[0], msg_len);
        if (!in_) {
            std::cerr << "[ITCH] Short read on payload\n";
            break;
        }

        // safety check: ensure buffer has data
        if (buffer_.empty() || msg_len == 0) {
            std::cerr << "[ITCH] Empty buffer or message\n";
            continue;
        }
        
        Event ev = parse_message(&buffer_[0], msg_len);
        if (ev.type != MessageType::Other) {
            events.push_back(ev);
        } else {
            static int unknown_dbg = 0;
            if (unknown_dbg < 5) {
                std::cerr << "[ITCH] Unknown message type: 0x"
                          << std::hex << (unsigned)(unsigned char)buffer_[0]
                          << std::dec << "\n";
                ++unknown_dbg;
            }
        }
    }

    return events;
}

Event ItchParser::parse_message(const char* msg, size_t len)
{
    /**
     * @brief Helper struct for safe message parsing with bounds checking
     * 
     * Provides a safe interface for reading from message buffers with
     * automatic bounds checking and position tracking.
     */
    struct Cur {
        const char* p;      // Current position in message buffer
        const char* end;    // End of message buffer
        bool ok(size_t n) const { 
            return __builtin_expect(size_t(end - p) >= n, 1);  
        }
        const char* take(size_t n) { const char* r = p; p += n; return r; }
        size_t remaining() const { return size_t(end - p); }
    };
    
    // Helper lambda functions for big-endian reading
    auto BE32 = [](const char* p){ return endian::read_u32_be(p); };
    auto BE64 = [](const char* p){ return endian::read_u64_be(p); };

    Event event{};                      
    if (len < 1) return event;

    const MessageType type = ParseMessageType(msg[0]);
    event.type = type;

    Cur cur{ msg + 1, msg + len }; 

    switch (type) {
    case MessageType::OrderbookState: {
        // ns(4) + book(4) + state(20 space-padded) = 28
        if (__builtin_expect(!cur.ok(4 + 4 + 20), 0)) { 
            event.type = MessageType::Other; 
            return event; 
        }
        event.nanosec      = BE32(cur.take(4));
        event.orderbook_id = BE32(cur.take(4));
        
        const char* state_start = cur.take(20);
        size_t state_len = 20;
        while (state_len > 0 && state_start[state_len-1] == ' ') --state_len;
        event.orderbook_state.assign(state_start, state_len);
        
        break;
    }

    case MessageType::AddOrder: {
        // ns(4) + id(8) + book(4) + side(1) + ranking_seq_num(4) + qty(8) + price(4) + attrs(2) + lot_type(1) + ranking_time(8) = 51
        if (__builtin_expect(!cur.ok(4 + 8 + 4 + 1 + 4 + 8 + 4 + 2 + 1 + 8), 0)) { 
            event.type = MessageType::Other; 
            return event; 
        }
        event.nanosec          = BE32(cur.take(4));
        event.order_id         = BE64(cur.take(8));
        event.orderbook_id     = BE32(cur.take(4));
        event.side             = ParseSide(*cur.take(1));
        event.ranking_seq_num  = BE32(cur.take(4));
        event.quantity         = BE64(cur.take(8));
        event.price            = BE32(cur.take(4));
        (void)cur.take(2); // skip Order Attributes
        (void)cur.take(1); // skip Lot Type
        event.ranking_time     = BE64(cur.take(8));
        break;
    }

    case MessageType::ExecuteOrder: {
        // ns(4) + id(8) + book(4) + side(1) + qty(8) + match(8) + combo(4) + res(7) + res(7) = 51
        if (__builtin_expect(!cur.ok(4 + 8 + 4 + 1 + 8), 0)) { 
            event.type = MessageType::Other; 
            return event; 
        }

        event.nanosec      = BE32(cur.take(4));
        event.order_id     = BE64(cur.take(8));
        event.orderbook_id = BE32(cur.take(4));
        event.side         = ParseSide(*cur.take(1));
        event.quantity     = BE64(cur.take(8));   // Executed Quantity

        // Consume remaining fields if present
        if (cur.remaining() >= 8)  (void)BE64(cur.take(8));  // Match ID
        if (cur.remaining() >= 4)  (void)BE32(cur.take(4));  // Combo Group ID
        if (cur.remaining() >= 7)  (void)cur.take(7);        // Reserved
        if (cur.remaining() >= 7)  (void)cur.take(7);        // Reserved

        break;
    }

    case MessageType::DeleteOrder: {
        // ns(4) + order_id(8) + book(4) + side(1) = 13
        if (__builtin_expect(!cur.ok(4 + 8 + 4 + 1), 0)) { 
            event.type = MessageType::Other; 
            return event; 
        }
        event.nanosec      = BE32(cur.take(4));
        event.order_id     = BE64(cur.take(8));
        event.orderbook_id = BE32(cur.take(4));
        event.side         = ParseSide(*cur.take(1));
        break;
    }

    default:
        event.type = MessageType::Other;
        break;
    }

    return event;
}













