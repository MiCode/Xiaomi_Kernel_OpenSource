// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/of_platform.h>
#include <soc/qcom/secure_buffer.h>
#include "linux/fastcvpd.h"

#define VMID_CDSP_Q6 (30)
#define SRC_VM_NUM 1
#define DEST_VM_NUM 2
#define FASTCVPD_VIDEO_SEND_HFI_CMD_QUEUE 0
#define FASTCVPD_VIDEO_SUSPEND 1
#define FASTCVPD_VIDEO_RESUME 2
#define FASTCVPD_VIDEO_SHUTDOWN 3
#define STATUS_INIT 0
#define STATUS_DEINIT 1
#define STATUS_OK 2
#define STATUS_SSR 3

struct fastcvpd_cmd_msg {
	uint32_t cmd_msg_type;
	int ret_val;
	uint64_t msg_ptr;
	uint32_t msg_ptr_len;
};

struct fastcvpd_cmd_msg_rsp {
	int ret_val;
};

struct fastcvpd_apps {
	struct rpmsg_device *chan;
	struct mutex smd_mutex;
	int rpmsg_register;
	uint32_t cdsp_state;
	uint32_t video_shutdown;
};

static struct completion work;

static struct fastcvpd_apps gfa_cv;

static struct fastcvpd_cmd_msg cmd_msg;

static struct fastcvpd_cmd_msg_rsp cmd_msg_rsp;

static int fastcvpd_send_cmd(void *msg, uint32_t len)
{
	struct fastcvpd_apps *me = &gfa_cv;
	int err;

	if (IS_ERR_OR_NULL(me->chan)) {
		err = -EINVAL;
		goto bail;
	}
	err = rpmsg_send(me->chan->ept, msg, len);

bail:
	return err;
}

static int fastcvpd_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int err = 0;
	struct fastcvpd_apps *me = &gfa_cv;
	uint32_t cdsp_state, video_shutdown;
	uint64_t msg_ptr;
	uint32_t msg_ptr_len;
	int srcVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVMperm[SRC_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC };

	if (strcmp(rpdev->dev.parent->of_node->name, "cdsp")) {
		pr_err("%s: Failed to probe rpmsg device.Node name:%s\n",
			__func__, rpdev->dev.parent->of_node->name);
		err = -EINVAL;
		goto bail;
	}
	mutex_lock(&me->smd_mutex);
	me->chan = rpdev;
	cdsp_state = me->cdsp_state;
	video_shutdown = me->video_shutdown;
	msg_ptr = cmd_msg.msg_ptr;
	msg_ptr_len =  cmd_msg.msg_ptr_len;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR && video_shutdown == STATUS_OK) {
		err = hyp_assign_phys((uint64_t)msg_ptr,
			msg_ptr_len, srcVM, DEST_VM_NUM, destVM,
			destVMperm, SRC_VM_NUM);
		if (err) {
			pr_err("%s: Failed to hyp_assign. err=%d\n",
				__func__, err);
			return err;
		}
		err = fastcvpd_video_send_cmd_hfi_queue(
			(phys_addr_t *)msg_ptr, msg_ptr_len);
		if (err) {
			pr_err("%s: Failed to send HFI Queue address. err=%d\n",
			__func__, err);
			goto bail;
		}
		mutex_lock(&me->smd_mutex);
		cdsp_state = me->cdsp_state;
		mutex_unlock(&me->smd_mutex);
	}

	pr_info("%s: Successfully probed. cdsp_state=%d video_shutdown=%d\n",
		__func__, cdsp_state, video_shutdown);
bail:
	return err;
}

static void fastcvpd_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct fastcvpd_apps *me = &gfa_cv;

	mutex_lock(&me->smd_mutex);
	me->chan = NULL;
	me->cdsp_state = STATUS_SSR;
	mutex_unlock(&me->smd_mutex);
	pr_info("%s: CDSP SSR triggered\n", __func__);
}

static int fastcvpd_rpmsg_callback(struct rpmsg_device *rpdev,
	void *data, int len, void *priv, u32 addr)
{
	int *rpmsg_resp = (int *)data;

	cmd_msg_rsp.ret_val = *rpmsg_resp;
	complete(&work);

	return 0;
}

