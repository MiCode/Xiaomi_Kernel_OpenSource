# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Wall -Werror -DCONFIG_RT_REGMAP

obj-$(CONFIG_DUAL_ROLE_USB_INTF)	+= class_dual_role.o

obj-$(CONFIG_TCPC_RT1711H)	+= tcpc_rt1711h.o

obj-$(CONFIG_XM_USB_PDPHY)	+= tcpci_drv.o
tcpci_drv-objs := tcpci_core.o tcpci_typec.o tcpci_timer.o tcpm.o tcpci.o \
		pd_dbg_info.o tcpci_alert.o rt-regmap.o tcpci_dual_role.o

ifdef CONFIG_USB_POWER_DELIVERY
tcpci_drv-objs += tcpci_event.o \
		pd_core.o pd_policy_engine.o pd_process_evt.o \
		pd_dpm_core.o pd_dpm_uvdm.o pd_dpm_alt_mode_dp.o pd_dpm_pdo_select.o\
		pd_dpm_reaction.o \
		pd_process_evt_snk.o pd_process_evt_src.o pd_process_evt_vdm.o \
		pd_process_evt_drs.o pd_process_evt_prs.o pd_process_evt_vcs.o \
		pd_process_evt_dbg.o pd_process_evt_tcp.o pd_process_evt_com.o \
		pd_policy_engine_src.o pd_policy_engine_snk.o pd_policy_engine_ufp.o pd_policy_engine_vcs.o \
		pd_policy_engine_dfp.o pd_policy_engine_dr.o pd_policy_engine_drs.o pd_policy_engine_prs.o \
		pd_policy_engine_dbg.o pd_policy_engine_com.o pd_dpm_alt_mode_dc.o
endif

obj-$(CONFIG_TCPC_CLASS)	+= rt_pd_manager.o
