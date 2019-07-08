#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)



ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/mt6779/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt6779/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/mt6779/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-amms
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci_all.o

# common services
ccci_all-y := ccci_core.o
ccci_all-y += ccci_bm.o
ccci_all-y += ccci_modem.o
ccci_all-y += ccci_rtc.o
ccci_all-y += modem_sys1.o
ccci_all-y += ccci_md_clk.o

ifeq  ($(CONFIG_MEDIATEK_MT6577_AUXADC), y)
ccci_all-y += ccci_auxadc.o
endif

ifeq ($(CONFIG_MTK_ECCCI_C2K), y)
ccci_all-y += modem_sys3.o
endif

# sub modules
obj-y += fsm/
obj-y += hif/
obj-y += port/
obj-y += udc/

$(info xuxin:)
$(info CONFIG_MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM)))
$(info CONFIG_MTK_PLATFORM := $(CONFIG_MTK_PLATFORM))
$(info MTK_PLATFORM := $(MTK_PLATFORM))

# platform code
#obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-y += mt6779/

endif
