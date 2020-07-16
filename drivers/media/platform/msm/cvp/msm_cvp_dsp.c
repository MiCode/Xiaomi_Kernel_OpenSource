// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <soc/qcom/secure_buffer.h>
#include "msm_cvp_dsp.h"
#include "msm_cvp_internal.h"

struct cvp_dsp_apps gfa_cv;
static int hlosVM[HLOS_VM_NUM] = {VMID_HLOS};
static int dspVM[DSP_VM_NUM] = {VMID_HLOS, VMID_CDSP_Q6};
static int dspVMperm[DSP_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC,
				PERM_READ | PERM_WRITE | PERM_EXEC };
static int hlosVMperm[HLOS_VM_NUM] = { PERM_READ | PERM_WRITE | PERM_EXEC };

static int cvp_dsp_send_cmd(struct cvp_dsp_cmd_msg *cmd, uint32_t len)
{
	int rc = 0;
	struct cvp_dsp_apps *me = &gfa_cv;

	dprintk(CVP_DSP, "%s: cmd = %d\n", __func__, cmd->type);

	if (IS_ERR_OR_NULL(me->chan)) {
		dprintk(CVP_ERR, "%s: DSP GLink is not ready\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
	rc = rpmsg_send(me->chan->ept, cmd, len);
	if (rc) {
		dprintk(CVP_ERR, "%s: DSP rpmsg_send failed rc=%d\n",
			__func__, rc);
		goto exit;
	}

exit:
	return rc;
}

static int cvp_dsp_send_cmd_sync(struct cvp_dsp_cmd_msg *cmd, uint32_t len)
{
	int rc = 0;
	struct cvp_dsp_apps *me = &gfa_cv;

	dprintk(CVP_DSP, "%s: cmd = %d\n", __func__, cmd->type);

	rc = cvp_dsp_send_cmd(cmd, len);
	if (rc) {
		dprintk(CVP_ERR, "%s: cvp_dsp_send_cmd failed rc=%d\n",
			__func__, rc);
		goto exit;
	}

	if (!wait_for_completion_timeout(&me->completions[cmd->type],
			msecs_to_jiffies(CVP_DSP_RESPONSE_TIMEOUT))) {
		dprintk(CVP_ERR, "%s cmd %d timeout\n", __func__, cmd->type);
		rc = -ETIMEDOUT;
		goto exit;
	}

	me->pending_dsp2cpu_rsp.type = CVP_INVALID_RPMSG_TYPE;

exit:
	return rc;
}

static int cvp_dsp_send_cmd_hfi_queue(phys_addr_t *phys_addr,
					uint32_t size_in_bytes)
{
	int rc = 0;
	struct cvp_dsp_cmd_msg cmd;

	cmd.type = CPU2DSP_SEND_HFI_QUEUE;
	cmd.msg_ptr = (uint64_t)phys_addr;
	cmd.msg_ptr_len = size_in_bytes;
	cmd.ddr_type = of_fdt_get_ddrtype();
	if (cmd.ddr_type < 0) {
		dprintk(CVP_ERR,
			"%s: Incorrect DDR type value %d\n",
			__func__, cmd.ddr_type);
		return -EINVAL;
	}

	dprintk(CVP_DSP,
		"%s: address of buffer, PA=0x%pK  size_buff=%d ddr_type=%d\n",
		__func__, phys_addr, size_in_bytes, cmd.ddr_type);

	rc = cvp_dsp_send_cmd_sync(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed rc = %d\n",
			__func__, rc);
		goto exit;
	}
exit:
	return rc;
}

static int cvp_hyp_assign_to_dsp(uint64_t addr, uint32_t size)
{
	int rc = 0;
	struct cvp_dsp_apps *me = &gfa_cv;

	if (!me->hyp_assigned) {
		rc = hyp_assign_phys(addr, size, hlosVM, HLOS_VM_NUM, dspVM,
			dspVMperm, DSP_VM_NUM);
		if (rc) {
			dprintk(CVP_ERR, "%s failed. rc=%d\n", __func__, rc);
			return rc;
		}
		me->addr = addr;
		me->size = size;
		me->hyp_assigned = true;
	}

	return rc;
}

static int cvp_hyp_assign_from_dsp(void)
{
	int rc = 0;
	struct cvp_dsp_apps *me = &gfa_cv;

	if (me->hyp_assigned) {
		rc = hyp_assign_phys(me->addr, me->size, dspVM, DSP_VM_NUM,
				hlosVM, hlosVMperm, HLOS_VM_NUM);
		if (rc) {
			dprintk(CVP_ERR, "%s failed. rc=%d\n", __func__, rc);
			return rc;
		}
		me->addr = 0;
		me->size = 0;
		me->hyp_assigned = false;
	}

	return rc;
}

static int cvp_dsp_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct cvp_dsp_apps *me = &gfa_cv;

	if (strcmp(rpdev->dev.parent->of_node->name, "cdsp")) {
		dprintk(CVP_ERR,
			"%s: Failed to probe rpmsg device.Node name:%s\n",
			__func__, rpdev->dev.parent->of_node->name);
		return -EINVAL;
	}

	mutex_lock(&me->lock);
	me->chan = rpdev;
	me->state = DSP_PROBED;
	mutex_unlock(&me->lock);

	cvp_dsp_send_hfi_queue();

	return 0;
}

