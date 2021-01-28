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

$(info CONFIG_MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM)))
$(info CONFIG_MTK_PLATFORM := $(CONFIG_MTK_PLATFORM))
$(info MTK_PLATFORM := $(MTK_PLATFORM))


ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/mt6779/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt6779/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-amms
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ifeq ($(CCCI_OBJ), m)
ccflags-y += -DCCCI_KMODULE_ENABLE
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/hif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc/udc_lib_inc

obj-$(CCCI_OBJ) += ccci_md_clk.o
obj-$(CCCI_OBJ) += ccci_rtc.o
ifeq  ($(CONFIG_MEDIATEK_MT6577_AUXADC), y)
obj-$(CCCI_OBJ) += ccci_auxadc.o
endif

obj-$(CCCI_OBJ) += ccci_md_all.o
ccci_md_all-$(CCCI_OBJ) := ccci_core.o ccci_bm.o ccci_modem.o modem_sys1.o \
		udc/udc.o \
		hif/ccci_hif.o hif/ccci_hif_cldma.o hif/ccci_ringbuf.o hif/ccci_hif_ccif.o

# port start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/port/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/udc/udc_lib_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/conn_md/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/clkbuf_v1

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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/mt6779/
ccci_md_all-$(CCCI_OBJ) += fsm/ccci_fsm.o \
			fsm/ccci_fsm_ioctl.o \
			fsm/ccci_fsm_monitor.o \
			fsm/ccci_fsm_poller.o \
			fsm/ccci_fsm_scp.o \
			fsm/ccci_fsm_ee.o \
			fsm/ccci_fsm_sys.o \
			fsm/mdee_dumper_v1.o \
			fsm/mdee_dumper_v2.o \
			fsm/mdee_dumper_v3.o
# fsm end

# plat start
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt6779/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi_bwl/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi_bwl/mt6779/
ccflags-y += -I$(srctree)/drivers/clk/mediatek
# security objects
MASP_CORE_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/drivers/masp

ifeq ($(CONFIG_MTK_QOS_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/devfreq/
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT), y)
clean-files += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
else
clean-files += $(patsubst $(obj)/%,%,$(wildcard auth/*.o))
endif
endif


ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT), y)
CCCI_CUSTOM_DRIVER_DIR := $(MTK_PATH_CUSTOM)/$(call lc,$(MTK_PROJECT))/kernel/ccci
ccci_md_all-$(CCCI_OBJ) += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
endif
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif
ccci_md_all-$(CCCI_OBJ) += mt6779/ccci_platform.o $(core_obj) $(mach_obj) $(sec_obj) \
		mt6779/ap_md_reg_dump.o \
		mt6779/md_sys1_platform.o
# plat end

ifeq ($(CONFIG_MTK_ECCCI_C2K), y)
ccci_md_all-$(CCCI_OBJ) += modem_sys3.o
endif


# ccci_dpmaif.ko
ccci_dpmaif-y := hif/ccci_hif_dpmaif.o hif/dpmaif_drv.o
obj-$(CCCI_OBJ) += ccci_dpmaif.o

else

obj-$(CCCI_OBJ) += ccci_all.o
# common services
ccci_all-$(CCCI_OBJ) := ccci_core.o ccci_bm.o ccci_modem.o ccci_rtc.o modem_sys1.o ccci_md_clk.o

ifeq  ($(CONFIG_MEDIATEK_MT6577_AUXADC), y)
ccci_all-$(CCCI_OBJ) += ccci_auxadc.o
endif

ifeq ($(CONFIG_MTK_ECCCI_C2K), y)
ccci_all-$(CCCI_OBJ) += modem_sys3.o
endif

# sub modules
obj-$(CCCI_OBJ) += fsm/
obj-$(CCCI_OBJ) += hif/
obj-$(CCCI_OBJ) += port/
obj-$(CCCI_OBJ) += udc/

$(info CONFIG_MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM)))
$(info CONFIG_MTK_PLATFORM := $(CONFIG_MTK_PLATFORM))
$(info MTK_PLATFORM := $(MTK_PLATFORM))

# platform code
#obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CCCI_OBJ) += mt6779/

endif

endif
