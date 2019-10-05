ccflags += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/include/trustzone/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
endif
ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
  ccflags-y += -DCONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
endif
#ccflags-y += -I$(srctree)/drivers/iommu
obj-y += pseudo_m4u.o
obj-y += m4u_hw_$(MTK_PLATFORM).o
