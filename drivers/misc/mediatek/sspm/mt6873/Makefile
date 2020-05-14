SSPM_VERSION := v2
SSPM_VERSION_PATH := ../$(SSPM_VERSION)

obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm.o
#obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += $(SSPM_VERSION_PATH)/

sspm-objs += $(SSPM_VERSION_PATH)/sspm_helper.o
sspm-objs += $(SSPM_VERSION_PATH)/sspm_logger_impl.o
sspm-objs += $(SSPM_VERSION_PATH)/sspm_plt.o
sspm-objs += $(SSPM_VERSION_PATH)/sspm_reservedmem.o
sspm-objs += $(SSPM_VERSION_PATH)/sspm_sysfs.o
sspm-objs += sspm_ipi_table.o
#sspm-objs += sspm_emi_mpu.o
sspm-objs += sspm_ipi_timeout_cb.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(SSPM_VERSION)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG

#SSPM_IPI_TEST=y
# obj-$(SSPM_IPI_TEST) += sspm_ipi_test.o

# sspm_platform-y += ../sspm_mbox.o ../sspm_ipi_mbox.o ../sspm_logger_impl.o ../sspm_timesync.o sspm_emi_mpu.o sspm_ipi_timeout_cb.o
