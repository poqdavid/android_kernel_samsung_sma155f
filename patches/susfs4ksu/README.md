## Introduction ##
- An addon root hiding kernel patches and userspace module for KernelSU.

- The userspace tool `ksu_susfs`, as well as the ksu module, require a susfs patched kernel to work.

# Warning #
- This is only experimental code, that said it can harm your system or cause performance hit, **YOU ARE !! W A R N E D !!** already

## Compatibility ##
- The susfs kernel patches may differ for different kernel version or even on the same kernel version, you may need to create your own patches for your kernel.

## Patch Instruction (For GKI Kernel only and building from official google artifacts) ##
**- Prerequisite -**
1. All susfs patches are mainly based on the **original official KernelSU (the one from weishu)**, since KernelSU updates frequently and susfs needs to sync up with it, so it is now suggested to just clone the KernelSU and susfs repo without tag/release tag. If the applied susfs patches fails on KernelSU, you need to rollback KernelSU version to a specific commit and apply the patches again.
2. Since v2.0.0, SUSFS does not rely on kernel features like KPROBES, KRETPROBES and HAVE_SYSCALL_TRACEPOINTS, which means it will patch all the sucompat code in KernelSU, fs/exec.c, fs/open.c, and fs/stat.c.
3. Do NOT apply custom manual hooks for sucompat unless you implement your own one instead of the susfs one.


**- Apply SUSFS patches -**
1. Make sure you follow the offical KSU guild here to clone and build the kernel with KSU: `https://kernelsu.org/guide/how-to-build.html`, the kernel root directory should be `$KERNEL_REPO/common`, you should run script to clone KernelSU in `$KERNEL_REPO`, **make sure you clone with a tag version.**
2. Clone this susfs branch with a **tag / release tag** or up to a commit message containing **"Bump version to vX.X.X"**, as they are more stable in general.
3. Run `cp ./kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch $KERNEL_REPO/KernelSU/`
4. Run `cp ./kernel_patches/50_add_susfs_in_kernel-<kernel_version>.patch $KERNEL_REPO/common/`
5. Run `cp ./kernel_patches/fs/* $KERNEL_REPO/common/fs/`
6. Run `cp ./kernel_patches/include/linux/* $KERNEL_REPO/common/include/linux/`
7. Run `cd $KERNEL_REPO/KernelSU` and then `patch -p1 < 10_enable_susfs_for_ksu.patch`
8. Run `cd $KERNEL_REPO/common` and then `patch -p1 < 50_add_susfs_in_kernel.patch`, **if there are failed patches, you may try to patch them manually by yourself.**
9. If you want to make your kernel support other KSU manager variant, you can add its own hash size and hash in `ksu_is_manager_apk()` function in `KernelSU/kernel/apk_sign.c`
10. Make sure again to have `CONFIG_KSU` and `CONFIG_KSU_SUSFS` enabled before building the kernel, some other SUSFS feature may be disabled by default, you may enable/disable them via `menuconfig`, `kernel defconfig`, or change the `default [y|n]` option under each `config KSU_SUSFS_` option in `$KernelSU_repo/kernel/Kconfig` if you build with a new defconfig every time.
11. For `gki kernel android14` or above, if you are building from google artifacts, it is necessary to delete the file `$KERNEL_REPO/common/android/abi_gki_protected_exports_aarch64` and `$KERNEL_REPO/common/android/abi_gki_protected_exports_x86_64`, otherwise some modules like WiFi will not work. Or you can just remove those files whenever they exist in your kernel repo.
12. If you want to flash the fresh built gki boot.img, then before you build the kernel, first you need to fix or hardcode the `local spl_date` in function `build_gki_boot_images()` in `$KERNEL_REPO/build/kernel/build_utils.sh` to match the current boot security patch level of your phone. Or you can just use magiskboot to unpack and repack the built kernel for your stock boot.img.
13. Build and flash the kernel.
14. For other building tips, please refer to the section **[Other Building Tips]** below.

