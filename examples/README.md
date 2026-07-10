# Examples

Self-contained programs, each compilable against `../include` (or the
single header). Build with `make`, build and run with `make run`.

| File | Shows |
|------|-------|
| [`config.cpp`](config.cpp) | a configuration baked into the binary: compile-time requirement checks, values as array sizes, zero-cost runtime reads |
| [`validation.cpp`](validation.cpp) | `is_valid` as a bool, strict RFC 8259 rejections, schema-style shape asserts with `kind` |
| [`introspection.cpp`](introspection.cpp) | a generic recursive visitor over any document: positional iteration, kind dispatch, re-serialization |
