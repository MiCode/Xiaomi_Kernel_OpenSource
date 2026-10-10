# Building Xiaomi Device Kernels

This document covers the typical workflow for working with a Xiaomi device branch in this repository.

## Prerequisites

- Git
- A Linux or WSL-based build environment
- A toolchain that matches the target SoC and kernel architecture
- Device-specific kernel source or board support files
- A matching defconfig for the selected device branch

## Typical workflow

```bash
git clone https://github.com/sketchie-durk/Xiaomi-Kernel-OpenSource-1.1.git
cd Xiaomi-Kernel-OpenSource-1.1
git checkout alioth-r-oss
```

Then continue with the device-specific kernel build process:

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make vendor/alioth_defconfig
make -j$(nproc)
```

## Common notes

- Qualcomm and MediaTek branches often use different defconfigs and vendor trees.
- Some kernels require vendor-supplied patches or proprietary module sources.
- Older Xiaomi kernel trees may require legacy GCC toolchains and older build scripts.

## Troubleshooting

- **Wrong defconfig**: confirm the exact board or device codename before building.
- **Toolchain mismatch**: use the toolchain version expected by the selected kernel branch.
- **Missing config**: verify the device branch and Android release match the target hardware.
- **Build errors**: check if the branch relies on vendor code or additional generated files.

## Recommendation

When working on a new device branch, confirm the following before building:

1. the branch name
2. the Android base version
3. the SoC family (Qualcomm / MediaTek / other)
4. the required toolchain and defconfig
