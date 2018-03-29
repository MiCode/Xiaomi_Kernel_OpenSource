/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/tcpci_typec.c
 * TCPC Type-C Driver for Richtek
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/cpu.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci_timer.h"

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
#define CONFIG_TYPEC_CAP_TRY_STATE
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
#undef CONFIG_TYPEC_CAP_TRY_STATE
#define CONFIG_TYPEC_CAP_TRY_STATE
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

#define TYPEC_EXIT_ATTACHED_SRC_NO_DEBOUNCE
#define TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS

static inline int typec_enable_low_power_mode(
	struct tcpc_device *tcpc_dev, int pull);

#define CC_ACT_AS_SINK(cc1, cc2) \
	((cc1+cc2) >= TYPEC_CC_VOLT_SNK_DFT)

#define TYPEC_ACT_AS_DRP(tcpc_dev) \
	(tcpc_dev->typec_role >= TYPEC_ROLE_DRP)

/* TYPEC_GET_CC_STATUS */

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
	 * Exit(-> TryWait.SNK) : after tDRPTry
	 * */
	typec_try_src,

	/* Require : Assert Rd
	 * Exit(-> Attached.SNK) : Detect Rp (tCCDebounce) and Vbus present.
	 * Exit(-> Unattached.SNK) : after tDRPTryWait
	 * */

	typec_trywait_snk,
	typec_trywait_snk_pe,
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK

	/* Require : Assert Rd
	 * Exit (-> Attached.SNK) : Detect Rp (tPDDebounce) and Vbus present.
	 * Exit (-> TryWait.SRC) : after tDRPTry if both of CC are SNK.Open.
	 */
	typec_try_snk,

	/*
	 * Require : Assert Rp
	 * Exit (-> Attached.SRC) : Detect Rd (tCCDebounce)
	 * Exit (-> Unattached.SNK) : After tDRPTryWait
	 */

	typec_trywait_src,
	typec_trywait_src_pe,
#endif

	typec_audioaccessory,
	typec_debugaccessory,

	typec_unattachwait_pe,	/* Wait Policy Engine go to Idle */
};

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
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	"Try.SNK",
	"TryWait.SRC",
	"TryWait.SRC.PE",
#endif

	"AudioAccessory",
	"DebugAccessory",
	"UnattachWait.PE",
};

static inline void typec_transfer_state(struct tcpc_device *tcpc_dev,
					enum TYPEC_CONNECTION_STATE state)
{
	TYPEC_INFO("** %s\r\n", typec_state_name[state]);
	tcpc_dev->typec_state = (uint8_t) state;
}

#define TYPEC_NEW_STATE(state)  \
	(typec_transfer_state(tcpc_dev, state))

/*
 * [BLOCK] TypeC Alert Attach Status Changed
 */

static const char *const typec_attach_name[] = {
	"NULL",
	"SINK",
	"SOURCE",
	"AUDIO",
	"DEBUG",
};

static int typec_alert_attach_state_change(struct tcpc_device *tcpc_dev)
{
	int ret = 0;

	if (tcpc_dev->typec_attach_old == tcpc_dev->typec_attach_new) {
		TYPEC_DBG("Attached-> %s(repeat)\r\n",
			typec_attach_name[tcpc_dev->typec_attach_new]);
		return 0;
	}

	TYPEC_INFO("Attached-> %s\r\n",
		   typec_attach_name[tcpc_dev->typec_attach_new]);

	/*Report function */
	ret = tcpci_report_usb_port_changed(tcpc_dev);

	tcpc_dev->typec_attach_old = tcpc_dev->typec_attach_new;
	return ret;
}

/*
 * [BLOCK] Unattached Entry
 */

static inline int typec_enable_low_power_mode(
	struct tcpc_device *tcpc_dev, int pull)
{
	int ret = 0;

	if (tcpc_dev->typec_cable_only) {
		TYPEC_DBG("LPM_RaOnly\r\n");

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
		if (tcpc_dev->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
			tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_WAKEUP);
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

		return 0;
	}

	if (tcpc_dev->typec_lpm != true)
		ret = tcpci_set_low_power_mode(tcpc_dev, true, pull);

	tcpc_dev->typec_lpm = true;
	return ret;
}

static inline int typec_disable_low_power_mode(
	struct tcpc_device *tcpc_dev)
{
	int ret = 0;

	if (tcpc_dev->typec_lpm != false)
		ret = tcpci_set_low_power_mode(tcpc_dev, false, TYPEC_CC_DRP);

	tcpc_dev->typec_lpm = false;
	return ret;
}

static void typec_unattached_power_entry(struct tcpc_device *tcpc_dev)
{
	tcpci_set_vconn(tcpc_dev, false);
	tcpci_sink_vbus(tcpc_dev, TCPC_VBUS_SINK_0V, 0);
	tcpci_source_vbus(tcpc_dev, TCPC_VBUS_SOURCE_0V, 0);
}

static void typec_unattached_entry(struct tcpc_device *tcpc_dev)
{
	typec_unattached_power_entry(tcpc_dev);

	switch (tcpc_dev->typec_role) {
	case TYPEC_ROLE_SNK:
		TYPEC_NEW_STATE(typec_unattached_snk);
		tcpci_set_cc(tcpc_dev, TYPEC_CC_RD);
		typec_enable_low_power_mode(tcpc_dev, TYPEC_CC_RD);
		break;
	case TYPEC_ROLE_SRC:
		TYPEC_NEW_STATE(typec_unattached_src);
		tcpci_set_cc(tcpc_dev, TYPEC_CC_RP);
		typec_enable_low_power_mode(tcpc_dev, TYPEC_CC_RP);
		break;
	default:
		switch (tcpc_dev->typec_state) {
		case typec_attachwait_snk:
			TYPEC_NEW_STATE(typec_unattached_src);
			tcpci_set_cc(tcpc_dev, TYPEC_CC_RP);
			tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_DRP_SRC_TOGGLE);
			break;
		default:
			TYPEC_NEW_STATE(typec_unattached_snk);
			tcpci_set_cc(tcpc_dev, TYPEC_CC_DRP);
			typec_enable_low_power_mode(tcpc_dev, TYPEC_CC_DRP);
			break;
		}
		break;
	}
}

