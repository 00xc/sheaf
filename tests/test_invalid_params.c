/* SPDX-License-Identifier: BSD-2-Clause */
#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "libtest.h"
#include "sheaf.h"

static int check_init(sheaf_t *stack, size_t ncpus, pa_t *pa, int exp)
{
	int ret;

	ret = sheaf_init(stack, ncpus, pa);
	if (ret != exp) {
		warnx("sheaf_init(stack=%p, ncpus=%lu, pa=%p): returned %d, expected %d",
			  (void *)stack, ncpus, (void *)pa, ret, exp);
		return 1;
	}

	return 0;
}

static int check_push(sheaf_t *stack, uintptr_t val, size_t ncpu, int exp)
{
	int ret;

	ret = sheaf_push(stack, val, ncpu);
	if (ret != exp) {
		warnx("sheaf_push(stack=%p, val=%p, ncpu=%lu): returned %d, expected %d",
			  (void *)stack, (void *)val, ncpu, ret, exp);
		return 1;
	}

	return 0;
}

static int check_pop(sheaf_t *stack, size_t ncpu, int exp, uintptr_t exp_val)
{
	int ret;
	uintptr_t val = 0;

	ret = sheaf_pop(stack, &val, ncpu);
	if (ret != exp) {
		warnx("sheaf_pop(stack=%p, val=0x_, ncpu=%lu): returned %d, expected %d",
			  (void *)stack, ncpu, ret, exp);
		return 1;
	}

	if (!ret) {
		if (val != exp_val) {
			warnx("sheaf_pop(stack=%p, val=0x_, ncpu=%lu): returned value %p, expected %p",
				  (void *)stack, ncpu, (void *)val, (void *)exp_val);
			return 1;
		}
	}

	return 0;
}

#define VAL 0xdeadbeefUL
#define BAD_VAL 0xbadbabeUL

#define NCPUS 8UL
#define BAD_NCPUS ((PAGE_SIZE / sizeof(percpu_t)) + 1)

#define CPU 0UL
#define BAD_CPU NCPUS

int main(int argc, const char *argv[])
{
	sheaf_t stack;
	int ret;

	(void)argc;
	(void)argv;

	/* No stack */
	ret = check_init(NULL, NCPUS, &pa, -SHEAF_EINVAL);
	if (ret)
		return EXIT_FAILURE;

	/* No allocator */
	ret = check_init(&stack, NCPUS, NULL, -SHEAF_ENOMEM);
	if (ret)
		return EXIT_FAILURE;

	/* No CPUs */
	ret = check_init(&stack, 0, &pa, -SHEAF_EINVAL);
	if (ret)
		return EXIT_FAILURE;

	/* Too many CPUs */
	ret = check_init(&stack, BAD_NCPUS, &pa, -SHEAF_ENOMEM);
	if (ret)
		return EXIT_FAILURE;

	/* Good parameters */
	ret = check_init(&stack, NCPUS, &pa, 0);
	if (ret)
		return EXIT_FAILURE;

	/* Push on NULL stack */
	ret = check_push(NULL, BAD_VAL, CPU, -SHEAF_EINVAL);
	if (ret)
		return EXIT_FAILURE;

	/* Push for invalid CPU */
	ret = check_push(&stack, BAD_VAL, BAD_CPU, -SHEAF_EINVAL);
	if (ret)
		return EXIT_FAILURE;

	/* Push for good CPU */
	ret = check_push(&stack, VAL, CPU, 0);
	if (ret)
		return EXIT_FAILURE;

	/* Pop for NULL stack */
	ret = check_pop(NULL, CPU, -SHEAF_EINVAL, 0);
	if (ret)
		return EXIT_FAILURE;

	/* Pop for invalid CPU */
	ret = check_pop(&stack, BAD_CPU, -SHEAF_EINVAL, 0);
	if (ret)
		return EXIT_FAILURE;

	/* Pop for good CPU */
	ret = check_pop(&stack, CPU, 0, VAL);
	if (ret)
		return EXIT_FAILURE;

	/* Pop on empty stack */
	ret = check_pop(&stack, CPU, -SHEAF_EAGAIN, 0);
	if (ret)
		return EXIT_FAILURE;

	sheaf_release(&stack);

	return EXIT_SUCCESS;
}
