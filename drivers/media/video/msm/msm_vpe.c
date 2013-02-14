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
 *
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include <asm/div64.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include "msm.h"
#include "msm_vpe.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_vpe: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static int vpe_enable(uint32_t, struct msm_cam_media_controller *);
static int vpe_disable(struct msm_cam_media_controller *);
static int vpe_update_scaler(struct msm_pp_crop *pcrop);
struct vpe_ctrl_type *vpe_ctrl;
static atomic_t vpe_init_done = ATOMIC_INIT(0);

static int msm_vpe_do_pp(struct msm_mctl_pp_frame_info *pp_frame_info);

static long long vpe_do_div(long long num, long long den)
{
	do_div(num, den);
	return num;
}

static int vpe_start(void)
{
	/*  enable the frame irq, bit 0 = Display list 0 ROI done */
	msm_camera_io_w_mb(1, vpe_ctrl->vpebase + VPE_INTR_ENABLE_OFFSET);
	msm_camera_io_dump(vpe_ctrl->vpebase, 0x120);
	msm_camera_io_dump(vpe_ctrl->vpebase + 0x00400, 0x18);
	msm_camera_io_dump(vpe_ctrl->vpebase + 0x10000, 0x250);
	msm_camera_io_dump(vpe_ctrl->vpebase + 0x30000, 0x20);
	msm_camera_io_dump(vpe_ctrl->vpebase + 0x50000, 0x30);
	msm_camera_io_dump(vpe_ctrl->vpebase + 0x50400, 0x10);

	/* this triggers the operation. */
	msm_camera_io_w_mb(1, vpe_ctrl->vpebase + VPE_DL0_START_OFFSET);
	return 0;
}

void vpe_reset_state_variables(void)
{
	/* initialize local variables for state control, etc.*/
	vpe_ctrl->op_mode = 0;
	vpe_ctrl->state = VPE_STATE_INIT;
}

static void vpe_config_axi_default(void)
{
	msm_camera_io_w(0x25, vpe_ctrl->vpebase + VPE_AXI_ARB_2_OFFSET);
	D("%s: yaddr %ld cbcraddr %ld", __func__,
		 vpe_ctrl->out_y_addr, vpe_ctrl->out_cbcr_addr);
	if (!vpe_ctrl->out_y_addr || !vpe_ctrl->out_cbcr_addr)
		return;
	msm_camera_io_w(vpe_ctrl->out_y_addr,
		vpe_ctrl->vpebase + VPE_OUTP0_ADDR_OFFSET);
	/* for video  CbCr address */
	msm_camera_io_w(vpe_ctrl->out_cbcr_addr,
		vpe_ctrl->vpebase + VPE_OUTP1_ADDR_OFFSET);
}

static int vpe_reset(void)
{
	uint32_t vpe_version;
	uint32_t rc = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	if (vpe_ctrl->state == VPE_STATE_IDLE) {
		D("%s: VPE already disabled.", __func__);
		spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
		return rc;
	}
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);

	vpe_reset_state_variables();
	vpe_version = msm_camera_io_r(
			vpe_ctrl->vpebase + VPE_HW_VERSION_OFFSET);
	D("vpe_version = 0x%x\n", vpe_version);
	/* disable all interrupts.*/
	msm_camera_io_w(0, vpe_ctrl->vpebase + VPE_INTR_ENABLE_OFFSET);
	/* clear all pending interrupts*/
	msm_camera_io_w(0x1fffff, vpe_ctrl->vpebase + VPE_INTR_CLEAR_OFFSET);
	/* write sw_reset to reset the core. */
	msm_camera_io_w(0x10, vpe_ctrl->vpebase + VPE_SW_RESET_OFFSET);
	/* then poll the reset bit, it should be self-cleared. */
	while (1) {
		rc =
		msm_camera_io_r(vpe_ctrl->vpebase + VPE_SW_RESET_OFFSET) & 0x10;
		if (rc == 0)
			break;
	}
	/*  at this point, hardware is reset. Then pogram to default
		values. */
	msm_camera_io_w(VPE_AXI_RD_ARB_CONFIG_VALUE,
			vpe_ctrl->vpebase + VPE_AXI_RD_ARB_CONFIG_OFFSET);

	msm_camera_io_w(VPE_CGC_ENABLE_VALUE,
			vpe_ctrl->vpebase + VPE_CGC_EN_OFFSET);
	msm_camera_io_w(1, vpe_ctrl->vpebase + VPE_CMD_MODE_OFFSET);
	msm_camera_io_w(VPE_DEFAULT_OP_MODE_VALUE,
			vpe_ctrl->vpebase + VPE_OP_MODE_OFFSET);
	msm_camera_io_w(VPE_DEFAULT_SCALE_CONFIG,
			vpe_ctrl->vpebase + VPE_SCALE_CONFIG_OFFSET);
	vpe_config_axi_default();
	return rc;
}

static int msm_vpe_cfg_update(void *pinfo)
{
	uint32_t  rot_flag, rc = 0;
	struct msm_pp_crop *pcrop = (struct msm_pp_crop *)pinfo;

	rot_flag = msm_camera_io_r(vpe_ctrl->vpebase +
						VPE_OP_MODE_OFFSET) & 0xE00;
	if (pinfo != NULL) {
		D("%s: Crop info in2_w = %d, in2_h = %d "
			"out2_w = %d out2_h = %d\n",
			__func__, pcrop->src_w, pcrop->src_h,
			pcrop->dst_w, pcrop->dst_h);
		rc = vpe_update_scaler(pcrop);
	}
	D("return rc = %d rot_flag = %d\n", rc, rot_flag);
	rc |= rot_flag;

	return rc;
}

