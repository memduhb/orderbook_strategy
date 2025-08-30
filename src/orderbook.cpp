#include "orderbook.h"
#include <cassert>
#include <iostream>

namespace {
    constexpr Quantity MAX_SUSPICIOUS_QUANTITY = 1000000000;  // Maximum reasonable quantity (1 billion)
}

void Orderbook::apply(const Event& event) 
{
	switch (event.type) 
	{
		case MessageType::OrderbookState : handle_state(event); break;
		case MessageType::AddOrder : handle_add(event); break;
		case MessageType::ExecuteOrder : handle_exec(event); break;
		case MessageType::DeleteOrder : handle_delete(event); break;
		default : break;
	} 
}

void Orderbook::handle_state(const Event& event) 
{
	trading_open_ = (event.orderbook_state == "P_SUREKLI_ISLEM");
}

/**
 * @details Implementation notes:
 * - Orders inserted in FIFO in price levels (O(N)) (N = orders at same price)
 * - O(log n) for map insertion with level_for (emplace), O(N) for FIFO insertion
 * - Update aggregate quantity and number of orders
 */
void Orderbook::handle_add(const Event& event) 
{
	Order order { event.order_id, event.side, event.price, event.quantity, event.ranking_time, event.ranking_seq_num };

	if (event.quantity == 0 || event.price == 0) {
		std::cerr << "[WARN] ADD weird qty/price id=" << event.order_id
				  << " qty=" << event.quantity << " px=" << event.price << "\n";
	}

	PriceLevel& level = level_for(event.side, event.price);

	// Find insertion position based on ranking time and sequence number (FIFO order)
	auto pos = level.fifo.end();
	for (auto it = level.fifo.begin(); it != level.fifo.end(); ++it)
	{
		if ((order.ranking_time < it->ranking_time) || 
			(order.ranking_time == it->ranking_time && order.ranking_seq_num < it->ranking_seq_num))
		{
			pos = it; break;
		}
	}
	auto it = level.fifo.insert(pos, order);

	level.aggregate += order.quantity;
	level.num_orders += 1;
	index_[order.id] = OrderHandle { order.side, order.price, it };
}

/**
 * @details Implementation notes:
 * - Time complexity: O(1) for order lookup (hash table - unordered_map), O(1) for partial exec, O(N) for full exec
 * - Updates last_exec_price_ with execution price or falls back to order price
 * - Removes fully executed orders and cleans up empty price levels
 */
void Orderbook::handle_exec(const Event& event) 
{
	auto hit = index_.find(event.order_id);
	if (hit == index_.end()) 
	{
		std::cerr << "\033[31m[WARN]\033[0m EXEC for unknown order_id=" << event.order_id
				  << " qty=" << event.quantity << "\n";
        return;
	}

	if (event.quantity == 0 || event.quantity > MAX_SUSPICIOUS_QUANTITY) {
		std::cerr << "\033[31m[WARN]\033[0m EXEC suspicious qty id=" << event.order_id
				  << " qty=" << event.quantity << "\n";
        return;
	}

	OrderHandle& handle = hit->second;
	PriceLevel& level = (handle.side == Side::Buy) 
						? bids_.at(handle.price) 
						: asks_.at(handle.price);

	// Update last executed price
	Price current_price = event.price;
	if (current_price == 0) current_price = handle.price; // fallback
	last_exec_price_ = current_price;

	if (event.quantity >= handle.it->quantity) 
	{
		// sanity checks before mutation 
    	assert(level.aggregate >= handle.it->quantity &&
        "Level aggregate smaller than order quantity (book inconsistency)");
    	assert(level.num_orders > 0 &&
        "Trying to remove order from empty level");

		// remove order completely
		level.aggregate -= handle.it->quantity;
		level.num_orders -= 1;
		level.fifo.erase(handle.it);
		index_.erase(hit);
		erase_level_if_empty(handle.side, handle.price);
	}
	else 
	{
		// partial execution - reduce order quantity
		handle.it->quantity -= event.quantity;
		level.aggregate -= event.quantity;
	}
}

/**
 * @details Implementation notes:
 * - Time complexity: O(1) for order lookup, O(N) for removal (FIFO erase)
 * - Completely removes order from book (unlike partial execution)
 * - Cleans up empty price levels
 */
