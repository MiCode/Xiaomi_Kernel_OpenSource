// SPDX-License-Identifier: GPL-2.0
/* Copyright 2019-2023 Unisoc(Shanghai) Technologies Co.Ltd
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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mdm_ctrl.h>
#include <linux/pcie-epf-sprd.h>
#include <linux/sched.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wait.h>

#ifdef CONFIG_SPRD_SIPA
#include "pcie_sipa_res.h"
#endif

#include "../include/sprd_actions_queue.h"
#include "../include/sprd_pcie_resource.h"

#define MAX_PMS_WAIT_TIME	(5000*12) /* need wait epf bind ready */
#define PCIE_REMOVE_SCAN_GAP	msecs_to_jiffies(100)

enum ep_msg {
	RC_SCANNED_MSG = 0,
	RC_REMOVING_MSG,
	EPC_UNLINK_MSG,
	EPC_LINKUP_MSG
};

enum pcie_ep_state {
	SPRD_PCIE_WAIT_FIRST_READY = 0,
	SPRD_PCIE_WAIT_SCANNED,
	SPRD_PCIE_SCANNED,
	SPRD_PCIE_WAIT_REMOVED,
	SPRD_PCIE_REMOVED
};

enum ep_actions {
	REQUEST_RES_ACTION = 0,
	RELEASE_RES_ACTION,
	EPF_BIND_ACTION,
	EPF_UNBIND_ACTION,
	EPF_PROBE_ACTION,
	EPF_REMOVE_ACTION,
	EPF_LINKUP_ACTION,
	EPF_UNLINK_ACTION,
	RC_SCANNED_ACTION,
	RC_REMOVING_ACTION
};

struct sprd_pci_res_notify {
	void	(*notify)(void *p);
	void	*data;
};

struct sprd_pcie_res {
	u32	dst;
	u32	ep_fun;
	u32	linkup_cnt;
	unsigned long	action_jiff;
	enum pcie_ep_state	state;
	bool	msi_later;
	bool	release_later;
	bool	do_first_ready;
	bool	wakeup_later;
	bool	power_off;
	bool	requested;

#ifdef CONFIG_SPRD_SIPA
	void *sipa_res;
#endif
	void *actions_queue;

	/*
	 * in client(Orca), The PCIE module wll blocks the chip Deep,
	 * so we must get a wake lock when pcie work to avoid this situation:
	 * the system is deep, but the PCIE is still working.
	 */
	struct wakeup_source	ws;
	wait_queue_head_t		wait_pcie_ready;
	struct sprd_pci_res_notify	first_ready_notify;
};

static struct sprd_pcie_res *g_pcie_res[SIPC_ID_NR];

/* the state machine of ep, init SPRD_PCIE_WAIT_FIRST_READY.
 * SPRD_PCIE_WAIT_FIRST_READY (receive RC scanned) ==> SPRD_PCIE_SCANNED
 * SPRD_PCIE_SCANNED (receive RC removing)==> SPRD_PCIE_WAIT_REMOVED
 * SPRD_PCIE_WAIT_REMOVED(receive epc unlink)==>SPRD_PCIE_REMOVED
 * SPRD_PCIE_WAIT_REMOVED(receive rc scanned)==>SPRD_PCIE_SCANNED
 * SPRD_PCIE_REMOVED(receive epc linkup)==>SPRD_PCIE_WAIT_SCANNED
 * SPRD_PCIE_WAIT_SCANNED(receive RC scanned)==>SPRD_PCIE_SCANNED
 * SPRD_PCIE_WAIT_POWER_OFF can do nothing, just wait shutdown.
 */
static const char *change_msg[EPC_LINKUP_MSG + 1] = {
	"rc scanned",
	"rc removing",
	"epc unlink",
	"epc linkup"
};

static const char *state_msg[SPRD_PCIE_REMOVED + 1] = {
	"wait first ready",
	"wait sacanned",
	"scanned",
	"wait remove",
	"removed"
};

static int pcie_resource_client_change_state(struct sprd_pcie_res *res,
					      enum ep_msg msg)
{
	int err = 0;
	u32 old_state = res->state;

	pr_debug("pcie res: change state msg=%s, old_state=%s.\n",
		 change_msg[msg], state_msg[old_state]);

