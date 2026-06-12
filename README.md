# Build Instructions

## 1. How to Build

### Get Toolchain
Get the proper toolchain packages from AOSP, CodeSourcery, or other sources.
[Download link](https://opensource.samsung.com/uploadSearch?searchValue=toolchain)

Please unzip the toolchain file in the path where `build_kernel.sh` is located:
- `kernel/prebuilts/`
- `kernel/prebuilts-master/`
- `prebuilts/`

### Patch the configs for KernelSU
```bash
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
```

### Set Build Environment and Export Target Config
```bash
cd kernel-5.10
python scripts/gen_build_config.py --kernel-defconfig a15_00_defconfig \
                                   --kernel-defconfig-overlays "entry_level.config" \
                                   -m user -o ../out/target/product/a15/obj/KERNEL_OBJ/build.config

export ARCH=arm64
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"
export OUT_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export DIST_DIR="../out/target/product/a15/obj/KERNEL_OBJ"
export BUILD_CONFIG="../out/target/product/a15/obj/KERNEL_OBJ/build.config"
```

### To Build
```bash
- Place your boot image in the root
- Run ./build_kernel.sh to build the Kernel
- Run ./script/repack to repack the boot image with the new Kernel
```

## 2. Output Files
- **Kernel**: `out/target/product/a15/obj/KERNEL_OBJ/kernel-5.10/arch/arm64/boot/Image.gz`
- **Module**: `out/target/product/a15/obj/KERNEL_OBJ/*.ko`

## 3. How to Clean
```bash
./clean_build.sh
```

## Acknowledgements

This project includes code from the https://github.com/ReeViiS69/sm155f/ project, licensed under the GPL-2.0. Also, a huge thanks to ReeViiS69 for helping me build the kernel.

This project includes code from the https://github.com/fei-ke/android_kernel_samsung_sm8550/ project, licensed under the GPL-2.0.

This project includes executable file/s from https://github.com/topjohnwu/Magisk/ project, licensed under the GPL-3.0.

This project includes executable file/s from https://github.com/tiann/KernelSU/ project, licensed under the GPL-3.0.

This project include file/s from https://android.googlesource.com/platform/external/avb project, licensed under the Apache License, Version 2.0.

**Donate**
<br/>**BTC Legacy:** 1Q2JQG3iCLZPT2iJfDLow1oQVGKmxheoAh
<br/>**BTC Segwit:** bc1q8gurls0wjkfe43ygmrqmu2pzmyjetnrvgws9sr
<br/>**BCH:** qrks52smlqw7d8700d77uqvmve03d4knzvd2vghaqz
<br/>**ETH:** 0x7218779242a8425879B09969431c20F5eC1a192D