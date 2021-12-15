
ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)/
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/mt6781
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM)
endif
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_trace_event/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6781/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/power_gs_v1

obj-y += mt6781.o
obj-y += mtk_lp_plat_apmcu.o
obj-y += mtk_lp_plat_apmcu_mbox.o

obj-y += suspend/mt6781_suspend.o
