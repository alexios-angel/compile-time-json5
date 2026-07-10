#ifndef CTJSON5__LOAD__HPP
#define CTJSON5__LOAD__HPP

#include "dumps.hpp"
#ifndef CTJSON5_IN_A_MODULE
#include <charconv>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#endif

// Runtime parsing in the style of Python's json module:
//
//   std::optional<ctjson5::value> doc = ctjson5::loads(text);   // json.loads
//   std::optional<ctjson5::value> doc = ctjson5::load(stream);  // json.load
//
//   if (doc) {
//       (*doc)["users"][0]["name"].view();
//       (*doc)["retries"].to<int>();
//       ctjson5::dumps(*doc, 2);   // the encoder accepts runtime documents
//   }
//
// The result is a ctjson5::value - a dynamic document mirroring the
// compile-time one's accessors (type(), get, contains, size, view,
// to<T>(), is_integer()...). Missing keys and out-of-range indices
// follow the null-object pattern: operator[] returns a shared null
// value, so lookup chains never dereference invalid memory and the
// library stays exception free. Use find()/contains() to distinguish
// null values from absent ones. On malformed input loads/load return
// std::nullopt; the overloads taking a load_error report the byte
// offset and a static message.
//
// Python parity notes: numbers keep the int/float distinction
// (is_integer(); integers beyond long long fall back to double, since
// C++ has no big ints), NaN/Infinity/-Infinity are accepted exactly
// like Python's default parse_constant, and numbers are stored as
// numbers - loads("2.50") re-dumps as "2.5", just like Python. (That is
// the one behavioural difference from the compile-time parser, which
// preserves the spelling.) Two deliberate divergences from Python:
// duplicate object keys are all kept, with lookups finding the first
// (matching the compile-time parser; Python keeps the last), and lone
// UTF-16 surrogates in \u escapes are rejected (Python lets them
// through even though they cannot round-trip through UTF-8).

