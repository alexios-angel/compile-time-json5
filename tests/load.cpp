// runtime tests for the runtime parser (json.load / json.loads style)

#include <ctjson5.hpp>
#undef NDEBUG // the assertions ARE the test; keep them in release builds
#include <cassert>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

using namespace std::string_view_literals;

int main() {
	// scalars
	assert(ctjson5::loads("42")->to<int>() == 42);
	assert(ctjson5::loads("42")->is_integer());
	assert(ctjson5::loads("2.5")->to<double>() == 2.5);
	assert(!ctjson5::loads("2.5")->is_integer());
	assert(ctjson5::loads("true")->boolean());
	assert(ctjson5::loads("null")->is_null());
	assert(ctjson5::loads(R"("text")")->view() == "text");

	// structures and chained lookup
	{
		const auto doc = ctjson5::loads(R"({
			"users": [{"name": "Hana", "stars": 4700}],
			"active": true
		})");
		assert(doc.has_value());
		assert(doc->is_object());
		assert((*doc)["users"][0]["name"].view() == "Hana");
		assert((*doc)["users"][0]["stars"].to<long>() == 4700);
		assert(doc->get("active").boolean());
		assert(doc->contains("users"));
		assert(!doc->contains("missing"));
		// the null-object pattern: absent paths chain safely
		assert((*doc)["missing"]["deep"][7].is_null());
		assert(doc->find("missing") == nullptr);
	}

	// iteration over the underlying containers
	{
		const auto doc = ctjson5::loads("[1, 2, 3]");
		long sum = 0;
		for (const auto & element : doc->as_array()) {
			sum += element.to<long>();
		}
		assert(sum == 6);
	}

	// escapes decode exactly like the compile-time parser
	assert(ctjson5::loads(R"("a\nb")")->view() == "a\nb");
	assert(ctjson5::loads(R"("A")")->view() == "A");
	assert(ctjson5::loads(R"("😀")")->view() == "\xf0\x9f\x98\x80"); // 😀
	assert(!ctjson5::loads(R"("\ud800")").has_value()); // lone surrogate

	// numbers: int/float split, overflow behaviours
	assert(ctjson5::loads("9223372036854775807")->is_integer());
	assert(!ctjson5::loads("9223372036854775808")->is_integer()); // beyond long long: double
	assert(ctjson5::loads("1e999")->to<double>() > 0 && std::isinf(ctjson5::loads("1e999")->to<double>()));
	assert(std::isnan(ctjson5::loads("NaN")->to<double>()));      // Python's parse_constant
	assert(std::isinf(ctjson5::loads("-Infinity")->to<double>()));

	// the JSON5 additions at runtime
	assert(ctjson5::loads("// c\n[1, /* c */ 2,]")->size() == 2);
	assert((*ctjson5::loads("{unquoted: 1, 'sq': 2,}"))["unquoted"].to<int>() == 1);
	assert((*ctjson5::loads("{unquoted: 1, 'sq': 2,}"))["sq"].to<int>() == 2);
	assert(ctjson5::loads("'single'")->view() == "single");
	assert(ctjson5::loads("0xFF")->to<int>() == 255);
	assert(ctjson5::loads("0xFF")->is_integer());
	assert(ctjson5::loads("-0x1f")->to<int>() == -31);
	assert(ctjson5::loads(".5")->to<double>() == 0.5);
	assert(ctjson5::loads("5.")->to<double>() == 5.0);
	assert(ctjson5::loads("+42")->to<int>() == 42);
	assert(std::isinf(ctjson5::loads("+Infinity")->to<double>()));
	assert(ctjson5::loads(R"("\x41")")->view() == "A");
	assert(ctjson5::loads(R"('\'')")->view() == "'");
	assert(ctjson5::loads(R"("\a\q")")->view() == "aq");
	assert(ctjson5::loads("\"a\\\nb\"")->view() == "ab");        // continuation LF
	assert(ctjson5::loads("\"a\\\r\nb\"")->view() == "ab");      // continuation CRLF
	assert(ctjson5::loads(std::string_view("\"a\0b\"", 5))->view() == std::string_view("a\0b", 3)); // raw NUL is legal json5
	assert(ctjson5::loads("\v\f[1]\v").has_value());
	assert(ctjson5::loads("\xEF\xBB\xBF[1]").has_value());
	assert(ctjson5::loads("\xC2\xA0[1]\xC2\xA0").has_value());
	assert(ctjson5::loads("\xE2\x80\xA8[1]\xE2\x80\xA9").has_value());

	// strictness that remains
	assert(!ctjson5::loads("").has_value());
	assert(!ctjson5::loads("01").has_value());
	assert(!ctjson5::loads("0x").has_value());
	assert(!ctjson5::loads(".").has_value());
	assert(!ctjson5::loads("1e").has_value());
	assert(!ctjson5::loads("[,]").has_value());
	assert(!ctjson5::loads("[1] []").has_value());
	assert(!ctjson5::loads("\"unterminated").has_value());
	assert(!ctjson5::loads("tru").has_value());
	assert(!ctjson5::loads("[1] /* unterminated").has_value());
	assert(!ctjson5::loads("[1] / [2]").has_value());
	assert(!ctjson5::loads(R"("\1")").has_value());
	assert(!ctjson5::loads("\"raw\nnewline\"").has_value());

	// error reporting carries the byte offset, line and column
	{
		ctjson5::load_error error;
		assert(!ctjson5::loads("[1, 2, }", error).has_value());
		assert(error.position == 7);
		assert(error.message[0] != '\0');
		assert(error.line == 1 && error.column == 8);
	}
	{
		ctjson5::load_error error;
		assert(!ctjson5::loads("[1,\n 2, }", error).has_value());
		assert(error.line == 2 && error.column == 5);
		const std::string rendered = ctjson5::to_string(error);
		assert(rendered.rfind("line 2, column 5: ", 0) == 0);
		assert(rendered.size() > sizeof("line 2, column 5: "));
	}

	// deep nesting is rejected, not a stack overflow
	{
		std::string bomb(100000, '[');
		assert(!ctjson5::loads(bomb).has_value());
	}

	// equality is structural
	assert(*ctjson5::loads("[1, {\"a\": null}]") == *ctjson5::loads("[ 1 , { \"a\" : null } ]"));
	assert(*ctjson5::loads("[1]") != *ctjson5::loads("[2]"));

	// the encoder accepts runtime documents, nested in native containers too
	{
		const auto doc = ctjson5::loads(R"({"n": [1, 2.5], "s": "x"})");
		assert(ctjson5::dumps(*doc) == R"({"n": [1, 2.5], "s": "x"})");
		assert(ctjson5::dumps(*doc, 2) == "{\n  \"n\": [\n    1,\n    2.5\n  ],\n  \"s\": \"x\"\n}");
		std::vector<ctjson5::value> wrapped{*doc};
		assert(ctjson5::dumps(wrapped) == R"([{"n": [1, 2.5], "s": "x"}])");
	}

	// like Python, runtime numbers are values, not spellings
	assert(ctjson5::dumps(*ctjson5::loads("2.50")) == "2.5");
	assert(ctjson5::dumps(*ctjson5::loads("1e2")) == "100.0");

	// json.load: from a stream
	{
		std::istringstream stream(R"({"from": "stream"})");
		const auto doc = ctjson5::load(stream);
		assert(doc.has_value());
		assert((*doc)["from"].view() == "stream");
	}

