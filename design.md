# Dynamic Objects Library for C - Design Document

## Overview

A lightweight, prototype-based dynamic object system for C, designed for interpreters, embedded systems, and applications requiring runtime object creation and manipulation. Built on top of the `dynamic_array.h` library for memory management and data structures.

## Design Principles

### 1. **Prototype-Based Inheritance**
- Objects serve as both instances and classes
- No separate class/object distinction
- Inheritance through prototype chains
- Runtime flexibility for dynamic languages

### 2. **Memory Efficiency**
- Minimal per-object overhead (~20 bytes)
- Reference counting for automatic memory management
- Reuse `dynamic_array.h` infrastructure
- Microcontroller-friendly (Pi Pico, ESP32-C3)

### 3. **Lock-free**
- Atomic reference counting (when DA_ATOMIC_REFCOUNT=1)
- Object content modifications require external sync
- Same patterns as `dynamic_array.h`

### 4. **Cross-Platform Portability**
- Pure C99/C11 code
- Configurable allocators
- Works on PC and microcontrollers
- Single header library format

## Core Data Structures

### Object Structure
```c
typedef struct {
    DA_ATOMIC_INT ref_count;        // Reference counting (object-level)
    struct do_object_t* prototype;  // Inheritance chain
    union {
        do_property_t* linear_props; // stb_ds array for small objects
        do_property_t* hash_props;   // stb_ds hash map for large objects
    } properties;
    int is_hashed;                  // 0 = linear array, 1 = hash table
} do_object_t, *do_object;
```

### Generic Property Storage
```c
typedef struct {
    char* key;                     // Property name (interned string)
    void* data;                    // Generic data pointer
    size_t size;                   // Size of the data in bytes
    void (*destructor)(void*);     // Optional cleanup function (can be NULL)
} do_property_t;
```

**Design Philosophy**: Like `dynamic_array.h`, DO is completely type-agnostic. It stores arbitrary data using `void*` + `size`, enabling any language interpreter to use its own value types directly. The optional destructor function allows proper cleanup of complex types when properties are overwritten or objects are destroyed.

## Core API Design

### Object Lifecycle
```c
// Creation and destruction
do_object do_create(void);
do_object do_create_with_prototype(do_object prototype);
do_object do_retain(do_object obj);
void do_release(do_object* obj);

// Prototype chain
void do_set_prototype(do_object obj, do_object prototype);
do_object do_get_prototype(do_object obj);
```

### Generic Property Access
```c
// Core functions for arbitrary data types (like DA's element handling)
void* do_get(do_object obj, const char* key);                    // Returns pointer to data, NULL if not found
void do_set(do_object obj, const char* key, const void* data, size_t size, void (*destructor)(void*));
int do_has(do_object obj, const char* key);
void do_delete(do_object obj, const char* key);

// Pre-interned keys - maximum performance when key is already interned
void* do_get_interned(do_object obj, const char* interned_key);
void do_set_interned(do_object obj, const char* interned_key, const void* data, size_t size, void (*destructor)(void*));
int do_has_interned(do_object obj, const char* interned_key);
void do_delete_interned(do_object obj, const char* interned_key);

// Type-safe macros (similar to DA_PUSH, DA_AT)
#define DO_SET(obj, key, value) \
    do { typeof(value) _temp = (value); do_set(obj, key, &_temp, sizeof(_temp), NULL); } while(0)

#define DO_SET_INTERNED(obj, key, value) \
    do { typeof(value) _temp = (value); do_set_interned(obj, key, &_temp, sizeof(_temp), NULL); } while(0)

#define DO_GET(obj, key, type) \
    (*(type*)do_get(obj, key))

#define DO_GET_INTERNED(obj, key, type) \
    (*(type*)do_get_interned(obj, key))

// For types requiring cleanup
#define DO_SET_WITH_DESTRUCTOR(obj, key, value, destructor_fn) \
    do { typeof(value) _temp = (value); do_set(obj, key, &_temp, sizeof(_temp), destructor_fn); } while(0)
```

