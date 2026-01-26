#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

base=$(git rev-parse --show-toplevel)
build="$base/scripts/build_bench.sh"

hyperfine --shell none \
	--warmup 5 \
	--parameter-list cc clang \
	--parameter-list page_size 0x1000UL,0x200000UL \
	--parameter-list yield 0,1 \
	--parameter-list impl sheaf \
	--parameter-list threads 1,2,4,8,16 \
	--prepare "$build {impl} {threads} {yield} clang {page_size}" \
	--export-csv clang-sheaf-page_size.csv \
	"nice -20 ./under-test"

	# --parameter-list impl baseline,sheaf \
	# --parameter-list cc gcc,clang \