	switch (msg) {
	case RC_SCANNED_MSG:
		/* SPRD_PCIE_WAIT_REMOVED, means rc cancel remove. */
		if (old_state != SPRD_PCIE_WAIT_FIRST_READY &&
		    old_state != SPRD_PCIE_WAIT_SCANNED &&
		    old_state != SPRD_PCIE_WAIT_REMOVED) {
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);
			err = -EINVAL;
		}

		res->state = SPRD_PCIE_SCANNED;
		break;

	case RC_REMOVING_MSG:
		if (old_state != SPRD_PCIE_SCANNED &&
		    old_state != SPRD_PCIE_WAIT_FIRST_READY) {
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);
			err = -EINVAL;
		}

		res->state = SPRD_PCIE_WAIT_REMOVED;
		break;

	case EPC_UNLINK_MSG:
		if (old_state != SPRD_PCIE_WAIT_REMOVED &&
		    old_state != SPRD_PCIE_WAIT_FIRST_READY) {
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);
			err = -EINVAL;
		}

		res->state = SPRD_PCIE_REMOVED;
		break;

	case EPC_LINKUP_MSG:
		if (old_state != SPRD_PCIE_REMOVED &&
		    old_state != SPRD_PCIE_WAIT_FIRST_READY) {
			pr_err("pcie res: %s msg err, old state=%s",
				       change_msg[msg], state_msg[old_state]);
			err = -EINVAL;
		}

		res->state = SPRD_PCIE_WAIT_SCANNED;
		break;

	default:
		err = -EINVAL;
		break;
	}

	pr_info("pcie res: change state from %s to %s.\n",
		 state_msg[old_state], state_msg[res->state]);

	return err;
}

static void sprd_pcie_resource_first_ready_notify(struct sprd_pcie_res *res)
{
	void (*notify)(void *p);

	pr_info("pcie res: first ready.\n");

#ifdef CONFIG_SPRD_SIPA
	/*
	 * in client side, producer res id is SIPA_RM_RES_PROD_PCIE_EP,
	 * consumer res id is SIPA_RM_RES_CONS_WWAN_DL.
	 */
	res->sipa_res = pcie_sipa_res_create(res->dst,
					     SIPA_RM_RES_PROD_PCIE_EP,
					     SIPA_RM_RES_CONS_WWAN_DL);
	if (!res->sipa_res)
		pr_err("pcie res:create ipa res failed.\n");
#endif

	notify = res->first_ready_notify.notify;
	if (notify)
		notify(res->first_ready_notify.data);
}

static void pcie_resource_client_epf_notify(int event, void *private)
{
	struct sprd_pcie_res *res = (struct sprd_pcie_res *)private;

	switch (event) {
	case SPRD_EPF_BIND:
		sprd_add_action(res->actions_queue, EPF_BIND_ACTION);
		break;

	case SPRD_EPF_UNBIND:
		sprd_add_action(res->actions_queue, EPF_UNBIND_ACTION);
		break;

	case SPRD_EPF_PROBE:
		sprd_add_action(res->actions_queue, EPF_PROBE_ACTION);
		break;

	case SPRD_EPF_REMOVE:
		sprd_add_action(res->actions_queue, EPF_REMOVE_ACTION);
		break;

	case SPRD_EPF_LINK_UP:
		sprd_add_action(res->actions_queue, EPF_LINKUP_ACTION);
		break;

	case SPRD_EPF_UNLINK:
		sprd_add_action(res->actions_queue, EPF_UNLINK_ACTION);
		break;

	default:
		break;
	}
}

static irqreturn_t pcie_resource_client_irq_handler(int irq, void *private)
{
	struct sprd_pcie_res *res = (struct sprd_pcie_res *)private;

	switch (irq) {
	case PCIE_DBEL_EP_SCANNED:
		sprd_add_action(res->actions_queue, RC_SCANNED_ACTION);
		break;

	case PCIE_DBEL_EP_REMOVING:
		sprd_add_action(res->actions_queue, RC_REMOVING_ACTION);
		break;

	default:
		break;
	}

	return IRQ_HANDLED;
}

static int sprd_pcie_resource_client_mcd(struct notifier_block *nb,
				      unsigned long mode, void *cmd)
{
	struct sprd_pcie_res *res;
	int i;

	if (mode != MDM_POWER_OFF)
		return NOTIFY_DONE;

	for (i = 0; i < SIPC_ID_NR; i++) {
		res = g_pcie_res[i];

		if (res)
			res->power_off = true;
	}

	pr_info("pcie res: MDM_POWER_OFF\n");

	return NOTIFY_DONE;
}

