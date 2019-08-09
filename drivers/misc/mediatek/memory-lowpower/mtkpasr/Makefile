ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ifeq ($(CONFIG_MTK_EMI),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
endif
ccflags-$(CONFIG_MACH_MT6799) += -DMTK_PASR_VCORE_DVFS_CONTROL
ccflags-$(CONFIG_MACH_MT6763) += -DMTK_PASR_VCORE_DVFS_CONTROL
ccflags-$(CONFIG_MACH_MT6758) += -DMTK_PASR_VCORE_DVFS_CONTROL
obj-y += mtkpasr_drv.o mtkpasr_hw.o
