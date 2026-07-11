#ifndef CTJSON5__BIND__HPP
#define CTJSON5__BIND__HPP

#include "grammar.hpp"
#include "types.hpp"
#ifndef CTJSON5_IN_A_MODULE
#include <cstddef>
#include <string_view>
#include <utility>
#endif

// Lowering a ctlark parse tree into ctjson5's document types. The
// tree shapes are fixed by grammar.hpp: object(pair...),
// pair(STRING|IDENT value), array(value...), string(STRING),
// number(NUMBER), true/false/null.
//
// Strings decode here with JSON5's escape semantics: the named
// escapes plus \v and \0, \xHH and \uXXXX to UTF-8 (surrogate pairs
// combine; lone surrogates are the one error a regular terminal
// cannot catch, so bind<Tree>::ok carries it into is_valid), any
// other escaped character stands for itself, and line continuations
// (backslash before LF, CR, CRLF, LS or PS) vanish. Unquoted keys
// pass through as-is; numbers keep their raw spelling.

namespace ctjson5 {

// why the binder rejected a document that PARSES - the one JSON5 rule
// the grammar itself cannot express
CTLL_EXPORT enum class bind_reason : unsigned char {
	none,
	bad_surrogate // \uXXXX surrogate pairing rules in a string
};

CTLL_EXPORT constexpr std::string_view to_string(bind_reason r) noexcept {
	switch (r) {
		case bind_reason::none: return "none";
		case bind_reason::bad_surrogate: return "bad \\u surrogate pairing in a string";
	}
	return "unknown";
}

// the first binder failure: which rule broke, and the raw offending
// token as written in the input
CTLL_EXPORT struct bind_error_t {
	bind_reason reason = bind_reason::none;
	std::string_view where{};

	constexpr bool ok() const noexcept {
		return reason == bind_reason::none;
	}
};

} // namespace ctjson5

