/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_isp.h>

#include "msm.h"
#include "msm_vfe40.h"

static int msm_axi_subdev_s_crystal_freq(struct v4l2_subdev *sd,
						u32 freq, u32 flags)
{
	int rc = 0;
	int round_rate;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);

	round_rate = clk_round_rate(axi_ctrl->vfe_clk[0], freq);
	if (rc < 0) {
		pr_err("%s: clk_round_rate failed %d\n",
					__func__, rc);
		return rc;
	}

	axi_ctrl->share_ctrl->vfe_clk_rate = round_rate;
	rc = clk_set_rate(axi_ctrl->vfe_clk[0], round_rate);
	if (rc < 0)
		pr_err("%s: clk_set_rate failed %d\n",
					__func__, rc);

	return rc;
}

void axi_start(struct axi_ctrl_t *axi_ctrl)
{
	switch (axi_ctrl->share_ctrl->operation_mode) {
	case VFE_OUTPUTS_PREVIEW:
	case VFE_OUTPUTS_PREVIEW_AND_VIDEO:
		if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_PRIMARY) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch1]);
		} else if (axi_ctrl->share_ctrl->outpath.output_mode &
				VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch1]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out0.ch2]);
		}
		break;
	default:
		if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch1]);
		} else if (axi_ctrl->share_ctrl->outpath.output_mode &
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS) {
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch0]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch1]);
			msm_camera_io_w(1, axi_ctrl->share_ctrl->vfebase +
				vfe40_AXI_WM_CFG[axi_ctrl->
				share_ctrl->outpath.out1.ch2]);
		}
		break;
	}
}

void axi_stop(struct axi_ctrl_t *axi_ctrl)
{
	uint8_t  axiBusyFlag = true;
	/* axi halt command. */
	msm_camera_io_w(AXI_HALT,
		axi_ctrl->share_ctrl->vfebase + VFE_AXI_CMD);
	wmb();
	while (axiBusyFlag) {
		if (msm_camera_io_r(
			axi_ctrl->share_ctrl->vfebase + VFE_AXI_STATUS) & 0x1)
			axiBusyFlag = false;
	}
	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(AXI_HALT_CLEAR,
		axi_ctrl->share_ctrl->vfebase + VFE_AXI_CMD);

	/* after axi halt, then ok to apply global reset. */
	/* enable reset_ack and async timer interrupt only while
	stopping the pipeline.*/
	msm_camera_io_w(0xf0000000,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_0);
	msm_camera_io_w(VFE_IMASK_WHILE_STOPPING_1,
		axi_ctrl->share_ctrl->vfebase + VFE_IRQ_MASK_1);

	/* Ensure the write order while writing
	to the command register using the barrier */
	msm_camera_io_w_mb(VFE_RESET_UPON_STOP_CMD,
		axi_ctrl->share_ctrl->vfebase + VFE_GLOBAL_RESET);
}

static int vfe40_config_axi(
	struct axi_ctrl_t *axi_ctrl, int mode, uint32_t *ao)
{
	uint32_t *ch_info;
	uint32_t *axi_cfg = ao;

	/* Update the corresponding write masters for each output*/
	ch_info = axi_cfg + V40_AXI_CFG_LEN;
	axi_ctrl->share_ctrl->outpath.out0.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out0.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out0.ch2 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out0.image_mode =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out1.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out1.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out1.ch2 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out1.image_mode =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out2.ch0 = 0x0000FFFF & *ch_info;
	axi_ctrl->share_ctrl->outpath.out2.ch1 =
		0x0000FFFF & (*ch_info++ >> 16);
	axi_ctrl->share_ctrl->outpath.out2.ch2 = 0x0000FFFF & *ch_info++;

