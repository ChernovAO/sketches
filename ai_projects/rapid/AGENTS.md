# Rapid Project - Model Instructions

This document provides guidance for AI models working with the Rapid project C++ codebase.

## 1. Planning Phase (Crucial)
Before writing any code or modifying files, you must thoroughly understand the task.

* **The 90% Confidence Rule:** Do not start implementation if you have doubts.
* **Ask Questions:** Proactively ask the user clarifying questions.
* **Keep Querying:** Continue asking questions until the requirements and scope are **at least 90% clear** to you.
* **Define Constraints:** Confirm expected behavior, edge cases, and tech stack limitations during this phase.

## 2. Implementation Rules
Once the planning phase is complete and confidence is above 90%, proceed with execution:

* **Step-by-Step Execution:** Break down the solution into small, logical steps.
* **Code Quality:** Write clean, readable, and well-documented code.
* **Self-Correction:** If an error occurs, analyze the logs, fix the root cause, and verify the solution.

## 3. Communication Style
* Be concise, direct, and polite.
* When asking questions, list them clearly (e.g., using bullet points).
* Explain *why* you are asking if it helps the user provide a better answer.

## 4. Task Skills

OpenCode and Kilo Code load shared skills through `skills.paths` in their project configs.
Start either tool from the Rapid project root so `tool/ai/skills` resolves correctly.

For C++ precommit review, load `rapid-cpp-precommit-review` from `tool/ai/skills/rapid-cpp-precommit-review/SKILL.md`.
Review staged changes only. If the entire stage is empty, review unstaged changes and untracked C++ files instead.
This is a read-only review, not an instruction to modify files or create a commit.

Run `/precommit-review` in either tool, optionally followed by a review focus (for example, `/precommit-review event ownership`).
Both project configs load the shared command template from `tool/ai/commands/precommit-review.md`.

---

## Project Overview

Rapid is a high-performance C++ trading system framework developed by Moscow Exchange (MOEX). It features:

- **Event-driven architecture** with custom event pool management
- **Module-based design** where each module processes specific event types
- **Low-latency messaging** using PGM/LLM transports
- **Order matching, risk management, and market data** functionality

## Code Organization

### Directory Structure

```
rapid/                          # Core framework code
├── include/rapid/              # Public headers
│   ├── engine/                 # Core engine (Module, Engine, PubSub)
│   ├── event/                  # Event system (Event, EventPool, EventPtr)
│   ├── sync/                   # Synchronization primitives (MpscEventQueue, SpinLock)
│   ├── logging/                # Logging infrastructure
│   ├── proto/                  # Protocol buffer utilities
│   ├── mq/                     # Message queue (PGM, LLM)
│   └── ...
├── src/                        # Implementation files

component/                      # Business logic components
├── light_gateway/              # Client gateway
├── matcher/                    # Order matching
├── twime/                      # TWIME protocol
├── dispatcher/                 # Event dispatching
└── ...

app/                            # Applications
├── pingpong/                   # Test application
├── light_gateway/              # Gateway application
└── ...

rapid_testkit/                  # Python test automation (not covered here)
```

### Key Architectural Patterns

#### Module Pattern

All business logic is implemented as modules derived from `rapid::Module`:

```cpp
class MyModule : public rapid::Module {
public:
  MyModule(ModuleContext& context, std::string_view definition, size_t prealloc_size = 128)
    : Module(context, inst, prealloc_size) {
    set_type("MyModule");
  }

  bool match_event(const Event* event) const override {
    return event->get_event_type_id() == MyEvent::event_type_id;
  }

protected:
  void handle_event(const Event* event) override {
    const auto* my_event = event->as<MyEvent>();
    // Process event
    send_event(response_event);
  }

  void flush() override {
    // Batch flush logic (optional)
  }
};
```

**Key points:**
- Modules receive events via `handle_event()`
- Use `match_event()` to filter events (optional, returns `true` by default)
- Send events using `send_event()` - uses move semantics
- Call `repeat()` to schedule another processing iteration
- Use `MLOG_*` macros for logging (automatically include module description)

#### Event System

Events are reference-counted objects managed by event pools:

```cpp
// Get event from pool
auto event = Module::get_event<MyEvent>();

// Set fields
event->field1 = value;
event->field2 = value2;

// Send (moves ownership)
send_event(event);

// For const references (read-only)
void handle_event(const Event* event) {
  const auto* typed = event->as<MyEvent>();
  // Cannot modify - const reference
}
```

