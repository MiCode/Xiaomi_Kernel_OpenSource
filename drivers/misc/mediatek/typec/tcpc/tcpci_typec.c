// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/cpu.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci_timer.h"

/* MTK only */
#include <mt-plat/mtk_boot.h>

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
#define CONFIG_TYPEC_CAP_TRY_STATE
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
#undef CONFIG_TYPEC_CAP_TRY_STATE
#define CONFIG_TYPEC_CAP_TRY_STATE
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

#define RICHTEK_PD_COMPLIANCE_FAKE_AUDIO_ACC	/* For Rp3A */
#define RICHTEK_PD_COMPLIANCE_FAKE_EMRAK_ONLY	/* For Rp3A */
#define RICHTEK_PD_COMPLIANCE_FAKE_RA_DETACH	/* For Rp-DFT */

enum TYPEC_WAIT_PS_STATE {
	TYPEC_WAIT_PS_DISABLE = 0,
	TYPEC_WAIT_PS_SNK_VSAFE5V,
	TYPEC_WAIT_PS_SRC_VSAFE0V,
	TYPEC_WAIT_PS_SRC_VSAFE5V,
#ifdef CONFIG_TYPEC_CAP_DBGACC
	TYPEC_WAIT_PS_DBG_VSAFE5V,
#endif	/* CONFIG_TYPEC_CAP_DBGACC */
};

enum TYPEC_ROLE_SWAP_STATE {
	TYPEC_ROLE_SWAP_NONE = 0,
	TYPEC_ROLE_SWAP_TO_SNK,
	TYPEC_ROLE_SWAP_TO_SRC,
};

#if TYPEC_INFO2_ENABLE
static const char *const typec_wait_ps_name[] = {
	"Disable",
	"SNK_VSafe5V",
	"SRC_VSafe0V",
	"SRC_VSafe5V",
	"DBG_VSafe5V",
};
#endif	/* TYPEC_INFO2_ENABLE */

static inline void typec_wait_ps_change(struct tcpc_device *tcpc,
					enum TYPEC_WAIT_PS_STATE state)
{
#if TYPEC_INFO2_ENABLE
	uint8_t old_state = tcpc->typec_wait_ps_change;
	uint8_t new_state = (uint8_t) state;

	if (new_state != old_state)
		TYPEC_INFO2("wait_ps=%s\n", typec_wait_ps_name[new_state]);
#endif	/* TYPEC_INFO2_ENABLE */

#ifdef CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_TIMEOUT
	if (state == TYPEC_WAIT_PS_SRC_VSAFE0V)
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_SAFE0V_TOUT);
#endif	/* CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_TIMEOUT */

	if (tcpc->typec_wait_ps_change == TYPEC_WAIT_PS_SRC_VSAFE0V
		&& state != TYPEC_WAIT_PS_SRC_VSAFE0V) {
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_SAFE0V_DELAY);

#ifdef CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_TIMEOUT
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_SAFE0V_TOUT);
#endif	/* CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_TIMEOUT */
	}

	tcpc->typec_wait_ps_change = (uint8_t) state;
}

/* #define TYPEC_EXIT_ATTACHED_SRC_NO_DEBOUNCE */
#define TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS

static inline int typec_enable_low_power_mode(
	struct tcpc_device *tcpc, uint8_t pull);

#define typec_check_cc1(cc)	\
	(typec_get_cc1() == cc)

#define typec_check_cc2(cc)	\
	(typec_get_cc2() == cc)

#define typec_check_cc(cc1, cc2)	\
	(typec_check_cc1(cc1) && typec_check_cc2(cc2))

#define typec_check_cc_both(res)	\
	(typec_check_cc(res, res))

#define typec_check_cc_any(res)		\
	(typec_check_cc1(res) || typec_check_cc2(res))

#define typec_is_drp_toggling() \
	(typec_get_cc1() == TYPEC_CC_DRP_TOGGLING)

#define typec_is_cc_open()	\
	typec_check_cc_both(TYPEC_CC_VOLT_OPEN)

#define typec_is_cable_only()	\
	(typec_get_cc1() + typec_get_cc2() == TYPEC_CC_VOLT_RA)

#define typec_is_sink_with_emark()	\
	(typec_get_cc1() + typec_get_cc2() == \
	TYPEC_CC_VOLT_RA+TYPEC_CC_VOLT_RD)

#define typec_is_cc_no_res()	\
	(typec_is_drp_toggling() || typec_is_cc_open())

static inline int typec_enable_vconn(struct tcpc_device *tcpc)
{
#ifndef CONFIG_USB_POWER_DELIVERY
	if (!typec_is_sink_with_emark())
		return 0;
#endif /* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (tcpc->tcpc_vconn_supply == TCPC_VCONN_SUPPLY_NEVER)
		return 0;
#endif /* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	return tcpci_set_vconn(tcpc, true);
}

/*
 * [BLOCK] TYPEC Connection State Definition
 */

enum TYPEC_CONNECTION_STATE {
	typec_disabled = 0,
	typec_errorrecovery,

	typec_unattached_snk,
	typec_unattached_src,

	typec_attachwait_snk,
	typec_attachwait_src,

	typec_attached_snk,
	typec_attached_src,

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
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

#ifdef CONFIG_TYPEC_CAP_TRY_SINK

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
#ifdef CONFIG_TYPEC_CAP_DBGACC
	typec_debugaccessory,
#endif	/* CONFIG_TYPEC_CAP_DBGACC */

#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	typec_attached_dbgacc_snk,
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */

#ifdef CONFIG_TYPEC_CAP_CUSTOM_SRC
	typec_attached_custom_src,
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	typec_attached_norp_src,
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	typec_role_swap,
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

#ifdef CONFIG_WATER_DETECTION
	typec_water_protection_wait,
	typec_water_protection,
#endif /* CONFIG_WATER_DETECTION */

	typec_unattachwait_pe,	/* Wait Policy Engine go to Idle */
};

#if TYPEC_INFO_ENABLE || TCPC_INFO_ENABLE
static const char *const typec_state_name[] = {
	"Disabled",
	"ErrorRecovery",

	"Unattached.SNK",
	"Unattached.SRC",

	"AttachWait.SNK",
	"AttachWait.SRC",

	"Attached.SNK",
	"Attached.SRC",

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	"Try.SRC",
	"TryWait.SNK",
	"TryWait.SNK.PE",
#endif	/* CONFIG_TYPEC_CAP_TRY_SOURCE */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	"Try.SNK",
	"TryWait.SRC",
	"TryWait.SRC.PE",
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */

	"AudioAccessory",
#ifdef CONFIG_TYPEC_CAP_DBGACC
	"DebugAccessory",
#endif	/* CONFIG_TYPEC_CAP_DBGACC */

#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	"DBGACC.SNK",
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */

#ifdef CONFIG_TYPEC_CAP_CUSTOM_SRC
	"Custom.SRC",
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	"NoRp.SRC",
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	"RoleSwap",
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

#ifdef CONFIG_WATER_DETECTION
	"WaterProtection.Wait",
	"WaterProtection",
#endif /* CONFIG_WATER_DETECTION */

	"UnattachWait.PE",
};
#endif /* TYPEC_INFO_ENABLE || TCPC_INFO_ENABLE */

static inline void typec_transfer_state(struct tcpc_device *tcpc,
					enum TYPEC_CONNECTION_STATE state)
{
#if TYPEC_INFO_ENABLE
	if (state >= 0 && state < ARRAY_SIZE(typec_state_name))
		TYPEC_INFO("** %s\n", typec_state_name[state]);
#endif /* TYPEC_INFO_ENABLE */
	tcpc->typec_state = (uint8_t) state;
}

#define TYPEC_NEW_STATE(state)  \
	(typec_transfer_state(tcpc, state))

/*
 * [BLOCK] TypeC Alert Attach Status Changed
 */

#if TYPEC_INFO_ENABLE || TYPEC_DBG_ENABLE
static const char *const typec_attach_name[] = {
	"NULL",
	"SINK",
	"SOURCE",
	"AUDIO",
	"DEBUG",

	"DBGACC_SNK",
	"CUSTOM_SRC",
	"NORP_SRC",
};
#endif /* TYPEC_INFO_ENABLE || TYPEC_DBG_ENABLE */

static int typec_alert_attach_state_change(struct tcpc_device *tcpc)
{
	int ret = 0;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (tcpc->typec_legacy_cable)
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_NOT_LEGACY);
	else
		tcpc_restart_timer(tcpc, TYPEC_RT_TIMER_NOT_LEGACY);
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	if (tcpc->typec_attach_old == tcpc->typec_attach_new) {
		TYPEC_DBG("Attached-> %s(repeat)\n",
			typec_attach_name[tcpc->typec_attach_new]);
		return 0;
	}

	TYPEC_INFO("Attached-> %s\n",
		   typec_attach_name[tcpc->typec_attach_new]);

	/*Report function */
	ret = tcpci_report_usb_port_changed(tcpc);

	tcpc->typec_attach_old = tcpc->typec_attach_new;
	return ret;
}

static inline int typec_set_drp_toggling(struct tcpc_device *tcpc)
{
	int ret;

	ret = tcpci_set_cc(tcpc, TYPEC_CC_DRP);
	if (ret < 0)
		return ret;

	return typec_enable_low_power_mode(tcpc, TYPEC_CC_DRP);
}

#ifdef CONFIG_WATER_DETECTION
static int typec_check_water_status(struct tcpc_device *tcpc)
{
	int ret;

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION))
		return 0;

	ret = tcpci_is_water_detected(tcpc);
	if (ret < 0)
		return ret;
	if (ret) {
		tcpc_typec_handle_wd(tcpc, true);
		return 1;
	}
	return 0;
}
#endif /* CONFIG_WATER_DETECTION */

/*
 * [BLOCK] NoRpSRC Entry
 */

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
static bool typec_try_enter_norp_src(struct tcpc_device *tcpc)
{
	if (tcpci_check_vbus_valid_from_ic(tcpc) &&
	    typec_is_cc_no_res() &&
	    tcpc->typec_state == typec_unattached_snk) {
		TYPEC_INFO("norp_src=1\n");
		tcpc_enable_timer(tcpc, TYPEC_TIMER_NORP_SRC);
		return true;
	}

	return false;
}

static void typec_unattach_wait_pe_idle_entry(struct tcpc_device *tcpc);
static bool typec_try_exit_norp_src(struct tcpc_device *tcpc)
{
	if ((!tcpci_check_vbus_valid_from_ic(tcpc) ||
	     !typec_is_cc_no_res()) &&
	    tcpc->typec_state == typec_attached_norp_src) {
		TYPEC_INFO("norp_src=0\n");
		typec_unattach_wait_pe_idle_entry(tcpc);
		typec_alert_attach_state_change(tcpc);
		return true;
	}

	return false;
}

