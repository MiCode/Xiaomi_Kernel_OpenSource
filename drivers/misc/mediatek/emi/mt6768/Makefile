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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/debug_latch/plat_dbg_info

obj-y += bwl_platform.o
obj-y += mpu_platform.o
obj-y += plat_debug_api.o

obj-y += ../emi_ctrl_v1.o
obj-y += ../submodule/bwl_v1.o
obj-y += ../submodule/mpu_v1.o
obj-y += ../submodule/elm_v2.o
obj-y += ../submodule/pasr_api_v1.o

