# SPDX-License-Identifier: GPL-2.0

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)

obj-$(CONFIG_MTK_TINYSYS_SSPM_V2) += v2/

else ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")

obj-y += $(subst ",,$(CONFIG_MTK_TINYSYS_SSPM_VERSION))/

else

# Common
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm.o

sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_common.o sspm_mbox.o sspm_sysfs.o sspm_plt.o sspm_ipi_mbox.o sspm_reservedmem.o sspm_emi_mpu.o sspm_logger_impl.o sspm_timesync.o


# Platform Drivers
sspm-y += sspm_$(subst ",,$(CONFIG_MTK_PLATFORM)).o

ccflags-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += -I$(srctree)/drivers/misc/mediatek/sspm

ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-$(CONFIG_MTK_EMI_LEGACY) += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)

ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG

endif

else

obj-y +=  dummy.o

endif