	switch (mode) {
	case OUTPUT_PRIM:
		axi_ctrl->share_ctrl->outpath.output_mode =
			VFE40_OUTPUT_MODE_PRIMARY;
		break;
	case OUTPUT_PRIM_ALL_CHNLS:
		axi_ctrl->share_ctrl->outpath.output_mode =
			VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC:
		axi_ctrl->share_ctrl->outpath.output_mode =
			VFE40_OUTPUT_MODE_PRIMARY;
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_SECONDARY;
		break;
	case OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS:
		axi_ctrl->share_ctrl->outpath.output_mode =
			VFE40_OUTPUT_MODE_PRIMARY;
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS;
		break;
	case OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC:
		axi_ctrl->share_ctrl->outpath.output_mode =
			VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS;
		axi_ctrl->share_ctrl->outpath.output_mode |=
			VFE40_OUTPUT_MODE_SECONDARY;
		break;
	default:
		pr_err("%s Invalid AXI mode %d ", __func__, mode);
		return -EINVAL;
	}
	msm_camera_io_w(*ao, axi_ctrl->share_ctrl->vfebase +
		VFE_BUS_IO_FORMAT_CFG);
	msm_camera_io_memcpy(axi_ctrl->share_ctrl->vfebase +
		vfe40_cmd[VFE_CMD_AXI_OUT_CFG].offset, axi_cfg,
		vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length - V40_AXI_CH_INF_LEN);
	msm_camera_io_w(*ch_info++,
		axi_ctrl->share_ctrl->vfebase + VFE_RDI0_CFG);
	if (msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
		V40_GET_HW_VERSION_OFF) ==
		VFE40_HW_NUMBER) {
		msm_camera_io_w(*ch_info++,
			axi_ctrl->share_ctrl->vfebase + VFE_RDI1_CFG);
		msm_camera_io_w(*ch_info++,
			axi_ctrl->share_ctrl->vfebase + VFE_RDI2_CFG);
	}
	return 0;
}

static int msm_axi_config(struct v4l2_subdev *sd, void __user *arg)
{
	struct msm_vfe_cfg_cmd cfgcmd;
	struct msm_isp_cmd vfecmd;
	int rc = 0;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);

	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return -EFAULT;
	}
	if (NULL != arg) {
		if (copy_from_user(&cfgcmd, arg, sizeof(cfgcmd))) {
			ERR_COPY_FROM_USER();
			return -EFAULT;
		}
	}
	if (NULL != cfgcmd.value) {
		if (copy_from_user(&vfecmd,
				(void __user *)(cfgcmd.value),
				sizeof(vfecmd))) {
			pr_err("%s %d: copy_from_user failed\n", __func__,
				__LINE__);
			return -EFAULT;
		}
	}

	switch (cfgcmd.cmd_type) {
	case CMD_AXI_CFG_PRIM: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl, OUTPUT_PRIM, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM_ALL_CHNLS: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl, OUTPUT_PRIM_ALL_CHNLS, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl, OUTPUT_PRIM|OUTPUT_SEC, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM|CMD_AXI_CFG_SEC_ALL_CHNLS: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl,
			OUTPUT_PRIM|OUTPUT_SEC_ALL_CHNLS, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC: {
		uint32_t *axio = NULL;
		axio = kmalloc(vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length,
				GFP_ATOMIC);
		if (!axio) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(axio, (void __user *)(vfecmd.value),
				vfe40_cmd[VFE_CMD_AXI_OUT_CFG].length)) {
			kfree(axio);
			rc = -EFAULT;
			break;
		}
		vfe40_config_axi(axi_ctrl,
			OUTPUT_PRIM_ALL_CHNLS|OUTPUT_SEC, axio);
		kfree(axio);
	}
		break;
	case CMD_AXI_CFG_PRIM_ALL_CHNLS|CMD_AXI_CFG_SEC_ALL_CHNLS:
		pr_err("%s Invalid/Unsupported AXI configuration %x",
			__func__, cfgcmd.cmd_type);
		break;
	case CMD_AXI_START:
		axi_start(axi_ctrl);
		break;
	case CMD_AXI_STOP:
		axi_stop(axi_ctrl);
		break;
	case CMD_AXI_RESET:
		break;
	default:
		pr_err("%s Unsupported AXI configuration %x ", __func__,
			cfgcmd.cmd_type);
		break;
	}
	return rc;
}

