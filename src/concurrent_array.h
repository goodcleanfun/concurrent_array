#ifndef CONCURRENT_ARRAY_H
#define CONCURRENT_ARRAY_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rw_ticket_spinlock/rw_ticket_spinlock.h"

#define concurrent_array_foreach(array, len, index, value, code) \
    rw_ticket_spinlock_read_lock(&array->lock); \
    len = atomic_load(&array->n); \
    for (index = 0; index < len; index++) { \
        value = array->a[index]; \
        code \
    } \
    rw_ticket_spinlock_read_unlock(&array->lock);

#endif // CONCURRENT_ARRAY_H

// Included once per new array type

#ifndef ARRAY_NAME
#error "Must define ARRAY_NAME"
#endif

#ifndef ARRAY_TYPE
#error "Must define ARRAY_TYPE"
#endif

#ifndef DEFAULT_ARRAY_SIZE
#define NO_DEFAULT_ARRAY_SIZE
#define DEFAULT_ARRAY_SIZE 8
#endif

#ifndef ARRAY_GROWTH_FUNC
#define ARRAY_GROWTH_FUNC(x) ((x) * 3 / 2)
#endif

#ifndef ARRAY_MALLOC
#define ARRAY_MALLOC_DEFINED
#ifndef ARRAY_ALIGNMENT
#define ARRAY_MALLOC(size) cache_line_aligned_malloc(size)
#else
#define ARRAY_MALLOC(size) aligned_malloc(size, ARRAY_ALIGNMENT)
#endif
#endif

#define ARRAY_REALLOC_NEEDS_PREV_SIZE

#ifndef ARRAY_REALLOC
#define ARRAY_REALLOC_DEFINED
#ifndef ARRAY_ALIGNMENT
#define ARRAY_REALLOC(a, prev_size, new_size) cache_line_aligned_resize(a, prev_size, new_size)
#else
#define ARRAY_REALLOC(a, prev_size, new_size) aligned_resize(a, prev_size, new_size, ARRAY_ALIGNMENT)
#endif
#endif

#ifndef ARRAY_FREE
#define ARRAY_FREE_DEFINED
#define ARRAY_FREE(a) default_aligned_free(a)
#endif

#include <stdatomic.h>
#include "threading/threading.h"

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)
#define ARRAY_FUNC(func) CONCAT(ARRAY_NAME, _##func)


typedef struct {
    atomic_size_t n, m, i;
    rw_ticket_spinlock_t lock;
    ARRAY_TYPE *a;
} ARRAY_NAME;

static inline ARRAY_NAME *ARRAY_FUNC(new_size)(size_t size) {
    ARRAY_NAME *array = malloc(sizeof(ARRAY_NAME));
    if (array == NULL) return NULL;
    atomic_init(&array->m, 0);
    atomic_init(&array->n, 0);
    atomic_init(&array->i, 0);
    rw_ticket_spinlock_init(&array->lock);
    array->a = ARRAY_MALLOC((size > 0 ? size : 1) * sizeof(ARRAY_TYPE));
    if (array->a == NULL) return NULL;
    atomic_init(&array->m, size);
    return array;
}

static inline ARRAY_NAME *ARRAY_FUNC(new_size_fixed)(size_t size) {
    ARRAY_NAME *array = ARRAY_FUNC(new_size)(size);
    if (array == NULL) return NULL;
    atomic_store(&array->n, size);
    atomic_store(&array->i, size);
    return array;
}

static inline ARRAY_NAME *ARRAY_FUNC(new)(void) {
    return ARRAY_FUNC(new_size)(DEFAULT_ARRAY_SIZE);
}


static inline bool ARRAY_FUNC(resize_impl)(ARRAY_NAME *array, size_t size, bool fixed) {
    if (array == NULL) return false;
    rw_ticket_spinlock_write_lock(&array->lock);
    size_t cap = atomic_load(&array->m);
    if (size <= cap) {
        rw_ticket_spinlock_write_unlock(&array->lock);
        return true;
    }

    #ifndef ARRAY_REALLOC_NEEDS_PREV_SIZE
    ARRAY_TYPE *ptr = ARRAY_REALLOC(array->a, sizeof(ARRAY_TYPE) * size);
    #else
    ARRAY_TYPE *ptr = ARRAY_REALLOC(array->a, sizeof(ARRAY_TYPE) * array->m, sizeof(ARRAY_TYPE) * size);
    #endif

    if (ptr == NULL) {
        rw_ticket_spinlock_write_unlock(&array->lock);
        return false;
    }
    array->a = ptr;
    atomic_store(&array->m, size);
    if (fixed) {
        atomic_store(&array->i, size);
        atomic_store(&array->n, size);
    }
    rw_ticket_spinlock_write_unlock(&array->lock);
    return true;
}

