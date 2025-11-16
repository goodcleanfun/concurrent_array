#include <stdint.h>
#include "greatest/greatest.h"

#define ARRAY_NAME test_concurrent_array
#define ARRAY_TYPE int32_t
#define DEFAULT_ARRAY_SIZE 8
#include "concurrent_array.h"
#undef ARRAY_NAME
#undef ARRAY_TYPE

#include "threading/threading.h"

#define NUM_MULTITHREADED_PUSHES 10000000
#define NUM_THREADS 4

int test_array_push_thread(void *arg) {
    test_concurrent_array *v = (test_concurrent_array *)arg;
    for (size_t i = 0; i < NUM_MULTITHREADED_PUSHES / 4; i++) {
        int32_t val = (int32_t)(i * 4);
        test_concurrent_array_push(v, val);
        size_t index;
        int32_t iter_val;
        size_t len;
        concurrent_array_foreach(v, len, index, iter_val, {
            if (index >= 10) {
                break;
            }
        });

        val++;
        test_concurrent_array_push_get_index(v, val, &index);
        int32_t set_val = val + 1;
        test_concurrent_array_set(v, index, set_val);
        int32_t index_val = test_concurrent_array_get_unchecked(v, index);
        if (index_val != set_val) {
            printf("error at %zu, %d != %d\n", index, index_val, set_val);
            return 1;
        }
        test_concurrent_array_set(v, index, val);

        val++;
        test_concurrent_array_push(v, val);
        val++;
        test_concurrent_array_push(v, val);
    }
    return 0;
}

TEST test_array_push_multithreaded(void) {
    test_concurrent_array *v = test_concurrent_array_new();
    ASSERT_EQ(v->m, DEFAULT_ARRAY_SIZE);
    ASSERT(test_concurrent_array_empty(v));
    ASSERT_EQ(v->n, 0);

    thrd_t threads[NUM_THREADS];
    for (size_t i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], test_array_push_thread, v);
    }
    for (size_t i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }

    ASSERT_EQ(test_concurrent_array_size(v), NUM_THREADS * NUM_MULTITHREADED_PUSHES);
    size_t sum = 0;
    size_t expected_sum = 0;
    for (size_t i = 0; i < NUM_THREADS * NUM_MULTITHREADED_PUSHES; i++) {
        sum += (size_t)v->a[i];
        expected_sum += i % NUM_MULTITHREADED_PUSHES;
    }

    ASSERT_EQ(sum, expected_sum);

    test_concurrent_array_destroy(v);
    PASS();
}

#define TEST_CHUNK_SIZE 16

int test_array_extend_thread(void *arg) {
    test_concurrent_array *v = (test_concurrent_array *)arg;
    int32_t values[TEST_CHUNK_SIZE];
    for (size_t i = 0; i < NUM_MULTITHREADED_PUSHES / TEST_CHUNK_SIZE; i++) {
        for (size_t j = 0; j < TEST_CHUNK_SIZE; j++) {
            values[j] = (int32_t)(i * TEST_CHUNK_SIZE + j);
        }
        size_t index;
        test_concurrent_array_extend_get_index(v, values, TEST_CHUNK_SIZE, &index);
        if (i % 1000 == 0) {
            for (size_t j = 0; j < TEST_CHUNK_SIZE; j++) {
                int32_t val = test_concurrent_array_get_unchecked(v, index + j);
                if (val != values[j]) {
                    printf("error at %zu, %d != %d\n", index + j, val, values[j]);
                    return 1;
                }
            }
        }
    }
    return 0;
}

TEST test_array_extend_multithreaded(void) {
    test_concurrent_array *v = test_concurrent_array_new();
    ASSERT_EQ(v->m, DEFAULT_ARRAY_SIZE);
    ASSERT(test_concurrent_array_empty(v));
    ASSERT_EQ(v->n, 0);

    thrd_t threads[NUM_THREADS];
    for (size_t i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], test_array_extend_thread, v);
    }
    for (size_t i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }

    ASSERT_EQ(test_concurrent_array_size(v), NUM_THREADS * NUM_MULTITHREADED_PUSHES);
    size_t sum = 0;
    size_t expected_sum = 0;
    for (size_t i = 0; i < NUM_THREADS * NUM_MULTITHREADED_PUSHES; i++) {
        sum += (size_t)v->a[i];
        expected_sum += i % NUM_MULTITHREADED_PUSHES;
    }

    ASSERT_EQ(sum, expected_sum);

    test_concurrent_array_destroy(v);
    PASS();
}


/* Add definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int32_t main(int32_t argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line options, initialization. */

    RUN_TEST(test_array_push_multithreaded);
    RUN_TEST(test_array_extend_multithreaded);

    GREATEST_MAIN_END();        /* display results */
}
