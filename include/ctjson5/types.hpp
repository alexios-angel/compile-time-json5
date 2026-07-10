#ifndef CTJSON5__TYPES__HPP
#define CTJSON5__TYPES__HPP

#include "../ctll/fixed_string.hpp"
#ifndef CTJSON5_IN_A_MODULE
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>
#endif

// The document types a parse produces. The whole document is a TYPE -
// every string, number and nesting level is encoded in template
// parameters - so the values here are empty structs whose accessors are
// all constexpr and static.
//
// String content is stored as UTF-8 bytes (escapes, including \uXXXX and
// surrogate pairs, are decoded during parsing); numbers keep their raw
// spelling and convert on demand.

namespace ctjson5 {

CTLL_EXPORT enum class kind {
	object,
	array,
	string,
	number,
	boolean,
	null
};

// --- string

CTLL_EXPORT template <auto... Chars> struct string {
	static constexpr kind type = kind::string;

	// null-terminated so c_str()/data() work as C strings; size() excludes it
	static constexpr char storage[sizeof...(Chars) + 1]{static_cast<char>(Chars)..., '\0'};

	static constexpr const char * c_str() noexcept {
		return storage;
	}

	static constexpr size_t size() noexcept {
		return sizeof...(Chars);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Chars) == 0;
	}
	static constexpr std::string_view view() noexcept {
		return std::string_view{storage, sizeof...(Chars)};
	}
	constexpr operator std::string_view() const noexcept {
		return view();
	}
	template <auto... Rhs> constexpr bool operator==(string<Rhs...>) const noexcept {
		return view() == string<Rhs...>::view();
	}
	friend constexpr bool operator==(string, std::string_view rhs) noexcept {
		return view() == rhs;
	}
	friend constexpr bool operator==(std::string_view lhs, string) noexcept {
		return lhs == view();
	}
};

// --- number (raw spelling, converted on demand)

CTLL_EXPORT template <auto... Chars> struct number {
	static constexpr kind type = kind::number;

	// null-terminated so c_str() works as a C string; view() excludes it
	static constexpr char storage[sizeof...(Chars) + 1]{static_cast<char>(Chars)..., '\0'};

	static constexpr const char * c_str() noexcept {
		return storage;
	}

	static constexpr std::string_view view() noexcept {
		return std::string_view{storage, sizeof...(Chars)};
	}

	static constexpr bool is_hexadecimal() noexcept {
		constexpr std::string_view text{storage, sizeof...(Chars)};
		const size_t start = (!text.empty() && (text[0] == '-' || text[0] == '+')) ? 1 : 0;
		return text.size() >= start + 2 && text[start] == '0' && (text[start + 1] == 'x' || text[start + 1] == 'X');
	}

	static constexpr bool is_finite() noexcept {
		// Infinity and NaN are spelled with letters no other form uses
		return ((Chars != 'I' && Chars != 'N') && ...);
	}

	static constexpr bool is_integer() noexcept {
		if (!is_finite()) {
			return false;
		}
		if (is_hexadecimal()) {
			return true;
		}
		return ((Chars != '.' && Chars != 'e' && Chars != 'E') && ...);
	}

	template <typename T> static constexpr T to() noexcept {
		constexpr std::string_view text = view();
		size_t i = 0;
		bool negative = false;
		if (text[0] == '-' || text[0] == '+') {
			negative = text[0] == '-';
			++i;
		}
		// the JSON5 non-finite literals
		if (i < text.size() && text[i] == 'I') {
			if constexpr (std::is_integral_v<T>) {
				return T{}; // no integral infinity; converting would be UB
			} else {
				return negative ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
			}
		}
		if (i < text.size() && text[i] == 'N') {
			if constexpr (std::is_integral_v<T>) {
				return T{};
			} else {
				return std::numeric_limits<T>::quiet_NaN();
			}
		}
		// hexadecimal (JSON5): 0x / 0X followed by hex digits
		if (i + 1 < text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
			unsigned long long magnitude = 0;
			for (i += 2; i < text.size(); ++i) {
				const char c = text[i];
				const auto digit = static_cast<unsigned long long>(
					c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10);
				magnitude = magnitude * 16 + digit;
			}
			if constexpr (std::is_integral_v<T>) {
				const auto value = static_cast<long long>(magnitude);
				return static_cast<T>(negative ? -value : value);
			} else {
				const auto value = static_cast<long double>(magnitude);
				return static_cast<T>(negative ? -value : value);
			}
		}
		// mantissa: integer digits, then fraction digits shifting the
		// decimal exponent down (a leading or trailing point is fine)
		unsigned long long mantissa = 0;
		int exponent10 = 0;
		for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
			mantissa = mantissa * 10 + static_cast<unsigned long long>(text[i] - '0');
		}
		if (i < text.size() && text[i] == '.') {
			++i;
			for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
				mantissa = mantissa * 10 + static_cast<unsigned long long>(text[i] - '0');
				--exponent10;
			}
		}
		if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
			++i;
			bool exp_negative = false;
			if (text[i] == '+' || text[i] == '-') {
				exp_negative = text[i] == '-';
				++i;
			}
			int exp_value = 0;
			for (; i < text.size(); ++i) {
				exp_value = exp_value * 10 + (text[i] - '0');
			}
			exponent10 += exp_negative ? -exp_value : exp_value;
		}

		if constexpr (std::is_integral_v<T>) {
			long long value = static_cast<long long>(mantissa);
			for (; exponent10 > 0; --exponent10) {
				value *= 10;
			}
			for (; exponent10 < 0; ++exponent10) {
				value /= 10; // truncates fractions, like a cast would
			}
			return static_cast<T>(negative ? -value : value);
		} else {
			long double value = static_cast<long double>(mantissa);
			for (; exponent10 > 0; --exponent10) {
				value *= 10.0L;
			}
			for (; exponent10 < 0; ++exponent10) {
				value /= 10.0L;
			}
			return static_cast<T>(negative ? -value : value);
		}
	}
};

