obj-$(CONFIG_DUAL_ROLE_USB_INTF)	+= tcpci_dual_role.o
obj-$(CONFIG_TCPC_MT6360)		+= tcpc_mt6360.o
obj-$(CONFIG_TCPC_MT6370)		+= tcpc_mt6370.o
obj-$(CONFIG_TCPC_RT5081)		+= tcpc_rt5081.o
obj-$(CONFIG_TCPC_CLASS)		+= tcpci_core.o tcpci_typec.o tcpci_timer.o \
					   tcpm.o tcpci.o pd_dbg_info.o
ifeq ($(CONFIG_TCPC_ALERT_V20), y)
obj-$(CONFIG_TCPC_CLASS)		+= tcpci_alert_v20.o
else
obj-$(CONFIG_TCPC_CLASS)		+= tcpci_alert.o
endif

obj-$(CONFIG_USB_POWER_DELIVERY)	+= tcpci_event.o \
					   pd_core.o pd_policy_engine.o pd_process_evt.o \
					   pd_dpm_core.o pd_dpm_uvdm.o pd_dpm_alt_mode_dp.o pd_dpm_pdo_select.o\
					   pd_dpm_reaction.o \
					   pd_process_evt_snk.o pd_process_evt_src.o pd_process_evt_vdm.o \
					   pd_process_evt_drs.o pd_process_evt_prs.o pd_process_evt_vcs.o \
					   pd_process_evt_dbg.o pd_process_evt_tcp.o pd_process_evt_com.o \
					   pd_policy_engine_src.o pd_policy_engine_snk.o pd_policy_engine_ufp.o pd_policy_engine_vcs.o \
					   pd_policy_engine_dfp.o pd_policy_engine_dr.o pd_policy_engine_drs.o pd_policy_engine_prs.o \
					   pd_policy_engine_dbg.o pd_policy_engine_com.o pd_dpm_alt_mode_dc.o