static inline int typec_norp_src_attached_entry(struct tcpc_device *tcpc)
{
#ifdef CONFIG_WATER_DETECTION
#ifdef CONFIG_WD_POLLING_ONLY
	if (!tcpc->typec_power_ctrl) {
		if (tcpc->bootmode == 8 || tcpc->bootmode == 9)
			typec_check_water_status(tcpc);

		tcpci_set_usbid_polling(tcpc, false);
	}
#else
	if (!tcpc->typec_power_ctrl && typec_check_water_status(tcpc))
		return 0;
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WATER_DETECTION */

	TYPEC_NEW_STATE(typec_attached_norp_src);
	tcpc->typec_attach_new = TYPEC_ATTACHED_NORP_SRC;

#ifdef CONFIG_TYPEC_CAP_A2C_C2C
	tcpc->typec_a2c_cable = true;
#endif	/* CONFIG_TYPEC_CAP_A2C_C2C */

	tcpci_report_power_control(tcpc, true);
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, 500);

	typec_alert_attach_state_change(tcpc);
	return 0;
}
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

/*
 * [BLOCK] Unattached Entry
 */

static inline int typec_try_low_power_mode(struct tcpc_device *tcpc)
{
	int ret = tcpci_set_low_power_mode(
		tcpc, true, tcpc->typec_lpm_pull);
	if (ret < 0)
		return ret;

#ifdef CONFIG_TCPC_LPM_CONFIRM
	ret = tcpci_is_low_power_mode(tcpc);
	if (ret < 0)
		return ret;

	if (ret == 1)
		return 0;

	if (tcpc->typec_lpm_retry == 0) {
		TYPEC_INFO("TryLPM Failed\n");
		return 0;
	}

	tcpc->typec_lpm_retry--;
	TYPEC_DBG("RetryLPM : %d\n", tcpc->typec_lpm_retry);
	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_LOW_POWER_MODE);
#endif	/* CONFIG_TCPC_LPM_CONFIRM */

	return 0;
}

static inline int typec_enter_low_power_mode(struct tcpc_device *tcpc)
{
	int ret = 0;

#ifdef CONFIG_TCPC_LPM_POSTPONE
	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_LOW_POWER_MODE);
#else
	ret = typec_try_low_power_mode(tcpc);
#endif	/* CONFIG_TCPC_POSTPONE_LPM */

	return ret;
}

static inline int typec_enable_low_power_mode(
	struct tcpc_device *tcpc, uint8_t pull)
{
	int ret = 0;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (tcpc->typec_legacy_cable) {
		TYPEC_DBG("LPM_LCOnly\n");
		return 0;
	}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	if (tcpc->typec_cable_only) {
		TYPEC_DBG("LPM_RaOnly\n");

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
		if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
			tcpc_enable_wakeup_timer(tcpc, true);
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

		return 0;
	}

	if (tcpc->typec_lpm != true) {
		tcpc->typec_lpm = true;
		tcpc->typec_lpm_retry = TCPC_LOW_POWER_MODE_RETRY;
		tcpc->typec_lpm_pull = (uint8_t) pull;
		ret = typec_enter_low_power_mode(tcpc);
	}

	return ret;
}

static inline int typec_disable_low_power_mode(
	struct tcpc_device *tcpc)
{
	int ret = 0;

	if (tcpc->typec_lpm != false) {
		tcpc->typec_lpm = false;
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_LOW_POWER_MODE);
		tcpci_set_low_rp_duty(tcpc, false);
		ret = tcpci_set_low_power_mode(tcpc, false, TYPEC_CC_DRP);
	}

#ifdef CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY
	tcpc->typec_wakeup_once = 0;
	tcpc->typec_low_rp_duty_cntdown = 0;
#endif	/* CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY */

	return ret;
}

static void typec_unattached_power_entry(struct tcpc_device *tcpc)
{
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	if (tcpc->typec_power_ctrl) {
		tcpci_set_vconn(tcpc, false);
		tcpci_disable_vbus_control(tcpc);
		tcpci_report_power_control(tcpc, false);
	}
}

static inline void typec_unattached_src_and_drp_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_unattached_src);
	tcpci_set_cc(tcpc, TYPEC_CC_RP);
	tcpc_enable_timer(tcpc, TYPEC_TIMER_DRP_SRC_TOGGLE);
}

static inline void typec_unattached_snk_and_drp_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_unattached_snk);
	tcpci_set_cc(tcpc, TYPEC_CC_DRP);
	typec_enable_low_power_mode(tcpc, TYPEC_CC_DRP);
}

static inline void typec_unattached_cc_entry(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	if (tcpc->typec_during_role_swap) {
		TYPEC_NEW_STATE(typec_role_swap);
		return;
	}
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */
#ifdef CONFIG_CABLE_TYPE_DETECTION
	tcpc_typec_handle_ctd(tcpc, TCPC_CABLE_TYPE_NONE);
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	tcpc->typec_role = tcpc->typec_role_new;
	switch (tcpc->typec_role) {
	case TYPEC_ROLE_SNK:
		TYPEC_NEW_STATE(typec_unattached_snk);
		tcpci_set_cc(tcpc, TYPEC_CC_RD);
		typec_enable_low_power_mode(tcpc, TYPEC_CC_RD);
		break;
	case TYPEC_ROLE_SRC:
#ifdef CONFIG_TYPEC_CHECK_SRC_UNATTACH_OPEN
		if (typec_check_cc_any(TYPEC_CC_VOLT_RD)) {
			TYPEC_DBG("typec_src_unattach not open\n");
			tcpci_set_cc(tcpc, TYPEC_CC_OPEN);
			usleep_rnage(5000, 6000);
		}
#endif	/* CONFIG_TYPEC_CHECK_SRC_UNATTACH_OPEN */
		TYPEC_NEW_STATE(typec_unattached_src);
		tcpci_set_cc(tcpc, TYPEC_CC_RP);
		typec_enable_low_power_mode(tcpc, TYPEC_CC_RP);
		break;
	case TYPEC_ROLE_TRY_SRC:
		if (tcpc->typec_state == typec_errorrecovery) {
			typec_unattached_src_and_drp_entry(tcpc);
			break;
		}
		/* pass through */
	default:
		switch (tcpc->typec_state) {
		case typec_attachwait_snk:
		case typec_audioaccessory:
			typec_unattached_src_and_drp_entry(tcpc);
			break;
		default:
			typec_unattached_snk_and_drp_entry(tcpc);
			break;
		}
		break;
	}

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	typec_try_enter_norp_src(tcpc);
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
}

static void typec_unattached_entry(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CAP_CUSTOM_HV
	tcpc->typec_during_custom_hv = false;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_HV */

	tcpc->typec_usb_sink_curr = CONFIG_TYPEC_SNK_CURR_DFT;

	if (tcpc->typec_power_ctrl)
		tcpci_set_vconn(tcpc, false);
	typec_unattached_cc_entry(tcpc);
	typec_unattached_power_entry(tcpc);
}

static void typec_unattach_wait_pe_idle_entry(struct tcpc_device *tcpc)
{
	tcpc->typec_attach_new = TYPEC_UNATTACHED;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc->pd_pe_running) {
		TYPEC_NEW_STATE(typec_unattachwait_pe);
		return;
	}
#endif	/* CONFIG_USB_POWER_DELIVERY */

	typec_unattached_entry(tcpc);
}

static void typec_postpone_state_change(struct tcpc_device *tcpc)
{
	TYPEC_DBG("Postpone AlertChange\n");
	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_STATE_CHANGE);
}

static void typec_cc_open_entry(struct tcpc_device *tcpc, uint8_t state)
{
	mutex_lock(&tcpc->access_lock);
	TYPEC_NEW_STATE(state);
	tcpc->typec_attach_new = TYPEC_UNATTACHED;
	mutex_unlock(&tcpc->access_lock);

	tcpci_set_cc(tcpc, TYPEC_CC_OPEN);
	typec_unattached_power_entry(tcpc);

	typec_postpone_state_change(tcpc);
}

static inline void typec_error_recovery_entry(struct tcpc_device *tcpc)
{
	typec_cc_open_entry(tcpc, typec_errorrecovery);
	tcpc_reset_typec_debounce_timer(tcpc);
	tcpc_enable_timer(tcpc, TYPEC_TIMER_ERROR_RECOVERY);
}

static inline void typec_disable_entry(struct tcpc_device *tcpc)
{
	typec_cc_open_entry(tcpc, typec_disabled);
}

/*
 * [BLOCK] Attached Entry
 */

static inline int typec_set_polarity(struct tcpc_device *tcpc,
					bool polarity)
{
	tcpc->typec_polarity = polarity;
	return tcpci_set_polarity(tcpc, polarity);
}

static inline int typec_set_plug_orient(struct tcpc_device *tcpc,
				uint8_t pull, bool polarity)
{
	int rv = typec_set_polarity(tcpc, polarity);

	if (rv)
		return rv;

	return tcpci_set_cc(tcpc, pull);
}

static void typec_source_attached_with_vbus_entry(struct tcpc_device *tcpc)
{
	tcpc->typec_attach_new = TYPEC_ATTACHED_SRC;
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);
}

static inline void typec_source_attached_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_attached_src);
	tcpc->typec_is_attached_src = true;
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_SRC_VSAFE5V);

	tcpc_disable_timer(tcpc, TYPEC_TRY_TIMER_DRP_TRY);

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	if (tcpc->typec_during_role_swap) {
		tcpc->typec_during_role_swap = TYPEC_ROLE_SWAP_NONE;
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_ROLE_SWAP_STOP);
	}
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

	typec_set_plug_orient(tcpc,
		TYPEC_CC_PULL(tcpc->typec_local_rp_level, TYPEC_CC_RP),
		typec_check_cc2(TYPEC_CC_VOLT_RD));

	tcpci_report_power_control(tcpc, true);
	typec_enable_vconn(tcpc);
	tcpci_source_vbus(tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, -1);
}

static inline void typec_sink_attached_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_attached_snk);
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;

#ifdef CONFIG_TYPEC_CAP_TRY_STATE
	if (tcpc->typec_role >= TYPEC_ROLE_DRP)
		tcpc_reset_typec_try_timer(tcpc);
#endif	/* CONFIG_TYPEC_CAP_TRY_STATE */

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	if (tcpc->typec_during_role_swap) {
		tcpc->typec_during_role_swap = TYPEC_ROLE_SWAP_NONE;
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_ROLE_SWAP_STOP);
	}
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

	typec_set_plug_orient(tcpc, TYPEC_CC_RD,
		!typec_check_cc2(TYPEC_CC_VOLT_OPEN));
	tcpc->typec_remote_rp_level = typec_get_cc_res();

	tcpci_report_power_control(tcpc, true);
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, -1);
}

static inline void typec_custom_src_attached_entry(
	struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	TYPEC_DBG("[Warning] Same Rp (%d)\n", typec_get_cc1());
#else
	TYPEC_DBG("[Warning] CC Both Rp\n");
#endif

#ifdef CONFIG_TYPEC_CAP_CUSTOM_SRC
	TYPEC_NEW_STATE(typec_attached_custom_src);
	tcpc->typec_attach_new = TYPEC_ATTACHED_CUSTOM_SRC;

	tcpc->typec_remote_rp_level = typec_get_cc1();

	tcpci_report_power_control(tcpc, true);
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, -1);
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */
}