static struct msm_free_buf *vfe40_check_free_buffer(
	int id, int path, struct axi_ctrl_t *axi_ctrl)
{
	struct vfe40_output_ch *outch = NULL;
	struct msm_free_buf *b = NULL;
	uint32_t image_mode = 0;

	if (path == VFE_MSG_OUTPUT_PRIMARY)
		image_mode = axi_ctrl->share_ctrl->outpath.out0.image_mode;
	else
		image_mode = axi_ctrl->share_ctrl->outpath.out1.image_mode;

	vfe40_subdev_notify(id, path, image_mode,
		&axi_ctrl->subdev, axi_ctrl->share_ctrl);
	outch = vfe40_get_ch(path, axi_ctrl->share_ctrl);
	if (outch->free_buf.ch_paddr[0])
		b = &outch->free_buf;
	return b;
}

static void vfe_send_outmsg(
	struct axi_ctrl_t *axi_ctrl, uint8_t msgid,
	uint32_t ch0_paddr, uint32_t ch1_paddr,
	uint32_t ch2_paddr, uint32_t image_mode)
{
	struct isp_msg_output msg;

	msg.output_id = msgid;
	msg.buf.image_mode = image_mode;
	msg.buf.ch_paddr[0]	= ch0_paddr;
	msg.buf.ch_paddr[1]	= ch1_paddr;
	msg.buf.ch_paddr[2]	= ch2_paddr;
	msg.frameCounter = axi_ctrl->share_ctrl->vfeFrameId;

	v4l2_subdev_notify(&axi_ctrl->subdev,
			NOTIFY_VFE_MSG_OUT,
			&msg);
	return;
}

static void vfe40_process_output_path_irq_0(
	struct axi_ctrl_t *axi_ctrl)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe40_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_PRIMARY, axi_ctrl);

	/* we render frames in the following conditions:
	1. Continuous mode and the free buffer is avaialable.
	2. In snapshot shot mode, free buffer is not always available.
	when pending snapshot count is <=1,  then no need to use
	free buffer.
	*/
	out_bool = (
		(axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_MAIN ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_RAW ||
		axi_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STARTED ||
		axi_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STOP_REQUESTED ||
		axi_ctrl->share_ctrl->liveshot_state ==
			VFE_STATE_STOPPED) &&
		(axi_ctrl->share_ctrl->vfe_capture_count <= 1)) ||
			free_buf;

	if (out_bool) {
		ping_pong = msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Channel 0*/
		ch0_paddr = vfe40_get_ch_addr(
			ping_pong, axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch0);
		/* Channel 1*/
		ch1_paddr = vfe40_get_ch_addr(
			ping_pong, axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch1);
		/* Channel 2*/
		ch2_paddr = vfe40_get_ch_addr(
			ping_pong, axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch2);

		CDBG("output path 0, ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out0.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe40_put_ch_addr(ping_pong,
					axi_ctrl->share_ctrl->vfebase,
					axi_ctrl->share_ctrl->outpath.out0.ch2,
					free_buf->ch_paddr[2]);
		}
		if (axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_JPEG ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			axi_ctrl->share_ctrl->liveshot_state ==
				VFE_STATE_STOPPED)
			axi_ctrl->share_ctrl->outpath.out0.capture_cnt--;

		vfe_send_outmsg(axi_ctrl,
			MSG_ID_OUTPUT_PRIMARY, ch0_paddr,
			ch1_paddr, ch2_paddr,
			axi_ctrl->share_ctrl->outpath.out0.image_mode);

		if (axi_ctrl->share_ctrl->liveshot_state == VFE_STATE_STOPPED)
			axi_ctrl->share_ctrl->liveshot_state = VFE_STATE_IDLE;

	} else {
		axi_ctrl->share_ctrl->outpath.out0.frame_drop_cnt++;
		CDBG("path_irq_0 - no free buffer!\n");
	}
}

