// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "inc/tcpci.h"
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#include "inc/tcpci_typec.h"

#define TCPC_NOTIFY_OVERTIME	(20) /* ms */

#if CONFIG_TCPC_NOTIFICATION_NON_BLOCKING
struct tcp_notify_work {
	struct work_struct work;
	struct tcpc_device *tcpc;
	struct tcp_notify tcp_noti;
	uint8_t type;
	uint8_t state;
};

static void tcp_notify_func(struct work_struct *work)
{
	struct tcp_notify_work *tn_work =
		container_of(work, struct tcp_notify_work, work);
	struct tcpc_device *tcpc = tn_work->tcpc;
	struct tcp_notify *tcp_noti = &tn_work->tcp_noti;
	uint8_t type = tn_work->type;
	uint8_t state = tn_work->state;
#if CONFIG_PD_BEGUG_ON
	long long begin = 0, end = 0;
	int timeval = 0;

	begin = local_clock();
	srcu_notifier_call_chain(&tcpc->evt_nh[type], state, tcp_noti);
	end = local_clock();
	timeval = (end - begin) / NSEC_PER_USEC;
	PD_BUG_ON(timeval > (TCPC_NOTIFY_OVERTIME * 1000));
#else
	srcu_notifier_call_chain(&tcpc->evt_nh[type], state, tcp_noti);
#endif

	kfree(tn_work);
}

static int tcpc_check_notify_time(struct tcpc_device *tcpc,
	struct tcp_notify *tcp_noti, uint8_t type, uint8_t state)
{
	struct tcp_notify_work *tn_work;

	tn_work = kzalloc(sizeof(*tn_work), GFP_KERNEL);
	if (!tn_work)
		return -ENOMEM;

	INIT_WORK(&tn_work->work, tcp_notify_func);
	tn_work->tcpc = tcpc;
	tn_work->tcp_noti = *tcp_noti;
	tn_work->type = type;
	tn_work->state = state;

	return queue_work(tcpc->evt_wq, &tn_work->work) ? 0 : -EAGAIN;
}
#else
static int tcpc_check_notify_time(struct tcpc_device *tcpc,
	struct tcp_notify *tcp_noti, uint8_t type, uint8_t state)
{
	int ret;
#if CONFIG_PD_BEGUG_ON
	struct timeval begin, end;
	int timeval = 0;

	do_gettimeofday(&begin);
	ret = srcu_notifier_call_chain(&tcpc->evt_nh[type], state, tcp_noti);
	do_gettimeofday(&end);
	timeval = (timeval_to_ns(end) - timeval_to_ns(begin))/1000/1000;
	PD_BUG_ON(timeval > TCPC_NOTIFY_OVERTIME);
#else
	ret = srcu_notifier_call_chain(&tcpc->evt_nh[type], state, tcp_noti);
#endif
	return ret;
}
#endif /* CONFIG_TCPC_NOTIFICATION_BLOCKING */

int tcpci_check_vbus_valid_from_ic(struct tcpc_device *tcpc)
{
	uint16_t power_status;
	int vbus_level = tcpc->vbus_level;

	if (tcpci_get_power_status(tcpc, &power_status) == 0) {
		if (vbus_level != tcpc->vbus_level) {
			TCPC_INFO("[Warning] ps_changed %d -> %d\n",
				vbus_level, tcpc->vbus_level);
		}
	}

	return tcpci_check_vbus_valid(tcpc);
}
EXPORT_SYMBOL(tcpci_check_vbus_valid_from_ic);

int tcpci_set_auto_dischg_discnt(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;

	if (tcpc->ops->set_auto_dischg_discnt)
		rv = tcpc->ops->set_auto_dischg_discnt(tcpc, en);

	return rv;
}
EXPORT_SYMBOL(tcpci_set_auto_dischg_discnt);

int tcpci_get_vbus_voltage(struct tcpc_device *tcpc, u32 *vbus)
{
	int rv = 0;

	if (tcpc->ops->get_vbus_voltage)
		rv = tcpc->ops->get_vbus_voltage(tcpc, vbus);

	return rv;
}
EXPORT_SYMBOL(tcpci_get_vbus_voltage);

bool tcpci_check_vsafe0v(struct tcpc_device *tcpc)
{
	return tcpc->vbus_level == TCPC_VBUS_SAFE0V;
}
EXPORT_SYMBOL(tcpci_check_vsafe0v);

int tcpci_alert_status_clear(
	struct tcpc_device *tcpc, uint32_t mask)
{
	PD_BUG_ON(tcpc->ops->alert_status_clear == NULL);

	return tcpc->ops->alert_status_clear(tcpc, mask);
}
EXPORT_SYMBOL(tcpci_alert_status_clear);

int tcpci_fault_status_clear(
	struct tcpc_device *tcpc, uint8_t status)
{
	PD_BUG_ON(tcpc->ops->fault_status_clear == NULL);

