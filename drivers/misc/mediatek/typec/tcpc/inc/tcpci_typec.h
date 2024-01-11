/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __LINUX_TCPCI_TYPEC_H
#define __LINUX_TCPCI_TYPEC_H
#include "tcpci.h"

struct tcpc_device;
extern struct class *tcpc_class;
extern bool tcpc_typec_is_act_as_sink_role(struct tcpc_device *tcpc);

/******************************************************************************
 *  Call following function to trigger TYPEC Connection State Change
 *
 * 1. H/W -> CC/PS Change.
 * 2. Timer -> CCDebounce or PDDebounce or others Timeout
 * 3. Policy Engine -> PR_SWAP, Error_Recovery, PE_Idle
 *****************************************************************************/

extern bool tcpc_typec_is_cc_open_state(struct tcpc_device *tcpc);
extern int tcpc_typec_handle_cc_change(struct tcpc_device *tcpc);

extern int tcpc_typec_handle_ps_change(
		struct tcpc_device *tcpc, int vbus_level);

extern int tcpc_typec_handle_timeout(
		struct tcpc_device *tcpc, uint32_t timer_id);

extern int tcpc_typec_handle_vsafe0v(struct tcpc_device *tcpc);

extern int tcpc_typec_set_rp_level(struct tcpc_device *tcpc, uint8_t rp_lvl);

extern int tcpc_typec_error_recovery(struct tcpc_device *tcpc);

extern int tcpc_typec_disable(struct tcpc_device *tcpc);
extern int tcpc_typec_enable(struct tcpc_device *tcpc);

extern int tcpc_typec_change_role(
	struct tcpc_device *tcpc, uint8_t typec_role, bool postpone);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
extern int tcpc_typec_handle_pe_pr_swap(struct tcpc_device *tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_TYPEC_CAP_ROLE_SWAP
extern int tcpc_typec_swap_role(struct tcpc_device *tcpc);
#endif /* CONFIG_TYPEC_CAP_ROLE_SWAP */

#if CONFIG_WATER_DETECTION
extern int tcpc_typec_handle_wd(struct tcpc_device **tcpcs, size_t nr, bool wd);
#endif /* CONFIG_WATER_DETECTION */

extern int tcpc_typec_handle_fod(struct tcpc_device *tcpc,
					enum tcpc_fod_status);
extern bool tcpc_typec_ignore_fod(struct tcpc_device *tcpc);

extern int tcpc_typec_handle_otp(struct tcpc_device *tcpc, bool otp);

#if CONFIG_CABLE_TYPE_DETECTION
extern int tcpc_typec_handle_ctd(struct tcpc_device *tcpc,
				 enum tcpc_cable_type cable_type);
#endif /* CONFIG_CABLE_TYPEC_DETECTION */

#define typec_get_cc1()		\
	tcpc->typec_remote_cc[0]
#define typec_get_cc2()		\
	tcpc->typec_remote_cc[1]
#define typec_get_cc_res()	\
	(tcpc->typec_polarity ? typec_get_cc2() : typec_get_cc1())

enum TYPEC_CONNECTION_STATE {
	typec_disabled = 0,
	typec_errorrecovery,

	typec_unattached_snk,
	typec_unattached_src,

	typec_attachwait_snk,
	typec_attachwait_src,

	typec_attached_snk,
	typec_attached_src,

#if CONFIG_TYPEC_CAP_TRY_SOURCE
	/* Require : Assert Rp
	 * Exit(-> Attached.SRC) : Detect Rd (tPDDebounce).
	 * Exit(-> TryWait.SNK) : Not detect Rd after tDRPTry
	 */
	typec_try_src,

	/* Require : Assert Rd
	 * Exit(-> Attached.SNK) : Detect Rp (tCCDebounce) and Vbus present.
	 * Exit(-> Unattached.SNK) : Not detect Rp (tPDDebounce)
	 */

	typec_trywait_snk,
	typec_trywait_snk_pe,
#endif

#if CONFIG_TYPEC_CAP_TRY_SINK

	/* Require : Assert Rd
	 * Wait for tDRPTry and only then begin monitoring CC.
	 * Exit (-> Attached.SNK) : Detect Rp (tPDDebounce) and Vbus present.
	 * Exit (-> TryWait.SRC) : Not detect Rp for tPDDebounce.
	 */
	typec_try_snk,

	/*
	 * Require : Assert Rp
	 * Exit (-> Attached.SRC) : Detect Rd (tCCDebounce)
	 * Exit (-> Unattached.SNK) : Not detect Rd after tDRPTry
	 */

	typec_trywait_src,
	typec_trywait_src_pe,
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */

	typec_audioaccessory,
#if CONFIG_TYPEC_CAP_DBGACC
	typec_debugaccessory,
#endif	/* CONFIG_TYPEC_CAP_DBGACC */

#if CONFIG_TYPEC_CAP_DBGACC_SNK
	typec_attached_dbgacc_snk,
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */

#if CONFIG_TYPEC_CAP_CUSTOM_SRC
	typec_attached_custom_src,
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */

#if CONFIG_TYPEC_CAP_NORP_SRC
	typec_attached_norp_src,
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

#if CONFIG_TYPEC_CAP_ROLE_SWAP
	typec_role_swap,
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

#if CONFIG_WATER_DETECTION
	typec_water_protection_wait,
	typec_water_protection,
#endif /* CONFIG_WATER_DETECTION */

	typec_foreign_object_protection,

	typec_otp,

	typec_unattachwait_pe,	/* Wait Policy Engine go to Idle */
};
#endif /* #ifndef __LINUX_TCPCI_TYPEC_H */