namespace ctjson5 {

struct load_error {
	size_t position = 0;
	const char * message = "";
};

CTLL_EXPORT class value {
	kind type_ = kind::null;
	bool boolean_ = false;
	bool integral_ = false;
	long long integer_ = 0;
	double floating_ = 0;
	std::string string_;
	std::vector<value> elements_;
	std::vector<std::pair<std::string, value>> members_;

	static const value & shared_null() noexcept {
		static const value null_value;
		return null_value;
	}

public:
	value() = default;

	// construction of each kind
	static value make_null() { return value{}; }
	static value make_boolean(bool b) {
		value v;
		v.type_ = kind::boolean;
		v.boolean_ = b;
		return v;
	}
	static value make_integer(long long i) {
		value v;
		v.type_ = kind::number;
		v.integral_ = true;
		v.integer_ = i;
		v.floating_ = static_cast<double>(i);
		return v;
	}
	static value make_double(double d) {
		value v;
		v.type_ = kind::number;
		v.floating_ = d;
		v.integer_ = static_cast<long long>(d);
		return v;
	}
	static value make_string(std::string s) {
		value v;
		v.type_ = kind::string;
		v.string_ = std::move(s);
		return v;
	}
	static value make_array(std::vector<value> elements) {
		value v;
		v.type_ = kind::array;
		v.elements_ = std::move(elements);
		return v;
	}
	static value make_object(std::vector<std::pair<std::string, value>> members) {
		value v;
		v.type_ = kind::object;
		v.members_ = std::move(members);
		return v;
	}

	// introspection, mirroring the compile-time document
	kind type() const noexcept { return type_; }
	bool is_null() const noexcept { return type_ == kind::null; }
	bool is_boolean() const noexcept { return type_ == kind::boolean; }
	bool is_number() const noexcept { return type_ == kind::number; }
	bool is_string() const noexcept { return type_ == kind::string; }
	bool is_array() const noexcept { return type_ == kind::array; }
	bool is_object() const noexcept { return type_ == kind::object; }

	bool boolean() const noexcept { return boolean_; }
	explicit operator bool() const noexcept { return type_ == kind::boolean && boolean_; }

	bool is_integer() const noexcept { return type_ == kind::number && integral_; }
	template <typename T> T to() const noexcept {
		if constexpr (std::is_integral_v<T>) {
			return integral_ ? static_cast<T>(integer_) : static_cast<T>(floating_);
		} else {
			return integral_ ? static_cast<T>(integer_) : static_cast<T>(floating_);
		}
	}

	std::string_view view() const noexcept { return string_; }
	const std::string & str() const noexcept { return string_; }

	size_t size() const noexcept {
		switch (type_) {
			case kind::array: return elements_.size();
			case kind::object: return members_.size();
			case kind::string: return string_.size();
			default: return 0;
		}
	}
	bool empty() const noexcept { return size() == 0; }

	// the whole containers, for iteration
	const std::vector<value> & as_array() const noexcept { return elements_; }
	const std::vector<std::pair<std::string, value>> & as_object() const noexcept { return members_; }

	// lookup: chains are safe through the shared null (see header note)
	const value * find(std::string_view key) const noexcept {
		for (const auto & entry : members_) {
			if (entry.first == key) {
				return &entry.second;
			}
		}
		return nullptr;
	}
	bool contains(std::string_view key) const noexcept { return find(key) != nullptr; }

	const value & operator[](std::string_view key) const noexcept {
		const value * found = find(key);
		return found != nullptr ? *found : shared_null();
	}
	const value & operator[](size_t index) const noexcept {
		return index < elements_.size() ? elements_[index] : shared_null();
	}
	const value & get(std::string_view key) const noexcept { return (*this)[key]; }
	const value & get(size_t index) const noexcept { return (*this)[index]; }

	friend bool operator==(const value & lhs, const value & rhs) noexcept {
		if (lhs.type_ != rhs.type_) {
			return false;
		}
		switch (lhs.type_) {
			case kind::null: return true;
			case kind::boolean: return lhs.boolean_ == rhs.boolean_;
			case kind::number:
				if (lhs.integral_ && rhs.integral_) {
					return lhs.integer_ == rhs.integer_;
				}
				return lhs.floating_ == rhs.floating_;
			case kind::string: return lhs.string_ == rhs.string_;
			case kind::array: return lhs.elements_ == rhs.elements_;
			case kind::object: return lhs.members_ == rhs.members_;
		}
		return false;
	}
	friend bool operator!=(const value & lhs, const value & rhs) noexcept { return !(lhs == rhs); }
};

// let the Python-style encoder accept runtime documents (the hook is
// declared in dumps.hpp)
namespace detail {

template <> struct is_runtime_document<value>: std::true_type { };

template <> struct runtime_document_writer<value> {
	static void write(dumper & d, const value & v, int depth) {
		switch (v.type()) {
			case kind::null:
				d.out += "null";
				return;
			case kind::boolean:
				d.out += v.boolean() ? "true" : "false";
				return;
			case kind::number:
				if (v.is_integer()) {
					d.write_integer(v.to<long long>());
				} else {
					d.write_floating(v.to<double>());
				}
				return;
			case kind::string:
				d.write_string(v.view());
				return;
			case kind::array:
				d.write_array_shell(v.as_array().size(), depth, [&] {
					bool first = true;
					for (const value & element : v.as_array()) {
						if (!first) {
							d.item_separator(depth + 1);
						}
						first = false;
						d.value(element, depth + 1);
					}
				});
				return;
			case kind::object: {
				std::vector<std::pair<std::string, std::string>> members;
				members.reserve(v.as_object().size());
				for (const auto & entry : v.as_object()) {
					members.emplace_back(d.render_key(entry.first), d.render_value(entry.second, depth + 1));
				}
				d.write_members(std::move(members), depth);
				return;
			}
		}
	}
};

// --- the runtime parser: recursive descent, RFC 8259 with Python's
// NaN/Infinity extension

struct runtime_parser {
	std::string_view text;
	size_t position = 0;
	load_error error;

	bool fail(const char * message) noexcept {
		error.position = position;
		error.message = message;
		return false;
	}

