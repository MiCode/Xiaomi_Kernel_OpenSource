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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/arch/arm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif
ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/m4u

ifeq ($(CONFIG_MTK_LEGACY_SECMEM_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/secmem
ccflags-y += -I$(srctree)/drivers/misc/mediatek/secmem/includes
else ifeq ($(CONFIG_MTK_SECURE_MEM_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.include
endif

obj-y += m4u.o m4u_mva.o m4u_pgtable.o m4u_debug.o
ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
obj-y += m4u_sec_gp.o
endif
