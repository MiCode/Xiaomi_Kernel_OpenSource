obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_v1.o

sspm_v1-y += sspm_emi_mpu.o
sspm_v1-y += sspm_ipi_timeout_cb.o
sspm_v1-y += sspm_helper.o sspm_irq.o sspm_sysfs.o sspm_reservedmem.o sspm_plt.o
sspm_v1-y += sspm_mbox.o sspm_ipi_mbox.o sspm_logger_impl.o sspm_timesync.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm
#ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
