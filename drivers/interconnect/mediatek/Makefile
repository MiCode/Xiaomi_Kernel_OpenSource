# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/iommu
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos
obj-$(CONFIG_INTERCONNECT_MTK_EMI) += mtk-emi.o
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON) += mmqos-common.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs
mmqos-common-objs += mmqos-mtk.o mmqos-hrt.o
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6873.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6873) += mmqos-mt6873.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6853.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6853) += mmqos-mt6853.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6983.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6983) += mmqos-mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6985.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6985) += mmqos-mt6985.o
endif