static void typec_unattach_wait_pe_idle_entry(struct tcpc_device *tcpc_dev)
{
	tcpc_dev->typec_attach_new = TYPEC_UNATTACHED;

#ifdef CONFIG_TYPEC_CAP_TRY_STATE
	if (TYPEC_ACT_AS_DRP(tcpc_dev))
		tcpc_disable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRYWAIT);
#endif

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc_dev->typec_attach_old) {
		TYPEC_NEW_STATE(typec_unattachwait_pe);
		return;
	}
#endif

	typec_unattached_entry(tcpc_dev);
}

/*
 * [BLOCK] Attached Entry
 */

static inline int typec_set_polarity(struct tcpc_device *tcpc_dev,
					bool polarity)
{
	tcpc_dev->typec_polarity = polarity;
	return tcpci_set_polarity(tcpc_dev, polarity);
}

static inline int typec_set_plug_orient(
	struct tcpc_device *tcpc_dev, uint8_t res, bool polarity)
{
	int rv = typec_set_polarity(tcpc_dev, polarity);

	if (rv)
		return rv;

	return tcpci_set_cc(tcpc_dev, res);
}

static void typec_source_attached_with_vbus_entry(struct tcpc_device *tcpc_dev)
{
	if (tcpc_dev->typec_attach_old != TYPEC_ATTACHED_SRC) {
		tcpc_dev->typec_check_legacy_cable = 0;
		tcpc_dev->typec_attach_new = TYPEC_ATTACHED_SRC;
		typec_alert_attach_state_change(tcpc_dev);
	}
}

static void typec_source_attached_without_vbus_entry(
					struct tcpc_device *tcpc_dev)
{
	tcpc_dev->typec_wait_ps_change = false;

	tcpci_set_vconn(tcpc_dev, true);
	tcpci_source_vbus(tcpc_dev, TCPC_VBUS_SOURCE_5V, 0);
}

static inline void typec_source_attached_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	bool vbus_absent;

	TYPEC_NEW_STATE(typec_attached_src);

#ifdef CONFIG_TYPEC_CAP_TRY_STATE
	if (TYPEC_ACT_AS_DRP(tcpc_dev))
		tcpc_reset_typec_try_timer(tcpc_dev);
#endif

	typec_set_plug_orient(tcpc_dev,
		tcpc_dev->typec_local_rp_level, (cc2 == TYPEC_CC_VOLT_RD));

	/* If Port Partner act as Sink with low VBUS, wait vSafe0v */
	vbus_absent = tcpci_check_vsafe0v(tcpc_dev, true);

	if (vbus_absent)
		typec_source_attached_without_vbus_entry(tcpc_dev);
	else {
		TYPEC_DBG("wait_ps_change (src)\r\n");
		tcpc_dev->typec_wait_ps_change = true;
	}
}

static inline void typec_sink_attached_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	TYPEC_NEW_STATE(typec_attached_snk);

	tcpc_dev->typec_wait_ps_change = false;
	tcpc_dev->typec_attach_new = TYPEC_ATTACHED_SNK;

#ifdef CONFIG_TYPEC_CAP_TRY_STATE
	if (TYPEC_ACT_AS_DRP(tcpc_dev))
		tcpc_reset_typec_try_timer(tcpc_dev);
#endif

	typec_set_plug_orient(tcpc_dev, TYPEC_CC_RD,
					(cc2 != TYPEC_CC_VOLT_OPEN));
	tcpc_dev->typec_remote_rp_level =
				(tcpc_dev->typec_polarity ? cc2 : cc1);

	tcpci_sink_vbus(tcpc_dev, TCPC_VBUS_SINK_5V, 0);
}

/*
 * [BLOCK] Try.SRC / TryWait.SNK
 */

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE

static inline void typec_try_src_entry(struct tcpc_device *tcpc_dev)
{
	TYPEC_NEW_STATE(typec_try_src);
	tcpci_set_cc(tcpc_dev, TYPEC_CC_RP);
	tcpc_enable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRY);
}

static inline void typec_trywait_snk_entry(struct tcpc_device *tcpc_dev)
{
	TYPEC_NEW_STATE(typec_trywait_snk);
	tcpc_dev->typec_wait_ps_change = false;

	tcpci_set_vconn(tcpc_dev, false);
	tcpci_set_cc(tcpc_dev, TYPEC_CC_RD);
	tcpci_source_vbus(tcpc_dev, TCPC_VBUS_SOURCE_0V, 0);
	tcpc_disable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRY);
	tcpc_enable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRYWAIT);
}

static inline void typec_trywait_snk_pe_entry(struct tcpc_device *tcpc_dev)
{
	tcpc_dev->typec_attach_new = TYPEC_UNATTACHED;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc_dev->typec_attach_old) {
		TYPEC_NEW_STATE(typec_trywait_snk_pe);
		return;
	}
#endif

	typec_trywait_snk_entry(tcpc_dev);
}

#endif /* #ifdef CONFIG_TYPEC_CAP_TRY_SOURCE */

/*
 * [BLOCK] Try.SNK / TryWait.SRC
 */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK

static inline void typec_try_snk_entry(struct tcpc_device *tcpc_dev)
{
	TYPEC_NEW_STATE(typec_try_snk);
	tcpc_dev->typec_wait_ps_change = false;
	tcpc_dev->typec_trysnk_timeout = false;

	tcpci_set_cc(tcpc_dev, TYPEC_CC_RD);
	tcpc_enable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRY);
}

