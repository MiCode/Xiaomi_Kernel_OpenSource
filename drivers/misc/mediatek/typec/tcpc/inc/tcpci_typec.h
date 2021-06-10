/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __LINUX_TCPCI_TYPEC_H
#define __LINUX_TCPCI_TYPEC_H
#include "tcpci.h"

struct tcpc_device;
extern struct class *tcpc_class;

/******************************************************************************
 *  Call following function to trigger TYPEC Connection State Change
 *
 * 1. H/W -> CC/PS Change.
 * 2. Timer -> CCDebounce or PDDebounce or others Timeout
 * 3. Policy Engine -> PR_SWAP, Error_Recovery, PE_Idle
 *****************************************************************************/

extern int tcpc_typec_enter_lpm_again(struct tcpc_device *tcpc_dev);
extern int tcpc_typec_handle_cc_change(struct tcpc_device *tcpc_dev);

extern int tcpc_typec_handle_ps_change(
		struct tcpc_device *tcpc_dev, int vbus_level);

extern int tcpc_typec_handle_timeout(
		struct tcpc_device *tcpc_dev, uint32_t timer_id);

extern int tcpc_typec_handle_vsafe0v(struct tcpc_device *tcpc_dev);

extern int tcpc_typec_set_rp_level(struct tcpc_device *tcpc_dev, uint8_t res);

extern int tcpc_typec_error_recovery(struct tcpc_device *tcpc_dev);

extern int tcpc_typec_disable(struct tcpc_device *tcpc_dev);
extern int tcpc_typec_enable(struct tcpc_device *tcpc_dev);

extern int tcpc_typec_change_role(
	struct tcpc_device *tcpc_dev, uint8_t typec_role);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
extern int tcpc_typec_handle_pe_pr_swap(struct tcpc_device *tcpc_dev);
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
extern int tcpc_typec_swap_role(struct tcpc_device *tcpc_dev);
#endif /* CONFIG_TYPEC_CAP_ROLE_SWAP */

extern int tcpc_typec_handle_wd(struct tcpc_device *tcpc_dev, bool wd);

extern int tcpc_typec_handle_fod(struct tcpc_device *tcpc_dev,
				 enum tcpc_fod_status);
extern bool tcpc_typec_ignore_fod(struct tcpc_device *tcpc_dev);

extern int tcpc_typec_handle_otp(struct tcpc_device *tcpc_dev, bool otp);

extern int tcpc_typec_handle_ctd(struct tcpc_device *tcpc_dev,
				 enum tcpc_cable_type cable_type);

extern bool tcpc_typec_is_cc_attach(struct tcpc_device *tcpc_dev);

#if IS_ENABLED(CONFIG_MTK_CHARGER)
extern int tcpc_get_charger_type(struct tcpc_device *tcpc_dev);
#endif /* CONFIG_MTK_CHARGER */
#endif /* #ifndef __LINUX_TCPCI_TYPEC_H */
