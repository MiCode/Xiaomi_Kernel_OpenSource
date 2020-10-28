# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_MTK_TEE_SANITY) += tee_sanity.o

# timesync to get boot time
ccflags-y += -I$(srctree)/drivers/misc/mediatek/timer/timesync

# sched for cpu prefer
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sched/