	return tcpc->ops->fault_status_clear(tcpc, status);
}
EXPORT_SYMBOL(tcpci_fault_status_clear);

int tcpci_set_alert_mask(struct tcpc_device *tcpc, uint32_t mask)
{
	int rv = 0;

	if (tcpc->ops->set_alert_mask)
		return tcpc->ops->set_alert_mask(tcpc, mask);

	return rv;
}
EXPORT_SYMBOL(tcpci_set_alert_mask);

int tcpci_get_alert_mask(
	struct tcpc_device *tcpc, uint32_t *mask)
{
	PD_BUG_ON(tcpc->ops->get_alert_mask == NULL);

	return tcpc->ops->get_alert_mask(tcpc, mask);
}
EXPORT_SYMBOL(tcpci_get_alert_mask);

int tcpci_get_alert_status(
	struct tcpc_device *tcpc, uint32_t *alert)
{
	PD_BUG_ON(tcpc->ops->get_alert_status == NULL);

	return tcpc->ops->get_alert_status(tcpc, alert);
}
EXPORT_SYMBOL(tcpci_get_alert_status);

int tcpci_get_fault_status(
	struct tcpc_device *tcpc, uint8_t *fault)
{
	if (tcpc->ops->get_fault_status)
		return tcpc->ops->get_fault_status(tcpc, fault);

	*fault = 0;
	return 0;
}
EXPORT_SYMBOL(tcpci_get_fault_status);

int tcpci_get_power_status(
	struct tcpc_device *tcpc, uint16_t *pw_status)
{
	int ret;

	PD_BUG_ON(tcpc->ops->get_power_status == NULL);

	ret = tcpc->ops->get_power_status(tcpc, pw_status);
	if (ret < 0)
		return ret;

	tcpci_vbus_level_init(tcpc, *pw_status);
	return 0;
}
EXPORT_SYMBOL(tcpci_get_power_status);

int tcpci_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	uint16_t power_status;

	PD_BUG_ON(tcpc->ops->init == NULL);

	ret = tcpc->ops->init(tcpc, sw_reset);
	if (ret < 0)
		return ret;

	return tcpci_get_power_status(tcpc, &power_status);
}
EXPORT_SYMBOL(tcpci_init);

int tcpci_init_alert_mask(struct tcpc_device *tcpc)
{
	if (tcpc->ops->init_alert_mask)
		return tcpc->ops->init_alert_mask(tcpc);
	return 0;
}
EXPORT_SYMBOL(tcpci_init_alert_mask);

int tcpci_get_cc(struct tcpc_device *tcpc)
{
	int ret;
	int cc1, cc2;

	PD_BUG_ON(tcpc->ops->get_cc == NULL);

	ret = tcpc->ops->get_cc(tcpc, &cc1, &cc2);
	if (ret < 0)
		return ret;

	if ((cc1 == tcpc->typec_remote_cc[0]) &&
			(cc2 == tcpc->typec_remote_cc[1])) {
		return 0;
	}

	tcpc->typec_remote_cc[0] = cc1;
	tcpc->typec_remote_cc[1] = cc2;

	return 1;
}
EXPORT_SYMBOL(tcpci_get_cc);

int tcpci_is_plugged_in(struct tcpc_device *tcpc)
{
	if (tcpci_check_vbus_valid_from_ic(tcpc))
		return 1;

	if (tcpci_get_cc(tcpc) < 0)
		return -1;

	if ((tcpc->typec_remote_cc[0] == TYPEC_CC_VOLT_OPEN ||
	     tcpc->typec_remote_cc[0] == TYPEC_CC_DRP_TOGGLING) &&
	    (tcpc->typec_remote_cc[1] == TYPEC_CC_VOLT_OPEN ||
	     tcpc->typec_remote_cc[1] == TYPEC_CC_DRP_TOGGLING))
		return 0;

	return 1;
}
EXPORT_SYMBOL(tcpci_is_plugged_in);

int tcpci_set_cc(struct tcpc_device *tcpc, int pull)
{
#if CONFIG_TYPEC_LEGACY3_ALWAYS_LOCAL_RP
	uint8_t rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull);
	uint8_t res = TYPEC_CC_PULL_GET_RES(pull);

	if (rp_lvl == TYPEC_RP_DFT) {
		if (tcpc->typec_state == typec_unattached_snk ||
		    tcpc->typec_state == typec_unattached_src)
			rp_lvl = TYPEC_RP_1_5;
		else
			rp_lvl = tcpc->typec_local_rp_level;
	}
	pull = TYPEC_CC_PULL(rp_lvl, res);
#endif /* CONFIG_TYPEC_LEGACY3_ALWAYS_LOCAL_RP */

	return __tcpci_set_cc(tcpc, pull);
}
EXPORT_SYMBOL(tcpci_set_cc);

