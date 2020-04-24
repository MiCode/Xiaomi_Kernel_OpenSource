# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/drivers/net/wireless/cnss_utils/
obj-$(CONFIG_ICNSS2) += icnss2.o

icnss2-y := main.o
icnss2-y += debug.o
icnss2-y += power.o
icnss2-y += genl.o
icnss2-$(CONFIG_ICNSS2_QMI) += qmi.o
