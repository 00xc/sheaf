/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef __SHEAF_OS
#define __SHEAF_OS

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
		defined(__OpenBSD__) || defined(__NetBSD__)

#include <sched.h>

static inline void __sheaf_os_relax(void)
{
	sched_yield();
}

#elif defined(__WIN32)

#include <windows.h>

static inline void __sheaf_os_relax(void)
{
	SwitchToThread();
}
#else

static inline void __sheaf_os_relax(void)
{
}

#endif

#endif
