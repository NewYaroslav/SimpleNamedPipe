# Development Guidelines

## Variable Naming

- Prefix `m_` – always use for class fields (e.g., `m_event_hub`, `m_task_manager`).
- Prefixes `p_` and `str_` – optional when a function or method has more than five variables or arguments of different types. For fewer than five variables, these prefixes may be omitted.
- Boolean variables – start with `is`, `has`, `use`, `enable` or with `m_is_`, `m_has_` and so on for class fields (e.g., `is_connected`, `m_is_active`).
- Prefixes `b_`, `n_`, `f_` – do not use.

## Doxygen Comments

- All code comments and Doxygen documentation must be in English.
- Use `/// \\brief` before functions and classes.
- Do not start descriptions with `The`.

## File Names

- If a file contains only one class, use `CamelCase` (e.g., `TradeManager.hpp`).
- If a file contains multiple classes, utilities, or helper structures, use `snake_case` (e.g., `trade_utils.hpp`, `market_event_listener.hpp`).

## Entity Names

- Class, struct, and enum names – `CamelCase`.
- Method names – `snake_case`.

## Method Naming

- Methods must be named in `snake_case`.
- Getter methods may omit the `get_` prefix when:
  - The method performs no complex computations and simply returns a reference or value.
  - The method provides access to an internal object rather than returning a computed value.
  - The method behaves like a property of the object, similar to `size()` or `empty()` in the STL.
- Use `get_` when:
  - The method computes the value it returns.
  - Omitting `get_` could make the method name misleading.

## Code Style

- Indent with **4 spaces**; do not use tabs.
- Place opening braces on the **same line** as class, method, and namespace declarations.
- Do not use `using namespace`; qualify names with their namespaces (e.g., `std::`).
- Project headers come **before** system headers in include lists.
- Each header file must start with `#pragma once` followed by an include guard of the form `_SIMPLE_NAMED_PIPE_*_HPP_INCLUDED`.

## Constants and Macros

- Constants and macro names use `UPPER_SNAKE_CASE` (e.g., `MAX_CLIENTS`).
- Enum values also follow `CamelCase`.
- Mark classes that should not be inherited from with `final` and use `override` for overridden virtual methods.

## Git Commit Style

- Commit messages are written in **English** using [Conventional Commits](https://www.conventionalcommits.org/).
- Format:

  ```bash
  git commit -a -m "type(scope): short summary" -m "Detailed description of changes."
  ```

- Allowed types:
  - `feat:` – new functionality
  - `fix:` – bug fix
  - `refactor:` – refactoring without behavior changes
  - `perf:` – performance improvements
  - `test:` – add or modify tests
  - `docs:` – documentation changes
  - `build:` – build system or dependency updates
  - `ci:` – CI/CD configuration changes
  - `chore:` – tasks that do not affect production code

