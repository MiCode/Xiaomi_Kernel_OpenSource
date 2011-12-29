/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/io.h>
#include <linux/slab.h>
#include "msm_vpe1.h"
#include <linux/pm_qos_params.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include <asm/div64.h>

static int vpe_enable(uint32_t);
static int vpe_disable(void);
static int vpe_update_scaler(struct video_crop_t *pcrop);
static struct vpe_device_type  vpe_device_data;
static struct vpe_device_type  *vpe_device;
struct vpe_ctrl_type    *vpe_ctrl;
char *vpe_general_cmd[] = {
	"VPE_DUMMY_0",  /* 0 */
	"VPE_SET_CLK",
	"VPE_RESET",
	"VPE_START",
	"VPE_ABORT",
	"VPE_OPERATION_MODE_CFG",  /* 5 */
	"VPE_INPUT_PLANE_CFG",
	"VPE_OUTPUT_PLANE_CFG",
	"VPE_INPUT_PLANE_UPDATE",
	"VPE_SCALE_CFG_TYPE",
	"VPE_ROTATION_CFG_TYPE",  /* 10 */
	"VPE_AXI_OUT_CFG",
	"VPE_CMD_DIS_OFFSET_CFG",
	"VPE_ENABLE",
	"VPE_DISABLE",
};
static uint32_t orig_src_y, orig_src_cbcr;

#define CHECKED_COPY_FROM_USER(in) {					\
	if (copy_from_user((in), (void __user *)cmd->value,		\
			cmd->length)) {					\
		rc = -EFAULT;						\
		break;							\
	}								\
}

#define msm_dequeue_vpe(queue, member) ({			\
	unsigned long flags;					\
	struct msm_device_queue *__q = (queue);			\
	struct msm_queue_cmd *qcmd = 0;				\
	spin_lock_irqsave(&__q->lock, flags);			\
	if (!list_empty(&__q->list)) {				\
		__q->len--;					\
		qcmd = list_first_entry(&__q->list,		\
				struct msm_queue_cmd, member);	\
		list_del_init(&qcmd->member);			\
	}							\
	spin_unlock_irqrestore(&__q->lock, flags);		\
	qcmd;							\
})

/*
static   struct vpe_cmd_type vpe_cmd[] = {
		{VPE_DUMMY_0, 0},
		{VPE_SET_CLK, 0},
		{VPE_RESET, 0},
		{VPE_START, 0},
		{VPE_ABORT, 0},
		{VPE_OPERATION_MODE_CFG, VPE_OPERATION_MODE_CFG_LEN},
		{VPE_INPUT_PLANE_CFG, VPE_INPUT_PLANE_CFG_LEN},
		{VPE_OUTPUT_PLANE_CFG, VPE_OUTPUT_PLANE_CFG_LEN},
		{VPE_INPUT_PLANE_UPDATE, VPE_INPUT_PLANE_UPDATE_LEN},
		{VPE_SCALE_CFG_TYPE, VPE_SCALER_CONFIG_LEN},
		{VPE_ROTATION_CFG_TYPE, 0},
		{VPE_AXI_OUT_CFG, 0},
		{VPE_CMD_DIS_OFFSET_CFG, VPE_DIS_OFFSET_CFG_LEN},
};
*/

static long long vpe_do_div(long long num, long long den)
{
	do_div(num, den);
	return num;
}

static int vpe_start(void)
{
	/*  enable the frame irq, bit 0 = Display list 0 ROI done */
	msm_io_w(1, vpe_device->vpebase + VPE_INTR_ENABLE_OFFSET);
	msm_io_dump(vpe_device->vpebase + 0x10000, 0x250);
	/* this triggers the operation. */
	msm_io_w(1, vpe_device->vpebase + VPE_DL0_START_OFFSET);

	return 0;
}

void vpe_reset_state_variables(void)
{
	/* initialize local variables for state control, etc.*/
	vpe_ctrl->op_mode = 0;
	vpe_ctrl->state = VPE_STATE_INIT;
	spin_lock_init(&vpe_ctrl->tasklet_lock);
	spin_lock_init(&vpe_ctrl->state_lock);
	INIT_LIST_HEAD(&vpe_ctrl->tasklet_q);
}

static void vpe_config_axi_default(void)
{
	msm_io_w(0x25, vpe_device->vpebase + VPE_AXI_ARB_2_OFFSET);

	CDBG("%s: yaddr %ld cbcraddr %ld", __func__,
		 vpe_ctrl->out_y_addr, vpe_ctrl->out_cbcr_addr);

	if (!vpe_ctrl->out_y_addr || !vpe_ctrl->out_cbcr_addr)
		return;

	msm_io_w(vpe_ctrl->out_y_addr,
		vpe_device->vpebase + VPE_OUTP0_ADDR_OFFSET);
	/* for video  CbCr address */
	msm_io_w(vpe_ctrl->out_cbcr_addr,
		vpe_device->vpebase + VPE_OUTP1_ADDR_OFFSET);

}

static int vpe_reset(void)
{
	uint32_t vpe_version;
	uint32_t rc;

	vpe_reset_state_variables();
	vpe_version = msm_io_r(vpe_device->vpebase + VPE_HW_VERSION_OFFSET);
	CDBG("vpe_version = 0x%x\n", vpe_version);

	/* disable all interrupts.*/
	msm_io_w(0, vpe_device->vpebase + VPE_INTR_ENABLE_OFFSET);
	/* clear all pending interrupts*/
	msm_io_w(0x1fffff, vpe_device->vpebase + VPE_INTR_CLEAR_OFFSET);

	/* write sw_reset to reset the core. */
	msm_io_w(0x10, vpe_device->vpebase + VPE_SW_RESET_OFFSET);

	/* then poll the reset bit, it should be self-cleared. */
	while (1) {
		rc =
		msm_io_r(vpe_device->vpebase + VPE_SW_RESET_OFFSET) & 0x10;
		if (rc == 0)
			break;
	}

	/*  at this point, hardware is reset. Then pogram to default
		values. */
	msm_io_w(VPE_AXI_RD_ARB_CONFIG_VALUE,
			vpe_device->vpebase + VPE_AXI_RD_ARB_CONFIG_OFFSET);

	msm_io_w(VPE_CGC_ENABLE_VALUE,
			vpe_device->vpebase + VPE_CGC_EN_OFFSET);

	msm_io_w(1, vpe_device->vpebase + VPE_CMD_MODE_OFFSET);

	msm_io_w(VPE_DEFAULT_OP_MODE_VALUE,
			vpe_device->vpebase + VPE_OP_MODE_OFFSET);

	msm_io_w(VPE_DEFAULT_SCALE_CONFIG,
			vpe_device->vpebase + VPE_SCALE_CONFIG_OFFSET);

	vpe_config_axi_default();
	return 0;
}

