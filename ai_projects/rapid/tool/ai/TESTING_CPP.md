# Rapid — C++ Testing Guide

## Naming Conventions

Tests use Google Test and follow the `.t.cpp` naming convention. Test files are placed in `test/` subdirectories alongside source code.

- `*.t.cpp` — Regular unit tests
- `*.perf.t.cpp` — Performance tests
- `*.db.t.cpp` — Database integration tests
- `*.network.t.cpp` — Network integration tests
- `*.manual.t.cpp` — Manual tests (not run in CI)
- `*.fuzz.t.cpp` — Fuzz tests

## Test Structure

```cpp
#include <gtest/gtest.h>
#include <rapid/engine/Engine.h>
#include <rapid/test_common/ModuleContextMocks.h>

#include <my_component/MyModule.h>
#include <my_component/MyEvent.h>

using namespace my_component;

namespace {

TEST(MyModuleTest, HandlesEvent) {
  // Setup
  rapid::test::ModuleContextMock context;
  auto module = std::make_unique<MyModule>(context, "test_instance");

  // Create and send event
  auto event = MyModule::get_event<MyEvent>();
  event->field1 = 42;
  module->add_event(event.release());

  // Process
  module->run();

  // Verify
  EXPECT_EQ(module->stats().event_count, 1);
}

} // namespace
```

## Running Tests

```bash
# All non-performance tests
cmake --build . -- testnp

# Performance tests (excluded from regular CI)
cmake --build . -- testp

# Specific test by name pattern
ctest --verbose -R MyTest

# Specific test file
ctest --verbose -R SessionModule.t
```

## Code Coverage

```bash
cmake -DCMAKE_BUILD_TYPE=DebWithCoverage ..
cmake --build . -j<n>
cmake --build . -- testnp
cmake --build . -- coverage
# Results in coverage_reports/ (project root)
```
