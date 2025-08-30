#pragma once
#include <cstdint>

/**
 * @brief ITCH message types enumeration - matches protocol character codes
 * 
 * @details Defines the different types of ITCH protocol messages that can be
 * parsed from the data stream. Each value corresponds to the character code
 * used in the ITCH protocol specification.
 */
enum class MessageType : uint8_t {
    OrderbookState = 'O',
    AddOrder       = 'A',
    ExecuteOrder   = 'E',
    DeleteOrder    = 'D',
    Other          = 0
};
