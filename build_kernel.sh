#!/bin/bash

# Color Variables
RED="\e[1;31m"
GREEN="\e[1;32m"
RESET="\e[0m"

# Print message function
print_msg() {
    local COLOR=$1
    shift
    echo -e "${COLOR}$*${RESET}"
}

# Print runtime function
print_runtime() {
    # Calculate runtime in seconds
    runtime=$(($3 - $2))
    
    # Convert seconds to HH:MM:SS format
    hours=$((runtime / 3600))
    minutes=$(((runtime % 3600) / 60))
    seconds=$((runtime % 60))
    
    # Display runtime with proper formatting (zero-padded)
    printf "\e[1;32m$1: %02d:%02d:%02d\n" $hours $minutes $seconds
}

config_start_time=$(date +%s)

# Script header
print_msg "$GREEN" "\n - Build script for Samsung kernel image - "
print_msg "$RED" "       by poqdavid \n"

./clean_build.sh

print_msg "$GREEN" "Modifying configs..."

# Samsung related configs like Kernel Protection
./kernel-5.10/scripts/config --file kernel-5.10/arch/arm64/configs/a15_00_defconfig \
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
--set-val RKP_CRED n

# Kernel optimizations
./kernel-5.10/scripts/config --file kernel-5.10/arch/arm64/configs/a15_00_defconfig \
--set-val TMPFS_XATTR y \
--set-val TMPFS_POSIX_ACL y \
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
--set-str DEFAULT_TCP_CONG "bbr" \
--set-val DEFAULT_RENO n \
--set-val DEFAULT_CUBIC n \
--set-val KSU y

print_msg "$GREEN" "Modified configs ..."

cd kernel-5.10

print_msg "$GREEN" "Setting up KernelSU..."

curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v1.0.5

print_msg "$GREEN" "Finished Setting up KernelSU..."

#print_msg "$GREEN" "Patching up Kernel..."
#patch -p1 -F 3 < ../patches/syscall_hooks.patch
#patch -p1 -F 3 < ../patches/new_hooks.patch
#patch -p1 -F 3 < ../patches/ksu_hooks.patch
#print_msg "$GREEN" "Finished Patching up Kernel..."

print_msg "$GREEN" "Generating configs..."

python2 scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig --kernel-defconfig-overlays entry_level.config -m user -o ../out/target/product/a15/obj/KERNEL_OBJ/build.config

print_msg "$GREEN" "Finished Generating configs..."

config_end_time=$(date +%s)

build_start_time=$(date +%s)

export LTO=thin
export ARCH=arm64
export PLATFORM_VERSION=12
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"
export OUT_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export DIST_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export BUILD_CONFIG="../out/target/product/a15/obj/KERNEL_OBJ/build.config"

print_msg "$GREEN" "Building Kernel..."

cd ../kernel
./build/build.sh

build_end_time=$(date +%s)

print_msg "$GREEN" "Finished Building Kernel..."

echo " "

print_runtime "Config runtime" config_start_time config_end_time
print_runtime "Build runtime" build_start_time build_end_time