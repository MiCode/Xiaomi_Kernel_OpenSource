// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_nic: " fmt

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "sipa_priv.h"
#include "sipa_hal.h"

#define SIPA_CP_SRC ((1 << SIPA_TERM_VAP0) | (1 << SIPA_TERM_VAP1) |\
		(1 << SIPA_TERM_VAP2) | (1 << SIPA_TERM_CP0) | \
		(1 << SIPA_TERM_CP1) | (1 << SIPA_TERM_VCP))

#define SIPA_PCIE_SRC ((1 << SIPA_TERM_PCIE0) | (1 << SIPA_TERM_PCIE1) | \
		(1 << SIPA_TERM_PCIE2))

#define SIPA_WIFI_SRC ((1 << SIPA_TERM_WIFI1) | (1 << SIPA_TERM_WIFI2))

#define SIPA_NIC_RM_INACTIVE_TIMER	3000

struct sipa_nic_statics_info {
	u32 src_mask;
	int netid;
	enum sipa_rm_res_id cons;
};

static struct sipa_nic_statics_info s_sipa_nic_statics[SIPA_NIC_MAX] = {
	{
		.src_mask = (1 << SIPA_TERM_USB),
		.netid = -1,
		.cons = SIPA_RM_RES_CONS_USB,
	},
	{
		.src_mask = SIPA_WIFI_SRC,
		.netid = -1,
		.cons = SIPA_RM_RES_CONS_WIFI_DL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 0,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 1,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 2,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 3,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 4,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 5,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 6,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 7,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 8,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 9,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 10,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 11,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 12,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 13,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 14,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_CP_SRC,
		.netid = 15,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.src_mask = SIPA_PCIE_SRC,
		.netid = 0,
		.cons = SIPA_RM_RES_CONS_WWAN_DL,
	},
	{
		.src_mask = SIPA_PCIE_SRC,
		.netid = 1,
		.cons = SIPA_RM_RES_CONS_WWAN_DL,
	},
	{
		.src_mask = SIPA_PCIE_SRC,
		.netid = 2,
		.cons = SIPA_RM_RES_CONS_WWAN_DL,
	},
	{
		.src_mask = SIPA_PCIE_SRC,
		.netid = 3,
		.cons = SIPA_RM_RES_CONS_WWAN_DL,
	},
};

static void sipa_nic_rm_res_release(struct sipa_nic *nic);

static bool sipa_nic_rm_res_granted(struct sipa_nic *nic)
{
	bool ret;
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;

	spin_lock_irqsave(&res->lock, flags);
	res->request_in_progress = false;
	ret = (atomic_read(&nic->status) == NIC_OPEN) &&
		nic->rm_res.rm_flow_ctrl;
	if (ret)
		nic->rm_res.rm_flow_ctrl = 0;
	spin_unlock_irqrestore(&res->lock, flags);
	return ret;
}

static bool sipa_nic_rm_res_reinit(struct sipa_nic *nic)
{
	bool ret;
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;

	spin_lock_irqsave(&res->lock, flags);
	ret = (atomic_read(&nic->status) == NIC_OPEN);
	if (ret)
		nic->rm_res.rm_flow_ctrl = 0;
	spin_unlock_irqrestore(&res->lock, flags);

	return ret;
}

static void sipa_nic_alarm_start(struct sipa_nic_cons_res *res)
{
	ktime_t now, add;

	now = ktime_get_boottime();
	add = ktime_set(SIPA_NIC_RM_INACTIVE_TIMER / MSEC_PER_SEC,
			(SIPA_NIC_RM_INACTIVE_TIMER % MSEC_PER_SEC)
			* NSEC_PER_SEC);
	alarm_start(&res->delay_timer, ktime_add(now, add));
}

static void sipa_nic_rm_notify_cb(void *user_data, enum sipa_rm_event event,
				  unsigned long data)
{
	struct sipa_nic *nic = user_data;

	switch (event) {
	case SIPA_RM_EVT_GRANTED:
		if (sipa_nic_rm_res_granted(nic))
			nic->cb(nic->cb_priv, SIPA_LEAVE_FLOWCTRL, 0);
		break;
	case SIPA_RM_EVT_RELEASED:
		if (sipa_nic_rm_res_reinit(nic))
			nic->cb(nic->cb_priv, SIPA_LEAVE_FLOWCTRL, 0);
		break;
	default:
		pr_err("unknown event %d\n", event);
		break;
	}
}

