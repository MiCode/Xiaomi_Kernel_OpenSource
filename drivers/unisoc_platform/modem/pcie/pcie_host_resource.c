// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019-2023 Unisoc(Shanghai) Technologies Co.Ltd
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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mdm_ctrl.h>
#include <linux/pci.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/sched.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_mpm.h>
#include <linux/soc/sprd/sprd_pcie_ep_device.h>
#include <linux/types.h>
#include <linux/wait.h>

#ifdef CONFIG_SPRD_SIPA
#include "pcie_sipa_res.h"
#endif

#include "../include/sprd_actions_queue.h"
#include "../include/sprd_pcie_resource.h"

#define PCIE_REMOVE_SCAN_GAP	msecs_to_jiffies(200)
#define MAX_PMS_WAIT_TIME	5000
#define MAX_PMS_DEFECTIVE_CHIP_FIRST_WAIT_TIME	(55 * 1000)

enum rc_state {
	SPRD_PCIE_WAIT_FIRST_READY = 0,
	SPRD_PCIE_WAIT_SCANNED,
	SPRD_PCIE_SCANNED,
	SPRD_PCIE_WAIT_REMOVED,
	SPRD_PCIE_START_REMOVED,
	SPRD_PCIE_REMOVED,
	SPRD_PCIE_SCANNED_2BAR
};

enum rc_msg {
	SCANNED_MSG = 0,
	BAR_SCANNED_MSG,
	REMOVED_MSG,
	WAIT_REMOVE_MSG,
	START_REMOVE_MSG,
	START_SCAN_MSG,
};

enum rc_actions {
	REQUEST_RES_ACTION = 0,
	RELEASE_RES_ACTION,
	SCAN_PCIE_ACTION,
	REMOVE_PCIE_ACTION,
	EP_PROBE_ACTION,
	BAR_PROBE_ACTION,
	EP_REMOVE_ACTION,
	EP_SCANNED_RESPOND_ACTION,
	EP_READY_SCAN_ACTION,
	EP_LOAD_DONE_ACTION,
	EP_WAKEUP_ACTION
};

struct sprd_pcie_res {
	u32	dst;
	u32	ep_dev;
	u32	state;
	u32	scan_cnt;
	u32	max_wait_time;
	u32	ep_reboot_cnt;
	u32	ep_up_cnt;

	bool	requested;
	bool	wait_scanned;
	bool	wait_scanned_respond;
	bool	ep_dev_probe;
	bool	power_off;
	bool	ep_ready_for_rescan;
	bool	ep_load_done;
	bool	reboot_ep_wait_removed;
	bool	pcie_configured;
	bool	ep_remove_respond;
	bool	wait_removed;
	bool	smem_send_to_ep;

	unsigned long	action_jiff;

	struct sprd_pms	*pms;
	char		pms_name[20];

	wait_queue_head_t	wait_reboot_ep;
	wait_queue_head_t	wait_pcie_ready;
	wait_queue_head_t	wait_load_ready;
	wait_queue_head_t	wait_ep_respond;
	wait_queue_head_t	wait_remove_ep;

#ifdef CONFIG_SPRD_SIPA
	void *sipa_res;
#endif
	void *actions_queue;

	struct wakeup_source	ws;
	struct platform_device		*pcie_dev;
	struct sprd_pcie_register_event	reg_event;
};

/* the state machine of host, init SPRD_PCIE_WAIT_FIRST_READY.
 * SPRD_PCIE_WAIT_FIRST_READY (receive ep dev scanned) ==> SPRD_PCIE_SCANNED
 * SPRD_PCIE_WAIT_FIRST_READY (receive ep dev scanned 2br)
 * ==> SPRD_PCIE_SCANNED_2BAR
 * SPRD_PCIE_SCANNED_2BAR(star remove)==>SPRD_PCIE_WAIT_REMOVED
 * SPRD_PCIE_SCANNED (start remove)==> SPRD_PCIE_WAIT_REMOVED
 * SPRD_PCIE_WAIT_REMOVED (ep up) ==> SPRD_PCIE_SCANNED
 * SPRD_PCIE_WAIT_REMOVED (remove response)==> SPRD_PCIE_START_REMOVED
 * SPRD_PCIE_START_REMOVED(receive ep dev removed)==>SPRD_PCIE_REMOVED
 * SPRD_PCIE_REMOVED(start scan)==>SPRD_PCIE_WAIT_SCANNED
 * SPRD_PCIE_WAIT_SCANNED(receiveep dev scanned)==>SPRD_PCIE_SCANNED
 * SPRD_PCIE_WAIT_SCANNED (receive ep dev scanned 2bar)
 * ==> SPRD_PCIE_SCANNED_2BAR
 */
