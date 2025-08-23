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
    da_array methods;               // Function pointers (optional, needs ref counting)
} do_object_t, *do_object;
```

### Value Types
```c
typedef enum {
    DO_UNDEFINED,
    DO_NULL,
    DO_BOOL,
    DO_INT,
    DO_FLOAT,
    DO_STRING,
    DO_OBJECT,
    DO_ARRAY,
    DO_FUNCTION
} do_value_type_t;

typedef struct {
    do_value_type_t type;
    union {
        int bool_val;
        int int_val;
        double float_val;
        char* string_val;        // Ref-counted string
        do_object object_val;
        da_array array_val;
        do_function function_val;
    } data;
} do_value_t;
```

### Property Storage
```c
typedef struct {
    char* key;                  // Property name (interned string)
    do_value_t value;          // Property value
    int flags;                 // Enumerable, writable, etc.
} do_property_t;
```

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

### Property Access
```c
// Generic property access
do_value_t do_get(do_object obj, const char* key);
void do_set(do_object obj, const char* key, do_value_t value);
int do_has(do_object obj, const char* key);
void do_delete(do_object obj, const char* key);

// Type-specific setters (convenience)
void do_set_undefined(do_object obj, const char* key);
void do_set_null(do_object obj, const char* key);
void do_set_bool(do_object obj, const char* key, int value);
void do_set_int(do_object obj, const char* key, int value);
void do_set_float(do_object obj, const char* key, double value);
void do_set_string(do_object obj, const char* key, const char* value);
void do_set_object(do_object obj, const char* key, do_object value);
void do_set_array(do_object obj, const char* key, da_array value);

// Type-specific getters
int do_get_bool(do_object obj, const char* key, int default_val);
int do_get_int(do_object obj, const char* key, int default_val);
double do_get_float(do_object obj, const char* key, double default_val);
char* do_get_string(do_object obj, const char* key, const char* default_val);
do_object do_get_object(do_object obj, const char* key);
da_array do_get_array(do_object obj, const char* key);
```

### Method System (Optional)
```c
typedef do_value_t (*do_method_fn)(do_object self, da_array args);

void do_set_method(do_object obj, const char* name, do_method_fn method);
do_value_t do_call(do_object obj, const char* method, da_array args);
```

**Method Binding**: Methods use explicit self-as-first-parameter binding. The `self` parameter provides the method's `this` context, while `args` contains user-provided arguments. This approach is thread-safe, explicit, and allows high-level languages to provide `this`-style syntax while the C implementation remains simple.

### Utility Functions
```c
// Introspection
da_array do_get_own_keys(do_object obj);
da_array do_get_all_keys(do_object obj);  // Including prototype chain
int do_property_count(do_object obj);

// Serialization helpers
char* do_to_json(do_object obj);
do_object do_from_json(const char* json);
```

## Implementation Details

### Property Lookup Algorithm
```c
do_value_t do_get(do_object obj, const char* key) {
    // 1. Check own properties first
    do_value_t val = find_own_property(obj, key);
    if (val.type != DO_UNDEFINED) return val;
    
    // 2. Walk prototype chain
    do_object current = obj->prototype;
    while (current != NULL) {
        val = find_own_property(current, key);
        if (val.type != DO_UNDEFINED) return val;
        current = current->prototype;
    }
    
    // 3. Not found
    return do_undefined_value();
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
    obj->prototype = prototype;
    return DO_SUCCESS;
}

// Similar check when setting object properties
int do_set_object(do_object obj, const char* key, do_object value) {
    // Check if value contains obj in its object graph
    if (contains_object(value, obj)) {
        return DO_ERROR_CYCLE;
    }
    // Safe to set
    set_property(obj, key, make_object_value(value));
    return DO_SUCCESS;
}
```

### Memory Management
- **Reference counting**: Same atomic patterns as `dynamic_array.h` (object-level only)
- **String interning**: Reduce memory usage and enable pointer-based key comparison
- **Circular reference prevention**: Fail-fast when setting prototype/property would create cycle
- **Property storage**: Use `stb_ds` arrays/maps (no ref counting overhead)

### Configuration Options
```c
#ifndef DO_MALLOC
#define DO_MALLOC DA_MALLOC
#endif

