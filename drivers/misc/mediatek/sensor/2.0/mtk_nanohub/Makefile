# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/core
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/mtk_nanohub/nanohub
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include

obj-$(CONFIG_CUSTOM_KERNEL_SENSORHUB) += nanohub.o
nanohub-objs += mtk_nanohub.o
nanohub-objs += mtk_nanohub_ipi.o
nanohub-objs += sensor_list.o
nanohub-$(CONFIG_NANOHUB) += nanohub/main.o
nanohub-$(CONFIG_NANOHUB) += nanohub/comms.o
nanohub-$(CONFIG_NANOHUB_MTK_IPI) += nanohub/nanohub-mtk.o
