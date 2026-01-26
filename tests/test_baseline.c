/* SPDX-License-Identifier: BSD-2-Clause */
#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include "libtest.h"

#ifndef NTHREADS
#define NTHREADS 8UL
#endif

#ifndef NELEMS
#define NELEMS 0x2000UL
#endif

#ifdef _BASELINE_MUTEX

typedef pthread_mutex_t lock_t;

static inline void lock_init(lock_t *lock)
{
	if (pthread_mutex_init(lock, NULL))
		err(EXIT_FAILURE, "pthread_mutex_init");
}

static inline void lock_destroy(lock_t *lock)
{
	if (pthread_mutex_destroy(lock))
		warn("pthread_mutex_destroy");
}

static inline void lock_lock(lock_t *lock)
{
	while (pthread_mutex_lock(lock)) {
	}
}

static inline void lock_unlock(lock_t *lock)
{
	while (pthread_mutex_unlock(lock)) {
	}
}

#else

#include "arch.h"

typedef pthread_spinlock_t lock_t;

static inline void lock_init(lock_t *lock)
{
	if (pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE))
		err(EXIT_FAILURE, "pthread_spin_init");
}

static inline void lock_destroy(lock_t *lock)
{
	if (pthread_spin_destroy(lock))
		warn("pthread_mutex_destroy");
}

static inline void lock_lock(pthread_spinlock_t *lock)
{
	int ret;

	while (1) {
		ret = pthread_spin_trylock(lock);
		if (!ret)
			break;
		__sheaf_arch_relax();
	}
}

static inline void lock_unlock(lock_t *lock)
{
	while (pthread_spin_unlock(lock)) {
	}
}

#endif

typedef struct node {
	struct node *next;
	uintptr_t val;
} node_t;

typedef struct stack {
	struct node *head;
} stack_t;

static node_t *stack_pop(stack_t *stack)
{
	node_t *node = NULL;

	node = stack->head;
	if (node)
		stack->head = node->next;
	return node;
}

static void stack_push(stack_t *stack, node_t *node)
{
	node->next = stack->head;
	stack->head = node;
}

static void stack_release(stack_t *stack)
{
	node_t *node;

	do {
		node = stack_pop(stack);
		if (node)
			free(node);
	} while (node);
}

static _Atomic size_t counters[NTHREADS] = { 0 };

struct args {
	lock_t *lock;
	stack_t *stack;
	size_t count;
	uintptr_t val;
	size_t id;
	pthread_barrier_t *barrier;
	int num_cores;
};

static void *push_worker(void *ctx)
{
	struct args *args = ctx;
	stack_t *stack = args->stack;
	node_t *node;
	size_t i;

	pin_to_core(args->id, args->num_cores);
	barrier_wait(args->barrier);

	for (i = 0; i < args->count; ++i) {
		do {
			node = malloc(sizeof(*node));
		} while (!node);
		node->val = args->val;

		lock_lock(args->lock);
		stack_push(stack, node);
		lock_unlock(args->lock);
	}

	return NULL;
}

static void *pop_worker(void *ctx)
{
	struct args *args = ctx;
	stack_t *stack = args->stack;
	node_t *node;
	size_t i = 0;

	pin_to_core(args->id, args->num_cores);
	barrier_wait(args->barrier);

	for (i = 0; i < args->count; ++i) {
		do {
			lock_lock(args->lock);
			node = stack_pop(stack);
			lock_unlock(args->lock);
		} while (!node);

		atomic_fetch_add_explicit(&counters[node->val], 1,
								  memory_order_relaxed);
		free(node);
	}

	return NULL;
}

static void args_init(struct args *arg, stack_t *stack, lock_t *lock,
					  pthread_barrier_t *barrier, size_t i, int num_cores)
{
	arg->lock = lock;
	arg->stack = stack;
	arg->count = NELEMS;
	arg->val = i;
	arg->id = i;
	arg->barrier = barrier;
	arg->num_cores = num_cores;
}

int main(int argc, const char *argv[])
{
	stack_t stack = { 0 };
	size_t i;
	pthread_t thrds[NTHREADS * 2];
	struct args args[NTHREADS * 2], *arg;
	lock_t lock;
	pthread_barrier_t barrier;
	int num_cores;

	(void)argc;
	(void)argv;

	num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (num_cores <= 0)
		err(EXIT_FAILURE, "sysconf(_SC_NPROCESSORS_ONLN)");

	if (pthread_setconcurrency(num_cores))
		err(EXIT_FAILURE, "pthread_setconcurrency");

	if (pthread_barrier_init(&barrier, NULL, NTHREADS * 2))
		err(EXIT_FAILURE, "pthread_barrier_init");

	lock_init(&lock);

	for (i = 0; i < NTHREADS; ++i) {
		arg = &args[i];
		args_init(arg, &stack, &lock, &barrier, i, num_cores);
		if (pthread_create(&thrds[i], NULL, push_worker, arg))
			err(EXIT_FAILURE, "pthread_create");
	}

	for (i = NTHREADS; i < NTHREADS * 2; ++i) {
		arg = &args[i];
		args_init(arg, &stack, &lock, &barrier, i, num_cores);
		if (pthread_create(&thrds[i], NULL, pop_worker, arg))
			err(EXIT_FAILURE, "pthread_create");
	}

	for (i = 0; i < NTHREADS * 2; ++i) {
		if (pthread_join(thrds[i], NULL))
			warn("pthread_join");
	}

	lock_destroy(&lock);
	pthread_barrier_destroy(&barrier);
	stack_release(&stack);

	for (i = 0; i < NTHREADS; ++i)
		assert(counters[i] == NELEMS);

	return EXIT_SUCCESS;
}
