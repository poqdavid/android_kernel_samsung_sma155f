## A KernelSU module for SUS-FS patched kernel ##

This module is used for installing a userspace helper tool called **ksu_susfs** and **sus_su** into /data/adb/ and provides a script example to communicate with SUSFS kernel

- To enable umount for zygote spawned system process on boot, create a new file to `/data/adb/susfs_umount_for_zygote_system_process` and reboot.

- To disable `KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT` on boot, create a new file to `/data/adb/susfs_no_auto_add_sus_ksu_default_mount` and reboot.

- To disable `KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT` on boot, create a new file to `/data/adb/susfs_no_auto_add_sus_bind_mount` and reboot.

- To disable `KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT` on boot, create a new file to `/data/adb/susfs_no_auto_add_try_umount_for_bind_mount` and reboot.

- To prevent a new `bind mount` from being added to `auto add try_umount`, it is suggested to do it in post-fs-data.d/ before post-fs-data.sh, and do following steps in the script:
    1. setup proper permission on source path
    2. add target mount path to sus_kstat
    3. bind mount the target path
    4. update sus_kstat on target path
  so now it won't automatically add the path to try_umount and the mount will stay for all processes while still be hidden from mountinfo, stat and statfs
  
