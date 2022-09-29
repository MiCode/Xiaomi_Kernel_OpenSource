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

#include "scp.h"
#include "apusys_core.h"
#include "aov_recovery.h"
#include "apu_ipi.h"
#include "apu_hw_sema.h"

#define RECOVERY_TIMEOUT_MS (100)

struct aov_recovery_ctx {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;

	/* ipi to apu */
	atomic_t apu_status;
	struct completion notify_tx_apu;
	struct task_struct *apu_tx_worker;

	/* ipi to scp */
	atomic_t ack_to_scp;
	struct completion notify_tx_scp;
	struct task_struct *scp_tx_worker;
};

static struct aov_recovery_ctx *recovery_ctx;

enum aov_apu_recovery_status get_aov_recovery_state(void)
{
	if (recovery_ctx)
		return (enum aov_apu_recovery_status)atomic_read(&recovery_ctx->apu_status);

	pr_info("%s recovery context is not initialized\n", __func__);

	return AOV_APU_INIT;
}

int aov_recovery_handler(struct npu_scp_ipi_param *recv_msg)
{
	int ret = 0;

	if (!recv_msg)
		return -EINVAL;

	if (!recovery_ctx) {
		pr_info("%s aov-recovery ctx is not available\n", __func__);
		return -ENODEV;
	}

	switch (recv_msg->act) {
	case NPU_SCP_RECOVERY_ACK:
		pr_info("%s Get NPU_SCP_RECOVERY_ACK\n", __func__); //debug
		break;
	case NPU_SCP_RECOVERY_TO_APMCU:
		pr_info("%s Get NPU_SCP_RECOVERY_TO_APMCU\n", __func__); //debug
		atomic_set(&recovery_ctx->ack_to_scp, 1);
		complete(&recovery_ctx->notify_tx_scp);
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
	struct aov_recovery_ctx *ctx = (struct aov_recovery_ctx *)data;
	long timeout = MAX_SCHEDULE_TIMEOUT;

	pr_info("%s start +++\n", __func__);

	while (!kthread_should_stop()) {
		int ret = 0, retry_cnt = 10;

		wait_for_completion_interruptible_timeout(&ctx->notify_tx_apu, timeout);

		do {
			uint32_t param = APU_IPI_SCP_NP_RECOVER;

			ret = rpmsg_send(ctx->ept, &param, sizeof(param));
			pr_info("%s send APU_IPI_SCP_NP_RECOVER, ret %d\n", __func__, ret); //debug
			/* send busy, retry */
			if (ret == -EBUSY || ret == -EAGAIN) {
				pr_info("%s: re-send ipi(retry_cnt = %d)\n", __func__, retry_cnt);
				usleep_range(10000, 11000);
			}
		} while ((ret == -EBUSY || ret == -EAGAIN) && retry_cnt-- > 0);

		if (ret)
			pr_info("%s Failed to send recover ipi to apu, ret %d\n", __func__, ret);
	}

	pr_info("%s end ---\n", __func__);

	return 0;
}

static int scp_tx_thread(void *data)
{
	struct aov_recovery_ctx *ctx = (struct aov_recovery_ctx *)data;
	long timeout = MAX_SCHEDULE_TIMEOUT;

	pr_info("%s start +++\n", __func__);

	while (!kthread_should_stop()) {
		int status;
		int ret = 0, retry_cnt = 10;
		struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };

		wait_for_completion_interruptible_timeout(&ctx->notify_tx_scp, timeout);

		status = atomic_read(&recovery_ctx->apu_status);
		if (status == AOV_APU_RECOVER_DONE) {
			send_msg.cmd = NPU_SCP_RECOVERY;
			send_msg.act = NPU_SCP_RECOVERY_TO_SCP;
			send_msg.ret = status;

			pr_info("%s send NPU_SCP_RECOVERY_TO_SCP\n", __func__); //debug

			ret = npu_scp_ipi_send(&send_msg, NULL, RECOVERY_TIMEOUT_MS);
			if (ret)
				pr_info("%s Failed to send to scp, ret %d, retry_cnt %d\n",
					__func__, ret, retry_cnt);

			atomic_set(&recovery_ctx->apu_status, AOV_APU_INIT);
		}

		status = atomic_read(&recovery_ctx->apu_status);
		if (atomic_read(&recovery_ctx->ack_to_scp)) {
			send_msg.cmd = NPU_SCP_RECOVERY;
			send_msg.act = NPU_SCP_RECOVERY_ACK;
			send_msg.ret = status;

			pr_info("%s send NPU_SCP_RECOVERY_ACK\n", __func__); //debug

			do {

				ret = npu_scp_ipi_send(&send_msg, NULL, RECOVERY_TIMEOUT_MS);
				if (ret)
					pr_info("%s Failed to send to scp, ret %d, retry_cnt %d\n",
						__func__, ret, retry_cnt);
			} while (ret != 0 && retry_cnt-- > 0);

			atomic_set(&recovery_ctx->ack_to_scp, 0);
		}
	}

	pr_info("%s end ---\n", __func__);

	return 0;
}