static const char *change_msg[START_SCAN_MSG + 1] = {
	"scanned",
	"2bar scanned",
	"removed",
	"wait remove",
	"start remove",
	"start scan"
};

static const char *state_msg[SPRD_PCIE_SCANNED_2BAR + 1] = {
	"wait first ready",
	"wait sacanned",
	"scanned",
	"wait remove",
	"start remove",
	"removed",
	"2bar sacanned"
};

static struct sprd_pcie_res *g_pcie_res[SIPC_ID_NR];

static void pcie_resource_host_change_state(struct sprd_pcie_res *res,
					      enum rc_msg msg)
{
	u32 old_state = res->state;

	pr_debug("pcie res: change state msg=%s, old_state=%s.\n",
		 change_msg[msg], state_msg[old_state]);

	switch (msg) {
	case SCANNED_MSG:
		if (old_state != SPRD_PCIE_WAIT_FIRST_READY
		    && old_state != SPRD_PCIE_WAIT_REMOVED
		    && old_state != SPRD_PCIE_WAIT_SCANNED)
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);

		res->state = SPRD_PCIE_SCANNED;
		break;

	case BAR_SCANNED_MSG:
		if (old_state != SPRD_PCIE_WAIT_FIRST_READY
		    && old_state != SPRD_PCIE_WAIT_SCANNED)
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);

		res->state = SPRD_PCIE_SCANNED_2BAR;
		break;

	case WAIT_REMOVE_MSG:
		if (old_state != SPRD_PCIE_SCANNED
			&& old_state != SPRD_PCIE_SCANNED_2BAR)
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);

		res->state = SPRD_PCIE_WAIT_REMOVED;
		break;

	case START_REMOVE_MSG:
		if (old_state != SPRD_PCIE_WAIT_REMOVED)
			pr_err("pcie res: %s msg err, old state=%s",
			       change_msg[msg], state_msg[old_state]);

		res->state = SPRD_PCIE_START_REMOVED;
		break;

	case REMOVED_MSG:
		if (old_state != SPRD_PCIE_START_REMOVED)
			if (old_state != SPRD_PCIE_WAIT_FIRST_READY)
				pr_err("pcie res: %s msg err, old state=%s",
				       change_msg[msg], state_msg[old_state]);

		res->state = SPRD_PCIE_REMOVED;
		break;

	case START_SCAN_MSG:
		if (old_state != SPRD_PCIE_REMOVED)
			if (old_state != SPRD_PCIE_WAIT_FIRST_READY)
				pr_err("pcie res: %s msg err, old state=%s",
				       change_msg[msg], state_msg[old_state]);

		res->state = SPRD_PCIE_WAIT_SCANNED;
		break;
	}

	pr_info("pcie res: change state from %s to %s.\n",
		 state_msg[old_state], state_msg[res->state]);
}

static void sprd_pcie_resource_host_first_probe_do(struct sprd_pcie_res *res)
{
	int ret = sprd_pcie_register_event(&res->reg_event);

	if (ret)
		pr_err("pcie res: register pci ret=%d.\n", ret);

#ifdef CONFIG_SPRD_SIPA
	/*
	 * in host side, producer res id is SIPA_RM_RES_PROD_PCIE3,
	 * consumer res id is SIPA_RM_RES_CONS_WWAN_UL.
	 */
	res->sipa_res = pcie_sipa_res_create(res->dst,
					     SIPA_RM_RES_PROD_PCIE3,
					     SIPA_RM_RES_CONS_WWAN_UL);
	if (!res->sipa_res)
		pr_err("pcie res:create ipa res failed.\n");
#endif
}

