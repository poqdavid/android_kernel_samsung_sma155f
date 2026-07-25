#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0
set -euo pipefail

# -------- KernelSU variant selection (must run before logging/config setup) --------
# Default to no KernelSU variant if neither --ksu nor --ksun is passed.
KSU_VARIANT="none"
KSU_FLAG_COUNT=0
for _arg in "$@"; do
    case "$_arg" in
        --ksu)  KSU_VARIANT="ksu";  KSU_FLAG_COUNT=$((KSU_FLAG_COUNT + 1));;
        --ksun) KSU_VARIANT="ksun"; KSU_FLAG_COUNT=$((KSU_FLAG_COUNT + 1));;
    esac
done
if [[ $KSU_FLAG_COUNT -gt 1 ]]; then
    echo "Error: --ksu and --ksun are mutually exclusive; pass only one." >&2
    exit 2
fi

if [[ "$KSU_VARIANT" == "ksun" ]]; then
    KERNELSU_SETUP_URL="https://raw.githubusercontent.com/poqdavid/KernelSU-Next/dev/kernel/setup.sh"
    KERNELSU_SETUP_BRANCH="dev"
    BASE_KSU_VERSION=30000
    KSU_DIR="KernelSU-Next"
    KSU_LABEL="KernelSU Next"
    KSU_DISCORD_LABEL="KernelSUNext"
    SUSFS_KSU_INTERNAL_PATCH_DESC="pershoot-fork SUSFS patch (10_pershoot_enable_susfs_for_ksun.patch)"
    elif [[ "$KSU_VARIANT" == "ksu" ]]; then
    KERNELSU_SETUP_URL="https://raw.githubusercontent.com/poqdavid/KernelSU/main/kernel/setup.sh"
    KERNELSU_SETUP_BRANCH="main"
    BASE_KSU_VERSION=20000
    KSU_DIR="KernelSU"
    KSU_LABEL="KernelSU"
    KSU_DISCORD_LABEL="KernelSU"
    SUSFS_KSU_INTERNAL_PATCH_DESC="upstream SUSFS patch (10_enable_susfs_for_ksu.patch)"
else
    KSU_DIR=""
    KSU_LABEL="None"
    KSU_DISCORD_LABEL="Variant"
    NO_SUSFS=1
fi

# -------- Discord Webhook Configuration --------
WEBHOOK_FILE="$(pwd)/.discord_webhook"
if [[ -f "$WEBHOOK_FILE" ]]; then
    DISCORD_WEBHOOK_URL=$(cat "$WEBHOOK_FILE" | tr -d '\n' | tr -d '\r')
else
    DISCORD_WEBHOOK_URL=""
fi

# -------- Discord USER ID Configuration --------
USERID_FILE="$(pwd)/.discord_userid"
if [[ -f "$USERID_FILE" ]]; then
    DISCORD_USER_ID=$(cat "$USERID_FILE" | tr -d '\n' | tr -d '\r')
else
    DISCORD_USER_ID=""
fi

LOGFILE="$(pwd)/logs/build_${KSU_VARIANT}_$(date +%Y%m%d_%H%M%S).log"

if [[ ! -d "$(pwd)/logs" ]]; then
    mkdir -p "$(pwd)/logs"
fi

# Strip ANSI color codes from the log file output, but keep them in the terminal
exec > >(tee >(sed "s/$(printf '\033')\\[[0-9;]*m//g" >> "$LOGFILE")) 2>&1

_calc_runtime() {
    local start=${1:-0} end=${2:-0}
    if [[ -z "$start" || "$start" -eq 0 ]]; then
        echo "N/A"
    else
        if [[ -z "$end" || "$end" -eq 0 ]]; then
            end=$(date +%s) # Calculate elapsed time if process didn't finish
        fi
        if [[ "$end" -lt "$start" ]]; then
            echo "N/A"
        else
            local runtime=$((end - start))
            printf "%02d:%02d:%02d" $((runtime / 3600)) $(((runtime % 3600) / 60)) $((runtime % 60))
        fi
    fi
}

