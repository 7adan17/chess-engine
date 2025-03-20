CXX ?= c++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pthread
CORE = src/bitboard.cpp src/position.cpp src/eval.cpp src/search.cpp
HDRS = $(wildcard src/*.hpp)

all: build/engine build/test

build/engine: $(CORE) src/uci.cpp $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DNDEBUG $(CORE) src/uci.cpp -o $@

build/test: $(CORE) tests/test_engine.cpp $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CORE) tests/test_engine.cpp -o $@

test: build/test
	./build/test

clean:
	rm -rf build

.PHONY: all test clean