void vpe_update_scale_coef(uint32_t *p)
{
	uint32_t i, offset;
	offset = *p;
	for (i = offset; i < (VPE_SCALE_COEFF_NUM + offset); i++) {
		msm_camera_io_w(*(++p),
			vpe_ctrl->vpebase + VPE_SCALE_COEFF_LSBn(i));
		msm_camera_io_w(*(++p),
			vpe_ctrl->vpebase + VPE_SCALE_COEFF_MSBn(i));
	}
}

void vpe_input_plane_config(uint32_t *p)
{
	msm_camera_io_w(*p, vpe_ctrl->vpebase + VPE_SRC_FORMAT_OFFSET);
	msm_camera_io_w(*(++p),
		vpe_ctrl->vpebase + VPE_SRC_UNPACK_PATTERN1_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_SRC_IMAGE_SIZE_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_SRC_YSTRIDE1_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_SRC_SIZE_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_SRC_XY_OFFSET);
}

void vpe_output_plane_config(uint32_t *p)
{
	msm_camera_io_w(*p, vpe_ctrl->vpebase + VPE_OUT_FORMAT_OFFSET);
	msm_camera_io_w(*(++p),
		vpe_ctrl->vpebase + VPE_OUT_PACK_PATTERN1_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_OUT_YSTRIDE1_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_OUT_SIZE_OFFSET);
	msm_camera_io_w(*(++p), vpe_ctrl->vpebase + VPE_OUT_XY_OFFSET);
}

static int vpe_operation_config(uint32_t *p)
{
	uint32_t w, h, temp;
	msm_camera_io_w(*p, vpe_ctrl->vpebase + VPE_OP_MODE_OFFSET);

	temp = msm_camera_io_r(vpe_ctrl->vpebase + VPE_OUT_SIZE_OFFSET);
	w = temp & 0xFFF;
	h = (temp & 0xFFF0000) >> 16;
	if (*p++ & 0xE00) {
		/* rotation enabled. */
		vpe_ctrl->out_w = h;
		vpe_ctrl->out_h = w;
	} else {
		vpe_ctrl->out_w = w;
		vpe_ctrl->out_h = h;
	}
	D("%s: out_w=%d, out_h=%d", __func__, vpe_ctrl->out_w,
		vpe_ctrl->out_h);
	return 0;
}