static void cvp_dsp_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct cvp_dsp_apps *me = &gfa_cv;

	dprintk(CVP_WARN, "%s: CDSP SSR triggered\n", __func__);

	mutex_lock(&me->lock);
	cvp_hyp_assign_from_dsp();

	me->chan = NULL;
	me->state = DSP_UNINIT;
	mutex_unlock(&me->lock);
	/* kernel driver needs clean all dsp sessions */

}

static int cvp_dsp_rpmsg_callback(struct rpmsg_device *rpdev,
	void *data, int len, void *priv, u32 addr)
{
	struct cvp_dsp_rsp_msg *rsp = (struct cvp_dsp_rsp_msg *)data;
	struct cvp_dsp_apps *me = &gfa_cv;

	dprintk(CVP_DSP, "%s: type = 0x%x ret = 0x%x\n",
		__func__, rsp->type, rsp->ret);

	if (rsp->type == CPU2DSP_SUSPEND ||
		rsp->type == CPU2DSP_RESUME) {
		me->pending_dsp2cpu_rsp.type = CVP_INVALID_RPMSG_TYPE;
		return 0;
	}

	if (me->pending_dsp2cpu_rsp.type != CVP_INVALID_RPMSG_TYPE ||
		me->pending_dsp2cpu_cmd.type != CVP_INVALID_RPMSG_TYPE)
		goto exit;

	if (rsp->type < CPU2DSP_MAX_CMD) {
		memcpy(&me->pending_dsp2cpu_rsp, rsp,
			sizeof(struct cvp_dsp_rsp_msg));
		complete(&me->completions[rsp->type]);
	} else if (rsp->type < CVP_DSP_MAX_CMD) {
		memcpy(&me->pending_dsp2cpu_cmd, rsp,
			sizeof(struct cvp_dsp2cpu_cmd_msg));
		complete(&me->completions[CPU2DSP_MAX_CMD]);
	} else {
		dprintk(CVP_ERR, "%s: Invalid type: %d\n", __func__, rsp->type);
		return 0;
	}

	return 0;
exit:
	dprintk(CVP_ERR, "concurrent dsp cmd type = %d, rsp type = %d\n",
			me->pending_dsp2cpu_cmd.type,
			me->pending_dsp2cpu_rsp.type);
	return 0;
}

int cvp_dsp_suspend(uint32_t session_flag)
{
	int rc = 0;
	struct cvp_dsp_cmd_msg cmd;
	struct cvp_dsp_apps *me = &gfa_cv;

	cmd.type = CPU2DSP_SUSPEND;

	mutex_lock(&me->lock);
	if (me->state != DSP_READY)
		goto exit;

	/* Use cvp_dsp_send_cmd_sync after dsp driver is ready */
	rc = cvp_dsp_send_cmd(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed rc = %d\n",
			__func__, rc);
		me->state = DSP_UNINIT;
		goto exit;
	}

	me->state = DSP_SUSPEND;

exit:
	mutex_unlock(&me->lock);
	return rc;
}

int cvp_dsp_resume(uint32_t session_flag)
{
	int rc = 0;
	struct cvp_dsp_cmd_msg cmd;
	struct cvp_dsp_apps *me = &gfa_cv;

	cmd.type = CPU2DSP_RESUME;

	mutex_lock(&me->lock);
	if (me->state != DSP_SUSPEND)
		goto exit;

	/* Use cvp_dsp_send_cmd_sync after dsp driver is ready */
	rc = cvp_dsp_send_cmd(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed rc = %d\n",
			__func__, rc);
		me->state = DSP_UNINIT;
		goto exit;
	}

	me->state = DSP_READY;

exit:
	mutex_unlock(&me->lock);
	return rc;
}

