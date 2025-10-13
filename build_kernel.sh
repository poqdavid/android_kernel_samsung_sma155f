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
--set-val IP6_NF_NAT y \
--set-val IP6_NF_TARGET_MASQUERADE y \
--set-val NF_NAT_IPV6 y

# KernelSU Next configs
./kernel-5.10/scripts/config --file kernel-5.10/arch/arm64/configs/a15_00_defconfig \
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
--set-val KSU_SUSFS_SUS_SU n \
--set-val KSU y

cd kernel-5.10

print_msg "$GREEN" "Modified configs ..."

print_msg "$GREEN" "Configuring Kernel metadata..."
sed -i '$s|echo "\$res"|echo "-android12-9-31117096"|' ./scripts/setlocalversion
perl -pi -e 's{UTS_VERSION="\$\(echo \$UTS_VERSION \$CONFIG_FLAGS \$TIMESTAMP \| cut -b -\$UTS_LEN\)"}{UTS_VERSION="#1 SMP PREEMPT Thu Jul 31 08:40:06 UTC 2025"}' ./scripts/mkcompile_h
sed -i 's/-dirty//' ./scripts/setlocalversion
print_msg "$GREEN" "Finished Configuring Kernel metadata..."

print_msg "$GREEN" "Generating configs..."

python2 scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig --kernel-defconfig-overlays entry_level.config -m user -o ../out/target/product/a15/obj/KERNEL_OBJ/build.config

print_msg "$GREEN" "Finished Generating configs..."

config_end_time=$(date +%s)

print_msg "$GREEN" "Setting up KernelSU Next SUSFS..."

patch_start_time=$(date +%s)

curl -LSs "https://raw.githubusercontent.com/poqdavid/KernelSU-Next/next/kernel/setup.sh" | bash -s next

print_msg "$GREEN" "Finished Setting up KernelSU Next SUSFS..."

print_msg "$GREEN" "Patching up..."

echo " "
print_msg "$GREEN" "Copying susfs4ksu Patchees to the Kernel..."
cp ../patches/susfs4ksu/kernel_patches/fs/* ./fs/
cp ../patches/susfs4ksu/kernel_patches/include/linux/* ./include/linux/
print_msg "$GREEN" "Finished Copying SUSFS4KSU Patchees to the Kernel..."

echo " "
print_msg "$GREEN" "Patching SUSFS in Kernel..."
patch -p1 < ../patches/susfs4ksu/kernel_patches/50_add_susfs_in_gki-android12-5.10.patch

echo " "
print_msg "$GREEN" "Patching namespace fix in Kernel..."
patch -p1 < ../patches/kernel_patches/next/hotfixsamsungnamespace.patch

echo " "
print_msg "$GREEN" "Patching syscall_hooks in Kernel..."
patch -p1  --fuzz=3 < ../patches/kernel_patches/next/syscall_hooks.patch

echo " "
print_msg "$GREEN" "Patching Makefile in Kernel for config_data..."
patch -p1 --forward < ../patches/fake_config.patch

cd ./KernelSU-Next/

BASE_VERSION=10200
KSU_VERSION=$(expr $(/usr/bin/git rev-list --count HEAD) "+" $BASE_VERSION)

echo " "
print_msg "$GREEN" "Detected KernelSU Next Version: $KSU_VERSION"

echo " "
print_msg "$GREEN" "Patching SUSFS in KernelSU Next..."

susfs_version=$(grep '#define SUSFS_VERSION' ../../kernel-5.10/include/linux/susfs.h | awk -F'"' '{print $2}')

echo " "
print_msg "$GREEN" "Detected SUSFS Version: $susfs_version"

echo " "
print_msg "$GREEN" "Patching 10_enable_susfs_for_ksu.patch..."
patch -p1 --forward < ../../patches/susfs4ksu/kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch


for file in $(find ./kernel -maxdepth 2 -name "*.rej" -printf "%f\n" | cut -d'.' -f1); do
    echo " "
    print_msg "$GREEN" "Patching file: $file.c with kernel_patches/next/susfs_fix_patches/$susfs_version/fix_$file.c.patch"
    patch -p1 --forward < "../../patches/kernel_patches/next/susfs_fix_patches/$susfs_version/fix_$file.c.patch"
done

echo " "
print_msg "$GREEN" "Patching fix_sucompat.c.patch..."
patch -p1 --forward < "../../patches/kernel_patches/next/susfs_fix_patches/$susfs_version/fix_kernel_compat.c.patch"

cd ..
print_msg "$GREEN" "Finished Patching up..."
patch_end_time=$(date +%s)

# Start timing
build_start_time=$(date +%s)

export LTO=thin
export ARCH=arm64
export PLATFORM_VERSION=12
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"
export OUT_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export DIST_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export BUILD_CONFIG="../out/target/product/a15/obj/KERNEL_OBJ/build.config"

echo " "
print_msg "$GREEN" "Building Kernel..."

cd ../kernel
./build/build.sh

# End timing
build_end_time=$(date +%s)

print_msg "$GREEN" "Finished Building Kernel..."

echo " "

print_runtime "Config runtime" config_start_time config_end_time
print_runtime "Patch runtime" patch_start_time patch_end_time
print_runtime "Build runtime" build_start_time build_end_time
