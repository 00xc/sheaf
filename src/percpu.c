// SPDX-License-Identifier: BSD-2-Clause
#include <stdatomic.h>
#include <stdint.h>

#include "sheaf.h"

static inline uintptr_t pa_alloc(pa_t *pa)
{
	void *ptr = NULL;

	if (pa && pa->alloc_page)
		ptr = pa->alloc_page(pa->opaque);

	return (uintptr_t)ptr;
}

static inline void pa_free(pa_t *pa, void *addr)
{
	if (pa && pa->free_page && addr)
		pa->free_page(pa->opaque, addr);
}

static inline idx_t rbuf_bump(idx_t val)
{
	return (val + 1) % (PAGE_SIZE / sizeof(sheaf_node_t *));
}

static inline int rbuf_full(idx_t push, idx_t pop)
{
	return rbuf_bump(push) == pop;
}

static inline int rbuf_empty(idx_t push, idx_t pop)
{
	return push == pop;
}

static void percpu_consume_deferred(percpu_t *pc)
{
	idx_t push, pop = atomic_load(&pc->pop);
	sheaf_node_t *node;

	while (1) {
		push = atomic_load(&pc->push);
		if (rbuf_empty(push, pop))
			break;

		/* Read the next entry. If it is NULL, the other end has reserved
		 * the index but is in the process of writing to it, so wait  */
		while (1) {
			node = atomic_exchange(&pc->ring[pop], NULL);
			if (node)
				break;
			__sheaf_relax();
		}

		/* Point to the next entry, and add the current entry to our
		 * freelist */
		pop = rbuf_bump(pop);
		percpu_free_node(pc, node);
	}

	/* Bump our index so that new entries can be pushed. */
	atomic_store_explicit(&pc->pop, pop, memory_order_release);
}

void percpu_free_remote_node(percpu_t *src, percpu_t *dst, sheaf_node_t *node)
{
	uint32_t pop, push = atomic_load(&dst->push);

	while (1) {
		pop = atomic_load(&dst->pop);

		/* If the receiving end has no more room then take over the node */
		if (rbuf_full(push, pop)) {
			percpu_free_node(src, node);
			break;
		}

		/* Attempt to reserve the next index. If successful, write the
		 * value. The consumer thread will wait until the entry is
		 * populated with a non-NULL value */
		if (atomic_compare_exchange_weak_explicit(
					&dst->push, &push, rbuf_bump(push), memory_order_acq_rel,
					memory_order_acquire)) {
			atomic_store_explicit(&dst->ring[push], node, memory_order_release);
			break;
		}
		__sheaf_relax();
	}
}

void percpu_free_node(percpu_t *percpu, sheaf_node_t *node)
{
	node->next_free = percpu->head;
	percpu->head = node;
}

static sheaf_node_t *percpu_alloc_page(pa_t *pa)
{
	sheaf_node_t *page, *node;
	size_t i, max = PAGE_SIZE / sizeof(*node);

	page = (sheaf_node_t *)pa_alloc(pa);
	if (!page)
		return NULL;

	for (i = 0; i < max - 1; ++i) {
		node = &page[i];
		node->next_free = node + 1;
	}
	page[max - 1].next_free = NULL;
	return page;
}

sheaf_node_t *percpu_alloc_node(percpu_t *percpu, pa_t *pa)
{
	sheaf_node_t *node;

	if (!percpu->head)
		percpu_consume_deferred(percpu);

	node = percpu->head;
	if (!node) {
		node = percpu_alloc_page(pa);
		if (!node)
			return NULL;
	}

	percpu->head = node->next_free;
	return node;
}

static int percpu_init_single(percpu_t *pc, pa_t *pa)
{
	pc->head = NULL;
	atomic_init(&pc->push, 0);
	atomic_init(&pc->pop, 0);

	pc->ring = (sheaf_node_t * _Atomic *)pa_alloc(pa);
	if (!pc->ring)
		return 1;
	__builtin_memset(pc->ring, 0, PAGE_SIZE);

	/* Pre-allocate the first node page */
	pc->head = percpu_alloc_page(pa);
	if (!pc->head) {
		pa_free(pa, pc->ring);
		return 1;
	}

	return 0;
}

