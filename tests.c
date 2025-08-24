/**
 * @file tests.c
 * @brief Comprehensive unit tests for dynamic_object.h library
 * 
 * Tests cover all major functionality including:
 * - Object creation and reference counting
 * - Property access and modification
 * - Prototype chain inheritance
 * - String interning
 * - Release function integration
 * - Type-safe macros
 * - Memory management
 * - Performance optimizations
 */

#include "libs/unity/unity.h"

#define DO_IMPLEMENTATION
#include "dynamic_object.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =============================================================================
 * TEST FIXTURES AND HELPERS
 * ============================================================================= */

// Test data structures
typedef struct {
    int id;
    char name[32];
} TestPerson;

typedef struct {
    int x, y;
} TestPoint;

// Global counter for testing release function calls
static int release_call_count = 0;
static int last_released_value = 0;

// Test release function
void test_release_fn(void* data) {
    if (data) {
        last_released_value = *(int*)data;
        release_call_count++;
    }
}

// Reset release counter
void reset_release_counter(void) {
    release_call_count = 0;
    last_released_value = 0;
}

// Test object creation helper
do_object create_test_object(void) {
    return do_create(NULL);
}

do_object create_managed_object(void) {
    return do_create(test_release_fn);
}

void setUp(void) {
    reset_release_counter();
}

void tearDown(void) {
    // Clean up string interning table after each test
    do_string_intern_cleanup();
}

/* =============================================================================
 * STRING INTERNING TESTS
 * ============================================================================= */

void test_string_intern_basic(void) {
    const char* str1 = do_string_intern("hello");
    const char* str2 = do_string_intern("hello");
    const char* str3 = do_string_intern("world");
    
    TEST_ASSERT_NOT_NULL(str1);
    TEST_ASSERT_NOT_NULL(str2);
    TEST_ASSERT_NOT_NULL(str3);
    
    // Same string should return same pointer
    TEST_ASSERT_EQUAL_PTR(str1, str2);
    
    // Different strings should have different pointers
    TEST_ASSERT_NOT_EQUAL(str1, str3);
    
    // Content should be correct
    TEST_ASSERT_EQUAL_STRING("hello", str1);
    TEST_ASSERT_EQUAL_STRING("world", str3);
}

void test_string_find_interned(void) {
    // Initially not interned
    TEST_ASSERT_NULL(do_string_find_interned("test"));
    
    // After interning
    const char* interned = do_string_intern("test");
    TEST_ASSERT_NOT_NULL(interned);
    
    const char* found = do_string_find_interned("test");
    TEST_ASSERT_EQUAL_PTR(interned, found);
}

void test_string_intern_cleanup(void) {
    // Intern some strings
    const char* str1 = do_string_intern("cleanup_test1");
    const char* str2 = do_string_intern("cleanup_test2");
    TEST_ASSERT_NOT_NULL(str1);
    TEST_ASSERT_NOT_NULL(str2);
    
    // Verify they can be found
    TEST_ASSERT_EQUAL_PTR(str1, do_string_find_interned("cleanup_test1"));
    TEST_ASSERT_EQUAL_PTR(str2, do_string_find_interned("cleanup_test2"));
    
    // Cleanup should not crash
    do_string_intern_cleanup();
    
    // After cleanup, new strings should get fresh pointers
    // (This tests that cleanup actually freed the intern table)
    const char* new_str1 = do_string_intern("cleanup_test1");
    TEST_ASSERT_NOT_NULL(new_str1);
    // Note: new_str1 may or may not equal str1 depending on allocator behavior
    // The important thing is cleanup didn't crash and we can still intern strings
}

/* =============================================================================
 * OBJECT CREATION AND LIFECYCLE TESTS
 * ============================================================================= */

void test_object_create_basic(void) {
    do_object obj = create_test_object();
    
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(obj));
    TEST_ASSERT_NULL(do_get_prototype(obj));
    TEST_ASSERT_EQUAL_INT(0, do_property_count(obj));
    
    do_release(&obj);
    TEST_ASSERT_NULL(obj);
}

void test_object_create_with_prototype(void) {
    do_object proto = create_test_object();
    do_object obj = do_create_with_prototype(proto, NULL);
    
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_PTR(proto, do_get_prototype(obj));
    TEST_ASSERT_EQUAL_INT(2, do_get_ref_count(proto)); // proto + obj
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(obj));
    
    do_release(&obj);
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(proto)); // Back to 1
    
    do_release(&proto);
}

void test_object_reference_counting(void) {
    do_object obj = create_test_object();
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(obj));
    
    do_object shared1 = do_retain(obj);
    TEST_ASSERT_EQUAL_PTR(obj, shared1);
    TEST_ASSERT_EQUAL_INT(2, do_get_ref_count(obj));
    
    do_object shared2 = do_retain(obj);
    TEST_ASSERT_EQUAL_INT(3, do_get_ref_count(obj));
    
    do_release(&shared1);
    TEST_ASSERT_NULL(shared1);
    TEST_ASSERT_EQUAL_INT(2, do_get_ref_count(obj));
    
    do_release(&shared2);
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(obj));
    
    do_release(&obj);
}

void test_object_create_with_release_fn(void) {
    do_object obj = create_managed_object();
    
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(obj));
    
    // Add a property that should be released
    int value = 42;
    do_set(obj, "test", &value, sizeof(value));
    
    do_release(&obj);
    
    // Release function should have been called
    TEST_ASSERT_EQUAL_INT(1, release_call_count);
    TEST_ASSERT_EQUAL_INT(42, last_released_value);
}

/* =============================================================================
 * PROPERTY ACCESS TESTS
 * ============================================================================= */

void test_property_set_get_basic(void) {
    do_object obj = create_test_object();
    
    int value = 42;
    int result = do_set(obj, "test", &value, sizeof(value));
    TEST_ASSERT_EQUAL_INT(DO_SUCCESS, result);
    TEST_ASSERT_EQUAL_INT(1, do_property_count(obj));
    
    int* retrieved = (int*)do_get(obj, "test");
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_INT(42, *retrieved);
    
    do_release(&obj);
}