// --- boolean and null

CTLL_EXPORT template <bool Value> struct boolean {
	static constexpr kind type = kind::boolean;
	static constexpr bool value = Value;
	constexpr operator bool() const noexcept {
		return Value;
	}
};

CTLL_EXPORT struct null {
	static constexpr kind type = kind::null;
};

// --- array

CTLL_EXPORT template <typename... Values> struct array {
	static constexpr kind type = kind::array;

	static constexpr size_t size() noexcept {
		return sizeof...(Values);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Values) == 0;
	}

	template <size_t Index> static constexpr auto get() noexcept {
		static_assert(Index < sizeof...(Values), "ctjson5: array index out of range");
		return nth<Index, Values...>();
	}

private:
	template <size_t Index, typename Head, typename... Tail> static constexpr auto nth() noexcept {
		if constexpr (Index == 0) {
			return Head{};
		} else {
			return nth<Index - 1, Tail...>();
		}
	}
};

// --- object

CTLL_EXPORT template <typename Key, typename Value> struct member {
	using key_type = Key;
	using value_type = Value;
};

CTLL_EXPORT template <typename... Members> struct object {
	static constexpr kind type = kind::object;

	static constexpr size_t size() noexcept {
		return sizeof...(Members);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Members) == 0;
	}

#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Name> static constexpr bool contains() noexcept {
		return (key_matches<Name, Members>() || ...);
	}

	// the value of the member with this key; a missing key is a
	// compile-time error (check contains<Name>() first when unsure)
	template <ctll::fixed_string Name> static constexpr auto get() noexcept {
		static_assert((key_matches<Name, Members>() || ...), "ctjson5: no member with this key");
		return find<Name, Members...>();
	}
#else
	// C++17: the key is a ctll::fixed_string variable with linkage
	template <const auto & Name> static constexpr bool contains() noexcept {
		return (key_matches<Name, Members>() || ...);
	}
	template <const auto & Name> static constexpr auto get() noexcept {
		static_assert((key_matches<Name, Members>() || ...), "ctjson5: no member with this key");
		return find<Name, Members...>();
	}
#endif

	// positional access, for iterating members
	template <size_t Index> static constexpr auto key() noexcept {
		static_assert(Index < sizeof...(Members), "ctjson5: member index out of range");
		return typename decltype(nth<Index, Members...>())::key_type{};
	}
	template <size_t Index> static constexpr auto value() noexcept {
		static_assert(Index < sizeof...(Members), "ctjson5: member index out of range");
		return typename decltype(nth<Index, Members...>())::value_type{};
	}

private:
#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Name, typename Member> static constexpr bool key_matches() noexcept {
#else
	template <const auto & Name, typename Member> static constexpr bool key_matches() noexcept {
#endif
		constexpr auto key_view = Member::key_type::view();
		if (Name.size() != key_view.size()) {
			return false;
		}
		for (size_t i = 0; i < key_view.size(); ++i) {
			if (static_cast<char32_t>(static_cast<unsigned char>(key_view[i])) != Name[i]) {
				return false;
			}
		}
		return true;
	}

#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Name, typename Head, typename... Tail> static constexpr auto find() noexcept {
#else
	template <const auto & Name, typename Head, typename... Tail> static constexpr auto find() noexcept {
#endif
		if constexpr (key_matches<Name, Head>()) {
			return typename Head::value_type{};
		} else {
			return find<Name, Tail...>();
		}
	}

	template <size_t Index, typename Head, typename... Tail> static constexpr auto nth() noexcept {
		if constexpr (Index == 0) {
			return Head{};
		} else {
			return nth<Index - 1, Tail...>();
		}
	}
};

// compile-time iteration: the callable is invoked once per element
// (arrays) or once per key/value pair (objects), each with its own type
CTLL_EXPORT template <typename F, typename... Values> constexpr void for_each(array<Values...>, F && f) {
	(f(Values{}), ...);
}

CTLL_EXPORT template <typename F, typename... Members> constexpr void for_each(object<Members...>, F && f) {
	(f(typename Members::key_type{}, typename Members::value_type{}), ...);
}

} // namespace ctjson5

#endif
