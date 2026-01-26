#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
#
# This script is called from bench.sh, which launches hyperfine, passing this
# script as a preparation step to launch a benchmark run

impl=$1
thrd=$2
yield=$3
cc=$4
page_size=$5

if [ "$#" -lt "3" ]; then
	echo "$0 <impl> <threads> <yield> [<cc>] [<page_size>]"
	exit 1
fi

[ -z "${cc}" ] && cc=clang
[ -z "${page_size}" ] && page_size=0x1000UL

nelems=$((0x800000 / thrd))
cflags="-march=native -mcx16 -DNTHREADS=${thrd}UL -DNELEMS=${nelems}UL -DPAGE_SIZE=${page_size} -ggdb"

case "$impl" in
	"baseline")
		target=tests/test_baseline
		case "$yield" in
			"0") ;;
			"1") cflags="$cflags -D_BASELINE_MUTEX" ;;
			*) echo "$0: invalid yield: '${yield}'"; exit 1 ;;
		esac
		;;
	"sheaf")
		target=tests/test_stack_total_elems
		case "$yield" in
			"0") cflags="${cflags} -D__SHEAF_RELAX_ARCH" ;;
			"1") cflags="${cflags} -D__SHEAF_RELAX_OS" ;;
			*) echo "$0: invalid yield: '${yield}'"; exit 1 ;;

		esac
		;;
	*) echo "$0: invalid impl: '${impl}'"; exit 1 ;;
esac

base=$(git rev-parse --show-toplevel)
make -C "$base" clean
rm -f under-test

make -C "$base" CC="$cc" CFLAGS="$cflags" "$target" -j"$(nproc)" && \
	cp $target under-test
