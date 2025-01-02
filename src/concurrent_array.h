#ifndef CONCURRENT_ARRAY_H
#define CONCURRENT_ARRAY_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define ARRAY_MALLOC malloc
#define ARRAY_MALLOC_DEFINED
#endif

#ifndef ARRAY_REALLOC
#define ARRAY_REALLOC realloc
#define ARRAY_REALLOC_DEFINED
#endif

#ifndef ARRAY_FREE
#define ARRAY_FREE free
#define ARRAY_FREE_DEFINED
#endif

#include <stdatomic.h>
#include "threading/threading.h"

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)
#define ARRAY_FUNC(func) CONCAT(ARRAY_NAME, _##func)


typedef struct {
    atomic_size_t n, m, i;
    rwlock_t resize_lock;
    ARRAY_TYPE *a;
} ARRAY_NAME;

static inline ARRAY_NAME *ARRAY_FUNC(new_size)(size_t size) {
    ARRAY_NAME *array = malloc(sizeof(ARRAY_NAME));
    if (array == NULL) return NULL;
    atomic_init(&array->m, 0);
    atomic_init(&array->n, 0);
    atomic_init(&array->i, 0);
    rwlock_init(&array->resize_lock, NULL);
    array->a = ARRAY_MALLOC((size > 0 ? size : 1) * sizeof(ARRAY_TYPE));
    if (array->a == NULL) return NULL;
    atomic_init(&array->m, size);
    return array;
}

static inline ARRAY_NAME *ARRAY_FUNC(new)(void) {
    return ARRAY_FUNC(new_size)(DEFAULT_ARRAY_SIZE);
}

static inline bool ARRAY_FUNC(resize)(ARRAY_NAME *array, size_t size) {
    size_t cap = atomic_load(&array->m);

    if (size <= cap) return true;
    #ifndef ARRAY_REALLOC_NEEDS_PREV_SIZE
    ARRAY_TYPE *ptr = ARRAY_REALLOC(array->a, sizeof(ARRAY_TYPE) * size);
    #else
    ARRAY_TYPE *ptr = ARRAY_REALLOC(array->a, sizeof(ARRAY_TYPE) * cap, sizeof(ARRAY_TYPE) * size);
    #endif

    if (ptr == NULL) {
        return false;
    }
    array->a = ptr;
    atomic_store(&array->m, size);
    return true;
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
    return ARRAY_FUNC(resize)(array, cap);
}

static inline bool ARRAY_FUNC(resize_fixed)(ARRAY_NAME *array, size_t size) {
    if (!ARRAY_FUNC(resize)(array, size)) return false;
    array->n = size;
    return true;
}

static inline bool ARRAY_FUNC(push)(ARRAY_NAME *array, ARRAY_TYPE value) {
    size_t i = atomic_fetch_add(&array->i, 1);
    while (i >= atomic_load(&array->m)) {
        if (rwlock_trywrlock(&array->resize_lock) != thrd_busy) {
            if (!ARRAY_FUNC(resize_to_fit)(array, i + 1)) {
                rwlock_unlock(&array->resize_lock);
                return false;
            }
            rwlock_unlock(&array->resize_lock);
        } else {
            thrd_yield();
        }
    }
    if (rwlock_rdlock(&array->resize_lock) == thrd_error) return false;
    array->a[i] = value;
    rwlock_unlock(&array->resize_lock);
    atomic_fetch_add(&array->n, 1);
    return true;
}

static inline bool ARRAY_FUNC(empty)(ARRAY_NAME *array) {
    return atomic_load(&array->n) == 0;
}

static inline void ARRAY_FUNC(clear)(ARRAY_NAME *array) {
    array->n = 0;
}

static inline bool ARRAY_FUNC(copy)(ARRAY_NAME *dst, ARRAY_NAME *src, size_t n) {
    bool ret = true;
    if (dst->m < n) ret = ARRAY_FUNC(resize)(dst, n);
    if (!ret) return false;
    memcpy(dst->a, src->a, n * sizeof(ARRAY_TYPE));
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
    rwlock_destroy(&array->resize_lock);
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