static void sprd_pcie_resource_do_ep_request(struct sprd_pcie_res *res)
{
	pr_info("pcie res: ep request res up cnt=%d.\n", res->ep_up_cnt);

	if (!res->ep_up_cnt++) {
		sprd_pms_power_up(res->pms);

		if (res->state == SPRD_PCIE_WAIT_REMOVED)
			wake_up(&res->wait_ep_respond);
	}
}

static void sprd_pcie_resource_do_ep_release(struct sprd_pcie_res *res)
{
	pr_info("pcie res: ep release res up cnt=%d.\n", res->ep_up_cnt);

	if (!--res->ep_up_cnt)
		sprd_pms_power_down(res->pms, false);
}

static void sprd_pcie_resource_host_ep_notify(int event, void *data)
{
	struct sprd_pcie_res *res = (struct sprd_pcie_res *)data;

	/* wait power off, do nothing */
	if (res->power_off)
		return;

	switch (event) {
	case PCIE_EP_PROBE:
		res->ep_dev_probe = true;
		 /* make sure the member ep_dev_probe has stored in the
		  * memory before do below actions.
		  */
		smp_mb();

		/*  firsrt scan do somtehing */
		if (!res->scan_cnt++)
			sprd_pcie_resource_host_first_probe_do(res);

		modem_ctrl_enable_cp_event();
		sprd_add_action(res->actions_queue, EP_PROBE_ACTION);
		break;

	case PCIE_EP_REMOVE:
		res->ep_dev_probe = false;
		 /* make sure the member ep_dev_probe has stored in the
		  * memory before do below actions.
		  */
		smp_mb();

		sprd_add_action(res->actions_queue, EP_REMOVE_ACTION);
		break;

	case PCIE_EP_PROBE_BEFORE_SPLIT_BAR:
		res->ep_dev_probe = true;
		 /* make sure the member ep_dev_probe has stored in the
		  * memory before do below actions.
		  */
		smp_mb();

		sprd_add_action(res->actions_queue, BAR_PROBE_ACTION);
		break;

	default:
		break;
	}
}

static irqreturn_t sprd_pcie_resource_host_irq_handler(int irq, void *private)
{
	struct sprd_pcie_res *res = (struct sprd_pcie_res *)private;

	if (res->reboot_ep_wait_removed)
		return IRQ_HANDLED;

	switch (irq) {
	case PCIE_MSI_SCANNED_RESPOND:
		sprd_add_action(res->actions_queue, EP_SCANNED_RESPOND_ACTION);
		break;

	case PCIE_MSI_REQUEST_RES:
		sprd_pcie_resource_do_ep_request(res);
		break;

	case PCIE_MSI_RELEASE_RES:
		sprd_pcie_resource_do_ep_release(res);
		break;

	case PCIE_MSI_EP_READY_FOR_RESCAN:
		sprd_add_action(res->actions_queue, EP_READY_SCAN_ACTION);
		break;

	case PCIE_MSI_REMOVE_RESPOND:
		res->ep_remove_respond = true;

		 /* make sure the member ep_remove_respond has stored in the
		  * memory before wake up the wait_ep_respond event.
		  */
		smp_mb();
		wake_up(&res->wait_ep_respond);
		break;

	default:
		break;
	}

	return IRQ_HANDLED;
}

static void sprd_pcie_resource_event_process(enum sprd_pcie_event event,
					    void *data)
{
	struct sprd_pcie_res *res = data;

	if (event == SPRD_PCIE_EVENT_WAKEUP) {
		/*
		 * scan pcie may caused wakeup signal,
		 * but It is not a expect siggnal.
		 */
		if (res->pcie_configured) {
			pr_info("pcie res: unexpect wakeup, state=%d.",
				res->state);
			return;
		}

		/* ep wakeup rc, power up for wake up. */
		sprd_pms_power_up(res->pms);
		sprd_pms_request_wakelock_period(res->pms, 500);
		sprd_add_action(res->actions_queue, EP_WAKEUP_ACTION);
	}
}

