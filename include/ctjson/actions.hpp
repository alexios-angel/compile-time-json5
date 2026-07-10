#ifndef CTJSON__ACTIONS__HPP
#define CTJSON__ACTIONS__HPP

#include "json.hpp"
#include "types.hpp"
#include "../ctll/list.hpp"
#include "../ctll/grammars.hpp"

// Semantic actions building the document on a type stack while CTLL
// parses (the same architecture as CTRE's pcre_actions): strings and
// numbers accumulate character by character, keys and members pair up as
// they complete, and closing an object or array collects everything back
// to its begin marker. Returning ctll::reject{} from an action turns the
// parse into a syntax error - used for lone UTF-16 surrogates.

namespace ctjson {

// the parser subject: just a stack of partial results
template <typename Stack = ctll::list<>> struct context {
	using stack_type = Stack;
	static constexpr inline auto stack = stack_type();
	constexpr context() noexcept { }
	constexpr context(Stack) noexcept { }
};

template <typename... Content> context(ctll::list<Content...>) -> context<ctll::list<Content...>>;

// parse-time markers
struct object_begin_marker { };
struct array_begin_marker { };
template <typename String> struct key_marker { };
template <size_t Value> struct hex_marker { };
template <size_t High> struct pending_surrogate { };

// append one code point to a string as UTF-8 bytes
template <char32_t CodePoint, auto... Cs> constexpr auto append_code_point(string<Cs...>) noexcept {
	constexpr auto cp = static_cast<size_t>(CodePoint);
	if constexpr (cp < 0x80) {
		return string<Cs..., static_cast<char32_t>(cp)>{};
	} else if constexpr (cp < 0x800) {
		return string<Cs..., static_cast<char32_t>(0xC0 | (cp >> 6)), static_cast<char32_t>(0x80 | (cp & 0x3F))>{};
	} else if constexpr (cp < 0x10000) {
		return string<Cs..., static_cast<char32_t>(0xE0 | (cp >> 12)), static_cast<char32_t>(0x80 | ((cp >> 6) & 0x3F)), static_cast<char32_t>(0x80 | (cp & 0x3F))>{};
	} else {
		return string<Cs..., static_cast<char32_t>(0xF0 | (cp >> 18)), static_cast<char32_t>(0x80 | ((cp >> 12) & 0x3F)), static_cast<char32_t>(0x80 | ((cp >> 6) & 0x3F)), static_cast<char32_t>(0x80 | (cp & 0x3F))>{};
	}
}

struct json_actions {

	// --- objects and arrays open with a marker

	template <auto V, typename... Ts> static constexpr auto apply(json::begin_object, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<object_begin_marker, Ts...>>{};
	}

	template <auto V, typename... Ts> static constexpr auto apply(json::begin_array, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<array_begin_marker, Ts...>>{};
	}

	// --- strings accumulate onto a fresh string<>

	template <auto V, typename... Ts> static constexpr auto apply(json::begin_string, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<string<>, Ts...>>{};
	}

	template <auto V, auto... Cs, typename... Ts> static constexpr auto apply(json::push_string_char, ctll::term<V>, context<ctll::list<string<Cs...>, Ts...>>) {
		if constexpr (static_cast<size_t>(V) > 0xFF) {
			// the input was decoded to code points: encode back to UTF-8
			return context{ctll::list<decltype(append_code_point<static_cast<char32_t>(V)>(string<Cs...>{})), Ts...>{}};
		} else {
			// byte-oriented input passes through unchanged
			return context<ctll::list<string<Cs..., static_cast<char32_t>(V)>, Ts...>>{};
		}
	}

	// a lone high surrogate followed by a regular character is invalid
	template <auto V, size_t High, typename... Ts> static constexpr auto apply(json::push_string_char, ctll::term<V>, context<ctll::list<pending_surrogate<High>, Ts...>>) {
		return ctll::reject{};
	}

	template <auto V, auto... Cs, typename... Ts> static constexpr auto apply(json::push_escaped_char, ctll::term<V>, context<ctll::list<string<Cs...>, Ts...>>) {
		constexpr char32_t decoded = [] {
			switch (static_cast<char>(V)) {
				case 'b': return char32_t{0x08};
				case 'f': return char32_t{0x0C};
				case 'n': return char32_t{0x0A};
				case 'r': return char32_t{0x0D};
				case 't': return char32_t{0x09};
				default: return static_cast<char32_t>(V); // " \ /
			}
		}();
		return context<ctll::list<string<Cs..., decoded>, Ts...>>{};
	}

	template <auto V, size_t High, typename... Ts> static constexpr auto apply(json::push_escaped_char, ctll::term<V>, context<ctll::list<pending_surrogate<High>, Ts...>>) {
		return ctll::reject{};
	}

	// --- \uXXXX escapes: accumulate four hex digits, then append the
	// code point (combining UTF-16 surrogate pairs)

	template <auto V, typename... Ts> static constexpr auto apply(json::begin_hex, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<hex_marker<0>, Ts...>>{};
	}

	template <auto V, size_t Value, typename... Ts> static constexpr auto apply(json::push_hex, ctll::term<V>, context<ctll::list<hex_marker<Value>, Ts...>>) {
		constexpr size_t digit = [] {
			if constexpr (V >= '0' && V <= '9') {
				return static_cast<size_t>(V - '0');
			} else if constexpr (V >= 'a' && V <= 'f') {
				return static_cast<size_t>(V - 'a' + 10);
			} else {
				return static_cast<size_t>(V - 'A' + 10);
			}
		}();
		return context<ctll::list<hex_marker<Value * 16 + digit>, Ts...>>{};
	}

