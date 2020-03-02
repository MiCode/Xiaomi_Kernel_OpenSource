# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_PM_DEVFREQ)	+= devfreq.o
obj-$(CONFIG_PM_DEVFREQ_EVENT)	+= devfreq-event.o
obj-$(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)	+= governor_simpleondemand.o
obj-$(CONFIG_DEVFREQ_GOV_PERFORMANCE)	+= governor_performance.o
obj-$(CONFIG_DEVFREQ_GOV_POWERSAVE)	+= governor_powersave.o
obj-$(CONFIG_DEVFREQ_GOV_USERSPACE)	+= governor_userspace.o
obj-$(CONFIG_DEVFREQ_GOV_PASSIVE)	+= governor_passive.o

# DEVFREQ Drivers
obj-$(CONFIG_ARM_EXYNOS_BUS_DEVFREQ)	+= exynos-bus.o
obj-$(CONFIG_ARM_RK3399_DMC_DEVFREQ)	+= rk3399_dmc.o
obj-$(CONFIG_ARM_TEGRA_DEVFREQ)		+= tegra-devfreq.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/spm/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)\

ifeq ($(CONFIG_MTK_QOS_SUPPORT), y)
obj-$(CONFIG_MACH_MT6771)		+= helio-dvfsrc.o helio-dvfsrc-sysfs.o
obj-$(CONFIG_MACH_MT6771)		+= helio-dvfsrc-mt6771.o
endif
obj-$(CONFIG_MACH_MT6768)		+= helio-dvfsrc-ipi.o
obj-$(CONFIG_MACH_MT6768)		+= helio-dvfsrc-v2/helio-dvfsrc.o helio-dvfsrc-v2/helio-dvfsrc-mt6768.o helio-dvfsrc-v2/helio-dvfsrc-opp-mt6768.o helio-dvfsrc-v2/helio-dvfsrc-opp.o helio-dvfsrc-v2/helio-dvfsrc-sysfs.o

obj-$(CONFIG_MACH_MT6765)		+= helio-dvfsrc-ipi.o
obj-$(CONFIG_MACH_MT6765)		+= helio-dvfsrc-v2/helio-dvfsrc.o helio-dvfsrc-v2/helio-dvfsrc-mt6765.o helio-dvfsrc-v2/helio-dvfsrc-opp-mt6765.o helio-dvfsrc-v2/helio-dvfsrc-opp.o helio-dvfsrc-v2/helio-dvfsrc-sysfs.o

obj-$(CONFIG_MACH_MT6771)		+= helio-dvfsrc-opp-mt6771.o
obj-$(CONFIG_MACH_MT6785)		+= helio-dvfsrc-v3/
# obj-$(CONFIG_MACH_MT6885)		+= helio-dvfsrc-v3/
# DEVFREQ Event Drivers
obj-$(CONFIG_PM_DEVFREQ_EVENT)		+= event/