namespace ctjson5::detail {

// first-failure-wins fold over child binders
template <typename... Bs> constexpr bind_error_t bind_first_fail() noexcept {
	const bind_error_t fails[] = {Bs::fail..., bind_error_t{}};
	for (const bind_error_t & f : fails) {
		if (f.reason != bind_reason::none) { return f; }
	}
	return bind_error_t{};
}

using bt_object = ctlark::text<'o', 'b', 'j', 'e', 'c', 't'>;
using bt_pair = ctlark::text<'p', 'a', 'i', 'r'>;
using bt_array = ctlark::text<'a', 'r', 'r', 'a', 'y'>;
using bt_string = ctlark::text<'s', 't', 'r', 'i', 'n', 'g'>;
using bt_number = ctlark::text<'n', 'u', 'm', 'b', 'e', 'r'>;
using bt_true = ctlark::text<'t', 'r', 'u', 'e'>;
using bt_false = ctlark::text<'f', 'a', 'l', 's', 'e'>;
using bt_null = ctlark::text<'n', 'u', 'l', 'l'>;
using bt_STRING = ctlark::text<'S', 'T', 'R', 'I', 'N', 'G'>;

constexpr int bind_hexval(char c) noexcept {
	if (c >= '0' && c <= '9') { return c - '0'; }
	if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
	return c - 'A' + 10;
}

template <typename Text> struct decode_string {
	struct out_t {
		char buf[Text::size() + 1]{};
		size_t len = 0;
		bool ok = true;
	};

	static constexpr void put_code_point(out_t & o, unsigned long cp) noexcept {
		if (cp < 0x80) {
			o.buf[o.len++] = static_cast<char>(cp);
		} else if (cp < 0x800) {
			o.buf[o.len++] = static_cast<char>(0xC0 | (cp >> 6));
			o.buf[o.len++] = static_cast<char>(0x80 | (cp & 0x3F));
		} else if (cp < 0x10000) {
			o.buf[o.len++] = static_cast<char>(0xE0 | (cp >> 12));
			o.buf[o.len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			o.buf[o.len++] = static_cast<char>(0x80 | (cp & 0x3F));
		} else {
			o.buf[o.len++] = static_cast<char>(0xF0 | (cp >> 18));
			o.buf[o.len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			o.buf[o.len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			o.buf[o.len++] = static_cast<char>(0x80 | (cp & 0x3F));
		}
	}

	static constexpr out_t compute() noexcept {
		out_t o{};
		constexpr std::string_view raw = Text::view();
		size_t i = 1;                      // the grammar guarantees the
		const size_t end = raw.size() - 1; // surrounding quotes
		while (i < end) {
			const char c = raw[i];
			if (c != '\\') {
				o.buf[o.len++] = c;
				++i;
				continue;
			}
			const char e = raw[i + 1];
			if (e == 'u') {
				unsigned long cp = 0;
				for (size_t k = i + 2; k < i + 6; ++k) {
					cp = cp * 16 + static_cast<unsigned long>(bind_hexval(raw[k]));
				}
				i += 6;
				if (cp >= 0xD800 && cp <= 0xDBFF) {
					if (i + 6 <= end && raw[i] == '\\' && raw[i + 1] == 'u') {
						unsigned long lo = 0;
						for (size_t k = i + 2; k < i + 6; ++k) {
							lo = lo * 16 + static_cast<unsigned long>(bind_hexval(raw[k]));
						}
						if (lo >= 0xDC00 && lo <= 0xDFFF) {
							cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
							i += 6;
						} else {
							o.ok = false;
							return o;
						}
					} else {
						o.ok = false;
						return o;
					}
				} else if (cp >= 0xDC00 && cp <= 0xDFFF) {
					o.ok = false; // lone low surrogate
					return o;
				}
				put_code_point(o, cp);
				continue;
			}
			if (e == 'x') {
				const unsigned long cp = static_cast<unsigned long>(
					bind_hexval(raw[i + 2]) * 16 + bind_hexval(raw[i + 3]));
				put_code_point(o, cp); // \xHH means U+00HH
				i += 4;
				continue;
			}
			// line continuations vanish: \ before LF, CR, CRLF, LS, PS
			if (e == '\x0A') {
				i += 2;
				continue;
			}
			if (e == '\x0D') {
				i += 2;
				if (i < end && raw[i] == '\x0A') { ++i; }
				continue;
			}
			if (static_cast<unsigned char>(e) == 0xE2 && i + 3 < end
			    && static_cast<unsigned char>(raw[i + 2]) == 0x80
			    && (static_cast<unsigned char>(raw[i + 3]) == 0xA8
			        || static_cast<unsigned char>(raw[i + 3]) == 0xA9)) {
				i += 4;
				continue;
			}
			switch (e) {
				case 'b': o.buf[o.len++] = '\b'; break;
				case 'f': o.buf[o.len++] = '\f'; break;
				case 'n': o.buf[o.len++] = '\n'; break;
				case 'r': o.buf[o.len++] = '\r'; break;
				case 't': o.buf[o.len++] = '\t'; break;
				case 'v': o.buf[o.len++] = '\v'; break;
				case '0': o.buf[o.len++] = '\0'; break;
				default: o.buf[o.len++] = e; break; // " ' \ / and self-escapes
			}
			i += 2;
		}
		return o;
	}

	static constexpr out_t data = compute();
	static constexpr bool ok = data.ok;

	template <size_t... I> static constexpr auto lift(std::index_sequence<I...>) noexcept {
		return ctjson5::string<data.buf[I]...>{};
	}
	using type = decltype(lift(std::make_index_sequence<data.len>{}));
};

// unquoted keys pass through unchanged; numbers keep their spelling

template <typename Text> struct make_string;
template <auto... Cs> struct make_string<ctlark::text<Cs...>> {
	using type = ctjson5::string<Cs...>;
};
template <typename Text> struct make_number;
template <auto... Cs> struct make_number<ctlark::text<Cs...>> {
	using type = ctjson5::number<Cs...>;
};

// a key token is either a quoted STRING (decode) or an IDENT (as-is)
template <typename Token> struct bind_key {
	using type = typename make_string<typename Token::value_type>::type;
	static constexpr bool ok = true;
	static constexpr bind_error_t fail{};
};
template <typename Value> struct bind_key<ctlark::token<bt_STRING, Value>> {
	using decoded = decode_string<Value>;
	using type = typename decoded::type;
	static constexpr bool ok = decoded::ok;
	static constexpr bind_error_t fail =
		decoded::ok ? bind_error_t{} : bind_error_t{bind_reason::bad_surrogate, Value::view()};
};

// --- the binder

template <typename Node> struct bind;

template <typename... Pairs> struct bind<ctlark::tree<bt_object, Pairs...>> {
	using type = ctjson5::object<typename bind<Pairs>::type...>;
	static constexpr bool ok = (bind<Pairs>::ok && ... && true);
	static constexpr bind_error_t fail = bind_first_fail<bind<Pairs>...>();
};

template <typename Key, typename Value> struct bind<ctlark::tree<bt_pair, Key, Value>> {
	using key = bind_key<Key>;
	using type = ctjson5::member<typename key::type, typename bind<Value>::type>;
	static constexpr bool ok = key::ok && bind<Value>::ok;
	static constexpr bind_error_t fail = key::ok ? bind<Value>::fail : key::fail;
};

template <typename... Values> struct bind<ctlark::tree<bt_array, Values...>> {
	using type = ctjson5::array<typename bind<Values>::type...>;
	static constexpr bool ok = (bind<Values>::ok && ... && true);
	static constexpr bind_error_t fail = bind_first_fail<bind<Values>...>();
};

template <typename Token> struct bind<ctlark::tree<bt_string, Token>> {
	using decoded = decode_string<typename Token::value_type>;
	using type = typename decoded::type;
	static constexpr bool ok = decoded::ok;
	static constexpr bind_error_t fail =
		decoded::ok ? bind_error_t{} : bind_error_t{bind_reason::bad_surrogate, Token::value_type::view()};
};

template <typename Token> struct bind<ctlark::tree<bt_number, Token>> {
	using type = typename make_number<typename Token::value_type>::type;
	static constexpr bool ok = true;
	static constexpr bind_error_t fail{};
};

template <> struct bind<ctlark::tree<bt_true>> {
	using type = ctjson5::boolean<true>;
	static constexpr bool ok = true;
	static constexpr bind_error_t fail{};
};
template <> struct bind<ctlark::tree<bt_false>> {
	using type = ctjson5::boolean<false>;
	static constexpr bool ok = true;
	static constexpr bind_error_t fail{};
};
template <> struct bind<ctlark::tree<bt_null>> {
	using type = ctjson5::null;
	static constexpr bool ok = true;
	static constexpr bind_error_t fail{};
};

} // namespace ctjson5::detail

#endif
