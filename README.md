# dynamic_object.h

[![Version](https://img.shields.io/badge/version-v0.0.3-blue.svg)](https://github.com/your-username/dynamic_object.h/releases)
[![Language](https://img.shields.io/badge/language-C11-blue.svg)](https://en.cppreference.com/w/c/11)
[![License](https://img.shields.io/badge/license-MIT%20OR%20Unlicense-green.svg)](#license)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS%20%7C%20MCU-lightgrey.svg)](#platform-support)

A high-performance, prototype-based dynamic object system for C with enhanced type inference and comprehensive method support. Designed for maximum portability across PC and microcontroller targets, with special focus on language interpreters and reference-counted object systems.

## Features

**High Performance**
- Lock-free atomic reference counting
- Hybrid storage: linear arrays for small objects, hash tables for large objects
- String interning system for efficient property key comparison
- Zero-copy data access with direct pointer operations

**Memory Safe**
- Reference counting prevents memory leaks and use-after-free
- Release functions for complex property value cleanup
- Perfect for language interpreters with garbage-collected objects
- Automatic cleanup when last reference is released
- Pointers always NULLed after release for safety

**Developer Friendly**
- Enhanced type inference with C23/C++11/GCC/Clang support
- JavaScript-like object semantics with prototype-based inheritance
- Comprehensive assert-based error checking
- Single header-only library
- 40+ unit tests with 100% function coverage

**Cross-Platform**
- Works on Linux, Windows, macOS
- Microcontroller support: Raspberry Pi Pico, ESP32-C3, ARM Cortex-M
- Both GCC and Clang compatible
- Graceful fallback for compilers without advanced features

**Complete Method Support**
- Store function pointers or interpreter function references as properties
- Prototype-based method inheritance (JavaScript-style)
- Method shadowing and overriding
- Perfect for building object-oriented interpreters

**Interpreter Integration**
- Generic void* property storage works with any interpreter value system
- Unified release function pattern for consistent memory management
- Prototype chain inheritance for class-based languages
- Efficient method dispatch through property lookup

## Quick Start

```c`
#define DO_IMPLEMENTATION
#include "dynamic_object.h"

int main() {
    // Create simple object
    do_object obj = do_create(NULL);  // NULL = no release function needed
    
    // Set properties with automatic type inference
    DO_SET(obj, "name", "Alice");
    DO_SET(obj, "age", 25);
    DO_SET(obj, "score", 95.5f);
    
    // Get properties 
    char* name = DO_GET(obj, "name", char*);
    int age = DO_GET(obj, "age", int);
    float score = DO_GET(obj, "score", float);
    
    printf("Name: %s, Age: %d, Score: %.1f\n", name, age, score);
    
    // Prototype-based inheritance
    do_object person_proto = do_create(NULL);
    DO_SET(person_proto, "species", "Human");
    
    do_object alice = do_create_with_prototype(person_proto, NULL);
    DO_SET(alice, "name", "Alice");
    
    // alice inherits "species" from prototype
    char* species = DO_GET(alice, "species", char*);  // "Human"
    
    // Cleanup (decrements ref count, frees when count reaches 0)
    do_release(&obj);
    do_release(&alice);
    do_release(&person_proto);
    
    return 0;
}
```

## Method Support

Store and call methods just like any other property:

```c
// Define method signature
typedef void (*greet_method_fn)(do_object self);

// Method implementation  
void person_greet(do_object self) {
    char* name = DO_GET(self, "name", char*);
    printf("Hello, I'm %s!\n", name);
}

// Create prototype with method
do_object person_proto = do_create(NULL);
DO_SET(person_proto, "greet", person_greet);

// Create instance
do_object alice = do_create_with_prototype(person_proto, NULL);
DO_SET(alice, "name", "Alice");

// Call inherited method
greet_method_fn greet = DO_GET(alice, "greet", greet_method_fn);
greet(alice);  // "Hello, I'm Alice!"
```

## Interpreter Integration

For language interpreters, store interpreter function references:

```c
// Your interpreter's function representation
typedef struct {
    unsigned char* bytecode;
    int length;
    int arity;
    char* name;
} interpreter_function_t;

// Release function for interpreter values
void interpreter_value_release(void* val) {
    interpreter_function_t* func = (interpreter_function_t*)val;
    free(func->bytecode);
    free(func->name);
}

// Create object with release function
do_object obj = do_create(interpreter_value_release);

// Store interpreter function as method
interpreter_function_t add_method = {
    .bytecode = compile("function add(x) { return this.value + x; }"),
    .length = 20,
    .arity = 1,
    .name = strdup("add")
};

DO_SET(obj, "add", add_method);

// Later: retrieve and execute in VM
interpreter_function_t* method = (interpreter_function_t*)do_get(obj, "add");
if (method) {
    vm_execute(method->bytecode, obj, arguments);  // Pass obj as 'this'
}
```

## Enhanced Type Inference

Works across different compiler versions:

```c
// C23/C++11/GCC/Clang - automatic type inference
DO_SET(obj, "value", 42);           // int inferred
DO_SET(obj, "pi", 3.14);           // double inferred  
DO_SET(obj, "name", "Hello");       // char* inferred

// Older compilers - explicit type
DO_SET(obj, "value", 42, int);
DO_SET(obj, "pi", 3.14, double);
DO_SET(obj, "name", "Hello", char*);

// Convenience macros
int existing = DO_GET_OR(obj, "count", int, 0);      // Default value
int copied = DO_COPY_PROPERTY(dest, src, "value", int);  // Copy between objects
int count = DO_COUNT_PROPERTIES(obj);                // Property count
```

## Configuration

Customize before including:

```c
// Custom memory allocators
#define DO_MALLOC my_malloc
#define DO_REALLOC my_realloc  
#define DO_FREE my_free

// Enable atomic reference counting (requires C11)
#define DO_ATOMIC_REFCOUNT 1

// Hash table threshold (linear array → hash table)
#define DO_HASH_THRESHOLD 16

// Disable string interning
#define DO_STRING_INTERNING 0

#define DO_IMPLEMENTATION
#include "dynamic_object.h"
```

## API Reference

### Object Lifecycle
```c
do_object do_create(void (*release_fn)(void*));
do_object do_create_with_prototype(do_object prototype, void (*release_fn)(void*)); 
do_object do_retain(do_object obj);
void do_release(do_object* obj);
```

### Property Access
```c
void* do_get(do_object obj, const char* key);
int do_set(do_object obj, const char* key, const void* data, size_t size);
int do_has(do_object obj, const char* key);  
int do_delete(do_object obj, const char* key);
```

### Type-Safe Macros
```c
// With enhanced type inference (C23/C++11/GCC/Clang)
DO_SET(obj, key, value)           // Type inferred automatically
DO_GET(obj, key, type)            // Type must be specified
DO_GET_OR(obj, key, type, fallback)  // With default value

// Fallback for older compilers  
DO_SET(obj, key, value, type)     // Type required
```

### Utility Functions
```c
const char** do_get_own_keys(do_object obj);
const char** do_get_all_keys(do_object obj);  // Includes prototype chain
int do_property_count(do_object obj);
void do_foreach_property(do_object obj, callback, context);
```

### String Interning (Optional)
```c
const char* do_string_intern(const char* str);
void do_string_intern_cleanup(void);
```

## Building and Testing

```bash
# Clone repository
git clone https://github.com/your-username/dynamic_object.h.git
cd dynamic_object.h

# Build and run tests
cmake -B build
cmake --build build
./build/tests

# Should see: "40 Tests 0 Failures 0 Ignored - OK"
```

## Architecture

**Core Structure:**
- `do_object_t`: Reference-counted object with prototype chain
- Hybrid property storage: linear arrays (≤8 properties) → hash tables (>8 properties)
- String interning table for efficient key comparison
- Generic `void*` + `size_t` property storage (like `dynamic_array.h`)

**Memory Management:**
- Reference counting prevents leaks and use-after-free
- Optional atomic operations for thread-safe sharing
- Release function called on property values during cleanup
- Automatic prototype chain cleanup

**Performance:**
- O(1) property access with hash tables
- O(1) string key comparison with interning
- Lock-free atomic reference counting
- Zero-copy property access with direct pointers

## Use Cases

**Perfect for:**
- Language interpreters and virtual machines
- Scripting language implementations  
- Game engines (entity component systems)
- Configuration systems with inheritance
- Plugin architectures with dynamic dispatch
- Any application needing JavaScript-like objects in C

**Example Use Cases:**
- JavaScript interpreter object system
- Python class and instance implementation
- Lua table implementation with metatables
- Game entity properties with component inheritance
- Configuration files with template inheritance

## Dependencies

- **Core**: Only standard C library (`stdlib.h`, `string.h`, `assert.h`)
- **Atomic Operations**: `stdatomic.h` (C11, optional)
- **Hash Tables**: `stb_ds.h` (required, see installation below)
- **Testing**: Unity framework (included)

### Installing stb_ds.h Dependency

This library requires `stb_ds.h` for its hash table implementation. The header uses `#include <stb_ds.h>` to allow flexible dependency management:

**Option 1: Local Project (recommended)**
```bash
# Place stb_ds.h in your project's include directory
mkdir -p deps/stb
wget -O deps/stb/stb_ds.h https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h

# Configure your build system (CMake example)
include_directories(deps/stb)
```

**Option 2: System-wide Installation**
```bash
# Install to system include directory
sudo wget -O /usr/local/include/stb_ds.h \
  https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h
```

**Option 3: Package Manager**
```bash
# Using vcpkg
vcpkg install stb

# Using conan
conan install stb/cci.20230920@
```

## Platform Support

**Tested Platforms:**
- Linux (GCC, Clang)  
- Windows (MinGW, MSVC)
- macOS (Clang)
- Raspberry Pi Pico (arm-none-eabi-gcc)
- ESP32-C3 (Espressif toolchain)

**Requirements:**
- C99 minimum (C11 for atomic operations)
- ~4KB memory overhead
- No external dependencies for core functionality

## Performance Characteristics

**Memory Usage:**
- Object overhead: ~32 bytes per object
- Property overhead: ~24 bytes per property
- String interning reduces key duplication
- Automatic cleanup prevents leaks

**Time Complexity:**
- Property access: O(1) with hash tables, O(n) with linear storage
- Property lookup with inheritance: O(prototype chain depth × property access)
- Object creation: O(1)
- Method calls: O(property lookup time) + function call

## Version History

### v0.0.3 (Current)
- Changed stb_ds.h include to use angle brackets for better portability
- Library can now be copied between projects without modification
- Dependency path configured through build system, not source code
- Updated CMake configuration for cleaner dependency management

### v0.0.2
- Update macro name from DYNAMIC_OBJECT_IMPLEMENTATION to DO_IMPLEMENTATION
- Fix Doxygen documentation configuration and version display
- Improve project metadata consistency across all files

### v0.0.1 (Initial Release)
- Complete prototype-based object system
- Enhanced type inference (C23/C++11/GCC/Clang support)
- Comprehensive method support with 40+ unit tests
- String interning and hybrid storage optimization
- Reference counting with optional atomic operations
- Perfect for interpreter integration
- Cross-platform compatibility (PC + microcontrollers)
- Zero external dependencies (except included stb_ds.h)

## License

This project is dual-licensed under your choice of:

- [MIT License](LICENSE-MIT) 
- [The Unlicense](LICENSE-UNLICENSE)

Choose whichever license works best for your project!

## Contributing

Contributions welcome! Please ensure:
- All tests pass (`cmake --build build && ./build/tests`)
- Code follows existing style
- New features include comprehensive tests
- Maintain compatibility across target platforms

---

*Built for performance, designed for portability, crafted for interpreters.*