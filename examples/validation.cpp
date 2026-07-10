// Validity checking and schema-style asserts.
//
// ctjson::is_valid answers as a bool without failing the build, so it
// works in static_asserts both ways; kind and the typed accessors turn
// "does this document have the right shape" into compile-time checks.
//
// Build: make validation

#include <ctjson.hpp>
#include <iostream>

using namespace std::string_view_literals;

// --- validity is just a bool

static_assert(ctjson::is_valid<R"({"ok": true})">);
static_assert(ctjson::is_valid<"[1, 2, 3]">);

// strict RFC 8259: all of these are invalid
static_assert(!ctjson::is_valid<"[1, 2,]">);       // trailing comma
static_assert(!ctjson::is_valid<"{'k': 1}">);      // single quotes
static_assert(!ctjson::is_valid<"{k: 1}">);        // unquoted key
static_assert(!ctjson::is_valid<"01">);            // leading zero
static_assert(!ctjson::is_valid<"1.">);            // dot needs digits
static_assert(!ctjson::is_valid<"[1] []">);        // one root value only
static_assert(!ctjson::is_valid<R"("\uD800")">);   // lone UTF-16 surrogate

// --- schema-style checks on a real document

constexpr auto user = ctjson::parse<R"({
	"id":    1042,
	"email": "hana@example.com",
	"roles": ["admin", "dev"]
})">();

// the shape a "user" must have, spelled as static_asserts
static_assert(user.get<"id">().type == ctjson::kind::number);
static_assert(user.get<"id">().is_integer());
static_assert(user.get<"email">().type == ctjson::kind::string);
static_assert(user.get<"roles">().type == ctjson::kind::array);
static_assert(user.get<"roles">().size() > 0);

int main() {
	std::cout << "user " << user.get<"id">().to<long>()
	          << " <" << user.get<"email">().view() << "> validated at compile time\n";
}