int tcpci_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	PD_BUG_ON(tcpc->ops->set_polarity == NULL);

	return tcpc->ops->set_polarity(tcpc, polarity);
}
EXPORT_SYMBOL(tcpci_set_polarity);

int tcpci_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
#if CONFIG_TYPEC_CAP_LOW_RP_DUTY
	if (low_rp)
		TCPC_INFO("low_rp_duty\n");

	if (tcpc->ops->set_low_rp_duty)
		return tcpc->ops->set_low_rp_duty(tcpc, low_rp);
#endif	/* CONFIG_TYPEC_CAP_LOW_RP_DUTY */

	return 0;
}
EXPORT_SYMBOL(tcpci_set_low_rp_duty);

int tcpci_set_vconn(struct tcpc_device *tcpc, int enable)
{
#if CONFIG_TCPC_SOURCE_VCONN
	struct tcp_notify tcp_noti;

	if (tcpc->tcpc_source_vconn == enable)
		return 0;

	tcpc->tcpc_source_vconn = enable;

	tcp_noti.en_state.en = enable != 0;
	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_SOURCE_VCONN);

	if (tcpc->ops->set_vconn)
		return tcpc->ops->set_vconn(tcpc, enable);
#endif	/* CONFIG_TCPC_SOURCE_VCONN */

	return 0;
}
EXPORT_SYMBOL(tcpci_set_vconn);

int tcpci_is_low_power_mode(struct tcpc_device *tcpc)
{
	int rv = 1;

#if CONFIG_TCPC_LOW_POWER_MODE
	if (tcpc->ops->is_low_power_mode)
		rv = tcpc->ops->is_low_power_mode(tcpc);
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	return rv;
}
EXPORT_SYMBOL(tcpci_is_low_power_mode);

int tcpci_set_low_power_mode(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;
#if CONFIG_TCPC_LOW_POWER_MODE
	int pull = TYPEC_CC_OPEN, i = 0;

	tcpc->typec_lpm = en;
	if (!tcpc_typec_is_cc_open_state(tcpc)) {
		switch (tcpc->typec_role) {
		case TYPEC_ROLE_SNK:
			pull = TYPEC_CC_RD;
			break;
		case TYPEC_ROLE_SRC:
			pull = TYPEC_CC_RP;
			break;
		default:
			pull = TYPEC_CC_DRP;
			break;
		}
	}

	/* [Workaround]
	 * rx_buffer can't clear, try to reset protocol before disable bmc clock
	 */
	if (en) {
		rv = tcpci_protocol_reset(tcpc);
		for (i = 0; i < 2; i++) {
			rv = tcpci_alert_status_clear(tcpc,
				TCPC_REG_ALERT_RX_ALL_MASK);
			if (rv < 0)
				TCPC_INFO("%s:%d clear rx event fail\n",
					  __func__, i);
		}
	}
	if (tcpc->ops->set_low_power_mode)
		rv = tcpc->ops->set_low_power_mode(tcpc, en, pull);
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	return rv;
}
EXPORT_SYMBOL(tcpci_set_low_power_mode);

int tcpci_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATCHDOG_EN)
		if (tcpc->ops->set_watchdog)
			rv = tcpc->ops->set_watchdog(tcpc, en);

	return rv;
}
EXPORT_SYMBOL(tcpci_set_watchdog);

int tcpci_alert_vendor_defined_handler(struct tcpc_device *tcpc)
{
	int rv = 0;

	if (tcpc->ops->alert_vendor_defined_handler)
		rv = tcpc->ops->alert_vendor_defined_handler(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpci_alert_vendor_defined_handler);

int tcpci_is_vsafe0v(struct tcpc_device *tcpc)
{
	int rv = -EOPNOTSUPP;

	if (tcpc->ops->is_vsafe0v)
		rv = tcpc->ops->is_vsafe0v(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpci_is_vsafe0v);

int tcpci_get_chip_id(struct tcpc_device *tcpc, uint32_t *chip_id)
{
       if (tcpc->ops->get_chip_id == NULL)
               return -ENOTSUPP;
       else
               tcpc->ops->get_chip_id(tcpc, chip_id);

       return 0;
}
EXPORT_SYMBOL(tcpci_get_chip_id);

#if CONFIG_WATER_DETECTION
int tcpci_set_water_protection(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->ops->set_water_protection)
		return tcpc->ops->set_water_protection(tcpc, en);
	return 0;
}
EXPORT_SYMBOL(tcpci_set_water_protection);

int tcpci_notify_wd_status(struct tcpc_device *tcpc, bool water_detected)
{
	struct tcp_notify tcp_noti;

	tcp_noti.wd_status.water_detected = water_detected;
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_WD_STATUS);
}
EXPORT_SYMBOL(tcpci_notify_wd_status);
#endif /* CONFIG_WATER_DETECTION */
int tcpci_notify_fod_status(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;

	tcp_noti.fod_status.fod = tcpc->typec_fod;
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_FOD_STATUS);
}
EXPORT_SYMBOL(tcpci_notify_fod_status);