static void vfe40_process_output_path_irq_1(
	struct axi_ctrl_t *axi_ctrl)
{
	uint32_t ping_pong;
	uint32_t ch0_paddr, ch1_paddr, ch2_paddr;
	/* this must be snapshot main image output. */
	uint8_t out_bool = 0;
	struct msm_free_buf *free_buf = NULL;

	free_buf = vfe40_check_free_buffer(VFE_MSG_OUTPUT_IRQ,
		VFE_MSG_OUTPUT_SECONDARY, axi_ctrl);
	out_bool = ((axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB) &&
			(axi_ctrl->share_ctrl->vfe_capture_count <= 1)) ||
				free_buf;

	if (out_bool) {
		ping_pong = msm_camera_io_r(axi_ctrl->share_ctrl->vfebase +
			VFE_BUS_PING_PONG_STATUS);

		/* Y channel */
		ch0_paddr = vfe40_get_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch0);
		/* Chroma channel */
		ch1_paddr = vfe40_get_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch1);
		ch2_paddr = vfe40_get_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch2);

		CDBG("%s ch0 = 0x%x, ch1 = 0x%x, ch2 = 0x%x\n",
			__func__, ch0_paddr, ch1_paddr, ch2_paddr);
		if (free_buf) {
			/* Y channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch0,
			free_buf->ch_paddr[0]);
			/* Chroma channel */
			vfe40_put_ch_addr(ping_pong,
			axi_ctrl->share_ctrl->vfebase,
			axi_ctrl->share_ctrl->outpath.out1.ch1,
			free_buf->ch_paddr[1]);
			if (free_buf->num_planes > 2)
				vfe40_put_ch_addr(ping_pong,
					axi_ctrl->share_ctrl->vfebase,
					axi_ctrl->share_ctrl->outpath.out1.ch2,
					free_buf->ch_paddr[2]);
		}
		if (axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_THUMB_AND_MAIN ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_MAIN_AND_THUMB ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_RAW ||
			axi_ctrl->share_ctrl->operation_mode ==
				VFE_OUTPUTS_JPEG_AND_THUMB)
			axi_ctrl->share_ctrl->outpath.out1.capture_cnt--;

		vfe_send_outmsg(axi_ctrl,
			MSG_ID_OUTPUT_SECONDARY, ch0_paddr,
			ch1_paddr, ch2_paddr,
			axi_ctrl->share_ctrl->outpath.out1.image_mode);

	} else {
		axi_ctrl->share_ctrl->outpath.out1.frame_drop_cnt++;
		CDBG("path_irq_1 - no free buffer!\n");
	}
}

static void msm_axi_process_irq(struct v4l2_subdev *sd, void *arg)
{
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	uint32_t irqstatus = (uint32_t) arg;

	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return;
	}
	/* next, check output path related interrupts. */
	if (irqstatus &
		VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK) {
		CDBG("Image composite done 0 irq occured.\n");
		vfe40_process_output_path_irq_0(axi_ctrl);
	}
	if (irqstatus &
		VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK) {
		CDBG("Image composite done 1 irq occured.\n");
		vfe40_process_output_path_irq_1(axi_ctrl);
	}
	/* in snapshot mode if done then send
	snapshot done message */
	if (axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_MAIN ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_MAIN_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_THUMB_AND_JPEG ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_JPEG_AND_THUMB ||
		axi_ctrl->share_ctrl->operation_mode ==
			VFE_OUTPUTS_RAW) {
		if ((axi_ctrl->share_ctrl->outpath.out0.capture_cnt == 0)
				&& (axi_ctrl->share_ctrl->outpath.out1.
				capture_cnt == 0)) {
			msm_camera_io_w_mb(
				CAMIF_COMMAND_STOP_IMMEDIATELY,
				axi_ctrl->share_ctrl->vfebase +
				VFE_CAMIF_COMMAND);
			vfe40_send_isp_msg(&axi_ctrl->subdev,
				axi_ctrl->share_ctrl->vfeFrameId,
				MSG_ID_SNAPSHOT_DONE);
		}
	}
}