static inline void typec_trywait_src_entry(struct tcpc_device *tcpc_dev)
{
	TYPEC_NEW_STATE(typec_trywait_src);

	tcpci_set_cc(tcpc_dev, TYPEC_CC_RP);

	tcpc_disable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRY);
	tcpc_enable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRYWAIT);
}

static inline void typec_trywait_src_pe_entry(struct tcpc_device *tcpc_dev)
{
	tcpc_dev->typec_attach_new = TYPEC_UNATTACHED;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc_dev->typec_attach_old) {
		TYPEC_NEW_STATE(typec_trywait_src_pe);
		return;
	}
#endif

	typec_trywait_src_entry(tcpc_dev);
}

#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

/*
 * [BLOCK] Attach / Detach
 */

static inline void typec_cc_snk_vbus_present_entry(struct tcpc_device *tcpc_dev,
						   int cc1, int cc2)
{
	tcpc_dev->typec_wait_ps_change = false;

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	if (tcpc_dev->typec_role == TYPEC_ROLE_TRY_SRC) {
		if (tcpc_dev->typec_state == typec_attachwait_snk) {
			typec_try_src_entry(tcpc_dev);
			return;
		}

	}
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */

	typec_sink_attached_entry(tcpc_dev, cc1, cc2);
}

static inline void typec_cc_snk_detect_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	/* If Port Partner act as Source without VBUS, wait vSafe5V */
	if (tcpci_check_vbus_valid(tcpc_dev))
		typec_cc_snk_vbus_present_entry(tcpc_dev, cc1, cc2);
	else {
		TYPEC_DBG("wait_ps_change (snk)\r\n");
		tcpc_dev->typec_wait_ps_change = true;
	}
}

static inline void typec_cc_src_detect_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	if (tcpc_dev->typec_role == TYPEC_ROLE_TRY_SNK) {
		if (tcpc_dev->typec_state == typec_attachwait_src) {
			typec_try_snk_entry(tcpc_dev);
			return;
		}
	}
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */

	typec_source_attached_entry(tcpc_dev, cc1, cc2);
}

static inline void typec_cc_src_remove_entry(struct tcpc_device *tcpc_dev)
{
	switch (tcpc_dev->typec_state) {
	case typec_attached_src:
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
		if (tcpc_dev->typec_attach_old != TYPEC_ATTACHED_SRC)
			TYPEC_DBG("check_legacy=%d\r\n",
				tcpc_dev->typec_check_legacy_cable++);
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
		if (tcpc_dev->typec_role == TYPEC_ROLE_TRY_SRC) {
			typec_trywait_snk_pe_entry(tcpc_dev);
			return;
		}
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */
		break;

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		TYPEC_DBG("Igrone src.remove, Try.SRC\r\n");
		return;
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_trywait_src:
	case typec_trywait_src_pe:
		TYPEC_DBG("Igrone src.remove, TryWait.SRC\r\n");
		return;
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */
	}

	typec_unattach_wait_pe_idle_entry(tcpc_dev);
}

static inline void typec_cc_snk_remove_entry(struct tcpc_device *tcpc_dev)
{
	switch (tcpc_dev->typec_state) {
#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_attached_snk:
		if (tcpc_dev->typec_role == TYPEC_ROLE_TRY_SNK) {
			typec_trywait_src_pe_entry(tcpc_dev);
			return;
		}
		break;

	case typec_try_snk:
		TYPEC_DBG("Igrone snk.remove, Try.SNK\r\n");
		return;
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_trywait_snk:
	case typec_trywait_snk_pe:
		TYPEC_DBG("Igrone snk.remove, TryWait.SNK\r\n");
		return;
#endif
	}

	typec_unattach_wait_pe_idle_entry(tcpc_dev);
}

/*
 * [BLOCK] CC Change (after debounce)
 */

#ifdef CONFIG_TYPEC_CHECK_CC_STABLE

static inline bool typec_check_cc_stable_source(
	struct tcpc_device *tcpc_dev, int *cc1, int *cc2)
{

	int ret, cc1a, cc2a, cc1b, cc2b;
	bool check_stable = false;

	if (!(tcpc_dev->tcpc_flags & TCPC_FLAGS_CHECK_CC_STABLE))
		return true;

	cc1a = *cc1;
	cc2a = *cc2;

	if ((cc1a == TYPEC_CC_VOLT_RD) && (cc2a == TYPEC_CC_VOLT_RD))
		check_stable = true;

	if ((cc1a == TYPEC_CC_VOLT_RA) || (cc2a == TYPEC_CC_VOLT_RA))
		check_stable = true;

	if (check_stable) {
		TYPEC_INFO("CC Stable Check...\r\n");
		typec_set_polarity(tcpc_dev, !tcpc_dev->typec_polarity);
		mdelay(1);

		ret = tcpci_get_cc(tcpc_dev, &cc1b, &cc2b);
		if ((cc1b != cc1a) || (cc2b != cc2a)) {
			TYPEC_INFO("CC Unstable... %d/%d\r\n", cc1b, cc2b);

			if ((cc1b == TYPEC_CC_VOLT_RD) &&
						(cc2b != TYPEC_CC_VOLT_RD)) {
				*cc1 = cc1b;
				*cc2 = cc2b;
				return true;
			}

			if ((cc1b != TYPEC_CC_VOLT_RD) &&
						(cc2b == TYPEC_CC_VOLT_RD)) {
				*cc1 = cc1b;
				*cc2 = cc2b;
				return true;
			}

			typec_cc_src_remove_entry(tcpc_dev);
			return false;
		}

		typec_set_polarity(tcpc_dev, !tcpc_dev->typec_polarity);
		mdelay(1);

		ret = tcpci_get_cc(tcpc_dev, &cc1b, &cc2b);
		if ((cc1b != cc1a) || (cc2b != cc2a)) {
			TYPEC_INFO("CC Unstable1... %d/%d\r\n", cc1b, cc2b);

			if ((cc1b == TYPEC_CC_VOLT_RD) &&
						(cc2b != TYPEC_CC_VOLT_RD)) {
				*cc1 = cc1b;
				*cc2 = cc2b;
				return true;
			}

			if ((cc1b != TYPEC_CC_VOLT_RD) &&
						(cc2b == TYPEC_CC_VOLT_RD)) {
				*cc1 = cc1b;
				*cc2 = cc2b;
				return true;
			}

			typec_cc_src_remove_entry(tcpc_dev);
			return false;
		}
	}