#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK

static inline uint8_t typec_get_sink_dbg_acc_rp_level(
	int cc1, int cc2)
{
	if (cc2 == TYPEC_CC_VOLT_SNK_DFT)
		return cc1;

	return TYPEC_CC_VOLT_SNK_DFT;
}

static inline void typec_sink_dbg_acc_attached_entry(
	struct tcpc_device *tcpc)
{
	bool polarity;
	uint8_t rp_level;

	uint8_t cc1 = typec_get_cc1();
	uint8_t cc2 = typec_get_cc2();

	if (cc1 == cc2) {
		typec_custom_src_attached_entry(tcpc);
		return;
	}

	TYPEC_NEW_STATE(typec_attached_dbgacc_snk);

	tcpc->typec_attach_new = TYPEC_ATTACHED_DBGACC_SNK;

	polarity = cc2 > cc1;

	if (polarity)
		rp_level = typec_get_sink_dbg_acc_rp_level(cc2, cc1);
	else
		rp_level = typec_get_sink_dbg_acc_rp_level(cc1, cc2);

	typec_set_plug_orient(tcpc, TYPEC_CC_RD, polarity);
	tcpc->typec_remote_rp_level = rp_level;

	tcpci_report_power_control(tcpc, true);
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, -1);
}
#else
static inline void typec_sink_dbg_acc_attached_entry(
	struct tcpc_device *tcpc)
{
	typec_custom_src_attached_entry(tcpc);
}
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */


/*
 * [BLOCK] Try.SRC / TryWait.SNK
 */

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE

static inline bool typec_role_is_try_src(
	struct tcpc_device *tcpc)
{
	if (tcpc->typec_role != TYPEC_ROLE_TRY_SRC)
		return false;

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	if (tcpc->typec_during_role_swap)
		return false;
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

	return true;
}

static inline void typec_try_src_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_try_src);
	tcpc->typec_drp_try_timeout = false;

	tcpci_set_cc(tcpc, TYPEC_CC_RP);
	tcpc_enable_timer(tcpc, TYPEC_TRY_TIMER_DRP_TRY);
}

static inline void typec_trywait_snk_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_trywait_snk);
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	tcpci_set_vconn(tcpc, false);
	tcpci_set_cc(tcpc, TYPEC_CC_RD);
	tcpci_source_vbus(tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
	tcpc_disable_timer(tcpc, TYPEC_TRY_TIMER_DRP_TRY);

	tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
}

static inline void typec_trywait_snk_pe_entry(struct tcpc_device *tcpc)
{
	tcpc->typec_attach_new = TYPEC_UNATTACHED;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc->typec_attach_old) {
		TYPEC_NEW_STATE(typec_trywait_snk_pe);
		return;
	}
#endif

	typec_trywait_snk_entry(tcpc);
}

#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */

/*
 * [BLOCK] Try.SNK / TryWait.SRC
 */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK

static inline bool typec_role_is_try_sink(
	struct tcpc_device *tcpc)
{
	if (tcpc->typec_role != TYPEC_ROLE_TRY_SNK)
		return false;

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	if (tcpc->typec_during_role_swap)
		return false;
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

	return true;
}

static inline void typec_try_snk_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_try_snk);
	tcpc->typec_drp_try_timeout = false;

	tcpci_set_cc(tcpc, TYPEC_CC_RD);
	tcpc_enable_timer(tcpc, TYPEC_TRY_TIMER_DRP_TRY);
}

static inline void typec_trywait_src_entry(struct tcpc_device *tcpc)
{
	TYPEC_NEW_STATE(typec_trywait_src);
	tcpc->typec_drp_try_timeout = false;

	tcpci_set_cc(tcpc, TYPEC_CC_RP);
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_0V, 0);
	tcpc_enable_timer(tcpc, TYPEC_TRY_TIMER_DRP_TRY);
}

#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

/*
 * [BLOCK] Attach / Detach
 */

static inline void typec_cc_snk_detect_vsafe5v_entry(
	struct tcpc_device *tcpc)
{
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	if (!typec_check_cc_any(TYPEC_CC_VOLT_OPEN)) {	/* Both Rp */
		typec_sink_dbg_acc_attached_entry(tcpc);
		return;
	}

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	if (typec_role_is_try_src(tcpc)) {
		if (tcpc->typec_state == typec_attachwait_snk) {
			typec_try_src_entry(tcpc);
			return;
		}
	}
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */

	typec_sink_attached_entry(tcpc);
}

static inline void typec_cc_snk_detect_entry(struct tcpc_device *tcpc)
{
	/* If Port Partner act as Source without VBUS, wait vSafe5V */
	if (tcpci_check_vbus_valid(tcpc))
		typec_cc_snk_detect_vsafe5v_entry(tcpc);
	else
		typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_SNK_VSAFE5V);
}

static inline void typec_cc_src_detect_vsafe0v_entry(
	struct tcpc_device *tcpc)
{
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	if (typec_role_is_try_sink(tcpc)) {
		if (tcpc->typec_state == typec_attachwait_src) {
			typec_try_snk_entry(tcpc);
			return;
		}
	}
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

	typec_source_attached_entry(tcpc);
}

static inline void typec_cc_src_detect_entry(
	struct tcpc_device *tcpc)
{
	/* If Port Partner act as Sink with low VBUS, wait vSafe0v */
	bool vbus_absent = tcpci_check_vsafe0v(tcpc, true);

	if (vbus_absent || tcpc->typec_reach_vsafe0v)
		typec_cc_src_detect_vsafe0v_entry(tcpc);
	else
		typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_SRC_VSAFE0V);
}

static inline void typec_cc_src_remove_entry(struct tcpc_device *tcpc)
{
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	tcpc->typec_is_attached_src = false;

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	if (typec_role_is_try_src(tcpc)) {
		switch (tcpc->typec_state) {
		case typec_attached_src:
			typec_trywait_snk_pe_entry(tcpc);
			return;
		case typec_try_src:
			typec_trywait_snk_entry(tcpc);
			return;
		}
	}
#endif	/* CONFIG_TYPEC_CAP_TRY_SOURCE */

	typec_unattach_wait_pe_idle_entry(tcpc);
}

static inline void typec_cc_snk_remove_entry(struct tcpc_device *tcpc)
{
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	if (tcpc->typec_state == typec_try_snk) {
		typec_trywait_src_entry(tcpc);
		return;
	}
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */

	typec_unattach_wait_pe_idle_entry(tcpc);
}

/*
 * [BLOCK] Check Legacy Cable
 */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE

static inline void typec_legacy_reset_cable_suspect(
	struct tcpc_device *tcpc)
{
#if TCPC_LEGACY_CABLE_SUSPECT_THD
	tcpc->typec_legacy_cable_suspect = 0;
#endif	/* TCPC_LEGACY_CABLE_SUSPECT_THD != 0 */
}

static inline void typec_legacy_reset_retry_wk(
	struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	tcpc->typec_legacy_retry_wk = 0;
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
}

static inline void typec_legacy_enable_discharge(
	struct tcpc_device *tcpc, bool en)
{
#ifdef CONFIG_TYPEC_CAP_FORCE_DISCHARGE
	if (tcpc->tcpc_flags & TCPC_FLAGS_PREFER_LEGACY2) {
		mutex_lock(&tcpc->access_lock);
		tcpci_enable_force_discharge(tcpc, en, 0);
		mutex_unlock(&tcpc->access_lock);
	}
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */
}

static inline void typec_legacy_keep_default_rp(
	struct tcpc_device *tcpc, bool en)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	typec_legacy_enable_discharge(tcpc, en);

	if (en) {
		tcpci_set_cc(tcpc, TYPEC_CC_RD);
		usleep_range(1000, 2000);
		tcpci_set_cc(tcpc, TYPEC_CC_RP);
		usleep_range(1000, 2000);
	}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
}

static inline bool typec_legacy_charge(
	struct tcpc_device *tcpc)
{
	int i, vbus_level = 0;

	TYPEC_INFO("LC->Charge\n");
	tcpci_source_vbus(tcpc,
		TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, 100);

	for (i = 0; i < 6; i++) { /* 275 ms */
		vbus_level = tcpm_inquire_vbus_level(tcpc, true);
		if (vbus_level >= TCPC_VBUS_VALID)
			return true;
		msleep(50);
	}

	TYPEC_INFO("LC->Charge Failed\n");
	return false;
}

static inline bool typec_legacy_discharge(
	struct tcpc_device *tcpc)
{
	int i, vbus_level = 0;

	TYPEC_INFO("LC->Discharge\n");
	tcpci_source_vbus(tcpc,
		TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);

	for (i = 0; i < 6; i++) { /* 275 ms */
		vbus_level = tcpm_inquire_vbus_level(tcpc, true);
		if (vbus_level < TCPC_VBUS_VALID)
			return true;
		msleep(50);
	}

	TYPEC_INFO("LC->Discharge Failed\n");
	return false;
}

static inline bool typec_legacy_suspect(struct tcpc_device *tcpc)
{
	int i = 0, vbus_level = 0;

	TYPEC_INFO("LC->Suspect\n");
	typec_legacy_reset_cable_suspect(tcpc);

	while (1) {
		vbus_level = tcpm_inquire_vbus_level(tcpc, true);
		if (vbus_level < TCPC_VBUS_VALID)
			break;

		i++;
		if (i > 3)	{ /* 150 ms */
			TYPEC_INFO("LC->TAIn\n");
			return false;
		}

		msleep(50);
	};

	tcpci_set_cc(tcpc, TYPEC_CC_RP_1_5);
	usleep_range(1000, 2000);

	return tcpci_get_cc(tcpc) != 0;
}

static inline bool typec_legacy_stable1(struct tcpc_device *tcpc)
{
	typec_legacy_charge(tcpc);
	typec_legacy_discharge(tcpc);
	TYPEC_INFO("LC->Stable\n");
	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_LEGACY_STABLE);

	return true;
}

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2

static inline bool typec_is_run_legacy_stable2(struct tcpc_device *tcpc)
{
	bool run_legacy2;
	uint8_t retry_max = TCPC_LEGACY_CABLE_RETRY_SOLUTION;

	run_legacy2 = tcpc->tcpc_flags & TCPC_FLAGS_PREFER_LEGACY2;

	TYPEC_INFO("LC->Retry%d\n", tcpc->typec_legacy_retry_wk++);

	if (tcpc->typec_legacy_retry_wk <= retry_max)
		return run_legacy2;

	if (tcpc->typec_legacy_retry_wk > (retry_max*2))
		typec_legacy_reset_retry_wk(tcpc);

	return !run_legacy2;
}

static inline bool typec_legacy_stable2(struct tcpc_device *tcpc)
{
	tcpc->typec_legacy_cable = 2;
	TYPEC_INFO("LC->Stable2\n");
	typec_legacy_keep_default_rp(tcpc, true);

	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_LEGACY_STABLE);

#ifdef CONFIG_TYPEC_LEGACY2_AUTO_RECYCLE
	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_LEGACY_RECYCLE);