/* Later we can separate the rotation and scaler calc. If
*  rotation is enabled, simply swap the destination dimension.
*  And then pass the already swapped output size to this
*  function. */
static int vpe_update_scaler(struct msm_pp_crop *pcrop)
{
	uint32_t out_ROI_width, out_ROI_height;
	uint32_t src_ROI_width, src_ROI_height;

	/*
	* phase_step_x, phase_step_y, phase_init_x and phase_init_y
	* are represented in fixed-point, unsigned 3.29 format
	*/
	uint32_t phase_step_x = 0;
	uint32_t phase_step_y = 0;
	uint32_t phase_init_x = 0;
	uint32_t phase_init_y = 0;

	uint32_t src_roi, src_x, src_y, src_xy, temp;
	uint32_t yscale_filter_sel, xscale_filter_sel;
	uint32_t scale_unit_sel_x, scale_unit_sel_y;
	uint64_t numerator, denominator;

	/* assumption is both direction need zoom. this can be
	improved. */
	temp =
		msm_camera_io_r(vpe_ctrl->vpebase + VPE_OP_MODE_OFFSET) | 0x3;
	msm_camera_io_w(temp, vpe_ctrl->vpebase + VPE_OP_MODE_OFFSET);

	src_ROI_width = pcrop->src_w;
	src_ROI_height = pcrop->src_h;
	out_ROI_width = pcrop->dst_w;
	out_ROI_height = pcrop->dst_h;

	D("src w = 0x%x, h=0x%x, dst w = 0x%x, h =0x%x.\n",
		src_ROI_width, src_ROI_height, out_ROI_width,
		out_ROI_height);
	src_roi = (src_ROI_height << 16) + src_ROI_width;

	msm_camera_io_w(src_roi, vpe_ctrl->vpebase + VPE_SRC_SIZE_OFFSET);

	src_x = pcrop->src_x;
	src_y = pcrop->src_y;

	D("src_x = %d, src_y=%d.\n", src_x, src_y);

	src_xy = src_y*(1<<16) + src_x;
	msm_camera_io_w(src_xy, vpe_ctrl->vpebase +
			VPE_SRC_XY_OFFSET);
	D("src_xy = %d, src_roi=%d.\n", src_xy, src_roi);

	/* decide whether to use FIR or M/N for scaling */
	if ((out_ROI_width == 1 && src_ROI_width < 4) ||
		(src_ROI_width < 4 * out_ROI_width - 3))
		scale_unit_sel_x = 0;/* use FIR scalar */
	else
		scale_unit_sel_x = 1;/* use M/N scalar */

	if ((out_ROI_height == 1 && src_ROI_height < 4) ||
		(src_ROI_height < 4 * out_ROI_height - 3))
		scale_unit_sel_y = 0;/* use FIR scalar */
	else
		scale_unit_sel_y = 1;/* use M/N scalar */

	/* calculate phase step for the x direction */

	/* if destination is only 1 pixel wide,
	the value of phase_step_x
	is unimportant. Assigning phase_step_x to
	src ROI width as an arbitrary value. */
	if (out_ROI_width == 1)
		phase_step_x = (uint32_t) ((src_ROI_width) <<
						SCALER_PHASE_BITS);

		/* if using FIR scalar */
	else if (scale_unit_sel_x == 0) {

		/* Calculate the quotient ( src_ROI_width - 1 )
			( out_ROI_width - 1)
			with u3.29 precision. Quotient is rounded up to
			the larger 29th decimal point*/
		numerator = (uint64_t)(src_ROI_width - 1) <<
			SCALER_PHASE_BITS;
		/* never equals to 0 because of the
			"(out_ROI_width == 1 )"*/
		denominator = (uint64_t)(out_ROI_width - 1);
		/* divide and round up to the larger 29th
			decimal point.*/
		phase_step_x = (uint32_t) vpe_do_div((numerator +
					denominator - 1), denominator);
	} else if (scale_unit_sel_x == 1) { /* if M/N scalar */
		/* Calculate the quotient ( src_ROI_width ) /
			( out_ROI_width)
			with u3.29 precision. Quotient is rounded down to the
			smaller 29th decimal point.*/
		numerator = (uint64_t)(src_ROI_width) <<
			SCALER_PHASE_BITS;
		denominator = (uint64_t)(out_ROI_width);
		phase_step_x =
			(uint32_t) vpe_do_div(numerator, denominator);
	}
	/* calculate phase step for the y direction */

	/* if destination is only 1 pixel wide, the value of
		phase_step_x is unimportant. Assigning phase_step_x
		to src ROI width as an arbitrary value. */
	if (out_ROI_height == 1)
		phase_step_y =
		(uint32_t) ((src_ROI_height) << SCALER_PHASE_BITS);

	/* if FIR scalar */
	else if (scale_unit_sel_y == 0) {
		/* Calculate the quotient ( src_ROI_height - 1 ) /
		( out_ROI_height - 1)
		with u3.29 precision. Quotient is rounded up to the
		larger 29th decimal point. */
		numerator = (uint64_t)(src_ROI_height - 1) <<
			SCALER_PHASE_BITS;
		/* never equals to 0 because of the "
		( out_ROI_height == 1 )" case */
		denominator = (uint64_t)(out_ROI_height - 1);
		/* Quotient is rounded up to the larger
		29th decimal point. */
		phase_step_y =
		(uint32_t) vpe_do_div(
			(numerator + denominator - 1), denominator);
	} else if (scale_unit_sel_y == 1) { /* if M/N scalar */
		/* Calculate the quotient ( src_ROI_height )
			( out_ROI_height)
			with u3.29 precision. Quotient is rounded down
			to the smaller 29th decimal point. */
		numerator = (uint64_t)(src_ROI_height) <<
			SCALER_PHASE_BITS;
		denominator = (uint64_t)(out_ROI_height);
		phase_step_y = (uint32_t) vpe_do_div(
			numerator, denominator);
	}

	/* decide which set of FIR coefficients to use */
	if (phase_step_x > HAL_MDP_PHASE_STEP_2P50)
		xscale_filter_sel = 0;
	else if (phase_step_x > HAL_MDP_PHASE_STEP_1P66)
		xscale_filter_sel = 1;
	else if (phase_step_x > HAL_MDP_PHASE_STEP_1P25)
		xscale_filter_sel = 2;
	else
		xscale_filter_sel = 3;

	if (phase_step_y > HAL_MDP_PHASE_STEP_2P50)
		yscale_filter_sel = 0;
	else if (phase_step_y > HAL_MDP_PHASE_STEP_1P66)
		yscale_filter_sel = 1;
	else if (phase_step_y > HAL_MDP_PHASE_STEP_1P25)
		yscale_filter_sel = 2;
	else
		yscale_filter_sel = 3;

	/* calculate phase init for the x direction */

	/* if using FIR scalar */
	if (scale_unit_sel_x == 0) {
		if (out_ROI_width == 1)
			phase_init_x =
				(uint32_t) ((src_ROI_width - 1) <<
							SCALER_PHASE_BITS);
		else
			phase_init_x = 0;
	} else if (scale_unit_sel_x == 1) /* M over N scalar  */
		phase_init_x = 0;

	/* calculate phase init for the y direction
	if using FIR scalar */
	if (scale_unit_sel_y == 0) {
		if (out_ROI_height == 1)
			phase_init_y =
			(uint32_t) ((src_ROI_height -
						1) << SCALER_PHASE_BITS);
		else
			phase_init_y = 0;
	} else if (scale_unit_sel_y == 1) /* M over N scalar   */
		phase_init_y = 0;

	D("phase step x = %d, step y = %d.\n",
		 phase_step_x, phase_step_y);
	D("phase init x = %d, init y = %d.\n",
		 phase_init_x, phase_init_y);

	msm_camera_io_w(phase_step_x, vpe_ctrl->vpebase +
			VPE_SCALE_PHASEX_STEP_OFFSET);
	msm_camera_io_w(phase_step_y, vpe_ctrl->vpebase +
			VPE_SCALE_PHASEY_STEP_OFFSET);

	msm_camera_io_w(phase_init_x, vpe_ctrl->vpebase +
			VPE_SCALE_PHASEX_INIT_OFFSET);

	msm_camera_io_w(phase_init_y, vpe_ctrl->vpebase +
			VPE_SCALE_PHASEY_INIT_OFFSET);

	return 1;
}

