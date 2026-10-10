# Xiaomi Kernel OpenSource

This repository is a lightweight catalog for Xiaomi and Redmi kernel sources, organized by device codename and Android release. It is intended as a quick reference for developers, kernel researchers, and custom ROM builders who need a stable branch map and build guidance.

## Repository purpose

- Track Android/Linux kernel sources by device codename
- Map device names to branch names and source tags
- Provide a quick-start for inspecting or building device kernels
- Serve as a clean entry point for kernel research and device-specific maintenance

## Quick start

```bash
git clone https://github.com/sketchie-durk/Xiaomi-Kernel-OpenSource-1.1.git
cd Xiaomi-Kernel-OpenSource-1.1
# List branches available in this repo
git branch -a
# Switch to a target device branch, for example:
git checkout alioth-r-oss
```

## Common device branch examples

| Device | Branch | Android | Notes |
| --- | --- | --- | --- |
| Redmi K40 | `alioth-r-oss` | Android R | Snapdragon-based branch |
| Redmi Note 8 Pro | `begonia-r-oss` | Android R | MTK device branch |
| Mi A2 | `jasmine-q-oss` | Android Q | Snapdragon 660 |
| Redmi Note 7 Pro | `violet-q-oss` | Android Q | Snapdragon 675 |
| Xiaomi 12 Lite | `taoyao-s-oss` | Android S | Qualcomm platform |

## Build note

This repository mainly acts as a branch index and source catalog. Actual kernel builds are device-specific and usually require:

- the correct device-specific kernel source branch
- a matching toolchain for the target SoC
- the required defconfig and device tree files
- vendor or board-specific patches for the platform

## Documentation

- `docs/BUILDING.md` — build workflow and common troubleshooting
- `CONTRIBUTING.md` — contribution and branch naming guidance
- `scripts/branch-info.sh` — quick branch inspection helper

## License

This repository is distributed under the GNU General Public License v2.0 (`GPL-2.0`). Individual device kernel branches may include vendor-specific modifications and follow their own upstream licensing terms.

## Notes

The repository is intentionally organized as a device index rather than a single monolithic kernel source tree. For each device, the relevant branch contains the actual kernel source tree and vendor-specific configuration required for the target platform.
