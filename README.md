# Xiaomi Kernel OpenSource

This repository is a lightweight catalog for Xiaomi and Redmi kernel sources, organized by device codename and Android release. It is meant to be a fast-reference index for developers, kernel researchers, and custom ROM builders who need a stable branch map and build guidance.

## Repository purpose

- Track Android/Linux kernel sources by device codename
- Map devices to branch names and source tags
- Provide a quick start for inspecting or building device kernels
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

## Common device branches

| Device | Branch | Android | Notes |
| --- | --- | --- | --- |
| Redmi K40 | `alioth-r-oss` | Android R | Snapdragon-based branch |
| Redmi Note 8 Pro | `begonia-r-oss` | Android R | MediaTek device branch |
| Mi A2 | `jasmine-q-oss` | Android Q | Snapdragon 660 |
| Redmi Note 7 Pro | `violet-q-oss` | Android Q | Snapdragon 675 |
| Xiaomi 12 Lite | `taoyao-s-oss` | Android S | Qualcomm platform |
| Redmi Note 11 Pro | `pissarro-r-oss` | Android R | MediaTek variant |
| Mi 11 Lite | `courbet-r-oss` | Android R | Qualcomm branch |
| Xiaomi Pad 6 | `pipa-t-oss` | Android T | Qualcomm branch |
| Redmi 13C / POCO C65 | `gale-s-oss` | Android T | MediaTek / newer platform |
| Xiaomi 15 / Xiaomi 15 Pro | `dada-v-oss` | Android V | newer Qualcomm platform |

## Branch catalog

The project keeps a device-by-device branch map for Xiaomi and Redmi kernel sources. The entries below are representative examples from the catalog and can be expanded over time as new branches are added or corrected.

| Branch | Device | Android version | Base tag | Link |
| :-: | :-: | :-: | :-: | :-: |
| `alioth-r-oss` | Mi 10S, Redmi K40 | Android R | `LA.UM.9.12.r1-08000-SMxx50.0` | [alioth-r-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/alioth-r-oss) |
| `andromeda-p-oss` | Mi MIX 3 5G | Android P | `LA.UM.7.1.r1-13000-sm8150.0` | [andromeda-p-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/andromeda-p-oss) |
| `apollo-q-oss` | Redmi K30S Ultra | Android Q | `LA.UM.8.12.r1-10600-sm8250.0` | [apollo-q-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/apollo-q-oss) |
| `begonia-r-oss` | Redmi Note 8 Pro | Android R | `MTK` | [begonia-r-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/begonia-r-oss) |
| `cmi-r-oss` | Redmi K30S Ultra, Mi 10 Ultra, Mi 10 Pro, Redmi K30 Pro, Mi 10 | Android R | `LA.UM.9.12.r1-08000-SMxx50.0` | [cmi-r-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/cmi-r-oss) |
| `daisy-q-oss` | Mi A2 Lite | Android Q | `LA.UM.8.6.r1-02600-89xx.0` | [daisy-q-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/daisy-q-oss) |
| `jasmine-q-oss` | Mi A2 | Android Q | `LA.UM.8.2.r1-04300-sdm660.0` | [jasmine-q-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/jasmine-q-oss) |
| `pipa-t-oss` | Xiaomi Pad 6 | Android T | `LA.UM.9.12.r1-13300-SMxx50.QSSI12.0-1` | [pipa-t-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/pipa-t-oss) |
| `taoyao-s-oss` | Xiaomi 12 Lite | Android S | `LA.UM.9.14.r1-18300.05-LAHAINA.QSSI12.0-1` | [taoyao-s-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/taoyao-s-oss) |
| `violet-q-oss` | Redmi Note 7 Pro | Android Q | `LA.UM.8.9.r1-04400-SM6xx.0-1` | [violet-q-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/violet-q-oss) |
| `gale-s-oss` | Redmi 13C / POCO C65 | Android T | `t-alps-release-s0.mp1.tc8sp-cs2-V1.31` | [gale-s-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/gale-s-oss) |
| `dada-v-oss` | Xiaomi 15 / Xiaomi 15 Pro | Android V | `qcom-LA.VENDOR.15.4.0.r1-08900-r1.0.r1_00022.1` | [dada-v-oss](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/dada-v-oss) |

> The full catalog can be expanded with additional device entries as new Xiaomi kernel trees are added or corrected over time.

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

## Contribution guidance

- Keep changes device-specific and clearly scoped.
- Preserve the naming structure used by the upstream Xiaomi kernel repository.
- Prefer small documentation or metadata updates when editing the branch index.
- Cite the correct branch link or source tag when available.

This merged README keeps the original branch-index structure, documents the project purpose, and preserves the contributor/build guidance added in the PR.