int fastcvpd_video_send_cmd_hfi_queue(phys_addr_t *phys_addr,
	uint32_t size_in_bytes)
{
	int err;
	struct fastcvpd_cmd_msg local_cmd_msg;
	struct fastcvpd_apps *me = &gfa_cv;
	int srcVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVMperm[DEST_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC,
		PERM_READ | PERM_WRITE | PERM_EXEC };

	local_cmd_msg.cmd_msg_type = FASTCVPD_VIDEO_SEND_HFI_CMD_QUEUE;
	local_cmd_msg.msg_ptr = (uint64_t)phys_addr;
	local_cmd_msg.msg_ptr_len = size_in_bytes;
	mutex_lock(&me->smd_mutex);
	cmd_msg.msg_ptr = (uint64_t)phys_addr;
	cmd_msg.msg_ptr_len = (size_in_bytes);
	mutex_unlock(&me->smd_mutex);

	pr_debug("%s :: address of buffer, PA=0x%pK  size_buff=%d\n",
		__func__, phys_addr, size_in_bytes);

	err = hyp_assign_phys((uint64_t)local_cmd_msg.msg_ptr,
		local_cmd_msg.msg_ptr_len, srcVM, SRC_VM_NUM, destVM,
		destVMperm, DEST_VM_NUM);
	if (err) {
		pr_err("%s: Failed in hyp_assign. err=%d\n",
			__func__, err);
		return err;
	}

	err = fastcvpd_send_cmd
			 (&local_cmd_msg, sizeof(struct fastcvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: fastcvpd_send_cmd failed with err=%d\n",
			__func__, err);
	else {
		mutex_lock(&me->smd_mutex);
		me->video_shutdown = STATUS_OK;
		me->cdsp_state = STATUS_OK;
		mutex_unlock(&me->smd_mutex);
	}

	return err;
}
EXPORT_SYMBOL(fastcvpd_video_send_cmd_hfi_queue);

int fastcvpd_video_suspend(uint32_t session_flag)
{
	int err = 0;
	struct fastcvpd_cmd_msg local_cmd_msg;
	struct fastcvpd_apps *me = &gfa_cv;
	uint32_t cdsp_state;

	mutex_lock(&me->smd_mutex);
	cdsp_state = me->cdsp_state;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR)
		return 0;

	local_cmd_msg.cmd_msg_type = FASTCVPD_VIDEO_SUSPEND;
	err = fastcvpd_send_cmd
			 (&local_cmd_msg, sizeof(struct fastcvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: fastcvpd_send_cmd failed with err=%d\n",
			__func__, err);

	return err;
}
EXPORT_SYMBOL(fastcvpd_video_suspend);

int fastcvpd_video_resume(uint32_t session_flag)
{
	int err;
	struct fastcvpd_cmd_msg local_cmd_msg;
	struct fastcvpd_apps *me = &gfa_cv;
	uint32_t cdsp_state;

	mutex_lock(&me->smd_mutex);
	cdsp_state = me->cdsp_state;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR)
		return 0;

	local_cmd_msg.cmd_msg_type = FASTCVPD_VIDEO_RESUME;
	err = fastcvpd_send_cmd
			 (&local_cmd_msg, sizeof(struct fastcvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: fastcvpd_send_cmd failed with err=%d\n",
			__func__, err);

	return err;
}
EXPORT_SYMBOL(fastcvpd_video_resume);

int fastcvpd_video_shutdown(uint32_t session_flag)
{
	struct fastcvpd_apps *me = &gfa_cv;
	int err, local_cmd_msg_rsp;
	struct fastcvpd_cmd_msg local_cmd_msg;
	int srcVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVMperm[SRC_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC };

	local_cmd_msg.cmd_msg_type = FASTCVPD_VIDEO_SHUTDOWN;
	err = fastcvpd_send_cmd
			 (&local_cmd_msg, sizeof(struct fastcvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: fastcvpd_send_cmd failed with err=%d\n",
			__func__, err);

	wait_for_completion(&work);

	mutex_lock(&me->smd_mutex);
	me->video_shutdown = STATUS_SSR;
	local_cmd_msg.msg_ptr = cmd_msg.msg_ptr;
	local_cmd_msg.msg_ptr_len = cmd_msg.msg_ptr_len;
	mutex_unlock(&me->smd_mutex);
	local_cmd_msg_rsp = cmd_msg_rsp.ret_val;
	if (local_cmd_msg_rsp == 0) {
		err = hyp_assign_phys((uint64_t)local_cmd_msg.msg_ptr,
			local_cmd_msg.msg_ptr_len, srcVM, DEST_VM_NUM, destVM,
			destVMperm, SRC_VM_NUM);
		if (err) {
			pr_err("%s: Failed to hyp_assign. err=%d\n",
				__func__, err);
			return err;
		}
	} else {
		pr_err("%s: Skipping hyp_assign as CDSP sent invalid response=%d\n",
			__func__, local_cmd_msg_rsp);
	}

	return err;
}
EXPORT_SYMBOL(fastcvpd_video_shutdown);

static const struct rpmsg_device_id fastcvpd_rpmsg_match[] = {
	{ FASTCVPD_GLINK_GUID },
	{ },
};

static struct rpmsg_driver fastcvpd_rpmsg_client = {
	.id_table = fastcvpd_rpmsg_match,
	.probe = fastcvpd_rpmsg_probe,
	.remove = fastcvpd_rpmsg_remove,
	.callback = fastcvpd_rpmsg_callback,
	.drv = {
		.name = "qcom,msm_fastcvpd_rpmsg",
	},
};

static int __init fastcvpd_device_init(void)
{
	struct fastcvpd_apps *me = &gfa_cv;
	int err;

	init_completion(&work);
	mutex_init(&me->smd_mutex);
	me->video_shutdown = STATUS_INIT;
	me->cdsp_state = STATUS_INIT;
	err = register_rpmsg_driver(&fastcvpd_rpmsg_client);
	if (err) {
		pr_err("%s : register_rpmsg_driver failed with err %d\n",
			__func__, err);
		goto register_bail;
	}
	me->rpmsg_register = 1;
	return 0;

register_bail:
	me->video_shutdown = STATUS_DEINIT;
	me->cdsp_state = STATUS_DEINIT;
	return err;
}

static void __exit fastcvpd_device_exit(void)
{
	struct fastcvpd_apps *me = &gfa_cv;

	me->video_shutdown = STATUS_DEINIT;
	me->cdsp_state = STATUS_DEINIT;
	mutex_destroy(&me->smd_mutex);
	if (me->rpmsg_register == 1)
		unregister_rpmsg_driver(&fastcvpd_rpmsg_client);
}

late_initcall(fastcvpd_device_init);
module_exit(fastcvpd_device_exit);

MODULE_LICENSE("GPL v2");