static int sipa_nic_register_rm(struct sipa_nic *nic, enum sipa_nic_id nic_id)
{
	struct sipa_rm_register_params r_param;

	r_param.user_data = nic;
	r_param.notify_cb = sipa_nic_rm_notify_cb;

	return sipa_rm_register(s_sipa_nic_statics[nic_id].cons, &r_param);
}

static int sipa_nic_deregister_rm(struct sipa_nic *nic, enum sipa_nic_id nic_id)
{
	struct sipa_rm_register_params r_param;

	r_param.user_data = nic;
	r_param.notify_cb = sipa_nic_rm_notify_cb;

	return sipa_rm_deregister(s_sipa_nic_statics[nic_id].cons, &r_param);
}

static enum alarmtimer_restart sipa_nic_alarm_to_release(struct alarm *alarm,
							 ktime_t time)
{
	struct sipa_nic_cons_res *res = container_of(alarm,
						     struct sipa_nic_cons_res,
						     delay_timer);
	ktime_t now, add;
	unsigned long flags;
	bool release_flag = false;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	now = ktime_get_boottime();
	add = ktime_set(SIPA_NIC_RM_INACTIVE_TIMER / MSEC_PER_SEC,
			(SIPA_NIC_RM_INACTIVE_TIMER % MSEC_PER_SEC) *
			NSEC_PER_SEC);

	pr_debug("timer expired for resource %d!\n",
		 res->cons);

	spin_lock_irqsave(&res->lock, flags);
	/* need check resource not used any more */
	if (res->reschedule_work || !res->chk_func(res->chk_priv)) {
		pr_debug("setting delayed work\n");
		res->reschedule_work = false;
		alarm_forward(&res->delay_timer, now, add);
		spin_unlock_irqrestore(&res->lock, flags);
		return ALARMTIMER_RESTART;
	} else if (res->resource_requested) {
		pr_debug("not calling release\n");
		alarm_forward(&res->delay_timer, now, add);
		spin_unlock_irqrestore(&res->lock, flags);
		return ALARMTIMER_RESTART;
	}

	pr_info("calling release_resource on resource %d!\n",
		 res->cons);
	release_flag = true;
	res->need_request = true;
	res->release_in_progress = false;
	res->request_in_progress = false;

	spin_unlock_irqrestore(&res->lock, flags);

	if (release_flag) {
		sipa_rm_release_resource(res->cons);
		if (!ipa->sipa_sys_eb) {
			pm_relax(ipa->dev);
			pr_info("ul nic to pm relax\n");
		}
	}

	return ALARMTIMER_NORESTART;
}

static int sipa_nic_rm_init(struct sipa_nic_cons_res *res,
			    struct sipa_skb_sender *sender,
			    enum sipa_rm_res_id cons,
			    unsigned long msecs)
{
	if (res->initied)
		return -EEXIST;

	res->initied = true;
	res->cons = cons;
	spin_lock_init(&res->lock);
	res->chk_func = (sipa_check_send_completed)
		sipa_skb_sender_check_send_complete;
	res->chk_priv = sender;
	res->jiffies = msecs_to_jiffies(msecs);
	res->resource_requested = false;
	res->reschedule_work = false;
	res->release_in_progress = false;
	res->need_request = true;
	res->request_in_progress = false;
	res->rm_flow_ctrl = 0;

	alarm_init(&res->delay_timer, ALARM_BOOTTIME,
		   sipa_nic_alarm_to_release);

	return 0;
}