static int sprd_pcie_resource_host_mcd(struct notifier_block *nb,
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
	.notifier_call = sprd_pcie_resource_host_mcd,
	.priority = 49,
};

void sprd_pcie_img_load_done(u32 dst)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return;

	res = g_pcie_res[dst];
	sprd_add_action(res->actions_queue, EP_LOAD_DONE_ACTION);
}

int sprd_pcie_wait_remove_resource(u32 dst)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];

	/* can load image, return immediately */
	if (res->state == SPRD_PCIE_REMOVED)
		return 0;

	res->wait_removed = true;

	return wait_event_interruptible_timeout(
		res->wait_remove_ep,
		res->state == SPRD_PCIE_REMOVED,
		msecs_to_jiffies(5000));
}

int sprd_pcie_wait_load_resource(u32 dst)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];

	if (res->power_off)
		return -EBUSY;

	/* can load image, return immediately */
	if (res->state == SPRD_PCIE_SCANNED ||
		res->state == SPRD_PCIE_SCANNED_2BAR)
		return 0;

	return wait_event_interruptible_timeout(
		res->wait_load_ready,
		(res->state == SPRD_PCIE_SCANNED ||
		res->state == SPRD_PCIE_SCANNED_2BAR),
		res->max_wait_time);
}

void sprd_pcie_resource_reboot_ep(u32 dst, bool b_warm_reboot)
{
	struct sprd_pcie_res *res;
	int ret;

	pr_info("pcie res: enter reboot ep.\n");

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return;

	res = g_pcie_res[dst];

	/* wait power off, do nothing */
	if (res->power_off)
		return;

	sprd_pms_request_wakelock(res->pms);

	res->reboot_ep_wait_removed = true;
	 /* make sure the member reboot_ep_wait_removed
	  * has stored in the memory before do below actions.
	  */
	smp_mb();

	/* must removed ep before reboot ep */
	if (b_warm_reboot) {
		/* start removed ep*/
		ret = sprd_pcie_unconfigure_device(res->pcie_dev);
		if (ret)
			pr_err("pcie res: remove error state=%d.\n!", ret);
	} else if (res->state != SPRD_PCIE_REMOVED) {
		/*  must remove. */
		if (res->state == SPRD_PCIE_SCANNED ||
			res->state == SPRD_PCIE_SCANNED_2BAR)
			sprd_add_action(res->actions_queue, REMOVE_PCIE_ACTION);
		ret = wait_event_interruptible_timeout(res->wait_reboot_ep,
						       res->state ==
						       SPRD_PCIE_REMOVED,
						       msecs_to_jiffies(5000));
		if (ret <= 0) {
			pr_info("pcie res: force unconfig pcie.\n");

			/* start removed ep*/
			ret = sprd_pcie_unconfigure_device(res->pcie_dev);
			if (ret)
				pr_err("pcie res: remove error=%d.\n!", ret);
		}
	}

	res->state = SPRD_PCIE_REMOVED;
	res->pcie_configured = false;

	/* wait power off, do nothing */
	if (res->power_off) {
		sprd_pms_release_wakelock(res->pms);
		return;
	}

	res->ep_reboot_cnt++;
	pr_info("pcie res: reboot ep cnt=%d.\n", res->ep_reboot_cnt);

	if (b_warm_reboot)
		modem_ctrl_poweron_modem(MDM_CTRL_WARM_RESET);
	else
		modem_ctrl_poweron_modem(MDM_CTRL_COLD_RESET);

	/* The defective chip , the first wait time must be enough long. */
	if (sprd_pcie_is_defective_chip())
		res->max_wait_time = MAX_PMS_DEFECTIVE_CHIP_FIRST_WAIT_TIME;
	else
		res->max_wait_time = MAX_PMS_WAIT_TIME;

	res->ep_up_cnt = 0;
	res->reboot_ep_wait_removed = false;
	res->ep_ready_for_rescan = false;
	res->ep_load_done = false;
	res->smem_send_to_ep = false;
	 /* make sure the member of res has stored
	  *  in the memory before do below actions.
	  */
	smp_mb();

	sprd_pms_power_down_complete(res->pms);

	/*  scan ep again, delay 1s for orca ready. */
	msleep(1000);
	sprd_add_action(res->actions_queue, SCAN_PCIE_ACTION);

	sprd_pms_release_wakelock(res->pms);
}

