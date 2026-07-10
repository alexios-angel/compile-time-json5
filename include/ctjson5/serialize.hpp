#ifndef CTJSON5__SERIALIZE__HPP
#define CTJSON5__SERIALIZE__HPP

#include "types.hpp"
#ifndef CTJSON5_IN_A_MODULE
#include <array>
#include <cstddef>
#include <string_view>
#endif

// Compile-time serialization: ctjson5::serialize(doc) renders any document
// value back to minified JSON in static storage and returns a
// std::string_view of it - nothing happens at runtime.
//
//   constexpr auto doc = ctjson5::parse<R"({ "a" : [ 1, 2 ] })">();
//   static_assert(ctjson5::serialize(doc) == R"({"a":[1,2]})");
//
// String content is written back with the mandatory escapes (quote,
// backslash, and control characters - \b \f \n \r \t by name, \u00XX
// otherwise); everything else, including multi-byte UTF-8, passes
// through as-is. Numbers keep the spelling they were parsed with.

namespace ctjson5 {

namespace detail {

constexpr bool needs_escape(char c) noexcept {
	return c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20;
}

constexpr size_t escaped_size(char c) noexcept {
	if (!needs_escape(c)) {
		return 1;
	}
	switch (c) {
		case '"': case '\\': case '\b': case '\f': case '\n': case '\r': case '\t':
			return 2;
		default:
			return 6; // \u00XX
	}
}

constexpr char * write_escaped(char * out, char c) noexcept {
	if (!needs_escape(c)) {
		*out++ = c;
		return out;
	}
	*out++ = '\\';
	switch (c) {
		case '"': *out++ = '"'; return out;
		case '\\': *out++ = '\\'; return out;
		case '\b': *out++ = 'b'; return out;
		case '\f': *out++ = 'f'; return out;
		case '\n': *out++ = 'n'; return out;
		case '\r': *out++ = 'r'; return out;
		case '\t': *out++ = 't'; return out;
		default: {
			constexpr char hex[] = "0123456789abcdef";
			*out++ = 'u';
			*out++ = '0';
			*out++ = '0';
			*out++ = hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
			*out++ = hex[static_cast<unsigned char>(c) & 0xF];
			return out;
		}
	}
}

// --- size pass

template <auto... Cs> constexpr size_t serialized_size(string<Cs...>) noexcept {
	size_t total = 2; // the quotes
	((total += escaped_size(static_cast<char>(Cs))), ...);
	return total;
}

template <auto... Cs> constexpr size_t serialized_size(number<Cs...>) noexcept {
	return sizeof...(Cs);
}

constexpr size_t serialized_size(boolean<true>) noexcept { return 4; }
constexpr size_t serialized_size(boolean<false>) noexcept { return 5; }
constexpr size_t serialized_size(null) noexcept { return 4; }

template <typename... Values> constexpr size_t serialized_size(array<Values...>) noexcept {
	size_t total = 2; // the brackets
	((total += serialized_size(Values{})), ...);
	if constexpr (sizeof...(Values) > 1) {
		total += sizeof...(Values) - 1; // the commas
	}
	return total;
}

template <typename... Members> constexpr size_t serialized_size(object<Members...>) noexcept {
	size_t total = 2; // the braces
	((total += serialized_size(typename Members::key_type{}) + 1 + serialized_size(typename Members::value_type{})), ...);
	if constexpr (sizeof...(Members) > 1) {
		total += sizeof...(Members) - 1; // the commas
	}
	return total;
}

// --- write pass

template <auto... Cs> constexpr char * serialize_to(char * out, string<Cs...>) noexcept {
	*out++ = '"';
	((out = write_escaped(out, static_cast<char>(Cs))), ...);
	*out++ = '"';
	return out;
}

template <auto... Cs> constexpr char * serialize_to(char * out, number<Cs...>) noexcept {
	((*out++ = static_cast<char>(Cs)), ...);
	return out;
}

constexpr char * write_literal(char * out, std::string_view text) noexcept {
	for (const char c : text) {
		*out++ = c;
	}
	return out;
}

constexpr char * serialize_to(char * out, boolean<true>) noexcept { return write_literal(out, "true"); }
constexpr char * serialize_to(char * out, boolean<false>) noexcept { return write_literal(out, "false"); }
constexpr char * serialize_to(char * out, null) noexcept { return write_literal(out, "null"); }

template <typename... Values> constexpr char * serialize_to(char * out, array<Values...>) noexcept {
	*out++ = '[';
	size_t index = 0;
	(((index++ != 0 ? void(*out++ = ',') : void()), out = serialize_to(out, Values{})), ...);
	*out++ = ']';
	return out;
}

template <typename... Members> constexpr char * serialize_to(char * out, object<Members...>) noexcept {
	*out++ = '{';
	size_t index = 0;
	(((index++ != 0 ? void(*out++ = ',') : void()),
	  out = serialize_to(out, typename Members::key_type{}),
	  *out++ = ':',
	  out = serialize_to(out, typename Members::value_type{})), ...);
	*out++ = '}';
	return out;
}

// the rendered document lives in static storage, one array per type
template <typename Node> struct serialized_storage {
	static constexpr size_t length = serialized_size(Node{});
	// one extra element keeps the rendering null-terminated
	static constexpr std::array<char, length + 1> compute() noexcept {
		std::array<char, length + 1> out{};
		serialize_to(out.data(), Node{});
		return out;
	}
	static constexpr std::array<char, length + 1> content = compute();
};

} // namespace detail

// minified JSON for any document value, in static storage
CTLL_EXPORT template <typename Node> constexpr std::string_view serialize(Node = Node{}) noexcept {
	using storage = detail::serialized_storage<Node>;
	return std::string_view{storage::content.data(), storage::length};
}

} // namespace ctjson5

#endif
