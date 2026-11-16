<!-- SPDX-License-Identifier: GPL-2.0-only -->

# GPLv2 module compliance audit — rothko (Redmi K70 Ultra / Xiaomi 14T Pro)

This directory adds reproducible evidence tooling for
[MiCode/Xiaomi_Kernel_OpenSource issue #41191](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/issues/41191).

It does **not** publish the missing vendor source, and it does not claim to.
Only Xiaomi and MediaTek can release the MT6989 `kernel_device_modules-6.1`
tree, the Android 16 kernel and the rothko device trees. What a fork can do is
make the request checkable one module at a time.

## Verified publication state (2026-11-15)

| Repository | Branch | Commit | Content |
| --- | --- | --- | --- |
| `MiCode/Xiaomi_Kernel_OpenSource` | `bsp-rothko-u-oss` | `197c453d27da88f3b7b8fed7323ee6d15fc02066` | Android U kernel, `alps-mp-u0.mp1.tc8sp3-V1` |
| `MiCode/MTK_kernel_modules` | `bsp-rothko-u-oss` | `a875701d71d29cfac0a9456c1e9c223d44f53d68` | `connectivity`, `fpsgo_cus`, `gpu`, `hbt_driver` |
| either repository | `bsp-rothko-b-oss` | — | absent |

Re-check before relying on this table:

```bash
git ls-remote https://github.com/MiCode/Xiaomi_Kernel_OpenSource 'refs/heads/*rothko*'
git ls-remote https://github.com/MiCode/MTK_kernel_modules 'refs/heads/*rothko*'
```

The firmware cited in the issue reports
`vermagic=6.1.138-android14-11-ga418eeb69cb6`. Nothing published today matches it.

## What the tool reports

For every `.ko`, `.ko.xz`, `.ko.gz` or `.ko.zst` under the given directories:

- module name and file path;
- partition inferred from the path (`vendor_dlkm`, `odm_dlkm`, or `unknown`);
- the exact `license=` string from the module's own `.modinfo`, and its class;
- `vermagic`, `srcversion`, `depends`, size and SHA-256 of the file as shipped;
- whether a matching module target exists in the supplied published trees.

It then renders a Markdown report, a CSV appendix and a JSON evidence file.
It does **not** establish the availability of kernel source, device trees or
build configuration, and it does not adjudicate legal obligations.

## Requirements

- Python 3.9 or newer, standard library only.
- A `zstd` binary on `PATH` only if the capture contains `.ko.zst` modules.
- No build toolchain, no root on the analysis machine.

## Capturing modules from the device

`adb pull` works when the partitions are readable:

```bash
mkdir -p capture
adb pull /vendor_dlkm/lib/modules capture/vendor_dlkm/lib/modules
adb pull /odm_dlkm/lib/modules capture/odm_dlkm/lib/modules
```

If `adb` cannot read them directly, pull an archive through a root shell:

```bash
adb shell "su -c 'tar -C / -cf - vendor_dlkm/lib/modules odm_dlkm/lib/modules'" > capture/modules.tar
tar -C capture -xf capture/modules.tar
```

Keep the capture directory together with the report: the report records each
module's SHA-256, so the evidence can be re-verified later.

## Quick start

```bash
# 1. Inventory the modules shipped in the firmware.
python3 tools/gpl-audit/rothko_gpl_audit.py scan \
    capture/vendor_dlkm capture/odm_dlkm \
    --out inventory.json

# 2. Inventory the module targets in each published tree.
git clone --depth 1 --branch bsp-rothko-u-oss \
    https://github.com/MiCode/Xiaomi_Kernel_OpenSource.git published-kernel
git clone --depth 1 --branch bsp-rothko-u-oss \
    https://github.com/MiCode/MTK_kernel_modules.git published-modules

python3 tools/gpl-audit/rothko_gpl_audit.py inventory published-kernel \
    --branch bsp-rothko-u-oss --out published-kernel.json
python3 tools/gpl-audit/rothko_gpl_audit.py inventory published-modules \
    --branch bsp-rothko-u-oss --out published-modules.json

# 3. Diff and render.
python3 tools/gpl-audit/rothko_gpl_audit.py audit \
    --scan inventory.json \
    --published published-kernel.json --published published-modules.json \
    --device "Redmi K70 Ultra / Xiaomi 14T Pro (rothko)" \
    --firmware "OS3.0.308.0.WNNCNXM" \
    --kernel-commit ga418eeb69cb6 \
    --outdir reports
```

`reports/` then contains `report.md` (postable summary), `audit.csv` (one row
per module) and `audit.json` (full evidence, including input hashes).

## Commands

| Command | Purpose | Output |
| --- | --- | --- |
| `scan DIR...` | Read module files and their `.modinfo` | `inventory.json`, schema `rothko-gpl-audit/scan/v1` |
| `inventory TREE` | Collect `obj-m` targets from a source tree | `published.json`, schema `rothko-gpl-audit/published/v1` |
| `audit --scan ... --published ...` | Diff, classify and render | `audit.json`, `audit.csv`, `report.md` |

### Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Every GPL-declared module has a matching published target, and the scan was complete |
| `2` | At least one GPL-declared module has no matching published target |
| `3` | Usage or input error, or an incomplete scan without `--allow-scan-errors` |

A scan that skipped unreadable files cannot certify coverage; `audit` exits `3`
in that case unless `--allow-scan-errors` is passed explicitly.

## Methodology and limits

**License extraction.** The tool parses the ELF section table (ELF32/ELF64,
little- and big-endian) and reads the `.modinfo` key/value pairs, which is the
same data `modinfo` prints. The repository's exact license string is preserved.
A module is classified `gpl` when its string contains `GPL` (case-insensitive),
which covers `GPL`, `GPL v2`, `Dual BSD/GPL` and similar declarations. Missing
or empty licenses are classified `unknown` and never counted as covered GPL.

**Published module inventory.** `inventory` walks `Makefile`/`Kbuild` files and
reads `obj-m` and `obj-$(CONFIG_...)` rules, including `+=`, `:=`, `=` and
continued lines. `obj-y` targets are excluded because they are built in. A
conditional target counts as present but is marked `conditional` and records its
config symbol. Other Kbuild conditionals (`ifeq`, `ifdef`) are not evaluated: a
target declared there is treated as unconditional. `tools`, `samples`,
`Documentation`, `rust` and hidden directories are skipped by default (override
with `--exclude`).

**Matching.** Module filenames are compared with Kbuild targets after dropping
the compression suffix and normalizing case and `_`/`-` (so `mi_t1_gpio` matches
`mi-t1-gpio`). This is a name-based comparison: it cannot prove that a published
tree corresponds to the shipped binary, and a renamed or relocated module can
appear as a false gap. Treat an uncovered entry as a candidate that needs
inspection, not as proof of a violation.

**Snapshot.** The published trees are a snapshot of one branch at one commit;
the report records only what was supplied to `inventory`. `--now` fixes the
report timestamp so that two runs over identical inputs and trees produce
byte-identical `audit.json`, `audit.csv` and `report.md`.

## Tests

```bash
python -m unittest discover -s tools/gpl-audit/tests -v
```

The suite builds minimal ELF objects at test time (ELF32, ELF64, gzip) and a
synthetic Kbuild tree. No vendor binaries and no firmware blobs are included in
this repository.

## License

GPL-2.0-only, matching the kernel tree. Every source file carries an SPDX
header.
