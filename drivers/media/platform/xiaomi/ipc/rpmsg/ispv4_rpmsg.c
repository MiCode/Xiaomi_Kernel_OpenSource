// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <uapi/linux/sched/types.h>
#include <linux/debugfs.h>
#include <linux/atomic.h>
#include "../rproc/ispv4_rproc.h"
#include "ispv4_rpmsg.h"
#include <linux/component.h>

#define USR_WORKQEUEU

extern struct rpmsg_driver xm_ipc_rpmsg_driver;

#ifndef USR_WORKQEUEU
static int ispv4_rpmsg_thread(void *data)
{
	struct xm_ispv4_rproc *rp = data;
	int vqid;
	bool do_work;
	unsigned long flags;

	dev_info(rp->dev, "%s Entry\n", __FUNCTION__);
	while (!kthread_should_stop()) {
		//wake -2
		spin_lock_irqsave(&rp->rpmsg_recv_lock, flags);
		if (rp->rpmsg_recv_count == 0) {
			set_current_state(TASK_INTERRUPTIBLE);
			do_work = false;
		} else {
			rp->rpmsg_recv_count--;
			do_work = true;
		}
		spin_unlock_irqrestore(&rp->rpmsg_recv_lock, flags);
		//wake -1
		if (do_work) {
			for (vqid = 0; vqid <= rp->rproc->max_notifyid; vqid++) {
				rproc_vq_interrupt(rp->rproc, vqid);
				dev_info(rp->dev, "Notify rproc vq %d\n", vqid);
			}
		}
		//wake -1
		schedule();
		// wake -2
	}

	rp->rpmsg_th = NULL;
	return 0;
}
#else
static void xm_ispv4_rpmsg_rxwork(struct work_struct *work)
{
	int vqid;
	struct xm_ispv4_rproc *rp =
		container_of(work, struct xm_ispv4_rproc, rpmsg_recvwork);
	for (vqid = 0; vqid <= rp->rproc->max_notifyid; vqid++) {
		dev_info(rp->dev, "Notify rproc vq %d start\n", vqid);
		rproc_vq_interrupt(rp->rproc, vqid);
		dev_info(rp->dev, "Notify rproc vq %d end\n", vqid);
	}
}
#endif

int xm_ispv4_rpmsg_init(struct xm_ispv4_rproc *rp)
{
	int ret;
	int i;
	struct sched_param param = {
		.sched_priority = 3 * MAX_RT_PRIO / 4,
	};
	(void)param;

	for(i = 0; i < XM_ISPV4_IPC_EPT_MAX; i++)
		rp->ipc_stopsend[i] = false;

	ret = register_rpmsg_driver(&xm_ipc_rpmsg_driver);
	if (ret < 0) {
		dev_err(rp->dev, "register rpmsg driver failed: %d\n", ret);
		return ret;
	}

#ifndef USR_WORKQEUEU
	spin_lock_init(&rp->rpmsg_recv_lock);
	rp->rpmsg_recv_count = 0;

	rp->rpmsg_th = kthread_run(ispv4_rpmsg_thread, rp, dev_name(rp->dev));
	if (IS_ERR(rp->rpmsg_th)) {
		ret = PTR_ERR(rp->rpmsg_th);
		unregister_rpmsg_driver(&xm_ipc_rpmsg_driver);
		dev_err(rp->dev, "kthread_create failed: %d\n", ret);
		return ret;
	}

	sched_setscheduler(rp->rpmsg_th, SCHED_FIFO, &param);
#else
	INIT_WORK(&rp->rpmsg_recvwork, xm_ispv4_rpmsg_rxwork);
#endif

	rp->rpmsg_inited = true;
	return 0;
}

void xm_ispv4_rpmsg_mbox_cb(struct mbox_client *cl, void *mssg)
{
	struct xm_ispv4_rproc *rp =
		container_of(cl, struct xm_ispv4_rproc, mbox_rpmsg);
	(void)mssg;
	pr_info("ispv4 rpmsg mbox cb\n");
#ifndef USR_WORKQEUEU
	xm_ispv4_rpmsg_irq(0, rp);
#else
	queue_work(system_wq, &rp->rpmsg_recvwork);
#endif
}

irqreturn_t xm_ispv4_rpmsg_irq(int irq, void *data)
{
	struct xm_ispv4_rproc *rp = data;
	spin_lock(&rp->rpmsg_recv_lock);
	if (rp->rpmsg_recv_count == 0) {
		rp->rpmsg_recv_count++;
		if (likely(rp->rpmsg_th != NULL))
			wake_up_process(rp->rpmsg_th);
	}
	spin_unlock(&rp->rpmsg_recv_lock);
	return IRQ_HANDLED;
}

void xm_ispv4_flush_send_stop(struct xm_ispv4_rproc *rp)
{
	int i;
	for(i = 0; i < XM_ISPV4_IPC_EPT_MAX; i++) {
		rp->ipc_stopsend[i] = true;
		smp_wmb();
		/* This time will not release rpeptdev */
		if (rp->rpeptdev[i] != NULL) {
			complete(&rp->rpeptdev[i]->cmd_complete);
			dev_err(rp->dev, "Early down rpmsg send");
		}
		mutex_lock(&rp->rpeptdev_lock[i]);
		/* Make sure all send context exit or could not entry */
		mutex_unlock(&rp->rpeptdev_lock[i]);
	}
}

void xm_ispv4_rpmsg_stopdeal(struct xm_ispv4_rproc *rp)
{
	struct task_struct *th = rp->rpmsg_th;
	(void)th;
	rp->rpmsg_th = NULL;
	if (rp->rpmsg_inited == true) {
		dev_alert(rp->dev, "%s", __FUNCTION__);
#ifndef USR_WORKQEUEU
		/* Prevent thread into `S` when waking up */
		rp->rpmsg_recv_count = U32_MAX;
		kthread_stop(th);
#else
		cancel_work_sync(&rp->rpmsg_recvwork);
#endif
	}
}

void xm_ispv4_rpmsg_exit(struct xm_ispv4_rproc *rp)
{
	if (rp->rpmsg_inited == true) {
		unregister_rpmsg_driver(&xm_ipc_rpmsg_driver);
		rp->rpmsg_inited = false;
	}
}

MODULE_LICENSE("GPL v2");