static int aov_recovery_scp_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (!recovery_ctx) {
		pr_info("%s aov-recovery ctx is not available\n", __func__);
		return NOTIFY_DONE;
	}

	if (event == SCP_EVENT_STOP) {
		pr_info("%s receive scp stop event\n", __func__); //debug

		if (apu_boot_host() != SYS_SCP_LP) {
			pr_info("%s SCP stop when APU is not LP mode\n", __func__);
			atomic_set(&recovery_ctx->apu_status, AOV_APU_RECOVERING);
			complete(&recovery_ctx->notify_tx_apu);
		}
	} else if (event == SCP_EVENT_READY)
		pr_info("%s receive scp ready event\n", __func__); //debug

	return NOTIFY_DONE;
}

static struct notifier_block aov_recovery_scp_notifier = {
	.notifier_call = aov_recovery_scp_notifier_call,
};

static int aov_recovery_callback(struct rpmsg_device *rpdev, void *data, int len, void *priv,
				 u32 src)
{
	pr_debug("%s get src %d\n", __func__, src);

	if (!recovery_ctx) {
		pr_info("%s aov-recovery ctx is not available\n", __func__);
		return -ENODEV;
	}

	atomic_set(&recovery_ctx->apu_status, AOV_APU_RECOVER_DONE);
	complete(&recovery_ctx->notify_tx_scp);

	return 0;
}

static int aov_recovery_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;

	pr_info("%s name %s, src %d\n", __func__, rpdev->id.name, rpdev->src);

	recovery_ctx = kzalloc(sizeof(*recovery_ctx), GFP_KERNEL);
	if (!recovery_ctx)
		return -ENOMEM;

	recovery_ctx->ept = rpdev->ept;
	recovery_ctx->rpdev = rpdev;
	atomic_set(&recovery_ctx->apu_status, AOV_APU_INIT);
	atomic_set(&recovery_ctx->ack_to_scp, 0);
	init_completion(&recovery_ctx->notify_tx_apu);

	/* create a kthread for sending to apu  */
	recovery_ctx->apu_tx_worker = kthread_create(apu_tx_thread, (void *)recovery_ctx,
						  "aov-recovery-apu-thread");
	if (IS_ERR(recovery_ctx->apu_tx_worker)) {
		ret = PTR_ERR(recovery_ctx->apu_tx_worker);
		goto apu_kthread_error;
	}

	//set_user_nice(recovery_ctx->apu_tx_worker, PRIO_TO_NICE(MAX_RT_PRIO) + 1);
	wake_up_process(recovery_ctx->apu_tx_worker);

	/* create a kthread for sending to scp  */
	init_completion(&recovery_ctx->notify_tx_scp);

	recovery_ctx->scp_tx_worker = kthread_create(scp_tx_thread, (void *)recovery_ctx,
						  "aov-recovery-scp-thread");
	if (IS_ERR(recovery_ctx->scp_tx_worker)) {
		ret = PTR_ERR(recovery_ctx->scp_tx_worker);
		goto scp_kthread_error;
	}

	//set_user_nice(recovery_ctx->scp_tx_worker, PRIO_TO_NICE(MAX_RT_PRIO) + 1);
	wake_up_process(recovery_ctx->scp_tx_worker);

	scp_A_register_notify(&aov_recovery_scp_notifier);

	pr_info("%s ---\n", __func__);

	return 0;

scp_kthread_error:
	complete_all(&recovery_ctx->notify_tx_apu);
	kthread_stop(recovery_ctx->apu_tx_worker);
apu_kthread_error:
	kfree(recovery_ctx);
	return ret;
}

static void aov_recovery_remove(struct rpmsg_device *rpdev)
{
	pr_info("%s +++\n", __func__);

	if (!recovery_ctx) {
		pr_info("%s aov rpmsg context is not available\n", __func__);
		return;
	}

	scp_A_unregister_notify(&aov_recovery_scp_notifier);

	complete_all(&recovery_ctx->notify_tx_apu);
	kthread_stop(recovery_ctx->apu_tx_worker);

	complete_all(&recovery_ctx->notify_tx_scp);
	kthread_stop(recovery_ctx->scp_tx_worker);

	kfree(recovery_ctx);

	recovery_ctx = NULL;

	pr_info("%s ---\n", __func__);
}

static const struct of_device_id apu_aov_recovery_of_match[] = {
	{ .compatible = "mediatek,apu-scp-np-recover-rpmsg", },
	{},
};

static struct rpmsg_driver aov_recovery_driver = {
	.drv = {
		.name = "apu-scp-np-recover-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = apu_aov_recovery_of_match,
	},
	.probe = aov_recovery_probe,
	.callback = aov_recovery_callback,
	.remove = aov_recovery_remove,
};

int aov_recovery_init(struct apusys_core_info *info)
{
	int ret = 0;

	pr_info("%s +++\n", __func__);
	ret = register_rpmsg_driver(&aov_recovery_driver);
	if (ret)
		pr_info("%s Failed to register aov rpmsg driver, ret %d\n", __func__, ret);
	pr_info("%s ---\n", __func__);

	return ret;
}

void aov_recovery_exit(void)
{
	unregister_rpmsg_driver(&aov_recovery_driver);
}