#endif	/* CONFIG_TYPEC_LEGACY2_AUTO_RECYCLE */

	return true;
}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */

static inline bool typec_legacy_confirm(struct tcpc_device *tcpc)
{
	TYPEC_INFO("LC->Confirm\n");
	tcpc->typec_legacy_cable = 1;
	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_NOT_LEGACY);

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	if (typec_is_run_legacy_stable2(tcpc))
		return typec_legacy_stable2(tcpc);
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */

	return typec_legacy_stable1(tcpc);
}

static inline bool typec_legacy_check_cable(struct tcpc_device *tcpc)
{
	bool check_legacy = false;

	if (tcpc->tcpc_flags & TCPC_FLAGS_DISABLE_LEGACY)
		return false;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	if (tcpc->typec_legacy_cable == 2) {
		typec_unattached_src_and_drp_entry(tcpc);
		return true;
	}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */

	if (typec_check_cc(TYPEC_CC_VOLT_RD, TYPEC_CC_VOLT_OPEN) ||
		typec_check_cc(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_RD))
		check_legacy = true;

#if TCPC_LEGACY_CABLE_SUSPECT_THD
	if (tcpc->typec_legacy_cable_suspect <
					TCPC_LEGACY_CABLE_SUSPECT_THD)
		check_legacy = false;
#endif	/* TCPC_LEGACY_CABLE_SUSPECT_THD */

	if (check_legacy) {
		if (typec_legacy_suspect(tcpc)) {
			typec_legacy_confirm(tcpc);
			return true;
		}

		tcpc->typec_legacy_cable = false;
		tcpci_set_cc(tcpc, TYPEC_CC_RP);
	}

	return false;
}

static inline void typec_legacy_reset_timer(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	if (tcpc->typec_legacy_cable == 2)
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_LEGACY_RECYCLE);

	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_LEGACY_STABLE);
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
}

static inline void typec_legacy_reach_vsafe5v(struct tcpc_device *tcpc)
{
	TYPEC_INFO("LC->Attached\n");
	tcpc->typec_legacy_cable = false;
	tcpci_set_cc(tcpc, TYPEC_CC_RD);
	typec_legacy_reset_timer(tcpc);
}

static inline void typec_legacy_reach_vsafe0v(struct tcpc_device *tcpc)
{
	TYPEC_INFO("LC->Detached (PS)\n");
	tcpc->typec_legacy_cable = false;
	typec_set_drp_toggling(tcpc);
	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_LEGACY_STABLE);
}

static inline void typec_legacy_handle_ps_change(
	struct tcpc_device *tcpc, int vbus_level)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	if (tcpc->typec_legacy_cable != 1)
		return;
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */

	if (vbus_level >= TCPC_VBUS_VALID)
		typec_legacy_reach_vsafe5v(tcpc);
	else if (vbus_level == TCPC_VBUS_SAFE0V)
		typec_legacy_reach_vsafe0v(tcpc);
}

static inline void typec_legacy_handle_detach(struct tcpc_device *tcpc)
{
#if TCPC_LEGACY_CABLE_SUSPECT_THD
	bool suspect_legacy = false;

	if (tcpc->typec_state == typec_attachwait_src)
		suspect_legacy = true;
	else if (tcpc->typec_state == typec_attached_src) {
		if (tcpc->typec_attach_old != TYPEC_ATTACHED_SRC)
			suspect_legacy = true;
	}

	if (suspect_legacy) {
		tcpc->typec_legacy_cable_suspect++;
		TYPEC_INFO2("LC->Suspect: %d\n",
			tcpc->typec_legacy_cable_suspect);
	}
#endif	/* TCPC_LEGACY_CABLE_SUSPECT_THD != 0 */
}

static inline int typec_legacy_handle_cc_open(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	if (tcpc->typec_legacy_cable == 2) {
		typec_legacy_keep_default_rp(tcpc, false);
		return 1;
	}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */

	return 0;
}

static inline int typec_legacy_handle_cc_present(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	return tcpc->typec_legacy_cable == 1;
#else
	return 1;
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
}

static inline int typec_legacy_handle_cc_change(struct tcpc_device *tcpc)
{
	int ret = 0;

	if (typec_is_cc_open() || typec_is_cable_only())
		ret = typec_legacy_handle_cc_open(tcpc);
	else
		ret = typec_legacy_handle_cc_present(tcpc);

	if (ret == 0)
		return 0;

	TYPEC_INFO("LC->Detached (CC)\n");

	tcpc->typec_legacy_cable = false;
	typec_set_drp_toggling(tcpc);
	typec_legacy_reset_timer(tcpc);
	return 1;
}

#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

/*
 * [BLOCK] CC Change (after debounce)
 */

#ifdef CONFIG_TYPEC_CAP_DBGACC
static void typec_debug_acc_attached_with_vbus_entry(
		struct tcpc_device *tcpc)
{
	tcpc->typec_attach_new = TYPEC_ATTACHED_DEBUG;
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);
}
#endif	/* CONFIG_TYPEC_CAP_DBGACC */

static inline void typec_debug_acc_attached_entry(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CAP_DBGACC
	TYPEC_NEW_STATE(typec_debugaccessory);
	TYPEC_DBG("[Debug] CC1&2 Both Rd\n");
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DBG_VSAFE5V);

	tcpci_report_power_control(tcpc, true);
	tcpci_source_vbus(tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, -1);
#endif	/* CONFIG_TYPEC_CAP_DBGACC */
}

#ifdef CONFIG_TYPEC_CAP_AUDIO_ACC_SINK_VBUS
static inline bool typec_audio_acc_sink_vbus(
	struct tcpc_device *tcpc, bool vbus_valid)
{
	if (vbus_valid) {
		tcpci_report_power_control(tcpc, true);
		tcpci_sink_vbus(tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, 500);
	} else {
		tcpci_sink_vbus(tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_0V, 0);
		tcpci_report_power_control(tcpc, false);
	}

	return true;
}
#endif	/* CONFIG_TYPEC_CAP_AUDIO_ACC_SINK_VBUS */

static bool typec_is_fake_ra_rp30(struct tcpc_device *tcpc)
{
	if (TYPEC_CC_PULL_GET_RP_LVL(tcpc->typec_local_cc) == TYPEC_RP_3_0) {
		__tcpci_set_cc(tcpc, TYPEC_CC_RP_DFT);
		usleep_range(1000, 2000);
		return tcpci_get_cc(tcpc) != 0;
	}

	return false;
}

static inline bool typec_audio_acc_attached_entry(struct tcpc_device *tcpc)
{
#ifdef RICHTEK_PD_COMPLIANCE_FAKE_AUDIO_ACC
	if (typec_is_fake_ra_rp30(tcpc)) {
		TYPEC_DBG("[Audio] Fake Both Ra\n");
		if (typec_check_cc_any(TYPEC_CC_VOLT_RD))
			typec_cc_src_detect_entry(tcpc);
		else
			typec_cc_src_remove_entry(tcpc);
		return 0;
	}
#endif	/* RICHTEK_PD_COMPLIANCE_FAKE_AUDIO_ACC */

	TYPEC_NEW_STATE(typec_audioaccessory);
	TYPEC_DBG("[Audio] CC1&2 Both Ra\n");
	tcpc->typec_attach_new = TYPEC_ATTACHED_AUDIO;

#ifdef CONFIG_TYPEC_CAP_AUDIO_ACC_SINK_VBUS
	if (tcpci_check_vbus_valid(tcpc))
		typec_audio_acc_sink_vbus(tcpc, true);
#endif	/* CONFIG_TYPEC_CAP_AUDIO_ACC_SINK_VBUS */

	return true;
}

static inline bool typec_cc_change_source_entry(struct tcpc_device *tcpc)
{
	bool src_remove = false;

	switch (tcpc->typec_state) {
	case typec_attached_src:
		if (typec_get_cc_res() != TYPEC_CC_VOLT_RD)
			src_remove = true;
		break;
	case typec_audioaccessory:
		if (!typec_check_cc_both(TYPEC_CC_VOLT_RA))
			src_remove = true;
		break;
#ifdef CONFIG_TYPEC_CAP_DBGACC
	case typec_debugaccessory:
		if (!typec_check_cc_both(TYPEC_CC_VOLT_RD))
			src_remove = true;
		break;
#endif	/* CONFIG_TYPEC_CAP_DBGACC */
	default:
		if (typec_check_cc_both(TYPEC_CC_VOLT_RD))
			typec_debug_acc_attached_entry(tcpc);
		else if (typec_check_cc_both(TYPEC_CC_VOLT_RA))
			typec_audio_acc_attached_entry(tcpc);
		else if (typec_check_cc_any(TYPEC_CC_VOLT_RD))
			typec_cc_src_detect_entry(tcpc);
		else
			src_remove = true;
		break;
	}

	if (src_remove)
		typec_cc_src_remove_entry(tcpc);

	return true;
}

static inline bool typec_attached_snk_cc_change(struct tcpc_device *tcpc)
{
	uint8_t cc_res = typec_get_cc_res();

	if (cc_res != tcpc->typec_remote_rp_level) {
		TYPEC_INFO("RpLvl Change\n");
		tcpc->typec_remote_rp_level = cc_res;

#ifdef CONFIG_TYPEC_CAP_CUSTOM_HV
		if (tcpc->typec_during_custom_hv)
			return true;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_HV */

		tcpci_sink_vbus(tcpc,
				TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, -1);
	}

	return true;
}

static inline bool typec_cc_change_sink_entry(struct tcpc_device *tcpc)
{
	bool snk_remove = false;

	switch (tcpc->typec_state) {
	case typec_attached_snk:
		if (typec_get_cc_res() == TYPEC_CC_VOLT_OPEN)
			snk_remove = true;
		else
			typec_attached_snk_cc_change(tcpc);
		break;

#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	case typec_attached_dbgacc_snk:
		if (typec_get_cc_res() == TYPEC_CC_VOLT_OPEN)
			snk_remove = true;
		else
			typec_attached_snk_cc_change(tcpc);
		break;
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */

#ifdef CONFIG_TYPEC_CAP_CUSTOM_SRC
	case typec_attached_custom_src:
		if (typec_check_cc_any(TYPEC_CC_VOLT_OPEN))
			snk_remove = true;
		break;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */

	default:
		if (!typec_is_cc_open())
			typec_cc_snk_detect_entry(tcpc);
		else
			snk_remove = true;
	}

	if (snk_remove)
		typec_cc_snk_remove_entry(tcpc);

	return true;
}

bool tcpc_typec_is_act_as_sink_role(struct tcpc_device *tcpc)
{
	bool as_sink = true;
	uint8_t cc_sum;

	switch (TYPEC_CC_PULL_GET_RES(tcpc->typec_local_cc)) {
	case TYPEC_CC_RP:
		as_sink = false;
		break;
	case TYPEC_CC_RD:
		as_sink = true;
		break;
	case TYPEC_CC_DRP:
		cc_sum = typec_get_cc1() + typec_get_cc2();
		as_sink = (cc_sum >= TYPEC_CC_VOLT_SNK_DFT);
		break;
	}

	return as_sink;
}