	template <auto V, size_t CodePoint, auto... Cs, typename... Ts> static constexpr auto apply(json::end_hex, ctll::term<V>, context<ctll::list<hex_marker<CodePoint>, string<Cs...>, Ts...>>) {
		if constexpr (CodePoint >= 0xD800 && CodePoint <= 0xDBFF) {
			// high surrogate: wait for the low half
			return context<ctll::list<pending_surrogate<CodePoint>, string<Cs...>, Ts...>>{};
		} else if constexpr (CodePoint >= 0xDC00 && CodePoint <= 0xDFFF) {
			// a low surrogate with no preceding high half
			return ctll::reject{};
		} else {
			return context{ctll::list<decltype(append_code_point<static_cast<char32_t>(CodePoint)>(string<Cs...>{})), Ts...>{}};
		}
	}

	template <auto V, size_t CodePoint, size_t High, auto... Cs, typename... Ts> static constexpr auto apply(json::end_hex, ctll::term<V>, context<ctll::list<hex_marker<CodePoint>, pending_surrogate<High>, string<Cs...>, Ts...>>) {
		if constexpr (CodePoint >= 0xDC00 && CodePoint <= 0xDFFF) {
			constexpr char32_t combined = static_cast<char32_t>(0x10000 + ((High - 0xD800) << 10) + (CodePoint - 0xDC00));
			return context{ctll::list<decltype(append_code_point<combined>(string<Cs...>{})), Ts...>{}};
		} else {
			// the high surrogate was not followed by a low one
			return ctll::reject{};
		}
	}

	template <auto V, auto... Cs, typename... Ts> static constexpr auto apply(json::end_string, ctll::term<V>, context<ctll::list<string<Cs...>, Ts...>>) {
		return context<ctll::list<string<Cs...>, Ts...>>{};
	}

	template <auto V, size_t High, typename... Ts> static constexpr auto apply(json::end_string, ctll::term<V>, context<ctll::list<pending_surrogate<High>, Ts...>>) {
		return ctll::reject{};
	}

	// --- numbers accumulate their raw spelling onto a fresh number<>

	template <auto V, typename... Ts> static constexpr auto apply(json::begin_number, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<number<>, Ts...>>{};
	}

	template <auto V, auto... Cs, typename... Ts> static constexpr auto apply(json::push_number_char, ctll::term<V>, context<ctll::list<number<Cs...>, Ts...>>) {
		return context<ctll::list<number<Cs..., static_cast<char32_t>(V)>, Ts...>>{};
	}

	// --- literals

	template <auto V, typename... Ts> static constexpr auto apply(json::push_true, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<boolean<true>, Ts...>>{};
	}

	template <auto V, typename... Ts> static constexpr auto apply(json::push_false, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<boolean<false>, Ts...>>{};
	}

	template <auto V, typename... Ts> static constexpr auto apply(json::push_null, ctll::term<V>, context<ctll::list<Ts...>>) {
		return context<ctll::list<null, Ts...>>{};
	}

	// --- members pair the completed key with the completed value

	template <auto V, auto... Cs, typename... Ts> static constexpr auto apply(json::make_key, ctll::term<V>, context<ctll::list<string<Cs...>, Ts...>>) {
		return context<ctll::list<key_marker<string<Cs...>>, Ts...>>{};
	}

	template <auto V, typename Value, typename Key, typename... Ts> static constexpr auto apply(json::make_member, ctll::term<V>, context<ctll::list<Value, key_marker<Key>, Ts...>>) {
		return context<ctll::list<member<Key, Value>, Ts...>>{};
	}

	// --- closing collects the stacked content back to the begin marker
	// (prepending while popping restores document order)

	template <typename... Collected, typename... Rest> static constexpr auto collect_object(ctll::list<object_begin_marker, Rest...>, ctll::list<Collected...>) {
		return context<ctll::list<object<Collected...>, Rest...>>{};
	}
	template <typename Key, typename Value, typename... Collected, typename... Rest> static constexpr auto collect_object(ctll::list<member<Key, Value>, Rest...>, ctll::list<Collected...>) {
		return collect_object(ctll::list<Rest...>{}, ctll::list<member<Key, Value>, Collected...>{});
	}

	template <auto V, typename... Ts> static constexpr auto apply(json::end_object, ctll::term<V>, context<ctll::list<Ts...>>) {
		return collect_object(ctll::list<Ts...>{}, ctll::list<>{});
	}

	template <typename... Collected, typename... Rest> static constexpr auto collect_array(ctll::list<array_begin_marker, Rest...>, ctll::list<Collected...>) {
		return context<ctll::list<array<Collected...>, Rest...>>{};
	}
	template <typename Value, typename... Collected, typename... Rest> static constexpr auto collect_array(ctll::list<Value, Rest...>, ctll::list<Collected...>) {
		return collect_array(ctll::list<Rest...>{}, ctll::list<Value, Collected...>{});
	}

	template <auto V, typename... Ts> static constexpr auto apply(json::end_array, ctll::term<V>, context<ctll::list<Ts...>>) {
		return collect_array(ctll::list<Ts...>{}, ctll::list<>{});
	}
};

} // namespace ctjson

#endif
