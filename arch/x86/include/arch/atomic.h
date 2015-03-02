/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_ATOMIC_H
#define INCLUDE_ARCH_ATOMIC_H

#include <protura/compiler.h>
#include <protura/types.h>
#include <arch/asm.h>

typedef struct {
    int32_t counter;
} atomic32_t;

typedef struct {
    int64_t counter;
} atomic64_t;

#define ATOMIC32_INIT(i) { (i) }
#define ATOMIC64_INIT(i) { (i) }

static __always_inline int32_t atomic32_get(const atomic32_t *v)
{
    return (*(volatile int32_t *)&(v)->counter);
}

static __always_inline void atomic32_set(atomic32_t *v, int32_t i)
{
    v->counter = i;
}

static __always_inline void atomic32_add(atomic32_t *v, int32_t i)
{
    asm volatile(LOCK_PREFIX "addl %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

static __always_inline void atomic32_sub(atomic32_t *v, int32_t i)
{
    asm volatile(LOCK_PREFIX "subl %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

static __always_inline void atomic32_inc(atomic32_t *v)
{
    asm volatile(LOCK_PREFIX "incl %0"
            : "+m" (v->counter));
}

static __always_inline void atomic32_dec(atomic32_t *v)
{
    asm volatile(LOCK_PREFIX "decl %0"
            : "+m" (v->counter));
}

#endif