int msm_vpe_is_busy(void)
{
	int busy = 0;
	unsigned long flags;
	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	if (vpe_ctrl->state == VPE_STATE_ACTIVE)
		busy = 1;
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
	return busy;
}

static int msm_send_frame_to_vpe(void)
{
	int rc = 0;
	unsigned long flags;
	unsigned long srcP0, srcP1, outP0, outP1;
	struct msm_mctl_pp_frame_info *frame_info = vpe_ctrl->pp_frame_info;

	if (!frame_info) {
		pr_err("%s Invalid frame", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&vpe_ctrl->lock, flags);

	if (frame_info->src_frame.frame.num_planes > 1) {
		srcP0 = frame_info->src_frame.map[0].paddr +
			frame_info->src_frame.map[0].data_offset;
		srcP1 = frame_info->src_frame.map[1].paddr +
			frame_info->src_frame.map[1].data_offset;
		outP0 = frame_info->dest_frame.map[0].paddr +
			frame_info->dest_frame.map[0].data_offset;
		outP1 = frame_info->dest_frame.map[1].paddr +
			frame_info->dest_frame.map[1].data_offset;
	} else {
		srcP0 = frame_info->src_frame.map[0].paddr;
		srcP1 = frame_info->src_frame.map[0].paddr +
			frame_info->src_frame.map[0].data_offset;
		outP0 = frame_info->dest_frame.map[0].paddr;
		outP1 = frame_info->dest_frame.map[0].paddr +
			frame_info->dest_frame.map[0].data_offset;
	}

	D("%s VPE Configured with Src %x, %x Dest %x, %x",
		__func__, (uint32_t)srcP0, (uint32_t)srcP1,
		(uint32_t)outP0, (uint32_t)outP1);

	msm_camera_io_w(srcP0, vpe_ctrl->vpebase + VPE_SRCP0_ADDR_OFFSET);
	msm_camera_io_w(srcP1, vpe_ctrl->vpebase + VPE_SRCP1_ADDR_OFFSET);
	msm_camera_io_w(outP0, vpe_ctrl->vpebase + VPE_OUTP0_ADDR_OFFSET);
	msm_camera_io_w(outP1, vpe_ctrl->vpebase + VPE_OUTP1_ADDR_OFFSET);

	vpe_ctrl->state = VPE_STATE_ACTIVE;
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
	vpe_start();
	return rc;
}

static void vpe_send_outmsg(void)
{
	unsigned long flags;
	struct v4l2_event v4l2_evt;
	struct msm_queue_cmd *event_qcmd;
	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	if (vpe_ctrl->state == VPE_STATE_IDLE) {
		pr_err("%s VPE is in IDLE state. Ignore the ack msg", __func__);
		spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
		return;
	}
	event_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_ATOMIC);
	atomic_set(&event_qcmd->on_heap, 1);
	event_qcmd->command = (void *)vpe_ctrl->pp_frame_info;
	vpe_ctrl->pp_frame_info = NULL;
	vpe_ctrl->state = VPE_STATE_INIT;   /* put it back to idle. */

	/* Enqueue the event payload. */
	msm_enqueue(&vpe_ctrl->eventData_q, &event_qcmd->list_eventdata);
	/* Now queue the event. */
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_MCTL_PP_EVENT;
	v4l2_evt.id = 0;
	v4l2_event_queue(vpe_ctrl->subdev.devnode, &v4l2_evt);
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
}

static void vpe_do_tasklet(unsigned long data)
{
	D("%s: irq_status = 0x%x",
		   __func__, vpe_ctrl->irq_status);
	if (vpe_ctrl->irq_status & 0x1)
		vpe_send_outmsg();

}
DECLARE_TASKLET(vpe_tasklet, vpe_do_tasklet, 0);

static irqreturn_t vpe_parse_irq(int irq_num, void *data)
{
	vpe_ctrl->irq_status = msm_camera_io_r_mb(vpe_ctrl->vpebase +
							VPE_INTR_STATUS_OFFSET);
	msm_camera_io_w_mb(vpe_ctrl->irq_status, vpe_ctrl->vpebase +
				VPE_INTR_CLEAR_OFFSET);
	msm_camera_io_w(0, vpe_ctrl->vpebase + VPE_INTR_ENABLE_OFFSET);
	D("%s: vpe_parse_irq =0x%x.\n", __func__, vpe_ctrl->irq_status);
	tasklet_schedule(&vpe_tasklet);
	return IRQ_HANDLED;
}

static struct msm_cam_clk_info vpe_clk_info[] = {
	{"vpe_clk", 160000000},
	{"vpe_pclk", -1},
};