#if CONFIG_CABLE_TYPE_DETECTION
int tcpci_notify_cable_type(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;

	tcp_noti.cable_type.type = tcpc->typec_cable_type;
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_CABLE_TYPE);
}
EXPORT_SYMBOL(tcpci_notify_cable_type);
#endif /* CONFIG_CABLE_TYPE_DETECTION */

int tcpci_notify_typec_otp(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;

	tcp_noti.typec_otp.otp = tcpc->typec_otp;
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_TYPEC_OTP);
}
EXPORT_SYMBOL(tcpci_notify_typec_otp);

int tcpci_set_cc_hidet(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;

	if (tcpc->ops->set_cc_hidet)
		rv = tcpc->ops->set_cc_hidet(tcpc, en);

	return rv;
}
EXPORT_SYMBOL(tcpci_set_cc_hidet);

int tcpci_notify_wd0_state(struct tcpc_device *tcpc, bool wd0_state)
{
	struct tcp_notify tcp_noti;

	tcp_noti.wd0_state.wd0 = wd0_state;

	TCPC_DBG("wd0: %d\n", wd0_state);
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_WD0_STATE);
}
EXPORT_SYMBOL(tcpci_notify_wd0_state);

int tcpci_notify_plug_out(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;

	memset(&tcp_noti, 0, sizeof(tcp_noti));
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_PLUG_OUT);
}
EXPORT_SYMBOL(tcpci_notify_plug_out);

int tcpci_set_floating_ground(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;

	if (tcpc->ops->set_floating_ground)
		rv = tcpc->ops->set_floating_ground(tcpc, en);

	return rv;
}
EXPORT_SYMBOL(tcpci_set_floating_ground);

int tcpci_set_otp_fwen(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;

	if (tcpc->ops->set_otp_fwen)
		rv = tcpc->ops->set_otp_fwen(tcpc, en);

	return rv;
}
EXPORT_SYMBOL(tcpci_set_otp_fwen);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)

int tcpci_set_msg_header(struct tcpc_device *tcpc,
	uint8_t power_role, uint8_t data_role)
{
	PD_BUG_ON(tcpc->ops->set_msg_header == NULL);

	return tcpc->ops->set_msg_header(tcpc, power_role, data_role);
}
EXPORT_SYMBOL(tcpci_set_msg_header);

int tcpci_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	PD_BUG_ON(tcpc->ops->set_rx_enable == NULL);

	return tcpc->ops->set_rx_enable(tcpc, enable);
}
EXPORT_SYMBOL(tcpci_set_rx_enable);

int tcpci_protocol_reset(struct tcpc_device *tcpc)
{
	if (tcpc->ops->protocol_reset)
		return tcpc->ops->protocol_reset(tcpc);

	return 0;
}
EXPORT_SYMBOL(tcpci_protocol_reset);

int tcpci_get_message(struct tcpc_device *tcpc,
	uint32_t *payload, uint16_t *head, enum tcpm_transmit_type *type)
{
	PD_BUG_ON(tcpc->ops->get_message == NULL);

	return tcpc->ops->get_message(tcpc, payload, head, type);
}
EXPORT_SYMBOL(tcpci_get_message);

void tcpc_tx_pending_work_func(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(work, struct tcpc_device,
						tx_pending_work.work);

	atomic_dec_if_positive(&tcpc->tx_pending);
	if (!atomic_read(&tcpc->tx_pending))
		wake_up(&tcpc->tx_wait_que);
}

static void tcpc_wait_tx_done(struct tcpc_device *tcpc)
{
	long ret = 0;
	const u64 j = jiffies;

	if (time_after_eq64(j, tcpc->tx_jiffies + tcpc->tx_jiffies_max))
		return;

	ret = wait_event_timeout(tcpc->tx_wait_que,
				 !atomic_read(&tcpc->tx_pending),
				 tcpc->tx_jiffies + tcpc->tx_jiffies_max - j);
	TCPC_INFO("%s ret = %ld\n", __func__, ret);
}

