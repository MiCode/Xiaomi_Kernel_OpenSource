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

ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
  ccflags-y += -I$(src)/includes
  #ccflags-y += -I$(src)/$(MTK_PLATFORM)
  ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
    include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
  else ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
    include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
  else ifeq ($(CONFIG_BLOWFISH_TEE_SUPPORT),y)
    ccflags-y += -I$(srctree)/drivers/misc/tzdev
  else
  endif
else # Legacy
  ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
    ccflags-y += -I$(src)/includes
    #ccflags-y += -I$(src)/$(MTK_PLATFORM)
    include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
  endif
endif # !CONFIG_MTK_TEE_GP_SUPPORT

ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
  ifeq ($(CONFIG_BLOWFISH_TEE_SUPPORT),y)
    obj-y += secmem_csmc.o
  else
    obj-y += secmem_gp.o
  endif
else
  obj-y += secmem.o
endif
