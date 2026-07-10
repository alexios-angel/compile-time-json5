// The classic use: a configuration baked into the binary at compile
// time. There is nothing to parse, load or validate at runtime - a typo
// in the JSON or a missing key is a build failure, and every lookup
// below compiles down to a constant.
//
// Build: make config

#include <ctjson.hpp>
#include <iostream>

constexpr auto config = ctjson::parse<R"({
	"app": {
		"name":    "demo",
		"workers": 8,
		"timeout": 2.5
	},
	"features": ["search", "export"],
	"debug":    false
})">();

// requirements checked at build time
static_assert(config.contains<"app">());
static_assert(config.get<"app">().get<"workers">().to<int>() >= 1);
static_assert(config.get<"features">().size() >= 1);

// values usable as template arguments / array sizes
int worker_slots[config.get<"app">().get<"workers">().to<int>()];

int main() {
	std::cout << "name:     " << config.get<"app">().get<"name">().view() << "\n";
	std::cout << "workers:  " << config.get<"app">().get<"workers">().to<int>()
	          << " (slots: " << sizeof(worker_slots) / sizeof(int) << ")\n";
	std::cout << "timeout:  " << config.get<"app">().get<"timeout">().to<double>() << "s\n";
	std::cout << "debug:    " << (config.get<"debug">() ? "on" : "off") << "\n";

	std::cout << "features:";
	std::cout << " " << config.get<"features">().get<0>().view();
	std::cout << " " << config.get<"features">().get<1>().view();
	std::cout << "\n";
}
