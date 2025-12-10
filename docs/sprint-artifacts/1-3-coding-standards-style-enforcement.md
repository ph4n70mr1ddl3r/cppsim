# Story 1.3: Coding Standards & Style Enforcement

**Status:** ready-for-dev  
**Epic:** Epic 1 - Project Foundation & Build Infrastructure  
**Story ID:** 1.3  
**Estimated Effort:** Small (2-4 hours)

---

## User Story

As a **developer**,  
I want **clang-format configuration and strict compiler warnings**,  
So that **all code follows consistent style and quality standards**.

---

## Acceptance Criteria

**Given** the project is configured  
**When** I build with CMake  
**Then** compiler warnings are set to pedantic levels (-Wall, -Wextra, -Wpedantic)  
**And** a .clang-format file exists enforcing snake_case and other architecture patterns  
**And** warnings are treated as errors  
**And** the build succeeds with placeholder code

---

## Technical Requirements

### Compiler Warning Configuration

**Goal:** Enforce strict compiler warnings to catch potential bugs and enforce code quality.

**Required Warning Flags:**

**GCC/Clang:**
```cmake
-Wall          # Enable all common warnings
-Wextra        # Enable extra warnings
-Wpedantic     # Strict ISO C++ compliance
-Werror        # Treat warnings as errors
```

**MSVC (Windows):**
```cmake
/W4            # Warning level 4 (highest reasonable level)
/WX            # Treat warnings as errors
```

**Additional Recommended Warnings (GCC/Clang):**
```cmake
-Wshadow                    # Warn about variable shadowing
-Wnon-virtual-dtor          # Warn about non-virtual destructors
-Wold-style-cast            # Warn about C-style casts
-Wcast-align                # Warn about pointer cast alignment issues
-Wunused                    # Warn about unused variables
-Woverloaded-virtual        # Warn about overloaded virtual functions
-Wconversion                # Warn about implicit type conversions
-Wsign-conversion           # Warn about sign conversions
-Wdouble-promotion          # Warn about float to double promotion
-Wformat=2                  # Extra format string checks
```

### clang-format Configuration

**Goal:** Enforce consistent code style matching the architecture specification.

**Key Style Requirements from Architecture:**
- **Naming Convention:** `snake_case` for all types, functions, and variables
- **Member Variables:** Trailing underscore (e.g., `my_variable_`)
- **Header Guards:** `#pragma once` (not traditional include guards)
- **Indentation:** 2 or 4 spaces (choose one consistently)
- **Line Length:** 120 characters maximum
- **Braces:** Opening brace on same line (K&R style) or next line (Allman style) - choose consistently

---

## Architecture Compliance

### Naming Conventions (From Architecture Document)

**Critical:** The architecture specifies **snake_case for all types, functions, variables**.

```cpp
// ✅ Correct
class game_engine { };
void process_action();
int player_count;
std::string session_id_;  // Member variable with trailing underscore

// ❌ Incorrect
class GameEngine { };      // PascalCase - violates architecture
void ProcessAction();      // PascalCase - violates architecture
int playerCount;           // camelCase - violates architecture
std::string m_sessionId;   // Hungarian notation - violates architecture
```

### Header Guards

**Architecture Requirement:** Use `#pragma once` instead of traditional include guards.

```cpp
// ✅ Correct
#pragma once

class my_class {
  // ...
};
```

```cpp
// ❌ Incorrect - Don't use traditional guards
#ifndef MY_CLASS_HPP
#define MY_CLASS_HPP

class my_class {
  // ...
};

#endif
```

### Include Ordering (From Architecture)

**Strict Order:**
1. Own header first (for .cpp files)
2. C system headers (`<cstdio>`, `<cstring>`)
3. C++ standard library (`<string>`, `<vector>`, `<memory>`)
4. Third-party libraries (`<boost/asio.hpp>`, `<nlohmann/json.hpp>`)
5. Project headers (`"protocol.hpp"`, `"game_engine.hpp"`)

```cpp
// ✅ Correct ordering in game_engine.cpp
#include "game_engine.hpp"    // Own header first

#include <cstddef>             // C system
#include <cstring>

#include <memory>              // C++ standard library
#include <string>
#include <vector>

#include <boost/asio.hpp>      // Third-party
#include <nlohmann/json.hpp>

#include "poker_rules.hpp"     // Project headers
#include "protocol.hpp"
```

---

## Implementation Requirements

### 1. Create .clang-format File

**Location:** Repository root (`cppsim/.clang-format`)

**Recommended Configuration:**

