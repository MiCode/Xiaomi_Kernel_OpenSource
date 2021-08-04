ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6873
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v3

ccflags-y += -I$(srctree)/drivers/misc/mediatek/tinysys_scmi

ifeq ($(CONFIG_MTK_QOS_MT6893),y)
 ccflags-y += -DMTK_SCMI
endif

obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos.o
ifeq ($(CONFIG_MTK_QOS_MT6893),y)
mtk_qos-objs := mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o mtk_qos_share.o qos-v6893.o mtk_qos_scmi.o
else
mtk_qos-objs := mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o mtk_qos_share.o qos-v6873.o mtk_qos_ipi_v2.o
endif
