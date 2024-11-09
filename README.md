# Build Instructions

## 1. How to Build

### Get Toolchain
Get the proper toolchain packages from AOSP, CodeSourcery, or other sources.
[Download link](https://opensource.samsung.com/uploadSearch?searchValue=toolchain)

Please unzip the toolchain file in the path where `build_kernel.sh` is located:
- `kernel/prebuilts/`
- `kernel/prebuilts-master/`
- `prebuilts/`

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