// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/of_platform.h>
#include <soc/qcom/secure_buffer.h>
#include "msm_cvp_dsp.h"

#define VMID_CDSP_Q6 (30)
#define SRC_VM_NUM 1
#define DEST_VM_NUM 2
#define CVP_DSP_SEND_HFI_CMD_QUEUE 0
#define CVP_DSP_SUSPEND 1
#define CVP_DSP_RESUME 2
#define CVP_DSP_SHUTDOWN 3
#define CVP_DSP_REGISTER_BUFFER 4
#define CVP_DSP_DEREGISTER_BUFFER 5
#define STATUS_INIT 0
#define STATUS_DEINIT 1
#define STATUS_OK 2
#define STATUS_SSR 3

struct cvpd_cmd_msg {
	uint32_t cmd_msg_type;
	int32_t ret_val;
	uint64_t msg_ptr;
	uint32_t msg_ptr_len;
	uint32_t iova_buff_addr;
	uint32_t buff_index;
	uint32_t buf_size;
	uint32_t session_id;
	uint32_t context;
};

struct cvpd_rsp_msg {
	uint32_t context;
	int32_t ret_val;
};

struct cvp_dsp_apps {
	struct rpmsg_device *chan;
	struct mutex smd_mutex;
	int rpmsg_register;
	uint32_t cdsp_state;
	uint32_t cvp_shutdown;
};

static struct completion work;

static struct cvp_dsp_apps gfa_cv;

static struct cvpd_cmd_msg cmd_msg;

static struct cvpd_rsp_msg cmd_msg_rsp;

static int cvp_dsp_send_cmd(void *msg, uint32_t len)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	int err;

	if (IS_ERR_OR_NULL(me->chan)) {
		err = -EINVAL;
		goto bail;
	}
	err = rpmsg_send(me->chan->ept, msg, len);

bail:
	return err;
}

static int cvp_dsp_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int err = 0;
	struct cvp_dsp_apps *me = &gfa_cv;
	uint32_t cdsp_state, cvp_shutdown;
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
	cvp_shutdown = me->cvp_shutdown;
	msg_ptr = cmd_msg.msg_ptr;
	msg_ptr_len =  cmd_msg.msg_ptr_len;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR && cvp_shutdown == STATUS_OK) {
		err = hyp_assign_phys((uint64_t)msg_ptr,
			msg_ptr_len, srcVM, DEST_VM_NUM, destVM,
			destVMperm, SRC_VM_NUM);
		if (err) {
			pr_err("%s: Failed to hyp_assign. err=%d\n",
				__func__, err);
			return err;
		}
		err = cvp_dsp_send_cmd_hfi_queue(
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

	pr_info("%s: Successfully probed. cdsp_state=%d cvp_shutdown=%d\n",
		__func__, cdsp_state, cvp_shutdown);
bail:
	return err;
}

static void cvp_dsp_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct cvp_dsp_apps *me = &gfa_cv;

	mutex_lock(&me->smd_mutex);
	me->chan = NULL;
	me->cdsp_state = STATUS_SSR;
	mutex_unlock(&me->smd_mutex);
	pr_info("%s: CDSP SSR triggered\n", __func__);
}

static int cvp_dsp_rpmsg_callback(struct rpmsg_device *rpdev,
	void *data, int len, void *priv, u32 addr)
{
	int *rpmsg_resp = (int *)data;

	cmd_msg_rsp.ret_val = *rpmsg_resp;
	complete(&work);

	return 0;
}

int cvp_dsp_send_cmd_hfi_queue(phys_addr_t *phys_addr,
	uint32_t size_in_bytes)
{
	int err;
	struct cvpd_cmd_msg local_cmd_msg;
	struct cvp_dsp_apps *me = &gfa_cv;
	int srcVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVMperm[DEST_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC,
		PERM_READ | PERM_WRITE | PERM_EXEC };

	local_cmd_msg.cmd_msg_type = CVP_DSP_SEND_HFI_CMD_QUEUE;
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

	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);
	else {
		mutex_lock(&me->smd_mutex);
		me->cvp_shutdown = STATUS_OK;
		me->cdsp_state = STATUS_OK;
		mutex_unlock(&me->smd_mutex);
	}

	return err;
}

