#include "strategy.h"
#include <algorithm>
#include <iostream>

namespace {
    // Configuration constants
    constexpr bool DEBUG_LOGS = false;                                    ///< Enable debug logging
    constexpr Price PRICE_TICK = 10;                                      ///< 1 tick in kuruş
    constexpr Price TIGHT_SPREAD = PRICE_TICK;                            ///< Normal tight market spread
    constexpr Price GAP_SPREAD = 2 * PRICE_TICK;                          ///< Required 1-tick gap spread
    constexpr const char* MARKET_CLOSE_STATE = "P_MARJ_YAYIN_KAPANIS";    ///< Market close state string
}

/**
 * @brief Debug logging function (no-op when DEBUG_LOGS is false)
 * @param function Function name for logging context
 * @param ns Nanosecond timestamp
 * @param message Debug message to log
 */
static void log_debug(const char* function, Nanoseconds ns, const std::string& message) {
    if (!DEBUG_LOGS) return;
    std::cout << "[DBG] " << function << " ns=" << ns << " " << message << "\n";
}

/**
 * @details Implementation notes:
 * - Initializes strategy with position limits and order sizing
 * - Sets initial state: flat position, zero P&L, trading open
 * - Validates input parameters (target_book must be non-zero)
 */
Strategy::Strategy(OrderbookId target_book,
					Quantity order_quantity,
					Quantity max_position,
					Quantity min_position) :
					target_book_(target_book),
					order_quantity_(order_quantity),
					max_position_(max_position),
					min_position_(min_position),
					position_(0),
					realized_pnl_(0),
					day_closed_(false) {
    if (target_book == 0) {
        std::cerr << "[ERROR] Strategy: Invalid target_book (0)\n";
    }
    if (order_quantity == 0) {
        std::cerr << "[ERROR] Strategy: Invalid order_quantity (0)\n";
    }
    if (max_position <= min_position) {
        std::cerr << "[ERROR] Strategy: Invalid position limits (max <= min)\n";
    }
}

/**
 * @details Implementation notes:
 * - Multi-step gap detection
 * - Time complexity: O(n) where n = events in batch
 * - Position limits enforced before trade execution
 * - End-of-day settlement using last executed price
 * - Early exits for invalid market conditions
 */
void Strategy::on_batch(Nanoseconds ns,
                        const Orderbook& ob,
                        const std::vector<Event>& batch)
{
    // Early exit conditions
    if (day_closed_) { 
        log_debug("on_batch", ns, "skip: day_closed"); 
        return; 
    }
    if (batch.empty()) {
        log_debug("on_batch", ns, "skip: empty batch");
        return;
    }

    // Hard stop on market close
    for (const auto& event : batch) {
        if (event.type == MessageType::OrderbookState &&
            event.orderbook_state == MARKET_CLOSE_STATE) {
            log_debug("on_batch", ns, "market_close detected -> settle_eod");
            settle_eod(ob);
            return; // do not update snapshot after EOD
        }
    }

    // require trading open and a top-of-book
    if (!ob.trading_open()) { 
        log_debug("on_batch", ns, "skip: trading not open"); 
        return; 
    }
    if (!ob.has_top()) { 
        log_debug("on_batch", ns, "skip: no top-of-book");  
        return; 
    }

    // Read current top once (we'll update prev_* to these at the end)
    const Price curr_best_bid = ob.best_bid_price();
    const Price curr_best_ask = ob.best_ask_price();
    const long  curr_spread   = static_cast<long>(curr_best_ask) - static_cast<long>(curr_best_bid);

    bool proceed = true;
    bool trade_executed = false;

    // "Oluşan kademe boşluğu, kademe değişiminin alıştan satışa veya satıştan alışa dönüşmesi ile oluşmamalıdır."
    // I applied this logic first but this logic is redundant since the events are 
    // submitted to the orderbook in same nanosecond batches (i.e. 'atomically')
    // Hence the logic is correct but redundant. So no need to keep this. 
    /*
    // Check if top-of-book gaps are immediately filled
    bool sell_executed_at_top = false; 
    bool buy_executed_at_top = false;   
    bool buy_added_at_gap = false;      
    bool sell_added_at_gap = false;     
    
    for (const auto& event : batch) {
        if (event.orderbook_id != target_book_) continue;
        
        if (event.type == MessageType::ExecuteOrder) {
            // Check if execution was at the top-of-book
            if (event.side == Side::Sell && event.price == prev_ask_) {
                sell_executed_at_top = true;
            }
            if (event.side == Side::Buy && event.price == prev_bid_) {
                buy_executed_at_top = true;
            }
        } else if (event.type == MessageType::AddOrder) {
            // Check if add order fills the gap created by top execution
            if (event.side == Side::Buy && sell_executed_at_top && event.price == prev_ask_) {
                buy_added_at_gap = true;  // Buy order added at the ask price that was executed
            }
            if (event.side == Side::Sell && buy_executed_at_top && event.price == prev_bid_) {
                sell_added_at_gap = true; // Sell order added at the bid price that was executed
            }
        }
    }
    
    // Ignore gaps that are immediately filled at the same price level
    if ((sell_executed_at_top && buy_added_at_gap) || (buy_executed_at_top && sell_added_at_gap)) {
        log_debug("on_batch", ns, "skip: top-of-book gap immediately filled");
        proceed = false;
    }
    */

    // this is for first snapshot
    if (proceed && !have_prev_) {
        log_debug("on_batch", ns, "first snapshot");
        proceed = false;
    }

    // require: prev was TIGHT, now is GAP (gap just formed this ns)
    if (proceed) {
        const long prev_spread = static_cast<long>(prev_ask_) - static_cast<long>(prev_bid_);
        if (!(prev_spread == TIGHT_SPREAD && curr_spread == GAP_SPREAD)) {
            log_debug("on_batch", ns, "skip: prev not tight or cur not gap");
            proceed = false;
        }
    }

    // direction: which top moved by exactly 1 tick? Trade at vanished price
    if (proceed) {
        // Ask moved UP by 1 tick, bid unchanged -> vanished ASK -> BUY @ prev_ask_
        if (curr_best_bid == prev_bid_ && (curr_best_ask - prev_ask_) == PRICE_TICK) {
            log_debug("on_batch", ns, "vanished ASK@" + std::to_string(prev_ask_) + " -> BUY");
            trade_executed = try_buy(prev_ask_);
        }
        // Bid moved DOWN by 1 tick, ask unchanged -> vanished BID -> SELL @ prev_bid_
        else if (curr_best_ask == prev_ask_ && (prev_bid_ - curr_best_bid) == PRICE_TICK) {
            log_debug("on_batch", ns, "vanished BID@" + std::to_string(prev_bid_) + " -> SELL");
            trade_executed = try_sell(prev_bid_);
        } else {
            log_debug("on_batch", ns, "skip: ambiguous move (both/none/>1 tick)");
        }
    }

    prev_bid_  = curr_best_bid;
    prev_ask_  = curr_best_ask;
    have_prev_ = true;

    if (!trade_executed) {
        log_debug("on_batch", ns, "no trade executed");
    }
}

