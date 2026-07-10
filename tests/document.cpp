#include <ctjson5.hpp>

void empty_symbol() { }

// the string-literal API needs C++20 class-type template parameters;
// tests/cxx17.cpp covers the C++17 variable form
#if CTLL_CNTTP_COMPILER_CHECK

#include <string_view>
using namespace std::literals;

// --- validity
static_assert(ctjson5::is_valid<"{}">);
static_assert(ctjson5::is_valid<"[]">);
static_assert(ctjson5::is_valid<"  [1, 2, 3]  ">);
static_assert(ctjson5::is_valid<R"({"a": 1, "b": [true, false, null]})">);
static_assert(ctjson5::is_valid<"0">);
static_assert(ctjson5::is_valid<"-0.5e+10">);
static_assert(ctjson5::is_valid<R"("just a string")">);
static_assert(ctjson5::is_valid<"true">);

// still rejected in JSON5
static_assert(!ctjson5::is_valid<"">);
static_assert(!ctjson5::is_valid<"01">);           // leading zero
static_assert(!ctjson5::is_valid<"1e">);           // exponent needs digits
static_assert(!ctjson5::is_valid<"-">);
static_assert(!ctjson5::is_valid<".">);            // a number needs a digit
static_assert(!ctjson5::is_valid<"0x">);           // hex needs digits
static_assert(!ctjson5::is_valid<"{\"a\":}">);
static_assert(!ctjson5::is_valid<"{\"a\" 1}">);    // missing colon
static_assert(!ctjson5::is_valid<"\"unterminated">);
static_assert(!ctjson5::is_valid<"[1 2]">);
static_assert(!ctjson5::is_valid<"[,]">);          // a comma needs an element
static_assert(!ctjson5::is_valid<"tru">);
static_assert(!ctjson5::is_valid<"nulll">);
static_assert(!ctjson5::is_valid<"[]  x">);        // trailing garbage
static_assert(!ctjson5::is_valid<"\"\\1\"">);      // digits cannot be escaped
static_assert(!ctjson5::is_valid<"\"\\uD800\"">);  // lone high surrogate
static_assert(!ctjson5::is_valid<"\"\\uDC00\"">);  // lone low surrogate
static_assert(!ctjson5::is_valid<"[1] /* unterminated">);
static_assert(!ctjson5::is_valid<"[1] / [2]">);    // a lone slash is not a comment

// --- the JSON5 additions
// comments are whitespace
static_assert(ctjson5::is_valid<"// hi\n[1] // bye">);
static_assert(ctjson5::is_valid<"/* a */ [1, /* b */ 2]">);
static_assert(ctjson5::is_valid<"[1] // a comment may end at the input's end">);
static_assert(ctjson5::is_valid<"/* stars ** inside * */ 1">);

// unquoted and single-quoted keys, single-quoted strings, trailing commas
constexpr auto j5 = ctjson5::parse<R"({
	// configuration, the json5 way
	unquoted: 'single quoted',
	$dollar_2: "double",
	'sq key': [1, 2,],
})">();
static_assert(j5.get<"unquoted">() == "single quoted"sv);
static_assert(j5.get<"$dollar_2">() == "double"sv);
static_assert(j5.get<"sq key">().size() == 2);

// json5 numbers
static_assert(ctjson5::parse<"0xFF">().to<int>() == 255);
static_assert(ctjson5::parse<"-0x1f">().to<int>() == -31);
static_assert(ctjson5::parse<"0xFF">().is_integer());
static_assert(ctjson5::parse<".5">().to<double>() == 0.5);
static_assert(ctjson5::parse<"5.">().to<double>() == 5.0);
static_assert(ctjson5::parse<"+42">().to<int>() == 42);
static_assert(ctjson5::parse<"Infinity">().to<double>() > 1e308);
static_assert(ctjson5::parse<"-Infinity">().to<double>() < -1e308);
static_assert(ctjson5::parse<"NaN">().to<double>() != ctjson5::parse<"NaN">().to<double>());
static_assert(!ctjson5::parse<"NaN">().is_integer());

// json5 string escapes and continuations
static_assert(ctjson5::parse<R"('\'')">() == "'"sv);
static_assert(ctjson5::parse<R"("\v")">() == "\v"sv);
static_assert(ctjson5::parse<R"("a\0b")">() == std::string_view("a\0b", 3));
static_assert(ctjson5::parse<R"("\x41")">() == "A"sv);
static_assert(ctjson5::parse<R"("\xe9")">() == "\xc3\xa9"sv);  // é, utf-8 encoded
static_assert(ctjson5::parse<R"("\a\q")">() == "aq"sv);         // self-escapes
static_assert(ctjson5::parse<"\"a\\\nb\"">() == "ab"sv);        // continuation
static_assert(!ctjson5::is_valid<"\"raw\nnewline\"">);

// json5 whitespace: \v \f, BOM, NBSP, LS, PS
static_assert(ctjson5::is_valid<"\v\f[1]\v">);
static_assert(ctjson5::is_valid<"\xEF\xBB\xBF[1]">);
static_assert(ctjson5::is_valid<"\xC2\xA0[1]\xC2\xA0">);
static_assert(ctjson5::is_valid<"\xE2\x80\xA8[1]\xE2\x80\xA9">);

// serialization keeps json5 spellings
static_assert(ctjson5::serialize(ctjson5::parse<"[0xFF, .5, Infinity, NaN,]">()) == "[0xFF,.5,Infinity,NaN]"sv);

