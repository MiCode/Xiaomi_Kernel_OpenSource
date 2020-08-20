ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_MTK_SOC_THERMAL)	+= soc_temp.o
obj-$(CONFIG_MTK_SOC_THERMAL_LVTS)	+= soc_temp_lvts.o
obj-$(CONFIG_MTK_PMIC_THERMAL) += pmic_temp.o
obj-$(CONFIG_MTK_MD_THERMAL)	+= md_cooling_common.o
md_cooling_common-y 		+= md_cooling.o
md_cooling_common-$(CONFIG_MTK_MD_THERMAL_DEBUGFS) += md_cooling_debugfs.o
obj-$(CONFIG_MTK_MD_THERMAL_MUTT)	+= md_cooling_mutt.o
obj-$(CONFIG_MTK_MD_THERMAL_TX_POWER)	+= md_cooling_tx_pwr.o
obj-$(CONFIG_MTK_MD_THERMAL_SCG_OFF)	+= md_cooling_scg_off.o

obj-$(CONFIG_MTK_THERMAL_TRACE) += thermal_trace.o