int msm_vpe_cfg_update(void *pinfo)
{
	uint32_t  rot_flag, rc = 0;
	struct video_crop_t *pcrop = (struct video_crop_t *)pinfo;

	rot_flag = msm_io_r(vpe_device->vpebase +
						VPE_OP_MODE_OFFSET) & 0xE00;
	if (pinfo != NULL) {
		CDBG("Crop info in2_w = %d, in2_h = %d "
			"out2_h = %d out2_w = %d \n", pcrop->in2_w,
			pcrop->in2_h,
			pcrop->out2_h, pcrop->out2_w);
		rc = vpe_update_scaler(pcrop);
	}
	CDBG("return rc = %d rot_flag = %d\n", rc, rot_flag);
	rc |= rot_flag;

	return rc;
}

void vpe_update_scale_coef(uint32_t *p)
{
	uint32_t i, offset;
	offset = *p;
	for (i = offset; i < (VPE_SCALE_COEFF_NUM + offset); i++) {
		msm_io_w(*(++p), vpe_device->vpebase + VPE_SCALE_COEFF_LSBn(i));
		msm_io_w(*(++p), vpe_device->vpebase + VPE_SCALE_COEFF_MSBn(i));
	}
}

void vpe_input_plane_config(uint32_t *p)
{
	msm_io_w(*p, vpe_device->vpebase + VPE_SRC_FORMAT_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_SRC_UNPACK_PATTERN1_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_SRC_IMAGE_SIZE_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_SRC_YSTRIDE1_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_SRC_SIZE_OFFSET);
	vpe_ctrl->in_h_w = *p;
	msm_io_w(*(++p), vpe_device->vpebase + VPE_SRC_XY_OFFSET);
}

void vpe_output_plane_config(uint32_t *p)
{
	msm_io_w(*p, vpe_device->vpebase + VPE_OUT_FORMAT_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_OUT_PACK_PATTERN1_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_OUT_YSTRIDE1_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_OUT_SIZE_OFFSET);
	msm_io_w(*(++p), vpe_device->vpebase + VPE_OUT_XY_OFFSET);
	vpe_ctrl->pcbcr_dis_offset = *(++p);
}

static int vpe_operation_config(uint32_t *p)
{
	uint32_t  outw, outh, temp;
	msm_io_w(*p, vpe_device->vpebase + VPE_OP_MODE_OFFSET);

	temp = msm_io_r(vpe_device->vpebase + VPE_OUT_SIZE_OFFSET);
	outw = temp & 0xFFF;
	outh = (temp & 0xFFF0000) >> 16;

	if (*p++ & 0xE00) {
		/* rotation enabled. */
		vpe_ctrl->out_w = outh;
		vpe_ctrl->out_h = outw;
	} else {
		vpe_ctrl->out_w = outw;
		vpe_ctrl->out_h = outh;
	}
	vpe_ctrl->dis_en = *p;
	return 0;
}

/* Later we can separate the rotation and scaler calc. If
*  rotation is enabled, simply swap the destination dimension.
*  And then pass the already swapped output size to this
*  function. */
static int vpe_update_scaler(struct video_crop_t *pcrop)
{
	uint32_t out_ROI_width, out_ROI_height;
	uint32_t src_ROI_width, src_ROI_height;

	uint32_t rc = 0;  /* default to no zoom. */
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

	if ((pcrop->in2_w >= pcrop->out2_w) &&
		(pcrop->in2_h >= pcrop->out2_h)) {
		CDBG(" =======VPE no zoom needed.\n");

		temp = msm_io_r(vpe_device->vpebase + VPE_OP_MODE_OFFSET)
		& 0xfffffffc;
		msm_io_w(temp, vpe_device->vpebase + VPE_OP_MODE_OFFSET);


		msm_io_w(0, vpe_device->vpebase + VPE_SRC_XY_OFFSET);

		CDBG("vpe_ctrl->in_h_w = %d \n", vpe_ctrl->in_h_w);
		msm_io_w(vpe_ctrl->in_h_w , vpe_device->vpebase +
				VPE_SRC_SIZE_OFFSET);

		return rc;
	}
	/* If fall through then scaler is needed.*/

	CDBG("========VPE zoom needed.\n");
	/* assumption is both direction need zoom. this can be
	improved. */
	temp =
		msm_io_r(vpe_device->vpebase + VPE_OP_MODE_OFFSET) | 0x3;
	msm_io_w(temp, vpe_device->vpebase + VPE_OP_MODE_OFFSET);

	src_ROI_width = pcrop->in2_w;
	src_ROI_height = pcrop->in2_h;
	out_ROI_width = pcrop->out2_w;
	out_ROI_height = pcrop->out2_h;

	CDBG("src w = 0x%x, h=0x%x, dst w = 0x%x, h =0x%x.\n",
		src_ROI_width, src_ROI_height, out_ROI_width,
		out_ROI_height);
	src_roi = (src_ROI_height << 16) + src_ROI_width;

	msm_io_w(src_roi, vpe_device->vpebase + VPE_SRC_SIZE_OFFSET);

	src_x = (out_ROI_width - src_ROI_width)/2;
	src_y = (out_ROI_height - src_ROI_height)/2;

	CDBG("src_x = %d, src_y=%d.\n", src_x, src_y);

	src_xy = src_y*(1<<16) + src_x;
	msm_io_w(src_xy, vpe_device->vpebase +
			VPE_SRC_XY_OFFSET);
	CDBG("src_xy = %d, src_roi=%d.\n", src_xy, src_roi);

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
		/ ( out_ROI_width - 1)
		with u3.29 precision. Quotient is rounded up to
		the larger 29th decimal point. */
		numerator = (uint64_t)(src_ROI_width - 1) <<
			SCALER_PHASE_BITS;
		/* never equals to 0 because of the
		"(out_ROI_width == 1 )"*/
		denominator = (uint64_t)(out_ROI_width - 1);
		/* divide and round up to the larger 29th
		decimal point. */
		phase_step_x = (uint32_t) vpe_do_div((numerator +
					denominator - 1), denominator);
	} else if (scale_unit_sel_x == 1) { /* if M/N scalar */
		/* Calculate the quotient ( src_ROI_width ) /
		( out_ROI_width)
		with u3.29 precision. Quotient is rounded down to the
		smaller 29th decimal point. */
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
		/ ( out_ROI_height)
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

	CDBG("phase step x = %d, step y = %d.\n",
		 phase_step_x, phase_step_y);
	CDBG("phase init x = %d, init y = %d.\n",
		 phase_init_x, phase_init_y);

	msm_io_w(phase_step_x, vpe_device->vpebase +
			VPE_SCALE_PHASEX_STEP_OFFSET);
	msm_io_w(phase_step_y, vpe_device->vpebase +
			VPE_SCALE_PHASEY_STEP_OFFSET);

	msm_io_w(phase_init_x, vpe_device->vpebase +
			VPE_SCALE_PHASEX_INIT_OFFSET);

	msm_io_w(phase_init_y, vpe_device->vpebase +
			VPE_SCALE_PHASEY_INIT_OFFSET);

	return 1;
}