send_discord_file() {
    local status="$1"
    local message="$2"
    local color="$3"
    
    if [[ -z "$DISCORD_WEBHOOK_URL" ]]; then
        return 0
    fi
    
    # Safely handle potentially unset timestamps
    local config_time=$(_calc_runtime "${CONFIG_START:-0}" "${CONFIG_END:-0}")
    local patch_time=$(_calc_runtime "${PATCH_START:-0}" "${PATCH_END:-0}")
    local build_time=$(_calc_runtime "${BUILD_START:-0}" "${BUILD_END:-0}")
    
    local status_emoji="✅"
    if [[ "$status" == *"FAILED"* ]]; then
        status_emoji="❌"
    fi
    
    # Format the content string to ping the user if the ID is provided
    local content_str=""
    if [[ -n "$DISCORD_USER_ID" ]]; then
        content_str="\"content\":\"<@$DISCORD_USER_ID>\", "
    fi
    
    # Use bash parameter expansion ${VAR:-N/A} to print N/A if the variable is unbound or empty
    curl -s \
    -F "payload_json={
        $content_str
        \"embeds\":[{
          \"title\":\"${status_emoji} Kernel Build $status\",
          \"description\":\"$message\",
          \"color\":$color,
          \"fields\": [
            { \"name\": \"🐧 Kernel Version\", \"value\": \"${kernel_version:-N/A}\", \"inline\": true },
            { \"name\": \"📱 Android Version\", \"value\": \"${android_version:-N/A}\", \"inline\": true },
            { \"name\": \"🐧 ${KSU_DISCORD_LABEL} Version\", \"value\": \"${KSU_VERSION:-Vanilla}\", \"inline\": true },
            { \"name\": \"🛡️ SUSFS Version\", \"value\": \"${SUSFS_VER:-N/A}\", \"inline\": false },
            { \"name\": \"⏱️ Config Time\", \"value\": \"${config_time}\", \"inline\": true },
            { \"name\": \"⏱️ Patch Time\", \"value\": \"${patch_time}\", \"inline\": true },
            { \"name\": \"⏱️ Build Time\", \"value\": \"${build_time}\", \"inline\": true }
          ]
    }]}" \
    -F "file1=@$LOGFILE" \
    "$DISCORD_WEBHOOK_URL" >/dev/null
}

on_error() {
    send_discord_file "FAILED" "Kernel build failed at line $1 ⚠️" 16711680
}

trap 'on_error $LINENO' ERR


# -------- Configuration / defaults --------
SCRIPT_NAME="$(basename "$0")"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PATCHES="$(realpath "$SCRIPT_DIR/patches")"
KERNEL_PATCHES="$(realpath "$PATCHES/kernel_patches")"
SUSFS_PATCHES="$(realpath "$PATCHES/susfs4ksu")"
ZEROMOUNT_PATCHES="$(realpath "$PATCHES/zeromount")"
DEFAULT_KERNEL_DIR="$(find . -maxdepth 1 -type d -name "kernel-*" | head -n1)"
DEFAULT_DEFCONFIG="arch/arm64/configs/a15_00_defconfig"
OTHER_DEFCONFIG="arch/arm64/configs/a15_defconfig"
DEFAULT_OUT="../out/target/product/a15/obj/KERNEL_OBJ"
# KERNELSU_SETUP_URL / BASE_KSU_VERSION / KSU_DIR / KSU_LABEL are set by the
# --ksu / --ksun pre-scan near the top of this script.
KSU_VERSION="N/A"
MIN_VERSION="5.16"

pushd "$DEFAULT_KERNEL_DIR" > /dev/null
KERNELVERSION="$(make -s kernelversion)"
android_version=$(grep -m1 '^BRANCH=' ./build.config.common | awk -F= '{print $2}' | awk -F- '{print $1}')
popd > /dev/null

kernel_version=$(echo "$KERNELVERSION" | cut -d. -f1,2)

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
    local runtime_str=$(_calc_runtime "$start" "$end")
    if [[ "$runtime_str" == "N/A" ]]; then
        printf "%b%s: skipped%b\n" "${YELLOW}" "$label" "${RESET}"
    else
        printf "%b%s: %s%b\n" "${GREEN}" "$label" "$runtime_str" "${RESET}"
    fi
}

# -------- CLI parsing --------
KERNEL_DIR="$DEFAULT_KERNEL_DIR"
OUT_DIR="$DEFAULT_OUT"
NO_CLEAN=0
NO_PATCH=0
NO_SUSFS=0
BUILD_ONLY=0
CLEAN_ONLY=0
JOBS=""
VERBOSE=0

info -n "Using kernel source: $KERNEL_DIR"
info -n "Using output directory: $OUT_DIR"

if [[ "$KSU_VARIANT" != "none" ]]; then
    info -n "Using KernelSU variant: $KSU_LABEL (./$KSU_DIR)"
else
    info -n "KernelSU variant: None (Vanilla kernel with optimizations)"
fi

info -n "Kernel version: $kernel_version"
info -n "Android version: $android_version"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --kernel-dir) KERNEL_DIR="$2"; shift 2;;
        --out-dir) OUT_DIR="$2"; shift 2;;
        --ksu) shift;;   # already handled by the pre-scan; consume it here so parsing doesn't error
        --ksun) shift;;  # already handled by the pre-scan; consume it here so parsing doesn't error
        --no-clean) NO_CLEAN=1; shift;;
        --no-patch) NO_PATCH=1; shift;;
        --no-susfs) NO_SUSFS=1; shift;;
        --build-only) BUILD_ONLY=1; shift;;
        --clean) CLEAN_ONLY=1; shift;;
        -j*) JOBS="${1#-j}"; [[ -z "$JOBS" ]] && { JOBS="$2"; shift; }; shift;;
        --verbose) VERBOSE=1; shift;;
        --help|-h)
      cat <<EOF
