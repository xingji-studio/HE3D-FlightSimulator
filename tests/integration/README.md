# Integration Tests

Put end-to-end example and backend-path tests here. These tests should observe HE3D through executable behavior and public APIs, not private engine state.

Do not depend on external sibling projects such as XSWL. If a scenario needs XSWL, keep it as a local manual debugging note instead of a HE3D test.

SDL3-based integration tests are allowed only as optional tests registered when CMake finds SDL3. A machine without SDL3 should still be able to configure and run the non-SDL3 test suite.