static int msm_axi_buf_cfg(struct v4l2_subdev *sd, void __user *arg)
{
	struct msm_camvfe_params *vfe_params =
		(struct msm_camvfe_params *)arg;
	struct msm_vfe_cfg_cmd *cmd = vfe_params->vfe_cfg;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	void *data = vfe_params->data;
	int rc = 0;

	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return -EFAULT;
	}

	switch (cmd->cmd_type) {
	case CMD_CONFIG_PING_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe40_output_ch *outch =
			vfe40_get_ch(path, axi_ctrl->share_ctrl);
		outch->ping = *((struct msm_free_buf *)data);
	}
		break;

	case CMD_CONFIG_PONG_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe40_output_ch *outch =
			vfe40_get_ch(path, axi_ctrl->share_ctrl);
		outch->pong = *((struct msm_free_buf *)data);
	}
		break;

	case CMD_CONFIG_FREE_BUF_ADDR: {
		int path = *((int *)cmd->value);
		struct vfe40_output_ch *outch =
			vfe40_get_ch(path, axi_ctrl->share_ctrl);
		outch->free_buf = *((struct msm_free_buf *)data);
	}
		break;
	default:
		pr_err("%s Unsupported AXI Buf config %x ", __func__,
			cmd->cmd_type);
	}
	return rc;
};

static struct msm_cam_clk_info vfe40_clk_info[] = {
	{"vfe_clk_src", 266670000},
	{"camss_vfe_vfe_clk", -1},
	{"camss_csi_vfe_clk", -1},
	{"top_clk", -1},
	{"iface_clk", -1},
	{"bus_clk", -1},
};

int msm_axi_subdev_init(struct v4l2_subdev *sd,
			struct msm_cam_media_controller *mctl)
{
	int rc = 0;
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	v4l2_set_subdev_hostdata(sd, mctl);
	spin_lock_init(&axi_ctrl->tasklet_lock);
	INIT_LIST_HEAD(&axi_ctrl->tasklet_q);
	spin_lock_init(&axi_ctrl->share_ctrl->sd_notify_lock);

	axi_ctrl->share_ctrl->vfebase = ioremap(axi_ctrl->vfemem->start,
		resource_size(axi_ctrl->vfemem));
	if (!axi_ctrl->share_ctrl->vfebase) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto remap_failed;
	}

	if (axi_ctrl->fs_vfe == NULL) {
		axi_ctrl->fs_vfe =
			regulator_get(&axi_ctrl->pdev->dev, "vdd");
		if (IS_ERR(axi_ctrl->fs_vfe)) {
			pr_err("%s: Regulator FS_VFE get failed %ld\n",
				__func__, PTR_ERR(axi_ctrl->fs_vfe));
			axi_ctrl->fs_vfe = NULL;
			goto fs_failed;
		} else if (regulator_enable(axi_ctrl->fs_vfe)) {
			pr_err("%s: Regulator FS_VFE enable failed\n",
							__func__);
			regulator_put(axi_ctrl->fs_vfe);
			axi_ctrl->fs_vfe = NULL;
			goto fs_failed;
		}
	}
	rc = msm_cam_clk_enable(&axi_ctrl->pdev->dev, vfe40_clk_info,
			axi_ctrl->vfe_clk, ARRAY_SIZE(vfe40_clk_info), 1);
	if (rc < 0)
			goto clk_enable_failed;

	msm_camio_bus_scale_cfg(
		mctl->sdata->pdata->cam_bus_scale_table, S_INIT);
	msm_camio_bus_scale_cfg(
		mctl->sdata->pdata->cam_bus_scale_table, S_PREVIEW);

	axi_ctrl->share_ctrl->register_total = VFE40_REGISTER_TOTAL;

	enable_irq(axi_ctrl->vfeirq->start);

	return rc;
clk_enable_failed:
	regulator_disable(axi_ctrl->fs_vfe);
	regulator_put(axi_ctrl->fs_vfe);
	axi_ctrl->fs_vfe = NULL;