int vpe_enable(uint32_t clk_rate, struct msm_cam_media_controller *mctl)
{
	int rc = 0;
	unsigned long flags = 0;
	D("%s", __func__);
	/* don't change the order of clock and irq.*/
	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	if (vpe_ctrl->state != VPE_STATE_IDLE) {
		pr_err("%s: VPE already enabled", __func__);
		spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
		return 0;
	}
	vpe_ctrl->state = VPE_STATE_INIT;
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
	enable_irq(vpe_ctrl->vpeirq->start);

	if (vpe_ctrl->fs_vpe) {
		rc = regulator_enable(vpe_ctrl->fs_vpe);
		if (rc) {
			pr_err("%s: Regulator enable failed\n", __func__);
			goto vpe_fs_failed;
		}
	}

	rc = msm_cam_clk_enable(&vpe_ctrl->pdev->dev, vpe_clk_info,
			vpe_ctrl->vpe_clk, ARRAY_SIZE(vpe_clk_info), 1);
	if (rc < 0)
		goto vpe_clk_failed;

#ifdef CONFIG_MSM_IOMMU
	rc = iommu_attach_device(mctl->domain, vpe_ctrl->iommu_ctx_src);
	if (rc < 0) {
		pr_err("%s: Device attach failed\n", __func__);
		goto src_attach_failed;
	}
	rc = iommu_attach_device(mctl->domain, vpe_ctrl->iommu_ctx_dst);
	if (rc < 0) {
		pr_err("%s: Device attach failed\n", __func__);
		goto dst_attach_failed;
	}
#endif
	return rc;

#ifdef CONFIG_MSM_IOMMU
dst_attach_failed:
	iommu_detach_device(mctl->domain, vpe_ctrl->iommu_ctx_src);
src_attach_failed:
#endif
	msm_cam_clk_enable(&vpe_ctrl->pdev->dev, vpe_clk_info,
		vpe_ctrl->vpe_clk, ARRAY_SIZE(vpe_clk_info), 0);
vpe_clk_failed:
	if (vpe_ctrl->fs_vpe)
		regulator_disable(vpe_ctrl->fs_vpe);
vpe_fs_failed:
	disable_irq(vpe_ctrl->vpeirq->start);
	vpe_ctrl->state = VPE_STATE_IDLE;
	return rc;
}

int vpe_disable(struct msm_cam_media_controller *mctl)
{
	int rc = 0;
	unsigned long flags = 0;
	D("%s", __func__);
	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	if (vpe_ctrl->state == VPE_STATE_IDLE) {
		D("%s: VPE already disabled", __func__);
		spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
		return rc;
	}
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
#ifdef CONFIG_MSM_IOMMU
	iommu_detach_device(mctl->domain, vpe_ctrl->iommu_ctx_dst);
	iommu_detach_device(mctl->domain, vpe_ctrl->iommu_ctx_src);
#endif
	disable_irq(vpe_ctrl->vpeirq->start);
	tasklet_kill(&vpe_tasklet);
	msm_cam_clk_enable(&vpe_ctrl->pdev->dev, vpe_clk_info,
			vpe_ctrl->vpe_clk, ARRAY_SIZE(vpe_clk_info), 0);

	regulator_disable(vpe_ctrl->fs_vpe);
	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	vpe_ctrl->state = VPE_STATE_IDLE;
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
	return rc;
}

static int msm_vpe_do_pp(struct msm_mctl_pp_frame_info *pp_frame_info)
{
	int rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&vpe_ctrl->lock, flags);
	if (vpe_ctrl->state == VPE_STATE_ACTIVE ||
		 vpe_ctrl->state == VPE_STATE_IDLE) {
		spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
		pr_err(" =====VPE in wrong state:%d!!!  Wrong!========\n",
		vpe_ctrl->state);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&vpe_ctrl->lock, flags);
	vpe_ctrl->pp_frame_info = pp_frame_info;
	msm_vpe_cfg_update(
		&vpe_ctrl->pp_frame_info->pp_frame_cmd.crop);
	D("%s Sending frame idx %d id %d to VPE ", __func__,
		pp_frame_info->src_frame.frame.buf_idx,
		pp_frame_info->src_frame.frame.frame_id);
	rc = msm_send_frame_to_vpe();
	return rc;
}

static int msm_vpe_resource_init(void);

int msm_vpe_subdev_init(struct v4l2_subdev *sd)
{
	int rc = 0;
	struct msm_cam_media_controller *mctl;
	mctl = v4l2_get_subdev_hostdata(sd);
	D("%s:begin", __func__);
	if (atomic_read(&vpe_init_done)) {
		pr_err("%s: VPE has been initialized", __func__);
		return -EBUSY;
	}
	atomic_set(&vpe_init_done, 1);

	rc = msm_vpe_resource_init();
	if (rc < 0) {
		atomic_set(&vpe_init_done, 0);
		return rc;
	}
	spin_lock_init(&vpe_ctrl->lock);
	D("%s:end", __func__);
	return rc;
}
EXPORT_SYMBOL(msm_vpe_subdev_init);

static int msm_vpe_resource_init(void)
{
	int rc = 0;

	vpe_ctrl->vpebase = ioremap(vpe_ctrl->vpemem->start,
		resource_size(vpe_ctrl->vpemem));

	if (!vpe_ctrl->vpebase) {
		rc = -ENOMEM;
		pr_err("%s: vpe ioremap failed\n", __func__);
		goto vpe_unmap_mem_region;
	}

	return rc;
/* from this part it is error handling. */
vpe_unmap_mem_region:
	iounmap(vpe_ctrl->vpebase);
	vpe_ctrl->vpebase = NULL;
	return rc;  /* this rc should have error code. */
}

void msm_vpe_subdev_release(struct v4l2_subdev *sd)
{
	struct msm_cam_media_controller *mctl;
	mctl = v4l2_get_subdev_hostdata(sd);
	if (!atomic_read(&vpe_init_done)) {
		/* no VPE object created */
		pr_err("%s: no VPE object to release", __func__);
		return;
	}
	vpe_reset();
	vpe_disable(mctl);
	iounmap(vpe_ctrl->vpebase);
	vpe_ctrl->vpebase = NULL;
	atomic_set(&vpe_init_done, 0);
}
EXPORT_SYMBOL(msm_vpe_subdev_release);

static int msm_vpe_process_vpe_cmd(struct msm_vpe_cfg_cmd *vpe_cmd,
				struct msm_cam_media_controller *mctl)
{
	int rc = 0;

