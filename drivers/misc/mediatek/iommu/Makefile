#
# Copyright (C) 2019 MediaTek Inc.
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

ifeq ($(CONFIG_MTK_AEE_FEATURE),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public
ccflags-y += -I$(srctree)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/GP
endif

obj-$(CONFIG_MTK_IOMMU_MISC_SEC) += m4u_secure.o
obj-$(CONFIG_MTK_IOMMU_MISC_DBG) += m4u_debug.o
