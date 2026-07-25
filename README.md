<div align="center">

# KernelSU Next for Samsung A15 4G

[![KernelSU](https://img.shields.io/badge/KernelSU-Supported-green)](https://kernelsu.org/)
[![SUSFS](https://img.shields.io/badge/SUSFS-Integrated-orange)](https://gitlab.com/simonpunk/susfs4ksu)
[![Telegram](https://img.shields.io/badge/Join-Build_Notification-blue?logo=telegram&style=flat-square)](https://t.me/sma155fkernelbuilds)

</div>
<div align="center">

## ⚠️ Your warranty is no longer valid!

I am **not responsible** for bricked devices, damaged hardware, or any issues that arise from using this kernel.

**Please** do thorough research and fully understand the features included in this kernel before flashing it!

By flashing this kernel, **YOU** are choosing to make these modifications. If something goes wrong, **do not blame me**!

---

### 🚨 Proceed at your own risk!

---

## 🔧 Available Kernels

| Kernel | Repository | Status |
|--------|------------|--------|
| 📱 **Samsung** | [SamsungA15_KernelSUNext_SUSFS](https://github.com/poqdavid/android_kernel_samsung_sma155f/tree/kernelsunext) | ✅ Active |

---
<div align="left">

## ✨ Features

- 🔐 **KernelSU/KernelSUNext**: A root solution for Android GKI devices that works in kernel mode and grants root permission to userspace applications directly in kernel space
- 🥷 **SUSFS**: An addon root hiding kernel patches and userspace module for KernelSU
- 🛡️ **BBG**: LSM-based Baseband Guard security to protect critical device partitions
- 🖧 **BBRv3**: Improved TCP congestion control
- ⚡️ **TMPFS XATTR / POSIX ACL**: Extended TMPFS support for meta modules and Mountify
- </> **Unicode Bypass Fix**: Prevent path traversal and other detections using non-printable Unicode codepoints
- 🖥️ **Droidspaces Support**: Support Portable Linux containers to run full Linux environments.
- 🔃 **NTSync**: Provide high-performance, low-latency synchronization primitives compatible with the Windows NT kernel API

---

## 🔗 Additional Resources

- 🩹 [Kernel Patches](https://github.com/WildKernels/kernel_patches)
- ⚡ [Kernel Flasher](https://github.com/fatalcoder524/KernelFlasher)

---

## 📋 Installation Instructions

- Grab the latest **.tar** from the Releases page of whichever branch you chose.
- Flash using [Kernel Flasher](https://github.com/fatalcoder524/KernelFlasher) (recommended), or your preferred method (Odin/Heimdall with the repacked `boot.img`).
- Reboot, then install the matching KernelSU / KernelSU Next manager APK.

---

## 🛠️ Building From Source

For devs / advanced users who want to compile the kernel themselves instead of using the prebuilt release.

### 1. Install build dependencies

*(Ubuntu/Debian — matches the CI environment)*

```bash
sudo apt-get update
sudo apt-get install -y bc bison build-essential ccache ca-certificates clang curl flex \
  gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi git libelf-dev libssl-dev lld llvm make \
  python3 rsync unzip wget zip zstd lz4
pip3 install telethon
```

### 2. Clone the branch you want to build

```bash
# KernelSU Next + SUSFS
git clone --branch kernelsunext https://github.com/poqdavid/android_kernel_samsung_sma155f.git
cd android_kernel_samsung_sma155f

# --- OR ---

# KernelSU (upstream) + SUSFS
git clone --branch kernelsu https://github.com/poqdavid/android_kernel_samsung_sma155f.git
cd android_kernel_samsung_sma155f
```

### 3. Make the scripts executable

```bash
chmod +x build.sh
chmod +x scripts/repack
```

### 4. Run the build

```bash
# KernelSU Next
./build.sh --ksun -j$(nproc)

# KernelSU (upstream)
./build.sh --ksu -j$(nproc)
```

<details>
<summary>⚙️ Useful <code>build.sh</code> flags</summary>

| Flag | Description |
|------|-------------|
| `--kernel-dir DIR` | Path to kernel source (default: auto-detected `kernel-*` folder) |
| `--out-dir DIR` | Output directory for build artifacts |
| `--ksu` / `--ksun` | Pick the root backend (mutually exclusive) |
| `--no-clean` | Skip the clean step |
| `--no-patch` | Skip patching / KernelSU setup |
| `--no-susfs` | Skip SUSFS config & patches |
| `--build-only` | Skip config & patch steps; just run the build |
| `--clean` | Only run the clean step |
| `--jobs N`, `-j N` | Number of parallel build jobs |
| `--verbose` | Print extra debug info |
| `--help`, `-h` | Show all options |

</details>

The script auto-detects your kernel/Android version, applies the Samsung/security config tweaks, BBG, BBRv3, SUSFS, and all the optimization patches before invoking the actual kernel build.

### 5. Repack the built Image into a flashable `boot.img`

```bash
mkdir -p repackfiles
cp /path/to/your/stock_boot.img repackfiles/boot.img

# Repack — use the SAME variant flag you built with
./scripts/repack --ksun   # or --ksu
```

This unpacks your stock `boot.img`, swaps in the freshly built kernel `Image`, re-signs it with a generated AVB key, and packages the result.

**Output:** `release/<kernelsu|kernelsunext>_<ksu_version>_susfs_<susfs_version>_A155FXXS7CYG4_A15_GKI.tar` containing `boot.img` (and `vbmeta.img.lz4` if present).

> ℹ️ `lz4` and `python3` must be installed for this step — `magiskboot`, `ksud`, and `avbtool` are already bundled in `scripts/bin`.

### 6. Flash

Follow the **[📋 Installation Instructions](#-installation-instructions)** above to flash your freshly built `boot.img`.

<details>
<summary>☁️ Building in the cloud instead (GitHub Actions)</summary>

Both branches ship a ready-made workflow (`.github/workflows/buildksun.yml` / `buildksu.yml`) that does the whole thing above on GitHub's runners — no local toolchain needed:

1. Fork the repo (on the branch you want).
2. Push a tag, or trigger it manually via **Actions → Run workflow** (`workflow_dispatch`).
3. It installs deps, runs `build.sh` + `scripts/repack`, uploads the `.tar` as a build artifact, and creates a GitHub Release automatically on tagged pushes.

Discord/Telegram build notifications are optional — they only fire if you add the relevant secrets (`TELEGRAM_BOT_TOKEN`, etc.) or a `.discord_webhook` file.

</details>

---
<div align="center">
  
🙏 Special thanks to the open-source community for their contributions!

---
<div align="left">

## 💬 Support

If you encounter any issues or need help, feel free to:
- 🐛 Open an issue in this repository
- 💬 Reach out to me directly

---

## ⚠️ Disclaimer

Flashing this kernel will void your warranty, and there is always a risk of bricking your device. Please make sure to:
- 💾 Back up your data
- 🧠 Understand the risks before proceeding

**🚨 Proceed at your own risk!**

---

<div align="center">

## 📱 Contacts

[![Telegram](https://img.shields.io/badge/Telegram-poqdavid-blue?logo=telegram)](https://t.me/poqdavid)

</div>

---
<div align="center">

## 🌟 Special Thanks

**These amazing people help make this project possible! ❤️**

| 🔧 **Project** | 👨‍💻 **Developer** | 🔗 **Link** |
|:---------------:|:----------------:|:-----------:|
| **KernelSU** | tiann | [![GitHub](https://img.shields.io/badge/GitHub-tiann-blue?style=flat-square&logo=github)](https://github.com/tiann/KernelSU) |
| **KernelSU-Next** | rifsxd | [![GitHub](https://img.shields.io/badge/GitHub-rifsxd-blue?style=flat-square&logo=github)](https://github.com/KernelSU-Next/KernelSU-Next) |
| **Magic-KSU** | 5ec1cff | [![GitHub](https://img.shields.io/badge/GitHub-5ec1cff-blue?style=flat-square&logo=github)](https://github.com/5ec1cff/KernelSU) |
| **SUSFS** | simonpunk | [![GitLab](https://img.shields.io/badge/GitLab-simonpunk-orange?style=flat-square&logo=gitlab)](https://gitlab.com/simonpunk/susfs4ksu.git) |
| **SUSFS Module** | sidex15 | [![GitHub](https://img.shields.io/badge/GitHub-sidex15-blue?style=flat-square&logo=github)](https://github.com/sidex15) |
| **ReeViiS69** | ReeViiS69 | [![GitHub](https://img.shields.io/badge/GitHub-ReeViiS69-blue?style=flat-square&logo=github)](https://github.com/ReeViiS69) |
| **fei-ke** | fei-ke | [![GitHub](https://img.shields.io/badge/GitHub-fei--ke-blue?style=flat-square&logo=github)](https://github.com/fei-ke/android_kernel_samsung_sm8550.git) |
| **pershoot** | pershoot | [![GitHub](https://img.shields.io/badge/GitHub-pershoot-blue?style=flat-square&logo=github)](https://github.com/pershoot) |
| **jimsterino98** | jimsterino98 | [![GitHub](https://img.shields.io/badge/GitHub-jimsterino98-blue?style=flat-square&logo=github)](https://github.com/jimsterino98) |
| **Baseband Guard** | vc-teahouse | [![GitHub](https://img.shields.io/badge/GitHub-vc--teahouse-blue?style=flat-square&logo=github)](https://github.com/vc-teahouse/Baseband-guard.git) |
| **Droidspaces** | ravindu644 | [![GitHub](https://img.shields.io/badge/GitHub-ravindu644-blue?style=flat-square&logo=github)](https://github.com/ravindu644/Droidspaces-OSS.git) |

*If you have contributed and are not listed here, please remind me!* 🙏

---
<div align="center">

## 💝 Donations

Any and all donations are appreciated!

<br/>**BTC Legacy:** 1Q2JQG3iCLZPT2iJfDLow1oQVGKmxheoAh
<br/>**BTC Segwit:** bc1q8gurls0wjkfe43ygmrqmu2pzmyjetnrvgws9sr
<br/>**BCH:** qrks52smlqw7d8700d77uqvmve03d4knzvd2vghaqz
<br/>**ETH:** 0x7218779242a8425879B09969431c20F5eC1a192D