int cvp_dsp_shutdown(uint32_t session_flag)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	int rc = 0;
	struct cvp_dsp_cmd_msg cmd;

	cmd.type = CPU2DSP_SHUTDOWN;

	mutex_lock(&me->lock);
	if (me->state == DSP_INVALID)
		goto exit;

	me->state = DSP_UNINIT;
	rc = cvp_dsp_send_cmd_sync(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc) {
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed with rc = %d\n",
			__func__, rc);
		goto exit;
	}

	rc = cvp_hyp_assign_from_dsp();

exit:
	mutex_unlock(&me->lock);
	return rc;
}

int cvp_dsp_register_buffer(uint32_t session_id, uint32_t buff_fd,
			uint32_t buff_fd_size, uint32_t buff_size,
			uint32_t buff_offset, uint32_t buff_index,
			uint32_t buff_fd_iova)
{
	struct cvp_dsp_cmd_msg cmd;
	int rc;
	struct cvp_dsp_apps *me = &gfa_cv;

	cmd.type = CPU2DSP_REGISTER_BUFFER;
	cmd.session_id = session_id;
	cmd.buff_fd = buff_fd;
	cmd.buff_fd_size = buff_fd_size;
	cmd.buff_size = buff_size;
	cmd.buff_offset = buff_offset;
	cmd.buff_index = buff_index;
	cmd.buff_fd_iova = buff_fd_iova;

	dprintk(CVP_DSP,
		"%s: type=0x%x, buff_fd_iova=0x%x buff_index=0x%x\n",
		__func__, cmd.type, buff_fd_iova,
		cmd.buff_index);
	dprintk(CVP_DSP, "%s: buff_size=0x%x session_id=0x%x\n",
		__func__, cmd.buff_size, cmd.session_id);

	mutex_lock(&me->lock);
	rc = cvp_dsp_send_cmd_sync(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc) {
		dprintk(CVP_ERR, "%s send failed rc = %d\n", __func__, rc);
		me->state = DSP_UNINIT;
		goto exit;
	}

exit:
	mutex_unlock(&me->lock);
	return rc;
}

int cvp_dsp_deregister_buffer(uint32_t session_id, uint32_t buff_fd,
			uint32_t buff_fd_size, uint32_t buff_size,
			uint32_t buff_offset, uint32_t buff_index,
			uint32_t buff_fd_iova)
{
	struct cvp_dsp_cmd_msg cmd;
	int rc;
	struct cvp_dsp_apps *me = &gfa_cv;

	cmd.type = CPU2DSP_DEREGISTER_BUFFER;
	cmd.session_id = session_id;
	cmd.buff_fd = buff_fd;
	cmd.buff_fd_size = buff_fd_size;
	cmd.buff_size = buff_size;
	cmd.buff_offset = buff_offset;
	cmd.buff_index = buff_index;
	cmd.buff_fd_iova = buff_fd_iova;

	dprintk(CVP_DSP,
		"%s: type=0x%x, buff_fd_iova=0x%x buff_index=0x%x\n",
		__func__, cmd.type, buff_fd_iova,
		cmd.buff_index);
	dprintk(CVP_DSP, "%s: buff_size=0x%x session_id=0x%x\n",
		__func__, cmd.buff_size, cmd.session_id);

	mutex_lock(&me->lock);
	rc = cvp_dsp_send_cmd_sync(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc) {
		dprintk(CVP_ERR, "%s send failed rc = %d\n", __func__, rc);
		me->state = DSP_UNINIT;
		goto exit;
	}

exit:
	mutex_unlock(&me->lock);
	return rc;
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

void cvp_dsp_send_hfi_queue(void)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *device;
	struct cvp_dsp_apps *me = &gfa_cv;
	uint64_t addr;
	uint32_t size;
	int rc;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	if (core && core->device)
		device = core->device->hfi_device_data;
	else
		return;

	if (!device) {
		dprintk(CVP_ERR, "%s: NULL device\n", __func__);
		return;
	}

	dprintk(CVP_DSP, "Entering %s\n", __func__);

	mutex_lock(&device->lock);
	mutex_lock(&me->lock);

	addr = (uint64_t)device->dsp_iface_q_table.mem_data.dma_handle;
	size = device->dsp_iface_q_table.mem_data.size;

	if (!addr || !size) {
		dprintk(CVP_DSP, "%s: HFI queue is not ready\n", __func__);
		goto exit;
	}

	if (me->state != DSP_PROBED)
		goto exit;

	rc = cvp_hyp_assign_to_dsp(addr, size);
	if (rc) {
		dprintk(CVP_ERR, "%s: cvp_hyp_assign_to_dsp. rc=%d\n",
			__func__, rc);
		goto exit;
	}

	rc = cvp_dsp_send_cmd_hfi_queue((phys_addr_t *)addr, size);
	if (rc) {
		dprintk(CVP_WARN, "%s: Send HFI Queue failed rc = %d\n",
			__func__, rc);

		rc = cvp_hyp_assign_from_dsp();
		goto exit;
	}

	dprintk(CVP_DSP, "%s: dsp initialized\n", __func__);
	me->state = DSP_READY;

exit:
	mutex_unlock(&me->lock);
	mutex_unlock(&device->lock);
}