int sprd_pcie_wait_resource(u32 dst, int timeout)
{
	struct sprd_pcie_res *res;
	int ret, wait;
	unsigned long delay;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];

	/* pcie ready and resource is requested return succ immediately. */
	if (res->state == SPRD_PCIE_SCANNED && res->requested)
		return 0;

	if (timeout == 0)
		return -ETIME;

	/*
	 * In some case, orca may has an exception, And the pcie
	 * resource may never ready again. So we	must set a
	 * maximum wait time for let user to know thereis an
	 * exception in pcie, and can return an error code to the user.
	 */
	if (timeout < 0 || timeout > res->max_wait_time)
		timeout = res->max_wait_time;

	/*
	 * timeout must add 1s,
	 * because the pcie scan may took some time.
	 */
	delay = msecs_to_jiffies(timeout + 1000);
	wait = wait_event_interruptible_timeout(res->wait_pcie_ready,
						res->state ==
						SPRD_PCIE_SCANNED &&
						res->requested,
						delay);
	if (wait == 0) {
		/* notify ep to crash. */
		/* only exceed max_wait_time notify ep to crash. */
		if (timeout == res->max_wait_time)
			modem_ctrl_poweron_modem(MDM_CTRL_CRASH_MODEM);
		ret = -ETIME;
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

	sipc_set_wakeup_flag();
	res->requested = true;
	 /* make sure the member requested has stored in the
	  * memory before do below actions.
	  */
	smp_mb();

	/* get a wakelock */
	__pm_stay_awake(&res->ws);

	return sprd_add_action(res->actions_queue, REQUEST_RES_ACTION);
}

int sprd_pcie_release_resource(u32 dst)
{
	struct sprd_pcie_res *res;
	int ret;

	if (dst >= SIPC_ID_NR || !g_pcie_res[dst])
		return -EINVAL;

	res = g_pcie_res[dst];
	res->requested = false;
	 /* make sure the member requested has stored in the
	  * memory before do below actions.
	  */
	smp_mb();

	ret = sprd_add_action(res->actions_queue, RELEASE_RES_ACTION);

	/* relax a wakelock */
	__pm_relax(&res->ws);
	return ret;
}

static void sprd_pcie_resource_do_scan_pcie(struct sprd_pcie_res *res)
{
	unsigned long diff;
	unsigned int delay;
	int ret;

	if (res->state == SPRD_PCIE_SCANNED ||
		res->state == SPRD_PCIE_WAIT_SCANNED) {
		pr_info("pcie res: scanned, do nothing!\n");
		return;
	}

	pr_info("pcie res: start scan.\n");

	pcie_resource_host_change_state(res, START_SCAN_MSG);

	/* request wakelock */
	sprd_pms_request_wakelock(res->pms);

	diff = jiffies - res->action_jiff;
	if (diff < PCIE_REMOVE_SCAN_GAP) {
		/* must ensure that the scan starts after a period of remove. */
		delay = jiffies_to_msecs(PCIE_REMOVE_SCAN_GAP - diff);
		msleep(delay);
	}

	if (res->power_off) {
		/* release wakelock */
		sprd_pms_release_wakelock(res->pms);
		return;
	}

	pr_info("pcie res: config pcie\n");

	res->pcie_configured = true;
	ret = sprd_pcie_configure_device(res->pcie_dev);
	if (ret)
		pr_err("pcie res: scan error = %d!\n", ret);

	/* record the last scan jiffies */
	res->action_jiff = jiffies;
}

