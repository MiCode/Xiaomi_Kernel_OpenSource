// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
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
#define CVP_DSP_MAX_RESERVED 5

struct cvp_dsp_cmd_msg {
	uint32_t cmd_msg_type;
	int32_t ret_val;
	uint64_t msg_ptr;
	uint32_t msg_ptr_len;
	uint32_t buff_fd_iova;
	uint32_t buff_index;
	uint32_t buff_size;
	uint32_t session_id;
	int32_t ddr_type;
	uint32_t buff_fd;
	uint32_t buff_offset;
	uint32_t buff_fd_size;
	uint32_t reserved1;
	uint32_t reserved2;
};

struct cvp_dsp_rsp_msg {
	uint32_t cmd_msg_type;
	int32_t ret_val;
	uint32_t reserved[CVP_DSP_MAX_RESERVED];
};

struct cvp_dsp_rsp_context {
	struct completion work;
};

struct cvp_dsp_apps {
	struct rpmsg_device *chan;
	struct mutex smd_mutex;
	struct mutex reg_buffer_mutex;
	struct mutex dereg_buffer_mutex;
	int rpmsg_register;
	uint32_t cdsp_state;
	uint32_t cvp_shutdown;
	struct completion reg_buffer_work;
	struct completion dereg_buffer_work;
	struct completion shutdown_work;
	struct completion cmdqueue_send_work;
	struct work_struct ssr_work;
	struct iris_hfi_device *device;
};


static struct cvp_dsp_apps gfa_cv;

static struct cvp_dsp_cmd_msg cmd_msg;

static struct cvp_dsp_rsp_msg cmd_msg_rsp;

static int cvp_dsp_send_cmd(void *msg, uint32_t len)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	int err = 0;

	if (IS_ERR_OR_NULL(me->chan)) {
		dprintk(CVP_ERR, "%s: DSP GLink is not ready\n", __func__);
		err = -EINVAL;
		goto bail;
	}
	err = rpmsg_send(me->chan->ept, msg, len);

bail:
	return err;
}

void msm_cvp_cdsp_ssr_handler(struct work_struct *work)
{
	struct cvp_dsp_apps *me;
	uint64_t msg_ptr;
	uint32_t msg_ptr_len;
	int err;

	me = container_of(work, struct cvp_dsp_apps, ssr_work);
	if (!me) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return;
	}

	msg_ptr = cmd_msg.msg_ptr;
	msg_ptr_len =  cmd_msg.msg_ptr_len;

	err = cvp_dsp_send_cmd_hfi_queue((phys_addr_t *)msg_ptr,
					msg_ptr_len,
					(void *)NULL);
	if (err) {
		dprintk(CVP_ERR,
			"%s: Failed to send HFI Queue address. err=%d\n",
			__func__, err);
		return;
	}

	if (me->device) {
		mutex_lock(&me->device->lock);
		me->device->dsp_flags |= DSP_INIT;
		mutex_unlock(&me->device->lock);
	}
	dprintk(CVP_DBG, "%s: dsp recover from SSR successfully\n", __func__);
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
		dprintk(CVP_ERR,
			"%s: Failed to probe rpmsg device.Node name:%s\n",
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
			dprintk(CVP_ERR,
				"%s: Failed to hyp_assign. err=%d\n",
				__func__, err);
			return err;
		}
		schedule_work(&me->ssr_work);
		mutex_lock(&me->smd_mutex);
		cdsp_state = me->cdsp_state;
		mutex_unlock(&me->smd_mutex);
	}

	dprintk(CVP_INFO,
		"%s: Successfully probed. cdsp_state=%d cvp_shutdown=%d\n",
		__func__, cdsp_state, cvp_shutdown);
bail:
	return err;
}

static void cvp_dsp_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct cvp_dsp_apps *me = &gfa_cv;

	cancel_work_sync(&me->ssr_work);
	mutex_lock(&me->smd_mutex);
	me->chan = NULL;
	me->cdsp_state = STATUS_SSR;
	if (me->device) {
		mutex_lock(&me->device->lock);
		me->device->dsp_flags &= ~DSP_INIT;
		mutex_unlock(&me->device->lock);
	}
	mutex_unlock(&me->smd_mutex);
	dprintk(CVP_INFO,
		"%s: CDSP SSR triggered\n", __func__);
}

