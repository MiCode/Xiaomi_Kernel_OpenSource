ifeq ($(CONFIG_MACH_MT6893),y)
PDIR = mt6885
else ifeq ($(CONFIG_MACH_MT6877),y)
PDIR = mt6877
else
PDIR = $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(PDIR)/include/mach
ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/apusys/power/2.0/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

ccflags-y += -I$(obj)/../$(PDIR)

obj-y += reviser.o

reviser-objs:= reviser_drv.o reviser_hw.o reviser_mem.o reviser_mem_mgt.o reviser_dbg.o reviser_export.o ../$(PDIR)/reviser_reg.o
