.PHONY: default all clean grammar regrammar single-header single-header/ctjson.hpp

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

grammar: include/ctjson/json.hpp

regrammar:
	@rm -f include/ctjson/json.hpp
	@$(MAKE) grammar

include/ctjson/json.hpp: include/ctjson/json.gram
	@echo "LL1q $<"
	@$(TABLEWRIGHT) --ll --q --input=include/ctjson/json.gram --output=include/ctjson/ --generator=cpp_ctll_v2 --cfg:fname=json.hpp --cfg:namespace=ctjson --cfg:guard=CTJSON__JSON__HPP --cfg:grammar_name=json

# needs python3 with the quom package
single-header: single-header/ctjson.hpp

single-header/ctjson.hpp:
	$(PYTHON) -m quom include/ctjson.hpp ctjson.hpp.tmp
	echo "/*" > single-header/ctjson.hpp
	cat LICENSE >> single-header/ctjson.hpp
	echo "*/" >> single-header/ctjson.hpp
	cat ctjson.hpp.tmp >> single-header/ctjson.hpp
	rm ctjson.hpp.tmp
