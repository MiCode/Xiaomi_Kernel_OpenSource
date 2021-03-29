SSPM_VERSION := v1
SSPM_VERSION_PATH := ../$(SSPM_VERSION)

obj-y += $(SSPM_VERSION_PATH)/

obj-y += sspm_emi_mpu.o
obj-y += sspm_ipi_timeout_cb.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(SSPM_VERSION)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG

#SSPM_IPI_TEST=y
obj-$(SSPM_IPI_TEST) += sspm_ipi_test.o

sspm_platform-y += ../sspm_mbox.o ../sspm_ipi_mbox.o ../sspm_logger_impl.o ../sspm_timesync.o sspm_emi_mpu.o sspm_ipi_timeout_cb.o

