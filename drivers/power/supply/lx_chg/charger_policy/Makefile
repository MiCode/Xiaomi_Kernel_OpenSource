obj-$(CONFIG_LX_CHARGER_MANAGER)        += main_chg.o
main_chg-objs:= lx_charger_manager.o lx_jeita.o lx_charge_sysfs.o lx_charge_class_sysfs.o 
ifeq ($(CONFIG_XIAOMI_SMART_CHG),m)
main_chg-objs += xm_smart_chg.o
endif
obj-$(CONFIG_LX_CHARGER_MANAGER)        += ../common/lx_voter.o
obj-$(CONFIG_LX_CHARGER_MANAGER)        += ../common/lx_notify.o
obj-$(CONFIG_LX_CP_POLICY)              += lx_cp_policy.o
obj-$(CONFIG_LX_CP_POLICY)              += lx_pd_adapter.o
obj-$(CONFIG_PD_BATTERY_SECRET)             += xm_pd_adapter.o
obj-$(CONFIG_LX_QC_POLICY)              += lx_qc_policy.o