	switch (vpe_cmd->cmd_type) {
	case VPE_CMD_RESET:
		rc = vpe_reset();
		break;

	case VPE_CMD_OPERATION_MODE_CFG: {
		struct msm_vpe_op_mode_cfg op_mode_cfg;
		if (sizeof(struct msm_vpe_op_mode_cfg) != vpe_cmd->length) {
			pr_err("%s: size mismatch cmd=%d, len=%d, expected=%d",
				__func__, vpe_cmd->cmd_type, vpe_cmd->length,
				sizeof(struct msm_vpe_op_mode_cfg));
			rc = -EINVAL;
			break;
		}
		COPY_FROM_USER(rc, &op_mode_cfg, (void __user *)vpe_cmd->value,
			sizeof(op_mode_cfg));
		if (rc) {
			ERR_COPY_FROM_USER();
			break;
		}

		vpe_cmd->value = (void *)&op_mode_cfg;
		rc = vpe_operation_config(vpe_cmd->value);
		break;
		}

	case VPE_CMD_INPUT_PLANE_CFG: {
		struct msm_vpe_input_plane_cfg input_cfg;
		if (sizeof(struct msm_vpe_input_plane_cfg) != vpe_cmd->length) {
			pr_err("%s: mismatch cmd = %d, len = %d, expected = %d",
				__func__, vpe_cmd->cmd_type, vpe_cmd->length,
				sizeof(struct msm_vpe_input_plane_cfg));
			rc = -EINVAL;
			break;
		}
		COPY_FROM_USER(rc, &input_cfg, (void __user *)vpe_cmd->value,
			sizeof(input_cfg));
		if (rc) {
			ERR_COPY_FROM_USER();
			break;
		}

		vpe_cmd->value = (void *)&input_cfg;
		vpe_input_plane_config(vpe_cmd->value);
		break;
		}

	case VPE_CMD_OUTPUT_PLANE_CFG: {
		struct msm_vpe_output_plane_cfg output_cfg;
		if (sizeof(struct msm_vpe_output_plane_cfg) !=
			vpe_cmd->length) {
			pr_err("%s: size mismatch cmd=%d, len=%d, expected=%d",
				__func__, vpe_cmd->cmd_type, vpe_cmd->length,
				sizeof(struct msm_vpe_output_plane_cfg));
				rc = -EINVAL;
				break;
		}
		COPY_FROM_USER(rc, &output_cfg, (void __user *)vpe_cmd->value,
			sizeof(output_cfg));
		if (rc) {
			ERR_COPY_FROM_USER();
			break;
		}

		vpe_cmd->value = (void *)&output_cfg;
		vpe_output_plane_config(vpe_cmd->value);
		break;
		}

	case VPE_CMD_SCALE_CFG_TYPE:{
		struct msm_vpe_scaler_cfg scaler_cfg;
		if (sizeof(struct msm_vpe_scaler_cfg) != vpe_cmd->length) {
			pr_err("%s: size mismatch cmd=%d, len=%d, expected=%d",
				__func__, vpe_cmd->cmd_type, vpe_cmd->length,
				sizeof(struct msm_vpe_scaler_cfg));
			rc = -EINVAL;
			break;
		}
		COPY_FROM_USER(rc, &scaler_cfg, (void __user *)vpe_cmd->value,
			sizeof(scaler_cfg));
		if (rc) {
			ERR_COPY_FROM_USER();
			break;
		}

		vpe_cmd->value = (void *)&scaler_cfg;
		vpe_update_scale_coef(vpe_cmd->value);
		break;
		}

	case VPE_CMD_ZOOM: {
		struct msm_mctl_pp_frame_info *zoom;
		zoom = kmalloc(sizeof(struct msm_mctl_pp_frame_info),
				GFP_ATOMIC);
		if (!zoom) {
			pr_err("%s Not enough memory ", __func__);
			rc = -ENOMEM;
			break;
		}

		if (sizeof(zoom->pp_frame_cmd) != vpe_cmd->length) {
			pr_err("%s: size mismatch id=%d, len=%d, expected=%d",
				__func__, vpe_cmd->cmd_type, vpe_cmd->length,
				sizeof(zoom->pp_frame_cmd));
			rc = -EINVAL;
			kfree(zoom);
			break;
		}
		COPY_FROM_USER(rc, &zoom->pp_frame_cmd,
			(void __user *)vpe_cmd->value,
			sizeof(zoom->pp_frame_cmd));
		if (rc) {
			ERR_COPY_FROM_USER();
			kfree(zoom);
			break;
		}

		zoom->user_cmd = vpe_cmd->cmd_type;
		zoom->p_mctl = v4l2_get_subdev_hostdata(&vpe_ctrl->subdev);
		D("%s: cookie=0x%x,action=0x%x,path=0x%x",
			__func__, zoom->pp_frame_cmd.cookie,
			zoom->pp_frame_cmd.vpe_output_action,
			zoom->pp_frame_cmd.path);

		D("%s Mapping Source frame ", __func__);
		zoom->src_frame.frame = zoom->pp_frame_cmd.src_frame;
		rc = msm_mctl_map_user_frame(&zoom->src_frame,
			zoom->p_mctl->client, mctl->domain_num);
		if (rc < 0) {
			pr_err("%s Error mapping source buffer rc = %d",
				__func__, rc);
			kfree(zoom);
			break;
		}

		D("%s Mapping Destination frame ", __func__);
		zoom->dest_frame.frame = zoom->pp_frame_cmd.dest_frame;
		rc = msm_mctl_map_user_frame(&zoom->dest_frame,
			zoom->p_mctl->client, mctl->domain_num);
		if (rc < 0) {
			pr_err("%s Error mapping dest buffer rc = %d",
				__func__, rc);
			msm_mctl_unmap_user_frame(&zoom->src_frame,
				zoom->p_mctl->client, mctl->domain_num);
			kfree(zoom);
			break;
		}

		rc = msm_vpe_do_pp(zoom);
		break;
		}

	case VPE_CMD_ENABLE: {
		struct msm_vpe_clock_rate clk_rate;
		int turbo_mode;
		if (sizeof(struct msm_vpe_clock_rate) != vpe_cmd->length) {
			pr_err("%s: size mismatch cmd=%d, len=%d, expected=%d",
				__func__, vpe_cmd->cmd_type, vpe_cmd->length,
				sizeof(struct msm_vpe_clock_rate));
			rc = -EINVAL;
			break;
		}
		if (copy_from_user(&clk_rate, (void __user *)vpe_cmd->value,
			sizeof(struct msm_vpe_clock_rate))) {
			pr_err("%s:clk_rate copy failed", __func__);
			return -EFAULT;
		}
		turbo_mode = (int)clk_rate.rate;
		rc = turbo_mode ? vpe_enable(VPE_TURBO_MODE_CLOCK_RATE, mctl) :
				vpe_enable(VPE_NORMAL_MODE_CLOCK_RATE, mctl);
		break;
		}

	case VPE_CMD_DISABLE:
		rc = vpe_disable(mctl);
		break;

	default:
		break;
	}

