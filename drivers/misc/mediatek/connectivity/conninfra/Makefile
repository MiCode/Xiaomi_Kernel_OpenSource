MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
###############################################################################
# Necessary Check

ifneq ($(KERNEL_OUT),)
    ccflags-y += -imacros $(KERNEL_OUT)/include/generated/autoconf.h
endif

#ifeq ($(CONFIG_MTK_COMBO_CHIP),)
#    $(error CONFIG_MTK_COMBO_CHIP not defined)
#endif

#ifeq ($(TARGET_BUILD_VARIANT),$(filter $(TARGET_BUILD_VARIANT),userdebug user))
#    ldflags-y += -s
#endif

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y
###############################################################################


CONNSYS_PLATFORM := $(TARGET_BOARD_PLATFORM_CONNINFRA)

ifeq ($(CONNSYS_PLATFORM),)
$(info $$CONFIG_MTK_COMBO_CHIP is [${CONFIG_MTK_COMBO_CHIP}])
ifneq ($(CONFIG_MTK_COMBO_CHIP),)
# replace CONSYS_XXXX to mtXXXX
CONNINFRA_PLATFORM_ID := $(patsubst CONSYS_%,mt%,$(subst ",,$(CONFIG_MTK_COMBO_CHIP)))
$(info CONNINFRA_PLATFORM_ID is [${CONNINFRA_PLATFORM_ID}])
CONNSYS_PLATFORM := $(CONNINFRA_PLATFORM_ID)
else
CONNSYS_PLATFORM := $(MTK_PLATFORM)
endif
endif
$(info $$CONNSYS_PLATFORM is [${CONNSYS_PLATFORM}])

#ccflags-y += -D MTK_WCN_REMOVE_KERNEL_MODULE
ifeq ($(CONFIG_ARM64), y)
    ccflags-y += -D CONFIG_MTK_WCN_ARM64
endif

# Option for some ALPS specific feature, ex: AEE.
ccflags-y += -D CONNINFRA_PLAT_ALPS=1

ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
ccflags-y += -D MTK_CONNINFRA_CLOCK_BUFFER_API_AVAILABLE=1

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONNSYS_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONNSYS_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONNSYS_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/$(CONNSYS_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(CONNSYS_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eemcs
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(CONNSYS_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONNSYS_PLATFORM)
ccflags-y += -I$(srctree)/drivers/mmc/core
ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/
###############################################################################


ccflags-y += -Werror
ccflags-y += -Wno-error=format
ccflags-y += -Wno-error=format-extra-args

###############################################################################
MODULE_NAME := conninfra
ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
$(warning $(MODULE_NAME) build-in boot.img)
obj-y += $(MODULE_NAME).o
else
$(warning $(MODULE_NAME) is kernel module)
obj-m += $(MODULE_NAME).o
endif

###############################################################################
# common_main
###############################################################################
ccflags-y += -I$(src)/include
ccflags-y += -I$(src)/base/include
ccflags-y += -I$(src)/core/include
ccflags-y += -I$(src)/conf/include
ccflags-y += -I$(src)/drv_init/include
ccflags-y += -I$(src)/platform/include
ccflags-y += -I$(src)/debug_utility
ccflags-y += -I$(src)/debug_utility/include
ccflags-y += -I$(src)/debug_utility/connsyslog
ccflags-y += -I$(src)/debug_utility/connsyslog/platform/include
ccflags-y += -I$(src)/debug_utility/coredump
ccflags-y += -I$(src)/debug_utility/coredump/platform/include

# By Plaftfrom
ccflags-y += -I$(src)/platform/$(CONNSYS_PLATFORM)/include

ifneq ($(TARGET_BUILD_VARIANT), user)
    ccflags-y += -D CONNINFRA_DBG_SUPPORT=1
else
    ccflags-y += -D CONNINFRA_DBG_SUPPORT=0
endif

# Build mode option
ifeq ($(TARGET_BUILD_VARIANT),eng)
    ccflags-y += -D CONNINFRA_PLAT_BUILD_MODE=1
else ifeq ($(TARGET_BUILD_VARIANT),userdebug)
    ccflags-y += -D CONNINFRA_PLAT_BUILD_MODE=2
else ifeq ($(TARGET_BUILD_VARIANT),user)
    ccflags-y += -D CONNINFRA_PLAT_BUILD_MODE=3
else
    $(info invalid $$TARGET_BUILD_VARIANT[${TARGET_BUILD_VARIANT}])
    ccflags-y += -D CONNINFRA_PLAT_BUILD_MODE=0
endif


#ifeq ($(findstring evb, $(MTK_PROJECT)), evb)
#ccflags-y += -D CFG_WMT_EVB
#endif

#ifneq ($(filter "CONSYS_%",$(CONFIG_MTK_COMBO_CHIP)),)
#$(MODULE_NAME)-objs += common_main/platform/$(CONNSYS_PLATFORM).o
#endif

$(MODULE_NAME)-objs += base/ring.o
$(MODULE_NAME)-objs += base/osal.o
#$(MODULE_NAME)-objs += base/log.o
$(MODULE_NAME)-objs += base/msg_thread.o
$(MODULE_NAME)-objs += core/conninfra_core.o
$(MODULE_NAME)-objs += src/conninfra_dev.o
$(MODULE_NAME)-objs += src/conninfra.o
$(MODULE_NAME)-objs += conf/conninfra_conf.o
$(MODULE_NAME)-objs += platform/consys_hw.o
$(MODULE_NAME)-objs += platform/clock_mng.o
$(MODULE_NAME)-objs += platform/pmic_mng.o
$(MODULE_NAME)-objs += platform/emi_mng.o
$(MODULE_NAME)-objs += platform/consys_reg_mng.o

$(MODULE_NAME)-objs += debug_utility/conninfra_dbg.o

# By Plaftfrom
$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM).o
$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM)_pmic.o
$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM)_emi.o
$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM)_consys_reg.o
$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM)_pos.o
#$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM).o
#$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM)_pmic.o
#$(MODULE_NAME)-objs += platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM)_clock.o

