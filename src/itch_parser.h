#pragma once
#include <istream>
#include <vector>
#include "types/event.h"

/**
 * @brief Parser for ITCH protocol messages from MoldUDP64 packets
 * 
 * @details Reads binary data from an input stream and parses individual ITCH messages
 * into Event objects. Handles MoldUDP64 packet structure and ITCH message parsing.
 * Supports ITCH message types: OrderbookState, AddOrder, ExecuteOrder, and DeleteOrder.
 */
class ItchParser 
{
public:
    /**
     * @brief Constructs parser with input stream
     * @param in Input stream containing ITCH data
     */
    explicit ItchParser(std::istream& in);

    /**
     * @brief Parses next MoldUDP64 packet and returns all ITCH messages
     * @return Vector of parsed Event objects (empty if end of stream)
     * 
     * @details Reads a complete MoldUDP64 packet from the input stream,
     * parses the header to determine message count, then processes each
     * length-prefixed ITCH message within the packet.
     */
    std::vector<Event> next_packet();

private: 
    std::istream& in_;  ///< Input stream reference for reading ITCH data
    std::vector<char> buffer_;  ///< Pre-allocated buffer for message parsing

    /**
     * @brief Parses individual ITCH message into Event
     * @param msg Raw message buffer pointer
     * @param len Length of message in bytes
     * @return Parsed Event object with all relevant fields populated
     * 
     * @details Parses the message type byte and routes to appropriate
     * parsing logic based on the ITCH protocol specification.
     */
    Event parse_message(const char* msg, size_t len);
};