static int sipa_nic_rm_res_request(struct sipa_nic *nic)
{
	bool need_flag = false;
	int ret = 0;
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	spin_lock_irqsave(&res->lock, flags);
	res->resource_requested = true;
	if (res->need_request && !res->request_in_progress) {
		need_flag = true;
		res->need_request = false;
		res->request_in_progress = true;
		res->rm_flow_ctrl = 1;
	} else {
		ret = res->request_in_progress ? -EINPROGRESS : 0;
	}

	if (ret == -EINPROGRESS)
		res->rm_flow_ctrl = 1;
	spin_unlock_irqrestore(&res->lock, flags);

	if (need_flag) {
		if (!ipa->sipa_sys_eb) {
			pm_stay_awake(ipa->dev);
			pr_info("ul nic to pm stay awake\n");
		}
		ret = sipa_rm_request_resource(nic->rm_res.cons);
		spin_lock_irqsave(&res->lock, flags);
		res->need_request = false;
		if (ret == -EINPROGRESS)
			res->request_in_progress = true;
		else
			res->request_in_progress = false;

		spin_unlock_irqrestore(&res->lock, flags);
	}

	return ret;
}

static void sipa_nic_rm_res_release(struct sipa_nic *nic)
{
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;

	spin_lock_irqsave(&res->lock, flags);
	res->resource_requested = false;
	if (res->release_in_progress) {
		res->reschedule_work = true;
		spin_unlock_irqrestore(&res->lock, flags);
		return;
	}

	res->release_in_progress = true;
	res->reschedule_work = false;
	sipa_nic_alarm_start(res);

	spin_unlock_irqrestore(&res->lock, flags);
}

/**
 * sipa_nic_notify_evt() - notify the arrival of nic device events.
 * @nic: nic devices that need to be notified.
 * @evt: event type,
 *
 * This function use callback function to notify events.
 */
void sipa_nic_notify_evt(struct sipa_nic *nic, enum sipa_evt_type evt)
{
	if (nic->cb)
		nic->cb(nic->cb_priv, evt, 0);
}
EXPORT_SYMBOL(sipa_nic_notify_evt);

/**
 * sipa_nic_open() - open a nic device and create and initialize the resources
 *                   it needs.
 * @src: the src id corresponding to this nic device.
 * @netid: the netid of the network card device corresponding to this
 *         nic device,
 * @cb: the callback used to notify the network card device after the event is
 *      triggered.
 * @priv: it will be passed in when the callback function is called.
 *
 * After this interface is called, the return value is its corresponding nic id,
 * normal situation is greater than or equal to 0, otherwise abnormal.
 */
int sipa_nic_open(enum sipa_term_type src, int netid,
		  sipa_notify_cb cb, void *priv)
{
	int i, ret;
	unsigned long flags;
	struct sipa_nic *nic = NULL;
	struct sipa_skb_sender *sender;
	enum sipa_nic_id nic_id = SIPA_NIC_MAX;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}

	sender = ipa->sender;
	for (i = 0; i < SIPA_NIC_MAX; i++) {
		if ((s_sipa_nic_statics[i].src_mask & (1 << src)) &&
		    netid == s_sipa_nic_statics[i].netid) {
			nic_id = i;
			break;
		}
	}

	dev_info(ipa->dev, "open nic_id = %d\n", nic_id);
	if (nic_id == SIPA_NIC_MAX)
		return -EINVAL;

	if (ipa->nic[nic_id]) {
		nic = ipa->nic[nic_id];
		if  (atomic_read(&nic->status) == NIC_OPEN)
			return -EBUSY;
	} else {
		nic = kzalloc(sizeof(*nic), GFP_KERNEL);
		if (!nic)
			return -ENOMEM;
		ipa->nic[nic_id] = nic;
	}

	spin_lock_irqsave(&ipa->mode_lock, flags);
	if (nic_id == SIPA_NIC_USB || nic_id == SIPA_NIC_WIFI) {
		ipa->mode_state |= 1 << nic_id;

		ipa->is_bypass = 0;
		if (ipa->enable_cnt) {
			ipa->glb_ops.set_work_mode(ipa->glb_virt_base,
						   ipa->is_bypass);
		}
	}
	spin_unlock_irqrestore(&ipa->mode_lock, flags);

	/* sipa rm operations */
	sipa_nic_rm_init(&nic->rm_res,
			 ipa->sender,
			 s_sipa_nic_statics[nic_id].cons,
			 SIPA_NIC_RM_INACTIVE_TIMER);
	ret = sipa_nic_register_rm(nic, nic_id);
	if (ret)
		return ret;

	nic->nic_id = nic_id;
	nic->need_notify = 0;
	nic->src_mask = s_sipa_nic_statics[i].src_mask;
	nic->netid = netid;
	nic->cb = cb;
	nic->cb_priv = priv;

	sipa_skb_sender_add_nic(sender, nic);

	atomic_set(&nic->status, NIC_OPEN);

	return nic_id;
}
EXPORT_SYMBOL(sipa_nic_open);

