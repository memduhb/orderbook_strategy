#pragma once
#include <cstdint>  

/**
 * @brief Order side enumeration (Buy/Sell) - matches ITCH protocol codes
 * 
 * @details Used throughout the order book to distinguish between bid and ask orders.
 * The enum values correspond to ITCH protocol character codes for compatibility.
 */
enum class Side : uint8_t 
{ 
	Buy = 'B', 
	Sell = 'S', 
	Unknown = 0 
};