static inline bool typec_handle_cc_changed_entry(struct tcpc_device *tcpc)
{
	TYPEC_INFO("[CC_Change] %d/%d\n", typec_get_cc1(), typec_get_cc2());

	tcpc->typec_attach_new = tcpc->typec_attach_old;

	if (tcpc_typec_is_act_as_sink_role(tcpc))
		typec_cc_change_sink_entry(tcpc);
	else
		typec_cc_change_source_entry(tcpc);

	typec_alert_attach_state_change(tcpc);
	return true;
}

/*
 * [BLOCK] Handle cc-change event (from HW)
 */

static inline void typec_attach_wait_entry(struct tcpc_device *tcpc)
{
	bool as_sink;
#ifdef CONFIG_USB_POWER_DELIVERY
	struct pd_port *pd_port = &tcpc->pd_port;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	if (tcpc->typec_attach_old == TYPEC_ATTACHED_SNK ||
	    tcpc->typec_attach_old == TYPEC_ATTACHED_DBGACC_SNK) {
#ifdef CONFIG_USB_POWER_DELIVERY
		if (pd_port->pe_data.pd_connected && pd_check_rev30(pd_port))
			pd_put_sink_tx_event(tcpc, typec_get_cc_res());
#endif	/* CONFIG_USB_POWER_DELIVERY */
		tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
		TYPEC_DBG("RpLvl Alert\n");
		return;
	}

	if (tcpc->typec_attach_old ||
		tcpc->typec_state == typec_attached_src) {
		tcpc_reset_typec_debounce_timer(tcpc);
		TYPEC_DBG("Attached, Ignore cc_attach\n");
#ifndef CONFIG_USB_POWER_DELIVERY
		typec_enable_vconn(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */
		return;
	}

	switch (tcpc->typec_state) {

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		tcpc_enable_timer(tcpc, TYPEC_TIMER_TRYCCDEBOUNCE);
		return;

	case typec_trywait_snk:
		tcpc_enable_timer(tcpc, TYPEC_TIMER_CCDEBOUNCE);
		return;
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_try_snk:	/* typec_drp_try_timeout = true */
		tcpc_enable_timer(tcpc, TYPEC_TIMER_TRYCCDEBOUNCE);
		return;

	case typec_trywait_src:	/* typec_drp_try_timeout = unknown */
		tcpc_enable_timer(tcpc, TYPEC_TIMER_TRYCCDEBOUNCE);
		return;
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */

#ifdef CONFIG_USB_POWER_DELIVERY
	case typec_unattachwait_pe:
		TYPEC_INFO("Force PE Idle\n");
		tcpc->pd_wait_pe_idle = false;
		tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_PE_IDLE);
		typec_unattached_power_entry(tcpc);
		break;
#endif
	default:
		break;
	}

	as_sink = tcpc_typec_is_act_as_sink_role(tcpc);

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (!as_sink && typec_legacy_check_cable(tcpc))
		return;
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT
	tcpci_notify_attachwait_state(tcpc, as_sink);
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT */

	if (as_sink)
		TYPEC_NEW_STATE(typec_attachwait_snk);
	else {
		/* Advertise Rp level before Attached.SRC Ellisys 3.1.6359 */
		tcpci_set_cc(tcpc,
			TYPEC_CC_PULL(tcpc->typec_local_rp_level, TYPEC_CC_RP));
		TYPEC_NEW_STATE(typec_attachwait_src);
	}

	tcpc_enable_timer(tcpc, TYPEC_TIMER_CCDEBOUNCE);
}

#ifdef TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS
static inline int typec_attached_snk_cc_detach(struct tcpc_device *tcpc)
{
	tcpc_reset_typec_debounce_timer(tcpc);
#ifdef CONFIG_USB_POWER_DELIVERY
	/*
	 * For Source detach during HardReset,
	 * However Apple TA may keep cc_open about 150 ms during HardReset
	 */
	if (tcpc->pd_wait_hard_reset_complete) {
#ifdef CONFIG_COMPATIBLE_APPLE_TA
		TYPEC_INFO2("Detach_CC (HardReset), compatible apple TA\n");
		tcpc_enable_timer(tcpc, TYPEC_TIMER_APPLE_CC_OPEN);
#else
		TYPEC_INFO2("Detach_CC (HardReset)\n");
		tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
	} else if (tcpc->pd_port.pe_data.pd_prev_connected) {
		TYPEC_INFO2("Detach_CC (PD)\n");
		tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
	}
#endif	/* CONFIG_USB_POWER_DELIVERY */
	return 0;
}
#endif	/* TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS */

static inline void typec_detach_wait_entry(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	typec_legacy_handle_detach(tcpc);
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	switch (tcpc->typec_state) {
#ifdef TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS
	case typec_attached_snk:
#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	case typec_attached_dbgacc_snk:
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */
		typec_attached_snk_cc_detach(tcpc);
		break;
#endif /* TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS */

	case typec_attached_src:
		tcpc_enable_timer(tcpc, TYPEC_TIMER_SRCDISCONNECT);
		break;

	case typec_audioaccessory:
		tcpc_enable_timer(tcpc, TYPEC_TIMER_CCDEBOUNCE);
		break;

#ifdef TYPEC_EXIT_ATTACHED_SRC_NO_DEBOUNCE
	case typec_attached_src:
		TYPEC_INFO("Exit Attached.SRC immediately\n");
		tcpc_reset_typec_debounce_timer(tcpc);

		/* force to terminate TX */
		tcpci_init(tcpc, true);

		typec_cc_src_remove_entry(tcpc);
		typec_alert_attach_state_change(tcpc);
		break;
#endif /* TYPEC_EXIT_ATTACHED_SRC_NO_DEBOUNCE */

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		if (tcpc->typec_drp_try_timeout)
			tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
		else {
			tcpc_reset_typec_debounce_timer(tcpc);
			TYPEC_DBG("[Try] Ignore cc_detach\n");
		}
		break;
#endif	/* CONFIG_TYPEC_CAP_TRY_SOURCE */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_trywait_src:
		if (tcpc->typec_drp_try_timeout)
			tcpc_enable_timer(tcpc, TYPEC_TIMER_TRYCCDEBOUNCE);
		else {
			tcpc_reset_typec_debounce_timer(tcpc);
			TYPEC_DBG("[Try] Ignore cc_detach\n");
		}
		break;
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */
	default:
		tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
		break;
	}
}

static inline bool typec_is_cc_attach(struct tcpc_device *tcpc)
{
	bool cc_attach = false;
	int cc1 = typec_get_cc1();
	int cc2 = typec_get_cc2();
	int cc_res = typec_get_cc_res();

	tcpc->typec_cable_only = false;

#ifdef RICHTEK_PD_COMPLIANCE_FAKE_RA_DETACH
	if (tcpc->typec_attach_old == TYPEC_ATTACHED_SRC
		&& (cc_res == TYPEC_CC_VOLT_RA) &&
		(tcpc->typec_local_cc == TYPEC_CC_RP_DFT)) {

		tcpci_set_cc(tcpc, TYPEC_CC_RP_1_5);
		usleep_range(1000, 2000);

		if (tcpci_get_cc(tcpc)) {
			TYPEC_DBG("[Detach] Fake Ra\n");
			cc1 = typec_get_cc1();
			cc2 = typec_get_cc2();
			cc_res = typec_get_cc_res();
		}
	}
#endif	/* RICHTEK_PD_COMPLIANCE_FAKE_RA_DETACH */
	switch (tcpc->typec_attach_old) {
	case TYPEC_ATTACHED_SNK:
	case TYPEC_ATTACHED_SRC:
		if ((cc_res != TYPEC_CC_VOLT_OPEN) &&
				(cc_res != TYPEC_CC_VOLT_RA))
			cc_attach = true;
		break;
#ifdef CONFIG_TYPEC_CAP_CUSTOM_SRC
	case TYPEC_ATTACHED_CUSTOM_SRC:
		if ((cc_res != TYPEC_CC_VOLT_OPEN) &&
				(cc_res != TYPEC_CC_VOLT_RA))
			cc_attach = true;
		break;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_SRC */

#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	case TYPEC_ATTACHED_DBGACC_SNK:
		if ((cc_res != TYPEC_CC_VOLT_OPEN) &&
				(cc_res != TYPEC_CC_VOLT_RA))
			cc_attach = true;
		break;
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */
	case TYPEC_ATTACHED_AUDIO:
		if (typec_check_cc_both(TYPEC_CC_VOLT_RA))
			cc_attach = true;
		break;

#ifdef CONFIG_TYPEC_CAP_DBGACC
	case TYPEC_ATTACHED_DEBUG:
		if (typec_check_cc_both(TYPEC_CC_VOLT_RD))
			cc_attach = true;
		break;
#endif	/* CONFIG_TYPEC_CAP_DBGACC */
	default:	/* TYPEC_UNATTACHED */
		if (cc1 != TYPEC_CC_VOLT_OPEN)
			cc_attach = true;

		if (cc2 != TYPEC_CC_VOLT_OPEN)
			cc_attach = true;

		/* Cable Only, no device */
		if ((cc1+cc2) == TYPEC_CC_VOLT_RA) {
#ifdef RICHTEK_PD_COMPLIANCE_FAKE_EMRAK_ONLY
			if (typec_is_fake_ra_rp30(tcpc)) {
				TYPEC_DBG("[Cable] Fake Ra\n");
				if ((cc1+cc2) == TYPEC_CC_VOLT_RD)
					cc_attach = true;
				break;
			}
#endif	/* RICHTEK_PD_COMPLIANCE_FAKE_EMRAK_ONLY */
			cc_attach = false;
			tcpc->typec_cable_only = true;
			TYPEC_DBG("[Cable] Ra Only\n");
		}
		break;
	}

	return cc_attach;
}

/**
 * typec_check_false_ra_detach
 *
 * Check the Single Ra resistance (eMark) exists or not when
 *	1) Ra_detach INT triggered.
 *	2) Wakeup_Timer triggered.
 *
 * If reentering low-power mode and eMark still exists,
 * it may cause an infinite loop.
 *
 * If the CC status is both open, return true; otherwise return false
 *
 */