**Event pool management:**
- Events are allocated from thread-local pools
- Reference counting manages lifetime (`add_ref()`, `release()`)
- Events return to pool when `use_count_` reaches zero
- Use `EventPtr` (movable) and `ConstEventPtr` (non-owning view)

#### Concurrency Model

- **MPSC queues**: Multi-Producer Single-Consumer for event delivery
- **Spin locks**: For short critical sections
- **Thread-local storage**: Event pools and thread-specific data
- **Atomic operations**: For lock-free data structures

```cpp
// MpscEventQueue usage (in Module base class)
MpscEventQueue queue_;  // Writers push, single reader pops

// Thread-specific data
thread_local ThreadSpecific specific;

// Atomic counters
std::atomic<size_t> counter_{0};
```

## Coding Standards

### Formatting

Use `.clang-format` (Google style with modifications):
- **Indent width**: 2 spaces
- **Column limit**: 120
- **Pointer alignment**: Left (`int* ptr`)
- **Braces**: Required (InsertBraces: true)

Format with:
```bash
clang-format -i <file>
```

### Naming Conventions

```cpp
// Classes: PascalCase
class MyModule;

// Functions: camelCase
void handleEvent();

// Members: trailing underscore
size_t event_count_;
std::string instance_;

// Constants: kPascalCase or UPPER_CASE
constexpr size_t kMaxEvents = 1000;

// Templates: PascalCase with descriptive names
template<typename EventType>
```

### Include Order

```cpp
// 1. Module header (for .cpp files)
#include <rapid/engine/Module.h>

// 2. Rapid project headers (sorted)
#include <rapid/event/Event.h>
#include <rapid/logging/Logging.h>

// 3. Generated headers
#include <mustang/generated/Equities_ORDER.h>

// 4. Third-party headers
#include <boost/uuid/uuid.hpp>
#include <gtest/gtest.h>

// 5. Standard library
#include <string>
#include <vector>
```

## Common Tasks

### Adding a New Module

1. Create header in `component/<name>/include/<name>/MyModule.h`:
```cpp
#pragma once

#include <rapid/engine/Module.h>
#include <rapid/event/Event.h>

namespace my_component {

class MyModule : public rapid::Module {
public:
  MyModule(rapid::ModuleContext& context, std::string_view definition, size_t prealloc_size = 128)
    : rapid::Module(context, inst, prealloc_size) {
    set_type("MyModule");
  }

  bool match_event(const Event* event) const override;

protected:
  void handle_event(const Event* event) override;
  void flush() override;
};

} // namespace my_component
```

2. Create implementation in `component/<name>/src/MyModule.cpp`

3. Add `CMakeLists.txt` using the `rapid_module()` macro (auto-discovers `src/`, `test/`, `include/`, `example/`, `datamodel/`):
```cmake
rapid_module(my_component)
rapid_link_later(my_component rapid)
```

### Adding a New Event Type

1. Define event in header:
```cpp
#pragma once

#include <rapid/event/Event.h>
#include <rapid/event/EventTypeId.h>

namespace my_component {

struct MyEvent : public rapid::TypableEvent<EventTypeId::MyEvent> {
  int32_t field1 = 0;
  std::string field2;

  void clear() override {
    Event::clear();
    field1 = 0;
    field2.clear();
  }

  std::ostream& print(std::ostream& out) const override {
    return out << "MyEvent{field1=" << field1 << ", field2=" << field2 << "}";
  }
};

} // namespace my_component
```

2. Add EventTypeId to `rapid/event/EventTypeId.h`

3. Register in event registry if needed

### Writing Tests

Tests use Google Test and follow the `.t.cpp` naming convention. Test files are placed in `test/` subdirectories alongside source code.

**Test naming:**
- `*.t.cpp` - Regular unit tests
- `*.perf.t.cpp` - Performance tests
- `*.db.t.cpp` - Database integration tests
- `*.network.t.cpp` - Network integration tests
- `*.manual.t.cpp` - Manual tests (not run in CI)
- `*.fuzz.t.cpp` - Fuzz tests

**Test structure:**
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

**Running tests:**
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

### Logging

Use the `MLOG_*` macros in modules (automatically include module description):

```cpp
MLOG_DEBUG("Processing event: {}", event);
MLOG_INFO("Module started");
MLOG_WARN("Queue time is too long: {}", time::Duration(queue_time));
MLOG_ERROR("Failed to process: {}", error_message);
```

For non-module code, use `LOG_*` macros from `rapid/logging/Logging.h`.

## Build System

### CMake Configuration

```bash
# Debug build
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j<n>

# Release build with extra optimizations
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DWITH_EXTRA_OPTIMIZATION=ON ..
```

### Environment Variables

