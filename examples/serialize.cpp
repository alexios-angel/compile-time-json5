// Serialization: any document renders back to minified JSON at compile
// time, in null-terminated static storage. Whitespace disappears,
// \uXXXX escapes come out decoded (as UTF-8) and mandatory escapes are
// re-emitted - so serialize(parse(x)) is a canonical form of x.
//
// Build: make serialize

#include <ctjson.hpp>
#include <iostream>

// messy input: spread over lines, indented, with redundant \u escapes
constexpr auto doc = ctjson::parse<R"({
	"name"   : "café",
	"tabs"   : "a\tb",
	"values" : [ 1 , 2.50 , -3e2 ],
	"nested" : { "ok" : true }
})">();

// one canonical line comes out
constexpr auto canonical = ctjson::serialize(doc);
static_assert(canonical == "{\"name\":\"caf\xc3\xa9\",\"tabs\":\"a\\tb\",\"values\":[1,2.50,-3e2],\"nested\":{\"ok\":true}}");

// numbers keep the spelling they were parsed with (2.50 stays 2.50)
static_assert(ctjson::serialize(ctjson::parse<"[ 2.50 ]">()) == "[2.50]");

// serialization is a fixed point: re-parsing the canonical form and
// serializing again changes nothing
static constexpr ctll::fixed_string canonical_text{"{\"name\":\"caf\xc3\xa9\",\"tabs\":\"a\\tb\",\"values\":[1,2.50,-3e2],\"nested\":{\"ok\":true}}"};
static_assert(ctjson::serialize(ctjson::parse<canonical_text>()) == canonical);

int main() {
	// the canonical form is a plain null-terminated string in the binary
	std::cout << "canonical: " << canonical << "\n";
	std::cout << "length:    " << canonical.size() << " bytes (source was "
	          << sizeof(R"({
	"name"   : "café",
	"tabs"   : "a\tb",
	"values" : [ 1 , 2.50 , -3e2 ],
	"nested" : { "ok" : true }
})") - 1 << ")\n";
}
