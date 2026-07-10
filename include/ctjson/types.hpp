#ifndef CTJSON__TYPES__HPP
#define CTJSON__TYPES__HPP

#include "../ctll/fixed_string.hpp"
#ifndef CTJSON_IN_A_MODULE
#include <cstddef>
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

namespace ctjson {

enum class kind {
	object,
	array,
	string,
	number,
	boolean,
	null
};

// --- string

template <auto... Chars> struct string {
	static constexpr kind type = kind::string;

	static constexpr char storage[sizeof...(Chars) == 0 ? 1 : sizeof...(Chars)]{static_cast<char>(Chars)...};

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

template <auto... Chars> struct number {
	static constexpr kind type = kind::number;

	static constexpr char storage[sizeof...(Chars) == 0 ? 1 : sizeof...(Chars)]{static_cast<char>(Chars)...};

	static constexpr std::string_view view() noexcept {
		return std::string_view{storage, sizeof...(Chars)};
	}

	static constexpr bool is_integer() noexcept {
		return ((Chars != '.' && Chars != 'e' && Chars != 'E') && ...);
	}

	template <typename T> static constexpr T to() noexcept {
		constexpr std::string_view text = view();
		size_t i = 0;
		const bool negative = text[0] == '-';
		if (negative) {
			++i;
		}
		// mantissa: integer digits, then fraction digits shifting the
		// decimal exponent down
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

template <bool Value> struct boolean {
	static constexpr kind type = kind::boolean;
	static constexpr bool value = Value;
	constexpr operator bool() const noexcept {
		return Value;
	}
};

struct null {
	static constexpr kind type = kind::null;
};

// --- array

template <typename... Values> struct array {
	static constexpr kind type = kind::array;

	static constexpr size_t size() noexcept {
		return sizeof...(Values);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Values) == 0;
	}

	template <size_t Index> static constexpr auto get() noexcept {
		static_assert(Index < sizeof...(Values), "ctjson: array index out of range");
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

template <typename Key, typename Value> struct member {
	using key_type = Key;
	using value_type = Value;
};

template <typename... Members> struct object {
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
		static_assert((key_matches<Name, Members>() || ...), "ctjson: no member with this key");
		return find<Name, Members...>();
	}
#else
	// C++17: the key is a ctll::fixed_string variable with linkage
	template <const auto & Name> static constexpr bool contains() noexcept {
		return (key_matches<Name, Members>() || ...);
	}
	template <const auto & Name> static constexpr auto get() noexcept {
		static_assert((key_matches<Name, Members>() || ...), "ctjson: no member with this key");
		return find<Name, Members...>();
	}
#endif

	// positional access, for iterating members
	template <size_t Index> static constexpr auto key() noexcept {
		static_assert(Index < sizeof...(Members), "ctjson: member index out of range");
		return typename decltype(nth<Index, Members...>())::key_type{};
	}
	template <size_t Index> static constexpr auto value() noexcept {
		static_assert(Index < sizeof...(Members), "ctjson: member index out of range");
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

} // namespace ctjson

#endif
