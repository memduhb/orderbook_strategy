#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <utility>

/**
 * @brief Type aliases for the order book system
 * 
 * @details Provides semantic type names for the trading system, making the code
 * more readable and maintainable. All types are based on standard integer types
 * for performance and compatibility with ITCH protocol specifications.
 */

// Time types
using Nanoseconds = std::uint32_t;
using RankingTime = std::uint64_t;

// Order book types  
using OrderbookId = std::uint32_t;
using OrderbookState = std::string;

// Order types
using OrderId = std::uint64_t;
using Quantity = std::uint64_t;
using Price = std::uint32_t;
using RankingSeqNum = std::uint32_t;

// Display types
using DisplayLevel = std::vector<std::pair<Price, Quantity>>;