static inline bool ARRAY_FUNC(resize)(ARRAY_NAME *array, size_t size) {
    bool fixed = false;
    return ARRAY_FUNC(resize_impl)(array, size, false);
}

static inline bool ARRAY_FUNC(resize_to_fit)(ARRAY_NAME *array, size_t needed_capacity) {
    size_t cap = atomic_load(&array->m);
    if (cap >= needed_capacity) return true;
    if (cap == 0) cap = DEFAULT_ARRAY_SIZE;
    size_t prev_cap = cap;
    while (cap < needed_capacity) {
        cap = ARRAY_GROWTH_FUNC(prev_cap);
        if (cap == prev_cap) cap++;
        prev_cap = cap;
    }
    bool fixed = false;
    return ARRAY_FUNC(resize_impl)(array, cap, fixed);
}

static inline bool ARRAY_FUNC(resize_fixed)(ARRAY_NAME *array, size_t size) {
    bool fixed = true;
    return ARRAY_FUNC(resize_impl)(array, size, fixed);
}

static inline ARRAY_TYPE ARRAY_FUNC(get_unchecked)(ARRAY_NAME *array, size_t index) {
    rw_ticket_spinlock_read_lock(&array->lock);
    ARRAY_TYPE value = array->a[index];
    rw_ticket_spinlock_read_unlock(&array->lock);
    return value;
}

static inline bool ARRAY_FUNC(get)(ARRAY_NAME *array, size_t index, ARRAY_TYPE *value) {
    if (index >= atomic_load(&array->n)) return false;
    rw_ticket_spinlock_read_lock(&array->lock);
    *value = array->a[index];
    rw_ticket_spinlock_read_unlock(&array->lock);
    return true;
}

static inline bool ARRAY_FUNC(set)(ARRAY_NAME *array, size_t index, ARRAY_TYPE value) {
    // can technically set if the index count is greater than the number of elements
    if (index >= atomic_load(&array->i)) return false;
    // only need a write lock for resize. Writes can happen concurrently
    rw_ticket_spinlock_read_lock(&array->lock);
    array->a[index] = value;
    rw_ticket_spinlock_read_unlock(&array->lock);
    return true;
}

static inline void ARRAY_FUNC(set_unchecked)(ARRAY_NAME *array, size_t index, ARRAY_TYPE value) {
    rw_ticket_spinlock_read_lock(&array->lock);
    array->a[index] = value;
    rw_ticket_spinlock_read_unlock(&array->lock);
}

static inline bool ARRAY_FUNC(push_get_index)(ARRAY_NAME *array, ARRAY_TYPE value, size_t *index) {
    size_t i = atomic_fetch_add_explicit(&array->i, 1, memory_order_relaxed);
    while (i >= atomic_load_explicit(&array->m, memory_order_relaxed)) {
        if (!ARRAY_FUNC(resize_to_fit)(array, i + 1)) {
            return false;
        }
    }
    rw_ticket_spinlock_read_lock(&array->lock);
    array->a[i] = value;
    rw_ticket_spinlock_read_unlock(&array->lock);

    atomic_fetch_add(&array->n, 1);
    if (index != NULL) *index = i;
    return true;
}

static inline bool ARRAY_FUNC(push)(ARRAY_NAME *array, ARRAY_TYPE value) {
    return ARRAY_FUNC(push_get_index)(array, value, NULL);
}

static inline size_t ARRAY_FUNC(capacity)(ARRAY_NAME *array) {
    return atomic_load(&array->m);
}

static inline size_t ARRAY_FUNC(size)(ARRAY_NAME *array) {
    return atomic_load(&array->n);
}

