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


int test_array_push_thread(void *arg) {
    test_concurrent_array *v = (test_concurrent_array *)arg;
    for (size_t i = 0; i < NUM_MULTITHREADED_PUSHES; i++) {
        test_concurrent_array_push(v, i);
    }
    return 0;
}

TEST test_array_push_multithreaded(void) {
    test_concurrent_array *v = test_concurrent_array_new();
    ASSERT_EQ(v->m, DEFAULT_ARRAY_SIZE);
    ASSERT(test_concurrent_array_empty(v));
    ASSERT_EQ(v->n, 0);

    size_t num_threads = 4;
    thrd_t threads[num_threads];
    for (size_t i = 0; i < num_threads; i++) {
        thrd_create(&threads[i], test_array_push_thread, v);
    }
    for (size_t i = 0; i < num_threads; i++) {
        thrd_join(threads[i], NULL);
    }

    ASSERT_EQ(v->n, num_threads * NUM_MULTITHREADED_PUSHES);
    size_t sum = 0;
    size_t expected_sum = 0;
    for (size_t i = 0; i < num_threads * NUM_MULTITHREADED_PUSHES; i++) {
        sum += (size_t)v->a[i];
        expected_sum += i % NUM_MULTITHREADED_PUSHES;
    }

    ASSERT_EQ(sum, expected_sum);
    printf("array->n = %zu\n", v->n);
    printf("sum = %zu\n", sum);

    test_concurrent_array_destroy(v);
    PASS();
}

/* Add definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int32_t main(int32_t argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line options, initialization. */

    RUN_TEST(test_array_push_multithreaded);

    GREATEST_MAIN_END();        /* display results */
}
