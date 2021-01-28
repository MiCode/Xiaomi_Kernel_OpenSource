ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_trace_event/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6779/

obj-$(CONFIG_MTK_LPM_MT6779) += lpm_mt6779.o

lpm_mt6779-objs += mt6779.o
lpm_mt6779-objs += mtk_lp_plat_apmcu.o
lpm_mt6779-objs += mtk_lp_plat_apmcu_mbox.o
lpm_mt6779-objs += mt6779_ipi_sspm.o

lpm_mt6779-objs += idles/mt6779_mcusys.o
lpm_mt6779-objs += suspend/mt6779_suspend.o