static inline bool typec_check_false_ra_detach(struct tcpc_device *tcpc)
{
	bool drp = tcpc->typec_role >= TYPEC_ROLE_DRP;

	/*
	 * If the DUT is DRP and current CC status has stopped toggle,
	 * let cc_handler to handle it later. (after debounce)
	 *
	 * If CC is toggling, force CC to present Rp.
	 */

	if (drp) {
		tcpci_get_cc(tcpc);

		if (!typec_is_drp_toggling()) {
			TYPEC_DBG("False_RaDetach1 (%d, %d)\n",
				typec_get_cc1(), typec_get_cc2());
			return true;
		}

		tcpci_set_cc(tcpc, TYPEC_CC_RP);
		usleep_range(1000, 2000);
	}

	/*
	 * Check the CC status
	 * Rd (device) -> let cc_handler to handle it later
	 * eMark Only -> Reschedule wakeup timer
	 *
	 * Open -> (true condition)
	 * Ready to reenter low-power mode.
	 * If we repeatedly enter this situation,
	 * it will trigger low rp duty protection.
	 */

	tcpci_get_cc(tcpc);

	if (typec_is_cc_open())
		tcpc->typec_cable_only = false;
	else if (typec_get_cc1() + typec_get_cc2() == TYPEC_CC_VOLT_RA) {
		tcpc->typec_cable_only = true;
		TYPEC_DBG("False_RaDetach2 (eMark)\n");
	} else {
		tcpc->typec_cable_only = false;
		TYPEC_DBG("False_RaDetach3 (%d, %d)\n",
			typec_get_cc1(), typec_get_cc2());
		return true;
	}

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	if (tcpc->typec_cable_only &&
		tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		tcpc_enable_wakeup_timer(tcpc, true);
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

#ifdef CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY
	if (!tcpc->typec_cable_only) {
		if (tcpc->typec_low_rp_duty_cntdown)
			tcpci_set_low_rp_duty(tcpc, true);
		else {
			tcpc->typec_wakeup_once = false;
			tcpc->typec_low_rp_duty_cntdown = true;
		}
	}
#endif	/* CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY */

	/*
	 * If the DUT is DRP, force CC to toggle again.
	 */

	if (drp) {
		tcpci_set_cc(tcpc, TYPEC_CC_DRP);
		tcpci_alert_status_clear(tcpc,
			TCPC_REG_ALERT_EXT_RA_DETACH);
	}

	return tcpc->typec_cable_only;
}

int tcpc_typec_enter_lpm_again(struct tcpc_device *tcpc)
{
	bool check_ra = (tcpc->typec_lpm) || (tcpc->typec_cable_only);

	if (check_ra && typec_check_false_ra_detach(tcpc))
		return 0;

	TYPEC_DBG("RetryLPM\n");

	tcpc->typec_lpm = true;

	tcpci_set_low_power_mode(tcpc, true,
		(tcpc->typec_role !=  TYPEC_ROLE_SRC) ?
		TYPEC_CC_DRP : TYPEC_CC_RP);

	return 0;
}

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
static inline int typec_handle_try_sink_cc_change(
	struct tcpc_device *tcpc)
{
	/*
	 * The port shall wait for tDRPTry and only then begin
	 * begin monitoring the CC1 and CC2 pins for the SNK.Rp state
	 */

	if (!tcpc->typec_drp_try_timeout) {
		TYPEC_DBG("[Try.SNK] Ignore CC_Alert\n");
		return 1;
	}

	if (!typec_is_cc_open()) {
		tcpci_notify_attachwait_state(tcpc, true);
		return 0;
	}

	return 0;
}
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */

static inline int typec_get_rp_present_flag(struct tcpc_device *tcpc)
{
	uint8_t rp_flag = 0;

	if (tcpc->typec_remote_cc[0] >= TYPEC_CC_VOLT_SNK_DFT
		&& tcpc->typec_remote_cc[0] != TYPEC_CC_DRP_TOGGLING)
		rp_flag |= 1;

	if (tcpc->typec_remote_cc[1] >= TYPEC_CC_VOLT_SNK_DFT
		&& tcpc->typec_remote_cc[1] != TYPEC_CC_DRP_TOGGLING)
		rp_flag |= 2;

	return rp_flag;
}

static bool typec_is_cc_open_state(struct tcpc_device *tcpc)
{
	if (tcpc->typec_state == typec_errorrecovery)
		return true;

	if (tcpc->typec_state == typec_disabled)
		return true;

#ifdef CONFIG_WATER_DETECTION
	if ((tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) &&
	    (tcpc->typec_state == typec_water_protection_wait ||
	    tcpc->typec_state == typec_water_protection))
		return true;
#endif /* CONFIG_WATER_DETECTION */

	return false;
}

static inline bool typec_is_ignore_cc_change(
	struct tcpc_device *tcpc, uint8_t rp_present)
{
	if (typec_is_cc_open_state(tcpc))
		return true;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (tcpc->typec_legacy_cable &&
		typec_legacy_handle_cc_change(tcpc)) {
		return true;
	}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc->typec_state == typec_attachwait_snk &&
		typec_get_rp_present_flag(tcpc) == rp_present) {
		TYPEC_DBG("[AttachWait] Ignore RpLvl Alert\n");
		return true;
	}

	if (tcpc->pd_wait_pr_swap_complete) {
		TYPEC_DBG("[PR.Swap] Ignore CC_Alert\n");
		return true;
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	if (tcpc->typec_state == typec_try_snk) {
		if (typec_handle_try_sink_cc_change(tcpc) > 0)
			return true;
	}

	if (tcpc->typec_state == typec_trywait_src_pe) {
		TYPEC_DBG("[Try.PE] Ignore CC_Alert\n");
		return true;
	}
#endif	/* CONFIG_TYPEC_CAP_TRY_SINK */

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	if (tcpc->typec_state == typec_trywait_snk_pe) {
		TYPEC_DBG("[Try.PE] Ignore CC_Alert\n");
		return true;
	}
#endif	/* CONFIG_TYPEC_CAP_TRY_SOURCE */

	return false;
}

int tcpc_typec_handle_cc_change(struct tcpc_device *tcpc)
{
	int ret;
	uint8_t rp_present;

#ifdef CONFIG_WATER_DETECTION
	/* For ellisys rp/rp to rp/open */
	u8 typec_state_old = tcpc->typec_state;
#endif /* CONFIG_WATER_DETECTION */

	rp_present = typec_get_rp_present_flag(tcpc);

	ret = tcpci_get_cc(tcpc);
	if (ret < 0)
		return ret;

	TYPEC_INFO("[CC_Alert] %d/%d\n", typec_get_cc1(), typec_get_cc2());

	if (typec_is_cc_no_res()) {
		TYPEC_DBG("[Warning] CC No Res\n");
		if (tcpc->typec_lpm && !tcpc->typec_cable_only)
			typec_enter_low_power_mode(tcpc);
		if (typec_is_drp_toggling())
			return 0;
	}

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	if (typec_try_exit_norp_src(tcpc))
		return 0;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

	if (typec_is_ignore_cc_change(tcpc, rp_present))
		return 0;

	if (tcpc->typec_state == typec_attachwait_snk
		|| tcpc->typec_state == typec_attachwait_src)
		typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	if (typec_is_cc_attach(tcpc)) {
		typec_disable_low_power_mode(tcpc);
		typec_attach_wait_entry(tcpc);
#ifdef CONFIG_WATER_DETECTION
		if (typec_state_old == typec_unattached_snk ||
		    typec_state_old == typec_unattached_src) {
#ifdef CONFIG_WD_POLLING_ONLY
			if (tcpc->bootmode == 8 || tcpc->bootmode == 9)
				typec_check_water_status(tcpc);
#else
			typec_check_water_status(tcpc);
#endif /* CONFIG_WD_POLLING_ONLY */
		}
#endif /* CONFIG_WATER_DETECTION */
	} else
		typec_detach_wait_entry(tcpc);

	return 0;
}

/*
 * [BLOCK] Handle timeout event
 */

#ifdef CONFIG_TYPEC_CAP_TRY_STATE
static inline int typec_handle_drp_try_timeout(struct tcpc_device *tcpc)
{
	bool src_detect = false, en_timer;

	tcpc->typec_drp_try_timeout = true;
	tcpc_disable_timer(tcpc, TYPEC_TRY_TIMER_DRP_TRY);

	if (typec_is_drp_toggling()) {
		TYPEC_DBG("[Warning] DRP Toggling\n");
		return 0;
	}

	src_detect = typec_check_cc_any(TYPEC_CC_VOLT_RD);

	switch (tcpc->typec_state) {
#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		en_timer = !src_detect;
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_trywait_src:
		en_timer = !src_detect;
		break;

	case typec_try_snk:
		en_timer = true;
		if (!typec_is_cc_open())
			tcpci_notify_attachwait_state(tcpc, true);
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

	default:
		en_timer = false;
		break;
	}

	if (en_timer)
		tcpc_enable_timer(tcpc, TYPEC_TIMER_TRYCCDEBOUNCE);

	return 0;
}
#endif	/* CONFIG_TYPEC_CAP_TRY_STATE */

static inline int typec_handle_debounce_timeout(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	if (typec_is_cc_no_res() && tcpci_check_vbus_valid(tcpc)
		&& (tcpc->typec_state == typec_unattached_snk))
		return typec_norp_src_attached_entry(tcpc);
#endif

	if (typec_is_drp_toggling()) {
		TYPEC_DBG("[Warning] DRP Toggling\n");
		return 0;
	}

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_STATE_CHANGE);
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	typec_handle_cc_changed_entry(tcpc);
	return 0;
}

static inline int typec_handle_error_recovery_timeout(
						struct tcpc_device *tcpc)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpc->pd_wait_pe_idle = false;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	typec_unattach_wait_pe_idle_entry(tcpc);
	typec_alert_attach_state_change(tcpc);
	return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static inline int typec_handle_pe_idle(struct tcpc_device *tcpc)
{
	switch (tcpc->typec_state) {

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_trywait_snk_pe:
		typec_trywait_snk_entry(tcpc);
		break;
#endif

	case typec_unattachwait_pe:
		typec_unattached_entry(tcpc);
		break;

	default:
		TYPEC_DBG("Dummy pe_idle\n");
		break;
	}

	return 0;
}

#ifdef CONFIG_USB_PD_WAIT_BC12
static inline void typec_handle_pd_wait_bc12(struct tcpc_device *tcpc)
{
	int ret = 0;
	uint8_t type = TYPEC_UNATTACHED;
	union power_supply_propval val = {.intval = 0};

	mutex_lock(&tcpc->access_lock);

	type = tcpc->typec_attach_new;
	ret = power_supply_get_property(tcpc->chg_psy,
		POWER_SUPPLY_PROP_USB_TYPE, &val);
	TYPEC_INFO("type=%d, ret,chg_type=%d,%d, count=%d\n", type,
		ret, val.intval, tcpc->pd_wait_bc12_count);

	if (type != TYPEC_ATTACHED_SNK && type != TYPEC_ATTACHED_DBGACC_SNK)
		goto out;

	if ((ret >= 0 && val.intval != POWER_SUPPLY_USB_TYPE_UNKNOWN) ||
		tcpc->pd_wait_bc12_count >= 20) {
		__pd_put_cc_attached_event(tcpc, type);
	} else {
		tcpc->pd_wait_bc12_count++;
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_PD_WAIT_BC12);
	}
out:
	mutex_unlock(&tcpc->access_lock);
}
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif /* CONFIG_USB_POWER_DELIVERY */

static inline int typec_handle_src_reach_vsafe0v(struct tcpc_device *tcpc)
{
	if (typec_is_drp_toggling()) {
		TYPEC_DBG("[Warning] DRP Toggling\n");
		return 0;
	}

	tcpc->typec_reach_vsafe0v = true;
	typec_cc_src_detect_vsafe0v_entry(tcpc);
	typec_alert_attach_state_change(tcpc);
	return 0;
}

