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
CCCI_OBJ=y
else ifeq ($(CONFIG_MTK_ECCCI_DRIVER), m)
CCCI_OBJ=m
else
CCCI_OBJ=n
endif

ifneq ($(CCCI_OBJ), n)

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/rv/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-amms
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/hif/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdpm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pbm/

ifeq ($(CCCI_OBJ), m)
ccflags-y += -DCCCI_KMODULE_ENABLE
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc/udc_lib_inc

ifneq ($(CONFIG_MEDIATEK_MT6577_AUXADC), n)
obj-$(CCCI_OBJ) += ccci_auxadc.o
endif

obj-$(CCCI_OBJ) += ccci_md_all.o
ccci_md_all-$(CCCI_OBJ) := ccci_core.o ccci_bm.o ccci_modem.o \
		udc/udc.o \
		hif/ccci_hif.o

# port start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/port/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc/udc_lib_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/

ccci_md_all-$(CCCI_OBJ) += port/port_cfg.o \
		port/port_char.o \
		port/port_ctlmsg.o \
		port/port_udc.o \
		port/port_ipc.o \
		port/port_net.o \
		port/port_poller.o \
		port/port_proxy.o \
		port/port_rpc.o \
		port/port_smem.o \
		port/port_sysmsg.o \
		port/ccci_swtp.o
# port end
# fsm start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/fsm/
ccci_md_all-$(CCCI_OBJ) += fsm/ccci_fsm.o \
			fsm/ccci_fsm_ioctl.o \
			fsm/ccci_fsm_monitor.o \
			fsm/ccci_fsm_poller.o \
			fsm/ccci_fsm_ee.o \
			fsm/ccci_fsm_sys.o \
			fsm/mdee_dumper_v1.o \
			fsm/mdee_dumper_v2.o \
			fsm/mdee_dumper_v3.o \
			fsm/mdee_dumper_v5.o \
			fsm/modem_sys1.o \
			fsm/md_sys1_platform.o \
			fsm/ap_md_reg_dump.o \
			fsm/ap_md_mem.o

# fsm end

# plat start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
ccflags-y += -I$(srctree)/drivers/clk/mediatek
# security objects
MASP_CORE_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/drivers/masp

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/fsm
obj-$(CCCI_OBJ)	+= fsm/

# ccci_ccif.ko, ccci_dpmaif.ko, ccci_cldma.ko
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/hif
obj-$(CCCI_OBJ)	+= hif/




else

obj-$(CCCI_OBJ) += ccci_all.o
# common services
ccci_all-$(CCCI_OBJ) := ccci_core.o ccci_bm.o ccci_modem.o modem_sys1.o


ifneq ($(CONFIG_MEDIATEK_MT6577_AUXADC), n)
ccci_all-$(CCCI_OBJ) += ccci_auxadc.o
endif

# sub modules
obj-$(CCCI_OBJ) += fsm/
obj-$(CCCI_OBJ) += hif/
obj-$(CCCI_OBJ) += port/
obj-$(CCCI_OBJ) += udc/

# platform code
obj-$(CCCI_OBJ) += platform/

endif

endif
