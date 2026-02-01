#ifdef __CLANG_TIDY__
#    ifndef __STDATOMIC_H_WRAPPER__
#        define __STDATOMIC_H_WRAPPER__
#        define _Atomic(T) T

typedef _Bool atomic_bool;
typedef char atomic_char;
typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef unsigned int atomic_uint_fast32_t;
typedef unsigned long atomic_uint_fast64_t;

typedef enum {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst,
} memory_order;

/* 2. 기존 함수형 매크로 모킹 */
#        define atomic_init(obj, val)     (*(obj) = (val))
#        define atomic_store(obj, val)    (*(obj) = (val))
#        define atomic_load(obj)          (*(obj))
#        define atomic_exchange(obj, des) (*(obj) = (des), (des))

#        define atomic_compare_exchange_strong(obj, exp, des)                                      \
            (*(obj) == *(exp) ? (*(obj) = (des), 1) : (*(exp) = *(obj), 0))

#        define atomic_compare_exchange_strong_explicit(obj, exp, des, succ, fail)                 \
            atomic_compare_exchange_strong(obj, exp, des)

/* 3. fetch 계열 및 explicit 매크로 추가 */
#        define atomic_fetch_add(obj, delta)                 (*(obj) += (delta))
#        define atomic_fetch_add_explicit(obj, delta, order) atomic_fetch_add(obj, delta)

#        define atomic_fetch_sub(obj, delta)                 (*(obj) -= (delta))
#        define atomic_fetch_sub_explicit(obj, delta, order) atomic_fetch_sub(obj, delta)

#        define atomic_fetch_or(obj, val)                 (*(obj) |= (val))
#        define atomic_fetch_or_explicit(obj, val, order) atomic_fetch_or(obj, val)

#        define atomic_fetch_and(obj, val)                 (*(obj) &= (val))
#        define atomic_fetch_and_explicit(obj, val, order) atomic_fetch_and(obj, val)

#        define atomic_fetch_xor(obj, val)                 (*(obj) ^= (val))
#        define atomic_fetch_xor_explicit(obj, val, order) atomic_fetch_xor(obj, val)

#    endif /* __STDATOMIC_H_WRAPPER__ */

#else
#    include_next <stdatomic.h>

#endif /* __CLANG_TIDY__ */
