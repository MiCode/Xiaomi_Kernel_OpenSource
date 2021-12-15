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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/mtk

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/include/trustzone/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
endif
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
endif
ifeq ($(CONFIG_MTK_CAM_GENIEZONE_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
endif
ifeq ($(CONFIG_MTK_SVP_ON_MTEE_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u
endif
ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
  ccflags-y += -DCONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
endif
ifeq ($(CONFIG_MTK_CAM_SECURITY_SUPPORT),y)
  ccflags-y += -DCONFIG_MTK_CAM_SECURITY_SUPPORT
endif
ifeq ($(CONFIG_MTK_GZ_SUPPORT_SDSP),y)
  ccflags-y += -DCONFIG_MTK_GZ_SUPPORT_SDSP
endif
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/arch/arm/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6761" "mt6758" "mt6779" "mt6768" "mt6785" "mt6885" "mt6873" "mt6893" "mt6853" "mt6877"))
obj-y += pseudo_m4u_v2.o pseudo_m4u_debug.o
ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
ifneq (,$(filter y_n, $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)_$(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)))
obj-y += pseudo_m4u_sec.o
else ifeq ($(CONFIG_MTK_CAM_SECURITY_SUPPORT),y)
obj-y += pseudo_m4u_sec.o
else ifeq ($(CONFIG_MTK_GZ_SUPPORT_SDSP),y)
obj-y += pseudo_m4u_sec.o
endif
endif
else
obj-y += pseudo_m4u.o
obj-y += m4u_hw_$(MTK_PLATFORM).o
endif

ifeq ($(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM),y)
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.include
endif
obj-$(CONFIG_FPGA_EARLY_PORTING) += pseudo_m4u_ut.o

ifeq ($(CONFIG_MTK_CAM_GENIEZONE_SUPPORT),y)
obj-y += pseudo_m4u_gz_sec.o
endif
ifeq ($(CONFIG_MTK_SVP_ON_MTEE_SUPPORT),y)
obj-y += pseudo_m4u_gz_sec.o
endif
