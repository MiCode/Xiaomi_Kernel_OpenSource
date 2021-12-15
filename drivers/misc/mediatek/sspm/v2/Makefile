obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm.o

sspm-objs += sspm_helper.o
sspm-objs += sspm_ipi_table.o
sspm-objs += sspm_ipi_timeout_cb.o
sspm-objs += sspm_logger_impl.o
sspm-objs += sspm_plt.o
sspm-objs += sspm_reservedmem.o
sspm-objs += sspm_sysfs.o
sspm-objs += sspm_timesync.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6781
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif

ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
#ccflags-y += -DDEBUG

#SSPM_IPI_TEST=y
# obj-$(SSPM_IPI_TEST) += sspm_ipi_test.o
