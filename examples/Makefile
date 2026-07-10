CXX ?= c++
CXXFLAGS := -std=c++20 -I../include -O2 -Wall -Wextra -pedantic

SOURCES := $(wildcard *.cpp)
TARGETS := $(SOURCES:%.cpp=%)

all: $(TARGETS)

# built in C++17 mode on purpose, to demonstrate the C++17 API
cxx17-syntax: cxx17-syntax.cpp
	$(CXX) -std=c++17 -I../include -O2 -Wall -Wextra $< -o $@

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	@for t in $(TARGETS); do printf '== %s ==\n' "$$t"; ./$$t; echo; done

clean:
	rm -f $(TARGETS)

.PHONY: all run clean