Required for build:
- `TKS_3RDPARTY_NEXUS_USERNAME` - Nexus repository username
- `TKS_3RDPARTY_NEXUS_PASSWORD` - Nexus repository password

Optional:
- `CMAKE_BUILD_TYPE` - Debug, Release, RelWithDebInfo, MinSizeRel
- `PVS_REG_NAME`, `PVS_REG_KEY` - PVS Studio license (for static analysis)

### Running Tests

```bash
# All non-performance tests
cmake --build . -- testnp

# Performance tests
cmake --build . -- testp

# Specific test
ctest --verbose -R MyTest
```

### Code Coverage

```bash
cmake -DCMAKE_BUILD_TYPE=DebWithCoverage ..
cmake --build . -j<n>
cmake --build . -- testnp
cmake --build . -- coverage
# Results in coverage_reports/ (project root)
```

## Performance Considerations

1. **Avoid allocations in hot paths**: Use pre-allocated buffers, object pools
2. **Minimize locking**: Use lock-free structures, thread-local storage
3. **Batch operations**: Process multiple events per `run()` iteration
4. **Cache-friendly layouts**: Use `Batch`, `Chunk` structures for sequential access
5. **Move semantics**: Always use `std::move()` for EventPtr transfers

## Common Pitfalls

### Event Ownership

```cpp
// WRONG - event may be deleted after send_event
auto event = get_event<MyEvent>();
send_event(event);
event->field = value;  // UB!

// CORRECT - move semantics
auto event = get_event<MyEvent>();
event->field = value;
send_event(event);  // event is null after this
```

### Module Thread Safety

Modules are not thread-safe, but the Engine ensures that module code executes on one thread at a time. Therefore, developers should not use `SpinLock` or atomic operations for shared state unless there is a specific necessity (e.g., sharing state across multiple modules or with external threads):

```cpp
class MyModule : public Module {
  // No SpinLock needed for internal state - Engine guarantees single-threaded execution
  void handle_event(const Event* event) override {
    // Safe to access member variables without locking
    counter_++;
  }

  // Only use SpinLock when sharing state with other threads
  // SpinLock mutex_;  // Avoid unless truly necessary
};
```

### Event Queue Management

- Set appropriate `batch_limit_` to control events per iteration
- Monitor `queue_time()` for latency issues
- Use `repeat()` for iterative processing instead of long loops

## Tools and Utilities

### clang-format

```bash
# Format specific file
clang-format -i file.cpp

# Check formatting
clang-format --dry-run --Werror file.cpp
```

### Git Hooks

Install pre-push hooks for quality checks:
```bash
tool/git-hooks/set-git-hooks-directory.sh
```

### Wireshark Dissector

Custom PGM/LLM protocol dissector located in `tool/wireshark/` for packet analysis.

## C++ Specific Guidelines

### Memory Management

**Event pools:**
- Events are allocated from thread-local pools, not heap
- Use `Module::get_event<T>()` for allocation
- Never use `new` for events
- Events auto-return to pool when reference count reaches zero

**Smart pointers:**
- Use `EventPtr` for owning event references (movable only)
- Use `ConstEventPtr` for non-owning views
- Use `std::unique_ptr` for regular objects
- Avoid `std::shared_ptr` in hot paths

### Move Semantics

The framework heavily uses move semantics for efficiency:

```cpp
// Correct: event is moved into send_event
auto event = get_event<MyEvent>();
event->value = 42;
send_event(event);  // event is null after this

// Correct: explicit move for parameters
void process(EventPtr&& event) {
  handle(std::move(event));
}

// Avoid: copying EventPtr (may not compile)
EventPtr copy = original;  // Usually deleted
```

### Const Correctness

Event handlers receive `const Event*` - events are immutable during processing:

```cpp
void handle_event(const Event* event) override {
  const auto* typed = event->as<MyEvent>();
  // typed is const - cannot modify fields
  
  // To create a modified copy, allocate new event:
  auto response = get_event<ResponseEvent>();
  response->correlation_id = typed->id;
  send_event(response);
}
```

### C++ Standard

The project uses **C++23** standard. Key features utilized:

- Concepts and constraints (`std::derived_from`, `requires` clauses)
- `std::span` for non-owning views
- `std::format` (where available)
- Structured bindings
- `std::optional`, `std::variant`, `std::any`
- Coroutines (in newer code)
- `std::jthread` for RAII thread management

### Template Metaprogramming

The framework uses C++20/23 features:

