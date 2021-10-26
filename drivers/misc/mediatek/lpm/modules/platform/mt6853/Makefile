ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1


ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_trace_event/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6853/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/power_gs_v1

obj-y += mt6853.o
obj-y += mtk_lp_plat_apmcu.o
obj-y += mtk_lp_plat_apmcu_mbox.o

obj-y += suspend/mt6853_suspend.o
