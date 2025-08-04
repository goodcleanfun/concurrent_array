# concurrent_array
Concurrent/thread-safe, generic, dynamic (push-only) array using a read-write spinlock and C11 atomics to ensure unique indices for each push or extend.

Reads/writes for the lock's purposes are relative to the array pointer location, not the array's data, i.e. a push in most cases only requires a non-exclusive read lock. Only when the array hits capacity and needs to be resized is an exclusive write lock required since resizing may change the pointer and it would no longer be safe to read from that memory location.

Focuses mostly on thread-safety and correctness rather than performance, though the extend operation is particularly efficient in a parallel setting if each thread can batch its inserts as it will incur fewer atomic operations and needs only a single read lock acquisition. The API includes a push_get_index and extend_get_index to obtain the position of the element/sub-array just inserted under concurrency.