static inline int typec_handle_src_toggle_timeout(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	if (tcpc->typec_during_role_swap)
		return 0;
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

	if (tcpc->typec_state == typec_unattached_src) {
		typec_unattached_snk_and_drp_entry(tcpc);
		typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		typec_try_enter_norp_src(tcpc);
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	}

	return 0;
}

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
static inline int typec_handle_role_swap_start(struct tcpc_device *tcpc)
{
	uint8_t role_swap = tcpc->typec_during_role_swap;

	if (role_swap == TYPEC_ROLE_SWAP_TO_SNK) {
		TYPEC_INFO("Role Swap to Sink\n");
		tcpci_set_cc(tcpc, TYPEC_CC_RD);
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_ROLE_SWAP_STOP);
	} else if (role_swap == TYPEC_ROLE_SWAP_TO_SRC) {
		TYPEC_INFO("Role Swap to Source\n");
		tcpci_set_cc(tcpc, TYPEC_CC_RP);
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_ROLE_SWAP_STOP);
	}

	return 0;
}

static inline int typec_handle_role_swap_stop(struct tcpc_device *tcpc)
{
	if (tcpc->typec_during_role_swap) {
		TYPEC_INFO("TypeC Role Swap Failed\n");
		tcpc->typec_during_role_swap = TYPEC_ROLE_SWAP_NONE;
		tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
	}

	return 0;
}
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

int tcpc_typec_handle_timeout(struct tcpc_device *tcpc, uint32_t timer_id)
{
	int ret = 0;

#ifdef CONFIG_TYPEC_CAP_TRY_STATE
	if (timer_id == TYPEC_TRY_TIMER_DRP_TRY)
		return typec_handle_drp_try_timeout(tcpc);
#endif	/* CONFIG_TYPEC_CAP_TRY_STATE */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (timer_id == TYPEC_TIMER_DRP_SRC_TOGGLE &&
		(tcpc->typec_state != typec_unattached_src)) {
		TCPC_DBG("Dummy SRC_TOGGLE\n");
		return 0;
	}
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	if (timer_id >= TYPEC_TIMER_START_ID)
		tcpc_reset_typec_debounce_timer(tcpc);
	else if (timer_id >= TYPEC_RT_TIMER_START_ID)
		tcpc_disable_timer(tcpc, timer_id);

	if (timer_id == TYPEC_TIMER_ERROR_RECOVERY)
		return typec_handle_error_recovery_timeout(tcpc);
	else if (timer_id == TYPEC_RT_TIMER_STATE_CHANGE)
		return typec_alert_attach_state_change(tcpc);
	else if (typec_is_cc_open_state(tcpc)) {
		TYPEC_DBG("[Open] Ignore timer_evt\n");
		return 0;
	}

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc->pd_wait_pr_swap_complete) {
		TYPEC_DBG("[PR.Swap] Ignore timer_evt\n");
		return 0;
	}
#endif	/* CONFIG_USB_POWER_DELIVERY */

	switch (timer_id) {
#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_COMPATIBLE_APPLE_TA
	case TYPEC_TIMER_APPLE_CC_OPEN:
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
#endif	/* CONFIG_USB_POWER_DELIVERY */
	case TYPEC_TIMER_CCDEBOUNCE:
	case TYPEC_TIMER_PDDEBOUNCE:
	case TYPEC_TIMER_TRYCCDEBOUNCE:
	case TYPEC_TIMER_SRCDISCONNECT:
		/* fall through */
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	case TYPEC_TIMER_NORP_SRC:
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
		ret = typec_handle_debounce_timeout(tcpc);
		break;

#ifdef CONFIG_USB_POWER_DELIVERY
	case TYPEC_RT_TIMER_PE_IDLE:
		ret = typec_handle_pe_idle(tcpc);
		break;
#ifdef CONFIG_USB_PD_WAIT_BC12
	case TYPEC_RT_TIMER_PD_WAIT_BC12:
		typec_handle_pd_wait_bc12(tcpc);
		break;
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_DELAY
	case TYPEC_RT_TIMER_SAFE0V_DELAY:
		ret = typec_handle_src_reach_vsafe0v(tcpc);
		break;
#endif	/* CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_DELAY */

	case TYPEC_RT_TIMER_LOW_POWER_MODE:
		if (tcpc->typec_lpm)
			typec_try_low_power_mode(tcpc);
		break;

#ifdef CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_TIMEOUT
	case TYPEC_RT_TIMER_SAFE0V_TOUT:
		TCPC_INFO("VSafe0V TOUT (%d)\n", tcpc->vbus_level);

		if (!tcpci_check_vbus_valid_from_ic(tcpc))
			ret = tcpc_typec_handle_vsafe0v(tcpc);
		break;
#endif	/* CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_TIMEOUT */

	case TYPEC_TIMER_DRP_SRC_TOGGLE:
		ret = typec_handle_src_toggle_timeout(tcpc);
		break;

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	case TYPEC_RT_TIMER_ROLE_SWAP_START:
		typec_handle_role_swap_start(tcpc);
		break;

	case TYPEC_RT_TIMER_ROLE_SWAP_STOP:
		typec_handle_role_swap_stop(tcpc);
		break;
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

	case TYPEC_RT_TIMER_DISCHARGE:
		if (!tcpc->typec_power_ctrl) {
			mutex_lock(&tcpc->access_lock);
			tcpci_enable_auto_discharge(tcpc, false);
			tcpci_enable_force_discharge(tcpc, false, 0);
			mutex_unlock(&tcpc->access_lock);
		}
		break;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	case TYPEC_RT_TIMER_NOT_LEGACY:
		tcpc->typec_legacy_cable = false;
		typec_legacy_reset_retry_wk(tcpc);
		typec_legacy_reset_cable_suspect(tcpc);
		break;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	case TYPEC_RT_TIMER_LEGACY_STABLE:
		if (tcpc->typec_legacy_cable)
			tcpc->typec_legacy_retry_wk--;
		break;

#ifdef CONFIG_TYPEC_LEGACY2_AUTO_RECYCLE
	case TYPEC_RT_TIMER_LEGACY_RECYCLE:
		if (tcpc->typec_legacy_cable == 2) {
			TYPEC_INFO("LC->Recycle\n");
			tcpc->typec_legacy_cable = false;
			typec_legacy_keep_default_rp(tcpc, false);
			typec_set_drp_toggling(tcpc);
		}
		break;
#endif	/* CONFIG_TYPEC_LEGACY2_AUTO_RECYCLE */
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */
	}

	return ret;
}

/*
 * [BLOCK] Handle ps-change event
 */

static inline int typec_handle_vbus_present(struct tcpc_device *tcpc)
{
	switch (tcpc->typec_wait_ps_change) {
	case TYPEC_WAIT_PS_SNK_VSAFE5V:
		typec_cc_snk_detect_vsafe5v_entry(tcpc);
		typec_alert_attach_state_change(tcpc);
		break;
	case TYPEC_WAIT_PS_SRC_VSAFE5V:
		typec_source_attached_with_vbus_entry(tcpc);

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
		if (typec_get_cc_res() != TYPEC_CC_VOLT_RD) {
			typec_postpone_state_change(tcpc);
			break;
		}
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

		typec_alert_attach_state_change(tcpc);
		break;
#ifdef CONFIG_TYPEC_CAP_DBGACC
	case TYPEC_WAIT_PS_DBG_VSAFE5V:
		typec_debug_acc_attached_with_vbus_entry(tcpc);
		typec_alert_attach_state_change(tcpc);
		break;
#endif	/* CONFIG_TYPEC_CAP_DBGACC */
	}

	return 0;
}

static inline int typec_attached_snk_vbus_absent(struct tcpc_device *tcpc)
{
#ifdef TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS
#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	if (tcpc->pd_during_direct_charge &&
		!tcpci_check_vsafe0v(tcpc, true)) {
		TYPEC_DBG("Ignore vbus_absent(snk), DirectCharge\n");
		return 0;
	}
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

	if (tcpc->pd_wait_hard_reset_complete) {
#ifdef CONFIG_COMPATIBLE_APPLE_TA
		TYPEC_DBG("Ignore vbus_absent(snk) and CC, HReset(apple)\n");
		return 0;
#else
		if (typec_get_cc_res() != TYPEC_CC_VOLT_OPEN) {
			TYPEC_DBG(
				 "Ignore vbus_absent(snk), HReset & CC!=0\n");
			return 0;
		}
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

	typec_unattach_wait_pe_idle_entry(tcpc);
	typec_alert_attach_state_change(tcpc);
#endif /* TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS */

	return 0;
}


static inline int typec_handle_vbus_absent(struct tcpc_device *tcpc)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc->pd_wait_pr_swap_complete) {
		TYPEC_DBG("[PR.Swap] Ignore vbus_absent\n");
		return 0;
	}
#endif	/* CONFIG_USB_POWER_DELIVERY */

	switch (tcpc->typec_state) {
	case typec_attached_snk:
#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	case typec_attached_dbgacc_snk:
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */
		typec_attached_snk_vbus_absent(tcpc);
		break;
	default:
		break;
	}

#ifndef CONFIG_TCPC_VSAFE0V_DETECT
	tcpc_typec_handle_vsafe0v(tcpc);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT */

	return 0;
}

int tcpc_typec_handle_ps_change(struct tcpc_device *tcpc, int vbus_level)
{
	tcpc->typec_reach_vsafe0v = false;

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (tcpc->typec_legacy_cable) {
		typec_legacy_handle_ps_change(tcpc, vbus_level);
		return 0;
	}
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	if (!typec_try_enter_norp_src(tcpc))
		if (typec_try_exit_norp_src(tcpc))
			return 0;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

	if (typec_is_cc_no_res()) {
		TYPEC_DBG("[Warning] CC No Res\n");
		if (tcpc->typec_lpm && !tcpc->typec_cable_only)
			typec_enter_low_power_mode(tcpc);
		if (typec_is_drp_toggling())
			return 0;
	}

#ifdef CONFIG_TYPEC_CAP_AUDIO_ACC_SINK_VBUS
	if (tcpc->typec_state == typec_audioaccessory) {
		return typec_audio_acc_sink_vbus(
			tcpc, vbus_level >= TCPC_VBUS_VALID);
	}
#endif	/* CONFIG_TYPEC_CAP_AUDIO_ACC_SINK_VBUS */

	if (vbus_level >= TCPC_VBUS_VALID)
		return typec_handle_vbus_present(tcpc);

	return typec_handle_vbus_absent(tcpc);
}

/*
 * [BLOCK] Handle PE event
 */

#ifdef CONFIG_USB_POWER_DELIVERY