void test_property_update_existing(void) {
    do_object obj = create_managed_object();
    
    int value1 = 42;
    do_set(obj, "test", &value1, sizeof(value1));
    TEST_ASSERT_EQUAL_INT(1, do_property_count(obj));
    TEST_ASSERT_EQUAL_INT(0, release_call_count);
    
    int value2 = 99;
    do_set(obj, "test", &value2, sizeof(value2));
    TEST_ASSERT_EQUAL_INT(1, do_property_count(obj)); // Same count
    TEST_ASSERT_EQUAL_INT(1, release_call_count); // Old value released
    TEST_ASSERT_EQUAL_INT(42, last_released_value);
    
    int* retrieved = (int*)do_get(obj, "test");
    TEST_ASSERT_EQUAL_INT(99, *retrieved);
    
    do_release(&obj);
    TEST_ASSERT_EQUAL_INT(2, release_call_count); // New value released
    TEST_ASSERT_EQUAL_INT(99, last_released_value);
}

void test_property_different_types(void) {
    do_object obj = create_test_object();
    
    // Integer
    int int_val = 42;
    do_set(obj, "int", &int_val, sizeof(int_val));
    
    // Float
    float float_val = 3.14f;
    do_set(obj, "float", &float_val, sizeof(float_val));
    
    // String
    const char* str_val = "hello";
    do_set(obj, "string", &str_val, sizeof(str_val));
    
    // Struct
    TestPerson person = {123, "Alice"};
    do_set(obj, "person", &person, sizeof(person));
    
    TEST_ASSERT_EQUAL_INT(4, do_property_count(obj));
    
    // Verify retrieval
    TEST_ASSERT_EQUAL_INT(42, *(int*)do_get(obj, "int"));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 3.14f, *(float*)do_get(obj, "float"));
    TEST_ASSERT_EQUAL_STRING("hello", *(const char**)do_get(obj, "string"));
    
    TestPerson* retrieved_person = (TestPerson*)do_get(obj, "person");
    TEST_ASSERT_EQUAL_INT(123, retrieved_person->id);
    TEST_ASSERT_EQUAL_STRING("Alice", retrieved_person->name);
    
    do_release(&obj);
}

void test_property_has_and_delete(void) {
    do_object obj = create_managed_object();
    
    int value = 42;
    do_set(obj, "test", &value, sizeof(value));
    
    TEST_ASSERT_TRUE(do_has(obj, "test"));
    TEST_ASSERT_TRUE(do_has_own(obj, "test"));
    TEST_ASSERT_FALSE(do_has(obj, "nonexistent"));
    
    int deleted = do_delete(obj, "test");
    TEST_ASSERT_EQUAL_INT(1, deleted);
    TEST_ASSERT_EQUAL_INT(1, release_call_count); // Value released
    TEST_ASSERT_EQUAL_INT(42, last_released_value);
    
    TEST_ASSERT_FALSE(do_has(obj, "test"));
    TEST_ASSERT_EQUAL_INT(0, do_property_count(obj));
    
    // Deleting non-existent property
    deleted = do_delete(obj, "nonexistent");
    TEST_ASSERT_EQUAL_INT(0, deleted);
    
    do_release(&obj);
}

/* =============================================================================
 * TYPE-SAFE MACRO TESTS
 * ============================================================================= */

void test_type_safe_macros(void) {
    do_object obj = create_test_object();
    
    // Test DO_SET and DO_GET macros
    int int_val = 42;
    float float_val = 3.14f;
    TestPoint point = {10, 20};
    
    DO_SET(obj, "int", int_val);
    DO_SET(obj, "float", float_val);
    DO_SET(obj, "point", point);
    
    TEST_ASSERT_EQUAL_INT(42, DO_GET(obj, "int", int));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 3.14f, DO_GET(obj, "float", float));
    
    TestPoint retrieved_point = DO_GET(obj, "point", TestPoint);
    TEST_ASSERT_EQUAL_INT(10, retrieved_point.x);
    TEST_ASSERT_EQUAL_INT(20, retrieved_point.y);
    
    do_release(&obj);
}

void test_interned_macros(void) {
    do_object obj = create_test_object();
    
    // Pre-intern keys
    const char* key1 = do_string_intern("test1");
    const char* key2 = do_string_intern("test2");
    
    int val1 = 100;
    float val2 = 2.71f;
    
    // Use interned key macros
    DO_SET_INTERNED(obj, key1, val1);
    DO_SET_INTERNED(obj, key2, val2);
    
    TEST_ASSERT_EQUAL_INT(100, DO_GET_INTERNED(obj, key1, int));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 2.71f, DO_GET_INTERNED(obj, key2, float));
    
    TEST_ASSERT_TRUE(do_has_interned(obj, key1));
    TEST_ASSERT_TRUE(do_has_interned(obj, key2));
    
    do_release(&obj);
}

/* =============================================================================
 * PROTOTYPE CHAIN TESTS
 * ============================================================================= */

void test_prototype_inheritance_basic(void) {
    do_object proto = create_test_object();
    do_object obj = create_test_object();
    
    // Set property on prototype
    int proto_val = 42;
    do_set(proto, "inherited", &proto_val, sizeof(proto_val));
    
    // Set property on object
    int obj_val = 99;
    do_set(obj, "own", &obj_val, sizeof(obj_val));
    
    // Set prototype
    int result = do_set_prototype(obj, proto);
    TEST_ASSERT_EQUAL_INT(DO_SUCCESS, result);
    TEST_ASSERT_EQUAL_PTR(proto, do_get_prototype(obj));
    
    // Test inheritance
    TEST_ASSERT_TRUE(do_has(obj, "inherited"));  // From prototype
    TEST_ASSERT_TRUE(do_has(obj, "own"));        // Own property
    TEST_ASSERT_FALSE(do_has_own(obj, "inherited")); // Not own property
    TEST_ASSERT_TRUE(do_has_own(obj, "own"));     // Is own property
    
    TEST_ASSERT_EQUAL_INT(42, *(int*)do_get(obj, "inherited"));
    TEST_ASSERT_EQUAL_INT(99, *(int*)do_get(obj, "own"));
    
    do_release(&obj);
    do_release(&proto);
}