```yaml
---
Language: Cpp
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 120
UseTab: Never
PointerAlignment: Left
ReferenceAlignment: Left

# Enforce snake_case (Note: clang-format doesn't enforce naming, but we document it)
# Developers must manually follow snake_case convention

# Bracing
BreakBeforeBraces: Attach  # K&R style (opening brace on same line)
# Alternative: BreakBeforeBraces: Allman  # Opening brace on next line

# Include ordering
SortIncludes: true
IncludeBlocks: Regroup
IncludeCategories:
  - Regex:           '^".*\.hpp"$'
    Priority:        1
    SortPriority:    1
  - Regex:           '^<c.*>$'
    Priority:        2
    SortPriority:    2
  - Regex:           '^<[^/]+>$'
    Priority:        3
    SortPriority:    3
  - Regex:           '^<boost/.*>$'
    Priority:        4
    SortPriority:    4
  - Regex:           '^<nlohmann/.*>$'
    Priority:        4
    SortPriority:    4
  - Regex:           '.*'
    Priority:        5
    SortPriority:    5

# Spacing
SpaceAfterCStyleCast: false
SpaceBeforeParens: ControlStatements
SpacesInAngles: false

# Alignment
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
AlignOperands: true

# Other
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AlwaysBreakTemplateDeclarations: Yes
---
```

**Note:** clang-format **cannot enforce snake_case naming**. Naming must be enforced through code review and developer discipline.

### 2. Create CMake Compiler Warnings Module

**Location:** `cmake/CompilerWarnings.cmake`

**Purpose:** Centralized compiler warning configuration reusable across all targets.

**Content:**

```cmake
# cmake/CompilerWarnings.cmake
# Comprehensive compiler warning configuration

function(set_project_warnings target)
  option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

  set(MSVC_WARNINGS
    /W4           # Warning level 4
    /w14242       # Conversion warning
    /w14254       # Operator warning
    /w14263       # Member function override warning
    /w14265       # Virtual destructor warning
    /w14287       # Unsigned/negative constant mismatch
    /we4289       # Loop variable scope
    /w14296       # Expression always true/false
    /w14311       # Pointer truncation
    /w14545       # Expression before comma has no effect
    /w14546       # Function call before comma missing argument list
    /w14547       # Operator before comma has no effect
    /w14549       # Operator before comma has no effect
    /w14555       # Expression has no effect
    /w14619       # Pragma warning suppression
    /w14640       # Thread-unsafe static member initialization
    /w14826       # Conversion warning
    /w14905       # Wide string literal cast
    /w14906       # String literal cast
    /w14928       # Illegal copy-initialization
  )

  set(CLANG_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
  )

  set(GCC_WARNINGS
    ${CLANG_WARNINGS}
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wuseless-cast
  )

  if(WARNINGS_AS_ERRORS)
    set(CLANG_WARNINGS ${CLANG_WARNINGS} -Werror)
    set(GCC_WARNINGS ${GCC_WARNINGS} -Werror)
    set(MSVC_WARNINGS ${MSVC_WARNINGS} /WX)
  endif()

  if(MSVC)
    set(PROJECT_WARNINGS ${MSVC_WARNINGS})
  elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(PROJECT_WARNINGS ${CLANG_WARNINGS})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_WARNINGS ${GCC_WARNINGS})
  else()
    message(WARNING "No compiler warnings set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
  endif()

  target_compile_options(${target} INTERFACE ${PROJECT_WARNINGS})
endfunction()
```

### 3. Integrate Warnings into Root CMakeLists.txt

**Modify:** `CMakeLists.txt` (root)

**Add:**

```cmake
# Include compiler warnings module
include(cmake/CompilerWarnings.cmake)

# Create interface library for project-wide settings
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# Apply to all targets
target_link_libraries(poker_server PRIVATE project_options project_warnings)
target_link_libraries(poker_client PRIVATE project_options project_warnings)
target_link_libraries(poker_common PUBLIC project_options project_warnings)
target_link_libraries(poker_tests PRIVATE project_options project_warnings)
```

### 4. Verify Placeholder Code Compiles

After adding strict warnings, ensure **existing placeholder code** (from Story 1.1) still compiles without warnings.

**If warnings appear in placeholder main.cpp files:**
- Fix them immediately
- This validates the warning configuration works correctly

---

## Testing Requirements

### Build Verification

1. **Configure and build:**
   ```bash
   cmake -B build -S .
   cmake --build build
   ```

2. **Verify warnings are enabled:**
   - Check compile output for warning flags
   - Should see `-Wall -Wextra -Wpedantic -Werror` (or MSVC equivalents)

