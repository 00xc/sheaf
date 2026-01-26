/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __SHEAF_ARCH
#define __SHEAF_ARCH

#if defined(__x86_64__) || defined(__i386__)

#include <immintrin.h>

static inline void __sheaf_arch_relax(void)
{
	_mm_pause();
}

#elif defined(__aarch64__) || defined(_M_ARM64)

static inline void __sheaf_arch_relax(void)
{
	__asm__ volatile("isb sy" ::: "memory");
}

#elif defined(__riscv__)

static inline void __sheaf_arch_relax(void)
{
	__builtin_riscv_pause();
}

#else

static inline void __sheaf_arch_relax(void)
{
}

#endif

#endif /* __SHEAF_ARCH  */