int tcpci_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	int ret = 0;
	unsigned int bits = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	PD_BUG_ON(tcpc->ops->transmit == NULL);

	tcpc_wait_tx_done(tcpc);

	mutex_lock(&pd_port->rxbuf_lock);
	ret = tcpc->ops->transmit(tcpc, type, header, data);
	mutex_unlock(&pd_port->rxbuf_lock);
	if (ret < 0)
		return ret;

	atomic_inc(&tcpc->tx_pending);
	tcpc->tx_jiffies = jiffies;
	switch (type) {
	case TCPC_TX_HARD_RESET:
	case TCPC_TX_CABLE_RESET:
		/*
		 * 64bits preamble + 20bits k-code
		 * Max_tUI = 3700ns
		 */
		tcpc->tx_jiffies_max = nsecs_to_jiffies((64 + 20) * 3700);
		break;
	case TCPC_TX_BIST_MODE_2:
		/* tBISTContMode = 30ms ~ 60ms */
		tcpc->tx_jiffies_max = msecs_to_jiffies(60);
		flush_delayed_work(&tcpc->tx_pending_work);
		schedule_delayed_work(&tcpc->tx_pending_work,
				      tcpc->tx_jiffies_max);
		break;
	case TCPC_TX_SOP:
	case TCPC_TX_SOP_PRIME:
	case TCPC_TX_SOP_PRIME_PRIME:
	case TCPC_TX_SOP_DEBUG_PRIME:
	case TCPC_TX_SOP_DEBUG_PRIME_PRIME:
	default:
		/*
		 * 64bits preamble + 20bits k-code +
		 * maximum 30bytes data (header + payload) in 4b +
		 * 4bytes CRC in 4b +
		 * 5bits k-code (EOP)
		 * Max_tUI = 3700ns
		 */
		bits = 64 + 20 +
			(2 + 4 * PD_HEADER_CNT(header) + 4) * 8 * 5 / 4 + 5;
		tcpc->tx_jiffies_max = nsecs_to_jiffies(bits * 3700);

		/* tReceive = 0.9ms ~ 1.1ms */
		tcpc->tx_jiffies_max =
			tcpc->tx_jiffies_max * (tcpc->pd_retry_count + 1) +
			usecs_to_jiffies(1100) * tcpc->pd_retry_count;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(tcpci_transmit);

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
int tcpci_retransmit(struct tcpc_device *tcpc)
{
	int ret = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	tcpc_wait_tx_done(tcpc);

	mutex_lock(&pd_port->rxbuf_lock);
	ret = tcpc->ops->retransmit(tcpc);
	mutex_unlock(&pd_port->rxbuf_lock);
	if (ret < 0)
		return ret;

	atomic_inc(&tcpc->tx_pending);
	tcpc->tx_jiffies = jiffies;

	return ret;
}
EXPORT_SYMBOL(tcpci_retransmit);
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

int tcpci_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->ops->set_bist_test_mode)
		return tcpc->ops->set_bist_test_mode(tcpc, en);

	return 0;
}
EXPORT_SYMBOL(tcpci_set_bist_test_mode);

int tcpci_set_bist_carrier_mode(struct tcpc_device *tcpc, uint8_t pattern)
{
	PD_BUG_ON(tcpc->ops->set_bist_carrier_mode == NULL);

	if (pattern)	/* wait for GoodCRC */
		udelay(240);

	return tcpc->ops->set_bist_carrier_mode(tcpc, pattern);
}
EXPORT_SYMBOL(tcpci_set_bist_carrier_mode);
#endif	/* CONFIG_USB_POWER_DELIVERY */

int tcpci_notify_typec_state(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;

	tcp_noti.typec_state.polarity = tcpc->typec_polarity;
	tcp_noti.typec_state.old_state = tcpc->typec_attach_old;
	tcp_noti.typec_state.new_state = tcpc->typec_attach_new;
	tcp_noti.typec_state.rp_level = tcpc->typec_remote_rp_level;
	tcp_noti.typec_state.local_rp_level = tcpc->typec_local_rp_level;

	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_USB, TCP_NOTIFY_TYPEC_STATE);
}
EXPORT_SYMBOL(tcpci_notify_typec_state);

int tcpci_notify_role_swap(
	struct tcpc_device *tcpc, uint8_t event, uint8_t role)
{
	struct tcp_notify tcp_noti;

	tcp_noti.swap_state.new_role = role;
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, event);
}
EXPORT_SYMBOL(tcpci_notify_role_swap);

int tcpci_notify_pd_state(struct tcpc_device *tcpc, uint8_t connect)
{
	struct tcp_notify tcp_noti;

	tcp_noti.pd_state.connected = connect;
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_USB, TCP_NOTIFY_PD_STATE);
}
EXPORT_SYMBOL(tcpci_notify_pd_state);

int tcpci_set_intrst(struct tcpc_device *tcpc, bool en)
{
#if CONFIG_TCPC_INTRST_EN
	if (tcpc->ops->set_intrst)
		tcpc->ops->set_intrst(tcpc, en);
#endif	/* CONFIG_TCPC_INTRST_EN */

	return 0;
}
EXPORT_SYMBOL(tcpci_set_intrst);

