# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_v0.o
sspm_v0-y += sspm_common.o sspm_mbox.o sspm_sysfs.o
sspm_v0-y += sspm_plt.o sspm_ipi_mbox.o sspm_reservedmem.o
sspm_v0-y += sspm_emi_mpu.o sspm_logger_impl.o sspm_timesync.o
sspm_v0-y += sspm_helper.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