static struct notifier_block mcd_notify = {
	.notifier_call = sprd_pcie_resource_client_mcd,
	.priority = 49,
};

int sprd_pcie_wait_resource(u32 dst, int timeout)
{
	struct sprd_pcie_res *res;
	int ret, wait;
	unsigned long delay;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];

	if (res->power_off)
		return -EBUSY;

	/*  pcie ready, return succ immediately */
	if (res->state == SPRD_PCIE_SCANNED)
		return 0;

	if (timeout == 0)
		return -ETIME;

	/*
	 * In some case, roc1 may has an exception, And the pcie
	 * resource may never ready again. So we must set a
	 * maximum wait time for let user to know thereis an
	 * exception in pcie, and can return an error code to the user.
	 */
	if (timeout < 0 || timeout > MAX_PMS_WAIT_TIME)
		timeout = MAX_PMS_WAIT_TIME;

	/*
	 * timeout must add 1s,
	 * because the pcie rescan may took some time.
	 */
	delay = msecs_to_jiffies(timeout + 1000);
	wait = wait_event_interruptible_timeout(res->wait_pcie_ready,
						res->state ==
						SPRD_PCIE_SCANNED,
						delay);
	if (wait == 0) {
		ret = -ETIME;
		/*
		 * only exceed MAX_PMS_WAIT_TIME and
		 * scanned cnt > 0 can panic system.
		 */
		if (timeout == MAX_PMS_WAIT_TIME &&
		    res->linkup_cnt > 0)
			panic("pcie res: wait time out!");
	} else if (wait > 0)
		ret = 0;
	else
		ret = wait;

	if (ret < 0 && ret != -ERESTARTSYS)
		pr_err("pcie res: wait resource, val=%d.\n", ret);

	return ret;
}

int sprd_pcie_request_resource(u32 dst)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];
	res->requested = true;
	 /* make sure the member requested has stored in the
	  * memory before do below actions.
	  */
	smp_mb();

	return sprd_add_action(res->actions_queue, REQUEST_RES_ACTION);
}

int sprd_pcie_release_resource(u32 dst)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];
	res->requested = false;
	 /* make sure the member requested has stored in the
	  * memory before do below actions.
	  */
	smp_mb();

	return sprd_add_action(res->actions_queue, RELEASE_RES_ACTION);
}

int sprd_register_pcie_resource_first_ready(u32 dst,
					    void (*notify)(void *p), void *data)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];

	res->first_ready_notify.data = data;
	res->first_ready_notify.notify = notify;

	return 0;
}

static void sprd_pcie_resource_wakeup_rc(struct sprd_pcie_res *res)
{
	unsigned long diff;
	unsigned int delay;

	diff = jiffies - res->action_jiff;
	if (diff < PCIE_REMOVE_SCAN_GAP) {
		/* must ensure that wakeup rc after a period of remove. */
		delay = jiffies_to_msecs(PCIE_REMOVE_SCAN_GAP - diff);
		msleep(delay);
	}

	pr_info("pcie res: send wakeup to rc.\n");
	sprd_pci_epf_start(res->ep_fun);
}

static void sprd_pcie_resource_do_request_res(struct sprd_pcie_res *res)
{
	pr_info("pcie res: request res, state=%d.\n", res->state);

	switch (res->state) {
	case SPRD_PCIE_WAIT_FIRST_READY:
	case SPRD_PCIE_WAIT_SCANNED:
		res->msi_later = true;
		break;

	case SPRD_PCIE_WAIT_REMOVED:
		res->wakeup_later = true;
		break;

	case SPRD_PCIE_SCANNED:
		/*
		 * if pcie state is SCANNED, just send
		 * PCIE_MSI_REQUEST_RES to the host.
		 * After host receive res msi interrupt,
		 * it will increase one vote in modem power manager.
		 */
		pr_info("pcie res: send request msi to rc.\n");
		sprd_pci_epf_raise_irq(res->ep_fun, PCIE_MSI_REQUEST_RES);
		break;

	case SPRD_PCIE_REMOVED:
		/*
		 * if pcie state is removed, poll wake_up singnal
		 * to host, and he host will rescan the pcie.
		 */
		sprd_pcie_resource_wakeup_rc(res);
		res->msi_later = true;
		break;

	default:
		pr_err("pcie res: request res err, state=%d.\n",
		       res->state);
		break;
	}
}

