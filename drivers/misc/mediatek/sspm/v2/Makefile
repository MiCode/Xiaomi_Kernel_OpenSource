obj-y += sspm_helper.o sspm_sysfs.o sspm_reservedmem.o sspm_plt.o
obj-y += sspm_logger_impl.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm

ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6781
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
