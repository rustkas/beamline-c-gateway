# Contributing to IPC Gateway

Thank you for your interest in contributing to the IPC Gateway project!

---

## How to Contribute

### Reporting Bugs

1. **Check existing issues** first to avoid duplicates
2. **Create a new issue** with:
   - Clear title and description
   - Steps to reproduce
   - Expected vs actual behavior
   - System information (OS, compiler version)
   - Relevant logs or error messages

### Suggesting Features

1. **Open an issue** with the `enhancement` label
2. **Describe**:
   - Use case and motivation
   - Proposed solution
   - Alternatives considered
   - Impact on existing functionality

### Pull Requests

1. **Fork the repository**
2. **Create a branch**: `feature/your-feature-name` or `fix/issue-number`
3. **Make changes** following our coding standards
4. **Test thoroughly**
5. **Submit Pull Request** with clear description

---

## Development Setup

### Prerequisites

- GCC 12.3+ or Clang
- CMake 4.1+
- NATS server (for integration tests)
- Git

### Getting Started

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/c-gateway.git
cd c-gateway

# Build
mkdir -p build && cd build
cmake ..
make

# Run tests
make test
```

---

## Coding Standards

### Code Style

- **Language**: C11 standard
- **Indentation**: 4 spaces (no tabs)
- **Line length**: Max 100 characters (flexible)
- **Braces**: K\u0026R style

### Safety Requirements

**REQUIRED**:
- ✅ Use `snprintf` instead of `sprintf`
- ✅ Use `strncpy` instead of `strcpy`  
- ✅ Use `strncat` instead of `strcat`
- ✅ Check all return values
- ✅ Initialize all variables
- ✅ Free allocated memory

**FORBIDDEN**:
- ❌ `gets` (always unsafe)
- ❌ `strcpy` (use `snprintf` or `strncpy`)
- ❌ `sprintf` (use `snprintf`)
- ❌ `strcat` (use `strncat`)

### Example

```c
/* GOOD */
char buffer[256];
snprintf(buffer, sizeof(buffer), "Hello, %s!", name);

/* BAD */
char buffer[256];
sprintf(buffer, "Hello, %s!", name);  /* ❌ Unsafe */
```

---

## Testing Requirements

### Before Submitting PR

**MUST PASS**:
1. All unit tests
2. AddressSanitizer (ASan)
3. Compilation with no warnings
4. Code review checklist

### Running Tests

```bash
# Unit tests
cd build
make test-buffer-pool test-nats-pool test-trace-context
./test-buffer-pool
./test-nats-pool
./test-trace-context

# ASan
mkdir -p build-san && cd build-san
cmake .. -DCMAKE_C_FLAGS="-fsanitize=address -g"
make
./test-buffer-pool

# All tests
make test
```

### Test Coverage

- **Core components**: 100% (required)
- **Integration**: Reasonable coverage
- **Edge cases**: Document if not testable

---

## Commit Messages

### Format

```
type(scope): Brief description

Detailed description of what and why.

Fixes #issue-number
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `test`: Tests
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `chore`: Build/tooling changes

### Example

```
fix(buffer-pool): Prevent race condition in acquire

Added mutex locking around free list manipulation
to prevent concurrent access issues.

Fixes #123
```

---

## Code Review Process

### Review Criteria

1. **Functionality**: Does it work as intended?
2. **Safety**: No memory leaks, buffer overflows?
3. **Performance**: No unnecessary allocations?
4. **Tests**: Adequate test coverage?
5. **Documentation**: Code comments, updated docs?
6. **Style**: Follows coding standards?

### Approval Process

1. **Submit PR**
2. **Automated checks** must pass (CI)
3. **Code review** by maintainer
4. **Address feedback**
5. **Approval** → **Merge**

---

## Documentation

### When to Update Docs

- New features
- API changes
- Configuration changes
- Bug fixes affecting behavior

### Documentation Types

1. **Code comments**: Complex logic
2. **README.md**: Project overview
3. **docs/**: Detailed documentation  
4. **CHANGELOG.md**: Version history

---

## Security

### Reporting Vulnerabilities

**DO NOT** create public issues for security vulnerabilities.

See [SECURITY.md](./SECURITY.md) for reporting process.

### Security Review

- All security-sensitive code requires review
- Run sanitizers (ASan, Valgrind)
- Test edge cases thoroughly

---

## Questions?

- **Issues**: For bugs and features
- **Discussions**: For general questions
- **Email**: For security issues (see SECURITY.md)

---

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

## Recognition

Contributors will be acknowledged in:
- `CONTRIBUTORS.md` (if we create one)
- GitHub contributors page
- Release notes (for significant contributions)

---

**Thank you for contributing!**
