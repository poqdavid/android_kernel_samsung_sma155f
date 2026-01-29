#!/usr/bin/env bash
set -euo pipefail

# -------- Configuration / defaults --------
SCRIPT_NAME="$(basename "$0")"
DEFAULT_KERNEL_DIR="kernel-5.10"
DEFAULT_DEFCONFIG="arch/arm64/configs/a15_00_defconfig"
DEFAULT_OUT="../out/target/product/a15/obj/KERNEL_OBJ"
KERNELSU_SETUP_URL="https://raw.githubusercontent.com/poqdavid/KernelSU-Next/dev/kernel/setup.sh"
BASE_KSU_VERSION=10200

# -------- Colors & logging --------
RED="\e[1;31m"
GREEN="\e[1;32m"
YELLOW="\e[1;33m"
RESET="\e[0m"

print_msg() {
    local color="$1"; shift
    printf "%b%s%b\n" "${color}" "$*" "${RESET}"
}

_log_handler() {
  local color="$1"
  local level="$2"
  shift 2
  
  local nl=""
  [[ "$1" == "-n" ]] && { nl="\n"; shift; }

  printf "${nl}%b[%s] %s%b\n" "${color}" "${level}" "$*" "${RESET}"
}

info() { _log_handler "${GREEN}"  "INFO"  "$@"; }
warn() { _log_handler "${YELLOW}" "WARN"  "$@"; }
err()  { _log_handler "${RED}"    "ERROR" "$@"; }

# -------- Script header --------
print_msg "$GREEN" " - Build script for Samsung kernel image - "
print_msg "$RED" "       by poqdavid "

# -------- Timing support --------
_ts() { date +%s; }
_print_runtime() {
    local label=$1 start=$2 end=$3
    if [[ -z "$start" || -z "$end" || "$end" -lt "$start" ]]; then
        printf "%b%s: skipped%b\n" "${YELLOW}" "$label" "${RESET}"
        return
    fi
    local runtime=$((end - start))
    printf "%b%s: %02d:%02d:%02d%b\n" "${GREEN}" "$label" \
    $((runtime / 3600)) \
    $(((runtime % 3600) / 60)) \
    $((runtime % 60)) \
    "${RESET}"
}

# -------- CLI parsing --------
KERNEL_DIR="$DEFAULT_KERNEL_DIR"
OUT_DIR="$DEFAULT_OUT"
NO_CLEAN=0
NO_PATCH=0
BUILD_ONLY=0
CLEAN_ONLY=0
JOBS=""
DRY_RUN=0
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --kernel-dir) KERNEL_DIR="$2"; shift 2;;
        --out-dir) OUT_DIR="$2"; shift 2;;
        --no-clean) NO_CLEAN=1; shift;;
        --no-patch) NO_PATCH=1; shift;;
        --build-only) BUILD_ONLY=1; shift;;
        --clean) CLEAN_ONLY=1; shift;;
        -j*) JOBS="${1#-j}"; [[ -z "$JOBS" ]] && { JOBS="$2"; shift; }; shift;;
        --dry-run) DRY_RUN=1; shift;;
        --verbose) VERBOSE=1; shift;;
        --help|-h)
      cat <<EOF
$SCRIPT_NAME - improved kernel build helper

Options:
  --kernel-dir DIR     Path to kernel source (default: ${DEFAULT_KERNEL_DIR})
  --out-dir DIR        Output directory for build artifacts (default: ${DEFAULT_OUT})
  --no-clean           Skip running clean_build.sh
  --no-patch           Skip patching steps / KernelSU setup
  --build-only         Skip config & patch steps; just run the build
  --clean              Only run the clean step
  --jobs N, -j N       Pass N to the build (if supported)
  --dry-run            Print actions but do not execute destructive commands
  --verbose            Print extra debug info
  --help, -h           Show this help
EOF
            exit 0
        ;;
        *)
            err "Unknown arg: $1"
            exit 2
        ;;
    esac
done

if [[ $VERBOSE -eq 1 ]]; then set -x; fi

# -------- Preconditions: required commands --------
CMDMISSING=0
require_cmds=(bash sed awk find git patch curl printf)

PYTHON_BIN=""
if command -v python2 >/dev/null 2>&1; then
    PYTHON_BIN=python2
else
    require_cmds+=(python2) # force readable error later
fi

for c in "${require_cmds[@]}"; do
    if ! command -v "$c" >/dev/null 2>&1; then
        err "Required command '$c' not found."
        CMDMISSING=1
    fi
done

# 2. Check if any commands were missing
if [ $CMDMISSING -eq 1 ]; then
    echo "--------------------------------------------------"
    echo "Please install the missing packages and try again."
    exit 2
fi

# -------- Helper Functions --------
run_cmd() {
    if [[ $DRY_RUN -eq 1 ]]; then
        info -n "DRY RUN: $*"
    else
        eval "$@"
    fi
}

# Timestamps
CONFIG_START=0; CONFIG_END=0
PATCH_START=0; PATCH_END=0
BUILD_START=0; BUILD_END=0