static int vpe_update_scaler_with_dis(struct video_crop_t *pcrop,
				struct dis_offset_type *dis_offset)
{
	uint32_t out_ROI_width, out_ROI_height;
	uint32_t src_ROI_width, src_ROI_height;

	uint32_t rc = 0;  /* default to no zoom. */
	/*
	* phase_step_x, phase_step_y, phase_init_x and phase_init_y
	* are represented in fixed-point, unsigned 3.29 format
	*/
	uint32_t phase_step_x = 0;
	uint32_t phase_step_y = 0;
	uint32_t phase_init_x = 0;
	uint32_t phase_init_y = 0;

	uint32_t src_roi, temp;
	int32_t  src_x, src_y, src_xy;
	uint32_t yscale_filter_sel, xscale_filter_sel;
	uint32_t scale_unit_sel_x, scale_unit_sel_y;
	uint64_t numerator, denominator;
	int32_t  zoom_dis_x, zoom_dis_y;

	CDBG("%s: pcrop->in2_w = %d, pcrop->in2_h = %d\n", __func__,
		 pcrop->in2_w, pcrop->in2_h);
	CDBG("%s: pcrop->out2_w = %d, pcrop->out2_h = %d\n", __func__,
		 pcrop->out2_w, pcrop->out2_h);

	if ((pcrop->in2_w >= pcrop->out2_w) &&
		(pcrop->in2_h >= pcrop->out2_h)) {
		CDBG(" =======VPE no zoom needed, DIS is still enabled. \n");

		temp = msm_io_r(vpe_device->vpebase + VPE_OP_MODE_OFFSET)
		& 0xfffffffc;
		msm_io_w(temp, vpe_device->vpebase + VPE_OP_MODE_OFFSET);

		/* no zoom, use dis offset directly. */
		src_xy = dis_offset->dis_offset_y * (1<<16) +
			dis_offset->dis_offset_x;

		msm_io_w(src_xy, vpe_device->vpebase + VPE_SRC_XY_OFFSET);

		CDBG("vpe_ctrl->in_h_w = 0x%x \n", vpe_ctrl->in_h_w);
		msm_io_w(vpe_ctrl->in_h_w, vpe_device->vpebase +
				 VPE_SRC_SIZE_OFFSET);
		return rc;
	}
	/* If fall through then scaler is needed.*/

	CDBG("========VPE zoom needed + DIS enabled.\n");
	/* assumption is both direction need zoom. this can be
	 improved. */
	temp = msm_io_r(vpe_device->vpebase +
					VPE_OP_MODE_OFFSET) | 0x3;
	msm_io_w(temp, vpe_device->vpebase +
			VPE_OP_MODE_OFFSET);
	zoom_dis_x = dis_offset->dis_offset_x *
		pcrop->in2_w / pcrop->out2_w;
	zoom_dis_y = dis_offset->dis_offset_y *
		pcrop->in2_h / pcrop->out2_h;

	src_x = zoom_dis_x + (pcrop->out2_w-pcrop->in2_w)/2;
	src_y = zoom_dis_y + (pcrop->out2_h-pcrop->in2_h)/2;

	out_ROI_width = vpe_ctrl->out_w;
	out_ROI_height = vpe_ctrl->out_h;

	src_ROI_width = out_ROI_width * pcrop->in2_w / pcrop->out2_w;
	src_ROI_height = out_ROI_height * pcrop->in2_h / pcrop->out2_h;

	/* clamp to output size.  This is because along
	processing, we mostly do truncation, therefore
	dis_offset tends to be
	smaller values.  The intention was to make sure that the
	offset does not exceed margin.   But in the case it could
	result src_roi bigger, due to subtract a smaller value. */
	CDBG("src w = 0x%x, h=0x%x, dst w = 0x%x, h =0x%x.\n",
		src_ROI_width, src_ROI_height, out_ROI_width,
		out_ROI_height);

	src_roi = (src_ROI_height << 16) + src_ROI_width;

	msm_io_w(src_roi, vpe_device->vpebase + VPE_SRC_SIZE_OFFSET);

	CDBG("src_x = %d, src_y=%d.\n", src_x, src_y);

	src_xy = src_y*(1<<16) + src_x;
	msm_io_w(src_xy, vpe_device->vpebase +
			VPE_SRC_XY_OFFSET);
	CDBG("src_xy = 0x%x, src_roi=0x%x.\n", src_xy, src_roi);

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