void Orderbook::handle_delete(const Event& event)
{
	auto hit = index_.find(event.order_id);
	if (hit == index_.end()) 
	{
		std::cerr << "\033[31m[WARN]\033[0m DELETE for unknown order_id=" << event.order_id
				  << " qty=" << event.quantity << "\n";
        return;
	}

	OrderHandle& handle = hit->second;
	PriceLevel& level = (handle.side == Side::Buy) ? bids_.at(handle.price) : asks_.at(handle.price);

	// remove order completely
	level.aggregate -= handle.it->quantity;
	level.num_orders -= 1;
	level.fifo.erase(handle.it);
	index_.erase(hit);
	erase_level_if_empty(handle.side, handle.price);
}

/**
 * @details Implementation notes:
 * - Uses std::map::emplace for insertion (O(log n))
 * - Initializes price field only if newly created
 */
PriceLevel& Orderbook::level_for(Side side, Price price) 
{
	if (side == Side::Buy)
	{
		auto result = bids_.emplace(price, PriceLevel{}); // std::pair 
		auto it = result.first;
		if (it->second.price == 0) it->second.price = price;
		return it->second;			// return price of that level
	}
	else
	{
		auto result = asks_.emplace(price, PriceLevel{});
		auto it = result.first;
		if (it->second.price == 0) it->second.price = price;
		return it->second;
	}
}

/**
 * @details Implementation notes:
 * - Normalize aggregate to 0 when num_orders reach 0
 * - Only remove level if both num_orders and aggregate are 0
 * - Called after order deletions and executions
 */
void Orderbook::erase_level_if_empty(Side side, Price price) {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            if (it->second.num_orders == 0) it->second.aggregate = 0; 
            if (it->second.num_orders == 0 && it->second.aggregate == 0) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            if (it->second.num_orders == 0) it->second.aggregate = 0; 
            if (it->second.num_orders == 0 && it->second.aggregate == 0) {
                asks_.erase(it);
            }
        }
    }
}

/**
 * @details Implementation notes:
 * - Time complexity: O(n) where n is number of price levels
 * - Only includes levels with aggregate > 0 (filters empty levels)
 * - Clears output vectors before populating
 * - Maintains price ordering (bids descending, asks ascending)
 */
void Orderbook::snapshot_n(size_t n,
                           DisplayLevel& bids_out,
                           DisplayLevel& asks_out) const
{
    bids_out.clear();
    asks_out.clear();

    size_t taken = 0;
    for (auto it = bids_.begin(); it != bids_.end() && taken < n; ++it) {
        const PriceLevel& lvl = it->second;
        if (lvl.aggregate > 0) {
            bids_out.emplace_back(it->first, lvl.aggregate);
            ++taken;
        }
    }

    taken = 0;
    for (auto it = asks_.begin(); it != asks_.end() && taken < n; ++it) {
        const PriceLevel& lvl = it->second;
        if (lvl.aggregate > 0) {
            asks_out.emplace_back(it->first, lvl.aggregate);
            ++taken;
        }
    }
}

/**
 * @details Implementation notes:
 * - Time complexity: O(n) where n is number of bid levels
 * - Returns first non-zero price in descending order (best bid)
 * - Returns 0 if no bids exist or all levels are empty
 */
Price Orderbook::first_nonzero_price_bid() const {
    for (const auto& kv : bids_) {
        if (kv.second.aggregate > 0) return kv.first;
    }
    return 0;
}

/**
 * @details Implementation notes:
 * - Time complexity: O(n) where n is number of bid levels
 * - Returns aggregate quantity at first non-zero price level
 * - Returns 0 if no bids exist or all levels are empty
 */
Quantity Orderbook::first_nonzero_qty_bid() const {
    for (const auto& kv : bids_) {
        if (kv.second.aggregate > 0) return kv.second.aggregate;
    }
    return 0;
}

/**
 * @details Implementation notes:
 * - Time complexity: O(n) where n is number of ask levels
 * - Returns first non-zero price in ascending order (best ask)
 * - Returns 0 if no asks exist or all levels are empty
 */
Price Orderbook::first_nonzero_price_ask() const {
    for (const auto& kv : asks_) {
        if (kv.second.aggregate > 0) return kv.first;
    }
    return 0;
}

/**
 * @details Implementation notes:
 * - Time complexity: O(n) where n is number of ask levels
 * - Returns aggregate quantity at first non-zero price level
 * - Returns 0 if no asks exist or all levels are empty
 */
Quantity Orderbook::first_nonzero_qty_ask() const {
    for (const auto& kv : asks_) {
        if (kv.second.aggregate > 0) return kv.second.aggregate;
    }
    return 0;
}