#ifndef DO_STRING_INTERNING
#define DO_STRING_INTERNING 1
#endif

#ifndef DO_METHOD_SUPPORT  
#define DO_METHOD_SUPPORT 1
#endif

#ifndef DO_ATOMIC_REFCOUNT
#define DO_ATOMIC_REFCOUNT DA_ATOMIC_REFCOUNT
#endif

#ifndef DO_HASH_THRESHOLD
#define DO_HASH_THRESHOLD 8  // Switch to hash table after N properties
#endif
```

## Use Cases

### 1. **Interpreter Variables**
```c
// var user = {name: "John", age: 30, address: {city: "NYC"}}
do_object user = do_create();
do_set_string(user, "name", "John");
do_set_int(user, "age", 30);

do_object address = do_create();
do_set_string(address, "city", "NYC");
do_set_object(user, "address", address);
```

### 2. **Class Hierarchies**
```c
// Create "Person" prototype
do_object person_proto = do_create();
do_set_string(person_proto, "species", "human");
do_set_method(person_proto, "speak", person_speak_method);

// Create "Student" prototype inheriting from Person
do_object student_proto = do_create_with_prototype(person_proto);
do_set_method(student_proto, "study", student_study_method);

// Create instance
do_object alice = do_create_with_prototype(student_proto);
do_set_string(alice, "name", "Alice");

// alice.speak() -> calls person_speak_method
// alice.study() -> calls student_study_method
```

### 3. **Configuration Objects**
```c
do_object config = do_from_json(`{
    "database": {
        "host": "localhost",
        "port": 5432,
        "timeout": 30.0
    },
    "features": ["auth", "logging"]
}`);

int port = do_get_int(do_get_object(config, "database"), "port", 3306);
```

### 4. **Plugin Architecture**
```c
// Plugin objects with methods
do_object plugin = do_create();
do_set_method(plugin, "init", plugin_init);
do_set_method(plugin, "process", plugin_process);
do_set_method(plugin, "cleanup", plugin_cleanup);

// Dynamic method dispatch
do_call(plugin, "init", NULL);
```

## Performance Characteristics

### Memory Usage
- **Per object**: ~20 bytes + property storage
- **Per property**: ~24 bytes (key pointer + value + flags)
- **String interning**: Reduces key storage overhead
- **Reference counting**: Automatic cleanup, no GC pauses

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
do_value_t find_own_property(do_object obj, const char* key) {
    char* interned_key = string_intern(key);  // Get interned version
    
    if (obj->is_hashed) {
        // O(1) hash table lookup using pointer-based hash
        do_property_t lookup = {interned_key};
        do_property_t* found = hmget(obj->properties.hash_props, lookup);
        return found ? found->value : do_undefined_value();
    } else {
        // O(n) linear search using pointer equality (fast for interned strings)
        int len = arrlen(obj->properties.linear_props);
        for (int i = 0; i < len; i++) {
            if (obj->properties.linear_props[i].key == interned_key) {  // Pointer comparison!
                return obj->properties.linear_props[i].value;
            }
        }
        return do_undefined_value();
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
// - Method tables (when ref counting is needed)
// - Argument passing
// - Key enumeration results

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

This design provides a powerful yet lightweight dynamic object system suitable for interpreters, embedded systems, and applications requiring runtime object manipulation. By building on `dynamic_array.h` foundations and using prototype-based inheritance, we achieve maximum flexibility with minimal complexity and memory overhead.

The library fills a gap in the C ecosystem by providing JavaScript-style objects with C performance, making it ideal for implementing dynamic languages on resource-constrained platforms.