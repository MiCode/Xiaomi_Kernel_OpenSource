#
# Copyright (C) 2021 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# driver code for VDEC-FMT

obj-$(CONFIG_MTK_VDEC_FMT) += vdec-fmt.o

vdec-fmt-$(CONFIG_MTK_VDEC_FMT) += vdec_fmt_driver.o \
			vdec_fmt_pm.o \
			vdec_fmt_dmabuf.o \
			vdec_fmt_sync.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/ \
			 -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/ \
			 -I$(srctree)/drivers/misc/mediatek/include/ \
			 -I$(srctree)/drivers/misc/mediatek/iommu/ \
			 -I$(srctree)/drivers/iommu/ \