void test_prototype_chain_multiple_levels(void) {
    do_object root = create_test_object();
    do_object middle = create_test_object();
    do_object leaf = create_test_object();
    
    // Set up chain: leaf -> middle -> root
    do_set_prototype(middle, root);
    do_set_prototype(leaf, middle);
    
    // Set properties at different levels
    int root_val = 1;
    int middle_val = 2;
    int leaf_val = 3;
    
    do_set(root, "root_prop", &root_val, sizeof(root_val));
    do_set(middle, "middle_prop", &middle_val, sizeof(middle_val));
    do_set(leaf, "leaf_prop", &leaf_val, sizeof(leaf_val));
    
    // Test inheritance through chain
    TEST_ASSERT_EQUAL_INT(3, *(int*)do_get(leaf, "leaf_prop"));
    TEST_ASSERT_EQUAL_INT(2, *(int*)do_get(leaf, "middle_prop"));
    TEST_ASSERT_EQUAL_INT(1, *(int*)do_get(leaf, "root_prop"));
    
    // Middle can't see leaf properties
    TEST_ASSERT_NULL(do_get(middle, "leaf_prop"));
    TEST_ASSERT_EQUAL_INT(2, *(int*)do_get(middle, "middle_prop"));
    TEST_ASSERT_EQUAL_INT(1, *(int*)do_get(middle, "root_prop"));
    
    do_release(&leaf);
    do_release(&middle);
    do_release(&root);
}

void test_prototype_property_shadowing(void) {
    do_object proto = create_test_object();
    do_object obj = create_test_object();
    
    // Set property on both prototype and object
    int proto_val = 42;
    int obj_val = 99;
    
    do_set(proto, "shared", &proto_val, sizeof(proto_val));
    do_set_prototype(obj, proto);
    
    // Initially gets from prototype
    TEST_ASSERT_EQUAL_INT(42, *(int*)do_get(obj, "shared"));
    
    // Shadow with own property
    do_set(obj, "shared", &obj_val, sizeof(obj_val));
    
    // Now gets own property (shadowing prototype)
    TEST_ASSERT_EQUAL_INT(99, *(int*)do_get(obj, "shared"));
    TEST_ASSERT_EQUAL_INT(42, *(int*)do_get(proto, "shared")); // Proto unchanged
    
    do_release(&obj);
    do_release(&proto);
}

void test_prototype_cycle_detection(void) {
    do_object obj1 = create_test_object();
    do_object obj2 = create_test_object();
    do_object obj3 = create_test_object();
    
    // Create chain: obj1 -> obj2 -> obj3
    TEST_ASSERT_EQUAL_INT(DO_SUCCESS, do_set_prototype(obj1, obj2));
    TEST_ASSERT_EQUAL_INT(DO_SUCCESS, do_set_prototype(obj2, obj3));
    
    // Try to create cycle: obj3 -> obj1 (should fail)
    TEST_ASSERT_EQUAL_INT(DO_ERROR_CYCLE, do_set_prototype(obj3, obj1));
    
    // Try direct self-reference (should fail)
    TEST_ASSERT_EQUAL_INT(DO_ERROR_CYCLE, do_set_prototype(obj1, obj1));
    
    do_release(&obj1);
    do_release(&obj2);
    do_release(&obj3);
}

void test_prototype_null_assignment(void) {
    do_object proto = create_test_object();
    do_object obj = create_test_object();
    
    do_set_prototype(obj, proto);
    TEST_ASSERT_EQUAL_PTR(proto, do_get_prototype(obj));
    TEST_ASSERT_EQUAL_INT(2, do_get_ref_count(proto));
    
    // Clear prototype
    do_set_prototype(obj, NULL);
    TEST_ASSERT_NULL(do_get_prototype(obj));
    TEST_ASSERT_EQUAL_INT(1, do_get_ref_count(proto));
    
    do_release(&obj);
    do_release(&proto);
}

/* =============================================================================
 * PERFORMANCE OPTIMIZATION TESTS
 * ============================================================================= */

void test_linear_to_hash_upgrade(void) {
    do_object obj = create_test_object();
    
    // Add properties below threshold (should stay linear)
    for (int i = 0; i < DO_HASH_THRESHOLD; i++) {
        char key[16];
        snprintf(key, sizeof(key), "prop_%d", i);
        do_set(obj, key, &i, sizeof(i));
    }
    
    TEST_ASSERT_EQUAL_INT(DO_HASH_THRESHOLD, do_property_count(obj));
    
    // Add one more property to trigger upgrade to hash table
    int extra = 999;
    do_set(obj, "extra", &extra, sizeof(extra));
    
    TEST_ASSERT_EQUAL_INT(DO_HASH_THRESHOLD + 1, do_property_count(obj));
    
    // Verify all properties are still accessible
    for (int i = 0; i < DO_HASH_THRESHOLD; i++) {
        char key[16];
        snprintf(key, sizeof(key), "prop_%d", i);
        int* value = (int*)do_get(obj, key);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT(i, *value);
    }
    
    TEST_ASSERT_EQUAL_INT(999, *(int*)do_get(obj, "extra"));
    
    do_release(&obj);
}

void test_interned_key_performance(void) {
    do_object obj = create_test_object();
    
    // Pre-intern keys
    const char* key1 = do_string_intern("fast_key_1");
    const char* key2 = do_string_intern("fast_key_2");
    
    int val1 = 100;
    int val2 = 200;
    
    // Use interned key functions directly
    TEST_ASSERT_EQUAL_INT(DO_SUCCESS, do_set_interned(obj, key1, &val1, sizeof(val1)));
    TEST_ASSERT_EQUAL_INT(DO_SUCCESS, do_set_interned(obj, key2, &val2, sizeof(val2)));
    
    TEST_ASSERT_EQUAL_INT(100, *(int*)do_get_interned(obj, key1));
    TEST_ASSERT_EQUAL_INT(200, *(int*)do_get_interned(obj, key2));
    
    TEST_ASSERT_TRUE(do_has_interned(obj, key1));
    TEST_ASSERT_TRUE(do_has_interned(obj, key2));
    
    do_release(&obj);
}

/* =============================================================================
 * UTILITY FUNCTION TESTS
 * ============================================================================= */

void test_get_own_keys(void) {
    do_object obj = create_test_object();
    
    int val1 = 1, val2 = 2, val3 = 3;
    do_set(obj, "first", &val1, sizeof(val1));
    do_set(obj, "second", &val2, sizeof(val2));
    do_set(obj, "third", &val3, sizeof(val3));
    
    const char** keys = do_get_own_keys(obj);
    TEST_ASSERT_NOT_NULL(keys);
    TEST_ASSERT_EQUAL_INT(3, arrlen(keys));
    
    // Check that all keys are present (order may vary)
    bool found_first = false, found_second = false, found_third = false;
    
    for (int i = 0; i < arrlen(keys); i++) {
        const char* key = keys[i];
        if (strcmp(key, "first") == 0) found_first = true;
        else if (strcmp(key, "second") == 0) found_second = true;
        else if (strcmp(key, "third") == 0) found_third = true;
    }
    
    TEST_ASSERT_TRUE(found_first);
    TEST_ASSERT_TRUE(found_second);
    TEST_ASSERT_TRUE(found_third);
    
    arrfree(keys);
    do_release(&obj);
}