static void sprd_pcie_resource_do_remove_pcie(struct sprd_pcie_res *res)
{
	unsigned long diff;
	unsigned int delay;
	int ret;
	u32 value;

	if (res->state != SPRD_PCIE_SCANNED &&
		res->state != SPRD_PCIE_SCANNED_2BAR) {
		pr_err("pcie res: remove err state=%d.", res->state);
		return;
	}

	/* has resource requested, don't remove pcie. */
	if (res->ep_up_cnt || res->requested)
		return;

	pcie_resource_host_change_state(res, WAIT_REMOVE_MSG);

	pr_info("pcie res: wait remove!\n");

	diff = jiffies - res->action_jiff;
	if (diff < PCIE_REMOVE_SCAN_GAP) {
		/* must ensure that the remove starts after a period of scan. */
		delay = jiffies_to_msecs(PCIE_REMOVE_SCAN_GAP - diff);
		msleep(delay);
	}

	/*
	 * in wait power off state, or ep device is not probing,
	 * can't access ep.
	 */
	if (res->power_off ||
		!res->ep_dev_probe) {
		/* release wakelock */
		sprd_pms_release_wakelock(res->pms);
		return;
	}

	res->ep_remove_respond = false;

	/* notify ep removed, must before removed */
	if (!res->ep_up_cnt && !res->requested)
		sprd_ep_dev_raise_irq(res->ep_dev, PCIE_DBEL_EP_REMOVING);

	/* waiting for ep remove respond, max time 4s,
	 * remove it when wait ep remove respond or time out.
	 */
	ret = wait_event_timeout(res->wait_ep_respond,
				 res->ep_remove_respond ||
				 res->ep_up_cnt || res->requested,
				 msecs_to_jiffies(4000));
	if (ret < 0) {
		/* release wakelock */
		sprd_pms_release_wakelock(res->pms);
		return;
	}

	/* if ep_up_cnt and reboot_ep_wait_removed is false,
	 * cancel remove pcie, return to scanned state.
	 */
	if ((res->ep_up_cnt || res->requested) &&
	    !res->reboot_ep_wait_removed) {
		pcie_resource_host_change_state(res, SCANNED_MSG);
		/* notify ep scanned and sipc irq. */
		value = BIT(PCIE_DBEL_EP_SCANNED) | BIT(PCIE_DBELL_SIPC_IRQ);
		sprd_ep_dev_raise_irq_ex(res->ep_dev, value);

		/* wakeup all blocked thread */
		wake_up_interruptible_all(&res->wait_pcie_ready);
		/* release wakelock */
		sprd_pms_release_wakelock(res->pms);
		return;
	}

	pcie_resource_host_change_state(res, START_REMOVE_MSG);

	pr_info("pcie res: unconfig pcie\n");

	/* start removed ep*/
	ret = sprd_pcie_unconfigure_device(res->pcie_dev);
	if (ret)
		pr_err("pcie res: remove error state=%d.\n!", ret);

	res->pcie_configured = false;
	/* record the last remov jiffies */
	res->action_jiff = jiffies;

	/* release wakelock */
	sprd_pms_release_wakelock(res->pms);
}

static void sprd_pcie_resource_do_request_res(struct sprd_pcie_res *res)
{
	pr_info("pcie res: request resource, state=%d.\n", res->state);

	if (res->reboot_ep_wait_removed)
		return;

	switch (res->state) {
	case SPRD_PCIE_SCANNED:
	case SPRD_PCIE_WAIT_SCANNED:
		pr_info("pcie res: scanned, do nothing!\n");
		return;

	case SPRD_PCIE_WAIT_REMOVED:
	case SPRD_PCIE_START_REMOVED:
		res->wait_scanned = true;
		return;

	/* The first scan is start by pcie driver automatically. */
	case SPRD_PCIE_WAIT_FIRST_READY:
		return;

	default:
		break;
	}

	sprd_pcie_resource_do_scan_pcie(res);
}

static void sprd_pcie_resource_do_release_res(struct sprd_pcie_res *res)
{
	pr_info("pcie res: release resource.\n");

	sprd_pcie_resource_do_remove_pcie(res);
}

