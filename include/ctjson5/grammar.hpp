#ifndef CTJSON5__GRAMMAR__HPP
#define CTJSON5__GRAMMAR__HPP

#include "../ctlark.hpp"

// The grammar layer: JSON5 (https://json5.org) written in lark's
// grammar language and parsed by ctlark. On top of RFC 8259 JSON this
// accepts, per the spec: // and /* */ comments (ignored like
// whitespace), single-quoted strings, \' \v \0 \xHH escapes with any
// other escaped character standing for itself, line continuations,
// unquoted ASCII identifier keys (keyword spellings included - the
// contextual lexer sees an identifier where a key is expected),
// trailing commas, hexadecimal numbers, leading/trailing decimal
// points, an explicit plus sign, Infinity and NaN with either sign,
// and the JSON5 whitespace set including NBSP, BOM, LS and PS as
// their UTF-8 byte sequences.
//
// Known divergences, both permissive and restrictive, documented in
// the README: unquoted keys are ASCII-only (the spec allows full
// ECMAScript identifiers), whitespace outside the list above (other
// Zs characters) is not recognized, \0 is accepted even when a digit
// follows, and LS/PS do not terminate // comments (LF and CR do).
// Surrogate pairing in \uXXXX - inexpressible in a regular terminal -
// is validated by the binder (bind.hpp).

namespace ctjson5::detail {

inline constexpr ctll::fixed_string json5_grammar = R"x(
?value: object
      | array
      | STRING -> string
      | NUMBER -> number
      | "true"  -> true
      | "false" -> false
      | "null"  -> null

object: "{" [pair ("," pair)* [","]] "}"
pair: (STRING | IDENT) ":" value
array: "[" [value ("," value)* [","]] "]"

STRING: /"([^"\\\x0a\x0d]|\\(u[0-9a-fA-F]{4}|x[0-9a-fA-F]{2}|\x0d\x0a|[^1-9ux]))*"/
      | /'([^'\\\x0a\x0d]|\\(u[0-9a-fA-F]{4}|x[0-9a-fA-F]{2}|\x0d\x0a|[^1-9ux]))*'/
NUMBER: /[+-]?(0[xX][0-9a-fA-F]+|((0|[1-9][0-9]*)(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?|Infinity|NaN)/
IDENT: /[A-Za-z_$][A-Za-z0-9_$]*/

%ignore /([ \x09\x0a\x0b\x0c\x0d]|\xc2\xa0|\xef\xbb\xbf|\xe2\x80[\xa8\xa9])+/
%ignore /\/\/[^\x0a\x0d]*/
%ignore /\/\*([^*]|\*+[^*\/])*\*+\//
)x";

inline constexpr ctll::fixed_string json5_start = "value";

static_assert(ctlark::grammar_valid<json5_grammar>,
              "ctjson5: internal error - the JSON5 grammar failed to compile");

} // namespace ctjson5::detail

#endif