static inline bool ARRAY_FUNC(extend_get_index)(ARRAY_NAME *array, ARRAY_TYPE *values, size_t n, size_t *index) {
    size_t start = atomic_fetch_add_explicit(&array->i, n, memory_order_relaxed);
    while (start + n >= atomic_load_explicit(&array->m, memory_order_relaxed)) {
        if (!ARRAY_FUNC(resize_to_fit)(array, start + n)) {
            return false;
        }
    }
    rw_ticket_spinlock_read_lock(&array->lock);
    if (memcpy(array->a + start, values, n * sizeof(ARRAY_TYPE)) == NULL) {
        rw_ticket_spinlock_read_unlock(&array->lock);
        return false;
    }
    rw_ticket_spinlock_read_unlock(&array->lock);

    atomic_fetch_add(&array->n, n);
    if (index != NULL) *index = start;
    return true;
}

static inline bool ARRAY_FUNC(extend)(ARRAY_NAME *array, ARRAY_TYPE *values, size_t n) {
    return ARRAY_FUNC(extend_get_index)(array, values, n, NULL);
}

static inline bool ARRAY_FUNC(empty)(ARRAY_NAME *array) {
    return atomic_load(&array->n) == 0;
}

static inline void ARRAY_FUNC(clear)(ARRAY_NAME *array) {
    // need an exclusive lock here
    rw_ticket_spinlock_write_lock(&array->lock);
    atomic_store(&array->i, 0);
    atomic_store(&array->n, 0);
    rw_ticket_spinlock_write_unlock(&array->lock);
}


static inline bool ARRAY_FUNC(copy)(ARRAY_NAME *dst, ARRAY_NAME *src, size_t n) {
    bool ret = true;
    if (dst->m < n) ret = ARRAY_FUNC(resize)(dst, n);
    if (!ret) return false;
    rw_ticket_spinlock_read_lock(&src->lock);
    memcpy(dst->a, src->a, n * sizeof(ARRAY_TYPE));
    rw_ticket_spinlock_read_unlock(&src->lock);
    dst->n = n;
    return ret;
}
static inline ARRAY_NAME *ARRAY_FUNC(new_copy)(ARRAY_NAME *array, size_t n) {
    ARRAY_NAME *cpy = ARRAY_FUNC(new_size)(n);
    if (!ARRAY_FUNC(copy)(cpy, array, n)) return NULL;
    return cpy;
}

static inline ARRAY_NAME *ARRAY_FUNC(new_value)(size_t n, ARRAY_TYPE value) {
    ARRAY_NAME *array = ARRAY_FUNC(new_size)(n);
    if (array == NULL) return NULL;
    for (size_t i = 0; i < n; i++) {
        array->a[i] = value;
    }
    array->n = n;
    return array;
}

#ifdef ARRAY_IS_NUMERIC
static inline ARRAY_NAME *ARRAY_FUNC(new_ones)(size_t n) {
    return ARRAY_FUNC(new_value)(n, (ARRAY_TYPE)1);
}

static inline ARRAY_NAME *ARRAY_FUNC(new_zeros)(size_t n) {
    ARRAY_NAME *array = ARRAY_FUNC(new_size)(n);
    if (array == NULL) return NULL;
    ARRAY_FUNC(zero)(array->a, n);
    array->n = n;
    return array;
}
#endif

static inline void ARRAY_FUNC(destroy)(ARRAY_NAME *array) {
    if (array == NULL) return;
    if (array->a != NULL) {
    #ifdef ARRAY_FREE_DATA
        for (size_t i = 0; i < array->n; i++) {
            ARRAY_FREE_DATA(array->a[i]);
        }
    #endif
        ARRAY_FREE(array->a);
    }
    free(array);
}

#undef CONCAT_
#undef CONCAT
#undef ARRAY_FUNC
#ifdef NO_DEFAULT_ARRAY_SIZE
#undef NO_DEFAULT_ARRAY_SIZE
#undef DEFAULT_ARRAY_SIZE
#endif
#ifdef ARRAY_MALLOC_DEFINED
#undef ARRAY_MALLOC
#undef ARRAY_MALLOC_DEFINED
#endif
#ifdef ARRAY_REALLOC_DEFINED
#undef ARRAY_REALLOC
#undef ARRAY_REALLOC_DEFINED
#endif
#ifdef ARRAY_FREE_DEFINED
#undef ARRAY_FREE
#undef ARRAY_FREE_DEFINED
#endif