/**
 * @details Implementation notes:
 * - Time complexity: O(1) - simple arithmetic operations
 * - Validates position limits before execution
 * - Updates P&L and position
 */
bool Strategy::try_buy(Price price) 
{
	Quantity max_buy = (position_ < max_position_) ? (max_position_ - position_) : 0;
	if (max_buy == 0) { 
		log_debug("try_buy", 0, "blocked: max_position reached"); 
		return false; 
	}

	Quantity fill_quantity = std::min(order_quantity_, max_buy);

	realized_pnl_ -= static_cast<int64_t>(fill_quantity) * static_cast<int64_t>(price);
	position_ += fill_quantity;

	std::cout << "[TRADE] BUY  " << fill_quantity << " @ " << price
              << " pos=" << position_ << " pnl=" << realized_pnl_ << "\n";
    return true;
}

/**
 * @details Implementation notes:
 * - Time complexity: O(1) - simple arithmetic operations
 * - Validates position limits before execution
 * - Updates P&L and position
 */
bool Strategy::try_sell(Price price)
{
	Quantity max_sell = (position_ > min_position_) ? (position_ - min_position_) : 0;
	if (max_sell == 0) { 
		log_debug("try_sell", 0, "blocked: min_position reached"); 
		return false; 
	}

	Quantity fill_quantity = std::min(order_quantity_, max_sell);

	realized_pnl_ += static_cast<int64_t>(fill_quantity) * static_cast<int64_t>(price);
	position_ -= fill_quantity;
    
    std::cout << "[TRADE] SELL " << fill_quantity << " @ " << price
              << " pos=" << position_ << " pnl=" << realized_pnl_ << "\n";
    return true;
}

/**
 * @details Implementation notes:
 * - Time complexity: O(1) - simple arithmetic operations
 * - Settles remaining position at last executed price
 * - Marks day as closed to prevent further trading
 * - Logs final position and P&L for monitoring
 */
void Strategy::settle_eod(const Orderbook& ob) 
{
	Price last_price = ob.last_exec_price();
	if (last_price != 0 && position_ != 0) 
	{
		realized_pnl_ += static_cast<int64_t>(position_) * static_cast<int64_t>(last_price);
	}

    std::cout << "[EOD] Close. last_exec_price=" << last_price
              << " final_pos=" << position_
              << " final_pnl=" << realized_pnl_
              << "\n";

    day_closed_ = true;
}

/**
 * @details Implementation notes:
 * - Simple wrapper around settle_eod for public interface
 * - Maintains consistent naming with header file
 */
void Strategy::end_of_day(const Orderbook& ob) { 
    settle_eod(ob); 
}









