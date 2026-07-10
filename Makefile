.PHONY: default all clean grammar regrammar single-header single-header/ctjson5.hpp

default: all

CXX_STANDARD := 20

PYTHON := python3

# LL1q parser generator: https://github.com/alexios-angel/Tablewright
# (needs python3 with the lark package)
TABLEWRIGHT := tablewright

override CXXFLAGS := $(CXXFLAGS) -std=c++$(CXX_STANDARD) -Iinclude -O3 -pedantic -Wall -Wextra -Werror -Wconversion

TESTS := $(wildcard tests/*.cpp)
OBJECTS := $(TESTS:%.cpp=%.o)
DEPENDENCY_FILES := $(OBJECTS:%.o=%.d)

all: $(OBJECTS)

$(OBJECTS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

-include $(DEPENDENCY_FILES)

clean:
	rm -f $(OBJECTS) $(DEPENDENCY_FILES)

grammar: include/ctjson5/json5.hpp

regrammar:
	@rm -f include/ctjson5/json5.hpp
	@$(MAKE) grammar

include/ctjson5/json5.hpp: include/ctjson5/json5.gram
	@echo "LL1q $<"
	@$(TABLEWRIGHT) --ll --q --input=include/ctjson5/json5.gram --output=include/ctjson5/ --generator=cpp_ctll_v2 --cfg:fname=json5.hpp --cfg:namespace=ctjson5 --cfg:guard=CTJSON5__JSON5__HPP --cfg:grammar_name=json5

# needs python3 with the quom package
single-header: single-header/ctjson5.hpp

single-header/ctjson5.hpp:
	$(PYTHON) -m quom include/ctjson5.hpp ctjson5.hpp.tmp
	echo "/*" > single-header/ctjson5.hpp
	cat LICENSE >> single-header/ctjson5.hpp
	echo "*/" >> single-header/ctjson5.hpp
	cat ctjson5.hpp.tmp >> single-header/ctjson5.hpp
	rm ctjson5.hpp.tmp