int tcpc_typec_handle_pe_pr_swap(struct tcpc_device *tcpc)
{
	int ret = 0;

	mutex_lock(&tcpc->typec_lock);
	switch (tcpc->typec_state) {
	case typec_attached_snk:
		TYPEC_NEW_STATE(typec_attached_src);
		tcpc->typec_is_attached_src = true;
		tcpc->typec_attach_new = TYPEC_ATTACHED_SRC;
		tcpci_set_cc(tcpc,
			TYPEC_CC_PULL(tcpc->typec_local_rp_level, TYPEC_CC_RP));
		break;
	case typec_attached_src:
		TYPEC_NEW_STATE(typec_attached_snk);
		tcpc->typec_is_attached_src = false;
		tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
		tcpci_set_cc(tcpc, TYPEC_CC_RD);
		break;
	default:
		break;
	}

	typec_alert_attach_state_change(tcpc);
	mutex_unlock(&tcpc->typec_lock);
	return ret;
}

#endif /* CONFIG_USB_POWER_DELIVERY */

/*
 * [BLOCK] Handle reach vSafe0V event
 */

int tcpc_typec_handle_vsafe0v(struct tcpc_device *tcpc)
{
#ifdef CONFIG_WATER_DETECTION
	if ((tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) &&
	    tcpc->typec_state == typec_water_protection_wait) {
		TYPEC_NEW_STATE(typec_water_protection);
		tcpci_set_water_protection(tcpc, true);
		return 0;
	}
#endif /* CONFIG_WATER_DETECTION */

	if (tcpc->typec_wait_ps_change == TYPEC_WAIT_PS_SRC_VSAFE0V) {
#ifdef CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_DELAY
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_SAFE0V_DELAY);
#else
		typec_handle_src_reach_vsafe0v(tcpc);
#endif
	}

	return 0;
}

/*
 * [BLOCK] TCPCI TypeC I/F
 */

#if TYPEC_INFO_ENABLE
static const char *const typec_role_name[] = {
	"UNKNOWN",
	"SNK",
	"SRC",
	"DRP",
	"TrySRC",
	"TrySNK",
};
#endif /* TYPEC_INFO_ENABLE */

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
int tcpc_typec_swap_role(struct tcpc_device *tcpc)
{
	if (tcpc->typec_role < TYPEC_ROLE_DRP)
		return TCPM_ERROR_NOT_DRP_ROLE;

	if (tcpc->typec_during_role_swap)
		return TCPM_ERROR_DURING_ROLE_SWAP;

	switch (tcpc->typec_attach_old) {
	case TYPEC_ATTACHED_SNK:
		tcpc->typec_during_role_swap = TYPEC_ROLE_SWAP_TO_SRC;
		break;
	case TYPEC_ATTACHED_SRC:
		tcpc->typec_during_role_swap = TYPEC_ROLE_SWAP_TO_SNK;
		break;
	}

	if (tcpc->typec_during_role_swap) {
		TYPEC_INFO("TypeC Role Swap Start\n");
		tcpci_set_cc(tcpc, TYPEC_CC_OPEN);
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_ROLE_SWAP_START);
		return TCPM_SUCCESS;
	}

	return TCPM_ERROR_UNATTACHED;
}
#endif /* CONFIG_TYPEC_CAP_ROLE_SWAP */

int tcpc_typec_set_rp_level(struct tcpc_device *tcpc, uint8_t rp_lvl)
{
	switch (rp_lvl) {
	case TYPEC_RP_DFT:
	case TYPEC_RP_1_5:
	case TYPEC_RP_3_0:
		TYPEC_INFO("TypeC-Rp: %d\n", rp_lvl);
		tcpc->typec_local_rp_level = rp_lvl;
		break;
	default:
		TYPEC_INFO("TypeC-Unknown-Rp (%d)\n", rp_lvl);
		return -EINVAL;
	}

	return 0;
}

int tcpc_typec_error_recovery(struct tcpc_device *tcpc)
{
	if (tcpc->typec_state != typec_errorrecovery)
		typec_error_recovery_entry(tcpc);

	return 0;
}

int tcpc_typec_disable(struct tcpc_device *tcpc)
{
	if (tcpc->typec_state != typec_disabled)
		typec_disable_entry(tcpc);

	return 0;
}

int tcpc_typec_enable(struct tcpc_device *tcpc)
{
	if (tcpc->typec_state == typec_disabled)
		typec_unattached_entry(tcpc);

	return 0;
}

int tcpc_typec_change_role(
	struct tcpc_device *tcpc, uint8_t typec_role, bool postpone)
{
	if (typec_role == TYPEC_ROLE_UNKNOWN ||
		typec_role >= TYPEC_ROLE_NR) {
		TYPEC_INFO("Wrong TypeC-Role: %d\n", typec_role);
		return -EINVAL;
	}

	if (tcpc->typec_role_new == typec_role) {
		TYPEC_INFO("typec_new_role: %s is the same\n",
			typec_role_name[typec_role]);
		return 0;
	}
	tcpc->typec_role_new = typec_role;

	TYPEC_INFO("typec_new_role: %s\n", typec_role_name[typec_role]);

	if (!postpone || tcpc->typec_attach_old == TYPEC_UNATTACHED)
		return tcpc_typec_error_recovery(tcpc);
	else
		return 0;
}

#ifdef CONFIG_TYPEC_CAP_POWER_OFF_CHARGE
static int typec_init_power_off_charge(struct tcpc_device *tcpc)
{
	bool cc_open;
	int ret = tcpci_get_cc(tcpc);

	if (ret < 0)
		return ret;

	if (tcpc->typec_role == TYPEC_ROLE_SRC)
		return 0;

	cc_open = typec_is_cc_open();

#ifndef CONFIG_TYPEC_CAP_NORP_SRC
	if (cc_open)
		return 0;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

	if (!tcpci_check_vbus_valid(tcpc))
		return 0;

	TYPEC_INFO2("PowerOffCharge\n");

	TYPEC_NEW_STATE(typec_unattached_snk);
	typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_DISABLE);

	tcpci_set_cc(tcpc, TYPEC_CC_DRP);
	typec_enable_low_power_mode(tcpc, TYPEC_CC_DRP);
	usleep_range(1000, 2000);

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	if (cc_open) {
		tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);
		return 1;
	}
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

	tcpci_set_cc(tcpc, TYPEC_CC_RD);

	return 1;
}
#endif	/* CONFIG_TYPEC_CAP_POWER_OFF_CHARGE */

int tcpc_typec_init(struct tcpc_device *tcpc, uint8_t typec_role)
{
	int ret = 0;

	if (typec_role == TYPEC_ROLE_UNKNOWN ||
		typec_role >= TYPEC_ROLE_NR) {
		TYPEC_INFO("Wrong TypeC-Role: %d\n", typec_role);
		return -EINVAL;
	}

	TYPEC_INFO("typec_init: %s\n", typec_role_name[typec_role]);

	tcpc->typec_role = typec_role;
	tcpc->typec_role_new = typec_role;
	tcpc->typec_attach_new = TYPEC_UNATTACHED;
	tcpc->typec_attach_old = TYPEC_UNATTACHED;

	tcpc->typec_remote_cc[0] = TYPEC_CC_VOLT_OPEN;
	tcpc->typec_remote_cc[1] = TYPEC_CC_VOLT_OPEN;

	mutex_lock(&tcpc->access_lock);
	tcpc->wake_lock_pd = 0;
	tcpc->wake_lock_user = true;
	mutex_unlock(&tcpc->access_lock);
	tcpc->typec_usb_sink_curr = CONFIG_TYPEC_SNK_CURR_DFT;

#ifdef CONFIG_TYPEC_CAP_CUSTOM_HV
	tcpc->typec_during_custom_hv = false;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_HV */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	tcpc->typec_legacy_cable = false;
	typec_legacy_reset_retry_wk(tcpc);
	typec_legacy_reset_cable_suspect(tcpc);
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_TYPEC_CAP_POWER_OFF_CHARGE
	ret = typec_init_power_off_charge(tcpc);
	if (ret != 0)
		return ret;
#endif	/* CONFIG_TYPEC_CAP_POWER_OFF_CHARGE */

#ifdef CONFIG_TYPEC_POWER_CTRL_INIT
	tcpc->typec_power_ctrl = true;
#endif	/* CONFIG_TYPEC_POWER_CTRL_INIT */

	typec_unattached_entry(tcpc);
	return ret;
}

void  tcpc_typec_deinit(struct tcpc_device *tcpc)
{
}

#ifdef CONFIG_WATER_DETECTION
int tcpc_typec_handle_wd(struct tcpc_device *tcpc, bool wd)
{
	int ret = 0;

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION))
		return 0;

	TYPEC_INFO("%s %d\n", __func__, wd);
	if (!wd) {
		tcpci_set_water_protection(tcpc, false);
		tcpc_typec_error_recovery(tcpc);
		goto out;
	}

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (tcpc->bootmode == 8 || tcpc->bootmode == 9) {
		TYPEC_INFO("KPOC does not enter water protection\n");
		goto out;
	}
#endif /* CONFIG_MTK_KERNEL_POWER_OFF_CHARGING */

	tcpc->typec_attach_new = TYPEC_UNATTACHED;
	ret = tcpci_set_cc(tcpc, TYPEC_CC_OPEN);
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = tcpci_is_vsafe0v(tcpc);
	if (ret == 0) {
		TYPEC_NEW_STATE(typec_water_protection_wait);
		typec_wait_ps_change(tcpc, TYPEC_WAIT_PS_SRC_VSAFE0V);
	} else {
		TYPEC_NEW_STATE(typec_water_protection);
		tcpci_set_water_protection(tcpc, true);
	}
#else
	/* TODO: Wait ps change ? */
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

out:
	tcpci_notify_wd_status(tcpc, wd);
	if (tcpc->typec_state == typec_water_protection ||
	    tcpc->typec_state == typec_water_protection_wait) {
		typec_alert_attach_state_change(tcpc);
		tcpc->typec_attach_old = tcpc->typec_attach_new;
	}
	return ret;
}
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
int tcpc_typec_handle_ctd(struct tcpc_device *tcpc,
			  enum tcpc_cable_type cable_type)
{
	int ret;

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_CABLE_TYPE_DETECTION))
		return 0;


	/* Filter out initial no cable */
	if (cable_type == TCPC_CABLE_TYPE_C2C) {
		ret = tcpci_get_cc(tcpc);
		if (ret >= 0) {
			if (typec_is_cc_no_res() &&
			    (tcpc->typec_state == typec_unattached_snk ||
			     tcpc->typec_state == typec_unattached_src)) {
				TCPC_INFO("%s toggling or open\n", __func__);
				cable_type = TCPC_CABLE_TYPE_NONE;
			}
		}
	}

	TCPC_INFO("%s cable (%d, %d)\n", __func__, tcpc->typec_cable_type,
		  cable_type);

	if (tcpc->typec_cable_type == cable_type)
		return 0;

	if (tcpc->typec_cable_type != TCPC_CABLE_TYPE_NONE &&
	    cable_type != TCPC_CABLE_TYPE_NONE) {
		TCPC_INFO("%s ctd done once %d\n", __func__,
			  tcpc->typec_cable_type);
		return 0;
	}

	tcpc->typec_cable_type = cable_type;

	TCPC_INFO("%s cable type %d\n", __func__, tcpc->typec_cable_type);
	tcpci_notify_cable_type(tcpc);
	return 0;
}
#endif /* CONFIG_CABLE_TYPE_DETECTION */
