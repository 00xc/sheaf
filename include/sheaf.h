/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __SHEAF_H
#define __SHEAF_H

#include <stddef.h>
#include <stdint.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifdef DEBUG
#include <assert.h>
#include <stdio.h>
#define DBG(...) printf(__VA_ARGS__)
#define DBG_ASSERT(arg) assert(arg)
#else
#define DBG(...)
#define DBG_ASSERT(arg)
#endif

/* Select spin relax strategy. Use arch by default */
#if defined(__SHEAF_RELAX_WEAK)
void __attribute__((weak)) __sheaf_relax(void)
{
}
#elif defined(__SHEAF_RELAX_EXTERN)
extern void __sheaf_relax(void);
#elif defined(__SHEAF_RELAX_OS)
#include "os.h"
#define __sheaf_relax() __sheaf_os_relax()
#elif defined(__SHEAF_RELAX_ARCH)
#include "arch.h"
#define __sheaf_relax() __sheaf_arch_relax()
#else
#include "arch.h"
#define __sheaf_relax() __sheaf_arch_relax()
#endif

#include "error.h"

struct sheaf_node {
	/* Next free node */
	struct sheaf_node *next_free;
	/* Next node in the stack */
	struct sheaf_node *next;
	/* CPU number of the owner of this node */
	size_t ncpu;
	/* Value stored in the node */
	uintptr_t val;
};

typedef struct sheaf_node sheaf_node_t;

/* Page allocator provided by the user */
struct pa {
	void *opaque;
	void *(*alloc_page)(void *);
	void (*free_page)(void *, void *);
};

typedef struct pa pa_t;

/* The head of the stack, pointing to the first node */
struct sheaf_head {
	sheaf_node_t *top;
	size_t aba;
} __attribute__((aligned(16)));

typedef struct sheaf_head sheaf_head_t;

typedef uint32_t idx_t;

/* A per-CPU structure */
struct percpu {
	/* Node freelist */
	sheaf_node_t *head;
	/* Deferred ring buffer */
	sheaf_node_t *_Atomic *ring;
	/* Indexes into the ring buffer */
	_Atomic idx_t push __attribute__((aligned(64)));
	_Atomic idx_t pop __attribute__((aligned(64)));
};

typedef struct percpu percpu_t;

struct sheaf {
	/* Head of the stack */
	_Atomic sheaf_head_t head;
	/* Per-CPU array */
	percpu_t *percpu;
	/* Number of items in the percpu array */
	size_t ncpus;
	/* Page allocator provided by the user */
	pa_t *pa;
};

typedef struct sheaf sheaf_t;

percpu_t *percpu_init(size_t ncpus, pa_t *pa);
void percpu_release(percpu_t *percpu, size_t ncpus, pa_t *pa);
sheaf_node_t *percpu_alloc_node(percpu_t *percpu, pa_t *pa);
void percpu_free_node(percpu_t *percpu, sheaf_node_t *node);
void percpu_free_remote_node(percpu_t *src, percpu_t *dst, sheaf_node_t *node);

int sheaf_init(sheaf_t *stack, size_t ncpus, pa_t *pa);
void sheaf_release(sheaf_t *stack);
int sheaf_push(sheaf_t *stack, uintptr_t val, size_t ncpu);
int sheaf_pop(sheaf_t *stack, uintptr_t *val, size_t ncpu);

#endif
