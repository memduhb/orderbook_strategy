#pragma once

#include <map>
#include <list>
#include <unordered_map>
#include <vector>


#include "types/event.h"

/**
 * @brief Represents a single order in the order book
 * 
 * Contains all order details including ID, side, price, quantity,
 * and ranking information for FIFO ordering within price levels.
 */
struct Order 
{
	OrderId 		id{};
	Side 			side{Side::Unknown};
	Price			price{};
	Quantity 		quantity{};
	RankingTime 	ranking_time{};
	RankingSeqNum 	ranking_seq_num{};

    /**
     * @brief Constructs an order with all required fields
     * @param id Order identifier
     * @param side Order side (Buy/Sell)
     * @param price Order price
     * @param quantity Order quantity
     * @param ranking_time Ranking timestamp
     * @param ranking_seq_num Ranking sequence number
     */
    Order(OrderId id, Side side, Price price, Quantity quantity, 
          RankingTime ranking_time, RankingSeqNum ranking_seq_num)
    : id(id), side(side), price(price), quantity(quantity), 
      ranking_time(ranking_time), ranking_seq_num(ranking_seq_num) {}
};

/**
 * @brief Represents a price level containing multiple orders
 * 
 * Maintains aggregate quantity and order count for a specific price,
 * with orders stored in FIFO order based on ranking time and sequence.
 */
struct PriceLevel 
{
    Price price{};                   ///< Price for this level
    Quantity aggregate{};            ///< Total quantity at this level
    uint32_t num_orders{};           ///< Number of orders at this level
    std::list<Order> fifo;           ///< Orders sorted by time/sequence (FIFO)
};

/**
 * @brief Handle to locate an order within the order book
 * 
 * Stores the side, price, and iterator needed to find and modify
 * a specific order in the order book's data structures.
 */
struct OrderHandle 
{
    Side side{Side::Unknown};                    ///< Order side
    Price price{};                               ///< Order price
    std::list<Order>::iterator it;               ///< Iterator to order in FIFO list

    OrderHandle() = default;
    
    /**
     * @brief Constructs an order handle
     * @param s Order side
     * @param p Order price
     * @param iter Iterator to order in FIFO list
     */
    OrderHandle(Side s, Price p, std::list<Order>::iterator iter)
    : side(s), price(p), it(iter) {}
};

/**
 * @brief Maintains a complete order book with bid and ask sides
 * 
 * Processes ITCH events to maintain real-time order book state.
 * Supports order addition, execution, deletion, and state changes.
 * Provides efficient queries for best bid/ask prices and quantities.
 */
class Orderbook 
{
public: 
    // Constructors and assignment
    Orderbook() = default; 
    Orderbook(const Orderbook&) = delete;                       ///< No copy constructor
    Orderbook& operator=(const Orderbook&) = delete;            ///< No copy assignment
    Orderbook(Orderbook&&) = delete;                            ///< No move constructor
    Orderbook& operator=(Orderbook&&) = delete;                 ///< No move assignment
    
    // Core operations
    /**
     * @brief Applies an ITCH event to the order book
     * @param event The ITCH event to process
     * 
     * Routes the event to appropriate handler based on message type:
     * - OrderbookState: Updates trading state
     * - AddOrder: Adds new order to appropriate side
     * - ExecuteOrder: Reduces order quantity or removes if fully executed
     * - DeleteOrder: Removes order from book
     */
    void apply(const Event& event);

    // Trading state queries
    /**
     * @brief Checks if trading is currently open
     * @return true if order book state is "P_SUREKLI_ISLEM"
     */
    bool trading_open() const { return trading_open_; }
    
    /**
     * @brief Checks if both bid and ask sides have orders
     * @return true if both sides have at least one order
     */
    bool has_top() const { return !bids_.empty() && !asks_.empty(); }