int tcpci_enable_watchdog(struct tcpc_device *tcpc, bool en)
{
	if (!(tcpc->tcpc_flags & TCPC_FLAGS_WATCHDOG_EN))
		return 0;

	TCPC_DBG2("enable_WG: %d\n", en);

	if (tcpc->typec_watchdog == en)
		return 0;

	mutex_lock(&tcpc->access_lock);
	tcpc->typec_watchdog = en;

	if (tcpc->ops->set_watchdog)
		tcpc->ops->set_watchdog(tcpc, en);

#if CONFIG_TCPC_INTRST_EN
	if (!en || tcpc->attach_wake_lock.active)
		tcpci_set_intrst(tcpc, en);
#endif	/* CONFIG_TCPC_INTRST_EN */

	mutex_unlock(&tcpc->access_lock);

	return 0;
}
EXPORT_SYMBOL(tcpci_enable_watchdog);

int tcpci_source_vbus(
	struct tcpc_device *tcpc, uint8_t type, int mv, int ma)
{
	struct tcp_notify tcp_noti;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (type >= TCP_VBUS_CTRL_PD &&
			tcpc->pd_port.pe_data.pd_prev_connected)
		type |= TCP_VBUS_CTRL_PD_DETECT;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	if (ma < 0) {
		if (mv != 0) {
			switch (tcpc->typec_local_rp_level) {
			case TYPEC_RP_3_0:
				ma = 3000;
				break;
			case TYPEC_RP_1_5:
				ma = 1500;
				break;
			case TYPEC_RP_DFT:
			default:
				ma = CONFIG_TYPEC_SRC_CURR_DFT;
				break;
			}
		} else
			ma = 0;
	}

	tcp_noti.vbus_state.ma = ma;
	tcp_noti.vbus_state.mv = mv;
	tcp_noti.vbus_state.type = type;

	tcpci_enable_watchdog(tcpc, mv != 0);
	TCPC_DBG("source_vbus: %d mV, %d mA\n", mv, ma);
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_SOURCE_VBUS);
}
EXPORT_SYMBOL(tcpci_source_vbus);

int tcpci_sink_vbus(
	struct tcpc_device *tcpc, uint8_t type, int mv, int ma)
{
	struct tcp_notify tcp_noti;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (type >= TCP_VBUS_CTRL_PD &&
			tcpc->pd_port.pe_data.pd_prev_connected)
		type |= TCP_VBUS_CTRL_PD_DETECT;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	if (ma < 0) {
		if (mv != 0) {
			switch (tcpc->typec_remote_rp_level) {
			case TYPEC_CC_VOLT_SNK_1_5:
				ma = 1500;
				break;
			case TYPEC_CC_VOLT_SNK_3_0:
				ma = 3000;
				break;
			case TYPEC_CC_VOLT_SNK_DFT:
			default:
				ma = tcpc->typec_usb_sink_curr;
				break;
			}
#if CONFIG_TYPEC_SNK_CURR_LIMIT > 0
			if (ma > CONFIG_TYPEC_SNK_CURR_LIMIT)
				ma = CONFIG_TYPEC_SNK_CURR_LIMIT;
#endif	/* CONFIG_TYPEC_SNK_CURR_LIMIT */
		} else
			ma = 0;
	}

	tcp_noti.vbus_state.ma = ma;
	tcp_noti.vbus_state.mv = mv;
	tcp_noti.vbus_state.type = type;

	TCPC_DBG("sink_vbus: %d mV, %d mA\n", mv, ma);
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_SINK_VBUS);
}
EXPORT_SYMBOL(tcpci_sink_vbus);

int tcpci_disable_vbus_control(struct tcpc_device *tcpc)
{
#if CONFIG_TYPEC_USE_DIS_VBUS_CTRL
	struct tcp_notify tcp_noti;

	TCPC_DBG("disable_vbus\n");
	tcpci_enable_watchdog(tcpc, false);

	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_DIS_VBUS_CTRL);
#else
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_REMOVE, TCPC_VBUS_SINK_0V, 0);
	tcpci_source_vbus(tcpc, TCP_VBUS_CTRL_REMOVE, TCPC_VBUS_SOURCE_0V, 0);
	return 0;
#endif	/* CONFIG_TYPEC_USE_DIS_VBUS_CTRL */
}
EXPORT_SYMBOL(tcpci_disable_vbus_control);

int tcpci_notify_attachwait_state(struct tcpc_device *tcpc, bool as_sink)
{
#if CONFIG_TYPEC_NOTIFY_ATTACHWAIT
	uint8_t notify = 0;
	struct tcp_notify tcp_noti;

#if CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK
	if (as_sink)
		notify = TCP_NOTIFY_ATTACHWAIT_SNK;
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */

#if CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SRC
	if (!as_sink)
		notify = TCP_NOTIFY_ATTACHWAIT_SRC;
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SRC */

	if (notify == 0)
		return 0;

	memset(&tcp_noti, 0, sizeof(tcp_noti));
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, notify);
#else
	return 0;
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT */

}
EXPORT_SYMBOL(tcpci_notify_attachwait_state);