$SCRIPT_NAME - improved kernel build helper

Options:
  --kernel-dir DIR     Path to kernel source (default: ${DEFAULT_KERNEL_DIR})
  --out-dir DIR        Output directory for build artifacts (default: ${DEFAULT_OUT})
  --ksu                Build against upstream KernelSU (main branch)
  --ksun               Build against KernelSU-Next (dev branch)
  --no-clean           Skip running clean_build.sh
  --no-patch           Skip patching steps / KernelSU setup
  --no-susfs           Skip SUSFS related config & patches
  --build-only         Skip config & patch steps; just run the build
  --clean              Only run the clean step
  --jobs N, -j N       Pass N to the build (if supported)
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

export LTO=thin
export ARCH=arm64
export PLATFORM_VERSION=12
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"
export OUT_DIR="$OUT_DIR"
export DIST_DIR="$OUT_DIR"
export BUILD_CONFIG="$OUT_DIR/build.config"
export LD=ld.lld
export HOSTLD=ld.lld
export AR=llvm-ar
export NM=llvm-nm

if [[ -n "$JOBS" ]]; then
    export MAKEFLAGS="-j$JOBS"
fi

# -------- Preconditions: required commands --------
CMDMISSING=0
require_cmds=(bash sed awk find git patch curl printf)

PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN=python3
else
    require_cmds+=(python3) # force readable error later
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
    rm -rf kernel-5.10/Baseband-guard
    rm -rf out
    
    info "Finsished cleaning up..."
    
    if [[ $CLEAN_ONLY -eq 1 ]]; then
        exit 0
    fi
fi

info -n "Applying Python3 support patch..."
patch -p1 --forward < ./patches/enable-python3-support.patch || true

