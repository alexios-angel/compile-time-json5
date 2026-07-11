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

// parse the input into its document value; invalid JSON5 fails to compile
CTLL_EXPORT template <CTJSON5_STRING_INPUT input> constexpr auto parse() noexcept {
	static_assert(is_valid<input>, "ctjson5: the input is not valid JSON5");
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

} // namespace ctjson5

#endif
