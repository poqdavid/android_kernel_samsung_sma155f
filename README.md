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
./scripts/config --file kernel-5.10/arch/arm64/configs/a15_defconfig \
  -d UH \
  -d RKP \
  -d KDP \
  -d SECURITY_DEFEX \
  -d INTEGRITY \
  -d FIVE \
  -d TRIM_UNUSED_KSYMS
```

```bash
./scripts/config --file kernel-5.10/arch/arm64/configs/a15_00_defconfig \
  -d UH \
  -d RKP \
  -d KDP \
  -d SECURITY_DEFEX \
  -d INTEGRITY \
  -d FIVE \
  -d TRIM_UNUSED_KSYMS
```

```bash
./scripts/config --file kernel-5.10/arch/arm64/configs/mgk_64_k510_defconfig \
  -d UH \
  -d RKP \
  -d KDP \
  -d INTEGRITY \
  -d FIVE \
  -d TRIM_UNUSED_KSYMS
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
cd ../kernel
./build/build.sh
```

## 2. Output Files
- **Kernel**: `out/target/product/a15/obj/KERNEL_OBJ/kernel-5.10/arch/arm64/boot/Image.gz`
- **Module**: `out/target/product/a15/obj/KERNEL_OBJ/*.ko`

## 3. How to Clean
```bash
make clean
```

## Create KerenlSU boot image.

- Place your boot.img here
- Run ./scripts/kernelsu

## Acknowledgements

This project includes code from the https://github.com/fei-ke/android_kernel_samsung_sm8550/ project, licensed under the GPL-2.0.

This project includes executable file/s from https://github.com/topjohnwu/Magisk/ project, licensed under the GPL-3.0.

This project includes executable file/s from https://github.com/tiann/KernelSU/ project, licensed under the GPL-3.0.