	/* if destination is only 1 pixel wide, the value of
	phase_step_x is unimportant. Assigning phase_step_x
	to src ROI width as an arbitrary value. */
	if (out_ROI_width == 1)
		phase_step_x = (uint32_t) ((src_ROI_width) <<
							SCALER_PHASE_BITS);
	else if (scale_unit_sel_x == 0) { /* if using FIR scalar */
		/* Calculate the quotient ( src_ROI_width - 1 )
		/ ( out_ROI_width - 1)with u3.29 precision.
		Quotient is rounded up to the larger
		29th decimal point. */
		numerator =
			(uint64_t)(src_ROI_width - 1) <<
			SCALER_PHASE_BITS;
		/* never equals to 0 because of the "
		(out_ROI_width == 1 )"*/
		denominator = (uint64_t)(out_ROI_width - 1);
		/* divide and round up to the larger 29th
		decimal point. */
		phase_step_x = (uint32_t) vpe_do_div(
			(numerator + denominator - 1), denominator);
	} else if (scale_unit_sel_x == 1) { /* if M/N scalar */
		/* Calculate the quotient
		( src_ROI_width ) / ( out_ROI_width)
		with u3.29 precision. Quotient is rounded
		down to the smaller 29th decimal point. */
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
	else if (scale_unit_sel_y == 0) { /* if FIR scalar */
		/* Calculate the quotient
		( src_ROI_height - 1 ) / ( out_ROI_height - 1)
		with u3.29 precision. Quotient is rounded up to the
		larger 29th decimal point. */
		numerator = (uint64_t)(src_ROI_height - 1) <<
			SCALER_PHASE_BITS;
		/* never equals to 0 because of the
		"( out_ROI_height == 1 )" case */
		denominator = (uint64_t)(out_ROI_height - 1);
		/* Quotient is rounded up to the larger 29th
		decimal point. */
		phase_step_y =
		(uint32_t) vpe_do_div(
		(numerator + denominator - 1), denominator);
	} else if (scale_unit_sel_y == 1) { /* if M/N scalar */
		/* Calculate the quotient ( src_ROI_height ) / ( out_ROI_height)
		with u3.29 precision. Quotient is rounded down to the smaller
		29th decimal point. */
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

	CDBG("phase step x = %d, step y = %d.\n",
		phase_step_x, phase_step_y);
	CDBG("phase init x = %d, init y = %d.\n",
		phase_init_x, phase_init_y);

	msm_io_w(phase_step_x, vpe_device->vpebase +
			VPE_SCALE_PHASEX_STEP_OFFSET);

	msm_io_w(phase_step_y, vpe_device->vpebase +
			VPE_SCALE_PHASEY_STEP_OFFSET);

	msm_io_w(phase_init_x, vpe_device->vpebase +
			VPE_SCALE_PHASEX_INIT_OFFSET);

	msm_io_w(phase_init_y, vpe_device->vpebase +
			VPE_SCALE_PHASEY_INIT_OFFSET);

	return 1;
}

void msm_send_frame_to_vpe(uint32_t p0_phy_add, uint32_t p1_phy_add,
		struct timespec *ts, int output_type)
{
	uint32_t temp_pyaddr = 0, temp_pcbcraddr = 0;

	CDBG("vpe input, p0_phy_add = 0x%x, p1_phy_add = 0x%x\n",
		p0_phy_add, p1_phy_add);
	msm_io_w(p0_phy_add, vpe_device->vpebase + VPE_SRCP0_ADDR_OFFSET);
	msm_io_w(p1_phy_add, vpe_device->vpebase + VPE_SRCP1_ADDR_OFFSET);

	if (vpe_ctrl->state == VPE_STATE_ACTIVE)
		CDBG(" =====VPE is busy!!!  Wrong!========\n");

	if (output_type != OUTPUT_TYPE_ST_R)
		vpe_ctrl->ts = *ts;

	if (output_type == OUTPUT_TYPE_ST_L) {
		vpe_ctrl->pcbcr_before_dis = msm_io_r(vpe_device->vpebase +
			VPE_OUTP1_ADDR_OFFSET);
		temp_pyaddr = msm_io_r(vpe_device->vpebase +
			VPE_OUTP0_ADDR_OFFSET);
		temp_pcbcraddr = temp_pyaddr + PAD_TO_2K(vpe_ctrl->out_w *
			vpe_ctrl->out_h * 2, vpe_ctrl->pad_2k_bool);
		msm_io_w(temp_pcbcraddr, vpe_device->vpebase +
			VPE_OUTP1_ADDR_OFFSET);
	}

	if (vpe_ctrl->dis_en) {
		/* Changing the VPE output CBCR address,
		to make Y/CBCR continuous */
		vpe_ctrl->pcbcr_before_dis = msm_io_r(vpe_device->vpebase +
			VPE_OUTP1_ADDR_OFFSET);
		temp_pyaddr = msm_io_r(vpe_device->vpebase +
			VPE_OUTP0_ADDR_OFFSET);
		temp_pcbcraddr = temp_pyaddr + vpe_ctrl->pcbcr_dis_offset;
		msm_io_w(temp_pcbcraddr, vpe_device->vpebase +
			VPE_OUTP1_ADDR_OFFSET);
	}

	vpe_ctrl->output_type = output_type;
	vpe_ctrl->state = VPE_STATE_ACTIVE;
	vpe_start();
}

static int vpe_proc_general(struct msm_vpe_cmd *cmd)
{
	int rc = 0;
	uint32_t *cmdp = NULL;
	struct msm_queue_cmd *qcmd = NULL;
	struct msm_vpe_buf_info *vpe_buf;
	int turbo_mode = 0;
	struct msm_sync *sync = (struct msm_sync *)vpe_ctrl->syncdata;
	CDBG("vpe_proc_general: cmdID = %s, length = %d\n",
		vpe_general_cmd[cmd->id], cmd->length);
	switch (cmd->id) {
	case VPE_ENABLE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto vpe_proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto vpe_proc_general_done;
		}
		turbo_mode = *((int *)(cmd->value));
		rc = turbo_mode ? vpe_enable(VPE_TURBO_MODE_CLOCK_RATE)
			: vpe_enable(VPE_NORMAL_MODE_CLOCK_RATE);
		break;
	case VPE_DISABLE:
		rc = vpe_disable();
		break;
	case VPE_RESET:
	case VPE_ABORT:
		rc = vpe_reset();
		break;
	case VPE_START:
		rc = vpe_start();
		break;

	case VPE_INPUT_PLANE_CFG:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto vpe_proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto vpe_proc_general_done;
		}
		vpe_input_plane_config(cmdp);
		break;

	case VPE_OPERATION_MODE_CFG:
		CDBG("cmd->length = %d \n", cmd->length);
		if (cmd->length != VPE_OPERATION_MODE_CFG_LEN) {
			rc = -EINVAL;
			goto vpe_proc_general_done;
		}
		cmdp = kmalloc(VPE_OPERATION_MODE_CFG_LEN,
					GFP_ATOMIC);
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			VPE_OPERATION_MODE_CFG_LEN)) {
			rc = -EFAULT;
			goto vpe_proc_general_done;
		}
		rc = vpe_operation_config(cmdp);
		CDBG("rc = %d \n", rc);
		break;

	case VPE_OUTPUT_PLANE_CFG:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto vpe_proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto vpe_proc_general_done;
		}
		vpe_output_plane_config(cmdp);
		break;

	case VPE_SCALE_CFG_TYPE:
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto vpe_proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto vpe_proc_general_done;
		}
		vpe_update_scale_coef(cmdp);
		break;

	case VPE_CMD_DIS_OFFSET_CFG: {
		struct msm_vfe_resp *vdata;
		/* first get the dis offset and frame id. */
		cmdp = kmalloc(cmd->length, GFP_ATOMIC);
		if (!cmdp) {
			rc = -ENOMEM;
			goto vpe_proc_general_done;
		}
		if (copy_from_user(cmdp,
			(void __user *)(cmd->value),
			cmd->length)) {
			rc = -EFAULT;
			goto vpe_proc_general_done;
		}
		/* get the offset. */
		vpe_ctrl->dis_offset = *(struct dis_offset_type *)cmdp;
		qcmd = msm_dequeue_vpe(&sync->vpe_q, list_vpe_frame);
		if (!qcmd) {
			pr_err("%s: no video frame.\n", __func__);
			kfree(cmdp);
			return -EAGAIN;
		}
		vdata = (struct msm_vfe_resp *)(qcmd->command);
		vpe_buf = &vdata->vpe_bf;
		vpe_update_scaler_with_dis(&(vpe_buf->vpe_crop),
					&(vpe_ctrl->dis_offset));

		msm_send_frame_to_vpe(vpe_buf->p0_phy, vpe_buf->p1_phy,
						&(vpe_buf->ts), OUTPUT_TYPE_V);

		if (!qcmd || !atomic_read(&qcmd->on_heap)) {
			kfree(cmdp);
			return -EAGAIN;
		}
		if (!atomic_sub_return(1, &qcmd->on_heap))
			kfree(qcmd);
		break;
	}

	default:
		break;
	}
