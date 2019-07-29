# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.

# in-tree kernel modules installed to vendor
board_vendor_kernel_modules :=

BOARD_VENDOR_KERNEL_MODULES += $(board_vendor_kernel_modules)
$(board_vendor_kernel_modules): $(KERNEL_ZIMAGE_OUT);

# in-tree kernel modules installed to ramdisk
board_ramdisk_kernel_modules :=

BOARD_RAMDISK_KERNEL_MODULES += $(board_ramdisk_kernel_modules)
$(board_ramdisk_kernel_modules): $(KERNEL_ZIMAGE_OUT);