gen_metadata(){
    
    # 3. Metadata Configuration
    info -n "Configuring Kernel metadata..."
    pushd "$KERNEL_DIR" > /dev/null
    sed -i '$s|echo "\$res"|echo "-android12-9-31117096"|' ./scripts/setlocalversion
    perl -pi -e 's{UTS_VERSION="\$\(echo \$UTS_VERSION \$CONFIG_FLAGS \$TIMESTAMP \| cut -b -\$UTS_LEN\)"}{UTS_VERSION="#1 SMP PREEMPT Thu Jul 31 08:40:06 UTC 2025"}' ./scripts/mkcompile_h
    sed -i 's/-dirty//' ./scripts/setlocalversion
    
    # 4. Generate build.config
    info -n "Generating build configs..."
    python3 scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig --kernel-defconfig-overlays entry_level.config -m user -o $OUT_DIR/build.config
    if [[ $BUILD_ONLY -eq 0 ]]; then
        info -n "Applying fake_config.patch..."
        patch -p1 --forward < $PATCHES/fake_config.patch || true
    fi
    popd > /dev/null
    
}

# 2. Config Modifications (Samsung & Optimizations)
if [[ $BUILD_ONLY -eq 0 ]]; then
    CONFIG_START=$(_ts)
    info -n "Modifying configs..."
    CONFIG_TOOL="./${KERNEL_DIR}/scripts/config"
    DEFAULTDEFCONFIG="./${KERNEL_DIR}/${DEFAULT_DEFCONFIG}"
    OTHERDEFCONFIG="./${KERNEL_DIR}/${OTHER_DEFCONFIG}"
    
    for DEFCONFIG in "$DEFAULTDEFCONFIG" "$OTHERDEFCONFIG"; do
        info -n "$DEFCONFIG"
        
        info -n "Settings Samsung & Security configs..."
        # Samsung & Security
        $CONFIG_TOOL --file $DEFCONFIG \
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
        --set-val GAF_V6 n \
        --set-val FIVE n \
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
        --set-val RKP_CRED n \
        --set-val MODULES y \
        --set-val MODULE_FORCE_LOAD y \
        --set-val MODULE_UNLOAD y \
        --set-val MODULE_FORCE_UNLOAD y \
        --set-val MODVERSIONS y \
        --set-val MODULE_SRCVERSION_ALL n \
        --set-val MODULE_SIG n \
        --set-val MODULE_COMPRESS n
        
        info -n "Setting optimization configs..."
        
        info "Adding BBG support..."
        # BBG support
        $CONFIG_TOOL --file $DEFCONFIG \
        --set-val BBG y
        
        info "Adding Droidspaces support..."
        # Droidspaces support
        $CONFIG_TOOL --file $DEFCONFIG \
        --set-val SYSVIPC y \
        --set-val DEVTMPFS y \
        --set-val IPC_NS y \
        --set-val PID_NS y \
        --set-val POSIX_MQUEUE y \
        --set-val NETFILTER_XT_TARGET_REJECT y \
        --set-val NETFILTER_XT_TARGET_LOG y \
        --set-val NETFILTER_XT_MATCH_RECENT y \
        --set-val CONFIG_USER_NS y \
        --set-val NTSYNC y
        
        info "Adding BBR3 Support Support..."
        # BBR Support
        $CONFIG_TOOL --file $DEFCONFIG \
        --set-val TCP_CONG_ADVANCED y \
        --set-val TCP_CONG_BBR y \
        --set-val NET_SCH_FQ y \
        --set-val NET_SCH_FQ_CODEL y \
        --set-val TCP_CONG_CUBIC y \
        --set-val TCP_CONG_BIC n \
        --set-val TCP_CONG_WESTWOOD n \
        --set-val TCP_CONG_HTCP n \
        --set-val NET_SCH_CAKE y \
        --set-val NET_SCH_PIE y \
        --set-val NET_SCH_FQ_PIE y \
        --set-val TCP_CONG_BBR3 y \
        --set-val DEFAULT_BBR3 y \
        --set-val DEFAULT_BIC n \
        --set-str DEFAULT_TCP_CONG "bbr" \
        --set-val DEFAULT_RENO n \
        --set-val DEFAULT_CUBIC n \
        
        info "Adding IP SET & IPv6_NAT Support..."
        #IP SET & IPv6_NAT Support
        $CONFIG_TOOL --file $DEFCONFIG \
        --set-val IP_SET y \
        --set-val IP_SET_MAX 65534 \
        --set-val IP_SET_BITMAP_IP y \
        --set-val IP_SET_BITMAP_IPMAC y \
        --set-val IP_SET_BITMAP_PORT y \
        --set-val IP_SET_HASH_IP y \
        --set-val IP_SET_HASH_IPMARK y \
        --set-val IP_SET_HASH_IPPORT y \
        --set-val IP_SET_HASH_IPPORTIP y \
        --set-val IP_SET_HASH_IPPORTNET y \
        --set-val IP_SET_HASH_IPMAC y \
        --set-val IP_SET_HASH_MAC y \
        --set-val IP_SET_HASH_NETPORTNET y \
        --set-val IP_SET_HASH_NET y \
        --set-val IP_SET_HASH_NETNET y \
        --set-val IP_SET_HASH_NETPORT y \
        --set-val IP_SET_HASH_NETIFACE y \
        --set-val IP_SET_LIST_SET y \
        --set-val NETFILTER_XT_MATCH_ADDRTYPE y \
        --set-val NETFILTER_XT_SET y \
        --set-val IP_NF_TARGET_TTL y \
        --set-val IP6_NF_TARGET_HL y \
        --set-val IP6_NF_MATCH_HL y \
        --set-val IP6_NF_NAT y \
        --set-val NF_NAT_IPV6 y \
        --set-val IP6_NF_TARGET_MASQUERADE y
        
        if [[ "$KSU_VARIANT" != "none" ]]; then
            info -n "Setting $KSU_LABEL & SUSFS configs..."
            # KernelSU & SUSFS
            $CONFIG_TOOL --file $DEFCONFIG \
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
            --set-val OVERLAY_FS y \
            --set-val TMPFS_XATTR y \
            --set-val TMPFS_POSIX_ACL y
        else
            info -n "Skipping KernelSU & SUSFS configs (vanilla kernel selected)..."
        fi
        
        if grep -q '^CONFIG_LSM=' "$DEFCONFIG"; then
            info -n "CONFIG_LSM found, adding baseband_guard"
            sed -i '/^CONFIG_LSM=/ s/"$/,baseband_guard"/' "$DEFCONFIG"
        fi
        
    done
    
    # 3. Metadata Configuration
    gen_metadata
    
    CONFIG_END=$(_ts)
