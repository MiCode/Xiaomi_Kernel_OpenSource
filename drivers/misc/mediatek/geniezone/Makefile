# Copyright (C) 2018 MediaTek Inc.
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
# Makefile for the GenieZone KREE driver
#
GZ_SEC_STORAGE_UT := y

ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssmr
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/ssmr


$(info ************  drivers/trusty/mtee-kree mk ************)

#ccflags-y := -I$(srctree)/drivers/misc/mediatek/geniezone/public -Werror $(ccflags-y)

ifneq ($(CONFIG_MACH_MT6758), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/ppm_v3/src/mach/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/ppm_v3
endif

ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
  ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/include/trustzone/utils/tlutils
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/utils/tlutils
  ccflags-y += -I$(src)/$(MTK_PLATFORM)
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/public
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)
  ifneq ($(filter y m, $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
    include $(srctree)/drivers/tee/gud/Makefile.include
  else ifneq ($(filter y m, $(CONFIG_MICROTRUST_TEE_SUPPORT)),)
    include $(srctree)/drivers/tee/teei/Makefile.include
  #else ifeq ($(CONFIG_BLOWFISH_TEE_SUPPORT),y)
  # ccflags-y += -I$(srctree)/drivers/misc/tzdev
  else
  endif
else # Legacy
  ifneq ($(filter y m, $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
    ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/include/trustzone/utils/tlutils
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/utils/tlutils
    ccflags-y += -I$(src)/$(MTK_PLATFORM)
    include $(srctree)/drivers/tee/gud/Makefile.include
  endif
endif # !CONFIG_MTK_TEE_GP_SUPPORT


obj-$(CONFIG_MTK_GZ_MAIN) += gz_main_mod.o
gz_main_mod-objs += gz_main.o \
			     mtee_ut/gz_ut.o \
			     mtee_ut/gz_chmem_ut.o mtee_ut/gz_shmem_ut.o mtee_ut/gz_vreg_ut.o \
			     mtee-kree/tz_mmap.o mtee-kree/tz_system.o mtee-kree/kree_mem.o

obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += gz-trusty/