int tcpci_enable_auto_discharge(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;

#if CONFIG_TYPEC_CAP_AUTO_DISCHARGE
#if CONFIG_TCPC_AUTO_DISCHARGE_IC
	if (tcpc->typec_auto_discharge != en) {
		tcpc->typec_auto_discharge = en;
		if (tcpc->ops->set_auto_discharge)
			ret = tcpc->ops->set_auto_discharge(tcpc, en);
	}
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

	return ret;
}
EXPORT_SYMBOL(tcpci_enable_auto_discharge);

static int __tcpci_enable_force_discharge(
	struct tcpc_device *tcpc, bool en, int mv)
{
	int ret = 0;

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
	if (tcpc->typec_force_discharge != en) {
		tcpc->typec_force_discharge = en;
		if (tcpc->ops->set_force_discharge)
			ret = tcpc->ops->set_force_discharge(tcpc, en, mv);
	}
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

	return ret;
}

#if CONFIG_TCPC_FORCE_DISCHARGE_EXT
static int __tcpci_enable_ext_discharge(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;
#if CONFIG_TCPC_EXT_DISCHARGE
	struct tcp_notify tcp_noti;

	if (tcpc->typec_ext_discharge != en) {
		tcpc->typec_ext_discharge = en;
		tcp_noti.en_state.en = en;
		TCPC_DBG("EXT-Discharge: %d\n", en);
		ret = tcpc_check_notify_time(tcpc, &tcp_noti,
			TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_EXT_DISCHARGE);
	}
#endif	/* CONFIG_TCPC_EXT_DISCHARGE */

	return ret;
}
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_EXT */

int tcpci_enable_force_discharge(struct tcpc_device *tcpc, bool en, int mv)
{
	int ret = 0;

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
	ret = __tcpci_enable_force_discharge(tcpc, en, mv);
#if CONFIG_TCPC_FORCE_DISCHARGE_EXT
	ret = __tcpci_enable_ext_discharge(tcpc, en);
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_EXT */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

	return ret;
}
EXPORT_SYMBOL(tcpci_enable_force_discharge);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)

int tcpci_notify_hard_reset_state(struct tcpc_device *tcpc, uint8_t state)
{
	struct tcp_notify tcp_noti;

	tcp_noti.hreset_state.state = state;

	if (state >= TCP_HRESET_SIGNAL_SEND)
		tcpc->pd_wait_hard_reset_complete = true;
	else if (tcpc->pd_wait_hard_reset_complete)
		tcpc->pd_wait_hard_reset_complete = false;
	else
		return 0;

	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_HARD_RESET_STATE);
}
EXPORT_SYMBOL(tcpci_notify_hard_reset_state);

int tcpci_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, uint32_t mode)
{
	struct tcp_notify tcp_noti;

	tcp_noti.mode_ctrl.svid = svid;
	tcp_noti.mode_ctrl.ops = ops;
	tcp_noti.mode_ctrl.mode = mode;

	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_ENTER_MODE);
}
EXPORT_SYMBOL(tcpci_enter_mode);

int tcpci_exit_mode(struct tcpc_device *tcpc, uint16_t svid)
{
	struct tcp_notify tcp_noti;

	tcp_noti.mode_ctrl.svid = svid;
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_EXIT_MODE);

}
EXPORT_SYMBOL(tcpci_exit_mode);

#if CONFIG_USB_PD_ALT_MODE

int tcpci_report_hpd_state(struct tcpc_device *tcpc, uint32_t dp_status)
{
	struct tcp_notify tcp_noti;
	struct dp_data *dp_data = pd_get_dp_data(&tcpc->pd_port);

	/* UFP_D to DFP_D only */

	if (PD_DP_CFG_DFP_D(dp_data->local_config)) {
		tcp_noti.ama_dp_hpd_state.irq = PD_VDO_DPSTS_HPD_IRQ(dp_status);
		tcp_noti.ama_dp_hpd_state.state =
					PD_VDO_DPSTS_HPD_LVL(dp_status);
		tcpc_check_notify_time(tcpc, &tcp_noti,
			TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_HPD_STATE);
	}

	return 0;
}
EXPORT_SYMBOL(tcpci_report_hpd_state);

int tcpci_dp_status_update(struct tcpc_device *tcpc, uint32_t dp_status)
{
	DP_INFO("Status0: 0x%x\n", dp_status);
	tcpci_report_hpd_state(tcpc, dp_status);
	return 0;
}
EXPORT_SYMBOL(tcpci_dp_status_update);