int cvp_dsp_suspend(uint32_t session_flag)
{
	int err = 0;
	struct cvpd_cmd_msg local_cmd_msg;
	struct cvp_dsp_apps *me = &gfa_cv;
	uint32_t cdsp_state;

	mutex_lock(&me->smd_mutex);
	cdsp_state = me->cdsp_state;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR)
		return 0;

	local_cmd_msg.cmd_msg_type = CVP_DSP_SUSPEND;
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);

	return err;
}

int cvp_dsp_resume(uint32_t session_flag)
{
	int err;
	struct cvpd_cmd_msg local_cmd_msg;
	struct cvp_dsp_apps *me = &gfa_cv;
	uint32_t cdsp_state;

	mutex_lock(&me->smd_mutex);
	cdsp_state = me->cdsp_state;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR)
		return 0;

	local_cmd_msg.cmd_msg_type = CVP_DSP_RESUME;
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);

	return err;
}

int cvp_dsp_shutdown(uint32_t session_flag)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	int err, local_cmd_msg_rsp;
	struct cvpd_cmd_msg local_cmd_msg;
	int srcVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVMperm[SRC_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC };

	local_cmd_msg.cmd_msg_type = CVP_DSP_SHUTDOWN;
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvpd_cmd_msg));
	if (err != 0)
		pr_err("%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);

	wait_for_completion(&work);

	mutex_lock(&me->smd_mutex);
	me->cvp_shutdown = STATUS_SSR;
	local_cmd_msg.msg_ptr = cmd_msg.msg_ptr;
	local_cmd_msg.msg_ptr_len = cmd_msg.msg_ptr_len;
	mutex_unlock(&me->smd_mutex);
	local_cmd_msg_rsp = cmd_msg_rsp.ret_val;
	if (local_cmd_msg_rsp == 0) {
		err = hyp_assign_phys((uint64_t)local_cmd_msg.msg_ptr,
			local_cmd_msg.msg_ptr_len, srcVM, DEST_VM_NUM,
			destVM,	destVMperm, SRC_VM_NUM);
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

static const struct rpmsg_device_id cvp_dsp_rpmsg_match[] = {
	{ CVP_APPS_DSP_GLINK_GUID },
	{ },
};

static struct rpmsg_driver cvp_dsp_rpmsg_client = {
	.id_table = cvp_dsp_rpmsg_match,
	.probe = cvp_dsp_rpmsg_probe,
	.remove = cvp_dsp_rpmsg_remove,
	.callback = cvp_dsp_rpmsg_callback,
	.drv = {
		.name = "qcom,msm_cvp_dsp_rpmsg",
	},
};

static int __init cvp_dsp_device_init(void)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	int err;

	init_completion(&work);
	mutex_init(&me->smd_mutex);
	me->cvp_shutdown = STATUS_INIT;
	me->cdsp_state = STATUS_INIT;
	err = register_rpmsg_driver(&cvp_dsp_rpmsg_client);
	if (err) {
		pr_err("%s : register_rpmsg_driver failed with err %d\n",
			__func__, err);
		goto register_bail;
	}
	me->rpmsg_register = 1;
	return 0;

register_bail:
	me->cvp_shutdown = STATUS_DEINIT;
	me->cdsp_state = STATUS_DEINIT;
	return err;
}

static void __exit cvp_dsp_device_exit(void)
{
	struct cvp_dsp_apps *me = &gfa_cv;

	me->cvp_shutdown = STATUS_DEINIT;
	me->cdsp_state = STATUS_DEINIT;
	mutex_destroy(&me->smd_mutex);
	if (me->rpmsg_register == 1)
		unregister_rpmsg_driver(&cvp_dsp_rpmsg_client);
}

late_initcall(cvp_dsp_device_init);
module_exit(cvp_dsp_device_exit);

MODULE_LICENSE("GPL v2");
