// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/rpmsg.h>
#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_remote.h"
#include "reviser_msg.h"

struct reviser_msg_item {
	struct reviser_msg msg;
	struct list_head list;
};


struct reviser_msg *g_reply;
struct reviser_msg_mgr *g_rvr_msg;

static struct reviser_msg g_reviser_msg_reply;
static struct reviser_msg_mgr g_msg_mgr;

bool reviser_is_remote(void)
{
	bool is_remote = false;

	mutex_lock(&g_rvr_msg->lock.mutex_mgr);
	if (g_rvr_msg->info.init) {
		//LOG_ERR("Can Not Read when rv disable\n");
		is_remote = true;
	}
	mutex_unlock(&g_rvr_msg->lock.mutex_mgr);

	return is_remote;
}
int reviser_remote_init(void)
{
	DEBUG_TAG;

	mutex_init(&g_msg_mgr.lock.mutex_ipi);
	mutex_init(&g_msg_mgr.lock.mutex_mgr);

	spin_lock_init(&g_msg_mgr.lock.lock_rx);
	init_waitqueue_head(&g_msg_mgr.lock.wait_rx);

	INIT_LIST_HEAD(&g_msg_mgr.list_rx);

	g_reply = &g_reviser_msg_reply;
	g_reply->sn = 0;
	g_rvr_msg = &g_msg_mgr;
	g_msg_mgr.info.init = true;

	return 0;
}
void reviser_remote_exit(void)
{
	DEBUG_TAG;

	g_msg_mgr.info.init = false;
	g_reply->sn = 0;
}


int reviser_remote_send_cmd_sync(void *drvinfo, void *request, void *reply, uint32_t timeout)
{
	// TODO No timeout
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;
	unsigned long flags;
	struct reviser_msg_item *item;
	struct list_head *tmp = NULL, *pos = NULL;
	struct reviser_msg *rmesg;
	int retry = 0;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;


	mutex_lock(&g_rvr_msg->lock.mutex_ipi);

	ret = rpmsg_send(rdv->rpdev->ept, request, sizeof(struct reviser_msg));

	mutex_unlock(&g_rvr_msg->lock.mutex_ipi);

wait:
	LOG_DBG_RVR_FLW("Wait for Getting cmd\n");
	//wait_event_interruptible(g_rvr_msg->lock.wait_rx,
	//		!list_empty(&g_rvr_msg->list_rx));
	ret = wait_event_interruptible_timeout(
				g_rvr_msg->lock.wait_rx,
				!list_empty(&g_rvr_msg->list_rx),
				msecs_to_jiffies(REVISER_REMOTE_TIMEOUT));

	if (ret == -ERESTARTSYS) {
		if (retry > 5) {
			LOG_ERR("Wake up by signal!, retry again");
			retry++;
			goto wait;
		} else {
			LOG_ERR("Wake up by signal!, Fail because it retry too many times!");
			ret = -ETIME;
			goto out;
		}

	}
	if (ret == 0) {
		LOG_ERR("wait command timeout!!\n");
		ret = -ETIME;
		goto out;
	}

	spin_lock_irqsave(&g_rvr_msg->lock.lock_rx, flags);

	list_for_each_safe(pos, tmp, &g_rvr_msg->list_rx) {
		item = list_entry(pos, struct reviser_msg_item, list);
		list_del(pos);
		rmesg = (struct reviser_msg *) &item->msg;
		LOG_DBG_RVR_FLW("item sn(%d) cmd(%x) option(%x) ack (%x)\n",
				rmesg->sn, rmesg->cmd, rmesg->option, rmesg->ack);

		memcpy(reply, rmesg, sizeof(struct reviser_msg));
		vfree(item);
		break;
	}

	spin_unlock_irqrestore(&g_rvr_msg->lock.lock_rx, flags);

	ret = 0;
out:
	return ret;
}


int reviser_remote_rx_cb(void *data, int len)
{
	unsigned long flags;
	struct reviser_msg_item *item;

	if (len != sizeof(struct reviser_msg)) {
		LOG_ERR("invalid len %d / %d\n", len, sizeof(struct reviser_msg));
		return -EINVAL;
	}

	item = vzalloc(sizeof(*item));

	memcpy(&item->msg, data, len);

	spin_lock_irqsave(&g_rvr_msg->lock.lock_rx, flags);
	list_add_tail(&item->list, &g_rvr_msg->list_rx);
	spin_unlock_irqrestore(&g_rvr_msg->lock.lock_rx, flags);

	wake_up_interruptible(&g_rvr_msg->lock.wait_rx);

	return 0;
}

int reviser_remote_sync_sn(void *drvinfo, uint32_t sn)
{
	mutex_lock(&g_rvr_msg->lock.mutex_mgr);

	g_rvr_msg->send_sn = sn;

	mutex_unlock(&g_rvr_msg->lock.mutex_mgr);

	return 0;
}