cleanup() {
    echo " "
    _print_runtime "Config runtime" "$CONFIG_START" "$CONFIG_END"
    _print_runtime "Patch runtime" "$PATCH_START" "$PATCH_END"
    _print_runtime "Build runtime" "$BUILD_START" "$BUILD_END"
}
trap cleanup EXIT

# 1. Clean Step
if [[ $NO_CLEAN -eq 0 ]]; then
    
    info -n "Started cleaning up..."
    
    git restore kernel-5.10/
    git clean -fd kernel-5.10/
    rm -rf kernel-5.10/KernelSU
    rm -rf kernel-5.10/KernelSU-Next
    rm -rf out
    
    info "Finsished cleaning up..."
    
    if [[ $CLEAN_ONLY -eq 1 ]]; then
        exit 0
    fi
fi

# 2. Config Modifications (Samsung & Optimizations)
if [[ $BUILD_ONLY -eq 0 ]]; then
    CONFIG_START=$(_ts)
    info -n "Modifying configs..."
    CONFIG_TOOL="./${KERNEL_DIR}/scripts/config"
    DEFCONFIG="./${KERNEL_DIR}/${DEFAULT_DEFCONFIG}"
    
    # Samsung & Security
    run_cmd "$CONFIG_TOOL --file $DEFCONFIG \
        --set-val UH n \
        --set-val RKP n \
        --set-val KDP n \
        --set-val SECURITY_DEFEX n \
        --set-val INTEGRITY n \
        --set-val FIVE n \
        --set-val TRIM_UNUSED_KSYMS n \
        --set-val PROCA n \
        --set-val PROCA_GKI_10 n \
        --set-val PROCA_S_OS n \
        --set-val PROCA_CERTIFICATES_XATTR n \
        --set-val PROCA_CERT_ENG n \
        --set-val PROCA_CERT_USER n \
        --set-val GAF_V6 n --set-val FIVE n \
        --set-val FIVE_CERT_USER n \
        --set-val FIVE_DEFAULT_HASH n \
        --set-val UH_RKP n \
        --set-val UH_LKMAUTH n \
        --set-val UH_LKM_BLOCK n \
        --set-val RKP_CFP_JOPP n \
        --set-val RKP_CFP n \
        --set-val KDP_CRED n \
        --set-val KDP_NS n \
        --set-val KDP_TEST n \
        --set-val RKP_CRED n"
    
    # Optimizations (BBR, etc)
    run_cmd "$CONFIG_TOOL --file $DEFCONFIG \
        --set-val IP_NF_TARGET_TTL y \
        --set-val IP6_NF_TARGET_HL y \
        --set-val IP6_NF_MATCH_HL y \
        --set-val TCP_CONG_ADVANCED y \
        --set-val TCP_CONG_BBR y \
        --set-val NET_SCH_FQ y \
        --set-val TCP_CONG_BIC n \
        --set-val TCP_CONG_WESTWOOD n \
        --set-val TCP_CONG_HTCP n \
        --set-val DEFAULT_BBR y \
        --set-val DEFAULT_BIC n \
        --set-str DEFAULT_TCP_CONG \"bbr\" \
        --set-val DEFAULT_RENO n \
        --set-val DEFAULT_CUBIC n \
        --set-val IP6_NF_NAT y \
        --set-val IP6_NF_TARGET_MASQUERADE y \
        --set-val NF_NAT_IPV6 y"
    
    # KernelSU Next & SUSFS
    run_cmd "$CONFIG_TOOL --file $DEFCONFIG \
        --set-val KSU y \
        --set-val KSU_KPROBES_HOOK n \
        --set-val KSU_SUSFS y \
        --set-val KSU_SUSFS_HAS_MAGIC_MOUNT y \
        --set-val KSU_SUSFS_SUS_PATH y \
        --set-val KSU_SUSFS_SUS_MOUNT y \
        --set-val KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT y \
        --set-val KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT y \
        --set-val KSU_SUSFS_SUS_KSTAT y \
        --set-val KSU_SUSFS_SUS_OVERLAYFS n \
        --set-val KSU_SUSFS_TRY_UMOUNT y \
        --set-val KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT y \
        --set-val KSU_SUSFS_SPOOF_UNAME y \
        --set-val KSU_SUSFS_ENABLE_LOG y \
        --set-val KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS y \
        --set-val KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG y \
        --set-val KSU_SUSFS_OPEN_REDIRECT y \
        --set-val KSU_SUSFS_SUS_MAP y \
        --set-val KSU_SUSFS_SUS_SU n \
        --set-val TMPFS_XATTR y --set-val TMPFS_POSIX_ACL y"
    
    # 3. Metadata Configuration
    info -n "Configuring Kernel metadata..."
    pushd "$KERNEL_DIR" > /dev/null
    run_cmd "sed -i '\$s|echo \"\\\$res\"|echo \"-android12-9-31117096\"|' ./scripts/setlocalversion"
    run_cmd "perl -pi -e 's{UTS_VERSION=\"\\\$\\(echo \\\$UTS_VERSION \\\$CONFIG_FLAGS \\\$TIMESTAMP \\| cut -b -\\\$UTS_LEN\\)\"}{UTS_VERSION=\"#1 SMP PREEMPT Thu Jul 31 08:40:06 UTC 2025\"}' ./scripts/mkcompile_h"
    run_cmd "sed -i 's/-dirty//' ./scripts/setlocalversion"
    
    # 4. Generate build.config
    info -n "Generating build configs..."
    run_cmd "python2 scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig --kernel-defconfig-overlays entry_level.config -m user -o $OUT_DIR/build.config"
    popd > /dev/null
    CONFIG_END=$(_ts)