void sipa_nic_set_bypass_mode(bool is_bypass)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	/* sipa_nic_usb_origin:
	 * SIPA_NIC_MAX is not real sipa network card,
	 * when usb half_hard_half_soft, usb0 use this
	 * parameter for mode_state.
	 */
	int sipa_nic_usb_origin = SIPA_NIC_MAX;

	sipa_set_enabled(true);
	if (!is_bypass) {
		ipa->is_bypass = is_bypass;

		/* mode_state:
		 * in order to align network card such as sipa_usb0,
		 * make sure that when half_hard_half_soft use usb0,
		 * it is not configured to bypass mode when nic
		 * close network card such as sipa_eth0.
		 */
		ipa->mode_state |= 1 << sipa_nic_usb_origin;
		ipa->glb_ops.set_work_mode(ipa->glb_virt_base,
					   is_bypass);
	} else {
		ipa->is_bypass = is_bypass;
		ipa->mode_state &= ~(1 << sipa_nic_usb_origin);
		ipa->glb_ops.set_work_mode(ipa->glb_virt_base,
					   is_bypass);
	}
	sipa_set_enabled(false);
}
EXPORT_SYMBOL(sipa_nic_set_bypass_mode);

/**
 * sipa_nic_close() - close the specified nic device.
 * @nic_id: the id of the nic device to be closed.
 *
 * After this interface is called, related resources required by this nic device
 * will be released.
 */
void sipa_nic_close(enum sipa_nic_id nic_id)
{
	unsigned long flags;
	struct sipa_nic *nic = NULL;
	struct sipa_skb_sender *sender;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa) {
		pr_err("sipa driver may not register\n");
		return;
	}

	if (nic_id == SIPA_NIC_MAX || !ipa->nic[nic_id])
		return;

	nic = ipa->nic[nic_id];

	spin_lock_irqsave(&ipa->mode_lock, flags);
	if ((1 << nic_id) & ipa->mode_state)
		ipa->mode_state &= ~(1 << nic_id);

	if (!ipa->mode_state) {
		ipa->is_bypass = 1;
		if (ipa->enable_cnt)
			ipa->glb_ops.set_work_mode(ipa->glb_virt_base,
						   ipa->is_bypass);
	}
	spin_unlock_irqrestore(&ipa->mode_lock, flags);

	atomic_set(&nic->status, NIC_CLOSE);
	sipa_nic_deregister_rm(nic, nic_id);

	sender = ipa->sender;
	sipa_skb_sender_remove_nic(sender, nic);
}
EXPORT_SYMBOL(sipa_nic_close);

/**
 * sipa_nic_tx() - send network data.
 * @nic_id: nic device id used to send data.
 * @dst: target id of network data.
 * @netid: the network data corresponding to the netid of the network card
 *         device.
 * @skb: skb to store network data.
 *
 * After this interface is called, if the return value is 0, it means that the
 * data is sent successfully, if it return -EINPROGRESS or -EAGAIN, it means
 * that the network card needs to enter the flow control state.
 */
int sipa_nic_tx(enum sipa_nic_id nic_id, enum sipa_term_type dst,
		int netid, struct sk_buff *skb)
{
	int ret;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}

	ret = sipa_nic_rm_res_request(ipa->nic[nic_id]);
	if (ret) {
		sipa_nic_rm_res_release(ipa->nic[nic_id]);
		return ret;
	}

	ret = sipa_skb_sender_send_data(ipa->sender, skb, dst, netid);
	if (ret == -EAGAIN)
		ipa->nic[nic_id]->flow_ctrl_status = true;

	sipa_nic_rm_res_release(ipa->nic[nic_id]);

	return ret;
}
EXPORT_SYMBOL(sipa_nic_tx);