else
    CONFIG_START=$(_ts)
    
    gen_metadata
    
    CONFIG_END=$(_ts)
fi

# 5. Patching / Setup
if [[ $NO_PATCH -eq 0 && $BUILD_ONLY -eq 0 ]]; then
    PATCH_START=$(_ts)
    
    pushd "$KERNEL_DIR" > /dev/null
    
    # Only fetch and setup KernelSU/SUSFS if a variant was selected
    if [[ "$KSU_VARIANT" != "none" ]]; then
        info -n "Setting up $KSU_LABEL..."
        curl -LSs $KERNELSU_SETUP_URL | bash -s $KERNELSU_SETUP_BRANCH
        
        if [[ -d "./$KSU_DIR" ]]; then
            # Version Detection
            pushd "./$KSU_DIR/kernel" > /dev/null
            BASE_VERSION=$(grep -m1 -oP 'expr\s*\K[0-9]+' Kbuild)
            info -n "Detected $KSU_LABEL Base Version: $BASE_VERSION"
            
            KSU_VERSION=$(expr $(git rev-list --count HEAD) "+" $BASE_VERSION)
            info -n "Detected $KSU_LABEL Version: $KSU_VERSION"
            
            if [ -n "${GITHUB_ENV:-}" ]; then
                info -n "Writing $KSU_LABEL version to GitHub Actions environment..."
                echo "REL_KERNEL=$KSU_VERSION" >> "$GITHUB_ENV"
            fi
            
            popd > /dev/null
            
            if [[ $NO_SUSFS -eq 0 ]]; then
                info -n "Copying SUSFS patches to kernel source..."
                cp $SUSFS_PATCHES/kernel_patches/fs/* ./fs/
                cp $SUSFS_PATCHES/kernel_patches/include/linux/* ./include/linux/
                
                # Get SUSFS version from header we just copied
                SUSFS_VER=$(grep '#define SUSFS_VERSION' ./include/linux/susfs.h | awk -F'"' '{print $2}' || echo "N/A")
                info -n "Detected SUSFS Version: $SUSFS_VER"
                
                if [ -n "${GITHUB_ENV:-}" ]; then
                    info -n "Writing SUSFS version to GitHub Actions environment..."
                    echo "REL_SUSFS=$SUSFS_VER" >> "$GITHUB_ENV"
                fi
                
                # Patch Main Kernel
                info -n "Patching SUSFS into Kernel..."
                patch -p1 < $SUSFS_PATCHES/kernel_patches/50_add_susfs_in_gki-android12-5.10.patch || true
                
                # Samsung Specific Patches
                info -n "Applying Samsung device patches..."
                for rej in $(find ./ -maxdepth 8 -name "*.rej" -exec basename {} .rej \;); do
                    FIX_PATCH="$KERNEL_PATCHES/samsung/SM-A155F-Oneui7/fix_$rej.patch"
                    if [[ -f "$FIX_PATCH" ]]; then
                        info "Patching $rej"
                        patch -p1 --forward < "$FIX_PATCH" || true
                    else
                        warn -n "No fix patch found for $rej; skipping."
                    fi
                done
                
                # Patch $KSU_LABEL internal
                pushd "./$KSU_DIR" > /dev/null
                info -n "Patching SUSFS into $KSU_LABEL ($SUSFS_KSU_INTERNAL_PATCH_DESC)..."
                if [[ "$KSU_VARIANT" == "ksun" ]]; then
                    patch -p1 --forward < $PATCHES/10_pershoot_enable_susfs_for_ksun.patch || true
                else
                    patch -p1 --forward < $SUSFS_PATCHES/kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch || true
                fi
                
                REJ_FILES=$(find ./kernel -maxdepth 2 -name "*.rej" -exec basename {} .rej \;)
                
                if [[ -z "$REJ_FILES" ]]; then
                    info -n "No .rej files found. Nothing to patch."
                else
                    info -n "Patching .rej fixes in $KSU_LABEL..."
                    for rej in $REJ_FILES; do
                        FIX_PATCH="$KERNEL_PATCHES/next/susfs_fix_patches/$SUSFS_VER/fix_$rej.patch"
                        
                        if [[ -f "$FIX_PATCH" ]]; then
                            info -n "Patching $rej"
                            patch -p1 --forward < "$FIX_PATCH" || true
                        else
                            warn -n "No fix patch found for $rej; skipping."
                        fi
                    done
                fi
                
                # Multi-manager Support for SUSFS
                if [[ "$KSU_VARIANT" == "ksun" ]]; then
                    if [ "$KSU_VERSION" -le 33095 ]; then
                        info -n "Patching Multi-manager Support for SusFS kernel!"
                        patch -p1 --forward < "$KERNEL_PATCHES/next/susfs_fix_patches/$SUSFS_VER/multi_manager.patch" ||true
                    else
                        info -n "Skipping Multi-manager patch for newer KernelSU versions (>= 33096)"
                    fi
                    
                    if [ "$KSU_VERSION" -ge 33068 ] && [ "$KSU_VERSION" -lt 33070 ]; then
                        echo ""
                        info -n "Patching Multi-manager sepolicy Support for SusFS kernel!"
                        patch -p1 --forward < "$KERNEL_PATCHES/next/susfs_fix_patches/$SUSFS_VER/multi_sepolicy_fix.patch" ||true
                    else
                        info -n "Skipping Multi-manager sepolicy patch for KernelSU versions outside 33068-33069"
                    fi
                else
                    info -n "Skipping Multi-manager patches; not applicable to standard KernelSU."
                fi
                popd > /dev/null
                
            else
                warn -n "SUSFS support is disabled; skipping SUSFS-related patches."
            fi
        else
            err "$KSU_LABEL setup failed! Please check the output above for errors."
            exit 1
        fi
    fi
    
    # ---> Common Kernel Optimizations (These run for all builds) <---
    info -n "Applying common kernel optimization patches..."
    patch -p1 --forward < "$KERNEL_PATCHES/common/optimized_mem_operations.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/file_struct_8bytes_align.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/reduce_cache_pressure.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/mem_opt_prefetch.patch"
    
    info -n "Applying optimise_noneon_memcmp_android12_5.10 patch..."
    patch -p1 --forward < "$PATCHES/optimise_noneon_memcmp_android12_5.10.patch"
    
    info -n "Applying pm_wakeup_event patch..."
    patch -p1 --forward < "$KERNEL_PATCHES/common/minimise_wakeup_time.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/int_sqrt.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/force_tcp_nodelay.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/reduce_gc_thread_sleep_time.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/add_timeout_wakelocks_globally.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/f2fs_reduce_congestion.patch"
    patch -p1 --forward < "$KERNEL_PATCHES/common/reduce_freeze_timeout.patch"
    
    info -n "Applying clear_page_16bytes_align patch..."
    if [ "$(printf '%s\n' "${kernel_version}" "$MIN_VERSION" | sort -V | head -n1)" = "${kernel_version}" ]; then
        patch -p1 --forward < "$KERNEL_PATCHES/common/clear_page_16bytes_align.patch"
    else
        cat "$KERNEL_PATCHES/common/clear_page_16bytes_align.patch" | sed -e 's/SYM_FUNC_START_PI(clear_page)/SYM_FUNC_START_PI(__pi_clear_page)/' | patch -p1 -F3 --forward
    fi
    
    info -n "Applying add_limitation_scaling_min_freq patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/add_limitation_scaling_min_freq.patch"
    info -n "Applying re_write_limitation_scaling_min_freq patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/re_write_limitation_scaling_min_freq.patch"
    info -n "Applying adjust_cpu_scan_order patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/adjust_cpu_scan_order.patch"
    
    info -n "Applying avoid_extra_s2idle_wake_attempts patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/avoid_extra_s2idle_wake_attempts.patch"
    
    info -n "Applying disable_cache_hot_buddy patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/disable_cache_hot_buddy.patch"
    info -n "Applying f2fs_enlarge_min_fsync_blocks patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/f2fs_enlarge_min_fsync_blocks.patch"
    info -n "Applying increase_ext4_default_commit_age patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/increase_ext4_default_commit_age.patch"
    info -n "Applying increase_sk_mem_packets patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/increase_sk_mem_packets.patch"
    info -n "Applying reduce_pci_pme_wakeups patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/reduce_pci_pme_wakeups.patch"
    info -n "Applying reduce_s2idle_wakeups patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/silence_irq_cpu_logspam.patch"
    info -n "Applying silence_system_logspam patch..."
    patch -p1 -F3 --forward < "$KERNEL_PATCHES/common/silence_system_logspam.patch"
    info -n "Applying use_unlikely_wrap_cpufreq patch..."
    patch -p1 --forward < "$KERNEL_PATCHES/common/use_unlikely_wrap_cpufreq.patch"
    
    info -n "Applying BBRv3 patch..."
    patch -p1 < "$KERNEL_PATCHES/common/bbrv3/0001-net-tcp-backport-BBRv3-to-${android_version}-${kernel_version}.patch"
    
    if [ "${android_version}" = "android12" ] && [ "${kernel_version}" = "5.10" ]; then
        if ! grep -qF 'int proc_dou8vec_minmax(' ./include/linux/sysctl.h; then
            info -n "Applying BBRv3 sysctl_add_proc_dou8vec_minmax patch..."
            patch -p1 < "$KERNEL_PATCHES/common/bbrv3/sysctl_add_proc_dou8vec_minmax.patch"
            
            info -n "Applying BBRv3 sysctl_fix_data"
            patch -p1 < "$KERNEL_PATCHES/common/bbrv3/sysctl_fix_data-races_in_proc_dou8vec_minmax.patch"
        fi
    fi
    
    info -n "Setting up Baseband Guard..."
    curl -LSs https://github.com/poqdavid/Baseband-guard/raw/main/setup.sh | bash
    
    info -n "Adding Baseband Guard into kconfig"
    sed -i '/^config LSM$/,/^help$/{ /^[[:space:]]*default/ { /baseband_guard/! s/selinux/selinux,baseband_guard/ } }' security/Kconfig
    
    if [[ "$kernel_version" == 6.* ]]; then
        info -n "Applying unicode bypass fix for 6.1+..."
        patch -p1 < "$KERNEL_PATCHES/common/unicode_bypass_fix_6.1+.patch"
    else
        info -n "Applying unicode bypass fix for 6.1-..."
        patch -p1 < "$KERNEL_PATCHES/common/unicode_bypass_fix_6.1-.patch"
    fi
    
    info -n "Applying droidspaces patch (fix_sysvipc_kabi_6_7_8.patch)..."
    patch -p1 < "$KERNEL_PATCHES/common/droidspaces/fix_sysvipc_kabi_6_7_8.patch"
    
    if [[ "$kernel_version" == 5.10* ]]; then
        info -n "Applying droidspaces patch (fix_abi_padding_for_posix_mqueue.patch)..."
        patch -p1 < "$KERNEL_PATCHES/common/droidspaces/fix_abi_padding_for_posix_mqueue.patch"
    fi
    
    info -n "Applying droidspaces patch (0001-Guard-USER_NS-for-non-root-users.patch)..."
    patch -p1 < "$KERNEL_PATCHES/common/droidspaces/0001-Guard-USER_NS-for-non-root-users.patch"
    
    info -n "Applying NTSync patch (ntsync_base.patch)..."
    patch -p1 < "$KERNEL_PATCHES/common/ntsync/ntsync_base.patch"
    
    ntsync_compat_patch_file="ntsync_compat_${android_version}-${kernel_version}.patch"
    ntsync_compat_patch_path="$KERNEL_PATCHES/common/ntsync/${ntsync_compat_patch_file}"
    
    info -n "Applying NTSync patch ($ntsync_compat_patch_file)..."
    
    if [[ -f "$ntsync_compat_patch_path" ]]; then
        patch -p1 < "$ntsync_compat_patch_path"
    else
        info -n "No specific NTSync compat patch found for Android $android_version / Kernel $kernel_version; skipping compat patch."
    fi
    
    popd > /dev/null
    PATCH_END=$(_ts)
else
    warn -n "Patching steps skipped."
fi

# 6. Build
BUILD_START=$(_ts)
info -n "Starting Kernel Build..."

pushd "$KERNEL_DIR" > /dev/null
cd ../kernel
./build/build.sh
popd > /dev/null

BUILD_END=$(_ts)
info -n "Build completed successfully."
send_discord_file "SUCCESS" "Kernel build completed successfully. 🎉" 65280