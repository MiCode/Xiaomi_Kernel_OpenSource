ifneq ($(CONFIG_MTK_TINYSYS_SSPM_V2),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
else ifneq ($(CONFIG_MTK_TINYSYS_SCMI),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/tinysys_scmi/
endif

obj-$(CONFIG_MTK_PMSR) += pmsr.o
obj-$(CONFIG_MTK_PMSR) += twam/