### Method System (Optional)
Since DO is completely generic, method dispatch is left to the application layer. Objects can store function pointers as properties just like any other data:

```c
// Example: Storing function pointers as properties
typedef void (*my_method_fn)(do_object self, void* args);

my_method_fn my_function = some_function;
DO_SET(obj, "my_method", my_function);

// Later: retrieve and call
my_method_fn method = DO_GET(obj, "my_method", my_method_fn);
if (method) {
    method(obj, some_args);
}
```

**Design Rationale**: By making DO completely generic, method dispatch becomes an application concern. This allows each interpreter to implement method calling in the way that best fits their value system and calling conventions, whether that's JavaScript-style `this` binding, Python-style `self` parameters, or something else entirely.

### Utility Functions
```c
// Introspection
da_array do_get_own_keys(do_object obj);              // Returns da_array of char* (string keys)
da_array do_get_all_keys(do_object obj);              // Including prototype chain
int do_property_count(do_object obj);

// Property iteration (for generic serialization, debugging, etc.)
void do_foreach_property(do_object obj, 
                        void (*callback)(const char* key, void* data, size_t size, void* context),
                        void* context);
```

**Note**: Serialization is intentionally omitted from the core library since DO doesn't know about specific data types. Applications can implement serialization by iterating over properties using `do_foreach_property()` and handling each data type according to their specific needs.

## Implementation Details

### Property Lookup Algorithm
```c
void* do_get(do_object obj, const char* key) {
    // 1. Check own properties first
    void* data = find_own_property(obj, key);
    if (data != NULL) return data;
    
    // 2. Walk prototype chain
    do_object current = obj->prototype;
    while (current != NULL) {
        data = find_own_property(current, key);
        if (data != NULL) return data;
        current = current->prototype;
    }
    
    // 3. Not found
    return NULL;
}
```

### Circular Reference Prevention
```c
// Prevent cycles when setting prototypes
int do_set_prototype(do_object obj, do_object prototype) {
    // Walk up from prototype to check if obj is already in the chain
    do_object current = prototype;
    while (current != NULL) {
        if (current == obj) {
            // Would create cycle - fail immediately
            return DO_ERROR_CYCLE;
        }
        current = current->prototype;
    }
    
    // Release old prototype and retain new one
    if (obj->prototype) do_release(&obj->prototype);
    obj->prototype = prototype ? do_retain(prototype) : NULL;
    return DO_SUCCESS;
}
```

