# concurrent_array
High-performance concurrent/thread-safe, generic, dynamic (push-only) array using read-write locks (write lock only held for resizing) and C11 atomics to ensure unique indices for each push or extend.

Currently Linux/Mac, compiles on Windows but tests not passing currently.