static int cvp_dsp_rpmsg_callback(struct rpmsg_device *rpdev,
	void *data, int len, void *priv, u32 addr)
{
	struct cvp_dsp_rsp_msg *dsp_response =
		(struct cvp_dsp_rsp_msg *)data;
	struct cvp_dsp_apps *me = &gfa_cv;

	dprintk(CVP_DBG,
		"%s: cmd_msg_type=0x%x dsp_response->ret_val =0x%x\n"
		, __func__, dsp_response->cmd_msg_type, dsp_response->ret_val);
	switch (dsp_response->cmd_msg_type) {
	case CVP_DSP_REGISTER_BUFFER:
		complete(&me->reg_buffer_work);
		break;
	case CVP_DSP_DEREGISTER_BUFFER:
		complete(&me->dereg_buffer_work);
		break;
	case CVP_DSP_SHUTDOWN:
		complete(&me->shutdown_work);
		break;
	case CVP_DSP_SUSPEND:
		break;
	case CVP_DSP_RESUME:
		break;
	case CVP_DSP_SEND_HFI_CMD_QUEUE:
		complete(&me->cmdqueue_send_work);
		break;
	default:
		dprintk(CVP_ERR,
		"%s: Invalid cmd_msg_type received from dsp: %d\n",
		__func__, dsp_response->cmd_msg_type);
		break;
	}
	return 0;
}

int cvp_dsp_send_cmd_hfi_queue(phys_addr_t *phys_addr,
				uint32_t size_in_bytes,
				struct iris_hfi_device *device)
{
	int err, timeout;
	struct msm_cvp_core *core;
	struct cvp_dsp_cmd_msg local_cmd_msg;
	struct cvp_dsp_apps *me = &gfa_cv;
	int srcVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVMperm[DEST_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC,
		PERM_READ | PERM_WRITE | PERM_EXEC };

	local_cmd_msg.cmd_msg_type = CVP_DSP_SEND_HFI_CMD_QUEUE;
	local_cmd_msg.msg_ptr = (uint64_t)phys_addr;
	local_cmd_msg.msg_ptr_len = size_in_bytes;
	local_cmd_msg.ddr_type = of_fdt_get_ddrtype();
	if (local_cmd_msg.ddr_type < 0) {
		dprintk(CVP_ERR,
			"%s: Incorrect DDR type value %d\n",
			__func__, local_cmd_msg.ddr_type);
		err = -EINVAL;
		goto exit;
	}

	mutex_lock(&me->smd_mutex);
	cmd_msg.msg_ptr = (uint64_t)phys_addr;
	cmd_msg.msg_ptr_len = (size_in_bytes);
	me->device = device;
	mutex_unlock(&me->smd_mutex);

	dprintk(CVP_DBG,
		"%s: address of buffer, PA=0x%pK  size_buff=%d ddr_type=%d\n",
		__func__, phys_addr, size_in_bytes, local_cmd_msg.ddr_type);

	err = hyp_assign_phys((uint64_t)local_cmd_msg.msg_ptr,
		local_cmd_msg.msg_ptr_len, srcVM, SRC_VM_NUM, destVM,
		destVMperm, DEST_VM_NUM);
	if (err) {
		dprintk(CVP_ERR,
			"%s: Failed in hyp_assign. err=%d\n",
			__func__, err);
		goto exit;
	}

	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvp_dsp_cmd_msg));
	if (err) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd faidmesgled with err=%d\n",
			__func__, err);
		goto exit;
	}

	core = list_first_entry(&cvp_driver->cores,
			struct msm_cvp_core, list);
	timeout = msecs_to_jiffies(
			core->resources.msm_cvp_dsp_rsp_timeout);
	if (!wait_for_completion_timeout(&me->cmdqueue_send_work, timeout)) {
		dprintk(CVP_ERR, "failed to send cmdqueue\n");
		err =  -ETIMEDOUT;
		goto exit;
	}

	mutex_lock(&me->smd_mutex);
	me->cvp_shutdown = STATUS_OK;
	me->cdsp_state = STATUS_OK;
	mutex_unlock(&me->smd_mutex);

exit:
	return err;
}

