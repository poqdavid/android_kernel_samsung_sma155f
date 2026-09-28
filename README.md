<div align="center">

# Samsung A15 4G Kernel

**SM-A155F** · MediaTek MT6789 (Helio G99) · Linux 5.10 (android12-5.10 GKI)

[![KernelSU](https://img.shields.io/badge/KernelSU-Supported-green)](https://kernelsu.org/)
[![KernelSU Next](https://img.shields.io/badge/KernelSU_Next-Supported-green)](https://github.com/KernelSU-Next/KernelSU-Next)
[![ReSukiSU](https://img.shields.io/badge/ReSukiSU-Supported-green)](https://github.com/ReSukiSU/ReSukiSU)
[![SUSFS](https://img.shields.io/badge/SUSFS-Integrated-orange)](https://gitlab.com/simonpunk/susfs4ksu)
[![Telegram](https://img.shields.io/badge/Join-Build_Notification-blue?logo=telegram&style=flat-square)](https://t.me/sma155fkernelbuilds)

## ⚠️ Your warranty is no longer valid!

I am **not responsible** for bricked devices, damaged hardware, or any issues that arise from using this kernel.

**Please** do thorough research and fully understand the features included in this kernel before flashing it!

By flashing this kernel, **YOU** are choosing to make these modifications. If something goes wrong, **do not blame me**!

---

### 🚨 Proceed at your own risk!

---

## 🔧 Available Kernels

| Variant | Branch | Release tags start with | Status |
|---------|--------|-------------------------|--------|
| 🔐 **KernelSU** + SUSFS | [`kernelsu`](https://github.com/poqdavid/android_kernel_samsung_sma155f/tree/kernelsu) | `KSU_` | ✅ Active |
| 🔐 **KernelSU Next** + SUSFS | [`kernelsunext`](https://github.com/poqdavid/android_kernel_samsung_sma155f/tree/kernelsunext) | `KSUN_` | ✅ Active |
| 🔐 **ReSukiSU** + SUSFS | [`resukisu`](https://github.com/poqdavid/android_kernel_samsung_sma155f/tree/resukisu) | `RESUKISU_` | ✅ Active |
| ⚡ **Stock optimized** (no root) | [`main`](https://github.com/poqdavid/android_kernel_samsung_sma155f/tree/main) | `STOCK_OPTIMIZED_` | ✅ Active |

</div>

---

## ✨ Features

- 🔐 **KernelSU / KernelSU Next / ReSukiSU**: Kernel-based root solutions for Android GKI devices that grant root to userspace apps directly from kernel space (one per branch; `main` ships without root)
- 🥷 **SUSFS**: An addon root hiding kernel patches and userspace module for KernelSU (ReSukiSU ships its own built-in SUSFS hooks)
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

## ✅ Requirements

- 📱 **Samsung Galaxy A15 4G (SM-A155F) only.** Do not flash this on the A15 5G (SM-A156x) or any other model.
- 🔓 **Unlocked bootloader** (OEM unlocking enabled).
- 🧾 **Matching firmware:** current releases are built for **A155FXXS7CYG1 / A155FXXS7CYG4**. The firmware is part of every release name, so compare it with your build number before flashing.
- 💾 **A copy of your stock `boot.img`** (from your firmware's AP file) so you can always go back.

---

## 📋 Installation Instructions

1. Grab the **.tar** for your variant from the [Releases](https://github.com/poqdavid/android_kernel_samsung_sma155f/releases) page. Releases for all branches are listed together, so use the tag prefix from the [table above](#-available-kernels) to pick the right one.
2. Flash it:
   - **Not rooted yet:** flash the `.tar` in Odin's **AP** slot (or extract `boot.img` and flash it with Heimdall).
   - **Already rooted:** extract `boot.img` from the `.tar` and flash it with [Kernel Flasher](https://github.com/fatalcoder524/KernelFlasher) (recommended).
3. Reboot, then install the matching manager app (KernelSU, KernelSU Next or ReSukiSU).

> ↩️ **Going back:** flash your stock `boot.img` the same way to return to the stock kernel.

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
pip3 install telethon   # optional: only needed for the Telegram release bot
```

### 2. Clone the branch you want to build

```bash
# Pick ONE: kernelsu | kernelsunext | resukisu | main (stock optimized, no root)
git clone --branch resukisu https://github.com/poqdavid/android_kernel_samsung_sma155f.git
cd android_kernel_samsung_sma155f
```

### 3. Make the scripts executable

```bash
chmod +x build.sh
chmod +x scripts/repack
```

### 4. Run the build

```bash
./build.sh --ksu -j$(nproc)        # KernelSU
./build.sh --ksun -j$(nproc)       # KernelSU Next
./build.sh --resukisu -j$(nproc)   # ReSukiSU
./build.sh -j$(nproc)              # no variant flag = stock optimized (no root)
```

<details>
<summary>⚙️ Useful <code>build.sh</code> flags</summary>

| Flag | Description |
|------|-------------|
| `--kernel-dir DIR` | Path to kernel source (default: auto-detected `kernel-*` folder) |
| `--out-dir DIR` | Output directory for build artifacts |
| `--ksu` / `--ksun` / `--resukisu` | Pick the root backend (mutually exclusive; omit for a non-root stock optimized build) |
| `--no-clean` | Skip the clean step |
| `--no-patch` | Skip patching / KernelSU setup |
| `--no-susfs` | Skip SUSFS config & patches |
| `--build-only` | Skip config & patch steps; just run the build |
| `--clean` | Only run the clean step |
| `--jobs N`, `-j N` | Number of parallel build jobs |
| `--verbose` | Print extra debug info |
| `--help`, `-h` | Show all options |

> ℹ️ `--ksu` and `--resukisu` both check out into `./KernelSU`, so don't use `--no-clean` when switching between them.

</details>

The script auto-detects your kernel/Android version, applies the Samsung/security config tweaks, BBG, BBRv3, SUSFS, and all the optimization patches before invoking the actual kernel build.

### 5. Repack the built Image into a flashable `boot.img`

The repo already ships the stock boot image the releases are built against (`repackfiles/boot.img.lz4`), and `scripts/repack` decompresses it automatically. To repack against a different firmware, replace `repackfiles/boot.img.lz4` with your own stock boot image (or delete it and put a plain `boot.img` in `repackfiles/`; a plain `boot.img` gets overwritten while `boot.img.lz4` is present).

```bash
# Use the SAME variant flag you built with (--ksu / --ksun / --resukisu, or none for stock)
./scripts/repack --resukisu

# Add --vbm to also include vbmeta.img.lz4 in the .tar
./scripts/repack --resukisu --vbm
```

This unpacks the stock `boot.img`, swaps in the freshly built kernel `Image`, re-signs it with a generated AVB key, and packages the result.

**Output:** `release/<kernelsu|kernelsunext|resukisu>_<ksu_version>_susfs_<susfs_version>_A155FXXS7CYG4_A15_GKI.tar` (or `release/stock_optimized_<kernel_version>_A155FXXS7CYG4_A15_GKI.tar`) containing `boot.img`, plus `vbmeta.img.lz4` when `--vbm` is used.

> ℹ️ `lz4` and `python3` must be installed for this step — `magiskboot`, `ksud`, and `avbtool` are already bundled in `scripts/bin`.

### 6. Flash

Follow the **[📋 Installation Instructions](#-installation-instructions)** above to flash your freshly built `boot.img`.

<details>
<summary>☁️ Building in the cloud instead (GitHub Actions)</summary>

Every branch ships the same workflow, `.github/workflows/build.yml`. The `VARIANT` value at the top of the file (`stock` on `main`, `ksu`, `ksun` or `resukisu`) decides what it builds, so it does everything above on GitHub's runners with no local toolchain needed:

1. Fork the repo (on the branch you want).
2. Push a tag, or trigger it manually via **Actions → Run workflow** (`workflow_dispatch`).
3. It installs deps, runs `build.sh` + `scripts/repack`, uploads the `.tar` as a build artifact (kept for 14 days), and creates a GitHub Release automatically on tagged pushes.

Telegram release notifications need the `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID`, `TELEGRAM_API_ID` and `TELEGRAM_API_HASH` secrets (`TELEGRAM_MESSAGE_THREAD_ID` is optional). Without them the final Telegram step fails, but the Release has already been created by then. Discord notifications are for local builds only, via a `.discord_webhook` file.

</details>

---

## 💬 Support

If you encounter any issues or need help, feel free to:
- 🐛 [Open an issue](https://github.com/poqdavid/android_kernel_samsung_sma155f/issues) in this repository
- 💬 Reach out to me on [Telegram](https://t.me/poqdavid)

When reporting a problem, please include the **release tag** you flashed, your **firmware build number**, and a log (`dmesg` / `logcat`) if you can get one.

---

## ⚠️ Disclaimer

Flashing this kernel will void your warranty, and there is always a risk of bricking your device. Please make sure to:
- 💾 Back up your data
- 🧠 Understand the risks before proceeding

**🚨 Proceed at your own risk!**

---

## 📄 License

The kernel source is licensed under **GPL-2.0** (see [`COPYING`](COPYING) and [`LICENSE`](LICENSE)). Build and repack scripts carry their own SPDX license headers.

---

<div align="center">

## 📱 Contacts

[![Telegram](https://img.shields.io/badge/Telegram-poqdavid-blue?logo=telegram)](https://t.me/poqdavid)

---

## 🌟 Special Thanks

**These amazing people help make this project possible! ❤️**

| 🔧 **Project** | 👨‍💻 **Developer** | 🔗 **Link** |
|:---------------:|:----------------:|:-----------:|
| **KernelSU** | tiann | [![GitHub](https://img.shields.io/badge/GitHub-tiann-blue?style=flat-square&logo=github)](https://github.com/tiann/KernelSU) |
| **KernelSU-Next** | rifsxd | [![GitHub](https://img.shields.io/badge/GitHub-rifsxd-blue?style=flat-square&logo=github)](https://github.com/KernelSU-Next/KernelSU-Next) |
| **ReSukiSU** | ReSukiSU | [![GitHub](https://img.shields.io/badge/GitHub-ReSukiSU-blue?style=flat-square&logo=github)](https://github.com/ReSukiSU/ReSukiSU) |
| **Magic-KSU** | 5ec1cff | [![GitHub](https://img.shields.io/badge/GitHub-5ec1cff-blue?style=flat-square&logo=github)](https://github.com/5ec1cff/KernelSU) |
| **SUSFS** | simonpunk | [![GitLab](https://img.shields.io/badge/GitLab-simonpunk-orange?style=flat-square&logo=gitlab)](https://gitlab.com/simonpunk/susfs4ksu.git) |
| **SUSFS Module** | sidex15 | [![GitHub](https://img.shields.io/badge/GitHub-sidex15-blue?style=flat-square&logo=github)](https://github.com/sidex15) |
| **ReeViiS69** | ReeViiS69 | [![GitHub](https://img.shields.io/badge/GitHub-ReeViiS69-blue?style=flat-square&logo=github)](https://github.com/ReeViiS69) |
| **fei-ke** | fei-ke | [![GitHub](https://img.shields.io/badge/GitHub-fei--ke-blue?style=flat-square&logo=github)](https://github.com/fei-ke/android_kernel_samsung_sm8550.git) |
| **pershoot** | pershoot | [![GitHub](https://img.shields.io/badge/GitHub-pershoot-blue?style=flat-square&logo=github)](https://github.com/pershoot) |
| **jimsterino98** | jimsterino98 | [![GitHub](https://img.shields.io/badge/GitHub-jimsterino98-blue?style=flat-square&logo=github)](https://github.com/jimsterino98) |
| **Baseband Guard** | vc-teahouse | [![GitHub](https://img.shields.io/badge/GitHub-vc--teahouse-blue?style=flat-square&logo=github)](https://github.com/vc-teahouse/Baseband-guard.git) |
| **Droidspaces** | ravindu644 | [![GitHub](https://img.shields.io/badge/GitHub-ravindu644-blue?style=flat-square&logo=github)](https://github.com/ravindu644/Droidspaces-OSS.git) |
| **KKdemergencia** | KKdemergencia | [![GitHub](https://img.shields.io/badge/GitHub-KKdemergencia-blue?style=flat-square&logo=github)](https://github.com/kkdemergencia) |

🙏 Special thanks to the open-source community for their contributions!

*If you have contributed and are not listed here, please remind me!* 🙏

---

## 💝 Donations

Any and all donations are appreciated!

<br/>**BTC Legacy:** 1Q2JQG3iCLZPT2iJfDLow1oQVGKmxheoAh
<br/>**BTC Segwit:** bc1q8gurls0wjkfe43ygmrqmu2pzmyjetnrvgws9sr
<br/>**BCH:** qrks52smlqw7d8700d77uqvmve03d4knzvd2vghaqz
<br/>**ETH:** 0x7218779242a8425879B09969431c20F5eC1a192D

</div>