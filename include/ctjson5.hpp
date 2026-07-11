#ifndef CTJSON5__HPP
#define CTJSON5__HPP

#include "ctlark.hpp"
#include "ctjson5/grammar.hpp"
#include "ctjson5/types.hpp"
#include "ctjson5/bind.hpp"
#include "ctjson5/serialize.hpp"
#include "ctjson5/views.hpp"
#include "ctjson5/dumps.hpp"
#include "ctjson5/load.hpp"

// ctjson5: compile-time JSON5.
//
//   constexpr auto doc = ctjson5::parse<R"({name: 'Hana', tags: [1, 2, 3,]})">();
//   static_assert(doc.get<"name">() == "Hana");
//   static_assert(doc.get<"tags">().get<1>().to<int>() == 2);
//   static_assert(ctjson5::is_valid<"[0xFF, .5, Infinity, /*ok*/]">);
//
// The document is parsed while your code compiles - malformed JSON5 is
// a compile error (or `false` from is_valid) - and the result is a
// TYPE whose accessors are all constexpr. The grammar layer is ctlark
// (compile-time Lark): the JSON5 grammar is a lark grammar string
// (grammar.hpp), parsed and compiled to tables at compile time, and
// the document goes through ctlark's contextual-lexing constexpr
// Earley parser before bind.hpp lowers the tree into the document
// types.

namespace ctjson5 {

#if CTLL_CNTTP_COMPILER_CHECK
#define CTJSON5_STRING_INPUT ctll::fixed_string
#else
// C++17: pass a constexpr ctll::fixed_string variable with linkage
#define CTJSON5_STRING_INPUT const auto &
#endif

namespace detail {

// grammar validity is a given (static_assert in grammar.hpp); input
// validity is the parse plus the binder's surrogate checks
template <CTJSON5_STRING_INPUT input> constexpr bool valid_document() noexcept {
	if constexpr (!ctlark::is_valid<json5_grammar, input, json5_start>) {
		return false;
	} else {
		return bind<decltype(ctlark::parse<json5_grammar, input, json5_start>())>::ok;
	}
}

} // namespace detail

// does the input parse as JSON5?
CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr bool is_valid =
	detail::valid_document<input>();

// what failed and where, when it does not: kind, byte offset, line,
// column and the expected terminals (kind none = the syntax is fine)
CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr ctlark::error_info_t error_info() noexcept {
	return ctlark::error_info<detail::json5_grammar, input, detail::json5_start>();
}

// the rendered diagnostic - location, snippet with a caret, expected
// terminals - as a static string ("" when the syntax is fine)
CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr std::string_view error_message() noexcept {
	return ctlark::error_message<detail::json5_grammar, input, detail::json5_start>();
}

// why the binder rejected a document that PARSES (the \u surrogate
// rules a grammar cannot express); reason none when the document is
// valid or when the syntax already failed
CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr bind_error_t bind_error() noexcept {
	if constexpr (!ctlark::is_valid<detail::json5_grammar, input, detail::json5_start>) {
		return bind_error_t{};
	} else {
		return detail::bind<decltype(ctlark::parse<detail::json5_grammar, input, detail::json5_start>())>::fail;
	}
}

// parse the input into its document value; invalid JSON5 fails to compile
CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr auto parse() noexcept {
#ifdef CTLARK_VERBOSE_ERRORS
	(void)ctlark::verbose_report<detail::json5_grammar, input, detail::json5_start>();
#endif
	static_assert(ctlark::is_valid<detail::json5_grammar, input, detail::json5_start>,
	              "ctjson5: the input is not valid JSON5 - print ctjson5::error_message<input>() "
	              "for the location and the expected tokens");
	static_assert(!ctlark::is_valid<detail::json5_grammar, input, detail::json5_start> || is_valid<input>,
	              "ctjson5: the input parses but breaks JSON5's \\u surrogate rules - print "
	              "ctjson5::bind_error<input>() for the offending string");
	if constexpr (is_valid<input>) {
		using bound = detail::bind<decltype(ctlark::parse<detail::json5_grammar, input, detail::json5_start>())>;
		return typename bound::type{};
	} else {
		return null{};
	}
}

// like json.loads, for symmetry with dumps: parse compile-time text
#if CTLL_CNTTP_COMPILER_CHECK
CTLL_EXPORT template <ctll::fixed_string input> constexpr auto loads() noexcept {
	return parse<input>();
}
#else
CTLL_EXPORT template <const auto & input> constexpr auto loads() noexcept {
	return parse<input>();
}
#endif

// the ctlark debugging toolbox with the JSON5 grammar baked in: traced
// parses (also runnable at runtime under a debugger), runtime inputs
// against the compile-time tables, token and grammar dumps
namespace debug {

CTLL_EXPORT template <CTJSON5_STRING_INPUT input, size_t Cap = 4096> constexpr auto traced_parse() noexcept {
	return ctlark::debug::traced_parse<detail::json5_grammar, input, detail::json5_start, Cap>();
}

CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr std::string_view dump_tokens() noexcept {
	return ctlark::debug::dump_tokens<detail::json5_grammar, input, detail::json5_start>();
}

CTLL_EXPORT constexpr std::string_view dump_grammar() noexcept {
	return ctlark::debug::dump_grammar<detail::json5_grammar>();
}

CTLL_EXPORT template <size_t MaxTokens = 1024>
ctlark::debug::runtime_result parse_runtime(std::string_view in) {
	return ctlark::debug::parse_runtime<detail::json5_grammar, MaxTokens>(in, "value");
}

} // namespace debug

} // namespace ctjson5

#endif
