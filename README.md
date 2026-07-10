> **Attribution:** this library is built on the CTLL compile-time LL(1)
> parser from [CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
> by Hana Dusíková, via the [notre](https://github.com/alexios-angel/notre)
> fork. Apache License 2.0 with LLVM Exceptions; see [NOTICE](NOTICE).

# ctjson — compile-time JSON

JSON parsed while your code compiles. The document is a *type*: malformed
JSON is a compile error, lookups are resolved at compile time, and every
accessor is `constexpr` — usable in `static_assert`, as template
arguments, or at runtime with zero parsing cost.

```c++
#include <ctjson.hpp>

constexpr auto doc = ctjson::parse<R"({
    "name":  "Hana",
    "stars": 4700,
    "ratio": -2.5e-1,
    "tags":  ["regex", "compile-time"],
    "extra": {"active": true, "parent": null}
})">();

static_assert(doc.get<"name">() == "Hana");
static_assert(doc.get<"stars">().to<int>() == 4700);
static_assert(doc.get<"ratio">().to<double>() == -0.25);
static_assert(doc.get<"tags">().get<1>() == "compile-time");
static_assert(doc.get<"extra">().get<"active">());
static_assert(!doc.contains<"missing">());

static_assert(ctjson::is_valid<"[1, 2, 3]">);
static_assert(!ctjson::is_valid<"[1, 2,]">);   // RFC 8259, strictly
```

## API

```c++
// validity as a bool (never a compile error):
template <ctll::fixed_string input> constexpr bool ctjson::is_valid;

// the parsed document; invalid JSON fails the build:
template <ctll::fixed_string input> constexpr auto ctjson::parse();
```

`parse` returns one of the document types, all in namespace `ctjson`:

| Type | Accessors |
|------|-----------|
| `object<members...>` | `get<"key">()`, `contains<"key">()`, `size()`, `empty()`, positional `key<N>()` / `value<N>()` |
| `array<values...>` | `get<N>()`, `size()`, `empty()` |
| `string<chars...>` | `view()`, `size()`, `empty()`, `==` with `std::string_view` |
| `number<chars...>` | `to<T>()` for any arithmetic `T`, `is_integer()`, `view()` (raw spelling) |
| `boolean<B>` | `value`, `operator bool` |
| `null` | — |

Every type carries `static constexpr ctjson::kind type` for
introspection (`kind::object`, `kind::array`, ...).

Details:

* String content is stored as UTF-8 bytes. All escapes are decoded at
  parse time, including `\uXXXX` and UTF-16 surrogate pairs
  (`"😀"` becomes the four UTF-8 bytes of 😀); lone
  surrogates are rejected.
* Numbers keep their raw spelling; `to<T>()` converts on demand
  (integral conversions truncate fractions, like a cast).
* Parsing is strict RFC 8259: no trailing commas, no leading zeros, no
  unquoted keys, no `'` strings, nothing after the root value.
  Duplicate keys are accepted (the RFC allows them); `get` finds the
  first.

## C++17

With a pre-C++20 compiler, inputs and keys are `constexpr
ctll::fixed_string` variables with linkage instead of string literals:

```c++
static constexpr auto text = ctll::fixed_string{R"({"n":42})"};
static constexpr ctll::fixed_string n_key = "n";

constexpr auto doc = ctjson::parse<text>();
static_assert(doc.template get<n_key>().template to<int>() == 42);
```

## How it works

The same architecture as CTRE: an RFC 8259 grammar
([`json.gram`](include/ctjson/json.gram)) is compiled by
[Tablewright](https://github.com/alexios-angel/Tablewright) into an
LL(1) parse table of `rule()` overloads
([`json.hpp`](include/ctjson/json.hpp)), which CTLL — the compile-time
LL parser from CTRE — walks character by character. Semantic actions
([`actions.hpp`](include/ctjson/actions.hpp)) build the document on a
type stack: strings and numbers accumulate as they are read, objects
and arrays collect their content when they close.

Regenerate the table after editing the grammar with `make regrammar`.

## Building

Header-only: add `include/` to your include path and `#include
<ctjson.hpp>` — or copy the amalgamated
[`single-header/ctjson.hpp`](single-header/ctjson.hpp) into your
project (regenerate it with `make single-header`, which needs the
[quom](https://pypi.org/project/quom/) tool). Requires C++17 (C++20 for
the string-literal API). Runnable demos live in
[`examples/`](examples/).

Run the tests (compilation is the test — the suite is `static_assert`s):

```bash
make CXX=clang++             # C++20
make CXX=clang++ CXX_STANDARD=17
```

## License

Apache License 2.0 with LLVM Exceptions (see [LICENSE](LICENSE)).
The CTLL parser is Hana Dusíková's work, via notre; see
[NOTICE](NOTICE).
