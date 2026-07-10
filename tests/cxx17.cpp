#include <ctjson.hpp>
#include <string_view>
using namespace std::literals;

// C++17: inputs and keys are fixed_string variables with linkage
static constexpr auto doc_text = ctll::fixed_string{R"({"name":"Hana","n":42})"};
static constexpr auto bad_text = ctll::fixed_string{"{oops"};
static constexpr ctll::fixed_string name_key = "name";
static constexpr ctll::fixed_string n_key = "n";

static_assert(ctjson::is_valid<doc_text>);
static_assert(!ctjson::is_valid<bad_text>);

constexpr auto doc = ctjson::parse<doc_text>();
static_assert(doc.template get<name_key>() == "Hana"sv);
static_assert(doc.template get<n_key>().template to<int>() == 42);

void empty_symbol() { }