void test_get_all_keys_with_inheritance(void) {
    do_object proto = create_test_object();
    do_object obj = create_test_object();
    
    int val1 = 1, val2 = 2, val3 = 3;
    do_set(proto, "inherited1", &val1, sizeof(val1));
    do_set(proto, "inherited2", &val2, sizeof(val2));
    do_set(obj, "own", &val3, sizeof(val3));
    
    do_set_prototype(obj, proto);
    
    const char** all_keys = do_get_all_keys(obj);
    TEST_ASSERT_NOT_NULL(all_keys);
    TEST_ASSERT_EQUAL_INT(3, arrlen(all_keys));
    
    // Check that all keys are present
    bool found_inherited1 = false, found_inherited2 = false, found_own = false;
    
    for (int i = 0; i < arrlen(all_keys); i++) {
        const char* key = all_keys[i];
        if (strcmp(key, "inherited1") == 0) found_inherited1 = true;
        else if (strcmp(key, "inherited2") == 0) found_inherited2 = true;
        else if (strcmp(key, "own") == 0) found_own = true;
    }
    
    TEST_ASSERT_TRUE(found_inherited1);
    TEST_ASSERT_TRUE(found_inherited2);
    TEST_ASSERT_TRUE(found_own);
    
    arrfree(all_keys);
    do_release(&obj);
    do_release(&proto);
}

// Foreach callback for testing
static int foreach_count = 0;
static int foreach_sum = 0;

void test_foreach_callback(const char* key, void* data, size_t size, void* context) {
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_INT(sizeof(int), size);
    
    foreach_count++;
    foreach_sum += *(int*)data;
    
    // Context should be passed through
    TEST_ASSERT_EQUAL_INT(12345, *(int*)context);
}

void test_foreach_property(void) {
    do_object obj = create_test_object();
    
    int val1 = 10, val2 = 20, val3 = 30;
    do_set(obj, "a", &val1, sizeof(val1));
    do_set(obj, "b", &val2, sizeof(val2));
    do_set(obj, "c", &val3, sizeof(val3));
    
    foreach_count = 0;
    foreach_sum = 0;
    int context = 12345;
    
    do_foreach_property(obj, test_foreach_callback, &context);
    
    TEST_ASSERT_EQUAL_INT(3, foreach_count);
    TEST_ASSERT_EQUAL_INT(60, foreach_sum); // 10 + 20 + 30
    
    do_release(&obj);
}

/* =============================================================================
 * ERROR HANDLING AND EDGE CASE TESTS
 * ============================================================================= */

void test_null_parameter_handling(void) {
    do_object obj = create_test_object();
    
    // These should not crash but return appropriate error codes/values
    TEST_ASSERT_NULL(do_get(obj, NULL));
    TEST_ASSERT_EQUAL_INT(0, do_has(obj, NULL));
    TEST_ASSERT_EQUAL_INT(0, do_delete(obj, NULL));
    
    do_release(&obj);
    
    // Releasing NULL should be safe
    do_object null_obj = NULL;
    do_release(&null_obj); // Should not crash
}

void test_empty_object_operations(void) {
    do_object obj = create_test_object();
    
    TEST_ASSERT_EQUAL_INT(0, do_property_count(obj));
    TEST_ASSERT_NULL(do_get(obj, "nonexistent"));
    TEST_ASSERT_FALSE(do_has(obj, "nonexistent"));
    TEST_ASSERT_FALSE(do_has_own(obj, "nonexistent"));
    TEST_ASSERT_EQUAL_INT(0, do_delete(obj, "nonexistent"));
    
    const char** keys = do_get_own_keys(obj);
    if (keys) {
        TEST_ASSERT_EQUAL_INT(0, arrlen(keys));
        arrfree(keys);
    }
    // Empty object may return NULL keys, which is acceptable
    
    do_release(&obj);
}

void test_large_property_set(void) {
    do_object obj = create_test_object();
    
    const int num_properties = 100;
    
    // Add many properties
    for (int i = 0; i < num_properties; i++) {
        char key[32];
        snprintf(key, sizeof(key), "prop_%d", i);
        do_set(obj, key, &i, sizeof(i));
    }
    
    TEST_ASSERT_EQUAL_INT(num_properties, do_property_count(obj));
    
    // Verify all properties exist and have correct values
    for (int i = 0; i < num_properties; i++) {
        char key[32];
        snprintf(key, sizeof(key), "prop_%d", i);
        
        TEST_ASSERT_TRUE(do_has(obj, key));
        int* value = (int*)do_get(obj, key);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT(i, *value);
    }
    
    do_release(&obj);
}

/* =============================================================================
 * RELEASE FUNCTION INTEGRATION TESTS
 * ============================================================================= */

void test_release_function_on_property_update(void) {
    do_object obj = create_managed_object();
    
    int original = 100;
    int updated = 200;
    
    // Set initial property
    do_set(obj, "test", &original, sizeof(original));
    TEST_ASSERT_EQUAL_INT(0, release_call_count);
    
    // Update property - should release old value
    do_set(obj, "test", &updated, sizeof(updated));
    TEST_ASSERT_EQUAL_INT(1, release_call_count);
    TEST_ASSERT_EQUAL_INT(100, last_released_value);
    
    // Verify new value
    TEST_ASSERT_EQUAL_INT(200, *(int*)do_get(obj, "test"));
    
    do_release(&obj);
    TEST_ASSERT_EQUAL_INT(2, release_call_count);
    TEST_ASSERT_EQUAL_INT(200, last_released_value);
}

void test_release_function_on_delete(void) {
    do_object obj = create_managed_object();
    
    int value = 42;
    do_set(obj, "test", &value, sizeof(value));
    TEST_ASSERT_EQUAL_INT(0, release_call_count);
    
    do_delete(obj, "test");
    TEST_ASSERT_EQUAL_INT(1, release_call_count);
    TEST_ASSERT_EQUAL_INT(42, last_released_value);
    
    do_release(&obj);
    // No additional release calls since property was deleted
    TEST_ASSERT_EQUAL_INT(1, release_call_count);
}