3. **Test warning-as-error:**
   - Introduce a deliberate warning (e.g., unused variable)
   - Build should FAIL with error
   - Remove the warning, build should SUCCEED

4. **Test clang-format:**
   ```bash
   # Format all source files
   find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

   # Check if formatting needed
   find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run -Werror
   ```

---

## Definition of Done

- [ ] `.clang-format` file created in repository root
- [ ] `cmake/CompilerWarnings.cmake` module created
- [ ] Root `CMakeLists.txt` updated to include compiler warnings
- [ ] All targets (server, client, common, tests) have warnings enabled
- [ ] Warnings are treated as errors (`-Werror` or `/WX`)
- [ ] Placeholder code from Story 1.1 compiles without warnings
- [ ] clang-format configuration enforces architecture style (snake_case documented)
- [ ] Build succeeds with strict warnings enabled
- [ ] Deliberate warning causes build failure (validates warnings-as-errors)

---

## Context & Dependencies

### Depends On
- **Story 1.1:** CMake Project Structure & Dependencies
- **Story 1.2:** Project Directory Structure

### Blocks
- **All Epic 2+ stories** - All future code must adhere to these standards

### Related Stories
- **Epic 9:** Testing stories will validate code quality standards

---

## Previous Story Intelligence

### Learnings from Story 1.1 & 1.2

**If Story 1.1 is complete:**
- Placeholder `main.cpp` files exist for server and client
- These files may have simple `std::cout` statements
- Ensure these compile cleanly with new warnings

**If Story 1.2 is complete:**
- `cmake/` directory exists
- Place `CompilerWarnings.cmake` there

**Common Integration Points:**
- Root `CMakeLists.txt` - Add compiler warnings setup
- Verify CMake targets (poker_server, poker_client, poker_common, poker_tests) are defined

---

## Developer Notes

### Why Warnings-as-Errors?

**Benefits:**
- **Forces code quality** - Developers must fix warnings immediately
- **Prevents warning accumulation** - Warnings don't pile up over time
- **Catches bugs early** - Many warnings indicate real bugs

**Trade-off:**
- Can be strict during rapid prototyping
- Can disable (`-DWARNINGS_AS_ERRORS=OFF`) for development, enable for CI/CD

### clang-format Limitations

**clang-format cannot enforce:**
- Naming conventions (snake_case vs. camelCase)
- Semantic rules (e.g., member variables must have trailing underscore)

**Solution:** Use **code review** and **developer discipline** to enforce naming.

**Alternative Tools:**
- `clang-tidy` - Can enforce naming conventions via checks
- Consider adding clang-tidy in future stories for stricter enforcement

### Include Ordering Automation

The `.clang-format` configuration automatically sorts and groups `#include` directives. This ensures:
- Consistency across all files
- Catches missing includes (own header first reveals dependencies)
- Reduces merge conflicts

### Estimated Complexity

**Low** - Standard CMake and clang-format setup. Key considerations:
- Ensure warning flags are compatible with chosen compiler versions
- Test on multiple platforms if cross-platform support is critical
- May need to fix warnings in placeholder code from Story 1.1

---

## Resources

### clang-format Documentation
- clang-format Style Options: https://clang.llvm.org/docs/ClangFormatStyleOptions.html
- clang-format Configurator: https://zed0.co.uk/clang-format-configurator/

### Compiler Warnings
- GCC Warning Options: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- Clang Diagnostic Flags: https://clang.llvm.org/docs/DiagnosticsReference.html
- MSVC Warning Levels: https://learn.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level

### CMake Best Practices
- Modern CMake Tooling: https://cliutils.gitlab.io/modern-cmake/
- Effective Modern CMake: https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1

---

## Post-Story Validation

### Manual Checks

1. **Verify .clang-format works:**
   ```bash
   # Create a deliberately badly formatted file
   echo "int    main(   ){return    0;}" > temp.cpp
   
   # Format it
   clang-format -i temp.cpp
   
   # Check result (should be properly formatted)
   cat temp.cpp
   
   # Clean up
   rm temp.cpp
   ```

2. **Verify warnings work:**
   ```bash
   # Add unused variable to main.cpp temporarily
   # Build should fail with -Werror
   ```

3. **Verify all targets compile:**
   ```bash
   cmake --build build --target poker_server
   cmake --build build --target poker_client
   cmake --build build --target poker_common
   cmake --build build --target poker_tests
   ```

---

**This story completes Epic 1! Once done, the project foundation is solid and ready for Epic 2 (Core Protocol & Network Communication).**