	return true;
}

static inline bool typec_check_cc_stable_sink(
	struct tcpc_device *tcpc_dev, int *cc1, int *cc2)
{
	int ret, cc1a, cc2a, cc1b, cc2b;

	if (!(tcpc_dev->tcpc_flags & TCPC_FLAGS_CHECK_CC_STABLE))
		return true;

	cc1a = *cc1;
	cc2a = *cc2;

	if ((cc1a != TYPEC_CC_VOLT_OPEN) && (cc2a != TYPEC_CC_VOLT_OPEN)) {
		TYPEC_INFO("CC Stable Check...\r\n");
		typec_set_polarity(tcpc_dev, !tcpc_dev->typec_polarity);
		mdelay(1);

		ret = tcpci_get_cc(tcpc_dev, &cc1b, &cc2b);
		if ((cc1b != cc1a) || (cc2b != cc2a)) {
			TYPEC_INFO("CC Unstable... %d/%d\r\n", cc1b, cc2b);
			*cc1 = cc1b;
			*cc2 = cc2b;
		}
	}

	return true;
}

#endif

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE

static inline bool typec_check_legacy_cable(
	struct tcpc_device *tcpc_dev, int cc1a, int cc2a)
{
	int cc1b, cc2b;
	bool check_legacy = false;

	if ((cc1a == TYPEC_CC_VOLT_RD) && (cc2a == TYPEC_CC_VOLT_OPEN))
		check_legacy = true;

	if ((cc2a == TYPEC_CC_VOLT_RD) && (cc1a == TYPEC_CC_VOLT_OPEN))
		check_legacy = true;

	if (check_legacy && (tcpc_dev->typec_check_legacy_cable > 5)) {
		tcpc_dev->typec_check_legacy_cable = 0;
		TYPEC_INFO("Check Legacy Cable ...\r\n");
		tcpci_set_cc(tcpc_dev, TYPEC_CC_RP_1_5);
		mdelay(1);

		tcpci_get_cc(tcpc_dev, &cc1b, &cc2b);

		if ((cc1b + cc2b) == TYPEC_CC_VOLT_OPEN) {
			tcpc_dev->typec_legacy_cable = true;
			TYPEC_INFO("Legacy Cable\r\n");

			tcpc_dev->typec_attach_new = TYPEC_UNATTACHED;
			typec_unattached_entry(tcpc_dev);
			return true;
		}

		tcpci_set_cc(tcpc_dev, TYPEC_CC_RP);
	}

	return false;
}

#endif

static inline bool typec_cc_change_source_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
#ifdef CONFIG_TYPEC_CHECK_CC_STABLE
	if (!typec_check_cc_stable_source(tcpc_dev, &cc1, &cc2))
		return true;
#endif /* CONFIG_TYPEC_CHECK_CC_STABLE */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (typec_check_legacy_cable(tcpc_dev, cc1, cc2))
		return true;
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	if ((cc1 == TYPEC_CC_VOLT_RD) && (cc2 == TYPEC_CC_VOLT_RD)) {
		TYPEC_NEW_STATE(typec_debugaccessory);
		TYPEC_DBG("[Debug] CC1&2 Both Rd\r\n");
		tcpc_dev->typec_attach_new = TYPEC_ATTACHED_DEBUG;
	} else if ((cc1 == TYPEC_CC_VOLT_RA) && (cc2 == TYPEC_CC_VOLT_RA)) {
		TYPEC_NEW_STATE(typec_audioaccessory);
		TYPEC_DBG("[Audio] CC1&2 Both Ra\r\n");
		tcpc_dev->typec_attach_new = TYPEC_ATTACHED_AUDIO;
	} else {
		if ((cc1 == TYPEC_CC_VOLT_RD) || (cc2 == TYPEC_CC_VOLT_RD))
			typec_cc_src_detect_entry(tcpc_dev, cc1, cc2);
		else {
			if ((cc1 == TYPEC_CC_VOLT_RA)
			    || (cc2 == TYPEC_CC_VOLT_RA))
				TYPEC_DBG("[Cable] Ra Only\r\n");
			typec_cc_src_remove_entry(tcpc_dev);
		}
	}
	return true;
}

static inline bool typec_cc_change_sink_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
#ifdef CONFIG_TYPEC_CHECK_CC_STABLE
	typec_check_cc_stable_sink(tcpc_dev, &cc1, &cc2);
#endif

	if (cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_OPEN)
		typec_cc_snk_remove_entry(tcpc_dev);
	else if (cc1 != TYPEC_CC_VOLT_OPEN && cc2 != TYPEC_CC_VOLT_OPEN) {
		TYPEC_DBG("[Warning] CC1&2 Both Rp\r\n");
		/* typec_cc_snk_remove_entry(tcpc_dev); */
	} else
		typec_cc_snk_detect_entry(tcpc_dev, cc1, cc2);

	return true;
}

static inline bool typec_drp_is_act_as_sink_role(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	bool as_sink;

	switch (tcpc_dev->typec_state) {

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		as_sink = false;
		break;
	case typec_trywait_snk:
	case typec_trywait_snk_pe:
		as_sink = true;
		break;
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_try_snk:
		as_sink = true;
		break;

	case typec_trywait_src:
	case typec_trywait_src_pe:
		as_sink = false;
		break;
#endif

	case typec_unattached_src:
		as_sink = false;
		break;

	case typec_attached_src:
		as_sink = false;
		break;

	case typec_attached_snk:
		as_sink = true;
		break;

	default:
		as_sink = CC_ACT_AS_SINK(cc1, cc2);
		break;
	}

	return as_sink;
}