static int cvp_dsp_thread(void *data)
{
	int rc = 0, old_state;
	struct cvp_dsp_apps *me = &gfa_cv;
	struct cvp_dsp_cmd_msg cmd;
	struct cvp_hfi_device *hdev;
	struct msm_cvp_core *core;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	if (!core) {
		dprintk(CVP_ERR, "%s: Failed to find core\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	hdev = (struct cvp_hfi_device *)core->device;
	if (!hdev) {
		dprintk(CVP_ERR, "%s Invalid device handle\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

wait_dsp:
	rc = wait_for_completion_interruptible(
			&me->completions[CPU2DSP_MAX_CMD]);

	if (me->state == DSP_INVALID)
		goto exit;

	cmd.type = me->pending_dsp2cpu_cmd.type;

	if (rc == -ERESTARTSYS) {
		dprintk(CVP_WARN, "%s received interrupt signal\n", __func__);
	} else {
		mutex_lock(&me->lock);
		switch (me->pending_dsp2cpu_cmd.type) {
		case DSP2CPU_POWERON:
		{
			if (me->state == DSP_READY)
				break;

			mutex_unlock(&me->lock);
			old_state = me->state;
			me->state = DSP_READY;
			rc = call_hfi_op(hdev, resume, hdev->hfi_device_data);
			if (rc) {
				dprintk(CVP_WARN, "%s Failed to resume cvp\n",
						__func__);
				mutex_lock(&me->lock);
				me->state = old_state;
				cmd.ret = 1;
				break;
			}
			mutex_lock(&me->lock);
			cmd.ret = 0;
			break;
		}
		case DSP2CPU_POWEROFF:
		{
			me->state = DSP_SUSPEND;
			cmd.ret = 0;
			break;
		}
		default:
			dprintk(CVP_ERR, "unrecognaized dsp cmds: %d\n",
					me->pending_dsp2cpu_cmd.type);
			break;
		}
		me->pending_dsp2cpu_cmd.type = CVP_INVALID_RPMSG_TYPE;
		mutex_unlock(&me->lock);
	}
	/* Responds to DSP */
	rc = cvp_dsp_send_cmd(&cmd, sizeof(struct cvp_dsp_cmd_msg));
	if (rc)
		dprintk(CVP_ERR,
			"%s: cvp_dsp_send_cmd failed rc = %d cmd type=%d\n",
			__func__, rc, cmd.type);
	goto wait_dsp;
exit:
	dprintk(CVP_DBG, "dsp thread exit\n");
	do_exit(rc);
}

int cvp_dsp_device_init(void)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	char tname[16];
	int rc;
	int i;

	mutex_init(&me->lock);
	me->state = DSP_INVALID;
	me->hyp_assigned = false;

	for (i = 0; i <= CPU2DSP_MAX_CMD; i++)
		init_completion(&me->completions[i]);

	me->pending_dsp2cpu_cmd.type = CVP_INVALID_RPMSG_TYPE;
	me->pending_dsp2cpu_rsp.type = CVP_INVALID_RPMSG_TYPE;

	rc = register_rpmsg_driver(&cvp_dsp_rpmsg_client);
	if (rc) {
		dprintk(CVP_ERR,
			"%s : register_rpmsg_driver failed rc = %d\n",
			__func__, rc);
		goto register_bail;
	}
	snprintf(tname, sizeof(tname), "cvp-dsp-thread");
	me->dsp_thread = kthread_run(cvp_dsp_thread, me, tname);
	if (!me->dsp_thread) {
		dprintk(CVP_ERR, "%s create %s fail", __func__, tname);
		rc = -ECHILD;
		goto register_bail;
	}
	me->state = DSP_UNINIT;
	return 0;

register_bail:
	return rc;
}

void cvp_dsp_device_exit(void)
{
	struct cvp_dsp_apps *me = &gfa_cv;
	int i;

	mutex_lock(&me->lock);
	me->state = DSP_INVALID;
	mutex_unlock(&me->lock);

	for (i = 0; i <= CPU2DSP_MAX_CMD; i++)
		complete_all(&me->completions[i]);

	mutex_destroy(&me->lock);
	unregister_rpmsg_driver(&cvp_dsp_rpmsg_client);
}
