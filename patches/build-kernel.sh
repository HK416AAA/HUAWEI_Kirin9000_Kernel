#!/usr/bin/env bash
# Build the WGR-AN19 (Kirin 9000) 5.10.43 kernel with the two port patches.
# Runs inside GitHub Actions (ubuntu-22.04); can also be used locally on Linux.
set -euo pipefail

KERNEL_DIR="${KERNEL_DIR:-kernel}"
OUT_DIR="${OUT_DIR:-out}"
JOBS="${JOBS:-$(nproc)}"

echo "== kernel tree: ${KERNEL_DIR}"
cd "${KERNEL_DIR}"

echo "== applying port patches"
for p in ../patches/0001-binder-async-sender-pid.patch \
         ../patches/0002-rdr-no-ap-reset.patch; do
    if git apply --check "$p" 2>/dev/null; then
        git apply "$p"
        echo "   applied $(basename "$p")"
    elif patch -p1 --dry-run < "$p" >/dev/null 2>&1; then
        patch -p1 < "$p"
        echo "   applied $(basename "$p") (patch -p1)"
    else
        echo "!! cannot apply $(basename "$p") — already applied?"
    fi
done

echo "== configuring"
cp ../patches/device_defconfig arch/arm64/configs/wgr_defconfig
make ARCH=arm64 O="${OUT_DIR}" wgr_defconfig
make ARCH=arm64 O="${OUT_DIR}" olddefconfig

echo "== building"
make -j"${JOBS}" ARCH=arm64 O="${OUT_DIR}" \
    CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy \
    OBJDUMP=llvm-objdump STRIP=llvm-strip READELF=llvm-readelf \
    HOSTCC=clang HOSTCXX=clang++ \
    Image Image.lz4

echo "== artifacts"
ls -l "${OUT_DIR}/arch/arm64/boot/Image" "${OUT_DIR}/arch/arm64/boot/Image.lz4" || true
