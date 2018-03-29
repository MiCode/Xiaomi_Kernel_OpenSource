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

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# common services
ccci-y := ccci_core.o ccci_bm.o ccci_support.o ccci_modem.o ccci_swtp.o ccci_fsm.o

# port routines
ccci-y += port_char.o port_net.o port_ipc.o port_cfg.o port_smem.o \
	port_rpc.o port_sysmsg.o port_poller.o port_ctlmsg.o port_proxy.o

# md ee routines
ccci-y += mdee_ctl.o mdee_dumper_v1.o mdee_dumper_v2.o

# modem drivers
ifeq ($(CONFIG_MTK_ECCCI_CCIF), y)
ccci-y += ccci_ringbuf.o modem_ccif.o
endif

ifeq ($(CONFIG_MTK_ECCCI_CLDMA), y)
ccci-y += modem_cldma.o
endif

# make sure modem_cldma.c run before modem_ccif_c2k.c, because when IRAT, c2k will use md1's ccmni
ifeq ($(CONFIG_MTK_ECCCI_C2K), y)
ccci-y += ccci_ringbuf.o modem_ccif_c2k.o
endif

# modem_xxx.o and modem_ut.o should not be enabled both, unless you checked code (device number. etc.).
ifeq ($(CONFIG_MTK_ECCCI_UT), y)
ccci-y += modem_ut.o
endif

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

else
obj-  := dummy.o # avoid build error
endif