/**
 * sipa_nic_rx() - recv network data.
 * @src: src id of network data.
 * @netid: the network data corresponding to the netid of the network card
 *         device.
 * @out_skb: skb to store network data.
 *
 * This interface obtains data directly from ipa common
 * fifo. if it returns 0, it means that the data is read normally, and it
 * returns NULL, which means the fifo is empty.
 */
int sipa_nic_rx(struct sk_buff **out_skb,
		struct sipa_node_info *node_info,
		u32 index, int fifoid)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}

	*out_skb = sipa_recv_skb(ipa->receiver, node_info, index, fifoid);

	if (*out_skb) {
		ipa->fifo_rate[fifoid]++;
		ipa->fifo_rate2[fifoid]++;
		return 0;
	} else {
		return -ENODATA;
	}
}
EXPORT_SYMBOL(sipa_nic_rx);

/**
 * sipa_nic_check_flow_ctrl() - check whether the nic device is in flow control
 *                              state.
 * @nic_id: nic device id to be checked.
 *
 * Return true to indicate the current flow control state, return false to
 * indicate the normal state.
 */
bool sipa_nic_check_flow_ctrl(enum sipa_nic_id nic_id)
{
	bool flow_ctrl;
	unsigned long flags;
	struct sipa_nic *nic;
	struct sipa_nic_cons_res *res;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa) {
		pr_err("sipa driver may not register\n");
		return false;
	}

	if (!ipa->nic[nic_id] ||
	    atomic_read(&ipa->nic[nic_id]->status) == NIC_CLOSE)
		return false;

	nic = ipa->nic[nic_id];
	res = &nic->rm_res;
	spin_lock_irqsave(&res->lock, flags);
	flow_ctrl = nic->rm_res.rm_flow_ctrl;
	spin_unlock_irqrestore(&res->lock, flags);

	return flow_ctrl;
}
EXPORT_SYMBOL(sipa_nic_check_flow_ctrl);

/**
 * sipa_nic_trigger_flow_ctrl_work() - trigger the workqueue to solve the flow
 *                                     control problem.
 * @nic_id: reserved.
 * @err: flow control type.
 *
 * The current flow control workqueue can only handle the exception of -EAGAIN.
 */
int sipa_nic_trigger_flow_ctrl_work(enum sipa_nic_id nic_id, int err)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}

	switch (err) {
	case -EAGAIN:
		ipa->sender->free_notify_net = true;
		schedule_work(&ipa->flow_ctrl_work);
		break;
	default:
		dev_warn(ipa->dev, "don't have this flow ctrl err type\n");
		break;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_nic_trigger_flow_ctrl_work);

/**
 * sipa_nic_check_suspend_condition() - check the working status of all
 *                                      nic devices.
 *
 * If the return value is 0, it means that all nic devices are in suspend state,
 * otherwise it means that some nic devices are in resume state.
 */
int sipa_nic_check_suspend_condition(void)
{
	int i;
	unsigned long flags;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	for (i = 0; i < SIPA_NIC_MAX; i++) {
		if (!ipa->nic[i])
			continue;

		if (atomic_read(&ipa->nic[i]->status) != NIC_OPEN)
			continue;

		spin_lock_irqsave(&ipa->nic[i]->rm_res.lock, flags);
		if (!ipa->nic[i]->rm_res.need_request) {
			spin_unlock_irqrestore(&ipa->nic[i]->rm_res.lock,
					       flags);
			dev_info(ipa->dev,
				 "nic can't sleep netid = %d, src_mask = 0x%x",
				 ipa->nic[i]->netid, ipa->nic[i]->src_mask);
			return -EAGAIN;
		}
		spin_unlock_irqrestore(&ipa->nic[i]->rm_res.lock, flags);
	}

	return 0;
}
EXPORT_SYMBOL(sipa_nic_check_suspend_condition);

/**
 * Determine whether the current common fifo still has unread data.
 */
