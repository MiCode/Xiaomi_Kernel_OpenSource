// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/rpmsg.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/kthread.h>

#include "apusys_core.h"
#include "aov_rpmsg.h"
#include "apu_ipi.h"
#include "mdw_rv_msg.h"

#include "aov_recovery.h"

#define MDW_TIMEOUT_MS (100)

struct aov_rpmsg_ctx {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;

	/* ipi to apu */
	atomic_t param;
	struct completion notify_tx_apu;
	struct task_struct *apu_tx_worker;

	/* ipi to scp */
	atomic_t ack;
	struct completion notify_tx_scp;
	struct task_struct *scp_tx_worker;
};

static struct aov_rpmsg_ctx *rpmsg_ctx;

int aov_rpmsg_send(uint32_t param)
{
	if (!rpmsg_ctx)
		return -ENODEV;

	atomic_set(&rpmsg_ctx->param, param);
	complete(&rpmsg_ctx->notify_tx_apu);

	return 0;
}

int scp_mdw_handler(struct npu_scp_ipi_param *recv_msg)
{
	int ret = 0;

	if (!recv_msg)
		return -EINVAL;

	switch (recv_msg->act) {
	case NPU_SCP_NP_MDW_ACK:
		pr_debug_ratelimited("%s Get Ack\n", __func__);
		break;
	case NPU_SCP_NP_MDW_TO_APMCU:
		ret = aov_rpmsg_send(APU_IPI_SCP_MIDDLEWARE);
		break;
	default:
		pr_info("%s Not supported act %d\n", __func__, recv_msg->act);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int apu_tx_thread(void *data)
{
	struct aov_rpmsg_ctx *ctx = (struct aov_rpmsg_ctx *)data;
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	long timeout = MAX_SCHEDULE_TIMEOUT;

	pr_info("%s start +++\n", __func__);

	while (!kthread_should_stop()) {
		int ret = 0, retry_cnt = 20;
		uint32_t param = 0;

		wait_for_completion_interruptible_timeout(&ctx->notify_tx_apu, timeout);

		/* If apu is recovering, skip sending to apu */
		if (get_aov_recovery_state() == AOV_APU_RECOVERING) {
			do {
				send_msg.cmd = NPU_SCP_NP_MDW;
				send_msg.act = NPU_SCP_NP_MDW_ACK;
				send_msg.arg = MDW_SCP_IPI_BUSY;

				ret = npu_scp_ipi_send(&send_msg, NULL, MDW_TIMEOUT_MS);
				if (ret)
					pr_info("%s Failed to send to scp, ret %d, retry_cnt %d\n",
						__func__, ret, retry_cnt);
			} while (ret != 0 && retry_cnt-- > 0);

			continue;
		}

		param = atomic_read(&ctx->param);

		do {
			ret = rpmsg_send(ctx->ept, &param, sizeof(param));
			pr_debug_ratelimited("%s rpmsg_send %d ret %d\n", __func__, param, ret);
			/* send busy, retry */
			if (ret == -EBUSY || ret == -EAGAIN) {
				pr_info("%s: re-send ipi(retry_cnt = %d)\n", __func__, retry_cnt);

				if (ret == -EAGAIN && retry_cnt > 15)
					usleep_range(200, 500);
				else if (ret == -EAGAIN && retry_cnt > 10)
					usleep_range(1000, 2000);
				else
					usleep_range(10000, 11000);
			}
		} while ((ret == -EBUSY || ret == -EAGAIN) && retry_cnt-- > 0);

		if (ret) {
			pr_info("%s Failed to send ipi to apu, ret %d\n", __func__, ret);

			retry_cnt = 20;
			do {
				send_msg.cmd = NPU_SCP_NP_MDW;
				send_msg.act = NPU_SCP_NP_MDW_TO_SCP;
				send_msg.arg = MDW_SCP_IPI_BUSY;

				ret = npu_scp_ipi_send(&send_msg, NULL, MDW_TIMEOUT_MS);
				if (ret)
					pr_info("%s Failed to notify scp, ret %d, retry_cnt %d\n",
						__func__, ret, retry_cnt);
			} while (ret != 0 && retry_cnt-- > 0);
		}
	}

	pr_info("%s end ---\n", __func__);

	return 0;
}

static int scp_tx_thread(void *data)
{
	struct aov_rpmsg_ctx *ctx = (struct aov_rpmsg_ctx *)data;
	long timeout = MAX_SCHEDULE_TIMEOUT;

	pr_info("%s start +++\n", __func__);

	while (!kthread_should_stop()) {
		int ret = 0, retry_cnt = 10;
		struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };

		wait_for_completion_interruptible_timeout(&ctx->notify_tx_scp, timeout);

		do {
			send_msg.cmd = NPU_SCP_NP_MDW;
			send_msg.act = NPU_SCP_NP_MDW_TO_SCP;
			send_msg.arg = 0;

			ret = npu_scp_ipi_send(&send_msg, NULL, MDW_TIMEOUT_MS);
			pr_debug_ratelimited("%s scp ipi, ret %d\n", __func__, ret);
			if (ret)
				pr_info("%s Failed to send to scp, ret %d, retry_cnt %d\n",
					__func__, ret, retry_cnt);
		} while (ret != 0 && retry_cnt-- > 0);
	}

	pr_info("%s end ---\n", __func__);

	return 0;
}

static int aov_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;

	pr_info("%s name %s, src %d\n", __func__, rpdev->id.name, rpdev->src);

	rpmsg_ctx = kzalloc(sizeof(*rpmsg_ctx), GFP_KERNEL);
	if (!rpmsg_ctx)
		return -ENOMEM;

	rpmsg_ctx->ept = rpdev->ept;
	rpmsg_ctx->rpdev = rpdev;
	atomic_set(&rpmsg_ctx->param, 0);
	init_completion(&rpmsg_ctx->notify_tx_apu);

	/* create a kthread for sending to apu  */
	rpmsg_ctx->apu_tx_worker = kthread_create(apu_tx_thread, (void *)rpmsg_ctx,
						  "aov-apu-thread");
	if (IS_ERR(rpmsg_ctx->apu_tx_worker)) {
		ret = PTR_ERR(rpmsg_ctx->apu_tx_worker);
		goto apu_kthread_error;
	}

	set_user_nice(rpmsg_ctx->apu_tx_worker, PRIO_TO_NICE(MAX_RT_PRIO) + 1);
	sched_set_fifo(rpmsg_ctx->apu_tx_worker);
	wake_up_process(rpmsg_ctx->apu_tx_worker);

	/* create a kthread for sending to scp  */
	init_completion(&rpmsg_ctx->notify_tx_scp);

	rpmsg_ctx->scp_tx_worker = kthread_create(scp_tx_thread, (void *)rpmsg_ctx,
						  "aov-scp-thread");
	if (IS_ERR(rpmsg_ctx->scp_tx_worker)) {
		ret = PTR_ERR(rpmsg_ctx->scp_tx_worker);
		goto scp_kthread_error;
	}

	set_user_nice(rpmsg_ctx->scp_tx_worker, PRIO_TO_NICE(MAX_RT_PRIO) + 1);
	sched_set_fifo(rpmsg_ctx->scp_tx_worker);
	wake_up_process(rpmsg_ctx->scp_tx_worker);

	pr_info("%s ---\n", __func__);

	return 0;

scp_kthread_error:
	complete_all(&rpmsg_ctx->notify_tx_apu);
	kthread_stop(rpmsg_ctx->apu_tx_worker);
apu_kthread_error:
	kfree(rpmsg_ctx);
	return ret;
}

