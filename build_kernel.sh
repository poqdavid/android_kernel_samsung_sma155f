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
--set-val KSU_WITH_KPROBES n \
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

print_msg "$GREEN" "Modified configs ..."

cd kernel-5.10

print_msg "$GREEN" "Setting up KernelSU Next SUSFS..."

#curl -LSs "https://raw.githubusercontent.com/KernelSU-Next/KernelSU-Next/next-susfs/kernel/setup.sh" | bash -s next-susfs
curl -LSs "https://raw.githubusercontent.com/poqdavid/KernelSU-Next/next/kernel/setup.sh" | bash -s v1.0.7

print_msg "$GREEN" "Finished Setting up KernelSU Next SUSFS..."

print_msg "$GREEN" "Patching up..."

print_msg "$GREEN" "Copying susfs4ksu Patchees to the Kernel..."
cp ../patches/susfs4ksu/kernel_patches/fs/* ./fs/
cp ../patches/susfs4ksu/kernel_patches/include/linux/* ./include/linux/
print_msg "$GREEN" "Finished Copying SUSFS4KSU Patchees to the Kernel..."

print_msg "$GREEN" "Patching SUSFS in Kernel..."
patch -p1 < ../patches/susfs4ksu/kernel_patches/50_add_susfs_in_gki-android12-5.10.patch

print_msg "$GREEN" "Patching namespace fix in Kernel..."
patch -p1 < ../patches/kernel_patches/next/hotfixsamsungnamespace.patch

print_msg "$GREEN" "Patching syscall_hooks in Kernel..."
patch -p1 -F 3 < ../patches/kernel_patches/next/syscall_hooks.patch

print_msg "$GREEN" "Patching Makefile in Kernel for config_data..."
patch -p1 --forward < ../patches/fake_config.patch

cd ./KernelSU-Next/
print_msg "$GREEN" "Patching SUSFS in KernelSU Next..."
patch -p1 --forward < ../../patches/kernel_patches/next/0001_susfs_157_for_ksunext.patch
patch -p1 --forward < ../../patches/hotfixcorehookc.patch
cd ..
print_msg "$GREEN" "Finished Patching up..."

print_msg "$GREEN" "Configuring Kernel metadata..."
sed -i '$s|echo "\$res"|echo "-android12-9-28575149"|' ./scripts/setlocalversion
perl -pi -e 's{UTS_VERSION="\$\(echo \$UTS_VERSION \$CONFIG_FLAGS \$TIMESTAMP \| cut -b -\$UTS_LEN\)"}{UTS_VERSION="#1 SMP PREEMPT Thu Mar 06 09:35:51 UTC 2025"}' ./scripts/mkcompile_h
print_msg "$GREEN" "Finished Configuring Kernel metadata..."

print_msg "$GREEN" "Generating configs..."

python2 scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig --kernel-defconfig-overlays entry_level.config -m user -o ../out/target/product/a15/obj/KERNEL_OBJ/build.config

print_msg "$GREEN" "Finished Generating configs..."

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

print_msg "$GREEN" "Finished Building Kernel..."
