// Validity checking and schema-style asserts.
//
// ctjson5::is_valid answers as a bool without failing the build, so it
// works in static_asserts both ways; kind and the typed accessors turn
// "does this document have the right shape" into compile-time checks.
//
// Build: make validation

#include <ctjson5.hpp>
#include <iostream>

using namespace std::string_view_literals;

// --- validity is just a bool

static_assert(ctjson5::is_valid<R"({"ok": true})">);
static_assert(ctjson5::is_valid<"[1, 2, 3]">);

// JSON5 is friendlier than JSON, but not lawless
static_assert(ctjson5::is_valid<"[1, 2,]">);        // trailing commas are json5
static_assert(ctjson5::is_valid<"{'k': 1}">);       // so are single quotes
static_assert(ctjson5::is_valid<"{k: 1}">);         // and unquoted keys
static_assert(!ctjson5::is_valid<"01">);            // leading zeros still are not
static_assert(!ctjson5::is_valid<"0x">);            // hex needs digits
static_assert(!ctjson5::is_valid<"[1] []">);        // one root value only
static_assert(!ctjson5::is_valid<"[1 /* open">);    // unterminated comment
static_assert(!ctjson5::is_valid<R"("\uD800")">);   // lone UTF-16 surrogate

// --- schema-style checks on a real document

constexpr auto user = ctjson5::parse<R"({
	"id":    1042,
	"email": "hana@example.com",
	"roles": ["admin", "dev"]
})">();

// the shape a "user" must have, spelled as static_asserts
static_assert(user.get<"id">().type == ctjson5::kind::number);
static_assert(user.get<"id">().is_integer());
static_assert(user.get<"email">().type == ctjson5::kind::string);
static_assert(user.get<"roles">().type == ctjson5::kind::array);
static_assert(user.get<"roles">().size() > 0);

int main() {
	std::cout << "user " << user.get<"id">().to<long>()
	          << " <" << user.get<"email">().view() << "> validated at compile time\n";
}
