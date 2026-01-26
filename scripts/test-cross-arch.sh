#/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
#
# e.g. for riscv:
# $ zypper in cross-riscv64-binutils cross-riscv64-gcc15 cross-riscv64-glibc-devel cross-riscv64-linux-glibc-devel

check_bin() {
	which "$1" >/dev/null
	if [ "$?" != "0" ]; then
		echo "$0: need $1!"
		exit 1
	fi
}

if [ "$#" -ne "1" ]; then
	echo "$0 <arch>"
	exit 1
fi

arch="$1"
case "$arch" in
	"ppc64le")
		# cc="powerpc64le-suse-linux-gcc"
		cross="powerpc64le-suse-linux-"
		ld_path="/usr/powerpc64le-suse-linux/sys-root/"
		lib_path="/usr/lib64/gcc/powerpc64le-suse-linux/15/"
		;;
	*)
		# cc="${arch}-suse-linux-gcc"
		cross="${arch}-suse-linux-"
		ld_path="/usr/${arch}-suse-linux/sys-root/"
		lib_path="/usr/lib64/gcc/${arch}-suse-linux/15/"
		;;
esac

qemu="qemu-${arch}"

check_bin "$qemu"

make clean

QEMU_SET_ENV="LD_LIBRARY_PATH=${lib_path}" \
	QEMU_LD_PREFIX="${ld_path}" \
	make CROSS_COMPILE="$cross" ARCH="$arch" run-tests