	return rc;
}

static long msm_vpe_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_vpe_cfg_cmd *vpe_cmd;
	int rc = 0;
	struct msm_cam_media_controller *mctl;
	mctl = v4l2_get_subdev_hostdata(sd);
	switch (cmd) {
	case VIDIOC_MSM_VPE_INIT: {
		msm_vpe_subdev_init(sd);
		break;
		}

	case VIDIOC_MSM_VPE_RELEASE:
		msm_vpe_subdev_release(sd);
		break;

	case MSM_CAM_V4L2_IOCTL_CFG_VPE: {
		vpe_cmd = (struct msm_vpe_cfg_cmd *)arg;
		rc = msm_vpe_process_vpe_cmd(vpe_cmd, mctl);
		if (rc < 0) {
			pr_err("%s Error processing VPE cmd %d ",
				__func__, vpe_cmd->cmd_type);
			break;
		}
		break;
		}

	case MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD: {
		struct msm_device_queue *queue = &vpe_ctrl->eventData_q;
		struct msm_queue_cmd *event_qcmd;
		struct msm_mctl_pp_event_info pp_event_info;
		struct msm_mctl_pp_frame_info *pp_frame_info;
		struct msm_camera_v4l2_ioctl_t *v4l2_ioctl = arg;

		event_qcmd = msm_dequeue(queue, list_eventdata);
		if (!event_qcmd) {
			pr_err("%s No events in the queue", __func__);
			return -EFAULT;
		}
		pp_frame_info = event_qcmd->command;

		D("%s Unmapping source and destination buffers ",
			__func__);
		msm_mctl_unmap_user_frame(&pp_frame_info->src_frame,
			pp_frame_info->p_mctl->client, mctl->domain_num);
		msm_mctl_unmap_user_frame(&pp_frame_info->dest_frame,
			pp_frame_info->p_mctl->client, mctl->domain_num);

		pp_event_info.event = MCTL_PP_EVENT_CMD_ACK;
		pp_event_info.ack.cmd = pp_frame_info->user_cmd;
		pp_event_info.ack.status = 0;
		pp_event_info.ack.cookie = pp_frame_info->pp_frame_cmd.cookie;
		D("%s Sending payload %d %d %d", __func__,
			pp_event_info.ack.cmd, pp_event_info.ack.status,
			pp_event_info.ack.cookie);
		if (copy_to_user((void __user *)v4l2_ioctl->ioctl_ptr,
			&pp_event_info,	sizeof(struct msm_mctl_pp_event_info)))
			pr_err("%s PAYLOAD Copy to user failed ", __func__);

		kfree(pp_frame_info);
		kfree(event_qcmd);
		break;
		}

	default:
		break;
	}
	return rc;
}

int msm_vpe_subdev_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	D("%s E\n", __func__);
	return v4l2_event_subscribe(fh, sub, VPE_SUBDEV_MAX_EVENTS);
}

int msm_vpe_subdev_unsubscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	D("%s E\n", __func__);
	return v4l2_event_unsubscribe(fh, sub);
}

static const struct v4l2_subdev_core_ops msm_vpe_subdev_core_ops = {
	.ioctl = msm_vpe_subdev_ioctl,
	.subscribe_event = msm_vpe_subdev_subscribe_event,
	.unsubscribe_event = msm_vpe_subdev_unsubscribe_event,
};

static const struct v4l2_subdev_ops msm_vpe_subdev_ops = {
	.core = &msm_vpe_subdev_core_ops,
};

static int msm_vpe_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct vpe_ctrl_type *vpe_ctrl = v4l2_get_subdevdata(sd);
	/* Only one client of VPE allowed. */
	if (atomic_read(&vpe_ctrl->active) != 0) {
		pr_err("%s already opened\n", __func__);
		return -EINVAL;
	}

	D("%s E ", __func__);
	atomic_inc(&vpe_ctrl->active);
	return 0;
}