#if CTLL_CNTTP_COMPILER_CHECK
	// the runtime and compile-time parsers agree, judged by the encoder
	#define AGREE(text) assert(ctjson5::dumps(*ctjson5::loads(text)) == ctjson5::dumps(ctjson5::parse<text>()))
	AGREE(R"({"a": [1, 2.5, true, null], "b": {"c": "d\ne"}})");
	AGREE(R"([[], {}, "", 0, -17])");
	AGREE(R"("café 😀")");
	// json5 syntax agrees when the number spellings are already
	// canonical (runtime numbers are values, compile-time ones keep
	// their spelling, so ".5" and "+2" legitimately dump differently)
	AGREE(R"({unquoted: 'single', "b": [1, 2,], /* comment */})");
	#undef AGREE
	assert(ctjson5::loads(".5")->to<double>() == ctjson5::parse<".5">().to<double>());
	assert(ctjson5::loads("+2")->to<int>() == ctjson5::parse<"+2">().to<int>());
	assert(ctjson5::loads("0xFF")->to<int>() == ctjson5::parse<"0xFF">().to<int>());

	// and they reject the same inputs
	#define BOTH_REJECT(text) do { static_assert(!ctjson5::is_valid<text>); assert(!ctjson5::loads(text).has_value()); } while (false)
	BOTH_REJECT("[1,,2]");
	BOTH_REJECT(R"({"a" 1})");
	BOTH_REJECT("00");
	BOTH_REJECT("1e");
	BOTH_REJECT("0x");
	BOTH_REJECT(R"("\3")");
	BOTH_REJECT("[1] /* open");
	#undef BOTH_REJECT
#endif

	std::puts("all load tests passed");
	return 0;
}
