ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/trustzone/m4u
ccflags-y += -I$(srctree)/include/trustzone/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
endif
ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
  ccflags-y += -DCONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
endif
ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/arch/arm/include
ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6761" "mt6758" "mt6779" "mt6768"))
obj-y += pseudo_m4u_v2.o
else
obj-y += pseudo_m4u.o
obj-y += m4u_hw_$(MTK_PLATFORM).o
endif
