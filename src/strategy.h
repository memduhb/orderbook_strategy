#pragma once

#include "types/event.h"
#include "orderbook.h"
#include <vector>
#include <cstdint>

/**
 * @brief Trading strategy that detects and exploits 1-tick gaps in the order book
 * 
 * @details Monitors the order book for 1-tick gaps (2 kuruş difference between
 * bid and ask prices) and places orders to capture the spread. Implements
 * position limits and tracks realized P&L. The strategy operates on nanosecond
 * batches to ensure proper order sequencing and gap detection.
 * 
 * Strategy Logic:
 * - Detects when bid-ask spread becomes exactly 2 kuruş (1 tick)
 * - Places orders to capture the spread when valid gaps are found
 * - Maintains position within specified limits (max_position, min_position)
 * - Tracks realized profit/loss from completed trades
 * - Handles end-of-day settlement using last executed price
 */
class Strategy 
{
public: 
	/**
	 * @brief Constructs a trading strategy with specified parameters
	 * @param target_book Order book ID to monitor for trading opportunities
	 * @param order_quantity Size of orders to place when gaps are detected
	 * @param max_position Maximum long position allowed (positive value)
	 * @param min_position Minimum short position allowed (negative value)
	 * 
	 * @details Initializes the strategy with position limits and order sizing.
	 * The strategy will not place orders that would exceed these position limits.
	 */
	Strategy(	OrderbookId target_book,
				Quantity order_quantity,
				Quantity max_position,
				Quantity min_position);

	/**
	 * @brief Processes a batch of events and executes trading logic
	 * @param ns Nanosecond timestamp of the batch
	 * @param ob Current order book state
	 * @param batch Vector of events in this time batch
	 * 
	 * @details Analyzes the order book state and event batch to detect
	 * 1-tick gaps. If a valid gap is found and position limits allow,
	 * places appropriate buy/sell orders to capture the spread.
	 */
	void on_batch(	Nanoseconds ns,
					const Orderbook& ob,
					const std::vector<Event>& batch);

	/**
	 * @brief Gets the current position size
	 * @return Current position (positive = long, negative = short, 0 = flat)
	 * 
	 * @details Returns the net position across all completed trades.
	 * Positive values indicate long positions, negative values indicate short positions.
	 */
	Quantity position() const { return position_; }

	/**
	 * @brief Handles end-of-day processing and position settlement
	 * @param ob Final order book state for settlement
	 * 
	 * @details Marks the end of trading day and settles any remaining
	 * position using the last executed price from the order book.
	 */
	void end_of_day(const Orderbook& ob);

	/**
	 * @brief Gets the realized profit/loss from completed trades
	 * @return Realized P&L in kuruş (positive = profit, negative = loss)
	 * 
	 * @details Returns the cumulative profit/loss from all completed trades,
	 * excluding unrealized gains/losses from open positions.
	 */
	int64_t realized_pnl() const { return realized_pnl_; }

private: 
	// Configuration parameters
	OrderbookId target_book_;      // target order book id
	Quantity 	order_quantity_;   // size of orders to place
	Quantity 	max_position_;     // maximum long position allowed
	Quantity 	min_position_;     // minimum short position allowed

	// Current state
	Quantity 	position_ = 0;     // current net position (long = positive, short = negative)
	Price prev_bid_  = 0;          // previous best bid price for gap detection
    Price prev_ask_  = 0;          // previous best ask price for gap detection
	int64_t 	realized_pnl_ = 0; // cumulative realized profit/loss in kuruş

	// Trading state flags
	bool day_closed_ = false;      // flag indicating end-of-day has been processed
	bool have_prev_  = false;      // flag indicating we have previous prices for gap detection

	/**
	 * @brief Attempts to place a buy order at the specified price
	 * @param price Price to place the buy order at
	 * @return true if order was placed successfully, false otherwise
	 * 
	 * @details Checks position limits before placing the order.
	 * Only places order if it wouldn't exceed max_position_.
	 */
	bool try_buy(Price price);

	/**
	 * @brief Attempts to place a sell order at the specified price
	 * @param price Price to place the sell order at
	 * @return true if order was placed successfully, false otherwise
	 * 
	 * @details Checks position limits before placing the order.
	 * Only places order if it wouldn't exceed min_position_.
	 */
	bool try_sell(Price price);

	/**
	 * @brief Settles any remaining position at end-of-day
	 * @param ob Final order book state for settlement
	 * 
	 * @details Calculates unrealized P&L on remaining position using
	 * the last executed price and adds it to realized P&L.
	 */
	void settle_eod(const Orderbook& ob);
};