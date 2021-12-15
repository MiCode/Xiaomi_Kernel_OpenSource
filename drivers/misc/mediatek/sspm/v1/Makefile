ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
obj-y += sspm_emi_mpu.o
obj-y += sspm_ipi_timeout_cb.o
obj-y += sspm_helper.o sspm_irq.o sspm_sysfs.o sspm_reservedmem.o sspm_plt.o
obj-y += sspm_mbox.o sspm_ipi_mbox.o sspm_logger_impl.o sspm_timesync.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
endif
