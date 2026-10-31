#!/usr/bin/env bash
# Build the HUAWEI_Kirin9000_Kernel (5.10.43) with the two port patches.
# Vendor flow: make ARCH=arm64 O=out merge_baltimore_defconfig && make -j
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${REPO_ROOT}"

OUT_DIR="${OUT_DIR:-out}"
JOBS="${JOBS:-$(nproc)}"
PATCHES="patches"

echo "== repo: ${REPO_ROOT}"
echo "== applying port patches"
for p in "${PATCHES}/0001-binder-async-sender-pid.patch" \
         "${PATCHES}/0002-rdr-no-ap-reset.patch"; do
    if git apply --check "$p" 2>/dev/null; then
        git apply "$p"; echo "   applied $(basename "$p")"
    elif patch -p1 --forward --dry-run < "$p" >/dev/null 2>&1; then
        patch -p1 --forward < "$p"; echo "   applied $(basename "$p") (patch)"
    else
        echo "   !! $(basename "$p") not applied (already?)"
    fi
done

echo "== toolchain"
clang --version | head -1
export PATH="${PATH}:/usr/bin"
export CROSS_COMPILE=aarch64-linux-gnu-
export CC=clang
export LD=ld.lld
export AR=llvm-ar
export NM=llvm-nm
export OBJCOPY=llvm-objcopy
export OBJDUMP=llvm-objdump
export STRIP=llvm-strip
export READELF=llvm-readelf

echo "== configure (vendor defconfig)"
mkdir -p "${OUT_DIR}"
make ARCH=arm64 O="${OUT_DIR}" merge_baltimore_defconfig

echo "== build"
make -j"${JOBS}" ARCH=arm64 O="${OUT_DIR}" Image.gz Image

echo "== artifacts"
ls -l "${OUT_DIR}/arch/arm64/boot/Image.gz" "${OUT_DIR}/arch/arm64/boot/Image" 2>/dev/null || true
grep -E "CONFIG_BINDER_TRANSACTION_PROC_BRIEF|CONFIG_ANDROID_BINDER_IPC" "${OUT_DIR}/.config" || true