```cpp
// Concept-based constraints (C++20)
template<std::derived_from<Event> EventType>
EventType* as();

// constexpr if with requires expressions
if constexpr (requires { config->batch_limit; }) {
  set_batch_limit(config->batch_limit);
}

// TypableEvent CRTP pattern
struct MyEvent : public TypableEvent<EventTypeId::MyEvent> {
  static constexpr EventTypeId event_type_id = EventTypeId::MyEvent;
  inline static EventRegId event_reg_id = register_event_id(event_type_id);
};
```

### Zero-Copy Design

Key patterns for low-latency:

```cpp
// Pre-allocated buffers
std::array<char, 4096> buffer_;

// String views instead of string copies
void process(std::string_view data) {
  // No allocation
}

// In-place construction
events_.emplace_back(args...);

// Batch processing to amortize overhead
void run() {
  while (pop_event(event_)) {
    handle_event(event_);  // Process multiple events per run
  }
  flush();  // Batch flush at end
}
```

## Common Framework Components

### PubSub (Publish-Subscribe)

```cpp
// Subscribe to event type
pubsub().subscribe<MyEvent>(module, [](const MyEvent* event) {
  // Handle event
});

// Publish event
pubsub().publish(std::move(event));
```

### TimerManager

```cpp
// Schedule one-shot timer
timer_manager().schedule(100ms, [this]() {
  on_timeout();
});

// Schedule repeating timer
timer_manager().schedule_repeating(1s, [this]() {
  on_periodic();
  return Repeat::Yes;  // or Repeat::No to cancel
});
```

### Configuration

Modules can receive configuration via events:

```cpp
void handle_event(const Event* event) override {
  if (const auto* config = event->as<ConfigEvent>()) {
    handle_configuration_event_module_base_part(config);
    // Process custom config fields
    if constexpr (requires { config->custom_field; }) {
      custom_field_ = config->custom_field;
    }
  }
}
```

## Debugging Tips

### Logging Levels

- `MLOG_DEBUG` - Detailed debugging (disabled in Release)
- `MLOG_INFO` - General operational messages
- `MLOG_WARN` - Potential issues
- `MLOG_ERROR` - Errors that prevent normal operation

### Statistics

Modules track performance metrics:

```cpp
const auto& stats = module->stats();
LOG_INFO("Handle event avg: {}ns", stats.handle_event.avg());
LOG_INFO("Queue time: {}ns", stats.event_queue.last());
LOG_INFO("Run count: {}, Max batch: {}", stats.run_count, stats.max_batch_count);
```

### Trace IDs

Events carry trace IDs for distributed tracing:

```cpp
// Generate new trace ID at entry point
event->generate_trace_id();

// Propagate to child events (automatic)
send_event(child_event);  // Inherits trace_id

// Log with trace context
MLOG_DEBUG("Processing [trace={}]", event->get_trace_id());
```

### Build Variants

- **Debug** - Full debugging symbols, assertions enabled
- **Release** - Optimized, no debug info
- **RelWithDebInfo** - Optimized with debug symbols
- **DebWithCoverage** - Debug with gcov coverage instrumentation

## Related Documentation