static inline bool typec_handle_cc_changed_entry(
				struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	bool as_sink;

	TYPEC_INFO("[CC_Change] %d/%d\r\n", cc1, cc2);

	tcpc_dev->typec_attach_new = tcpc_dev->typec_attach_old;

	switch (tcpc_dev->typec_role) {
	case TYPEC_ROLE_SNK:
		as_sink = true;
		break;

	case TYPEC_ROLE_SRC:
		as_sink = false;
		break;

	default:
		as_sink = typec_drp_is_act_as_sink_role(tcpc_dev, cc1, cc2);
		break;
	}

	if (as_sink)
		typec_cc_change_sink_entry(tcpc_dev, cc1, cc2);
	else
		typec_cc_change_source_entry(tcpc_dev, cc1, cc2);

	typec_alert_attach_state_change(tcpc_dev);
	return true;
}

/*
 * [BLOCK] Handle cc-change event
 */

static inline void typec_attach_wait_entry(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	bool as_sink;

	if (tcpc_dev->typec_attach_old ||
		tcpc_dev->typec_state == typec_attached_src) {
		tcpc_reset_typec_debounce_timer(tcpc_dev);
		TYPEC_DBG("Attached, Ignore cc_attach\r\n");
		return;
	}

	switch (tcpc_dev->typec_state) {

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_PDDEBOUNCE);
		return;

	case typec_trywait_snk:
	case typec_trywait_snk_pe:
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_CCDEBOUNCE);
		return;
#endif

#ifdef	CONFIG_TYPEC_CAP_TRY_SINK
	case typec_try_snk:
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_PDDEBOUNCE);
		return;

	case typec_trywait_src:
	case typec_trywait_src_pe:
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_CCDEBOUNCE);
		return;
#endif
	}

	switch (tcpc_dev->typec_role) {
	case TYPEC_ROLE_SNK:
		as_sink = true;
		break;

	case TYPEC_ROLE_SRC:
		as_sink = false;
		break;

	default:
		as_sink = CC_ACT_AS_SINK(cc1, cc2);
		break;
	}

#ifdef CONFIG_USB_POWER_DELIVERY
	switch (tcpc_dev->typec_state) {
	case typec_unattachwait_pe:
		tcpc_dev->pd_wait_pe_idle = false;
		tcpc_disable_timer(tcpc_dev, TYPEC_TIMER_PE_IDLE);
		typec_unattached_power_entry(tcpc_dev);
		break;
	}
#endif

	if (as_sink)
		TYPEC_NEW_STATE(typec_attachwait_snk);
	else
		TYPEC_NEW_STATE(typec_attachwait_src);

	tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_CCDEBOUNCE);
}

static inline void typec_detach_wait_entry(
		struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	switch (tcpc_dev->typec_state) {

	case typec_audioaccessory:
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_CCDEBOUNCE);
		break;

#ifdef TYPEC_EXIT_ATTACHED_SRC_NO_DEBOUNCE
	case typec_attached_src:
		TYPEC_INFO("Exit Attached.SRC immediately\r\n");
		tcpc_reset_typec_debounce_timer(tcpc_dev);

		/* force to terminate TX */
		tcpci_init(tcpc_dev, true);

		typec_cc_src_remove_entry(tcpc_dev);
		typec_alert_attach_state_change(tcpc_dev);
		break;
#endif /* TYPEC_EXIT_ATTACHED_SRC_NO_DEBOUNCE */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_try_snk:
		tcpc_dev->typec_wait_ps_change = false;
		if (tcpc_dev->typec_trysnk_timeout) {
			tcpc_reset_typec_debounce_timer(tcpc_dev);
			typec_trywait_src_entry(tcpc_dev);
		}
		break;

	case typec_trywait_src:
		TYPEC_DBG("Ignore cc_detach, TryState\n");
		tcpc_reset_typec_debounce_timer(tcpc_dev);
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */
#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		TYPEC_DBG("Ignore cc_detach, TryState\n");
		tcpc_reset_typec_debounce_timer(tcpc_dev);
		break;
	case typec_trywait_snk:
		TYPEC_DBG("Ignore cc_detach, TryState\n");
		tcpc_reset_typec_debounce_timer(tcpc_dev);
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */

	default:
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_PDDEBOUNCE);
		break;
	}
}

static inline bool typec_is_cc_attach(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	bool cc_attach = false;
	int cc_res = (tcpc_dev->typec_polarity ? cc2 : cc1);

	tcpc_dev->typec_cable_only = false;

	if (tcpc_dev->typec_attach_old) {
		if ((cc_res != TYPEC_CC_VOLT_OPEN) &&
					(cc_res != TYPEC_CC_VOLT_RA))
			cc_attach = true;
	} else {
		if (cc1 != TYPEC_CC_VOLT_OPEN)
			cc_attach = true;

		if (cc2 != TYPEC_CC_VOLT_OPEN)
			cc_attach = true;

		/* Cable Only, no device */
		if ((cc1+cc2) == TYPEC_CC_VOLT_RA) {
			cc_attach = false;
			tcpc_dev->typec_cable_only = true;
		}
	}

	return cc_attach;
}