static int aov_rpmsg_callback(struct rpmsg_device *rpdev, void *data, int len, void *priv, u32 src)
{
	struct mdw_ipi_msg *ret_msg = (struct mdw_ipi_msg *)data;

	pr_debug_ratelimited("%s get src %d\n", __func__, src);

	if (!ret_msg || len != sizeof(struct mdw_ipi_msg)) {
		pr_info("%s get NULL or error returned msg\n", __func__);
		return 0;
	}

	if (ret_msg->ret == MDW_IPI_MSG_STATUS_ABORT) {
		pr_info("%s get err %d, drop it\n", __func__, ret_msg->ret);
		return 0;
	}

	complete(&rpmsg_ctx->notify_tx_scp);

	return 0;
}

static void aov_rpmsg_remove(struct rpmsg_device *rpdev)
{
	pr_info("%s +++\n", __func__);

	if (!rpmsg_ctx) {
		pr_info("%s aov rpmsg context is not available\n", __func__);
		return;
	}

	complete_all(&rpmsg_ctx->notify_tx_apu);
	kthread_stop(rpmsg_ctx->apu_tx_worker);

	complete_all(&rpmsg_ctx->notify_tx_scp);
	kthread_stop(rpmsg_ctx->scp_tx_worker);

	kfree(rpmsg_ctx);

	rpmsg_ctx = NULL;

	pr_info("%s ---\n", __func__);
}

static const struct of_device_id apu_aov_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-scp-mdw-rpmsg", },
	{},
};

static struct rpmsg_driver aov_rpmsg_driver = {
	.drv = {
		.name = "apu-scp-mdw-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = apu_aov_rpmsg_of_match,
	},
	.probe = aov_rpmsg_probe,
	.callback = aov_rpmsg_callback,
	.remove = aov_rpmsg_remove,
};

int aov_rpmsg_init(struct apusys_core_info *info)
{
	int ret = 0;

	pr_info("%s +++\n", __func__);
	ret = register_rpmsg_driver(&aov_rpmsg_driver);
	if (ret)
		pr_info("%s Failed to register aov rpmsg driver, ret %d\n", __func__, ret);
	pr_info("%s ---\n", __func__);

	return ret;
}

void aov_rpmsg_exit(void)
{
	unregister_rpmsg_driver(&aov_rpmsg_driver);
}
