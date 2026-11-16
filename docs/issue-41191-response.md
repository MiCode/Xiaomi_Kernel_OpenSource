<!-- SPDX-License-Identifier: GPL-2.0-only -->

# Draft response for issue #41191

Post this from the fork maintainer account after generating the reports with
[`tools/gpl-audit`](../tools/gpl-audit/README.md). Attach `reports/report.md` and
`reports/audit.csv` to the comment. If the audit exited `3`, the scan was
incomplete: fix the capture first rather than quoting coverage counts.

The text deliberately states no module counts and no legal conclusion. Both
must come from the attached report, not from this template.

## English

I re-checked the publication state on 2026-11-15:

- `Xiaomi_Kernel_OpenSource` has only `bsp-rothko-u-oss`
  (`197c453d27da88f3b7b8fed7323ee6d15fc02066`, Android U,
  `alps-mp-u0.mp1.tc8sp3-V1`).
- `MTK_kernel_modules` `bsp-rothko-u-oss`
  (`a875701d71d29cfac0a9456c1e9c223d44f53d68`) contains only `connectivity`,
  `fpsgo_cus`, `gpu` and `hbt_driver`.
- Neither repository has a branch matching the Android 16 firmware, and neither
  has a `bsp-rothko-b-oss` branch.

To make the module part of the request checkable module by module, I added a
small audit tool on branch `rothko-a16-gpl-audit` of my fork:

https://github.com/Om-Karakoti/Xiaomi_Kernel_OpenSource/tree/rothko-a16-gpl-audit/tools/gpl-audit

It reads every `.ko` in `vendor_dlkm` and `odm_dlkm`, records the license string
and vermagic that each module declares in its own `.modinfo`, hashes the shipped
file, and diffs the list against module targets in the published trees. The
generated report and CSV are attached; the tool also records the input hashes so
the result can be reproduced.

The request is unchanged. Please publish, for the firmware that reports
`vermagic=6.1.138-android14-11-ga418eeb69cb6`:

1. kernel source corresponding to that build, in `Xiaomi_Kernel_OpenSource`;
2. the vendor module sources (`kernel_device_modules-6.1` for MT6989) in
   `MTK_kernel_modules`, including the modules listed in the report;
3. the MT6989 / rothko device tree sources and overlays;
4. the build configuration and scripts: defconfig fragments, `build.config.*`,
   Bazel/Kleaf files and module lists.

A branch named `bsp-rothko-b-oss` in both repositories, matching the Android 16
firmware, would make the published trees correspond to the distributed binaries.

## 中文

我在 2026-11-15 重新核对了公开状态：

- `Xiaomi_Kernel_OpenSource` 只有 `bsp-rothko-u-oss`
  （`197c453d27da88f3b7b8fed7323ee6d15fc02066`，Android U，
  `alps-mp-u0.mp1.tc8sp3-V1`）。
- `MTK_kernel_modules` 的 `bsp-rothko-u-oss`
  （`a875701d71d29cfac0a9456c1e9c223d44f53d68`）只有 `connectivity`、
  `fpsgo_cus`、`gpu`、`hbt_driver` 四个目录。
- 两个仓库都没有与 Android 16 固件对应的分支，也没有 `bsp-rothko-b-oss`。

为了让模块部分可以逐个核对，我在自己 fork 的 `rothko-a16-gpl-audit` 分支上
加入了一个审计工具：

https://github.com/Om-Karakoti/Xiaomi_Kernel_OpenSource/tree/rothko-a16-gpl-audit/tools/gpl-audit

它会读取 `vendor_dlkm`、`odm_dlkm` 中每个 `.ko` 自带的 `.modinfo`，记录声明的
许可证和 vermagic，对发布文件计算哈希，并与已公开代码树中的模块目标逐一比对。
生成的报告和 CSV 已附上；工具同时记录输入哈希，结果可复现。

请求不变。请为 `vermagic=6.1.138-android14-11-ga418eeb69cb6` 对应的固件公开：

1. 与该构建对应的内核源码，位于 `Xiaomi_Kernel_OpenSource`；
2. `MTK_kernel_modules` 中的 vendor 模块源码（MT6989 对应
   `kernel_device_modules-6.1`），包括报告列出的模块；
3. MT6989 / rothko 设备树源码及 overlay；
4. 构建配置和脚本：defconfig 片段、`build.config.*`、Bazel/Kleaf 文件及模块列表。

如果两个仓库都提供与 Android 16 固件对应的 `bsp-rothko-b-oss` 分支，公开源码
就能与分发的二进制对应起来。

## Closure criteria

The issue can be closed when both repositories expose a branch whose tree
contains the source for `6.1.138-android14-11-ga418eeb69cb6`, including the
vendor modules, device trees and build configuration, and the audit can be
re-run against that branch without uncovered GPL-declared modules.
