// Using ctjson5 with C++17 (no class-type template parameters): the
// input and the keys are constexpr ctll::fixed_string variables with
// linkage instead of string literals, and dependent calls need the
// `template` keyword. Everything else works the same.
//
// Build: make cxx17-syntax   (compiled with -std=c++17)

#include <ctjson5.hpp>
#include <iostream>

static constexpr auto text = ctll::fixed_string{R"({
	"project": "ctjson5",
	"port":    8080,
	"tags":    ["json", "constexpr"]
})"};

static constexpr ctll::fixed_string project_key = "project";
static constexpr ctll::fixed_string port_key = "port";
static constexpr ctll::fixed_string tags_key = "tags";

static_assert(ctjson5::is_valid<text>);

constexpr auto doc = ctjson5::parse<text>();

static_assert(doc.template get<project_key>() == std::string_view{"ctjson5"});
static_assert(doc.template get<port_key>().template to<int>() == 8080);
static_assert(doc.template get<tags_key>().template get<1>() == std::string_view{"constexpr"});

int main() {
	std::cout << "project: " << doc.template get<project_key>().view() << "\n";
	std::cout << "port:    " << doc.template get<port_key>().template to<int>() << "\n";
	std::cout << "tags:    " << doc.template get<tags_key>().size() << "\n";
#if CTLL_CNTTP_COMPILER_CHECK
	std::cout << "(compiled with C++20: string literals would work too)\n";
#else
	std::cout << "(compiled with C++17)\n";
#endif
}