**Note**: Since DO is generic, it cannot automatically detect cycles in property values (it doesn't know which properties contain object references). Cycle detection for property values must be implemented by the application layer if needed.

### Memory Management
- **Reference counting**: Same atomic patterns as `dynamic_array.h` (object-level only)
- **String interning**: Global intern table reduces memory usage and enables pointer-based key comparison
- **Property cleanup**: Each property can have an optional destructor function for proper cleanup
- **Generic data storage**: Properties store copies of data (like DA), destructor called when property is overwritten or object destroyed
- **Property storage**: Use `stb_ds` arrays/maps for efficient property management

### Configuration Options
```c
#ifndef DO_MALLOC
#define DO_MALLOC DA_MALLOC
#endif

#ifndef DO_STRING_INTERNING
#define DO_STRING_INTERNING 1
#endif

#ifndef DO_ATOMIC_REFCOUNT
#define DO_ATOMIC_REFCOUNT DA_ATOMIC_REFCOUNT
#endif

#ifndef DO_HASH_THRESHOLD
#define DO_HASH_THRESHOLD 8  // Switch to hash table after N properties
#endif
```

## Use Cases

### 1. **JavaScript Interpreter**
```c
// JavaScript values using NaN-boxing
typedef uint64_t JSValue;

do_object user = do_create();
JSValue name = js_make_string("John");
JSValue age = js_make_number(30);

DO_SET(user, "name", name);
DO_SET(user, "age", age);

// Later access
JSValue retrieved_name = DO_GET(user, "name", JSValue);
```

### 2. **Python Interpreter**
```c
// Python objects
typedef struct { int refcount; PyTypeObject* ob_type; /* ... */ } PyObject;

do_object globals = do_create();
PyObject* py_string = PyString_FromString("Hello World");
PyObject* py_int = PyInt_FromLong(42);

DO_SET(globals, "message", py_string);  
DO_SET(globals, "answer", py_int);

// Prototype-based inheritance for Python classes
do_object person_class = do_create();
PyObject* init_method = /* ... */;
DO_SET(person_class, "__init__", init_method);
```

### 3. **Configuration System**
```c
// Application-specific config structure
typedef struct {
    char* host;
    int port;
    double timeout;
} DatabaseConfig;

do_object config = do_create();
DatabaseConfig db_config = {"localhost", 5432, 30.0};
DO_SET(config, "database", db_config);

// Later retrieval
DatabaseConfig db = DO_GET(config, "database", DatabaseConfig);
printf("Connecting to %s:%d\n", db.host, db.port);
```

### 4. **Generic Plugin System**
```c
// Plugin function signatures
typedef void (*plugin_init_fn)(do_object plugin);
typedef int (*plugin_process_fn)(do_object plugin, void* data);

do_object plugin = do_create();
DO_SET(plugin, "init", my_plugin_init);
DO_SET(plugin, "process", my_plugin_process);

// Plugin state
int plugin_state = PLUGIN_READY;
DO_SET(plugin, "state", plugin_state);

// Dynamic dispatch
plugin_init_fn init = DO_GET(plugin, "init", plugin_init_fn);
if (init) init(plugin);
```

### 5. **High-Performance Lua-style Interpreter**
```c
// Lua value type
typedef struct { int tag; union { double n; void* p; } value; } lua_Value;

// Pre-intern common keys for speed
const char* INTERN_METATABLE = string_intern("__metatable");
const char* INTERN_INDEX = string_intern("__index");

do_object lua_table = do_create();
lua_Value index_fn = /* ... */;

// Blazing fast access with pre-interned keys
DO_SET_INTERNED(lua_table, INTERN_INDEX, index_fn);
lua_Value retrieved = DO_GET_INTERNED(lua_table, INTERN_INDEX, lua_Value);
```

## Performance Characteristics

### Memory Usage
- **Per object**: ~20 bytes + property storage
- **Per property**: ~32 bytes (key pointer + data pointer + size + destructor + stb_ds overhead)
- **String interning**: Reduces key storage overhead
- **Reference counting**: Automatic cleanup, no GC pauses
- **Generic storage**: Data copied like DA (value semantics), but destructor enables complex cleanup

### Time Complexity
- **Property get**: O(1) with hash table, O(n) with linear array
- **Property set**: O(1) with hash table, O(n) with linear array
- **Method call**: O(prototype chain depth) + property lookup time
- **Object creation**: O(1)

### Hash Table Optimization
Objects automatically upgrade from linear array to hash table storage when property count exceeds threshold:

```c
#ifndef DO_HASH_THRESHOLD
#define DO_HASH_THRESHOLD 8  // Switch to hash table after N properties
#endif

// Hash table optimization using interned string keys for maximum performance
#define PROPERTY_HASH(p) ((uintptr_t)(p.key) >> 3)  // Fast pointer-based hash
#define PROPERTY_EQUAL(a,b) ((a.key) == (b.key))   // Pointer equality for interned strings

// Property lookup implementation
void* find_own_property(do_object obj, const char* key) {
    char* interned_key = string_intern(key);  // Get interned version
    
    if (obj->is_hashed) {
        // O(1) hash table lookup using pointer-based hash
        do_property_t lookup = {interned_key};
        do_property_t* found = hmget(obj->properties.hash_props, lookup);
        return found ? found->data : NULL;
    } else {
        // O(n) linear search using pointer equality (fast for interned strings)
        int len = arrlen(obj->properties.linear_props);
        for (int i = 0; i < len; i++) {
            if (obj->properties.linear_props[i].key == interned_key) {  // Pointer comparison!
                return obj->properties.linear_props[i].data;
            }
        }
        return NULL;
    }
}

// Upgrade to hash table when threshold exceeded
void maybe_upgrade_to_hash(do_object obj) {
    if (!obj->is_hashed && arrlen(obj->properties.linear_props) > DO_HASH_THRESHOLD) {
        // Convert linear array to hash table
        do_property_t* hash_props = NULL;
        int len = arrlen(obj->properties.linear_props);
        for (int i = 0; i < len; i++) {
            do_property_t* prop = &obj->properties.linear_props[i];
            hmput(hash_props, *prop);  // stb_ds uses PROPERTY_HASH/PROPERTY_EQUAL macros
        }
        arrfree(obj->properties.linear_props);
        obj->properties.hash_props = hash_props;
        obj->is_hashed = 1;
    }
}
```

### Optimization Strategies
- **Hybrid storage**: Linear array for small objects (cache-friendly), hash table for large objects (O(1) access)
- **String interning**: Reduce memory and enable pointer equality for keys
- **Property caching**: Cache frequently accessed properties from prototype chain
- **Inline properties**: Store first N properties inline in object structure

## Integration with dynamic_array.h

### Dependencies
```c
#include "dynamic_array.h"  // Required dependency
#include "stb_ds.h"        // Required for hash table optimization

// Uses da_array for:
// - Argument passing (method calls)
// - Key enumeration results (when ref counting is needed)

// Uses stb_ds for:
// - Property storage (both linear arrays and hash tables)
// - Simpler memory management for property arrays
```

### Shared Configuration
```c
// Inherits allocator configuration
#define DO_MALLOC DA_MALLOC
#define DO_REALLOC DA_REALLOC
#define DO_FREE DA_FREE
#define DO_ASSERT DA_ASSERT
#define DO_ATOMIC DA_ATOMIC
```

## Comparison with Alternatives

| Feature | dynamic_objects.h | GObject | cJSON | Raw structs |
|---------|-------------------|---------|-------|-------------|
| Memory overhead | Low (~20 bytes) | High (100+ bytes) | Medium | Lowest |
| Runtime flexibility | High | Medium | High | None |
| Type safety | Runtime | Compile+Runtime | Runtime | Compile |
| Prototype inheritance | Yes | No | No | No |
| Thread safety | Optional | Yes | No | Manual |
| Embedded friendly | Yes | No | Yes | Yes |
| Method dispatch | Yes | Yes | No | No |

## Future Enhancements

### Version 1.0 (Core)
- Basic object creation and property access
- Prototype-based inheritance
- Reference counting
- String interning

### Version 1.1 (Methods)
- Method dispatch system
- Built-in method support
- Self-as-first-parameter binding (completed in design)

### Version 1.2 (Optimization)
- Property caching
- Inline property storage
- Method table optimization

### Version 1.3 (Serialization)
- JSON import/export
- Binary serialization
- Schema validation

### Version 2.0 (Advanced)
- Weak references
- Property descriptors (enumerable, writable, configurable)
- Proxy objects
- Symbol support

## Conclusion

This design provides a completely generic, lightweight dynamic object system suitable for any interpreter, embedded system, or application requiring runtime object manipulation. By making DO type-agnostic like `dynamic_array.h` and using prototype-based inheritance, we achieve maximum flexibility with minimal assumptions about the application's type system.

The library serves as a **universal object container** that any language implementation can use with their own value types, whether that's Python's PyObject, JavaScript's NaN-boxed values, Lua's tagged unions, or custom application structures. This generic approach makes DO broadly applicable across many use cases while maintaining the performance and memory efficiency needed for resource-constrained platforms.