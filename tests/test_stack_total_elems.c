/* SPDX-License-Identifier: BSD-2-Clause */
#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "libtest.h"
#include "sheaf.h"

#ifndef NTHREADS
#define NTHREADS 8UL
#endif

#ifndef NELEMS
#define NELEMS 0x2000UL
#endif

static size_t _Atomic counters[NTHREADS] = { 0 };

struct args {
	sheaf_t *stack;
	size_t count;
	uintptr_t val;
	size_t id;
	pthread_barrier_t *barrier;
	int num_cores;
};

static void *push_worker(void *ctx)
{
	struct args *args = ctx;
	sheaf_t *stack = args->stack;
	size_t i;
	int ret;

	DBG("Starting thread ID = %lu\n", args->id);
	pin_to_core(args->id, args->num_cores);
	barrier_wait(args->barrier);

	for (i = 0; i < args->count; ++i) {
		do {
			ret = sheaf_push(stack, args->val, args->id);
		} while (ret == -ENOMEM);

		if (ret)
			errx(EXIT_FAILURE, "sheaf_push: %s", strerror(-ret));
	}

	return NULL;
}

static void *pop_worker(void *ctx)
{
	struct args *args = ctx;
	sheaf_t *stack = args->stack;
	int ret;
	size_t i = 0;
	uintptr_t val;

	DBG("Starting thread ID = %lu\n", args->id);
	pin_to_core(args->id, args->num_cores);
	barrier_wait(args->barrier);

	for (i = 0; i < args->count; ++i) {
		do {
			ret = sheaf_pop(stack, &val, args->id);
		} while (ret == -EAGAIN);

		if (ret)
			errx(EXIT_FAILURE, "sheaf_pop: %s", strerror(-ret));
		atomic_fetch_add_explicit(&counters[val], 1, memory_order_relaxed);
	}

	return NULL;
}

static void args_init(struct args *arg, sheaf_t *stack,
					  pthread_barrier_t *barrier, size_t i, int num_cores)
{
	arg->stack = stack;
	arg->count = NELEMS;
	arg->val = i;
	arg->id = i;
	arg->barrier = barrier;
	arg->num_cores = num_cores;
}

int main(int argc, const char *argv[])
{
	sheaf_t stack;
	size_t i;
	pthread_t thrds[NTHREADS * 2];
	struct args args[NTHREADS * 2], *arg;
	pthread_barrier_t barrier;
	int ret;
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

	ret = sheaf_init(&stack, NTHREADS * 2, &pa);
	if (ret)
		errx(EXIT_FAILURE, "sheaf_init: %s", strerror(ret));

	for (i = 0; i < NTHREADS; ++i) {
		arg = &args[i];
		args_init(arg, &stack, &barrier, i, num_cores);
		if (pthread_create(&thrds[i], NULL, push_worker, arg))
			err(EXIT_FAILURE, "pthread_create");
	}

	for (i = NTHREADS; i < NTHREADS * 2; ++i) {
		arg = &args[i];
		args_init(arg, &stack, &barrier, i, num_cores);
		if (pthread_create(&thrds[i], NULL, pop_worker, arg))
			err(EXIT_FAILURE, "pthread_create");
	}

	for (i = 0; i < NTHREADS * 2; ++i) {
		if (pthread_join(thrds[i], NULL))
			warn("pthread_join");
	}

	sheaf_release(&stack);
	pthread_barrier_destroy(&barrier);

	for (i = 0; i < NTHREADS; ++i)
		assert(counters[i] == NELEMS);

	return EXIT_SUCCESS;
}