// --- document access
constexpr auto doc = ctjson5::parse<R"({
	"name": "Hana",
	"stars": 4700,
	"ratio": -2.5e-1,
	"tags": ["regex", "compile-time"],
	"active": true,
	"parent": null,
	"nested": {"deep": [{"x": 42}]}
})">();

static_assert(doc.size() == 7);
static_assert(doc.contains<"name">());
static_assert(!doc.contains<"missing">());
static_assert(doc.get<"name">() == "Hana"sv);
static_assert(doc.get<"stars">().to<int>() == 4700);
static_assert(doc.get<"stars">().is_integer());
static_assert(doc.get<"ratio">().to<double>() == -0.25);
static_assert(!doc.get<"ratio">().is_integer());
static_assert(doc.get<"tags">().size() == 2);
static_assert(doc.get<"tags">().get<0>() == "regex"sv);
static_assert(doc.get<"tags">().get<1>() == "compile-time"sv);
static_assert(doc.get<"active">());
static_assert(doc.get<"parent">().type == ctjson5::kind::null);
static_assert(doc.get<"nested">().get<"deep">().get<0>().get<"x">().to<int>() == 42);

// positional member access
static_assert(doc.key<0>() == "name"sv);
static_assert(doc.value<1>().to<int>() == 4700);

// --- numbers
static_assert(ctjson5::parse<"0">().to<int>() == 0);
static_assert(ctjson5::parse<"-0">().to<int>() == 0);
static_assert(ctjson5::parse<"1e3">().to<int>() == 1000);
static_assert(ctjson5::parse<"1.5">().to<double>() == 1.5);
static_assert(ctjson5::parse<"12.34e2">().to<double>() == 1234.0);
static_assert(ctjson5::parse<"1E+2">().to<int>() == 100);
static_assert(ctjson5::parse<"-17">().to<long long>() == -17);
static_assert(ctjson5::parse<"1.75">().to<int>() == 1); // truncates, like a cast

// --- strings and escapes
static_assert(ctjson5::parse<R"("")">().empty());
static_assert(ctjson5::parse<R"("a\nb")">() == "a\nb"sv);
static_assert(ctjson5::parse<R"("q\"q")">() == "q\"q"sv);
static_assert(ctjson5::parse<R"("back\\slash")">() == "back\\slash"sv);
static_assert(ctjson5::parse<R"("sol\/idus")">() == "sol/idus"sv);
static_assert(ctjson5::parse<R"("\t\r\b\f")">() == "\t\r\b\f"sv);
static_assert(ctjson5::parse<R"("A")">() == "A"sv);
static_assert(ctjson5::parse<R"("é")">() == "\xc3\xa9"sv);          // é as utf-8
static_assert(ctjson5::parse<R"("€")">() == "\xe2\x82\xac"sv);      // €
static_assert(ctjson5::parse<R"("😀")">() == "\xf0\x9f\x98\x80"sv); // 😀 via surrogate pair
static_assert(ctjson5::parse<R"("café")">().size() == 5);           // bytes, not code points

// --- structures
static_assert(ctjson5::parse<"[]">().empty());
static_assert(ctjson5::parse<"{}">().empty());
static_assert(ctjson5::parse<"[[1],[2,3]]">().get<1>().get<0>().to<int>() == 2);
static_assert(ctjson5::parse<"[true,false,null]">().size() == 3);

// duplicate keys are legal per RFC; get finds the first
static_assert(ctjson5::parse<R"({"k":1,"k":2})">().get<"k">().to<int>() == 1);

// --- serialization: any document renders back to minified JSON
static_assert(ctjson5::serialize(ctjson5::parse<R"({ "a" : [ 1, 2 ] , "b" : null })">()) == R"({"a":[1,2],"b":null})"sv);
static_assert(ctjson5::serialize(ctjson5::parse<"[]">()) == "[]"sv);
static_assert(ctjson5::serialize(ctjson5::parse<"{}">()) == "{}"sv);
static_assert(ctjson5::serialize(ctjson5::parse<"-2.5e-1">()) == "-2.5e-1"sv); // raw spelling kept
static_assert(ctjson5::serialize(ctjson5::parse<"[true,false]">()) == "[true,false]"sv);
// escapes are re-emitted: named ones by name, other controls as \u00XX
static_assert(ctjson5::serialize(ctjson5::parse<R"("a\nb\"q\"")">()) == R"("a\nb\"q\"")"sv);
// multi-byte utf-8 passes through
static_assert(ctjson5::serialize(ctjson5::parse<R"(["caf\u00e9"])">()) == "[\"caf\xc3\xa9\"]"sv);
// round-trip: parse(serialize(x)) is the same type as x
constexpr auto rt = ctjson5::parse<R"({"n":[1,{"d":true}]})">();
static_assert(std::is_same_v<decltype(ctjson5::parse<ctll::fixed_string{R"({"n":[1,{"d":true}]})"}>()), std::remove_const_t<decltype(rt)>>);

// --- iteration
static_assert([] {
	long long sum = 0;
	ctjson5::for_each(ctjson5::parse<"[1,2,3,4]">(), [&](auto v) { sum += v.template to<long long>(); });
	return sum;
}() == 10);
static_assert([] {
	size_t keys = 0, numbers = 0;
	ctjson5::for_each(ctjson5::parse<R"({"a":1,"b":"x","c":2})">(), [&](auto key, auto value) {
		keys += key.size();
		if constexpr (decltype(value)::type == ctjson5::kind::number) {
			++numbers;
		}
	});
	return keys * 10 + numbers;
}() == 32);

#endif
