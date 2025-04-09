#!/bin/bash

./kernel-5.10/scripts/config --file kernel-5.10/arch/arm64/configs/a15_defconfig \
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
  --set-val RKP_CFP n

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
  --set-val RKP_CFP n

./kernel-5.10/scripts/config --file kernel-5.10/arch/arm64/configs/mgk_64_k510_defconfig \
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
  --set-val RKP_CFP n

cd kernel-5.10
python scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig --kernel-defconfig-overlays entry_level.config -m user -o ../out/target/product/a15/obj/KERNEL_OBJ/build.config

export LTO=thin
export ARCH=arm64
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"
export OUT_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export DIST_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export BUILD_CONFIG="../out/target/product/a15/obj/KERNEL_OBJ/build.config"

cd ../kernel
./build/build.sh