bool sipa_nic_check_recv_queue_empty(int fifoid)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	enum sipa_cmn_fifo_index fifo_id;

	if (ipa->enable_cnt == 0) {
		dev_err(ipa->dev, "sipa check recv queue enable cnt is 0\n");
		return true;
	}

	fifo_id = ipa->receiver->ep->recv_fifo.idx;
	return sipa_hal_get_tx_fifo_empty_status(ipa->dev,
						 fifo_id + fifoid);
}
EXPORT_SYMBOL(sipa_nic_check_recv_queue_empty);

/**
 * Revert to the common fifo interrupt in the AP direction.
 */
void sipa_nic_restore_cmn_fifo_irq(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	ipa->fifo_ops.restore_irq_map_out(ipa->cmn_fifo_cfg);
}
EXPORT_SYMBOL(sipa_nic_restore_cmn_fifo_irq);

u32 sipa_nic_sync_recv_pkts(u32 budget, int fifoid)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	return ipa->fifo_ops.sync_node_from_tx_fifo(ipa->dev,
						    SIPA_FIFO_MAP0_OUT +
						    fifoid,
						    ipa->cmn_fifo_cfg,
						    budget);
}
EXPORT_SYMBOL(sipa_nic_sync_recv_pkts);

void sipa_nic_switch_core(int fifoid)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa->enable_sw_core || ipa->cpu_num >= 4 /*core4*/ || ipa->sw_mc_set_rps_doing)
		return;

	ipa->fifo_ops.get_filled_depth(SIPA_FIFO_MAP0_OUT + fifoid, ipa->cmn_fifo_cfg,
					      &(ipa->rx_filled), &(ipa->tx_filled));

	if (ipa->tx_filled > SIPA_SW_TO_MIDDLECORE_THRD) {
		ipa->sw_mc_set_rps_doing = true;
		hrtimer_cancel(&ipa->sw_little_core_timer);
		hrtimer_start(&ipa->sw_little_core_timer, ms_to_ktime(0), HRTIMER_MODE_REL);
	}
}
EXPORT_SYMBOL(sipa_nic_switch_core);

int sipa_nic_add_tx_fifo_rptr(u32 num, int fifoid)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	return ipa->fifo_ops.add_tx_fifo_rptr(SIPA_FIFO_MAP0_OUT +
					      fifoid,
					      ipa->cmn_fifo_cfg, num);
}
EXPORT_SYMBOL(sipa_nic_add_tx_fifo_rptr);

void sipa_nic_update_need_fill_cnt(u32 num, int fifoid)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	atomic_add(num, &ipa->receiver->fill_array[fifoid]->need_fill_cnt);
}
EXPORT_SYMBOL(sipa_nic_update_need_fill_cnt);

int sipa_nic_update_res(enum sipa_rule_tag type, bool has)
{
	int ret;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	switch (type) {
	case SIPA_USB_RULE:
		if (has) {
			ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_USB,
						     SIPA_RM_RES_PROD_CP);
			if (ret)
				dev_err(ipa->dev,
					"cons usb add prod cp ret = %d\n", ret);
		} else {
			ret = sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
							SIPA_RM_RES_PROD_CP);
			if (ret)
				dev_err(ipa->dev,
					"cons usb delete prod cp ret = %d\n",
					ret);
		}
		break;
	case SIPA_WIFI_RULE:
		if (has) {
			ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WIFI_UL,
						     SIPA_RM_RES_PROD_CP);
			if (ret)
				dev_err(ipa->dev,
					"cons wifi ul add prod cp ret = %d\n",
					ret);
			ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WIFI_DL,
						     SIPA_RM_RES_PROD_CP);
			if (ret)
				dev_err(ipa->dev,
					"cons wifi dl add prod cp ret = %d\n",
					ret);
		} else {
			ret = sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WIFI_UL,
							SIPA_RM_RES_PROD_CP);
			if (ret)
				dev_err(ipa->dev,
					"cons wifi ul del prod cp ret = %d\n",
					ret);
			ret = sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WIFI_DL,
							SIPA_RM_RES_PROD_CP);
			if (ret)
				dev_err(ipa->dev,
					"cons wifi dl del prod cp ret = %d\n",
					ret);
		}
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_nic_update_res);