- **README.md** - Build instructions and project structure
- **.clang-format** - Code formatting rules
- **rapid/include/rapid/** - Core framework headers
- **component/** - Business logic implementations

## Key Files to Understand

| File | Purpose |
|------|---------|
| `rapid/include/rapid/engine/Module.h` | Base module class with `handle_event()`, `send_event()` |
| `rapid/include/rapid/engine/Engine.h` | Main engine managing threads, modules, event pools |
| `rapid/include/rapid/event/Event.h` | Event base class with reference counting, trace info |
| `rapid/include/rapid/event/EventPtr.h` | Smart pointer wrappers for event ownership |
| `rapid/include/rapid/sync/MpscEventQueue.h` | Lock-free multi-producer single-consumer queue |
| `rapid/include/rapid/sync/SpinLock.h` | Spin lock for short critical sections |
| `rapid/include/rapid/logging/Logging.h` | Logging macros (`LOG_*`, `MLOG_*`) |
| `rapid/include/rapid/engine/PubSub.h` | Publish-subscribe mechanism |
| `rapid/include/rapid/engine/TimerManager.h` | Timer scheduling |
| `rapid/include/rapid/engine/ModuleContext.h` | Module dependencies (pubsub, timers, event pools) |

## Java Part (`app_java/`)

### Overview

Multi-module Maven project (Spring Boot 3.5.3, Java 25) for database synchronization, Kafka integration, and auxiliary services. Package root: `com.moex.tks.rapid`.

### Module Structure

| Module | Purpose |
|--------|---------|
| `dbsync` | Database synchronization service |
| `dbsync-generator` | Code generation for dbsync |
| `dbmodel` | Database model definitions |
| `dbmodel-generator` | Code generation for dbmodel |
| `kafka_db_store` | Kafka → DB consumer/writer |
| `db_kafka_pub` | DB → Kafka publisher |
| `interop` | Interoperability layer between C++ and Java |
| `proto/proto-rapid` | Protobuf definitions for Rapid protocol |
| `proto/proto-pingpong` | Protobuf definitions for ping-pong |
| `proto/proto-oi` | Protobuf definitions for OI |
| `utils` | Shared utilities |
| `zonar` | Monitoring/health service |
| `replay_app` / `replay_cmd` | Transaction replay tools |
| `settleccp_app` | Settlement CCP application |
| `xfers` | Transfer service |

### Build and Test

```bash
cd app_java

# Full build with tests
mvn clean install

# Single module
mvn -pl kafka_db_store clean install

# Run tests only
mvn test

# Single module tests
mvn -pl kafka_db_store test

# Single test class
mvn -Dtest=DataEventTest test

# Single test method
mvn -Dtest=DataEventTest#testMethod test
```

### Key Dependencies

- **Spring Boot 3.5.3** — application framework, dependency management
- **Lombok 1.18.42** — boilerplate reduction (`@Data`, `@Builder`, etc.)
- **Protobuf 3.19.4** — serialization
- **Mockito 5.20** — test mocking (requires `-javaagent` for Java 25)
- **Instancio 4.0** — random test data generation
- **JaCoCo 0.8.13** — code coverage

### Test Configuration

- Tests tagged `LongRunning` are excluded by default (`excluded.unittest.tags` property)
- Mockito requires the JVM argument: `-javaagent:.../mockito-core-5.20.0.jar` (configured in surefire plugin)
- Run with SonarQube analysis: `mvn -PSonar install sonar:sonar` (requires `SONAR_HOST_URL`, `SONAR_TOKEN`, `SONAR_PROJECTKEY_JAVA` env vars)

### Maven Repository

Nexus mirror must be configured in `~/.m2/settings.xml`:

```xml
<mirrors>
    <mirror>
        <id>moex-mirror-central</id>
        <name>moex-mirror-central</name>
        <url>https://nexus-dev.tech.moex.com/repository/moex-maven-static-group</url>
        <mirrorOf>central</mirrorOf>
    </mirror>
</mirrors>
```

## Python Part

### Database Management (`database/`)

PostgreSQL schema management tooling. Core script: `database/git-pg.py` — versioned schema deployment driven by `.sql` files organized in `sqlrepo/` directories.

**Dependencies:** `psycopg2`, `typing_extensions` (see `database/requirements.txt`).

**DB schemas** are organized under `database/rapid/sqlrepo/` by logical domain:

| Schema | Purpose |
|--------|---------|
| `public` | Common/shared objects |
| `rapid` | Core Rapid tables |
| `rapid_cu`, `cu_oi` | Clearing/settlement schemas |
| `rapid_se`, `se_oi` | Securities schemas |
| `xfers_se` | Transfer schemas |

**Key scripts:**

```bash
cd database

# Create empty database from schema definitions
python3 create-empty-db.py

# Export full database to SQL
python3 export-full-db.py

# Git-based schema management (apply/diff/export)
python3 git-pg.py
```

**Internal library** `database/pysql/` provides `PGConnectionParams`, `SQLStrategy`, `DBStrategy`, `FileStrategy` for connecting to PostgreSQL and applying SQL.

### Integration Test Kit (`rapid_testkit/`)

Python-based end-to-end test runner for Rapid ping-pong and protocol tests. Uses `pytest` + `paramiko` for remote execution.

```bash
cd rapid_testkit
pip install -r requirements.txt
export PYTHONPATH=$PYTHONPATH:$(pwd)
python3 runner.py tests/
```

Test suites: `lgw_tests/` (light gateway), `twime_tests/` (TWIME protocol), `rpt0x_tests/` (report tests).

Results (logs, zips) appear in `rapid_testkit/logs/`.

### Python Linting

Ruff is used across all Python code. Each subdirectory may extend the root config:

```bash
# Lint all Python code from project root
ruff check .

# Lint database scripts only
ruff check database/
```

| Scope | Config | Line length | Target |
|-------|--------|-------------|--------|
| Root | `ruff.toml` | 120 | (default) |
| `database/` | `database/ruff.toml` | 160 | Python 3.8 |
| `rapid_testkit/` | `rapid_testkit/ruff.toml` | 120 | (default) |

Indent width is 2 spaces everywhere. Double quotes enforced.
