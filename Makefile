CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -Isrc   # add headers in src and subdirs
TARGET = integration_main
TEST_TARGET = test_file
TEST_ORDERBOOK_TARGET = test_orderbook
TEST_PARSER_TARGET = test_parser
TEST_STRATEGY_TARGET = test_strategy
INTEGRATION_MAIN_TARGET = integration_main

# Find all .cpp files under src/ (excluding main.cpp which is now in integration tests)
SRC = $(shell find src -name '*.cpp' ! -name 'main.cpp')
OBJ = $(SRC:.cpp=.o)

# Test files
TEST_SRC = test/test.cpp
TEST_OBJ = test/test.o
TEST_ORDERBOOK_SRC = test/unit/test_orderbook.cpp
TEST_ORDERBOOK_OBJ = test/unit/test_orderbook.o
TEST_PARSER_SRC = test/unit/test_parser.cpp
TEST_PARSER_OBJ = test/unit/test_parser.o
TEST_STRATEGY_SRC = test/unit/test_strategy.cpp
TEST_STRATEGY_OBJ = test/unit/test_strategy.o
INTEGRATION_MAIN_SRC = test/integration/main.cpp
INTEGRATION_MAIN_OBJ = test/integration/main.o

all: $(TARGET)

$(TARGET): $(INTEGRATION_MAIN_OBJ) $(filter-out test/integration/main.o, $(OBJ))
	$(CXX) $(CXXFLAGS) -o $@ $^

# Test target
test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJ) $(filter-out src/main.o, $(OBJ))
	$(CXX) $(CXXFLAGS) -o $@ $^

# Test orderbook target
test-orderbook: $(TEST_ORDERBOOK_TARGET)

$(TEST_ORDERBOOK_TARGET): $(TEST_ORDERBOOK_OBJ) $(filter-out src/main.o, $(OBJ))
	$(CXX) $(CXXFLAGS) -o $@ $^

# Test parser target
test-parser: $(TEST_PARSER_TARGET)

$(TEST_PARSER_TARGET): $(TEST_PARSER_OBJ) $(filter-out test/integration/main.o, $(OBJ))
	$(CXX) $(CXXFLAGS) -o $@ $^

# Test strategy target
test-strategy: $(TEST_STRATEGY_TARGET)

$(TEST_STRATEGY_TARGET): $(TEST_STRATEGY_OBJ) $(filter-out test/integration/main.o, $(OBJ))
	$(CXX) $(CXXFLAGS) -o $@ $^

# Integration test target
integration: $(INTEGRATION_MAIN_TARGET)

$(INTEGRATION_MAIN_TARGET): $(INTEGRATION_MAIN_OBJ) $(filter-out test/integration/main.o, $(OBJ))
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generic rule: compile .cpp -> .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

run-quiet: $(TARGET)
	./$(TARGET) --quiet

run-test-orderbook: $(TEST_ORDERBOOK_TARGET)
	./$(TEST_ORDERBOOK_TARGET)

run-test-parser: $(TEST_PARSER_TARGET)
	./$(TEST_PARSER_TARGET)

run-test-strategy: $(TEST_STRATEGY_TARGET)
	./$(TEST_STRATEGY_TARGET)

run-integration: $(INTEGRATION_MAIN_TARGET)
	./$(INTEGRATION_MAIN_TARGET)

clean:
	rm -f $(OBJ) $(TARGET) $(TEST_OBJ) $(TEST_TARGET) $(TEST_ORDERBOOK_OBJ) $(TEST_ORDERBOOK_TARGET) $(TEST_PARSER_OBJ) $(TEST_PARSER_TARGET) $(TEST_STRATEGY_OBJ) $(TEST_STRATEGY_TARGET) $(INTEGRATION_MAIN_OBJ) $(INTEGRATION_MAIN_TARGET)

.PHONY: all clean run run-quiet test run-test test-orderbook run-test-orderbook test-parser run-test-parser test-strategy run-test-strategy integration run-integration