	bool at_end() const noexcept { return position >= text.size(); }
	char peek() const noexcept { return text[position]; }

	bool has(std::string_view expected) const noexcept {
		return text.substr(position, expected.size()) == expected;
	}

	// JSON5 whitespace: the ASCII spaces plus \v and \f, comments, and
	// the multibyte NBSP, BOM, LS and PS
	bool skip_whitespace() noexcept {
		for (;;) {
			if (at_end()) {
				return true;
			}
			const char c = peek();
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
				++position;
				continue;
			}
			if (has("\xC2\xA0") || has("\xE2\x80\xA8") || has("\xE2\x80\xA9")) {
				position += 2 + (static_cast<unsigned char>(c) == 0xE2 ? 1 : 0);
				continue;
			}
			if (has("\xEF\xBB\xBF")) {
				position += 3;
				continue;
			}
			if (c == '/') {
				if (position + 1 >= text.size()) {
					return fail("a lone '/' is not a comment");
				}
				const char second = text[position + 1];
				if (second == '/') {
					position += 2;
					while (!at_end() && peek() != '\n' && peek() != '\r') {
						++position;
					}
					continue;
				}
				if (second == '*') {
					position += 2;
					for (;;) {
						if (position + 1 >= text.size()) {
							return fail("unterminated block comment");
						}
						if (text[position] == '*' && text[position + 1] == '/') {
							position += 2;
							break;
						}
						++position;
					}
					continue;
				}
				return fail("a lone '/' is not a comment");
			}
			return true;
		}
	}

	bool consume_literal(std::string_view literal) noexcept {
		if (text.substr(position, literal.size()) != literal) {
			return fail("unrecognized literal");
		}
		position += literal.size();
		return true;
	}

	bool parse_hex4(unsigned int & out) noexcept {
		if (position + 4 > text.size()) {
			return fail("truncated \\u escape");
		}
		out = 0;
		for (int i = 0; i < 4; ++i) {
			const char c = text[position + static_cast<size_t>(i)];
			unsigned int digit = 0;
			if (c >= '0' && c <= '9') {
				digit = static_cast<unsigned int>(c - '0');
			} else if (c >= 'a' && c <= 'f') {
				digit = static_cast<unsigned int>(c - 'a' + 10);
			} else if (c >= 'A' && c <= 'F') {
				digit = static_cast<unsigned int>(c - 'A' + 10);
			} else {
				return fail("invalid \\u escape digit");
			}
			out = out * 16 + digit;
		}
		position += 4;
		return true;
	}

