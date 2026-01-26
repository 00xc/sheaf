/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef _SHEAF_LIBTEST_H
#define _SHEAF_LIBTEST_H

#include <bits/pthreadtypes.h>
#include <err.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "sheaf.h"

static void *alloc_page(void *opaque)
{
	void *page = NULL;

	(void)opaque;
	if (posix_memalign(&page, PAGE_SIZE, PAGE_SIZE))
		errx(EXIT_FAILURE, "posix_memalign() failed");

	return page;
}

static void free_page(void *opaque, void *page)
{
	(void)opaque;
	free(page);
}

pa_t pa = {
	.alloc_page = alloc_page,
	.free_page = free_page,
};

static inline void barrier_wait(pthread_barrier_t *barrier)
{
	int ret;

	ret = pthread_barrier_wait(barrier);
	if (ret && ret != PTHREAD_BARRIER_SERIAL_THREAD)
		err(EXIT_FAILURE, "pthread_barrier_wait");
}

static inline void pin_to_core(size_t id, int num_cores)
{
	int core = (int)id % num_cores;
	cpu_set_t cpuset;
	pthread_t self = pthread_self();

	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	if (pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset))
		err(EXIT_FAILURE, "pthread_setaffinity_np");
}

#endif