#ifdef TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS
static inline int typec_attached_snk_cc_change(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	int vbus_valid = tcpci_check_vbus_valid(tcpc_dev);
	int cc_res = (tcpc_dev->typec_polarity ? cc2 : cc1);

	bool detach_by_cc = false;

	/* For Ellisys Test, Applying Low VBUS as Sink */
	if (vbus_valid && (cc_res == TYPEC_CC_VOLT_OPEN)) {
		detach_by_cc = true;
		TYPEC_DBG("Detach_CC (LowVBUS)\r\n");
	}

#ifdef CONFIG_USB_POWER_DELIVERY
	/* For Source detach during HardReset */
	if (tcpc_dev->pd_wait_hard_reset_complete &&
		(!vbus_valid) && (cc_res == TYPEC_CC_VOLT_OPEN)) {
		detach_by_cc = true;
		TYPEC_DBG("Detach_CC (HardReset)\r\n");
	}
#endif

	if (detach_by_cc) {

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
		if (tcpc_dev->typec_role == TYPEC_ROLE_TRY_SNK)
			typec_trywait_src_pe_entry(tcpc_dev);
		else
#endif
		typec_unattach_wait_pe_idle_entry(tcpc_dev);

		typec_alert_attach_state_change(tcpc_dev);
	}

	return 0;
}
#endif /* TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS */

int tcpc_typec_handle_cc_change(struct tcpc_device *tcpc_dev)
{
	int ret, cc1, cc2;

	ret = tcpci_get_cc(tcpc_dev, &cc1, &cc2);
	if (ret)
		return ret;

	if (cc1 == TYPEC_CC_DRP_TOGGLING) {
		TYPEC_DBG("[Waring] DRP Toggling\r\n");
		if (tcpc_dev->typec_lpm)
			tcpci_set_low_power_mode(tcpc_dev, true, TYPEC_CC_DRP);
		return 0;
	}

	TYPEC_INFO("[CC_Alert] %d/%d\r\n", cc1, cc2);

	typec_disable_low_power_mode(tcpc_dev);

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	tcpc_dev->typec_legacy_cable = false;
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc_dev->pd_wait_pr_swap_complete) {
		TYPEC_DBG("[PR.Swap] Ignore CC_Change\r\n");
		return 0;
	}

	if (tcpc_dev->pd_wait_error_recovery) {
		TYPEC_DBG("[Recovery] Ignore CC_Change\r\n");
		return 0;
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS
	if (tcpc_dev->typec_state == typec_attached_snk) {
		typec_attached_snk_cc_change(tcpc_dev, cc1, cc2);
		return 0;
	}
#endif /* TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS */

	if (typec_is_cc_attach(tcpc_dev, cc1, cc2))
		typec_attach_wait_entry(tcpc_dev, cc1, cc2);
	else
		typec_detach_wait_entry(tcpc_dev, cc1, cc2);

	return 0;
}

/*
 * [BLOCK] Handle timeout event
 */

static inline int typec_handle_trywait_timeout(struct tcpc_device *tcpc_dev)
{
	switch (tcpc_dev->typec_state) {

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_try_src:
		typec_trywait_snk_entry(tcpc_dev);
		break;

	case typec_trywait_snk:
		typec_unattach_wait_pe_idle_entry(tcpc_dev);
		break;
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_try_snk:
		if (tcpc_dev->typec_wait_ps_change) {
			tcpc_dev->typec_trysnk_timeout = true;
			tcpc_disable_timer(tcpc_dev, TYPEC_TRY_TIMER_DRP_TRY);
		} else
			typec_trywait_src_entry(tcpc_dev);
		break;

	case typec_trywait_src:
		typec_unattach_wait_pe_idle_entry(tcpc_dev);
		break;
#endif
	}

	return 0;
}

static inline int typec_handle_debounce_timeout(struct tcpc_device *tcpc_dev)
{
	int ret, cc1, cc2;

	ret = tcpci_get_cc(tcpc_dev, &cc1, &cc2);
	if (ret)
		return ret;

	if (cc1 == TYPEC_CC_DRP_TOGGLING) {
		TYPEC_DBG("[Waring] DRP Toggling\r\n");
		return 0;
	}

	typec_handle_cc_changed_entry(tcpc_dev, cc1, cc2);
	return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static inline int typec_handle_error_recovery_timeout(
						struct tcpc_device *tcpc_dev)
{
	/* TODO: Check it later */
	tcpc_dev->typec_attach_new = TYPEC_UNATTACHED;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_error_recovery = false;
	mutex_unlock(&tcpc_dev->access_lock);

	typec_unattach_wait_pe_idle_entry(tcpc_dev);
	typec_alert_attach_state_change(tcpc_dev);

	return 0;
}

static inline int typec_handle_pe_idle(struct tcpc_device *tcpc_dev)
{
	switch (tcpc_dev->typec_state) {

#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_trywait_snk_pe:
		typec_trywait_snk_entry(tcpc_dev);
		break;
#endif

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_trywait_src_pe:
		typec_trywait_src_entry(tcpc_dev);
		break;
#endif

	case typec_unattachwait_pe:
		typec_unattached_entry(tcpc_dev);
		break;

	default:
		TYPEC_DBG("Dummy pe_idle\r\n");
		break;
	}

	return 0;
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static inline int typec_handle_reach_vsafe0v(struct tcpc_device *tcpc_dev)
{
	typec_source_attached_without_vbus_entry(tcpc_dev);
	return 0;
}

static int typec_alert_vsafe0v(struct tcpc_device *tcpc_dev)
{
	if (tcpc_dev->typec_wait_ps_change) {
#ifdef CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_DELAY
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_SAFE0V);
#else
		typec_handle_reach_vsafe0v(tcpc_dev);
#endif
	}

	return 0;
}

static inline int typec_handle_src_toggle_timeout(struct tcpc_device *tcpc_dev)
{
	if (tcpc_dev->typec_state == typec_unattached_src) {
		TYPEC_NEW_STATE(typec_unattached_snk);
		tcpci_set_cc(tcpc_dev, TYPEC_CC_DRP);
		typec_enable_low_power_mode(tcpc_dev, TYPEC_CC_DRP);
	}

	return 0;
}

int tcpc_typec_handle_timeout(struct tcpc_device *tcpc_dev, uint32_t timer_id)
{
	int ret = 0;

	tcpc_reset_typec_debounce_timer(tcpc_dev);

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc_dev->pd_wait_pr_swap_complete) {
		TYPEC_DBG("[PR.Swap] Igrone timer_evt\r\n");
		return 0;
	}

	if (tcpc_dev->pd_wait_error_recovery &&
		(timer_id != TYPEC_TIMER_ERROR_RECOVERY)) {
		TYPEC_DBG("[Recovery] Igrone timer_evt\r\n");
		return 0;
	}
#endif

	switch (timer_id) {
#ifdef CONFIG_TYPEC_CAP_TRY_STATE
	case TYPEC_TRY_TIMER_DRP_TRY:
	case TYPEC_TRY_TIMER_DRP_TRYWAIT:
		ret = typec_handle_trywait_timeout(tcpc_dev);
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_STATE */

	case TYPEC_TIMER_CCDEBOUNCE:
	case TYPEC_TIMER_PDDEBOUNCE:
		ret = typec_handle_debounce_timeout(tcpc_dev);
		break;

#ifdef CONFIG_USB_POWER_DELIVERY
	case TYPEC_TIMER_ERROR_RECOVERY:
		ret = typec_handle_error_recovery_timeout(tcpc_dev);
		break;

	case TYPEC_TIMER_PE_IDLE:
		ret = typec_handle_pe_idle(tcpc_dev);
		break;
#endif /* CONFIG_USB_POWER_DELIVERY */

	case TYPEC_TIMER_SAFE0V:
		ret = typec_handle_reach_vsafe0v(tcpc_dev);
		break;

	case TYPEC_TIMER_WAKEUP:
		if (tcpc_dev->typec_lpm || tcpc_dev->typec_cable_only) {
			tcpc_dev->typec_lpm = true;
			ret = tcpci_set_low_power_mode(tcpc_dev, true,
				(tcpc_dev->typec_role == TYPEC_ROLE_SRC) ?
				TYPEC_CC_RP : TYPEC_CC_DRP);
		}
		break;

	case TYPEC_TIMER_DRP_SRC_TOGGLE:
		ret = typec_handle_src_toggle_timeout(tcpc_dev);
		break;

	}

	return ret;
}