	static void append_utf8(std::string & out, unsigned int code_point) {
		if (code_point < 0x80) {
			out += static_cast<char>(code_point);
		} else if (code_point < 0x800) {
			out += static_cast<char>(0xC0 | (code_point >> 6));
			out += static_cast<char>(0x80 | (code_point & 0x3F));
		} else if (code_point < 0x10000) {
			out += static_cast<char>(0xE0 | (code_point >> 12));
			out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (code_point & 0x3F));
		} else {
			out += static_cast<char>(0xF0 | (code_point >> 18));
			out += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
			out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (code_point & 0x3F));
		}
	}

	bool parse_string(std::string & out) noexcept {
		const char closing = peek();
		++position; // the opening quote
		for (;;) {
			if (at_end()) {
				return fail("unterminated string");
			}
			const char c = peek();
			if (c == closing) {
				++position;
				return true;
			}
			// JSON5 permits raw control characters except line terminators
			if (c == '\n' || c == '\r') {
				return fail("a raw line terminator needs a \\ continuation");
			}
			if (c != '\\') {
				out += c;
				++position;
				continue;
			}
			++position; // the backslash
			if (at_end()) {
				return fail("truncated escape");
			}
			const char escape = peek();
			++position;
			switch (escape) {
				case '"': out += '"'; continue;
				case '\'': out += '\''; continue;
				case '\\': out += '\\'; continue;
				case '/': out += '/'; continue;
				case 'b': out += '\b'; continue;
				case 'f': out += '\f'; continue;
				case 'n': out += '\n'; continue;
				case 'r': out += '\r'; continue;
				case 't': out += '\t'; continue;
				case 'v': out += '\v'; continue;
				case '0': out += '\0'; continue;
				// line continuations: the terminator is swallowed
				case '\n': continue;
				case '\r':
					if (!at_end() && peek() == '\n') {
						++position;
					}
					continue;
				case 'x': case 'X': {
					unsigned int code_point = 0;
					if (position + 2 > text.size()) {
						return fail("truncated \\x escape");
					}
					for (int i = 0; i < 2; ++i) {
						const char h = text[position + static_cast<size_t>(i)];
						unsigned int digit = 0;
						if (h >= '0' && h <= '9') {
							digit = static_cast<unsigned int>(h - '0');
						} else if (h >= 'a' && h <= 'f') {
							digit = static_cast<unsigned int>(h - 'a' + 10);
						} else if (h >= 'A' && h <= 'F') {
							digit = static_cast<unsigned int>(h - 'A' + 10);
						} else {
							return fail("invalid \\x escape digit");
						}
						code_point = code_point * 16 + digit;
					}
					position += 2;
					append_utf8(out, code_point);
					continue;
				}
				case 'u': {
					unsigned int code_point = 0;
					if (!parse_hex4(code_point)) {
						return false;
					}
					if (code_point >= 0xD800 && code_point <= 0xDBFF) {
						// a high surrogate must pair with a low one
						if (position + 2 > text.size() || text[position] != '\\' || text[position + 1] != 'u') {
							return fail("lone high surrogate");
						}
						position += 2;
						unsigned int low = 0;
						if (!parse_hex4(low)) {
							return false;
						}
						if (low < 0xDC00 || low > 0xDFFF) {
							return fail("invalid low surrogate");
						}
						code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
					} else if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
						return fail("lone low surrogate");
					}
					append_utf8(out, code_point);
					continue;
				}
				default:
					if (escape >= '1' && escape <= '9') {
						return fail("digits cannot be escaped");
					}
					// \<U+2028> and \<U+2029> are line continuations
					if (static_cast<unsigned char>(escape) == 0xE2 && position + 1 < text.size()
					    && text[position] == '\x80'
					    && (text[position + 1] == '\xA8' || text[position + 1] == '\xA9')) {
						position += 2;
						continue;
					}
					// any other escaped character stands for itself
					out += escape;
					continue;
			}
		}
	}

	bool parse_number(value & out) noexcept {
		const size_t start = position;
		bool negative = false;
		if (!at_end() && (peek() == '-' || peek() == '+')) {
			negative = peek() == '-';
			++position;
		}
		// the non-finite literals may carry either sign
		if (has("Infinity")) {
			position += 8;
			out = value::make_double(negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
			return true;
		}
		if (has("NaN")) {
			position += 3;
			out = value::make_double(std::numeric_limits<double>::quiet_NaN());
			return true;
		}
		if (at_end() || ((peek() < '0' || peek() > '9') && peek() != '.')) {
			return fail("a number needs a digit");
		}
		// hexadecimal: 0x / 0X with at least one digit
		if (peek() == '0' && position + 1 < text.size() && (text[position + 1] == 'x' || text[position + 1] == 'X')) {
			position += 2;
			unsigned long long magnitude = 0;
			size_t digits = 0;
			while (!at_end()) {
				const char h = peek();
				unsigned int digit = 0;
				if (h >= '0' && h <= '9') {
					digit = static_cast<unsigned int>(h - '0');
				} else if (h >= 'a' && h <= 'f') {
					digit = static_cast<unsigned int>(h - 'a' + 10);
				} else if (h >= 'A' && h <= 'F') {
					digit = static_cast<unsigned int>(h - 'A' + 10);
				} else {
					break;
				}
				magnitude = magnitude * 16 + digit;
				++digits;
				++position;
			}
			if (digits == 0) {
				return fail("a hex number needs a digit");
			}
			const auto integer = static_cast<long long>(magnitude);
			out = value::make_integer(negative ? -integer : integer);
			return true;
		}
		bool integral = true;
		bool any_int_digits = false;
		if (!at_end() && peek() == '0') {
			++position; // no leading zeros: 0 must stand alone
			any_int_digits = true;
		} else {
			while (!at_end() && peek() >= '0' && peek() <= '9') {
				++position;
				any_int_digits = true;
			}
		}
		if (!at_end() && peek() == '.') {
			integral = false;
			++position;
			// ".5" needs fraction digits; "5." does not
			bool any_frac_digits = false;
			while (!at_end() && peek() >= '0' && peek() <= '9') {
				++position;
				any_frac_digits = true;
			}
			if (!any_int_digits && !any_frac_digits) {
				return fail("a number needs a digit");
			}
		}
		if (!at_end() && (peek() == 'e' || peek() == 'E')) {
			integral = false;
			++position;
			if (!at_end() && (peek() == '+' || peek() == '-')) {
				++position;
			}
			if (at_end() || peek() < '0' || peek() > '9') {
				return fail("an exponent needs a digit");
			}
			while (!at_end() && peek() >= '0' && peek() <= '9') {
				++position;
			}
		}

		// normalize the JSON5-only spellings into what from_chars accepts:
		// drop an explicit '+', give a leading '.' its zero
		std::string normalized;
		{
			std::string_view spelling = text.substr(start, position - start);
			if (!spelling.empty() && spelling[0] == '+') {
				spelling.remove_prefix(1);
			}
			const bool leading_point = !spelling.empty() && spelling[0] == '.';
			const bool negative_point = spelling.size() >= 2 && spelling[0] == '-' && spelling[1] == '.';
			normalized.reserve(spelling.size() + 1);
			if (negative_point) {
				normalized += "-0";
				normalized.append(spelling.data() + 1, spelling.size() - 1);
			} else if (leading_point) {
				normalized += '0';
				normalized.append(spelling.data(), spelling.size());
			} else {
				normalized.append(spelling.data(), spelling.size());
			}
		}
		if (integral) {
			long long parsed = 0;
			const auto result = std::from_chars(normalized.data(), normalized.data() + normalized.size(), parsed);
			if (result.ec == std::errc{} && result.ptr == normalized.data() + normalized.size()) {
				out = value::make_integer(parsed);
				return true;
			}
			// beyond long long: fall back to double (C++ has no big ints)
		}
		double parsed = 0;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
		const auto result = std::from_chars(normalized.data(), normalized.data() + normalized.size(), parsed);
		if (result.ec == std::errc::result_out_of_range) {
			// Python overflows to the infinities
			parsed = normalized[0] == '-' ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
		}
#else
		parsed = std::strtod(normalized.c_str(), nullptr);
#endif
		out = value::make_double(parsed);
		return true;
	}

	bool parse_value(value & out, int nesting) noexcept {
		if (nesting > 256) {
			return fail("nesting too deep");
		}
		if (at_end()) {
			return fail("a value was expected");
		}
		const auto is_id_start = [](char ch) {
			return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '$';
		};
		const auto is_id_continue = [&](char ch) {
			return is_id_start(ch) || (ch >= '0' && ch <= '9');
		};

		const char c = peek();
		switch (c) {
			case '{': {
				++position;
				std::vector<std::pair<std::string, value>> members;
				if (!skip_whitespace()) {
					return false;
				}
				if (!at_end() && peek() == '}') {
					++position;
					out = value::make_object(std::move(members));
					return true;
				}
				for (;;) {
					if (!skip_whitespace()) {
						return false;
					}
					// a trailing comma leaves us on the closing brace
					if (!at_end() && peek() == '}') {
						++position;
						out = value::make_object(std::move(members));
						return true;
					}
					// keys: double- or single-quoted, or an identifier
					std::string key;
					if (!at_end() && (peek() == '"' || peek() == '\'')) {
						if (!parse_string(key)) {
							return false;
						}
					} else if (!at_end() && is_id_start(peek())) {
						while (!at_end() && is_id_continue(peek())) {
							key += peek();
							++position;
						}
					} else {
						return fail("an object key must be a string or an identifier");
					}
					if (!skip_whitespace()) {
						return false;
					}
					if (at_end() || peek() != ':') {
						return fail("':' expected after an object key");
					}
					++position;
					if (!skip_whitespace()) {
						return false;
					}
					value member_value;
					if (!parse_value(member_value, nesting + 1)) {
						return false;
					}
					members.emplace_back(std::move(key), std::move(member_value));
					if (!skip_whitespace()) {
						return false;
					}
					if (!at_end() && peek() == ',') {
						++position;
						continue;
					}
					if (!at_end() && peek() == '}') {
						++position;
						out = value::make_object(std::move(members));
						return true;
					}
					return fail("',' or '}' expected in an object");
				}
			}
			case '[': {
				++position;
				std::vector<value> elements;
				if (!skip_whitespace()) {
					return false;
				}
				if (!at_end() && peek() == ']') {
					++position;
					out = value::make_array(std::move(elements));
					return true;
				}
				for (;;) {
					if (!skip_whitespace()) {
						return false;
					}
					// a trailing comma leaves us on the closing bracket
					if (!at_end() && peek() == ']') {
						++position;
						out = value::make_array(std::move(elements));
						return true;
					}
					value element;
					if (!parse_value(element, nesting + 1)) {
						return false;
					}
					elements.push_back(std::move(element));
					if (!skip_whitespace()) {
						return false;
					}
					if (!at_end() && peek() == ',') {
						++position;
						continue;
					}
					if (!at_end() && peek() == ']') {
						++position;
						out = value::make_array(std::move(elements));
						return true;
					}
					return fail("',' or ']' expected in an array");
				}
			}
			case '"':
			case '\'': {
				std::string content;
				if (!parse_string(content)) {
					return false;
				}
				out = value::make_string(std::move(content));
				return true;
			}
			case 't':
				if (!consume_literal("true")) {
					return false;
				}
				out = value::make_boolean(true);
				return true;
			case 'f':
				if (!consume_literal("false")) {
					return false;
				}
				out = value::make_boolean(false);
				return true;
			case 'n':
				if (!consume_literal("null")) {
					return false;
				}
				out = value::make_null();
				return true;
			default:
				// numbers own every remaining valid start: digits, the
				// signs, the point, Infinity and NaN
				if (c == '-' || c == '+' || c == '.' || c == 'I' || c == 'N' || (c >= '0' && c <= '9')) {
					return parse_number(out);
				}
				return fail("a value was expected");
		}
	}

	bool parse_document(value & out) noexcept {
		if (!skip_whitespace()) {
			return false;
		}
		if (!parse_value(out, 0)) {
			return false;
		}
		if (!skip_whitespace()) {
			return false;
		}
		if (!at_end()) {
			return fail("input continues after the value");
		}
		return true;
	}
};

} // namespace detail

// like json.loads on runtime text
CTLL_EXPORT inline std::optional<value> loads(std::string_view text, load_error & error) {
	detail::runtime_parser parser{text, 0, {}};
	value result;
	if (!parser.parse_document(result)) {
		error = parser.error;
		return std::nullopt;
	}
	return result;
}

CTLL_EXPORT inline std::optional<value> loads(std::string_view text) {
	load_error ignored;
	return loads(text, ignored);
}

// like json.load: parse a whole stream
CTLL_EXPORT inline std::optional<value> load(std::istream & stream, load_error & error) {
	std::string content;
	char buffer[4096];
	while (stream.read(buffer, sizeof(buffer)) || stream.gcount() > 0) {
		content.append(buffer, static_cast<size_t>(stream.gcount()));
	}
	if (stream.bad()) {
		error.message = "the stream failed";
		return std::nullopt;
	}
	return loads(content, error);
}

CTLL_EXPORT inline std::optional<value> load(std::istream & stream) {
	load_error ignored;
	return load(stream, ignored);
}

} // namespace ctjson5

#endif