int tcpci_dp_configure(struct tcpc_device *tcpc, uint32_t dp_config)
{
	struct tcp_notify tcp_noti;
	uint8_t active = 1;

	DP_INFO("LocalCFG: 0x%x\n", dp_config);

	switch (PD_DP_CFG_ROLE(dp_config)) {
	case DP_CONFIG_DFP_D:
		tcp_noti.ama_dp_state.sel_config = SW_DFP_D;
		break;
	case DP_CONFIG_UFP_D:
		tcp_noti.ama_dp_state.sel_config = SW_UFP_D;
		break;
	case DP_CONFIG_USB:
	default:
		tcp_noti.ama_dp_state.sel_config = SW_USB;
		active = 0;
		break;
	}
	tcp_noti.ama_dp_state.signal = PD_DP_CFG_SIGNAL(dp_config);
	tcp_noti.ama_dp_state.pin_assignment = PD_DP_CFG_PIN(dp_config);
	DP_INFO("pin assignment: 0x%x\n",
		tcp_noti.ama_dp_state.pin_assignment);
	tcp_noti.ama_dp_state.polarity = tcpc->typec_polarity;
	tcp_noti.ama_dp_state.active = active;

	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_STATE);
}
EXPORT_SYMBOL(tcpci_dp_configure);

int tcpci_dp_attention(struct tcpc_device *tcpc, uint32_t dp_status)
{
	/* DFP_U : Not call this function during internal flow */
	struct tcp_notify tcp_noti;

	DP_INFO("Attention: 0x%x\n", dp_status);
	tcp_noti.ama_dp_attention.state = (uint8_t) dp_status;
	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_ATTENTION);
	return tcpci_report_hpd_state(tcpc, dp_status);
}
EXPORT_SYMBOL(tcpci_dp_attention);

int tcpci_dp_notify_status_update_done(
	struct tcpc_device *tcpc, uint32_t dp_status, bool ack)
{
	/* DFP_U : Not call this function during internal flow */
	DP_INFO("Status1: 0x%x, ack=%d\n", dp_status, ack);
	return 0;
}
EXPORT_SYMBOL(tcpci_dp_notify_status_update_done);

int tcpci_dp_notify_config_start(struct tcpc_device *tcpc)
{
	/* DFP_U : Put signal & mux into the Safe State */
	struct tcp_notify tcp_noti;

	DP_INFO("ConfigStart\n");
	memset(&tcp_noti, 0, sizeof(tcp_noti));
	tcp_noti.ama_dp_state.sel_config = SW_USB;
	tcp_noti.ama_dp_state.polarity = tcpc->typec_polarity;
	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_STATE);
	return 0;
}
EXPORT_SYMBOL(tcpci_dp_notify_config_start);

int tcpci_dp_notify_config_done(struct tcpc_device *tcpc,
	uint32_t local_cfg, uint32_t remote_cfg, bool ack)
{
	/* DFP_U : If DP success,
	 * internal flow will enter this function finally
	 */
	DP_INFO("ConfigDone, L:0x%x, R:0x%x, ack=%d\n",
		local_cfg, remote_cfg, ack);

	if (ack)
		tcpci_dp_configure(tcpc, local_cfg);

	return 0;
}
EXPORT_SYMBOL(tcpci_dp_notify_config_done);

#endif	/* CONFIG_USB_PD_ALT_MODE */

#if CONFIG_USB_PD_CUSTOM_VDM
int tcpci_notify_uvdm(struct tcpc_device *tcpc, bool ack)
{
	struct tcp_notify tcp_noti;
	struct pd_port *pd_port = &tcpc->pd_port;

	tcp_noti.uvdm_msg.ack = ack;

	if (ack) {
		tcp_noti.uvdm_msg.uvdm_cnt = pd_port->uvdm_cnt;
		tcp_noti.uvdm_msg.uvdm_svid = pd_port->uvdm_svid;
		tcp_noti.uvdm_msg.uvdm_data = pd_port->uvdm_data;
	}

	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_UVDM);
	return 0;
}
EXPORT_SYMBOL(tcpci_notify_uvdm);
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

/* ---- Policy Engine (PD30) ---- */

#if CONFIG_USB_PD_REV30

#if CONFIG_USB_PD_REV30_ALERT_REMOTE
int tcpci_notify_alert(struct tcpc_device *tcpc, uint32_t ado)
{
	struct tcp_notify tcp_noti;

	tcp_noti.alert_msg.ado = ado;
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_ALERT);
}
EXPORT_SYMBOL(tcpci_notify_alert);
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

#if CONFIG_USB_PD_REV30_STATUS_REMOTE
int tcpci_notify_status(struct tcpc_device *tcpc, struct pd_status *sdb)
{
	struct tcp_notify tcp_noti;

	tcp_noti.status_msg.sdb = sdb;
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_STATUS);
}
EXPORT_SYMBOL(tcpci_notify_status);
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#if CONFIG_USB_PD_REV30_BAT_INFO
int tcpci_notify_request_bat_info(
	struct tcpc_device *tcpc, enum pd_battery_reference ref)
{
	struct tcp_notify tcp_noti;

	tcp_noti.request_bat.ref = ref;
	return tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_REQUEST_BAT_INFO);
}
EXPORT_SYMBOL(tcpci_notify_request_bat_info);
#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */
#endif	/* CONFIG_USB_PD_REV30 */
#endif	/* CONFIG_USB_POWER_DELIVERY */
