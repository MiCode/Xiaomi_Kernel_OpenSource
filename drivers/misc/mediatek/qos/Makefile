ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6873
endif

QOS_PLAT_NAME = $(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

ifneq (,$(findstring mt6873, $(QOS_PLAT_NAME)))
 ccflags-y += -DMTK_QOS_V1
endif

obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos.o
mtk_qos-objs := mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o qos-v6873.o mtk_qos_ipi_v2.o