int cvp_dsp_suspend(uint32_t session_flag)
{
	int err = 0;
	struct cvp_dsp_cmd_msg local_cmd_msg;
	struct cvp_dsp_apps *me = &gfa_cv;
	uint32_t cdsp_state;

	mutex_lock(&me->smd_mutex);
	cdsp_state = me->cdsp_state;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR)
		return 0;

	local_cmd_msg.cmd_msg_type = CVP_DSP_SUSPEND;
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvp_dsp_cmd_msg));
	if (err != 0)
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);

	return err;
}

int cvp_dsp_resume(uint32_t session_flag)
{
	int err;
	struct cvp_dsp_cmd_msg local_cmd_msg;
	struct cvp_dsp_apps *me = &gfa_cv;
	uint32_t cdsp_state;

	mutex_lock(&me->smd_mutex);
	cdsp_state = me->cdsp_state;
	mutex_unlock(&me->smd_mutex);

	if (cdsp_state == STATUS_SSR)
		return 0;

	local_cmd_msg.cmd_msg_type = CVP_DSP_RESUME;
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvp_dsp_cmd_msg));
	if (err != 0)
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);

	return err;
}

void cvp_dsp_set_cvp_ssr(void)
{
	struct cvp_dsp_apps *me = &gfa_cv;

	mutex_lock(&me->smd_mutex);
	me->cvp_shutdown = STATUS_SSR;
	mutex_unlock(&me->smd_mutex);
}

int cvp_dsp_shutdown(uint32_t session_flag)
{
	struct msm_cvp_core *core;
	struct cvp_dsp_apps *me = &gfa_cv;
	int err, local_cmd_msg_rsp, timeout;
	struct cvp_dsp_cmd_msg local_cmd_msg;
	int srcVM[DEST_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
	int destVM[SRC_VM_NUM] = {VMID_HLOS};
	int destVMperm[SRC_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC };

	local_cmd_msg.cmd_msg_type = CVP_DSP_SHUTDOWN;
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvp_dsp_cmd_msg));
	if (err != 0)
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	timeout = msecs_to_jiffies(core->resources.msm_cvp_dsp_rsp_timeout);
	err = wait_for_completion_timeout(&me->shutdown_work, timeout);
	if (!err) {
		dprintk(CVP_ERR, "failed to shutdown dsp\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&me->smd_mutex);
	local_cmd_msg.msg_ptr = cmd_msg.msg_ptr;
	local_cmd_msg.msg_ptr_len = cmd_msg.msg_ptr_len;
	mutex_unlock(&me->smd_mutex);
	local_cmd_msg_rsp = cmd_msg_rsp.ret_val;
	if (local_cmd_msg_rsp == 0) {
		err = hyp_assign_phys((uint64_t)local_cmd_msg.msg_ptr,
			local_cmd_msg.msg_ptr_len, srcVM, DEST_VM_NUM,
			destVM,	destVMperm, SRC_VM_NUM);
		if (err) {
			dprintk(CVP_ERR,
				"%s: Failed to hyp_assign. err=%d\n",
				__func__, err);
			return err;
		}
	} else {
		dprintk(CVP_ERR,
			"%s: Skipping hyp_assign as CDSP sent invalid response=%d\n",
			__func__, local_cmd_msg_rsp);
	}

	return err;
}

int cvp_dsp_register_buffer(uint32_t session_id, uint32_t buff_fd,
			uint32_t buff_fd_size, uint32_t buff_size,
			uint32_t buff_offset, uint32_t buff_index,
			uint32_t buff_fd_iova)
{
	struct cvp_dsp_cmd_msg local_cmd_msg;
	int err;
	struct cvp_dsp_apps *me = &gfa_cv;

	local_cmd_msg.cmd_msg_type = CVP_DSP_REGISTER_BUFFER;
	local_cmd_msg.session_id = session_id;
	local_cmd_msg.buff_fd = buff_fd;
	local_cmd_msg.buff_fd_size = buff_fd_size;
	local_cmd_msg.buff_size = buff_size;
	local_cmd_msg.buff_offset = buff_offset;
	local_cmd_msg.buff_index = buff_index;
	local_cmd_msg.buff_fd_iova = buff_fd_iova;

	dprintk(CVP_DBG,
		"%s: cmd_msg_type=0x%x, buff_fd_iova=0x%x buff_index=0x%x\n",
		__func__, local_cmd_msg.cmd_msg_type, buff_fd_iova,
		local_cmd_msg.buff_index);
	dprintk(CVP_DBG,
		"%s: buff_size=0x%x session_id=0x%x\n",
		__func__, local_cmd_msg.buff_size, local_cmd_msg.session_id);

	mutex_lock(&me->reg_buffer_mutex);
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvp_dsp_cmd_msg));
	if (err != 0) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);
		mutex_unlock(&me->reg_buffer_mutex);
		return err;
	}

	dprintk(CVP_DBG,
		"%s: calling wait_for_completion work=%pK\n",
		__func__, &me->reg_buffer_work);
	wait_for_completion(&me->reg_buffer_work);
	mutex_unlock(&me->reg_buffer_mutex);
	dprintk(CVP_DBG,
			"%s: done calling wait_for_completion\n", __func__);

	return err;
}