static void sprd_pcie_resource_do_release_res(struct sprd_pcie_res *res)
{
	pr_info("pcie res: release resource.\n");

	switch (res->state) {
	case SPRD_PCIE_SCANNED:
		/*
		 * if pcie state is SCANNED, send PCIE_MSI_RELEASE_RES
		 * to the host, else, do nothing. After host receive res msi
		 * interrupt, it will decrease one vote in modem power manager,
		 * and if modem power manager is idle, the host will remove
		 * the pcie.
		 */
		pr_info("pcie res: send release msi to rc.\n");
		sprd_pci_epf_raise_irq(res->ep_fun, PCIE_MSI_RELEASE_RES);
		break;

	case SPRD_PCIE_WAIT_FIRST_READY:
		res->release_later = true;
		break;

	default:
		res->release_later = true;
		pr_err("pcie res: release res state=%d.\n", res->state);
		break;
	}
}

static void sprd_pcie_resource_do_epf_bind(struct sprd_pcie_res *res)
{
	pr_info("pcie res: epf be binded.\n");

	sprd_pci_epf_raise_irq(res->ep_fun, PCIE_MSI_EP_READY_FOR_RESCAN);
}

static void sprd_pcie_resource_do_efp_unbind(struct sprd_pcie_res *res)
{
	pr_info("pcie res: epf be unbinded.\n");
}

static void sprd_pcie_resource_do_epf_probe(struct sprd_pcie_res *res)
{
	pr_info("pcie res: epf probe.\n");
}

static void sprd_pcie_resource_do_epf_remove(struct sprd_pcie_res *res)
{
	pr_info("pcie res: epf remove.\n");
}

static void sprd_pcie_resource_do_epf_linkup(struct sprd_pcie_res *res)
{
	res->linkup_cnt++;

	pr_info("pcie res: epf linkup, cnt=%d.\n", res->linkup_cnt);

	/* get a wakelock */
	__pm_stay_awake(&res->ws);

	if (pcie_resource_client_change_state(res, EPC_LINKUP_MSG))
		pr_warn("pcie res: change state err.\n");

	/* first ready notify */
	if (!res->do_first_ready) {
		res->do_first_ready = true;
		sprd_pcie_resource_first_ready_notify(res);
	}
}

static void sprd_pcie_resource_do_epf_unlink(struct sprd_pcie_res *res)
{
	pr_info("pcie res: epf unlink.\n");

	/* record the last remove jiffies */
	res->action_jiff = jiffies;

	if (pcie_resource_client_change_state(res, EPC_UNLINK_MSG))
		pr_warn("pcie res: change state err.\n");

	/*	if has wakeup pending, send wakeup to rc */
	if (res->wakeup_later) {
		res->wakeup_later = false;
		pr_info("pcie res: send wakeup to rc.\n");

		res->msi_later = true;
		pr_info("pcie res: later send request msi to rc.\n");
		sprd_pcie_resource_wakeup_rc(res);
	}

	/* relax a wakelock */
	__pm_relax(&res->ws);
}

static void sprd_pcie_resource_do_rc_scanned(struct sprd_pcie_res *res)
{
	pr_info("pcie res: scanned.\n");

	/* SCANNED also can recv RC_SCANNED_MSG, but don't change state. */
	if (res->state != SPRD_PCIE_SCANNED) {
		if (pcie_resource_client_change_state(res, RC_SCANNED_MSG))
			pr_warn("pcie res: change state err.\n");
	}

	/* send respond to rc. */
	sprd_pci_epf_raise_irq(res->ep_fun, PCIE_MSI_SCANNED_RESPOND);

	/* wakeup all blocked thread */
	wake_up_interruptible_all(&res->wait_pcie_ready);
	/*	if has msi or wakeup pending, send request msi to rc */
	if (res->msi_later || res->wakeup_later) {
		res->msi_later = false;
		res->wakeup_later = false;
		pr_info("pcie res: send request msi to rc.\n");
		sprd_pci_epf_raise_irq(res->ep_fun,
					 PCIE_MSI_REQUEST_RES);
	}

	if (res->release_later) {
		res->release_later = false;
		pr_info("pcie res: send release msi to rc.\n");
		sprd_pci_epf_raise_irq(res->ep_fun, PCIE_MSI_RELEASE_RES);
	}
}