static int msm_vpe_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct vpe_ctrl_type *vpe_ctrl = v4l2_get_subdevdata(sd);
	struct msm_mctl_pp_frame_info *frame_info = vpe_ctrl->pp_frame_info;
	struct msm_cam_media_controller *mctl;
	mctl = v4l2_get_subdev_hostdata(sd);
	if (atomic_read(&vpe_ctrl->active) == 0) {
		pr_err("%s already closed\n", __func__);
		return -EINVAL;
	}

	D("%s E ", __func__);
	if (frame_info) {
		D("%s Unmap the pending item from the queue ", __func__);
		msm_mctl_unmap_user_frame(&frame_info->src_frame,
			frame_info->p_mctl->client, mctl->domain_num);
		msm_mctl_unmap_user_frame(&frame_info->dest_frame,
			frame_info->p_mctl->client, mctl->domain_num);
	}
	/* Drain the payload queue. */
	msm_queue_drain(&vpe_ctrl->eventData_q, list_eventdata);
	atomic_dec(&vpe_ctrl->active);
	return 0;
}

static const struct v4l2_subdev_internal_ops msm_vpe_internal_ops = {
	.open = msm_vpe_subdev_open,
	.close = msm_vpe_subdev_close,
};

static int __devinit msm_vpe_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cam_subdev_info sd_info;

	D("%s: device id = %d\n", __func__, pdev->id);
	vpe_ctrl = kzalloc(sizeof(struct vpe_ctrl_type), GFP_KERNEL);
	if (!vpe_ctrl) {
		pr_err("%s: not enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&vpe_ctrl->subdev, &msm_vpe_subdev_ops);
	v4l2_set_subdevdata(&vpe_ctrl->subdev, vpe_ctrl);
	vpe_ctrl->subdev.internal_ops = &msm_vpe_internal_ops;
	vpe_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(vpe_ctrl->subdev.name, sizeof(vpe_ctrl->subdev.name), "vpe");
	platform_set_drvdata(pdev, &vpe_ctrl->subdev);

	media_entity_init(&vpe_ctrl->subdev.entity, 0, NULL, 0);
	vpe_ctrl->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	vpe_ctrl->subdev.entity.group_id = VPE_DEV;
	vpe_ctrl->subdev.entity.name = vpe_ctrl->subdev.name;

	vpe_ctrl->subdev.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;

	vpe_ctrl->vpemem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "vpe");
	if (!vpe_ctrl->vpemem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto vpe_no_resource;
	}
	vpe_ctrl->vpeirq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "vpe");
	if (!vpe_ctrl->vpeirq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto vpe_no_resource;
	}

	vpe_ctrl->vpeio = request_mem_region(vpe_ctrl->vpemem->start,
		resource_size(vpe_ctrl->vpemem), pdev->name);
	if (!vpe_ctrl->vpeio) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto vpe_no_resource;
	}

	rc = request_irq(vpe_ctrl->vpeirq->start, vpe_parse_irq,
		IRQF_TRIGGER_RISING, "vpe", 0);
	if (rc < 0) {
		release_mem_region(vpe_ctrl->vpemem->start,
			resource_size(vpe_ctrl->vpemem));
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto vpe_no_resource;
	}

	vpe_ctrl->fs_vpe = regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(vpe_ctrl->fs_vpe)) {
		pr_err("%s: Regulator FS_VPE get failed %ld\n", __func__,
			PTR_ERR(vpe_ctrl->fs_vpe));
		vpe_ctrl->fs_vpe = NULL;
	}

	disable_irq(vpe_ctrl->vpeirq->start);

#ifdef CONFIG_MSM_IOMMU
	/*get device context for IOMMU*/
	vpe_ctrl->iommu_ctx_src = msm_iommu_get_ctx("vpe_src"); /*re-confirm*/
	vpe_ctrl->iommu_ctx_dst = msm_iommu_get_ctx("vpe_dst"); /*re-confirm*/
	if (!vpe_ctrl->iommu_ctx_src || !vpe_ctrl->iommu_ctx_dst) {
		release_mem_region(vpe_ctrl->vpemem->start,
			resource_size(vpe_ctrl->vpemem));
		pr_err("%s: No iommu fw context found\n", __func__);
		rc = -ENODEV;
		goto vpe_no_resource;
	}
#endif

	atomic_set(&vpe_ctrl->active, 0);
	vpe_ctrl->pdev = pdev;
	sd_info.sdev_type = VPE_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = vpe_ctrl->vpeirq->start;
	msm_cam_register_subdev_node(&vpe_ctrl->subdev, &sd_info);
	vpe_ctrl->subdev.entity.revision = vpe_ctrl->subdev.devnode->num;
	msm_queue_init(&vpe_ctrl->eventData_q, "ackevents");

	return 0;

vpe_no_resource:
	pr_err("%s: VPE Probe failed.\n", __func__);
	kfree(vpe_ctrl);
	return rc;
}

struct platform_driver msm_vpe_driver = {
	.probe = msm_vpe_probe,
	.driver = {
		.name = MSM_VPE_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_vpe_init_module(void)
{
	return platform_driver_register(&msm_vpe_driver);
}

static void __exit msm_vpe_exit_module(void)
{
	platform_driver_unregister(&msm_vpe_driver);
}

module_init(msm_vpe_init_module);
module_exit(msm_vpe_exit_module);
MODULE_DESCRIPTION("VPE driver");
MODULE_LICENSE("GPL v2");