    // Best bid/ask queries
    /**
     * @brief Gets the best bid price (highest buy price)
     * @return Best bid price, or 0 if no bids
     */
    Price best_bid_price() const { return first_nonzero_price_bid(); }
    
    /**
     * @brief Gets the quantity at best bid price
     * @return Total quantity at best bid, or 0 if no bids
     */
    Quantity best_bid_quantity() const { return first_nonzero_qty_bid(); }
    
    /**
     * @brief Gets the best ask price (lowest sell price)
     * @return Best ask price, or 0 if no asks
     */
    Price best_ask_price() const { return first_nonzero_price_ask(); }
    
    /**
     * @brief Gets the quantity at best ask price
     * @return Total quantity at best ask, or 0 if no asks
     */
    Quantity best_ask_quantity() const { return first_nonzero_qty_ask(); }
    
    /**
     * @brief Gets the last executed price
     * @return Last execution price, or 0 if no executions
     */
    Price last_exec_price() const { return last_exec_price_; }

    /**
     * @brief Gets the total number of active orders
     * @return Number of orders in the book
     */
    size_t order_count() const { return index_.size(); }
    
    /**
     * @brief Checks if order book is completely empty
     * @return true if no orders on either side
     */
    bool empty() const { return asks_.empty() && bids_.empty(); }

    /**
     * @brief Creates a snapshot of top N price levels
     * @param n Number of levels to include in snapshot
     * @param bids_out Output vector for bid levels (price, quantity pairs)
     * @param asks_out Output vector for ask levels (price, quantity pairs)
     * 
     * Fills the output vectors with the top N price levels from each side,
     * sorted by price (bids descending, asks ascending). Only includes
     * levels with aggregate quantity > 0.
     */
    void snapshot_n(size_t n, 
                    std::vector<std::pair<Price, Quantity>>& bids_out,
                    std::vector<std::pair<Price, Quantity>>& asks_out) const;

private: 
    // Data structures
    std::map<Price, PriceLevel, std::greater<Price>> bids_;  ///< Bid side (price descending)
    std::map<Price, PriceLevel, std::less<Price>> asks_;     ///< Ask side (price ascending)
    std::unordered_map<OrderId, OrderHandle> index_;         ///< Order lookup by ID

    // State
    bool trading_open_{false};       ///< Trading state flag
    Price last_exec_price_{0};       ///< Last execution price

    // Event handlers
    /**
     * @brief Handles OrderbookState events
     * @param event State change event
     */
    void handle_state(const Event& event);
    
    /**
     * @brief Handles AddOrder events
     * @param event Add order event
     */
    void handle_add(const Event& event);
    
    /**
     * @brief Handles ExecuteOrder events
     * @param event Execute order event
     */
    void handle_exec(const Event& event);
    
    /**
     * @brief Handles DeleteOrder events
     * @param event Delete order event
     */
    void handle_delete(const Event& event);

    // Utility methods
    /**
     * @brief Gets or creates a price level
     * @param side Order side (Buy/Sell)
     * @param price Price level
     * @return Reference to the price level
     */
    PriceLevel& level_for(Side side, Price price);
    
    /**
     * @brief Removes empty price levels
     * @param side Order side
     * @param price Price level to check
     */
    void erase_level_if_empty(Side side, Price price);

    // Helper methods for best bid/ask
    /**
     * @brief Finds first non-zero bid price
     * @return First non-zero bid price, or 0 if none
     */
    Price first_nonzero_price_bid() const;
    
    /**
     * @brief Finds quantity at first non-zero bid
     * @return Quantity at first non-zero bid, or 0 if none
     */
    Quantity first_nonzero_qty_bid() const;
    
    /**
     * @brief Finds first non-zero ask price
     * @return First non-zero ask price, or 0 if none
     */
    Price first_nonzero_price_ask() const;
    
    /**
     * @brief Finds quantity at first non-zero ask
     * @return Quantity at first non-zero ask, or 0 if none
     */
    Quantity first_nonzero_qty_ask() const;
};







