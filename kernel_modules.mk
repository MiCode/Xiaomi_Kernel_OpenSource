# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.

# in-tree kernel modules installed to vendor
BOARD_VENDOR_KERNEL_MODULES +=
$(BOARD_VENDOR_KERNEL_MODULES): $(KERNEL_ZIMAGE_OUT);

# in-tree kernel modules installed to ramdisk
BOARD_RAMDISK_KERNEL_MODULES +=
$(BOARD_RAMDISK_KERNEL_MODULES): $(KERNEL_ZIMAGE_OUT);