percpu_t *percpu_init(size_t ncpus, pa_t *pa)
{
	percpu_t *percpus;
	size_t i;

	/* We need to fit all percpu structures in a single page */
	if (ncpus * sizeof(*percpus) > PAGE_SIZE)
		return NULL;

	percpus = (percpu_t *)pa_alloc(pa);
	if (!percpus)
		return NULL;

	for (i = 0; i < ncpus; ++i) {
		if (percpu_init_single(&percpus[i], pa)) {
			percpu_release(percpus, i, pa);
			return NULL;
		}
	}

	return percpus;
}

static inline uintptr_t align_to(uintptr_t addr, size_t align)
{
	return addr & ~(align - 1);
}

static inline uintptr_t page_align(uintptr_t addr)
{
	return align_to(addr, PAGE_SIZE);
}

static inline int is_page_aligned(uintptr_t addr)
{
	return page_align(addr) == addr;
}

#define POINTERS_PER_PAGE (PAGE_SIZE / sizeof(uintptr_t))

static int percpu_release_nodes(percpu_t *percpu, void **accounting,
								size_t *pages_found)
{
	sheaf_node_t *node;
	int ret = 0;
	size_t num_pages = *pages_found;

	if (num_pages >= POINTERS_PER_PAGE)
		return -1;

	while (percpu->head) {
		node = percpu->head;
		percpu->head = node->next_free;

		if (!is_page_aligned((uintptr_t)node))
			continue;

		accounting[num_pages++] = (void *)node;
		if (num_pages >= POINTERS_PER_PAGE) {
			ret = -1;
			break;
		}
	}

	*pages_found = num_pages;
	return ret;
}

void percpu_release(percpu_t *percpu, size_t ncpus, pa_t *pa)
{
	size_t i, j, pages_found = 0, acc_pages = 0;
	void **accounting;

	if (!percpu)
		return;

	/*
	 * Now begins a complicated cleanup process. The main issue is that
	 * CPUs/threads can take over each other's nodes, so it is hard to
	 * keep track of all the pages used.
	 *
	 * The idea here is to reuse the deferred ring buffer page of the
	 * CPU 0 to account for all the pages that need to be freed. Go over
	 * each per-CPU structure, and for each one, traverse the free list
	 * Whenever we find a page-aligned address, store it into the ring
	 * buffer page (aka "accounting" page) page. Whenever we fill that
	 * page with addresses, take the ring buffer page of the next CPU.
	 * If we run out of accounting pages, simply bail, as there is
	 * nothing we can do.
	 *
	 * We cannot free any pages until we've traversed all per-CPU
	 * freelists, as there are no guarantees in terms of what CPU will
	 * have a node belonging to a given page - IOW, freeing a page that
	 * we found in CPU 0's freelist might incur in a UAF if a node in
	 * that page is linked from the CPU 1's freelist.
	 *
	 * All this complexity avoids having to allocate pages while we
	 * attempt to give back all the resources, which is needed if the
	 * caller is precisely asking for its pages back because it ran out
	 * of them.
	 */
	accounting = (void **)percpu[acc_pages++].ring;

	for (i = 0; i < ncpus; ++i)
		percpu_consume_deferred(&percpu[i]);

	for (i = 0; i < ncpus; ++i) {
		while (percpu_release_nodes(&percpu[i], accounting, &pages_found)) {
			/* If we ran out of accounting pages just skip this
			 * per-CPU. This will leak memory but it's all we can do */
			if (acc_pages >= ncpus) {
				DBG("WARN: leaking pages\n");
				break;
			}
			accounting = (void **)percpu[acc_pages++].ring;
			pages_found = 0;
		}
	}

	/* Now we can free all the pages at once. Go over each per-CPU, and
	 * if we used it's ring buffer page as an accounting page, free the
	 * pages listed there first. Finally, free the accounting page
	 * itself */
	for (i = 0; i < ncpus; ++i) {
		accounting = (void **)percpu[i].ring;

		if (i < acc_pages) {
			size_t lim;

			/* If this is the last accounting page we used, we might not
			 * have filled it to the end */
			if (i == acc_pages - 1)
				lim = pages_found;
			else
				lim = POINTERS_PER_PAGE;
			for (j = 0; j < lim; ++j)
				pa_free(pa, accounting[j]);
		}

		pa_free(pa, accounting);
	}

	pa_free(pa, percpu);
}