# Debug utility
$(MODULE_NAME)-objs += debug_utility/connsyslog/ring_emi.o
$(MODULE_NAME)-objs += debug_utility/connsyslog/connsyslog.o
$(MODULE_NAME)-objs += debug_utility/connsyslog/platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM).o
$(MODULE_NAME)-objs += debug_utility/coredump/connsys_coredump.o
$(MODULE_NAME)-objs += debug_utility/coredump/conndump_netlink.o
$(MODULE_NAME)-objs += debug_utility/coredump/platform/$(CONNSYS_PLATFORM)/$(CONNSYS_PLATFORM).o

# Drv init
$(MODULE_NAME)-objs += drv_init/bluetooth_drv_init.o
$(MODULE_NAME)-objs += drv_init/conn_drv_init.o
$(MODULE_NAME)-objs += drv_init/fm_drv_init.o
$(MODULE_NAME)-objs += drv_init/gps_drv_init.o
$(MODULE_NAME)-objs += drv_init/wlan_drv_init.o

###############################################################################
# test
###############################################################################
ifneq ($(TARGET_BUILD_VARIANT), user)
ccflags-y += -D CFG_CONNINFRA_UT_SUPPORT

ccflags-y += -I$(src)/test/include
$(MODULE_NAME)-objs += test/conninfra_core_test.o
$(MODULE_NAME)-objs += test/conf_test.o
$(MODULE_NAME)-objs += test/cal_test.o
$(MODULE_NAME)-objs += test/msg_evt_test.o
$(MODULE_NAME)-objs += test/chip_rst_test.o
$(MODULE_NAME)-objs += test/conninfra_test.o
$(MODULE_NAME)-objs += test/mailbox_test.o
$(MODULE_NAME)-objs += test/connsyslog_test.o
$(MODULE_NAME)-objs += test/dump_test.o
endif