static void sprd_pcie_resource_do_ep_probe(struct sprd_pcie_res *res)
{
	u32 value, base, size;

	pr_info("pcie res: ep_notify, probed cnt=%d.\n",
		res->scan_cnt);

	if (!res->smem_send_to_ep) {
		res->smem_send_to_ep = true;
		if (smem_get_area(SIPC_ID_MINIAP, &base, &size, 0) == 0)
			sprd_ep_dev_pass_smem(res->ep_dev, base, size);
	}

	/* set state to scanned */
	pcie_resource_host_change_state(res, SCANNED_MSG);
	res->wait_scanned = false;
	res->wait_scanned_respond = true;

	/* power up for ep until receive ep resond scan. */
	sprd_pms_power_up(res->pms);

	/* notify ep scanned and sipc irq. */
	value = BIT(PCIE_DBEL_EP_SCANNED) | BIT(PCIE_DBELL_SIPC_IRQ);
	sprd_ep_dev_raise_irq_ex(res->ep_dev, value);

	/* wakeup all blocked thread */
	wake_up_interruptible_all(&res->wait_pcie_ready);
	wake_up_interruptible_all(&res->wait_load_ready);

	/* reboot_ep_wait_removed must remove. */
	if (res->reboot_ep_wait_removed)
		sprd_add_action(res->actions_queue, REMOVE_PCIE_ACTION);
}

static void sprd_pcie_resource_do_2bar_probe(struct sprd_pcie_res *res)
{
	pr_info("pcie res: probed before split bar.\n");

	res->wait_scanned = true;
	pcie_resource_host_change_state(res, BAR_SCANNED_MSG);

	/* ep_ready_for_rescan or reboot_ep_wait_removed must remove. */
	if (res->ep_ready_for_rescan || res->reboot_ep_wait_removed)
		sprd_add_action(res->actions_queue, REMOVE_PCIE_ACTION);

	/* wake up wait load ready. */
	wake_up_interruptible_all(&res->wait_load_ready);
}

static void sprd_pcie_resource_do_ep_remove(struct sprd_pcie_res *res)
{
	pr_info("pcie res: ep_notify, removed.\n");

	pcie_resource_host_change_state(res, REMOVED_MSG);

	/* wake up wait removed. */
	if (res->wait_removed) {
		res->wait_removed = false;
		wake_up_interruptible_all(&res->wait_remove_ep);
		return;
	}

	/* wake up ep reboot wait removed. */
	if (res->reboot_ep_wait_removed) {
		wake_up_interruptible_all(&res->wait_reboot_ep);
		return;
	}

	/* wait_scanned must rescan. */
	if (res->wait_scanned && !res->reboot_ep_wait_removed)
		sprd_add_action(res->actions_queue, SCAN_PCIE_ACTION);
}

static void sprd_pcie_resource_do_ep_scaned_respond(struct sprd_pcie_res *res)
{
	pr_info("pcie res: ep scanned respond.");
	if (res->wait_scanned_respond) {
		res->wait_scanned_respond = false;
		sprd_pms_power_down(res->pms, false);
	}
}

static void sprd_pcie_resource_do_ready_scan(struct sprd_pcie_res *res)
{
	pr_info("pcie res: ep ready for rescan.\n");

	res->ep_ready_for_rescan = true;

	if (res->wait_scanned && res->ep_load_done)
		sprd_add_action(res->actions_queue, REMOVE_PCIE_ACTION);
}

static void sprd_pcie_resource_do_ep_wakeup(struct sprd_pcie_res *res)
{

	pr_info("pcie res: wakeup by ep.\n");

	/* power done fore wake up. */
	sprd_pms_power_down(res->pms, false);
}