/*
 * [BLOCK] Handle ps-change event
 */

static inline int typec_handle_vbus_present(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
	switch (tcpc_dev->typec_state) {
#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
	case typec_trywait_snk:
		if (tcpc_dev->typec_wait_ps_change) {
			typec_cc_snk_vbus_present_entry(tcpc_dev, cc1, cc2);
			typec_alert_attach_state_change(tcpc_dev);
		}
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCE */
#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	case typec_try_snk:
		if (tcpc_dev->typec_wait_ps_change) {
			typec_cc_snk_vbus_present_entry(tcpc_dev, cc1, cc2);
			typec_alert_attach_state_change(tcpc_dev);
		}
		break;
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */
	case typec_attachwait_snk:
		if (tcpc_dev->typec_wait_ps_change) {
			typec_cc_snk_vbus_present_entry(tcpc_dev, cc1, cc2);
			typec_alert_attach_state_change(tcpc_dev);
		}
		break;

	case typec_attached_src:
		typec_source_attached_with_vbus_entry(tcpc_dev);
		break;
	}

	return 0;
}

static inline int typec_attached_snk_vbus_absent(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
#ifdef TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS
#ifdef CONFIG_USB_POWER_DELIVERY
	int cc_res = (tcpc_dev->typec_polarity ? cc2 : cc1);

	if (tcpc_dev->pd_wait_hard_reset_complete ||
				tcpc_dev->pd_hard_reset_event_pending) {
		if (cc_res != TYPEC_CC_VOLT_OPEN) {
			TYPEC_DBG
			    ("Ignore vbus_absent(snk), HReset & CC!=0\r\n");
			return 0;
		}
	}
#ifdef CONFIG_RT7207_ADAPTER
	if (tcpc_dev->rt7207_direct_charge_flag) {
		if (cc_res != TYPEC_CC_VOLT_OPEN &&
				!tcpci_check_vsafe0v(tcpc_dev, true)) {
			TYPEC_DBG("Ignore vbus_absent(snk), Dircet Charging\n");
			return 0;
		}
	}
#endif /* CONFIG_RT7207_ADAPTER */

#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TYPEC_CAP_TRY_SINK
	if (tcpc_dev->typec_role == TYPEC_ROLE_TRY_SNK)
		typec_trywait_src_pe_entry(tcpc_dev);
	else
#endif
		typec_unattach_wait_pe_idle_entry(tcpc_dev);

	typec_alert_attach_state_change(tcpc_dev);
#endif /* TYPEC_EXIT_ATTACHED_SNK_VIA_VBUS */

	return 0;
}


static inline int typec_attached_src_vbus_absent(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
#ifndef CONFIG_TCPC_VSAFE0V_DETECT
	typec_alert_vsafe0v(tcpc_dev);
#endif /* #ifdef CONFIG_TCPC_VSAFE0V_DETECT */

	return 0;
}

static inline int typec_handle_vbus_absent(
			struct tcpc_device *tcpc_dev, int cc1, int cc2)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc_dev->pd_wait_pr_swap_complete) {
		TYPEC_DBG("[PR.Swap] Igrone vbus_absent\r\n");
		return 0;
	}

	if (tcpc_dev->pd_wait_error_recovery) {
		TYPEC_DBG("[Recovery] Igrone vbus_absent\r\n");
		return 0;
	}
#endif

	switch (tcpc_dev->typec_state) {

	case typec_attached_snk:
		typec_attached_snk_vbus_absent(tcpc_dev, cc1, cc2);
		break;

	case typec_attached_src:
		typec_attached_src_vbus_absent(tcpc_dev, cc1, cc2);
		break;

	default:
		break;
	}

	return 0;
}