void test_release_function_multiple_properties(void) {
    do_object obj = create_managed_object();
    
    int val1 = 10, val2 = 20, val3 = 30;
    do_set(obj, "prop1", &val1, sizeof(val1));
    do_set(obj, "prop2", &val2, sizeof(val2));
    do_set(obj, "prop3", &val3, sizeof(val3));
    
    TEST_ASSERT_EQUAL_INT(0, release_call_count);
    
    do_release(&obj);
    
    // All three properties should be released
    TEST_ASSERT_EQUAL_INT(3, release_call_count);
    
    // Note: The order of release calls is not guaranteed,
    // but all values should be released
}

/* =============================================================================
 * STRESS AND INTEGRATION TESTS
 * ============================================================================= */

void test_complex_inheritance_scenario(void) {
    // Create a complex inheritance scenario: Animal -> Dog -> Puppy
    do_object animal = create_test_object();
    do_object dog = create_test_object();
    do_object puppy = create_test_object();
    
    // Set up inheritance chain
    do_set_prototype(dog, animal);
    do_set_prototype(puppy, dog);
    
    // Add properties at each level
    const char* animal_sound = "sound";
    const char* dog_breed = "labrador";
    int puppy_age = 6; // months
    
    do_set(animal, "sound", &animal_sound, sizeof(animal_sound));
    do_set(dog, "breed", &dog_breed, sizeof(dog_breed));
    do_set(puppy, "age", &puppy_age, sizeof(puppy_age));
    
    // Test inheritance
    TEST_ASSERT_EQUAL_STRING("sound", *(const char**)do_get(puppy, "sound"));
    TEST_ASSERT_EQUAL_STRING("labrador", *(const char**)do_get(puppy, "breed"));
    TEST_ASSERT_EQUAL_INT(6, *(int*)do_get(puppy, "age"));
    
    // Test property shadowing
    const char* puppy_sound = "yip";
    do_set(puppy, "sound", &puppy_sound, sizeof(puppy_sound));
    
    TEST_ASSERT_EQUAL_STRING("yip", *(const char**)do_get(puppy, "sound"));
    TEST_ASSERT_EQUAL_STRING("sound", *(const char**)do_get(animal, "sound")); // Unchanged
    
    do_release(&puppy);
    do_release(&dog);
    do_release(&animal);
}

void test_mixed_storage_types(void) {
    do_object obj = create_test_object();
    
    // Add enough properties to trigger hash table upgrade
    for (int i = 0; i < DO_HASH_THRESHOLD + 5; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        do_set(obj, key, &i, sizeof(i));
    }
    
    // Verify all properties are accessible after upgrade
    for (int i = 0; i < DO_HASH_THRESHOLD + 5; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT_EQUAL_INT(i, *(int*)do_get(obj, key));
    }
    
    // Delete some properties
    for (int i = 0; i < 3; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        do_delete(obj, key);
    }
    
    // Verify deletions worked
    for (int i = 0; i < 3; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT_FALSE(do_has(obj, key));
    }
    
    // Remaining properties should still be accessible
    for (int i = 3; i < DO_HASH_THRESHOLD + 5; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT_EQUAL_INT(i, *(int*)do_get(obj, key));
    }
    
    do_release(&obj);
}

/* =============================================================================
 * MEMORY MANAGEMENT TESTS
 * ============================================================================= */

void test_object_with_circular_properties(void) {
    // Test that objects can reference each other in properties without issues
    do_object obj1 = create_test_object();
    do_object obj2 = create_test_object();
    
    // Each object holds a reference to the other in properties
    do_set(obj1, "other", &obj2, sizeof(obj2));
    do_set(obj2, "other", &obj1, sizeof(obj1));
    
    // Verify references are stored correctly
    do_object* ref1 = (do_object*)do_get(obj1, "other");
    do_object* ref2 = (do_object*)do_get(obj2, "other");
    
    TEST_ASSERT_EQUAL_PTR(obj2, *ref1);
    TEST_ASSERT_EQUAL_PTR(obj1, *ref2);
    
    // Note: This creates a circular reference in properties, but not in prototype chain
    // The objects will still be freed when released since we're not using retain/release
    // on the object references themselves in this test
    
    do_release(&obj1);
    do_release(&obj2);
}

/* =============================================================================
 * METHOD SUPPORT TESTS 
 * ============================================================================= */

// Test method signatures
typedef void (*simple_method_fn)(do_object self);
typedef int (*method_with_args_fn)(do_object self, int arg);
typedef const char* (*getter_method_fn)(do_object self);

// Global counters for testing method calls
static int method_call_count = 0;
static int last_method_arg = 0;

// Test method implementations
void test_simple_method(do_object self) {
    method_call_count++;
    // Access object properties in method
    int value = DO_GET(self, "value", int);
    last_method_arg = value;
}

int test_method_with_args(do_object self, int arg) {
    method_call_count++;
    last_method_arg = arg;
    
    // Return value based on object state and argument
    int base = DO_GET(self, "base", int);
    return base + arg;
}

const char* test_getter_method(do_object self) {
    method_call_count++;
    return DO_GET(self, "name", char*);
}

// Alternative method for testing shadowing
void test_shadow_method(do_object self) {
    (void)self; // Suppress unused parameter warning
    method_call_count += 10;  // Different behavior
}

// JavaScript-style method implementations for interpreter test
typedef struct {
    int type;  // 0=number, 1=string
    union {
        int number;
        char* string;
    } value;
} js_value_t;

js_value_t test_js_add_method(do_object self, js_value_t other) {
    method_call_count++;
    js_value_t my_val = DO_GET(self, "value", js_value_t);
    
    js_value_t result = {0};
    if (my_val.type == 0 && other.type == 0) {
        // Number addition
        result.type = 0;
        result.value.number = my_val.value.number + other.value.number;
    }
    return result;
}

const char* test_js_to_string_method(do_object self) {
    method_call_count++;
    js_value_t my_val = DO_GET(self, "value", js_value_t);
    
    if (my_val.type == 0) {
        // Convert number to string (simplified)
        return "42";  // Hardcoded for test
    }
    return my_val.value.string;
}

/**
 * Test basic function pointer storage and retrieval
 */