fs_failed:
	iounmap(axi_ctrl->share_ctrl->vfebase);
	axi_ctrl->share_ctrl->vfebase = NULL;
remap_failed:
	disable_irq(axi_ctrl->vfeirq->start);
	return rc;
}

void msm_axi_subdev_release(struct v4l2_subdev *sd)
{
	struct msm_cam_media_controller *pmctl =
		(struct msm_cam_media_controller *)v4l2_get_subdev_hostdata(sd);
	struct axi_ctrl_t *axi_ctrl = v4l2_get_subdevdata(sd);
	if (!axi_ctrl->share_ctrl->vfebase) {
		pr_err("%s: base address unmapped\n", __func__);
		return;
	}

	CDBG("%s, free_irq\n", __func__);
	disable_irq(axi_ctrl->vfeirq->start);
	tasklet_kill(&axi_ctrl->vfe40_tasklet);
	msm_cam_clk_enable(&axi_ctrl->pdev->dev, vfe40_clk_info,
		axi_ctrl->vfe_clk, ARRAY_SIZE(vfe40_clk_info), 0);

	if (axi_ctrl->fs_vfe) {
		regulator_disable(axi_ctrl->fs_vfe);
		regulator_put(axi_ctrl->fs_vfe);
		axi_ctrl->fs_vfe = NULL;
	}
	iounmap(axi_ctrl->share_ctrl->vfebase);
	axi_ctrl->share_ctrl->vfebase = NULL;

	if (atomic_read(&axi_ctrl->share_ctrl->irq_cnt))
		pr_warning("%s, Warning IRQ Count not ZERO\n", __func__);

	msm_camio_bus_scale_cfg(
		pmctl->sdata->pdata->cam_bus_scale_table, S_EXIT);
}

static long msm_axi_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = -ENOIOCTLCMD;
	switch (cmd) {
	case VIDIOC_MSM_AXI_INIT:
		rc = msm_axi_subdev_init(sd,
			(struct msm_cam_media_controller *)arg);
		break;
	case VIDIOC_MSM_AXI_CFG:
		rc = msm_axi_config(sd, arg);
		break;
	case VIDIOC_MSM_AXI_IRQ:
		msm_axi_process_irq(sd, arg);
		rc = 0;
		break;
	case VIDIOC_MSM_AXI_BUF_CFG:
		msm_axi_buf_cfg(sd, arg);
		rc = 0;
		break;
	case VIDIOC_MSM_AXI_RELEASE:
		msm_axi_subdev_release(sd);
		rc = 0;
		break;
	default:
		pr_err("%s: command not found\n", __func__);
	}
	return rc;
}

static const struct v4l2_subdev_core_ops msm_axi_subdev_core_ops = {
	.ioctl = msm_axi_subdev_ioctl,
};

static const struct v4l2_subdev_video_ops msm_axi_subdev_video_ops = {
	.s_crystal_freq = msm_axi_subdev_s_crystal_freq,
};

static const struct v4l2_subdev_ops msm_axi_subdev_ops = {
	.core = &msm_axi_subdev_core_ops,
	.video = &msm_axi_subdev_video_ops,
};

static const struct v4l2_subdev_internal_ops msm_axi_internal_ops;

void vfe40_axi_probe(struct axi_ctrl_t *axi_ctrl)
{
	struct msm_cam_subdev_info sd_info;
	v4l2_subdev_init(&axi_ctrl->subdev, &msm_axi_subdev_ops);
	axi_ctrl->subdev.internal_ops = &msm_axi_internal_ops;
	axi_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(axi_ctrl->subdev.name,
			 sizeof(axi_ctrl->subdev.name), "axi");
	v4l2_set_subdevdata(&axi_ctrl->subdev, axi_ctrl);

	sd_info.sdev_type = AXI_DEV;
	sd_info.sd_index = axi_ctrl->pdev->id;
	sd_info.irq_num = 0;
	msm_cam_register_subdev_node(&axi_ctrl->subdev, &sd_info);
}
