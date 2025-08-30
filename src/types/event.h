#pragma once
#include <string>
#include "usings.h"
#include "side.h"
#include "message_type.h"

/**
 * @brief Represents an ITCH message event parsed from the data stream
 * 
 * @details This struct holds all possible fields from ITCH protocol messages.
 * Not all fields are used for every message type:
 * - AddOrder: uses order_id, side, quantity, price, ranking_time, ranking_seq_num
 * - ExecuteOrder: uses order_id, side, quantity
 * - DeleteOrder: uses order_id, side
 * - OrderbookState: uses orderbook_state
 */
struct Event {
    Event() = default;  

    // Message type
    MessageType   type = MessageType::Other;

    // Time fields
    Nanoseconds   nanosec = 0;
    RankingTime   ranking_time = 0;

    // Order book and order identifiers
    OrderbookId   orderbook_id = 0;
    Side          side = Side::Unknown;
    OrderId       order_id = 0;

    // Order details
    Quantity      quantity = 0;
    Price         price = 0;
    RankingSeqNum ranking_seq_num = 0;

    // Orderbook state message
    OrderbookState orderbook_state;
};