void test_method_storage_basic(void) {
    do_object obj = do_create(NULL);
    TEST_ASSERT_NOT_NULL(obj);
    
    method_call_count = 0;
    
    // Store method as property
    DO_SET(obj, "simple_method", test_simple_method);
    DO_SET(obj, "method_with_args", test_method_with_args); 
    DO_SET(obj, "getter_method", test_getter_method);
    
    // Verify methods are stored
    TEST_ASSERT_TRUE(do_has(obj, "simple_method"));
    TEST_ASSERT_TRUE(do_has(obj, "method_with_args"));
    TEST_ASSERT_TRUE(do_has(obj, "getter_method"));
    
    // Retrieve and verify method pointers
    simple_method_fn simple = DO_GET(obj, "simple_method", simple_method_fn);
    method_with_args_fn with_args = DO_GET(obj, "method_with_args", method_with_args_fn);
    getter_method_fn getter = DO_GET(obj, "getter_method", getter_method_fn);
    
    TEST_ASSERT_NOT_NULL(simple);
    TEST_ASSERT_NOT_NULL(with_args);
    TEST_ASSERT_NOT_NULL(getter);
    
    // Verify function pointers point to correct functions
    TEST_ASSERT_EQUAL_PTR(test_simple_method, simple);
    TEST_ASSERT_EQUAL_PTR(test_method_with_args, with_args);
    TEST_ASSERT_EQUAL_PTR(test_getter_method, getter);
    
    do_release(&obj);
}

/**
 * Test calling methods stored as properties
 */
void test_method_calling(void) {
    do_object obj = do_create(NULL);
    TEST_ASSERT_NOT_NULL(obj);
    
    method_call_count = 0;
    last_method_arg = 0;
    
    // Set up object state
    DO_SET(obj, "value", 42);
    DO_SET(obj, "base", 10);
    DO_SET(obj, "name", "TestObject");
    
    // Store methods
    DO_SET(obj, "simple_method", test_simple_method);
    DO_SET(obj, "method_with_args", test_method_with_args);
    DO_SET(obj, "getter_method", test_getter_method);
    
    // Call simple method
    simple_method_fn simple = DO_GET(obj, "simple_method", simple_method_fn);
    TEST_ASSERT_NOT_NULL(simple);
    simple(obj);
    
    TEST_ASSERT_EQUAL_INT(1, method_call_count);
    TEST_ASSERT_EQUAL_INT(42, last_method_arg);  // Method accessed obj.value
    
    // Call method with arguments
    method_with_args_fn with_args = DO_GET(obj, "method_with_args", method_with_args_fn);
    TEST_ASSERT_NOT_NULL(with_args);
    int result = with_args(obj, 5);
    
    TEST_ASSERT_EQUAL_INT(2, method_call_count);
    TEST_ASSERT_EQUAL_INT(5, last_method_arg);   // Method received argument
    TEST_ASSERT_EQUAL_INT(15, result);           // base(10) + arg(5) = 15
    
    // Call getter method
    getter_method_fn getter = DO_GET(obj, "getter_method", getter_method_fn);
    TEST_ASSERT_NOT_NULL(getter);
    const char* name = getter(obj);
    
    TEST_ASSERT_EQUAL_INT(3, method_call_count);
    TEST_ASSERT_EQUAL_STRING("TestObject", name);
    
    do_release(&obj);
}

/**
 * Test method inheritance through prototype chains
 */
void test_method_inheritance(void) {
    // Create prototype with methods
    do_object prototype = do_create(NULL);
    TEST_ASSERT_NOT_NULL(prototype);
    
    DO_SET(prototype, "simple_method", test_simple_method);
    DO_SET(prototype, "method_with_args", test_method_with_args);
    DO_SET(prototype, "shared_value", 100);
    
    // Create instance inheriting from prototype  
    do_object instance = do_create_with_prototype(prototype, NULL);
    TEST_ASSERT_NOT_NULL(instance);
    
    // Instance has its own properties
    DO_SET(instance, "value", 55);
    DO_SET(instance, "base", 20);
    
    method_call_count = 0;
    
    // Call inherited method
    simple_method_fn inherited_simple = DO_GET(instance, "simple_method", simple_method_fn);
    TEST_ASSERT_NOT_NULL(inherited_simple);
    TEST_ASSERT_EQUAL_PTR(test_simple_method, inherited_simple);  // Same function pointer
    
    inherited_simple(instance);
    TEST_ASSERT_EQUAL_INT(1, method_call_count);
    TEST_ASSERT_EQUAL_INT(55, last_method_arg);  // Accessed instance.value, not prototype
    
    // Call inherited method with args
    method_with_args_fn inherited_with_args = DO_GET(instance, "method_with_args", method_with_args_fn);
    TEST_ASSERT_NOT_NULL(inherited_with_args);
    
    int result = inherited_with_args(instance, 7);
    TEST_ASSERT_EQUAL_INT(2, method_call_count);
    TEST_ASSERT_EQUAL_INT(27, result);  // instance.base(20) + arg(7) = 27
    
    // Verify prototype properties are also inherited
    int shared = DO_GET(instance, "shared_value", int);
    TEST_ASSERT_EQUAL_INT(100, shared);
    
    do_release(&instance);
    do_release(&prototype);
}

/**
 * Test method shadowing (instance methods override prototype methods)
 */
void test_method_shadowing(void) {
    // Create prototype with method
    do_object prototype = do_create(NULL);
    DO_SET(prototype, "test_method", test_simple_method);
    
    // Create instance
    do_object instance = do_create_with_prototype(prototype, NULL);
    DO_SET(instance, "value", 99);
    
    method_call_count = 0;
    
    // Initially, instance uses prototype method
    simple_method_fn method = DO_GET(instance, "test_method", simple_method_fn);
    TEST_ASSERT_EQUAL_PTR(test_simple_method, method);
    
    method(instance);
    TEST_ASSERT_EQUAL_INT(1, method_call_count);
    TEST_ASSERT_EQUAL_INT(99, last_method_arg);
    
    // Shadow the prototype method with instance method
    DO_SET(instance, "test_method", test_shadow_method);
    
    // Now instance uses its own method
    method = DO_GET(instance, "test_method", simple_method_fn);
    TEST_ASSERT_EQUAL_PTR(test_shadow_method, method);
    
    method(instance);
    TEST_ASSERT_EQUAL_INT(11, method_call_count);  // 1 + 10 from test_shadow_method
    
    do_release(&instance);
    do_release(&prototype);
}

/**
 * Test interpreter-style method dispatch scenario
 * Simulates a simple JavaScript-like object system
 */
