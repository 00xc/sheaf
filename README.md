# sheaf

A dynamically-sized, concurrent, lock-free, multi-producer / multi-consumer
stack for bare-metal environments.

## Memory allocation

The user of the library only needs to provide a page allocator. The library
will manage memory in chunks of `PAGE_SIZE` and will use it to store stack
nodes. This means that the stack can grow dynamically, but the user can limit
its memory consumption. The stack is able to gracefully handle allocation
failures, simply rejecting pushes if all memory is used and none is reclaimed
by popping from the stack.

## Spin relax strategy

When the library is spinning on a value, e.g. attemping to compare-and-swap, it
it sometimes more efficient to let the winning thread to make progress first
and then take an attempt again. There are several ways of doing this depending
on the platform and architecture.

By default, the library uses an architecture-specific method to relax on a
spinning loop, which is not the most efficient, but the most portable. To
change the strategy you may define one of the following:

* `__SHEAF_RELAX_ARCH` (default): an architecture-specific instruction is
  emitted to signal the processor that the CPU is waiting in a busy loop. Upon
  receiving the spin-loop signal the processor can optimize its behavior by, for
  example, saving power or switching hyper-threads.
* `__SHEAF_RELAX_OS`: uses a OS-specific method of yielding to the scheduler.
  Currently, this supports Linux, Windows, MacOS, FreeBSD, OpenBSD and NetBSD.
  This is the most efficient strategy. On unsupported OSes no action will be
  taken when spinning.
* `__SHEAF_RELAX_EXTERN`: `__sheaf_relax()` is defined as an extern function
  that the library user must implement, and which the library will call at the
  appropriate times.
* `__SHEAF_RELAX_WEAK`: same as `__SHEAF_RELAX_EXTERN`, but the function is
  defined as a symbol with weak linkage, meaning that if the user does not
  implement it, the function will be a no-nop and will not cause a link-time
  error.

To select the strategy, simply add a C-level define. For example:

```shell
make CFLAGS="-D__SHEAF_RELAX_OS"
```

See the following section for more details on how to build the library.

# Building

Shared and static libraries:

```shell
make
```

Build with clang:

```shell
make LLVM=1
```

Run tests:

```shell
make run-tests
# or with clang
make run-tests LLVM=1
```

## Cross-compiling

With a GCC-based toolchain:

```shell
make ARCH=$arch CROSS_COMPILE=$cross_compiler_prefix
```

With an LLVM-based toolchain (clang):

```shell
make ARCH=$arch LLVM=1
```

You may override the compiler or linker via `CC` and `LD` respectively:

```shell
CC=clang-13 make ARCH=$arch LLVM=1
```

To cross-compile and run tests there are several requirements. The library is
built for a bare-metal environment by default, but tests are built for the host
system. If you want to run cross-compiled tests, you will need the right
`qemu-user` variant installed and `binfmt` properly configured. If the Makefile
is not able to find a sysroot to link the required Linux libraries
(e.g. libatomic), you may have to supply it:

```shell
make run-tests ARCH=$arch LLVM=1 TEST_CFLAGS="--sysroot=$sysroot_path"
```

# License

This project is licensed under the BSD 2-Clause License.

**Note**: `.clang-format` is taken from the Linux kernel and is licensed under GPL-2.0.