static void sprd_pcie_resource_do_rc_removing(struct sprd_pcie_res *res)
{
	pr_info("pcie res: removing.\n");

	/* be requested, do not send PCIE_MSI_REMOVE_RESPOND to rc. */
	if (res->requested)
		return;

	if (pcie_resource_client_change_state(res, RC_REMOVING_MSG))
		pr_warn("pcie res: change state err.\n");

	/* give a respound msg after change remove state */
	sprd_pci_epf_raise_irq(res->ep_fun, PCIE_MSI_REMOVE_RESPOND);
}

static void sprd_pcie_resource_do_actions(u32 action, int param, void *data)
{
	struct sprd_pcie_res *res = (struct sprd_pcie_res *)data;

	if (res->power_off)
		return;

	switch (action) {
	case REQUEST_RES_ACTION:
		sprd_pcie_resource_do_request_res(res);
		break;

	case RELEASE_RES_ACTION:
		sprd_pcie_resource_do_release_res(res);
		break;

	case EPF_BIND_ACTION:
		sprd_pcie_resource_do_epf_bind(res);
		break;

	case EPF_UNBIND_ACTION:
		sprd_pcie_resource_do_efp_unbind(res);
		break;

	case EPF_PROBE_ACTION:
		sprd_pcie_resource_do_epf_probe(res);
		break;

	case EPF_REMOVE_ACTION:
		sprd_pcie_resource_do_epf_remove(res);
		break;

	case EPF_LINKUP_ACTION:
		sprd_pcie_resource_do_epf_linkup(res);
		break;

	case EPF_UNLINK_ACTION:
		sprd_pcie_resource_do_epf_unlink(res);
		break;

	case RC_SCANNED_ACTION:
		sprd_pcie_resource_do_rc_scanned(res);
		break;

	case RC_REMOVING_ACTION:
		sprd_pcie_resource_do_rc_removing(res);
		break;

	default:
		break;
	}
}

bool sprd_pcie_is_defective_chip(void)
{
	static bool first_read = true, defective;

	if (first_read) {
		first_read = false;
		defective = sprd_kproperty_chipid("UD710-AB") == 0;
	}

	return defective;
}

int sprd_pcie_resource_client_init(u32 dst, u32 ep_fun)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR)
		return -EINVAL;

	res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	res->dst = dst;
	res->state = SPRD_PCIE_WAIT_FIRST_READY;
	res->ep_fun = ep_fun;

	wakeup_source_init(&res->ws, "pcie_res");
	init_waitqueue_head(&res->wait_pcie_ready);

	res->actions_queue =
		sprd_create_action_queue("pcie_res",
					 sprd_pcie_resource_do_actions, res, 9);
	if (!res->actions_queue) {
		pr_err("pcie res:create actions failed.\n");
		kfree(res);
		return -ENOMEM;
	}

	sprd_pci_epf_register_irq_handler_ex(res->ep_fun,
					     PCIE_DBEL_EP_SCANNED,
					     PCIE_DBEL_EP_REMOVING,
					     pcie_resource_client_irq_handler,
					     res);
	sprd_pci_epf_register_notify(res->ep_fun,
				     pcie_resource_client_epf_notify,
				     res);

	modem_ctrl_register_notifier(&mcd_notify);

	g_pcie_res[dst] = res;

	return 0;
}

int sprd_pcie_resource_trash(u32 dst)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];

#ifdef CONFIG_SPRD_SIPA
	if (res->sipa_res)
		pcie_sipa_res_destroy(res->sipa_res);
#endif

	sprd_pci_epf_unregister_irq_handler_ex(res->ep_fun,
		PCIE_DBEL_EP_SCANNED,
		PCIE_DBEL_EP_REMOVING);
	sprd_pci_epf_unregister_notify(res->ep_fun);
	modem_ctrl_unregister_notifier(&mcd_notify);
	sprd_destroy_action_queue(res->actions_queue);

	kfree(res);
	g_pcie_res[dst] = NULL;

	return 0;
}

