#pragma once
#include "../types/side.h"
#include "../types/message_type.h"

/**
 * Parses a character to determine the order side
 * @param c Character to parse ('B' for Buy, 'S' for Sell)
 * @return Side enum value
 */
inline Side ParseSide(char c) noexcept 
{
    if (c == 'B') return Side::Buy;
    if (c == 'S') return Side::Sell;
    return Side::Unknown;
}

/**
 * Parses a character to determine the message type
 * @param c Character to parse ('O' for OrderbookState, 'A' for AddOrder, 
 *          'E' for ExecuteOrder, 'D' for DeleteOrder)
 * @return MessageType enum value
 */
inline MessageType ParseMessageType(char c) noexcept
{
    switch (c) {
        case 'O': return MessageType::OrderbookState;
        case 'A': return MessageType::AddOrder;
        case 'E': return MessageType::ExecuteOrder;
        case 'D': return MessageType::DeleteOrder;
        default:  return MessageType::Other;
    }
}