void test_interpreter_style_methods(void) {
    // Define method signatures for interpreter
    typedef js_value_t (*js_binary_op_fn)(do_object self, js_value_t other);
    typedef const char* (*js_to_string_fn)(do_object self);
    
    // Create "Number" prototype
    do_object number_proto = do_create(NULL);
    DO_SET(number_proto, "add", test_js_add_method);
    DO_SET(number_proto, "toString", test_js_to_string_method);
    
    // Create number instances
    do_object num1 = do_create_with_prototype(number_proto, NULL);
    do_object num2 = do_create_with_prototype(number_proto, NULL);
    
    // Set values
    js_value_t val1 = {0, .value.number = 10};
    js_value_t val2 = {0, .value.number = 20};
    
    DO_SET(num1, "value", val1);
    DO_SET(num2, "value", val2);
    
    method_call_count = 0;
    
    // Call methods like an interpreter would
    js_binary_op_fn add_method = DO_GET(num1, "add", js_binary_op_fn);
    TEST_ASSERT_NOT_NULL(add_method);
    
    js_value_t result = add_method(num1, val2);
    TEST_ASSERT_EQUAL_INT(1, method_call_count);
    TEST_ASSERT_EQUAL_INT(0, result.type);  // Number type
    TEST_ASSERT_EQUAL_INT(30, result.value.number);  // 10 + 20
    
    // Call toString method
    js_to_string_fn to_string = DO_GET(num1, "toString", js_to_string_fn);
    TEST_ASSERT_NOT_NULL(to_string);
    
    const char* str_result = to_string(num1);
    TEST_ASSERT_EQUAL_INT(2, method_call_count);
    TEST_ASSERT_EQUAL_STRING("42", str_result);
    
    // Test method inheritance - num2 has same methods
    js_to_string_fn inherited_to_string = DO_GET(num2, "toString", js_to_string_fn);
    TEST_ASSERT_EQUAL_PTR(test_js_to_string_method, inherited_to_string);
    
    do_release(&num1);
    do_release(&num2);
    do_release(&number_proto);
}

/**
 * Test realistic interpreter method storage
 * Methods are language function references, not C function pointers
 */
void test_interpreter_language_methods(void) {
    // Simulate interpreter function/method representation
    typedef struct {
        int type;                    // 0=bytecode, 1=native, 2=AST
        union {
            struct {
                unsigned char* bytecode;
                int length;
                int arity;           // Number of parameters
            } bc;
            struct {
                void* ast_node;
                int param_count;
            } ast;
        } impl;
        char* name;                  // Method name for debugging
    } interpreter_function_t;
    
    // Mock bytecode for methods (in real interpreter, this would be compiled code)
    unsigned char add_bytecode[] = {0x01, 0x02, 0x03, 0x04}; // LOAD_ARG, LOAD_SELF, ADD, RETURN
    unsigned char to_string_bytecode[] = {0x05, 0x06, 0x07}; // LOAD_SELF, TO_STRING, RETURN
    
    // Create interpreter function objects
    interpreter_function_t add_method = {
        .type = 0,
        .impl.bc = {add_bytecode, 4, 1},  // 1 parameter + implicit self
        .name = "add"
    };
    
    interpreter_function_t to_string_method = {
        .type = 0, 
        .impl.bc = {to_string_bytecode, 3, 0},  // 0 parameters + implicit self
        .name = "toString"
    };
    
    // Create prototype with language methods (not C functions!)
    do_object number_proto = do_create(NULL);
    DO_SET(number_proto, "add", add_method);
    DO_SET(number_proto, "toString", to_string_method);
    DO_SET(number_proto, "type", "Number");  // Type information
    
    // Create instances
    do_object num1 = do_create_with_prototype(number_proto, NULL);
    do_object num2 = do_create_with_prototype(number_proto, NULL);
    
    // Set interpreter values
    int value1 = 10, value2 = 20;
    DO_SET(num1, "value", value1);
    DO_SET(num2, "value", value2);
    
    // Interpreter would look up methods like this:
    interpreter_function_t* add_func = (interpreter_function_t*)do_get(num1, "add");
    TEST_ASSERT_NOT_NULL(add_func);
    
    // Verify it's the right method
    TEST_ASSERT_EQUAL_INT(0, add_func->type);  // Bytecode type
    TEST_ASSERT_EQUAL_INT(4, add_func->impl.bc.length);
    TEST_ASSERT_EQUAL_INT(1, add_func->impl.bc.arity);
    TEST_ASSERT_EQUAL_STRING("add", add_func->name);
    
    // Verify bytecode content
    TEST_ASSERT_EQUAL_UINT8(0x01, add_func->impl.bc.bytecode[0]); // LOAD_ARG
    TEST_ASSERT_EQUAL_UINT8(0x02, add_func->impl.bc.bytecode[1]); // LOAD_SELF  
    TEST_ASSERT_EQUAL_UINT8(0x03, add_func->impl.bc.bytecode[2]); // ADD
    TEST_ASSERT_EQUAL_UINT8(0x04, add_func->impl.bc.bytecode[3]); // RETURN
    
    // Test toString method
    interpreter_function_t* to_string_func = (interpreter_function_t*)do_get(num1, "toString");
    TEST_ASSERT_NOT_NULL(to_string_func);
    TEST_ASSERT_EQUAL_STRING("toString", to_string_func->name);
    TEST_ASSERT_EQUAL_INT(0, to_string_func->impl.bc.arity);
    
    // Test method inheritance - num2 gets same methods  
    interpreter_function_t* inherited_add = (interpreter_function_t*)do_get(num2, "add");
    TEST_ASSERT_EQUAL_PTR(add_func, inherited_add);  // Same method object
    
    // Test prototype properties
    char* proto_type = DO_GET(num1, "type", char*);
    TEST_ASSERT_EQUAL_STRING("Number", proto_type);
    
    // In a real interpreter, method calling would be:
    // 1. Look up method: do_get(obj, method_name) 
    // 2. Get interpreter_function_t*
    // 3. Check arity matches arguments
    // 4. Execute bytecode/AST with VM
    // 5. Pass 'self' object as first parameter
    
    do_release(&num1);
    do_release(&num2);
    do_release(&number_proto);
}

/* =============================================================================
 * ENHANCED TYPE INFERENCE TESTS
 * ============================================================================= */

/**
 * Test enhanced compiler detection and type inference system
 */
