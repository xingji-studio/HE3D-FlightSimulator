# HE3D Context

HE3D is a C++11 software 3D renderer with platform backends and example programs. This glossary defines the language used for public API documentation, tests, and example boundaries.

## Language

**Public API Contract**:
The behavior exposed through HE3D public headers, especially `include/he3d.hpp`, `include/he3d_math.h`, and `include/he3d_platform.hpp`. Public API contracts are what the technical manual documents and what regression tests should verify.
_Avoid_: implementation detail, sample behavior

**Example Program**:
A program under `examples/` that demonstrates HE3D usage only through the public API contract. An example must not depend on engine-private functions, private renderer state, backend internals, or relative-path includes into engine implementation directories.
_Avoid_: privileged sample, internal demo

**Public API Test**:
A regression test that verifies behavior through HE3D public headers and treats the engine implementation as replaceable. Public API tests define compatibility expectations for applications and examples, and must not use relative-path includes into engine implementation directories.
_Avoid_: black-box-ish internal test, implementation probe

**Console Test Host**:
The built-in console backend used as the default platform implementation for public API tests. Public API tests may link it to satisfy platform services, but must still interact with HE3D only through public headers.
_Avoid_: console backend internal test, SDL test host

**Test Window**:
A small window created through `CreateWindow()` inside a public API test to exercise renderer-facing contracts. Public API tests may use test windows, but must not assert terminal framebuffer text or other backend-specific presentation details.
_Avoid_: backend output assertion, console rendering fixture

**Test Resource File**:
A temporary resource file created by a test to exercise file loading, mesh loading, or texture loading contracts. Test resource files must be written to the system temporary directory or the test build directory, never to the source tree, and should be removed by the test.
_Avoid_: checked-in fixture churn, source-tree temp file

**Internal Test**:
A regression test that targets a private implementation rule, helper, or algorithm inside HE3D. Internal tests may include implementation files or access private helpers, but only inside `tests/internal/` and internal test targets; they must be clearly separated from public API tests so they do not accidentally define application-facing compatibility.
_Avoid_: public contract test, example behavior test

**Internal Test Build**:
The optional CMake test configuration enabled by `HE3D_BUILD_INTERNAL_TESTS`. It is separate from `HE3D_BUILD_TESTS`, which builds public API tests, and enabling it must not automatically enable public API tests.
_Avoid_: default test suite, public API test build

**Integration Test**:
A regression test that runs an example program or backend path end-to-end through the public API contract using dependencies owned by this repository or explicitly configured by its build. Integration tests verify that HE3D pieces work together, but they must not require external sibling projects such as XSWL; optional system dependencies such as SDL3 may only enable integration tests when CMake finds them.
_Avoid_: public API unit test, internal implementation test

**Developer Note**:
An internal project note for debugging lessons, implementation constraints, and internal test rules that are useful to HE3D maintainers but are not part of the public API contract. Developer notes can mention internals that the technical manual must not promise to users.
_Avoid_: public manual, API reference