## Build ksu_susfs userspace tool ##
1. Run `./build_ksu_susfs_tool.sh` to build the userspace tool `ksu_susfs`, and the arm64 and arm binary will be copied to `ksu_module_susfs/tools/` as well.
2. Now you can also push the compiled `ksu_susfs` tool to `/data/adb/ksu/bin/` so that you can run it directly in adb root shell or termux root shell, as well as in your own ksu modules.

## Build susfs4ksu module ##
- The ksu module here is just a demo to show how to use it.
- It will also copy the `ksu_susfs` tool to `/data/adb/ksu/bin/` as well when installing the module.

1. ksu_susfs tool can be run in any stage scripts, post-fs-data.sh, services.sh, boot-completed.sh according to your own need.
2. Run `./build_ksu_module.sh` to build the susfs KSU module.

## Usage of ksu_susfs and supported features ##
- Run `ksu_susfs` in root shell for detailed usages.
- See `$KernelSU_repo/kernel/Kconfig` for supported features after applying the susfs patches.

## Other Building Tips ##
- To only remove the `-dirty` string from kernel release string, open file `$KERNEL_ROOT/scripts/setlocalversion`, then look for all the lines that containing `printf '%s' -dirty`, and replace it with `printf '%s' ''`
- Alternatively, If you want to directly hardcode the whole kernel release string, then open file `$KERNEL_ROOT/scripts/setlocalversion`, look for the last line `echo "$res"`, and for example, replace it with `echo "-android13-01-gb123456789012-ab12345678"`
- To hardcode your kernel version string, open `$KERNEL_ROOT/scripts/mkcompile_h`, and look for line `UTS_VERSION="$(echo $UTS_VERSION $CONFIG_FLAGS $TIMESTAMP | cut -b -$UTS_LEN)"`, then for example, replace it with `UTS_VERSION="#1 SMP PREEMPT Mon Jan 1 18:00:00 UTC 2024"`. But for kernel 6.1+, you need to edit `${KERNEL_ROOT}/init/Makefile`, and look for line `build-timestamp = $(or $(KBUILD_BUILD_TIMESTAMP), $(build-timestamp-auto))`, replace it with your own timestamp, like `build-timestamp = "Wed Jan 30 12:00:00 UTC 2025"`
- To hardcode your kernel version string which can be seen from /proc/version, open `$KERNEL_ROOT/scripts/mkcompile_h`, then search for variable name `LINUX_COMPILE_BY` and `LINUX_COMPILE_HOST`, then for example, append `LINUX_COMPILE_BY=build-user` and `LINUX_COMPILE_HOST=build-host` after line `UTS_VERSION="$(echo $UTS_VERSION $CONFIG_FLAGS $TIMESTAMP | cut -b -$UTS_LEN)"`
- To spoof the `/proc/config.gz` with the stock config, 

   1. Make sure you are on the stock ROM and using stock kernel.
   2. Use adb shell or root shell to pull your stock `/proc/config.gz` from your device to PC.
   3. Decompress it using `gunzip` or whatever tools, then copy it to `$KERNEL_ROOT/arch/arm64/configs/stock_defconfig`
   4. Open file `$KERNEL_ROOT/kernel/Makefile`.
   5. Look for line `$(obj)/config_data: $(KCONFIG_CONFIG) FORCE`, and replace it with `$(obj)/config_data: arch/arm64/configs/stock_defconfig FORCE`

## Credits ##
- KernelSU: https://github.com/tiann/KernelSU
- KernelSU fork: https://github.com/5ec1cff/KernelSU
- @Kartatz: for ideas and original commit from https://github.com/Dominium-Apum/kernel_xiaomi_chime/pull/1/commits/74f8d4ecacd343432bb8137b7e7fbe3fd9fef189

## Telegram ##
- @simonpunk