int tcpc_typec_handle_ps_change(struct tcpc_device *tcpc_dev, int vbus_level)
{
	int ret, cc1, cc2;

	ret = tcpci_get_cc(tcpc_dev, &cc1, &cc2);
	if (ret)
		return ret;

	if (cc1 == TYPEC_CC_DRP_TOGGLING) {
		TYPEC_DBG("[Waring] DRP Toggling\r\n");
		return 0;
	}

	if (vbus_level)
		return typec_handle_vbus_present(tcpc_dev, cc1, cc2);
	else
		return typec_handle_vbus_absent(tcpc_dev, cc1, cc2);
}

/*
 * [BLOCK] Handle PE event
 */

#ifdef CONFIG_USB_POWER_DELIVERY

int tcpc_typec_handle_pe_pr_swap(struct tcpc_device *tcpc_dev)
{
	int ret = 0;

	mutex_lock(&tcpc_dev->typec_lock);
	switch (tcpc_dev->typec_state) {
	case typec_attached_snk:
		TYPEC_NEW_STATE(typec_attached_src);
		tcpci_set_cc(tcpc_dev, tcpc_dev->typec_local_rp_level);
		break;
	case typec_attached_src:
		TYPEC_NEW_STATE(typec_attached_snk);
		tcpci_set_cc(tcpc_dev, TYPEC_CC_RD);
		break;
	default:
		break;
	}
	mutex_unlock(&tcpc_dev->typec_lock);
	return ret;
}

#endif /* CONFIG_USB_POWER_DELIVERY */

/*
 * [BLOCK] Handle reach vSafe0V event
 */

#ifdef CONFIG_TCPC_VSAFE0V_DETECT

int tcpc_typec_handle_vsafe0v(struct tcpc_device *tcpc_dev)
{
	if (tcpc_dev->typec_state == typec_attached_src)
		typec_alert_vsafe0v(tcpc_dev);

	return 0;
}

#endif /* CONFIG_TCPC_VSAFE0V_DETECT */


/*
 * [BLOCK] TCPCI TypeC I/F
 */

static const char *const typec_role_name[] = {
	"UNKNOWN",
	"SNK",
	"SRC",
	"DRP",
	"TrySRC",
	"TrySNK",
};

#ifndef CONFIG_USB_POWER_DELIVERY
int tcpc_typec_swap_role(struct tcpc_device *tcpc_dev)
{
	if (tcpc_dev->typec_role < TYPEC_ROLE_DRP)
		return -1;
	TYPEC_INFO("tcpc_typec_swap_role\r\n");

	switch (tcpc_dev->typec_state) {
	case typec_attached_src:
#ifdef CONFIG_TYPEC_CAP_TRY_SOURCE
		typec_trywait_snk_pe_entry(tcpc_dev);
#else
		TYPEC_INFO("SRC->SNK (X)\r\n");
#endif /* CONFIG_TYPEC_CAP_TRY_SOURCR */
		break;
	case typec_attached_snk:
#ifdef CONFIG_TYPEC_CAP_TRY_SINK
		typec_trywait_src_pe_entry(tcpc_dev);
#else
		TYPEC_INFO("SNK->SRC (X)\r\n");
#endif /* CONFIG_TYPEC_CAP_TRY_SINK */
		break;
	}
	return typec_alert_attach_state_change(tcpc_dev);
}
#endif /* ifndef CONFIG_USB_POWER_DELIVERY */

int tcpc_typec_set_rp_level(struct tcpc_device *tcpc_dev, uint8_t res)
{
	switch (res) {
	case TYPEC_CC_RP_DFT:
	case TYPEC_CC_RP_1_5:
	case TYPEC_CC_RP_3_0:
		TYPEC_INFO("TypeC-Rp: %d\r\n", res);
		tcpc_dev->typec_local_rp_level = res;
		break;

	default:
		TYPEC_INFO("TypeC-Unknown-Rp (%d)\r\n", res);
		return -1;
	}

#ifdef CONFIG_USB_PD_DBG_ALWAYS_LOCAL_RP
	tcpci_set_cc(tcpc_dev, tcpc_dev->typec_local_rp_level);
#else
	if ((tcpc_dev->typec_attach_old != TYPEC_UNATTACHED) &&
		(tcpc_dev->typec_attach_new != TYPEC_UNATTACHED)) {
		return tcpci_set_cc(tcpc_dev, res);
	}
#endif

	return 0;
}

int tcpc_typec_change_role(
	struct tcpc_device *tcpc_dev, uint8_t typec_role)
{
	if ((tcpc_dev->typec_attach_old != TYPEC_UNATTACHED) ||
		(tcpc_dev->typec_attach_new != TYPEC_UNATTACHED)) {
		return -1;
	}

	if (typec_role >= TYPEC_ROLE_NR) {
		TYPEC_INFO("Wrong TypeC-Role: %d\r\n", typec_role);
		return -2;
	}

	TYPEC_INFO("typec_new_role: %s\r\n", typec_role_name[typec_role]);
	tcpc_dev->typec_role = typec_role;

	typec_unattached_entry(tcpc_dev);
	return 0;
}

int tcpc_typec_init(struct tcpc_device *tcpc_dev, uint8_t typec_role)
{
	if (typec_role >= TYPEC_ROLE_NR) {
		TYPEC_INFO("Wrong TypeC-Role: %d\r\n", typec_role);
		return -2;
	}

	TYPEC_INFO("typec_init: %s\r\n", typec_role_name[typec_role]);

	tcpc_dev->typec_role = typec_role;
	tcpc_dev->typec_attach_new = TYPEC_UNATTACHED;
	tcpc_dev->typec_attach_old = TYPEC_UNATTACHED;

	if (typec_role < TYPEC_ROLE_DRP)
		tcpci_set_cc(tcpc_dev, TYPEC_CC_OPEN);

	typec_unattached_entry(tcpc_dev);
	return 0;
}

void  tcpc_typec_deinit(struct tcpc_device *tcpc_dev)
{
}
