#!/usr/bin/env bash
# Build HUAWEI_Kirin9000_Kernel (5.10.43) with the two port patches.
# The vendor Makefile hardcodes a prebuilt clang path; it is overridable via
# CLANG_PREBUILTS_PATH, so we point it at the CI toolchain.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${REPO_ROOT}"

OUT_DIR="${OUT_DIR:-out}"
JOBS="${JOBS:-$(nproc)}"

echo "== applying port patches"
for p in patches/0001-binder-async-sender-pid.patch patches/0002-rdr-no-ap-reset.patch; do
    if git apply --check "$p" 2>/dev/null; then
        git apply "$p"; echo "   applied $(basename "$p")"
    elif patch -p1 --forward --dry-run < "$p" >/dev/null 2>&1; then
        patch -p1 --forward < "$p"; echo "   applied $(basename "$p") (patch)"
    else
        echo "   !! $(basename "$p") not applied"
    fi
done

echo "== toolchain wiring"
if [ ! -e /usr/bin/llvm-ar ]; then sudo ln -sf /usr/bin/llvm-ar-12 /usr/bin/llvm-ar; fi
if [ ! -e /usr/bin/llvm-nm ]; then sudo ln -sf /usr/bin/llvm-nm-12 /usr/bin/llvm-nm; fi
if [ ! -e /usr/bin/llvm-objcopy ]; then sudo ln -sf /usr/bin/llvm-objcopy-12 /usr/bin/llvm-objcopy; fi
if [ ! -e /usr/bin/llvm-objdump ]; then sudo ln -sf /usr/bin/llvm-objdump-12 /usr/bin/llvm-objdump; fi
if [ ! -e /usr/bin/llvm-readelf ]; then sudo ln -sf /usr/bin/llvm-readelf-12 /usr/bin/llvm-readelf; fi
if [ ! -e /usr/bin/llvm-strip ]; then sudo ln -sf /usr/bin/llvm-strip-12 /usr/bin/llvm-strip; fi
if [ ! -e /usr/bin/clang++ ]; then sudo ln -sf /usr/bin/clang++-12 /usr/bin/clang++; fi

export CROSS_COMPILE=aarch64-linux-gnu-
CLANG_PREBUILTS_PATH=/usr/
echo "   CLANG_PREBUILTS_PATH=${CLANG_PREBUILTS_PATH}"

echo "== configure (vendor defconfig)"
mkdir -p "${OUT_DIR}"
make ARCH=arm64 O="${OUT_DIR}" CLANG_PREBUILTS_PATH="${CLANG_PREBUILTS_PATH}" merge_baltimore_defconfig

echo "== build"
make -j"${JOBS}" ARCH=arm64 O="${OUT_DIR}" CLANG_PREBUILTS_PATH="${CLANG_PREBUILTS_PATH}" Image.gz Image

echo "== artifacts"
ls -l "${OUT_DIR}/arch/arm64/boot/Image.gz" "${OUT_DIR}/arch/arm64/boot/Image" 2>/dev/null || true
grep -E "CONFIG_BINDER_TRANSACTION_PROC_BRIEF|CONFIG_ANDROID_BINDER_IPC" "${OUT_DIR}/.config" || true
