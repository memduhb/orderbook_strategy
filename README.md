# Order Book Strategy

A C++ program that reads market data and trades when the bid-ask spread changes from 1 kuruş to 2 kuruş.

## Overview

This program:
- Reads ITCH format market data
- Tracks bid and ask prices in an order book
- Buys when the ask price disappears and spread widens to 2 kuruş
- Sells when the bid price disappears and spread widens to 2 kuruş
- Processes events in batches by timestamp

## Project Structure

```
order_book_strategy/
├── src/                   # Core source files
│   ├── types/             # Type definitions
│   │   ├── event.h        # Event structures
│   │   ├── message_type.h # ITCH message types
│   │   ├── side.h         # Buy/Sell side definitions
│   │   └── usings.h       # Type aliases
│   ├── util/              # Utility functions
│   │   ├── endian.h       # Endianness utilities
│   │   └── parse_utils.h  # Parsing utilities
│   ├── orderbook.h        # Order book header
│   ├── orderbook.cpp      # Order book implementation
│   ├── strategy.h         # Trading strategy header
│   ├── strategy.cpp       # Trading strategy implementation
│   ├── itch_parser.h      # ITCH parser header
│   └── itch_parser.cpp    # ITCH parser implementation
├── test/                  # Test files
│   ├── unit/             # Unit tests
│   │   ├── test_parser.cpp    # Parser unit tests
│   │   ├── test_orderbook.cpp # Order book unit tests
│   │   └── test_strategy.cpp  # Strategy unit tests
│   └── integration/      # Integration tests
│       └── main.cpp      # End-to-end integration test
├── data/                 # Market data files
│   └── itch_data_250815_HI2.dat  # ITCH format market data
├── Makefile              # Build configuration
└── README.md            # This file
```

## Key Components

### Order Book (`src/orderbook.*`)
- Stores bid and ask orders by price
- Handles adding, executing, and removing orders
- Shows current best bid and ask prices
- Groups events by timestamp

### Trading Strategy (`src/strategy.*`)
- Watches for spread changes
- Buys when spread goes from 1 to 2 kuruş after ask disappears
- Sells when spread goes from 1 to 2 kuruş after bid disappears
- Keeps track of position and profit/loss

### ITCH Parser (`src/itch_parser.*`)
- Reads ITCH market data files
- Handles Add, Execute, Delete, and State messages
- Converts raw data into order book events

## How It Works

The program trades based on these rules:

1. **Wait for tight spread**: When bid-ask spread is 1 kuruş
2. **Watch for gap**: When spread widens to 2 kuruş after being tight
3. **Trade**: 
   - Buy when ask disappears (spread widens above)
   - Sell when bid disappears (spread widens below)
4. **Track results**: Keep count of position and profit/loss

### Key Numbers
- `PRICE_TICK = 10` (1 kuruş per tick)
- `TIGHT_SPREAD = 10` (1 kuruş spread)
- `GAP_SPREAD = 20` (2 kuruş spread)

## Building and Running

### Prerequisites
- C++ compiler
- Make

### Build Commands
```bash
# Build everything
make

# Build specific tests
make test-parser      # Parser test
make test-orderbook   # Order book test  
make test-strategy    # Strategy test
make integration      # Full program test

# Run tests
make run-test-parser
make run-test-orderbook
make run-test-strategy
make run              # Run full program (verbose)
make run-quiet        # Run full program (quiet mode)

# Clean up
make clean
```

### Test Output Examples

#### Parser Test
```bash
make run-test-parser
```
Shows parsed market data messages and counts.

#### Strategy Test
```bash
make run-test-strategy
```
Shows trading scenarios:
- Setting up tight spreads
- Creating gaps and trading
- Tracking position and profit/loss

#### Full Program Test
```bash
make run              # Verbose mode - shows all details
make run-quiet        # Quiet mode - shows only essential info
```
Runs the complete program on real market data.

**Quiet mode shows only:**
- Day start/end messages
- Final position and P&L
- No batch details or order book snapshots

References: 
- https://github.com/Tzadiko/Orderbook
- learncpp.com
- cppreference.com
- https://www.youtube.com/watch?v=XeLWe0Cx_Lg
- https://www.borsaistanbul.com/files/bistech-itch-protocol-specification.pdf
- https://borsaistanbul.com/files/moldudp64-protocol-specification9890234EF41CE5A6B1C8A4A3.pdf
