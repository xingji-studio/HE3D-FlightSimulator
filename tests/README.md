# HE3D Tests

Tests are split by the boundary they protect.

Example programs must use HE3D through public headers only. Do not add relative-path includes from `examples/` into `src/` or other engine-private implementation directories.

`he3d_include_boundaries` runs with `HE3D_BUILD_TESTS=ON` and enforces this rule for `examples/` and `tests/public_api/`.

Build switches:

- `HE3D_BUILD_TESTS=ON` builds public API tests.
- `HE3D_BUILD_INTERNAL_TESTS=ON` additionally enables internal implementation tests.
- The two switches are independent; enabling internal tests must not implicitly enable public API tests.

## `public_api/`

Public API tests verify behavior through `include/` headers. A failure here means an application-facing HE3D contract may have changed or broken.

Public API tests must not use relative-path includes into `src/` or other engine-private implementation directories.

Public API tests may link the built-in console backend as their platform host. They must not include or inspect console backend implementation details.

Public API tests may create small windows with `CreateWindow()` to exercise renderer call chains. They must not assert terminal output bytes, console framebuffer text, or other backend-specific presentation details.

Public API tests may create temporary resource files for loader coverage. Write them to the system temporary directory or the test build directory, not the source tree, and clean them up before the test exits.

## `internal/`

Internal tests verify private implementation rules, helpers, or algorithms. They may include implementation files or access private helpers, but only inside `tests/internal/` and test targets whose names include `internal`. A failure here means an implementation assumption may have changed; it does not automatically define a public API promise.

## `integration/`

Integration tests run example programs or backend paths end-to-end through the public API. They may use dependencies owned by this repository or explicitly configured by its build, but they must not require external sibling projects such as XSWL. Optional system dependencies such as SDL3 may enable integration tests only when CMake finds them; missing optional dependencies must not fail the default test configuration. A failure here means pieces no longer work together in an expected scenario; it is not automatically a single-function public API contract failure.
