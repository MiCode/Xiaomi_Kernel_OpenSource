#
# Copyright (C) 2015 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/tee/gud/Makefile.include
endif
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/tee/teei/Makefile.include
endif

ifneq ($(filter $(CONFIG_MTK_SECURE_MEM_SUPPORT) $(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM),y),)
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.include
endif

obj-y += m4u.o m4u_mva.o m4u_pgtable.o m4u_debug.o
ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT))),y)
obj-y += m4u_sec_gp.o
endif