int cvp_dsp_deregister_buffer(uint32_t session_id, uint32_t buff_fd,
			uint32_t buff_fd_size, uint32_t buff_size,
			uint32_t buff_offset, uint32_t buff_index,
			uint32_t buff_fd_iova)
{
	struct cvp_dsp_cmd_msg local_cmd_msg;
	int err;
	struct cvp_dsp_apps *me = &gfa_cv;

	local_cmd_msg.cmd_msg_type = CVP_DSP_DEREGISTER_BUFFER;
	local_cmd_msg.session_id = session_id;
	local_cmd_msg.buff_fd = buff_fd;
	local_cmd_msg.buff_fd_size = buff_fd_size;
	local_cmd_msg.buff_size = buff_size;
	local_cmd_msg.buff_offset = buff_offset;
	local_cmd_msg.buff_index = buff_index;
	local_cmd_msg.buff_fd_iova = buff_fd_iova;

	dprintk(CVP_DBG,
		"%s: cmd_msg_type=0x%x, buff_fd_iova=0x%x buff_index=0x%x\n",
		__func__, local_cmd_msg.cmd_msg_type, buff_fd_iova,
		local_cmd_msg.buff_index);
	dprintk(CVP_DBG,
			"%s: buff_size=0x%x session_id=0x%x\n",
		__func__, local_cmd_msg.buff_size, local_cmd_msg.session_id);

	mutex_lock(&me->dereg_buffer_mutex);
	err = cvp_dsp_send_cmd
			 (&local_cmd_msg, sizeof(struct cvp_dsp_cmd_msg));
	if (err != 0) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed with err=%d\n",
			__func__, err);
		mutex_unlock(&me->dereg_buffer_mutex);
		return err;
	}

	dprintk(CVP_DBG,
			"%s: calling wait_for_completion work=%pK\n",
			__func__, &me->dereg_buffer_work);
	wait_for_completion(&me->dereg_buffer_work);
	dprintk(CVP_DBG,
			"%s: done calling wait_for_completion\n", __func__);
	mutex_unlock(&me->dereg_buffer_mutex);

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

	mutex_init(&me->smd_mutex);
	mutex_init(&me->reg_buffer_mutex);
	mutex_init(&me->dereg_buffer_mutex);
	init_completion(&me->shutdown_work);
	init_completion(&me->reg_buffer_work);
	init_completion(&me->dereg_buffer_work);
	init_completion(&me->cmdqueue_send_work);
	me->cvp_shutdown = STATUS_INIT;
	me->cdsp_state = STATUS_INIT;
	INIT_WORK(&me->ssr_work, msm_cvp_cdsp_ssr_handler);
	err = register_rpmsg_driver(&cvp_dsp_rpmsg_client);
	if (err) {
		dprintk(CVP_ERR,
			"%s : register_rpmsg_driver failed with err %d\n",
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
	mutex_destroy(&me->reg_buffer_mutex);
	mutex_destroy(&me->dereg_buffer_mutex);
	if (me->rpmsg_register == 1)
		unregister_rpmsg_driver(&cvp_dsp_rpmsg_client);
}

late_initcall(cvp_dsp_device_init);
module_exit(cvp_dsp_device_exit);

MODULE_LICENSE("GPL v2");
