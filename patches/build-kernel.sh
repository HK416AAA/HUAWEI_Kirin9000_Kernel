#!/usr/bin/env bash
# Build HUAWEI_Kirin9000_Kernel (5.10.43) with the HarmonyOS port patches.
#
# Notes:
#  * the vendor makefiles branch on make variables named CONFIG_* -> export them
#  * the modem sources are entered through drivers/platform_drivers/, which does
#    not carry the per-driver -I flags of makefile_soc_modem.mk, so those include
#    directories are passed through KCFLAGS
#  * espe/espe_entry_hook.c is not part of the published tree -> disable
#    CONFIG_BALONG_ESPE
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
if [ ! -e /usr/bin/clang++ ]; then sudo ln -sf /usr/bin/clang++-12 /usr/bin/clang++; fi
for t in llvm-ar llvm-nm llvm-objcopy llvm-objdump llvm-readelf llvm-strip; do
    if [ ! -e "/usr/bin/$t" ]; then sudo ln -sf "/usr/bin/$t-12" "/usr/bin/$t"; fi
done
export CROSS_COMPILE=aarch64-linux-gnu-
CLANG_PREBUILTS_PATH=/usr/

echo "== configure (vendor defconfig)"
mkdir -p "${OUT_DIR}"
make ARCH=arm64 O="${OUT_DIR}" CLANG_PREBUILTS_PATH="${CLANG_PREBUILTS_PATH}" merge_kirin9000_defconfig

echo "== adjust config"
scripts/config --file "${OUT_DIR}/.config" -d BALONG_ESPE || true
make ARCH=arm64 O="${OUT_DIR}" CLANG_PREBUILTS_PATH="${CLANG_PREBUILTS_PATH}" olddefconfig

echo "== export CONFIG_* as make variables"
# shellcheck disable=SC2046
export $(grep -E '^CONFIG_[A-Za-z0-9_]+=y$' "${OUT_DIR}/.config" | tr -d '\r')

MODEM_INC="-Idrivers/hisi/modem/drv/icc \
 -Idrivers/hisi/modem/drv/balong_timer \
 -Idrivers/hisi/modem/drv/hds \
 -Idrivers/hisi/modem/drv/diag/cpm \
 -Idrivers/hisi/tzdriver \
 -Idrivers/hisi/modem/drv/include"

echo "== build"
make -j"${JOBS}" ARCH=arm64 O="${OUT_DIR}" \
     CLANG_PREBUILTS_PATH="${CLANG_PREBUILTS_PATH}" \
     KCFLAGS="${MODEM_INC}" \
     Image.gz Image

echo "== artifacts"
ls -l "${OUT_DIR}/arch/arm64/boot/Image.gz" "${OUT_DIR}/arch/arm64/boot/Image" 2>/dev/null || true
grep -E "CONFIG_BINDER_TRANSACTION_PROC_BRIEF|CONFIG_ANDROID_BINDER_IPC" "${OUT_DIR}/.config" || true