static void sprd_pcie_resource_do_ep_load_done(struct sprd_pcie_res *res)
{
	pr_info("pcie res: load done.\n");

	res->ep_load_done = true;

	if (res->wait_scanned && res->ep_ready_for_rescan)
		sprd_add_action(res->actions_queue, REMOVE_PCIE_ACTION);
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

	case SCAN_PCIE_ACTION:
		sprd_pcie_resource_do_scan_pcie(res);
		break;

	case REMOVE_PCIE_ACTION:
		sprd_pcie_resource_do_remove_pcie(res);
		break;

	case EP_PROBE_ACTION:
		sprd_pcie_resource_do_ep_probe(res);
		break;

	case BAR_PROBE_ACTION:
		sprd_pcie_resource_do_2bar_probe(res);
		break;

	case EP_REMOVE_ACTION:
		sprd_pcie_resource_do_ep_remove(res);
		break;

	case EP_SCANNED_RESPOND_ACTION:
		sprd_pcie_resource_do_ep_scaned_respond(res);
		break;

	case EP_READY_SCAN_ACTION:
		sprd_pcie_resource_do_ready_scan(res);
		break;

	case EP_WAKEUP_ACTION:
		sprd_pcie_resource_do_ep_wakeup(res);
		break;

	case EP_LOAD_DONE_ACTION:
		sprd_pcie_resource_do_ep_load_done(res);
		break;

	default:
		break;
	}
}

bool sprd_pcie_ep_power_on(void)
{
	struct sprd_pcie_res *res;

	res = g_pcie_res[SIPC_ID_MINIAP];
	if (!res)
		return false;

	return res->ep_load_done > 0;
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

int sprd_pcie_resource_host_init(u32 dst, u32 ep_dev,
				 struct platform_device *pcie_dev)
{
	struct sprd_pcie_res *res;

	if (dst >= SIPC_ID_NR)
		return -EINVAL;

	res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	init_waitqueue_head(&res->wait_load_ready);
	init_waitqueue_head(&res->wait_pcie_ready);
	init_waitqueue_head(&res->wait_reboot_ep);
	init_waitqueue_head(&res->wait_ep_respond);
	init_waitqueue_head(&res->wait_remove_ep);

	wakeup_source_init(&res->ws, "pcie_res");

	res->dst = dst;
	res->state = SPRD_PCIE_WAIT_FIRST_READY;
	res->pcie_dev = pcie_dev;

	/* The defective chip , the first wait time must be enough long. */
	if (sprd_pcie_is_defective_chip())
		res->max_wait_time = MAX_PMS_DEFECTIVE_CHIP_FIRST_WAIT_TIME;
	else
		res->max_wait_time = MAX_PMS_WAIT_TIME;

	res->actions_queue =
		sprd_create_action_queue("pcie_res",
					 sprd_pcie_resource_do_actions, res, 9);
	if (!res->actions_queue) {
		pr_err("pcie res:create actions failed.\n");
		kfree(res);
		return -ENOMEM;
	}

	sprintf(res->pms_name, "ep-request-%d", dst);
	res->pms = sprd_pms_create(dst, res->pms_name, true);
	if (!res->pms)
		pr_err("pcie res:create pms failed.\n");

	sprd_ep_dev_register_irq_handler_ex(res->ep_dev,
					    PCIE_MSI_REQUEST_RES,
					    PCIE_MSI_REMOVE_RESPOND,
					    sprd_pcie_resource_host_irq_handler,
					    res);

	sprd_ep_dev_register_notify(res->ep_dev,
		sprd_pcie_resource_host_ep_notify, res);

	modem_ctrl_register_notifier(&mcd_notify);

	/* init wake up event callback */
	res->reg_event.events = SPRD_PCIE_EVENT_WAKEUP;
	res->reg_event.pdev = pcie_dev;
	res->reg_event.callback = sprd_pcie_resource_event_process;
	res->reg_event.data = res;

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

	sprd_pcie_deregister_event(&res->reg_event);

	sprd_ep_dev_unregister_irq_handler_ex(res->ep_dev,
					      PCIE_MSI_REQUEST_RES,
					      PCIE_MSI_RELEASE_RES);
	sprd_ep_dev_unregister_notify(res->ep_dev);
	modem_ctrl_unregister_notifier(&mcd_notify);
	sprd_pms_destroy(res->pms);
	sprd_destroy_action_queue(res->actions_queue);

	kfree(res);
	g_pcie_res[dst] = NULL;

	return 0;
}

