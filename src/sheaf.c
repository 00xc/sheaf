// SPDX-License-Identifier: BSD-2-Clause
#include <stdatomic.h>
#include <stddef.h>

#include "sheaf.h"

void sheaf_release(sheaf_t *stack)
{
	int ret;

	if (!stack)
		return;

	do {
		ret = sheaf_pop(stack, NULL, 0);
	} while (ret != -SHEAF_EAGAIN);
	percpu_release(stack->percpu, stack->ncpus, stack->pa);
}

int sheaf_init(sheaf_t *stack, size_t ncpus, pa_t *pa)
{
	if (!stack || !ncpus)
		return -SHEAF_EINVAL;

	stack->pa = pa;
	stack->ncpus = ncpus;
	atomic_init(&stack->head, (sheaf_head_t){ 0 });

	stack->percpu = percpu_init(ncpus, pa);
	if (!stack->percpu)
		return -SHEAF_ENOMEM;

	return 0;
}

int sheaf_push(sheaf_t *stack, uintptr_t val, size_t ncpu)
{
	sheaf_head_t head, new;
	sheaf_node_t *node;

	if (!stack || ncpu >= stack->ncpus)
		return -SHEAF_EINVAL;

	node = percpu_alloc_node(&stack->percpu[ncpu], stack->pa);
	if (!node)
		return -SHEAF_ENOMEM;

	node->val = val;
	node->ncpu = ncpu;

	head = atomic_load(&stack->head);
	while (1) {
		node->next = head.top;
		new.top = node;
		new.aba = head.aba + 1;
		if (atomic_compare_exchange_weak(&stack->head, &head, new))
			break;
		__sheaf_relax();
	};

	DBG("t=%02lu Updated head (push): (%p, %lu) -> (%p, %lu)\n", ncpu,
		(void *)head.top, head.aba, (void *)new.top, new.aba);

	return 0;
}

int sheaf_pop(sheaf_t *stack, uintptr_t *ret, size_t ncpu)
{
	sheaf_head_t head, new;
	sheaf_node_t *node;
	percpu_t *percpus;

	if (!stack || ncpu >= stack->ncpus)
		return -SHEAF_EINVAL;

	head = atomic_load(&stack->head);
	while (1) {
		if (!head.top)
			return -SHEAF_EAGAIN;
		new.top = head.top->next;
		new.aba = head.aba + 1;
		if (atomic_compare_exchange_weak(&stack->head, &head, new))
			break;
		__sheaf_relax();
	};

	DBG("t=%02lu Updated head (pop):  (%p, %lu) -> (%p, %lu)\n", ncpu,
		(void *)head.top, head.aba, (void *)new.top, new.aba);

	node = head.top;
	if (ret)
		*ret = node->val;

	/* Now free this node. If it is in our percpu pool we can do it
	 * ourselves. If not, we need to push it to that cpu's ringbuffer */
	percpus = stack->percpu;
	if (node->ncpu == ncpu)
		percpu_free_node(&percpus[ncpu], node);
	else
		percpu_free_remote_node(&percpus[ncpu], &percpus[node->ncpu], node);

	return 0;
}