vpe_proc_general_done:
	kfree(cmdp);
	return rc;
}

static void vpe_addr_convert(struct msm_vpe_phy_info *pinfo,
	enum vpe_resp_msg type, void *data, void **ext, int32_t *elen)
{
	CDBG("In vpe_addr_convert type = %d\n", type);
	switch (type) {
	case VPE_MSG_OUTPUT_V:
		pinfo->output_id = OUTPUT_TYPE_V;
		break;
	case VPE_MSG_OUTPUT_ST_R:
		/* output_id will be used by user space only. */
		pinfo->output_id = OUTPUT_TYPE_V;
		break;
	default:
		break;
	} /* switch */

	CDBG("In vpe_addr_convert output_id = %d\n", pinfo->output_id);

	pinfo->p0_phy =
		((struct vpe_message *)data)->_u.msgOut.p0_Buffer;
	pinfo->p1_phy =
		((struct vpe_message *)data)->_u.msgOut.p1_Buffer;
	*ext  = vpe_ctrl->extdata;
	*elen = vpe_ctrl->extlen;
}

void vpe_proc_ops(uint8_t id, void *msg, size_t len)
{
	struct msm_vpe_resp *rp;

	rp = vpe_ctrl->resp->vpe_alloc(sizeof(struct msm_vpe_resp),
		vpe_ctrl->syncdata, GFP_ATOMIC);
	if (!rp) {
		CDBG("rp: cannot allocate buffer\n");
		return;
	}

	CDBG("vpe_proc_ops, msgId = %d rp->evt_msg.msg_id = %d\n",
		id, rp->evt_msg.msg_id);
	rp->evt_msg.type   = MSM_CAMERA_MSG;
	rp->evt_msg.msg_id = id;
	rp->evt_msg.len    = len;
	rp->evt_msg.data   = msg;

	switch (rp->evt_msg.msg_id) {
	case MSG_ID_VPE_OUTPUT_V:
		rp->type = VPE_MSG_OUTPUT_V;
		vpe_addr_convert(&(rp->phy), VPE_MSG_OUTPUT_V,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case MSG_ID_VPE_OUTPUT_ST_R:
		rp->type = VPE_MSG_OUTPUT_ST_R;
		vpe_addr_convert(&(rp->phy), VPE_MSG_OUTPUT_ST_R,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case MSG_ID_VPE_OUTPUT_ST_L:
		rp->type = VPE_MSG_OUTPUT_ST_L;
		break;

	default:
		rp->type = VPE_MSG_GENERAL;
		break;
	}
	CDBG("%s: time = %ld\n",
			__func__, vpe_ctrl->ts.tv_nsec);

	vpe_ctrl->resp->vpe_resp(rp, MSM_CAM_Q_VPE_MSG,
					vpe_ctrl->syncdata,
					&(vpe_ctrl->ts), GFP_ATOMIC);
}

int vpe_config_axi(struct axidata *ad)
{
	uint32_t p1;
	struct msm_pmem_region *regp1 = NULL;
	CDBG("vpe_config_axi:bufnum1 = %d.\n", ad->bufnum1);

	if (ad->bufnum1 != 1)
		return -EINVAL;

	regp1 = &(ad->region[0]);
	/* for video  Y address */
	p1 = (regp1->paddr + regp1->info.planar0_off);
	msm_io_w(p1, vpe_device->vpebase + VPE_OUTP0_ADDR_OFFSET);
	/* for video  CbCr address */
	p1 = (regp1->paddr + regp1->info.planar1_off);
	msm_io_w(p1, vpe_device->vpebase + VPE_OUTP1_ADDR_OFFSET);

	return 0;
}

int msm_vpe_config(struct msm_vpe_cfg_cmd *cmd, void *data)
{
	struct msm_vpe_cmd vpecmd;
	int rc = 0;
	if (copy_from_user(&vpecmd,
			(void __user *)(cmd->value),
			sizeof(vpecmd))) {
		pr_err("%s %d: copy_from_user failed\n", __func__,
				__LINE__);
		return -EFAULT;
	}
	CDBG("%s: cmd_type %d\n", __func__, cmd->cmd_type);
	switch (cmd->cmd_type) {
	case CMD_VPE:
		rc = vpe_proc_general(&vpecmd);
		CDBG(" rc = %d\n", rc);
		break;

	case CMD_AXI_CFG_VPE:
	case CMD_AXI_CFG_SNAP_VPE:
	case CMD_AXI_CFG_SNAP_THUMB_VPE: {
		struct axidata *axid;
		axid = data;
		if (!axid)
			return -EFAULT;
		vpe_config_axi(axid);
		break;
	}
	default:
		break;
	}
	CDBG("%s: rc = %d\n", __func__, rc);
	return rc;
}

void msm_vpe_offset_update(int frame_pack, uint32_t pyaddr, uint32_t pcbcraddr,
	struct timespec *ts, int output_id, struct msm_st_half st_half,
	int frameid)
{
	struct msm_vpe_buf_info vpe_buf;
	uint32_t input_stride;

	vpe_buf.vpe_crop.in2_w = st_half.stCropInfo.in_w;
	vpe_buf.vpe_crop.in2_h = st_half.stCropInfo.in_h;
	vpe_buf.vpe_crop.out2_w = st_half.stCropInfo.out_w;
	vpe_buf.vpe_crop.out2_h = st_half.stCropInfo.out_h;
	vpe_ctrl->dis_offset.dis_offset_x = st_half.pix_x_off;
	vpe_ctrl->dis_offset.dis_offset_y = st_half.pix_y_off;
	vpe_ctrl->dis_offset.frame_id = frameid;
	vpe_ctrl->frame_pack = frame_pack;
	vpe_ctrl->output_type = output_id;

	input_stride = (st_half.buf_p1_stride * (1<<16)) +
		st_half.buf_p0_stride;

	msm_io_w(input_stride, vpe_device->vpebase + VPE_SRC_YSTRIDE1_OFFSET);

	vpe_update_scaler_with_dis(&(vpe_buf.vpe_crop),
		&(vpe_ctrl->dis_offset));

	msm_send_frame_to_vpe(pyaddr, pcbcraddr, ts, output_id);
}

static void vpe_send_outmsg(uint8_t msgid, uint32_t p0_addr,
	uint32_t p1_addr, uint32_t p2_addr)
{
	struct vpe_message msg;
	uint8_t outid;
	msg._d = outid = msgid;
	msg._u.msgOut.output_id   = msgid;
	msg._u.msgOut.p0_Buffer = p0_addr;
	msg._u.msgOut.p1_Buffer = p1_addr;
	msg._u.msgOut.p2_Buffer = p2_addr;
	vpe_proc_ops(outid, &msg, sizeof(struct vpe_message));
	return;
}

int msm_vpe_reg(struct msm_vpe_callback *presp)
{
	if (presp && presp->vpe_resp)
		vpe_ctrl->resp = presp;

	return 0;
}

static void vpe_send_msg_no_payload(enum VPE_MESSAGE_ID id)
{
	struct vpe_message msg;

	CDBG("vfe31_send_msg_no_payload\n");
	msg._d = id;
	vpe_proc_ops(id, &msg, 0);
}

static void vpe_do_tasklet(unsigned long data)
{
	unsigned long flags;
	uint32_t pyaddr = 0, pcbcraddr = 0;
	uint32_t src_y, src_cbcr, temp;

	struct vpe_isr_queue_cmd_type *qcmd = NULL;

	CDBG("=== vpe_do_tasklet start === \n");

	spin_lock_irqsave(&vpe_ctrl->tasklet_lock, flags);
	qcmd = list_first_entry(&vpe_ctrl->tasklet_q,
		struct vpe_isr_queue_cmd_type, list);

	if (!qcmd) {
		spin_unlock_irqrestore(&vpe_ctrl->tasklet_lock, flags);
		return;
	}

	list_del(&qcmd->list);
	spin_unlock_irqrestore(&vpe_ctrl->tasklet_lock, flags);

	/* interrupt to be processed,  *qcmd has the payload.  */
	if (qcmd->irq_status & 0x1) {
		if (vpe_ctrl->output_type == OUTPUT_TYPE_ST_L) {
			CDBG("vpe left frame done.\n");
			vpe_ctrl->output_type = 0;
			CDBG("vpe send out msg.\n");
			orig_src_y = msm_io_r(vpe_device->vpebase +
				VPE_SRCP0_ADDR_OFFSET);
			orig_src_cbcr = msm_io_r(vpe_device->vpebase +
				VPE_SRCP1_ADDR_OFFSET);

			pyaddr = msm_io_r(vpe_device->vpebase +
				VPE_OUTP0_ADDR_OFFSET);
			pcbcraddr = msm_io_r(vpe_device->vpebase +
				VPE_OUTP1_ADDR_OFFSET);
			CDBG("%s: out_w = %d, out_h = %d\n", __func__,
				vpe_ctrl->out_w, vpe_ctrl->out_h);

			if ((vpe_ctrl->frame_pack == TOP_DOWN_FULL) ||
				(vpe_ctrl->frame_pack == TOP_DOWN_HALF)) {
				msm_io_w(pyaddr + (vpe_ctrl->out_w *
					vpe_ctrl->out_h), vpe_device->vpebase +
					VPE_OUTP0_ADDR_OFFSET);
				msm_io_w(pcbcraddr + (vpe_ctrl->out_w *
					vpe_ctrl->out_h/2),
					vpe_device->vpebase +
					VPE_OUTP1_ADDR_OFFSET);
			} else if ((vpe_ctrl->frame_pack ==
				SIDE_BY_SIDE_HALF) || (vpe_ctrl->frame_pack ==
				SIDE_BY_SIDE_FULL)) {
				msm_io_w(pyaddr + vpe_ctrl->out_w,
					vpe_device->vpebase +
					VPE_OUTP0_ADDR_OFFSET);
				msm_io_w(pcbcraddr + vpe_ctrl->out_w,
					vpe_device->vpebase +
					VPE_OUTP1_ADDR_OFFSET);
			} else
				CDBG("%s: Invalid packing = %d\n", __func__,
					vpe_ctrl->frame_pack);

			vpe_send_msg_no_payload(MSG_ID_VPE_OUTPUT_ST_L);
			vpe_ctrl->state = VPE_STATE_INIT;
			kfree(qcmd);
			return;
		} else if (vpe_ctrl->output_type == OUTPUT_TYPE_ST_R) {
			src_y = orig_src_y;
			src_cbcr = orig_src_cbcr;
			CDBG("%s: out_w = %d, out_h = %d\n", __func__,
				vpe_ctrl->out_w, vpe_ctrl->out_h);

			if ((vpe_ctrl->frame_pack == TOP_DOWN_FULL) ||
				(vpe_ctrl->frame_pack == TOP_DOWN_HALF)) {
				pyaddr = msm_io_r(vpe_device->vpebase +
					VPE_OUTP0_ADDR_OFFSET) -
					(vpe_ctrl->out_w * vpe_ctrl->out_h);
			} else if ((vpe_ctrl->frame_pack ==
				SIDE_BY_SIDE_HALF) || (vpe_ctrl->frame_pack ==
				SIDE_BY_SIDE_FULL)) {
				pyaddr = msm_io_r(vpe_device->vpebase +
				VPE_OUTP0_ADDR_OFFSET) - vpe_ctrl->out_w;
			} else
				CDBG("%s: Invalid packing = %d\n", __func__,
					vpe_ctrl->frame_pack);

			pcbcraddr = vpe_ctrl->pcbcr_before_dis;
		} else {
			src_y =	msm_io_r(vpe_device->vpebase +
				VPE_SRCP0_ADDR_OFFSET);
			src_cbcr = msm_io_r(vpe_device->vpebase +
				VPE_SRCP1_ADDR_OFFSET);
			pyaddr = msm_io_r(vpe_device->vpebase +
				VPE_OUTP0_ADDR_OFFSET);
			pcbcraddr = msm_io_r(vpe_device->vpebase +
				VPE_OUTP1_ADDR_OFFSET);
		}

		if (vpe_ctrl->dis_en)
			pcbcraddr = vpe_ctrl->pcbcr_before_dis;

		msm_io_w(src_y,
				vpe_device->vpebase + VPE_OUTP0_ADDR_OFFSET);
		msm_io_w(src_cbcr,
				vpe_device->vpebase + VPE_OUTP1_ADDR_OFFSET);

		temp = msm_io_r(vpe_device->vpebase + VPE_OP_MODE_OFFSET) &
			0xFFFFFFFC;
		msm_io_w(temp, vpe_device->vpebase + VPE_OP_MODE_OFFSET);

		/*  now pass this frame to msm_camera.c. */
		if (vpe_ctrl->output_type == OUTPUT_TYPE_ST_R) {
			CDBG("vpe send out R msg.\n");
			vpe_send_outmsg(MSG_ID_VPE_OUTPUT_ST_R, pyaddr,
				pcbcraddr, pyaddr);
		} else if (vpe_ctrl->output_type == OUTPUT_TYPE_V) {
			CDBG("vpe send out V msg.\n");
			vpe_send_outmsg(MSG_ID_VPE_OUTPUT_V, pyaddr,
				pcbcraddr, pyaddr);
		}

		vpe_ctrl->output_type = 0;
		vpe_ctrl->state = VPE_STATE_INIT;   /* put it back to idle. */

	}
	kfree(qcmd);
}
DECLARE_TASKLET(vpe_tasklet, vpe_do_tasklet, 0);

static irqreturn_t vpe_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	uint32_t irq_status = 0;
	struct vpe_isr_queue_cmd_type *qcmd;

	CDBG("vpe_parse_irq.\n");
	/* read and clear back-to-back. */
	irq_status = msm_io_r_mb(vpe_device->vpebase +
							VPE_INTR_STATUS_OFFSET);
	msm_io_w_mb(irq_status, vpe_device->vpebase +
				VPE_INTR_CLEAR_OFFSET);

	msm_io_w(0, vpe_device->vpebase + VPE_INTR_ENABLE_OFFSET);

	if (irq_status == 0) {
		pr_err("%s: irq_status = 0,Something is wrong!\n", __func__);
		return IRQ_HANDLED;
	}
	irq_status &= 0x1;
	/* apply mask. only interested in bit 0.  */
	if (irq_status) {
		qcmd = kzalloc(sizeof(struct vpe_isr_queue_cmd_type),
			GFP_ATOMIC);
		if (!qcmd) {
			pr_err("%s: qcmd malloc failed!\n", __func__);
			return IRQ_HANDLED;
		}
		/* must be 0x1 now. so in bottom half we don't really
		need to check. */
		qcmd->irq_status = irq_status & 0x1;
		spin_lock_irqsave(&vpe_ctrl->tasklet_lock, flags);
		list_add_tail(&qcmd->list, &vpe_ctrl->tasklet_q);
		spin_unlock_irqrestore(&vpe_ctrl->tasklet_lock, flags);
		tasklet_schedule(&vpe_tasklet);
	}
	return IRQ_HANDLED;
}

static int vpe_enable_irq(void)
{
	uint32_t   rc = 0;
	rc = request_irq(vpe_device->vpeirq,
				vpe_parse_irq,
				IRQF_TRIGGER_HIGH, "vpe", 0);
	return rc;
}

int msm_vpe_open(void)
{
	int rc = 0;

	CDBG("%s: In \n", __func__);

	vpe_ctrl = kzalloc(sizeof(struct vpe_ctrl_type), GFP_KERNEL);
	if (!vpe_ctrl) {
		pr_err("%s: no memory!\n", __func__);
		return -ENOMEM;
	}

	spin_lock_init(&vpe_ctrl->ops_lock);
	CDBG("%s: Out\n", __func__);

	return rc;
}

int msm_vpe_release(void)
{
	/* clean up....*/
	int rc = 0;
	CDBG("%s: state %d\n", __func__, vpe_ctrl->state);
	if (vpe_ctrl->state != VPE_STATE_IDLE)
		rc = vpe_disable();

	kfree(vpe_ctrl);
	return rc;
}


int vpe_enable(uint32_t clk_rate)
{
	int rc = 0;
	unsigned long flags = 0;
	/* don't change the order of clock and irq.*/
	CDBG("%s: enable_clock rate %u\n", __func__, clk_rate);
	spin_lock_irqsave(&vpe_ctrl->ops_lock, flags);
	if (vpe_ctrl->state != VPE_STATE_IDLE) {
		CDBG("%s: VPE already enabled", __func__);
		spin_unlock_irqrestore(&vpe_ctrl->ops_lock, flags);
		return 0;
	}
	vpe_ctrl->state = VPE_STATE_INIT;
	spin_unlock_irqrestore(&vpe_ctrl->ops_lock, flags);

	rc = msm_camio_vpe_clk_enable(clk_rate);
	if (rc < 0) {
		pr_err("%s: msm_camio_vpe_clk_enable failed", __func__);
		vpe_ctrl->state = VPE_STATE_IDLE;
		return rc;
	}

	CDBG("%s: enable_irq\n", __func__);
	vpe_enable_irq();

	/* initialize the data structure - lock, queue etc. */
	spin_lock_init(&vpe_ctrl->tasklet_lock);
	INIT_LIST_HEAD(&vpe_ctrl->tasklet_q);

	return rc;
}

int vpe_disable(void)
{
	int rc = 0;
	unsigned long flags = 0;
	CDBG("%s: called", __func__);
	spin_lock_irqsave(&vpe_ctrl->ops_lock, flags);
	if (vpe_ctrl->state == VPE_STATE_IDLE) {
		CDBG("%s: VPE already disabled", __func__);
		spin_unlock_irqrestore(&vpe_ctrl->ops_lock, flags);
		return 0;
	}
	vpe_ctrl->state = VPE_STATE_IDLE;
	spin_unlock_irqrestore(&vpe_ctrl->ops_lock, flags);
	vpe_ctrl->out_y_addr = msm_io_r(vpe_device->vpebase +
		VPE_OUTP0_ADDR_OFFSET);
	vpe_ctrl->out_cbcr_addr = msm_io_r(vpe_device->vpebase +
		VPE_OUTP1_ADDR_OFFSET);
	free_irq(vpe_device->vpeirq, 0);
	tasklet_kill(&vpe_tasklet);
	rc = msm_camio_vpe_clk_disable();
	return rc;
}

static int __msm_vpe_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource   *vpemem, *vpeirq, *vpeio;
	void __iomem      *vpebase;

	/* first allocate */

	vpe_device = &vpe_device_data;
	memset(vpe_device, 0, sizeof(struct vpe_device_type));

	/* does the device exist? */
	vpeirq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!vpeirq) {
		pr_err("%s: no vpe irq resource.\n", __func__);
		rc = -ENODEV;
		goto vpe_free_device;
	}
	vpemem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!vpemem) {
		pr_err("%s: no vpe mem resource!\n", __func__);
		rc = -ENODEV;
		goto vpe_free_device;
	}
	vpeio = request_mem_region(vpemem->start,
			resource_size(vpemem), pdev->name);
	if (!vpeio) {
		pr_err("%s: VPE region already claimed.\n", __func__);
		rc = -EBUSY;
		goto vpe_free_device;
	}

	vpebase =
		ioremap(vpemem->start,
				(vpemem->end - vpemem->start) + 1);
	if (!vpebase) {
		pr_err("%s: vpe ioremap failed.\n", __func__);
		rc = -ENOMEM;
		goto vpe_release_mem_region;
	}

	/* Fall through, _probe is successful. */
	vpe_device->vpeirq = vpeirq->start;
	vpe_device->vpemem = vpemem;
	vpe_device->vpeio = vpeio;
	vpe_device->vpebase = vpebase;
	return rc;  /* this rc should be zero.*/

	iounmap(vpe_device->vpebase);  /* this path should never occur */

/* from this part it is error handling. */
vpe_release_mem_region:
	release_mem_region(vpemem->start, (vpemem->end - vpemem->start) + 1);
vpe_free_device:
	return rc;  /* this rc should have error code. */
}

static int __msm_vpe_remove(struct platform_device *pdev)
{
	struct resource	*vpemem;
	vpemem = vpe_device->vpemem;

	iounmap(vpe_device->vpebase);
	release_mem_region(vpemem->start,
					(vpemem->end - vpemem->start) + 1);
	return 0;
}

static struct platform_driver msm_vpe_driver = {
	.probe = __msm_vpe_probe,
	.remove = __msm_vpe_remove,
	.driver = {
		.name = "msm_vpe",
		.owner = THIS_MODULE,
	},
};

static int __init msm_vpe_init(void)
{
	return platform_driver_register(&msm_vpe_driver);
}
module_init(msm_vpe_init);

static void __exit msm_vpe_exit(void)
{
	platform_driver_unregister(&msm_vpe_driver);
}
module_exit(msm_vpe_exit);

MODULE_DESCRIPTION("msm vpe 1.0 driver");
MODULE_VERSION("msm vpe driver 1.0");
MODULE_LICENSE("GPL v2");