fi

# 5. Patching / KernelSU Setup
if [[ $NO_PATCH -eq 0 && $BUILD_ONLY -eq 0 ]]; then
    PATCH_START=$(_ts)
    
    pushd "$KERNEL_DIR" > /dev/null
    
    info -n "Setting up KernelSU Next SUSFS..."
    run_cmd "curl -LSs $KERNELSU_SETUP_URL | bash -s dev"
    
    info -n "Copying SUSFS patches to kernel source..."
    run_cmd "cp ../patches/susfs4ksu/kernel_patches/fs/* ./fs/"
    run_cmd "cp ../patches/susfs4ksu/kernel_patches/include/linux/* ./include/linux/"
    
    info -n "Applying fake_config.patch..."
    run_cmd "patch -p1 --forward < ../patches/fake_config.patch || true"
    
    # Version Detection
    if [[ -d "./KernelSU-Next" ]]; then
        pushd "./KernelSU-Next" > /dev/null
        KSU_COUNT=$(git rev-list --count HEAD || echo 0)
        KSU_VERSION=$((KSU_COUNT + BASE_KSU_VERSION))
        info -n "Detected KernelSU Next Version: $KSU_VERSION"
        popd > /dev/null
    fi
    
    # Get SUSFS version from header we just copied
    SUSFS_VER=$(grep '#define SUSFS_VERSION' ./include/linux/susfs.h | awk -F'"' '{print $2}' || echo "unknown")
    info -n "Detected SUSFS Version: $SUSFS_VER"
    
    # Patch KernelSU-Next internal
    if [[ -d "./KernelSU-Next" ]]; then
        pushd "./KernelSU-Next" > /dev/null
        info -n "Patching SUSFS into KernelSU-Next..."
        run_cmd "patch -p1 --forward < ../../patches/susfs4ksu/kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch || true"
        popd > /dev/null
    fi
    
    # Patch Main Kernel
    info -n "Patching SUSFS into Kernel..."
    run_cmd "patch -p1 < ../patches/susfs4ksu/kernel_patches/50_add_susfs_in_gki-android12-5.10.patch || true"
    
    # Samsung Specific Patches
    info -n "Applying Samsung device patches..."
    for file in $(find ../patches/kernel_patches/samsung/SM-A155F -maxdepth 2 -name "*.patch"); do
        info "Patching $file"
        run_cmd "patch -p1 --forward < \"$file\" || true"
    done
    
    # KernelSU Next .rej Fixes
    if [[ -d "./KernelSU-Next" ]]; then
        pushd "./KernelSU-Next" > /dev/null
        info -n "Patching .rej fixes in KernelSU Next..."
        for rej in $(find ./kernel -maxdepth 2 -name "*.rej" -exec basename {} .rej \;); do
            FIX_PATCH="../../patches/kernel_patches/next/susfs_fix_patches/$SUSFS_VER/fix_$rej.patch"
            if [[ -f "$FIX_PATCH" ]]; then
			    info "Patching $rej"
                run_cmd "patch -p1 --forward < \"$FIX_PATCH\" || true"
            fi
        done
        
        # Final KSU patches
        info -n "Patching Hook Mode!"
        run_cmd "patch -p1 --forward < \"../../patches/kernel_patches/next/susfs_fix_patches/$SUSFS_VER/overwrite_hook_mode.patch\" || true"

        info -n "Patching KSU_TOOLKIT Support for SusFS kernel!"
        run_cmd "patch -p1 --forward < \"../../patches/kernel_patches/next/susfs_fix_patches/$SUSFS_VER/ksu_toolkit.patch\" || true"

        info -n "Patching Multi-manager Support for SusFS kernel!"
        run_cmd "patch -p1 --forward < \"../../patches/kernel_patches/next/susfs_fix_patches/$SUSFS_VER/multi_manager.patch\" ||true"
        popd > /dev/null
    fi
    
    popd > /dev/null
    PATCH_END=$(_ts)
fi

# 6. Build
BUILD_START=$(_ts)
info -n "Starting Kernel Build..."

export LTO=thin
export ARCH=arm64
export PLATFORM_VERSION=12
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"
export OUT_DIR="$OUT_DIR"
export DIST_DIR="$OUT_DIR"
export BUILD_CONFIG="$OUT_DIR/build.config"

if [[ -n "$JOBS" ]]; then
    export MAKEFLAGS="-j$JOBS"
fi

pushd "$KERNEL_DIR" > /dev/null
cd ../kernel
run_cmd "./build/build.sh"
popd > /dev/null

BUILD_END=$(_ts)
info -n "Build completed successfully."