/*!
 * @file atomic.h
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#ifndef __ATOMIC_H__
#define __ATOMIC_H__

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \brief wrapper for OS/compiler specific atomic compare and swap (CAS)
 *         function.
 *
 *  \param addr Address of the variable to CAS
 *  \param tv Test value to compare the value at address against
 *  \param nv New value to set the variable at addr to
 *
 *  \retval 0 CAS failed
 *  \retval 1 CAS succeeded
 */
#define __atomic_compare_and_swap__(addr, tv, nv) \
    __sync_bool_compare_and_swap((addr), (tv), (nv))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and add
 *         function.
 *
 *  \param addr Address of the variable to add to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_fetch_and_add__(addr, value) \
    __sync_fetch_and_add((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and sub
 *         function.
 *
 *  \param addr Address of the variable to add to
 *  \param value Value to sub from the variable at addr
 */
#define __atomic_fetch_and_sub__(addr, value) \
    __sync_fetch_and_sub((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and add
 *         function.
 *
 *  \param addr Address of the variable to add to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_add_and_fetch__(addr, value) \
    __sync_add_and_fetch((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and sub
 *         function.
 *
 *  \param addr Address of the variable to add to
 *  \param value Value to sub from the variable at addr
 */
#define __atomic_sub_and_fetch__(addr, value) \
    __sync_sub_and_fetch((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and "AND"
 *         function.
 *
 *  \param addr Address of the variable to AND to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_fetch_and_and__(addr, value) \
    __sync_fetch_and_and((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and "AND"
 *         function.
 *
 *  \param addr Address of the variable to AND to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_fetch_and_and__(addr, value) \
    __sync_fetch_and_and((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and "NAND"
 *         function.
 *
 *  \param addr Address of the variable to NAND to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_fetch_and_nand__(addr, value) \
    __sync_fetch_and_nand((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and "XOR"
 *         function.
 *
 *  \param addr Address of the variable to XOR to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_fetch_and_xor__(addr, value) \
    __sync_fetch_and_xor((addr), (value))

/**
 *  \brief wrapper for OS/compiler specific atomic fetch and or
 *         function.
 *
 *  \param addr Address of the variable to or to
 *  \param value Value to add to the variable at addr
 */
#define __atomic_fetch_and_or__(addr, value) \
    __sync_fetch_and_or((addr), (value))

/**
 *  \brief wrapper for declaring atomic variables.
 *
 *  \warning Only char, short, int, long, long long and their unsigned
 *           versions are supported.
 *
 *  \param type Type of the variable (char, short, int, long, long long)
 *  \param name Name of the variable.
 *
 *  We just declare the variable here as we rely on atomic operations
 *  to modify it, so no need for locks.
 *
 *  \warning variable is not initialized
 */
#define ATOMIC_DECLARE(type, name) \
    type name ## __atomic__

/**
 *  \brief wrapper for initializing an atomic variable.
 **/
#define ATOMIC_INIT(name) \
    (name ## __atomic__) = 0

/**
 *  \brief wrapper for referencing an atomic variable declared on another file.
 *
 *  \warning Only char, short, int, long, long long and their unsigned
 *           versions are supported.
 *
 *  \param type Type of the variable (char, short, int, long, long long)
 *  \param name Name of the variable.
 *
 *  We just declare the variable here as we rely on atomic operations
 *  to modify it, so no need for locks.
 *
 */
#define ATOMIC_EXTERN(type, name) \
    extern type name ## __atomic__

/**
 *  \brief wrapper for declaring an atomic variable and initializing it.
 **/
#define ATOMIC_DECL_AND_INIT(type, name) \
    type name ## __atomic__ = 0

/**
 *  \brief No-op.
 */
#define ATOMIC_DESTROY(name)

/**
 *  \brief wrapper for reinitializing an atomic variable.
 **/
#define atomic_reset(name) \
    (name ## __atomic__) = 0

/**
 *  \brief add a value to our atomic variable
 *
 *  \param name the atomic variable
 *  \param val the value to add to the variable
 */
#define atomic_add(name, val) \
    __atomic_add_and_fetch__(&(name ## __atomic__), (val))

/**
 *  \brief sub a value from our atomic variable
 *
 *  \param name the atomic variable
 *  \param val the value to sub from the variable
 */
#define atomic_sub(name, val) \
    __atomic_sub_and_fetch__(&(name ## __atomic__), (val))

/**
 *  \brief Bitwise AND a value to our atomic variable
 *
 *  \param name the atomic variable
 *  \param val the value to AND to the variable
 */
#define atomic_and(name, val) \
    __atomic_fetch_and_and__(&(name ## __atomic__), (val))

/**
 *  \brief Bitwise NAND a value to our atomic variable
 *
 *  \param name the atomic variable
 *  \param val the value to NAND to the variable
 */
#define atomic_nand(name, val) \
    __atomic_fetch_and_nand__(&(name ## __atomic__), (val))

/**
 *  \brief Bitwise XOR a value to our atomic variable
 *
 *  \param name the atomic variable
 *  \param val the value to XOR to the variable
 */
#define atomic_xor(name, val) \
    __atomic_fetch_and_xor__(&(name ## __atomic__), (val))

/**
 *  \brief atomic Compare and Switch
 *
 *  \warning "name" is passed to us as "&var"
 */
#define atomic_cas(name, cmpval, newval) \
    __atomic_compare_and_swap__(&(name ## __atomic__), cmpval, newval)

/**
 *  \brief read the value from the atomic variable.
 *
 *  \retval var value
 */
#define atomic_get(name) \
    (name ## __atomic__)

/**
 *  \brief set our atomic variable as a value
 *
 *  \param @name the atomic variable
 *  \param @val the value to set to the variable
 *
 *  Atomically reads the value of @name.
 */
#define atomic_set(name, val) ({       \
    while (atomic_cas(name, atomic_get(name), val) == 0) \
        ;                                                       \
        })

#define atomic_read(name) \
			atomic_get(name)
#define atomic_inc(name) \
			atomic_add(name, 1)
#define atomic_dec(name) \
			atomic_sub(name, 1)

#ifdef __cplusplus
}
#endif /* C++ */

#endif

