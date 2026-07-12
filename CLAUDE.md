# compile-time-json5 (ctjson5)

Compile-time (constexpr) JSON5 parser: `ctjson5::parse<"...">()` turns JSON5 text
into a *type* while the code compiles; malformed input is a compile error (or
`false` from `is_valid`). Structural TWIN of compile-time-json (same layout, same
`load.hpp`) — keep the two in lockstep. Namespace `ctjson5`, umbrella
`include/ctjson5.hpp`. Repo: github.com/alexios-angel/compile-time-json5, work on `main`.

## Build / test (compiling IS the test — every tests/*.cpp is a static_assert suite)
```bash
make CXX=clang++                        # C++20 (default); compiles tests/*.cpp -> .o
make CXX=clang++ CXX_STANDARD=17        # C++17 path
make pch                                # builds the umbrella PCH (auto for tests)
make clean
cmake -B build && cmake --build build && ctest --test-dir build
```
Flags are `-O2 -pedantic -Wall -Wextra -Werror -Wconversion` — code must stay
warning-clean. Constexpr budgets are raised in the Makefile (needed for the Earley
parse); hitting the compiler's own step cap is a distinct failure from the library's
queryable `error_info`/`bind_error`:
```
clang:  -fconstexpr-steps=500000000 -fconstexpr-depth=1024 -fbracket-depth=2048
gcc:    -fconstexpr-ops-limit=3000000000 -fconstexpr-loop-limit=10000000 -fconstexpr-depth=1024
```
CMake attaches them via `CTJSON5_CONSTEXPR_LIMITS` (default ON; opt out `-DCTJSON5_CONSTEXPR_LIMITS=OFF`).
Other CMake knobs: `CTJSON5_PCH`, `CTJSON5_MODULE`, `CTJSON5_BUILD_TESTS`,
`CTJSON5_BUILD_EXAMPLES`, `CTJSON5_CXX_STANDARD` (default 20).

## Layout / key files
- `include/ctjson5.hpp` — umbrella (includes the parts below + `ctlark.hpp`).
- `include/ctjson5/grammar.hpp` — the JSON5 grammar as a **lark grammar STRING** (data, not a generated table).
- `include/ctjson5/types.hpp` — document types (`object/array/string/number/boolean/null`, `kind`).
- `include/ctjson5/bind.hpp` — lowers the ctlark tree into document types; decodes escapes; validates `\u` surrogate pairing (folds into `is_valid`).
- `include/ctjson5/serialize.hpp` — `serialize()` (minified, null-terminated).
- `include/ctjson5/views.hpp` — `value_view` / `member_view` (uniform records for runtime `[]` and range-for).
- `include/ctjson5/dumps.hpp` — Python-style `dumps`/`dump`/`loads` + constexpr Dragon4 float formatting.
- `include/ctjson5/load.hpp` — runtime `loads(text)`/`load(stream)` -> `optional<value>`, `load_error` with line/column.
- `include/ctlark.hpp` + `include/ctlark/`, `include/ctll.hpp` + `include/ctll/` — **VENDORED** (see Gotchas).
- `ctjson5.cppm` — named C++ module (`import std;`, experimental).
- `tests/` — cxx17, document, dumps, load. `examples/` — runnable tours.

## Public API (all in `ctjson5`)
- `is_valid<input>` (bool, never a compile error); `parse<input>()` / `loads<input>()` (typed document; invalid = build error).
- Accessors: `get<"key">()`/`get<N>()`, `[...]`, `contains<>()`, `size()/empty()`, `to<T>()`, `view()/c_str()`; `serialize(doc)`; `for_each(...)`.
- Diagnostics: `error_info<input>()`, `error_message<input>()` (caret snippet), `bind_error<input>()` (reason `bad_surrogate`).
- `debug::` — `traced_parse`, `parse_runtime`, `dump_tokens`, `dump_grammar`. Env: `CTLARK_VERBOSE_ERRORS`, `CTLARK_DEBUG`, `CTLARK_CONSTEXPR_ASSERT`.

## Conventions
- **C++17/20 CNTTP split**: `CTLL_CNTTP_COMPILER_CHECK` gates the `CTJSON5_STRING_INPUT` macro — C++20 passes `ctll::fixed_string` as an NTTP; C++17 passes a `const auto &` to a `constexpr ctll::fixed_string` variable with linkage. Keep both paths compiling (`tests/cxx17.cpp` guards it).
- Prefer ripgrep (`rg`) over grep.

## GOTCHAS (load-bearing)
- **Vendored, do NOT edit directly**: `include/ctlark*` and `include/ctll*` are BYTE-IDENTICAL copies from **compile-time-lark (the source of truth)**. Edit the core THERE, then resync: `../compile-time-lark/tools/sync-vendor.sh`, verify with `diff -rq`, and regenerate the single-header here.
- **single-header**: `make single-header` amalgamates `include/ctjson5.hpp` via `quom` and prepends `LICENSE` -> `single-header/ctjson5.hpp`. Regenerate after core/header changes.
- **Grammar regen (Tablewright)**: `make regrammar` rebuilds `include/ctlark/lark.hpp` (ctlark's *grammar-of-grammars* CTLL table) from `include/ctlark/lark.gram` via the `tablewright` tool (`--generator=cpp_ctll_v2`; needs python3 + the `lark` package). This is a **vendored** file — regenerate upstream in compile-time-lark, not here. The JSON5 grammar itself (`grammar.hpp`) is plain data and needs no regen. Generated header contract: guard `CTLARK__LARK__HPP`, namespace `ctlark`, grammar name `lark_grammar`. Tablewright derives from Desatomat.
- **Attribution**: CTLL is Hana Dusíková's (CTRE, via the notre fork); the Lark grammar language is lark-parser's (Erez Shinan). Preserve `NOTICE`/`LICENSE` (Apache-2.0 w/ LLVM Exceptions).