void test_enhanced_type_inference(void) {
    do_object obj = do_create(NULL);
    TEST_ASSERT_NOT_NULL(obj);
    
    // Test basic type inference with DO_SET
    int int_val = 42;
    float float_val = 3.14f;
    double double_val = 2.718;
    
    // These should work regardless of compiler support
    DO_SET(obj, "integer", int_val);
    DO_SET(obj, "float", float_val);
    DO_SET(obj, "double", double_val);
    
    // Verify values were stored correctly
    int retrieved_int = DO_GET(obj, "integer", int);
    float retrieved_float = DO_GET(obj, "float", float);
    double retrieved_double = DO_GET(obj, "double", double);
    
    TEST_ASSERT_EQUAL_INT(int_val, retrieved_int);
    TEST_ASSERT_EQUAL_FLOAT(float_val, retrieved_float);
    // Skip double test since Unity has double precision disabled
    (void)retrieved_double; // Suppress unused variable warning
    
#if DO_SUPPORT_TYPE_INFERENCE
    // Test advanced type inference if available
    DO_SET_VAR(obj, "inferred_int", 100);
    DO_SET_VAR(obj, "inferred_double", 1.414);
    
    int inferred_int = DO_GET(obj, "inferred_int", int);
    double inferred_double = DO_GET(obj, "inferred_double", double);
    
    TEST_ASSERT_EQUAL_INT(100, inferred_int);
    // Skip double test since Unity has double precision disabled
    (void)inferred_double; // Suppress unused variable warning
#endif
    
#if DO_STRING_INTERNING
    // Test interned key performance
    const char* interned_key = do_string_intern("performance_test");
    DO_SET_INTERNED(obj, interned_key, 12345);
    
    int interned_val = DO_GET_INTERNED(obj, interned_key, int);
    TEST_ASSERT_EQUAL_INT(12345, interned_val);
#endif
    
    do_release(&obj);
}

/**
 * Test new convenience macros
 */
void test_convenience_macros(void) {
    do_object obj = DO_CREATE_SIMPLE();
    TEST_ASSERT_NOT_NULL(obj);
    
    // Set up test properties
    DO_SET(obj, "test_int", 42);
    DO_SET(obj, "test_string", "hello");
    
    // Test DO_GET_OR with existing and missing properties
    int existing_val = DO_GET_OR(obj, "test_int", int, -1);
    int missing_val = DO_GET_OR(obj, "nonexistent", int, 999);
    
    TEST_ASSERT_EQUAL_INT(42, existing_val);
    TEST_ASSERT_EQUAL_INT(999, missing_val);
    
    // Test DO_COUNT_PROPERTIES
    int prop_count = DO_COUNT_PROPERTIES(obj);
    TEST_ASSERT_EQUAL_INT(2, prop_count);
    
    // Test DO_COPY_PROPERTY
    do_object dest_obj = DO_CREATE_SIMPLE();
    TEST_ASSERT_NOT_NULL(dest_obj);
    
    int copy_result = DO_COPY_PROPERTY(dest_obj, obj, "test_int", int);
    TEST_ASSERT_EQUAL_INT(1, copy_result);
    
    int copied_value = DO_GET(dest_obj, "test_int", int);
    TEST_ASSERT_EQUAL_INT(42, copied_value);
    
    // Test DO_DELETE_PROPERTY 
    int delete_result = DO_DELETE_PROPERTY(obj, "test_int");
    TEST_ASSERT_EQUAL_INT(1, delete_result);
    TEST_ASSERT_FALSE(do_has(obj, "test_int"));
    
    // Test DO_CREATE_WITH_PROTO
    do_object proto_obj = DO_CREATE_WITH_PROTO(obj, NULL);
    TEST_ASSERT_NOT_NULL(proto_obj);
    TEST_ASSERT_EQUAL_PTR(obj, do_get_prototype(proto_obj));
    
    do_release(&obj);
    do_release(&dest_obj);
    do_release(&proto_obj);
}

/* =============================================================================
 * TEST RUNNER
 * ============================================================================= */

int main(void) {
    UNITY_BEGIN();
    
    // String interning tests
    RUN_TEST(test_string_intern_basic);
    RUN_TEST(test_string_find_interned);
    RUN_TEST(test_string_intern_cleanup);
    
    // Object creation and lifecycle tests
    RUN_TEST(test_object_create_basic);
    RUN_TEST(test_object_create_with_prototype);
    RUN_TEST(test_object_reference_counting);
    RUN_TEST(test_object_create_with_release_fn);
    
    // Property access tests
    RUN_TEST(test_property_set_get_basic);
    RUN_TEST(test_property_update_existing);
    RUN_TEST(test_property_different_types);
    RUN_TEST(test_property_has_and_delete);
    
    // Type-safe macro tests
    RUN_TEST(test_type_safe_macros);
    RUN_TEST(test_interned_macros);
    
    // Prototype chain tests
    RUN_TEST(test_prototype_inheritance_basic);
    RUN_TEST(test_prototype_chain_multiple_levels);
    RUN_TEST(test_prototype_property_shadowing);
    RUN_TEST(test_prototype_cycle_detection);
    RUN_TEST(test_prototype_null_assignment);
    
    // Performance optimization tests
    RUN_TEST(test_linear_to_hash_upgrade);
    RUN_TEST(test_interned_key_performance);
    
    // Utility function tests
    RUN_TEST(test_get_own_keys);
    RUN_TEST(test_get_all_keys_with_inheritance);
    RUN_TEST(test_foreach_property);
    
    // Error handling and edge case tests
    RUN_TEST(test_null_parameter_handling);
    RUN_TEST(test_empty_object_operations);
    RUN_TEST(test_large_property_set);
    
    // Release function integration tests
    RUN_TEST(test_release_function_on_property_update);
    RUN_TEST(test_release_function_on_delete);
    RUN_TEST(test_release_function_multiple_properties);
    
    // Stress and integration tests
    RUN_TEST(test_complex_inheritance_scenario);
    RUN_TEST(test_mixed_storage_types);
    RUN_TEST(test_object_with_circular_properties);
    
    // Method support tests
    RUN_TEST(test_method_storage_basic);
    RUN_TEST(test_method_calling);
    RUN_TEST(test_method_inheritance);
    RUN_TEST(test_method_shadowing);
    RUN_TEST(test_interpreter_style_methods);
    RUN_TEST(test_interpreter_language_methods);
    
    // Enhanced type inference tests
    RUN_TEST(test_enhanced_type_inference);
    RUN_TEST(test_convenience_macros);
    
    return UNITY_END();
}