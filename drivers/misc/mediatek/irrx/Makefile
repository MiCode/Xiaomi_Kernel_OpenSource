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

ifneq ($(strip $(CONFIG_MTK_IRRX_PLAT)),)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/irrx/inc/$(CONFIG_MTK_IRRX_PLAT)/
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/irrx/inc/default/
endif
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/irrx/inc/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
obj-$(CONFIG_MTK_IRRX_NEC)   +=  nec/
obj-$(CONFIG_MTK_IRRX_RC5)   +=  rc5/
obj-$(CONFIG_MTK_IRRX_RC6)   +=  rc6/
obj-$(CONFIG_MTK_IRRX_SIRC)   +=  sirc/
obj-$(CONFIG_MTK_IRRX_RCMM)   +=  rcmm/
obj-y   +=  mtk_ir_core.o mtk_ir_mouse.o
