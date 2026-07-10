> **Fork notice:** this repository is a fork of
> [compile-time-json](https://github.com/alexios-angel/compile-time-json)
> (full history preserved), being rewritten for
> [JSON5](https://json5.org/). Built on the CTLL compile-time LL(1)
> parser from [CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
> by Hana Dusíková, via the [notre](https://github.com/alexios-angel/notre)
> fork. Apache License 2.0 with LLVM Exceptions; see [NOTICE](NOTICE).

# ctjson5 — compile-time JSON5

[JSON5](https://json5.org/) parsed while your code compiles. The document
is a *type*: malformed JSON5 is a compile error, lookups are resolved at
compile time, and every accessor is `constexpr` — usable in
`static_assert`, as template arguments, or at runtime with zero parsing
cost. Every JSON document is also a JSON5 document, so this is a strict
superset of the JSON library it forked from.

```c++
#include <ctjson5.hpp>

constexpr auto doc = ctjson5::parse<R"({
    // JSON5: comments, unquoted keys, single quotes, trailing commas
    name:  'Hana',
    stars: 4700,
    ratio: -.25,
    flags: 0x2A,
    tags:  ['regex', 'compile-time',],
    extra: {active: true, parent: null},
})">();

static_assert(doc.get<"name">() == "Hana");
static_assert(doc.get<"stars">().to<int>() == 4700);
static_assert(doc.get<"ratio">().to<double>() == -0.25);
static_assert(doc.get<"flags">().to<int>() == 42);
static_assert(doc.get<"tags">().get<1>() == "compile-time");
static_assert(doc.get<"extra">().get<"active">());
static_assert(!doc.contains<"missing">());

static_assert(ctjson5::is_valid<"[1, 2, 3,] // fine">);
static_assert(!ctjson5::is_valid<"[01]">);   // still not lawless
```

## API

```c++
// validity as a bool (never a compile error):
template <ctll::fixed_string input> constexpr bool ctjson5::is_valid;

// the parsed document; invalid JSON fails the build:
template <ctll::fixed_string input> constexpr auto ctjson5::parse();
```

`parse` returns one of the document types, all in namespace `ctjson5`:

| Type | Accessors |
|------|-----------|
| `object<members...>` | `get<"key">()`, `contains<"key">()`, `size()`, `empty()`, positional `key<N>()` / `value<N>()` |
| `array<values...>` | `get<N>()`, `size()`, `empty()` |
| `string<chars...>` | `view()`, `c_str()` (null-terminated), `size()`, `empty()`, `==` with `std::string_view` |
| `number<chars...>` | `to<T>()` for any arithmetic `T`, `is_integer()`, `view()` (raw spelling), `c_str()` |
| `boolean<B>` | `value`, `operator bool` |
| `null` | — |

Every type carries `static constexpr ctjson5::kind type` for
introspection (`kind::object`, `kind::array`, ...).

Two free functions round out the API:

```c++
// render any document value back to minified JSON, in static storage:
static_assert(ctjson5::serialize(ctjson5::parse<R"({ "a" : [ 1, 2 ] })">()) == R"({"a":[1,2]})");

// compile-time iteration (elements, or key/value pairs):
ctjson5::for_each(doc.get<"tags">(), [](auto value) { /* each has its own type */ });
ctjson5::for_each(doc, [](auto key, auto value) { ... });
```

`serialize` re-emits strings with the mandatory escapes and numbers with
the spelling they were parsed with; the result is null-terminated.

## Python-style runtime API

`ctjson5::dumps` is `json.dumps` for ordinary C++ values — and it is
`constexpr`, so whole encodings can be `static_assert`ed (needs C++20's
constexpr `std::string`/`std::vector`; `dump` to a stream is inherently
runtime). Floats are rendered by a built-in constexpr Dragon4 (shortest
round-tripping digits via exact big-integer arithmetic) with Python
repr's formatting rule, and the output is verified byte-identical
against CPython across 100,000 fuzzed doubles plus every edge case:

```c++
static_assert(ctjson5::dumps(std::vector<int>{1, 2, 3}) == "[1, 2, 3]");
static_assert(ctjson5::dumps(0.1) == "0.1");
static_assert(ctjson5::dumps(1e16) == "1e+16");     // Python's sci threshold
static_assert(ctjson5::dumps(5e-324) == "5e-324");  // denormals exact
```

```c++
ctjson5::dumps(std::map<std::string, std::vector<int>>{{"a", {1, 2}}});
//  -> {"a": [1, 2]}                        (Python's separators)
ctjson5::dumps(value, 2);                    // indent=2 pretty printing
ctjson5::dumps(value, {.indent = 2, .sort_keys = true, .ensure_ascii = true});
ctjson5::dump(value, stream);                // json.dump, onto a std::ostream
ctjson5::loads<"[1, 2]">();                  // parse, under its Python name
```

The encoder accepts what Python's does: map-likes become objects
(arithmetic keys are quoted, `{1: "x"}` style), iterables plus
`std::pair`/`std::tuple` become arrays, string-likes and `char` become
strings, integral and floating-point values become numbers (floats stay
visibly floats: `1.0`, and NaN/Infinity render like Python's
`allow_nan=True`), `nullptr` and empty `std::optional` become `null`,
`std::variant` dumps its active alternative, and parsed ctjson
documents dump directly (numbers keeping their parsed spelling). For
anything else, define a `to_json(const T &)` findable by ADL returning
something dumpable — the `default=` hook. One deliberate divergence:
`ensure_ascii` defaults to **false** (UTF-8 passes through); switch it
on for Python's `\uXXXX` output, surrogate pairs included.

What JSON5 adds over JSON, all supported here at compile time and at
runtime:

* `//` line and `/* block */` comments
* unquoted object keys (`{key: 1}`) and single-quoted strings/keys
* trailing commas in objects and arrays
* hexadecimal numbers (`0x2A`), leading/trailing decimal points (`.5`,
  `5.`), an explicit plus sign, and `Infinity`/`NaN` with either sign
* string escapes `\'`, `\v`, `\0`, `\xHH`, self-escapes (`\a` is
  `a`), and line continuations (backslash before LF, CR, CRLF, LS, PS)
* extra whitespace: `\v`, `\f`, NBSP, BOM, LS and PS

Details:

* String content is stored as UTF-8 bytes. All escapes are decoded at
  parse time, including `\uXXXX` and UTF-16 surrogate pairs; lone
  surrogates are rejected.
* Numbers keep their raw spelling — `0x2A` stays hexadecimal through
  `serialize` — and `to<T>()` converts on demand (integral conversions
  truncate fractions, like a cast; `is_integer()` is true for hex).
* Still rejected, per the spec: leading zeros, digits after a
  backslash, a lone `/`, unterminated block comments, raw line
  terminators inside strings, and anything after the root value.
  Duplicate keys are accepted; `get` finds the first.
* Documented divergences from the spec: unquoted keys are ASCII
  identifiers (`[A-Za-z_$][A-Za-z0-9_$]*`; full ECMAScript identifiers
  are not recognized), whitespace outside the list above (other `Zs`
  characters) is not, and `\0` is accepted even when a digit follows.

## C++17

With a pre-C++20 compiler, inputs and keys are `constexpr
ctll::fixed_string` variables with linkage instead of string literals:

```c++
static constexpr auto text = ctll::fixed_string{R"({"n":42})"};
static constexpr ctll::fixed_string n_key = "n";

constexpr auto doc = ctjson5::parse<text>();
static_assert(doc.template get<n_key>().template to<int>() == 42);
```

## Runtime parsing

`ctjson5::loads(text)` and `ctjson5::load(stream)` are `json.loads`/`json.load`
for JSON5 text that only exists at runtime. They return
`std::optional<ctjson5::value>` — a dynamic document mirroring the
compile-time accessors — with `std::nullopt` (plus a byte offset through
the `load_error` overloads) on malformed input; the library stays
exception free:

```c++
if (auto doc = ctjson5::loads(request_body)) {
    (*doc)["users"][0]["name"].view();   // chains are null-safe
    (*doc)["retries"].to<int>();
    ctjson5::dumps(*doc, 2);              // the encoder accepts values
}
```

Numbers keep Python's int/float distinction (`is_integer()`), `NaN` and
the infinities parse like Python's `parse_constant`, and missing lookups
follow the null-object pattern (`find`/`contains` distinguish absent
from null). Documented divergences from Python: integers beyond
`long long` become doubles, duplicate keys are all kept (first wins,
like the compile-time parser), and lone `\u` surrogates are rejected.

## How it works

The same architecture as CTRE: a JSON5 grammar
([`json5.gram`](include/ctjson5/json5.gram)) is compiled by
[Tablewright](https://github.com/alexios-angel/Tablewright) into an
LL(1) parse table of `rule()` overloads
([`json5.hpp`](include/ctjson5/json5.hpp)), which CTLL — the compile-time
LL parser from CTRE — walks character by character. Semantic actions
([`actions.hpp`](include/ctjson5/actions.hpp)) build the document on a
type stack: strings and numbers accumulate as they are read, objects
and arrays collect their content when they close.

Regenerate the table after editing the grammar with `make regrammar`.

## Building and integrating

Header-only. Pick whichever fits your project:

**CMake, as a subdirectory or via FetchContent:**

```cmake
add_subdirectory(compile-time-json5)   # or FetchContent_MakeAvailable(ctjson5)
target_link_libraries(your-target PRIVATE ctjson5::ctjson5)
```

**CMake, installed** (`cmake -B build && cmake --install build`):

```cmake
find_package(ctjson5 0.1 REQUIRED)
target_link_libraries(your-target PRIVATE ctjson5::ctjson5)
```

The install also ships a `pkg-config` file (`ctjson5.pc`). Tests and
examples build only when ctjson5 is the top-level project
(`CTJSON5_BUILD_TESTS`, `CTJSON5_BUILD_EXAMPLES`); `CTJSON5_CXX_STANDARD`
selects the advertised standard (default 20). CPack can produce
TGZ/ZIP archives (plus DEB/RPM where the tooling exists), and
`-DCTJSON5_MODULE=ON` builds `ctjson5.cppm` as a named C++ module
(experimental; needs CMake 3.30+, a modules-capable toolchain and
`import std`).

**No build system:** add `include/` to your include path, or copy the
amalgamated [`single-header/ctjson5.hpp`](single-header/ctjson5.hpp)
(regenerate with `make single-header`, which needs the
[quom](https://pypi.org/project/quom/) tool).

Requires C++17 (C++20 for the string-literal API). Runnable demos live
in [`examples/`](examples/).

Run the tests (compilation is the test — the suite is `static_assert`s):

```bash
make CXX=clang++                       # C++20
make CXX=clang++ CXX_STANDARD=17
# or through CMake/CTest:
cmake -B build && cmake --build build && ctest --test-dir build
```

## License

Apache License 2.0 with LLVM Exceptions (see [LICENSE](LICENSE)).
The CTLL parser is Hana Dusíková's work, via notre; see
[NOTICE](NOTICE).
