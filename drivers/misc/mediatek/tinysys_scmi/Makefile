# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_MTK_TINYSYS_SCMI) += tinysys-scmi.o

tinysys-scmi-y := tinysys-scmi-drv.o tinysys.o


ccflags-y += -I$(srctree)/drivers/firmware/arm_scmi

ccflags-y += -DTINYSYS_SCMI_DEBUG
