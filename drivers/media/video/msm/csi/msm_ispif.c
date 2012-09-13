/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "msm_ispif.h"
#include "msm.h"
#include "msm_ispif_hwreg.h"

#define V4L2_IDENT_ISPIF                     50001
#define CSID_VERSION_V2                      0x02000011
#define CSID_VERSION_V3                      0x30000000

#define MAX_CID 15

static atomic_t ispif_irq_cnt;
static spinlock_t ispif_tasklet_lock;
static struct list_head ispif_tasklet_q;

static int msm_ispif_intf_reset(struct ispif_device *ispif,
	uint16_t intfmask, uint8_t vfe_intf)
{
	int rc = 0;
	uint32_t data = (0x1 << STROBED_RST_EN);
	uint16_t intfnum = 0, mask = intfmask;

	while (mask != 0) {
		if (!(intfmask & (0x1 << intfnum))) {
			mask >>= 1;
			intfnum++;
			continue;
		}
		switch (intfnum) {
		case PIX0:
			data |= (0x1 << PIX_0_VFE_RST_STB) |
				(0x1 << PIX_0_CSID_RST_STB);
			ispif->pix_sof_count = 0;
			break;

		case RDI0:
			data |= (0x1 << RDI_0_VFE_RST_STB) |
				(0x1 << RDI_0_CSID_RST_STB);
			break;

		case PIX1:
			data |= (0x1 << PIX_1_VFE_RST_STB) |
				(0x1 << PIX_1_CSID_RST_STB);
			break;

		case RDI1:
			data |= (0x1 << RDI_1_VFE_RST_STB) |
				(0x1 << RDI_1_CSID_RST_STB);
			break;

		case RDI2:
			data |= (0x1 << RDI_2_VFE_RST_STB) |
				(0x1 << RDI_2_CSID_RST_STB);
			break;

		default:
			rc = -EINVAL;
			break;
		}
		mask >>= 1;
		intfnum++;
	}	/*end while */
	if (data > 0x1) {
		if (vfe_intf == VFE0)
			msm_camera_io_w(data, ispif->base + ISPIF_RST_CMD_ADDR);
		else
			msm_camera_io_w(data, ispif->base +
				ISPIF_RST_CMD_1_ADDR);
		rc = wait_for_completion_interruptible(&ispif->reset_complete);
	}
	return rc;
}

static int msm_ispif_reset(struct ispif_device *ispif)
{
	int rc = 0;
	ispif->pix_sof_count = 0;
	msm_camera_io_w(ISPIF_RST_CMD_MASK, ispif->base + ISPIF_RST_CMD_ADDR);
	if (ispif->csid_version == CSID_VERSION_V3)
		msm_camera_io_w(ISPIF_RST_CMD_1_MASK, ispif->base +
				ISPIF_RST_CMD_1_ADDR);
	rc = wait_for_completion_interruptible(&ispif->reset_complete);
	return rc;
}

static int msm_ispif_subdev_g_chip_ident(struct v4l2_subdev *sd,
			struct v4l2_dbg_chip_ident *chip)
{
	BUG_ON(!chip);
	chip->ident = V4L2_IDENT_ISPIF;
	chip->revision = 0;
	return 0;
}

static void msm_ispif_sel_csid_core(struct ispif_device *ispif,
	uint8_t intftype, uint8_t csid, uint8_t vfe_intf)
{
	int rc = 0;
	uint32_t data = 0;

	if (ispif->csid_version <= CSID_VERSION_V2) {
		if (ispif->ispif_clk[intftype] == NULL) {
			pr_err("%s: ispif NULL clk\n", __func__);
			return;
		}
		rc = clk_set_rate(ispif->ispif_clk[intftype], csid);
		if (rc < 0)
			pr_err("%s: clk_set_rate failed %d\n", __func__, rc);
	}
	data = msm_camera_io_r(ispif->base + ISPIF_INPUT_SEL_ADDR +
		(0x200 * vfe_intf));
	switch (intftype) {
	case PIX0:
		data &= ~(0x3);
		data |= csid;
		break;

	case RDI0:
		data &= ~(0x3 << 4);
		data |= (csid << 4);
		break;

	case PIX1:
		data &= ~(0x3 << 8);
		data |= (csid << 8);
		break;

	case RDI1:
		data &= ~(0x3 << 12);
		data |= (csid << 12);
		break;

	case RDI2:
		data &= ~(0x3 << 20);
		data |= (csid << 20);
		break;
	}
	if (data) {
		msm_camera_io_w(data, ispif->base + ISPIF_INPUT_SEL_ADDR +
			(0x200 * vfe_intf));
	}
}

static void msm_ispif_enable_intf_cids(struct ispif_device *ispif,
	uint8_t intftype, uint16_t cid_mask, uint8_t vfe_intf)
{
	uint32_t data = 0;
	mutex_lock(&ispif->mutex);
	switch (intftype) {
	case PIX0:
		data = msm_camera_io_r(ispif->base +
			ISPIF_PIX_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		data |= cid_mask;
		msm_camera_io_w(data, ispif->base +
			ISPIF_PIX_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case RDI0:
		data = msm_camera_io_r(ispif->base +
			ISPIF_RDI_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		data |= cid_mask;
		msm_camera_io_w(data, ispif->base +
			ISPIF_RDI_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case PIX1:
		data = msm_camera_io_r(ispif->base +
			ISPIF_PIX_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		data |= cid_mask;
		msm_camera_io_w(data, ispif->base +
			ISPIF_PIX_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case RDI1:
		data = msm_camera_io_r(ispif->base +
			ISPIF_RDI_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		data |= cid_mask;
		msm_camera_io_w(data, ispif->base +
			ISPIF_RDI_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case RDI2:
		data = msm_camera_io_r(ispif->base +
			ISPIF_RDI_2_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		data |= cid_mask;
		msm_camera_io_w(data, ispif->base +
			ISPIF_RDI_2_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;
	}
	mutex_unlock(&ispif->mutex);
}

static int32_t msm_ispif_validate_intf_status(struct ispif_device *ispif,
	uint8_t intftype, uint8_t vfe_intf)
{
	int32_t rc = 0;
	uint32_t data = 0;
	mutex_lock(&ispif->mutex);
	switch (intftype) {
	case PIX0:
		data = msm_camera_io_r(ispif->base +
				ISPIF_PIX_0_STATUS_ADDR + (0x200 * vfe_intf));
		break;

	case RDI0:
		data = msm_camera_io_r(ispif->base +
				ISPIF_RDI_0_STATUS_ADDR + (0x200 * vfe_intf));
		break;

	case PIX1:
		data = msm_camera_io_r(ispif->base +
				ISPIF_PIX_1_STATUS_ADDR + (0x200 * vfe_intf));
		break;

	case RDI1:
		data = msm_camera_io_r(ispif->base +
				ISPIF_RDI_1_STATUS_ADDR + (0x200 * vfe_intf));
		break;

	case RDI2:
		data = msm_camera_io_r(ispif->base +
				ISPIF_RDI_2_STATUS_ADDR + (0x200 * vfe_intf));
		break;
	}
	if ((data & 0xf) != 0xf)
		rc = -EBUSY;
	mutex_unlock(&ispif->mutex);
	return rc;
}

static int msm_ispif_config(struct ispif_device *ispif,
	struct msm_ispif_params_list *params_list)
{
	uint32_t params_len;
	struct msm_ispif_params *ispif_params;
	int rc = 0, i = 0;
	uint8_t intftype;
	uint8_t vfe_intf;
	params_len = params_list->len;
	ispif_params = params_list->params;
	CDBG("Enable interface\n");
	msm_camera_io_w(0x00000000, ispif->base + ISPIF_IRQ_MASK_ADDR);
	msm_camera_io_w(0x00000000, ispif->base + ISPIF_IRQ_MASK_1_ADDR);
	msm_camera_io_w(0x00000000, ispif->base + ISPIF_IRQ_MASK_2_ADDR);
	for (i = 0; i < params_len; i++) {
		intftype = ispif_params[i].intftype;
		vfe_intf = ispif_params[i].vfe_intf;
		CDBG("%s intftype %x, vfe_intf %d, csid %d\n", __func__,
			intftype, vfe_intf, ispif_params[i].csid);
		if ((intftype >= INTF_MAX) ||
			(ispif->csid_version <= CSID_VERSION_V2 &&
			vfe_intf > VFE0) ||
			(ispif->csid_version == CSID_VERSION_V3 &&
			vfe_intf >= VFE_MAX)) {
			pr_err("%s: intftype / vfe intf not valid\n",
				__func__);
			return -EINVAL;
		}

		rc = msm_ispif_validate_intf_status(ispif, intftype, vfe_intf);
		if (rc < 0) {
			pr_err("%s:%d failed rc %d\n", __func__, __LINE__, rc);
			return rc;
		}
		msm_ispif_sel_csid_core(ispif, intftype, ispif_params[i].csid,
			vfe_intf);
		msm_ispif_enable_intf_cids(ispif, intftype,
			ispif_params[i].cid_mask, vfe_intf);
	}

	msm_camera_io_w(ISPIF_IRQ_STATUS_MASK, ispif->base +
					ISPIF_IRQ_MASK_ADDR);
	msm_camera_io_w(ISPIF_IRQ_STATUS_MASK, ispif->base +
					ISPIF_IRQ_CLEAR_ADDR);
	msm_camera_io_w(ISPIF_IRQ_STATUS_1_MASK, ispif->base +
					ISPIF_IRQ_MASK_1_ADDR);
	msm_camera_io_w(ISPIF_IRQ_STATUS_1_MASK, ispif->base +
					ISPIF_IRQ_CLEAR_1_ADDR);
	msm_camera_io_w(ISPIF_IRQ_STATUS_2_MASK, ispif->base +
					ISPIF_IRQ_MASK_2_ADDR);
	msm_camera_io_w(ISPIF_IRQ_STATUS_2_MASK, ispif->base +
					ISPIF_IRQ_CLEAR_2_ADDR);
	msm_camera_io_w(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
		 ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);
	return rc;
}

static uint32_t msm_ispif_get_cid_mask(struct ispif_device *ispif,
	uint16_t intftype, uint8_t vfe_intf)
{
	uint32_t mask = 0;
	switch (intftype) {
	case PIX0:
		mask = msm_camera_io_r(ispif->base +
			ISPIF_PIX_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case RDI0:
		mask = msm_camera_io_r(ispif->base +
			ISPIF_RDI_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case PIX1:
		mask = msm_camera_io_r(ispif->base +
			ISPIF_PIX_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case RDI1:
		mask = msm_camera_io_r(ispif->base +
			ISPIF_RDI_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	case RDI2:
		mask = msm_camera_io_r(ispif->base +
			ISPIF_RDI_2_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;

	default:
		break;
	}
	return mask;
}

static void msm_ispif_intf_cmd(struct ispif_device *ispif, uint16_t intfmask,
	uint8_t intf_cmd_mask, uint8_t vfe_intf)
{
	uint8_t vc = 0, val = 0;
	uint16_t mask = intfmask, intfnum = 0;
	uint32_t cid_mask = 0;
	uint32_t global_intf_cmd_mask1 = 0xFFFFFFFF;
	while (mask != 0) {
		if (!(intfmask & (0x1 << intfnum))) {
			mask >>= 1;
			intfnum++;
			continue;
		}

		cid_mask = msm_ispif_get_cid_mask(ispif, intfnum, vfe_intf);
		vc = 0;

		while (cid_mask != 0) {
			if ((cid_mask & 0xf) != 0x0) {
				if (intfnum != RDI2) {
					val = (intf_cmd_mask>>(vc*2)) & 0x3;
					ispif->global_intf_cmd_mask |=
						(0x3 << ((vc * 2) +
						(intfnum * 8)));
					ispif->global_intf_cmd_mask &=
						~((0x3 & ~val) << ((vc * 2) +
						(intfnum * 8)));
				} else
					global_intf_cmd_mask1 &=
						~((0x3 & ~intf_cmd_mask)
						<< ((vc * 2) + 8));
			}
			vc++;
			cid_mask >>= 4;
		}
		mask >>= 1;
		intfnum++;
	}
	msm_camera_io_w(ispif->global_intf_cmd_mask,
		ispif->base + ISPIF_INTF_CMD_ADDR + (0x200 * vfe_intf));
	if (global_intf_cmd_mask1 != 0xFFFFFFFF)
		msm_camera_io_w(global_intf_cmd_mask1,
			ispif->base + ISPIF_INTF_CMD_1_ADDR +
			(0x200 * vfe_intf));
}

static int msm_ispif_abort_intf_transfer(struct ispif_device *ispif,
	uint16_t intfmask, uint8_t vfe_intf)
{
	int rc = 0;
	uint8_t intf_cmd_mask = 0xAA;
	uint16_t intfnum = 0, mask = intfmask;
	mutex_lock(&ispif->mutex);
	CDBG("%s intfmask %x intf_cmd_mask %x\n", __func__, intfmask,
		intf_cmd_mask);
	msm_ispif_intf_cmd(ispif, intfmask, intf_cmd_mask, vfe_intf);
	while (mask != 0) {
		if (intfmask & (0x1 << intfnum))
			ispif->global_intf_cmd_mask |= (0xFF << (intfnum * 8));
		mask >>= 1;
		intfnum++;
		if (intfnum == RDI2)
			break;
	}
	mutex_unlock(&ispif->mutex);
	return rc;
}

static int msm_ispif_start_intf_transfer(struct ispif_device *ispif,
	uint16_t intfmask, uint8_t vfe_intf)
{
	uint8_t intf_cmd_mask = 0x55;
	int rc = 0;
	mutex_lock(&ispif->mutex);
	rc = msm_ispif_intf_reset(ispif, intfmask, vfe_intf);
	CDBG("%s intfmask start after%x intf_cmd_mask %x\n", __func__, intfmask,
		intf_cmd_mask);
	msm_ispif_intf_cmd(ispif, intfmask, intf_cmd_mask, vfe_intf);
	mutex_unlock(&ispif->mutex);
	return rc;
}

static int msm_ispif_stop_intf_transfer(struct ispif_device *ispif,
	uint16_t intfmask, uint8_t vfe_intf)
{
	int rc = 0;
	uint8_t intf_cmd_mask = 0x00;
	uint16_t intfnum = 0, mask = intfmask;
	mutex_lock(&ispif->mutex);
	CDBG("%s intfmask %x intf_cmd_mask %x\n", __func__, intfmask,
		intf_cmd_mask);
	msm_ispif_intf_cmd(ispif, intfmask, intf_cmd_mask, vfe_intf);
	while (mask != 0) {
		if (intfmask & (0x1 << intfnum)) {
			switch (intfnum) {
			case PIX0:
				while ((msm_camera_io_r(ispif->base +
					ISPIF_PIX_0_STATUS_ADDR +
					(0x200 * vfe_intf))
					& 0xf) != 0xf) {
					CDBG("Wait for pix0 Idle\n");
				}
				break;

			case RDI0:
				while ((msm_camera_io_r(ispif->base +
					ISPIF_RDI_0_STATUS_ADDR +
					(0x200 * vfe_intf))
					& 0xf) != 0xf) {
					CDBG("Wait for rdi0 Idle\n");
				}
				break;

			case PIX1:
				while ((msm_camera_io_r(ispif->base +
					ISPIF_PIX_1_STATUS_ADDR +
					(0x200 * vfe_intf))
					& 0xf) != 0xf) {
					CDBG("Wait for pix1 Idle\n");
				}
				break;

			case RDI1:
				while ((msm_camera_io_r(ispif->base +
					ISPIF_RDI_1_STATUS_ADDR +
					(0x200 * vfe_intf))
					& 0xf) != 0xf) {
					CDBG("Wait for rdi1 Idle\n");
				}
				break;

			case RDI2:
				while ((msm_camera_io_r(ispif->base +
					ISPIF_RDI_2_STATUS_ADDR +
					(0x200 * vfe_intf))
					& 0xf) != 0xf) {
					CDBG("Wait for rdi2 Idle\n");
				}
				break;

			default:
				break;
			}
			if (intfnum != RDI2)
				ispif->global_intf_cmd_mask |= (0xFF <<
					(intfnum * 8));
		}
		mask >>= 1;
		intfnum++;
	}
	mutex_unlock(&ispif->mutex);
	return rc;
}

static int msm_ispif_subdev_video_s_stream(struct v4l2_subdev *sd,
	int enable)
{
	struct ispif_device *ispif =
			(struct ispif_device *)v4l2_get_subdevdata(sd);
	uint32_t cmd = enable & ((1<<ISPIF_S_STREAM_SHIFT)-1);
	uint16_t intf = enable >> ISPIF_S_STREAM_SHIFT;
	uint8_t vfe_intf = enable >> ISPIF_VFE_INTF_SHIFT;
	int rc = -EINVAL;
	CDBG("%s enable %x, cmd %x, intf %x\n", __func__, enable, cmd, intf);
	BUG_ON(!ispif);
	if ((ispif->csid_version <= CSID_VERSION_V2 && vfe_intf > VFE0) ||
		(ispif->csid_version == CSID_VERSION_V3 &&
		vfe_intf >= VFE_MAX)) {
		pr_err("%s invalid csid version %x && vfe intf %d\n", __func__,
			ispif->csid_version, vfe_intf);
		return rc;
	}
	switch (cmd) {
	case ISPIF_ON_FRAME_BOUNDARY:
		rc = msm_ispif_start_intf_transfer(ispif, intf, vfe_intf);
		break;
	case ISPIF_OFF_FRAME_BOUNDARY:
		rc = msm_ispif_stop_intf_transfer(ispif, intf, vfe_intf);
		break;
	case ISPIF_OFF_IMMEDIATELY:
		rc = msm_ispif_abort_intf_transfer(ispif, intf, vfe_intf);
		break;
	default:
		break;
	}
	return rc;
}

static void send_rdi_sof(struct ispif_device *ispif,
	enum msm_ispif_intftype interface, int count)
{
	struct rdi_count_msg sof_msg;
	sof_msg.rdi_interface = interface;
	sof_msg.count = count;
	v4l2_subdev_notify(&ispif->subdev, NOTIFY_AXI_RDI_SOF_COUNT,
					   (void *)&sof_msg);
}

static void ispif_do_tasklet(unsigned long data)
{
	unsigned long flags;

	struct ispif_isr_queue_cmd *qcmd = NULL;
	struct ispif_device *ispif;

	ispif = (struct ispif_device *)data;
	while (atomic_read(&ispif_irq_cnt)) {
		spin_lock_irqsave(&ispif_tasklet_lock, flags);
		qcmd = list_first_entry(&ispif_tasklet_q,
			struct ispif_isr_queue_cmd, list);
		atomic_sub(1, &ispif_irq_cnt);

		if (!qcmd) {
			spin_unlock_irqrestore(&ispif_tasklet_lock,
				flags);
			return;
		}
		list_del(&qcmd->list);
		spin_unlock_irqrestore(&ispif_tasklet_lock,
			flags);

		kfree(qcmd);
	}
}

static void ispif_process_irq(struct ispif_device *ispif,
	struct ispif_irq_status *out)
{
	unsigned long flags;
	struct ispif_isr_queue_cmd *qcmd;

	qcmd = kzalloc(sizeof(struct ispif_isr_queue_cmd),
		GFP_ATOMIC);
	if (!qcmd) {
		pr_err("ispif_process_irq: qcmd malloc failed!\n");
		return;
	}
	qcmd->ispifInterruptStatus0 = out->ispifIrqStatus0;
	qcmd->ispifInterruptStatus1 = out->ispifIrqStatus1;
	qcmd->ispifInterruptStatus2 = out->ispifIrqStatus2;

	if (qcmd->ispifInterruptStatus0 &
			ISPIF_IRQ_STATUS_PIX_SOF_MASK) {
			CDBG("%s: ispif PIX irq status", __func__);
			ispif->pix_sof_count++;
			v4l2_subdev_notify(&ispif->subdev,
				NOTIFY_VFE_PIX_SOF_COUNT,
				(void *)&ispif->pix_sof_count);
	}

	if (qcmd->ispifInterruptStatus0 &
			ISPIF_IRQ_STATUS_RDI0_SOF_MASK) {
			CDBG("%s: ispif RDI0 irq status", __func__);
			ispif->rdi0_sof_count++;
			send_rdi_sof(ispif, RDI_0, ispif->rdi0_sof_count);
	}
	if (qcmd->ispifInterruptStatus1 &
		ISPIF_IRQ_STATUS_RDI1_SOF_MASK) {
		CDBG("%s: ispif RDI1 irq status", __func__);
		ispif->rdi1_sof_count++;
		send_rdi_sof(ispif, RDI_1, ispif->rdi1_sof_count);
	}
	if (qcmd->ispifInterruptStatus2 &
		ISPIF_IRQ_STATUS_RDI2_SOF_MASK) {
		CDBG("%s: ispif RDI2 irq status", __func__);
		ispif->rdi2_sof_count++;
		send_rdi_sof(ispif, RDI_2, ispif->rdi2_sof_count);
	}

	spin_lock_irqsave(&ispif_tasklet_lock, flags);
	list_add_tail(&qcmd->list, &ispif_tasklet_q);

	atomic_add(1, &ispif_irq_cnt);
	spin_unlock_irqrestore(&ispif_tasklet_lock, flags);
	tasklet_schedule(&ispif->ispif_tasklet);
	return;
}

static inline void msm_ispif_read_irq_status(struct ispif_irq_status *out,
	void *data)
{
	uint32_t status0 = 0, status1 = 0, status2 = 0;
	struct ispif_device *ispif = (struct ispif_device *)data;
	out->ispifIrqStatus0 = msm_camera_io_r(ispif->base +
		ISPIF_IRQ_STATUS_ADDR);
	out->ispifIrqStatus1 = msm_camera_io_r(ispif->base +
		ISPIF_IRQ_STATUS_1_ADDR);
	out->ispifIrqStatus2 = msm_camera_io_r(ispif->base +
		ISPIF_IRQ_STATUS_2_ADDR);
	msm_camera_io_w(out->ispifIrqStatus0,
		ispif->base + ISPIF_IRQ_CLEAR_ADDR);
	msm_camera_io_w(out->ispifIrqStatus1,
		ispif->base + ISPIF_IRQ_CLEAR_1_ADDR);
	msm_camera_io_w(out->ispifIrqStatus2,
		ispif->base + ISPIF_IRQ_CLEAR_2_ADDR);

	CDBG("%s: irq vfe0 Irq_status0 = 0x%x, 1 = 0x%x, 2 = 0x%x\n",
		__func__, out->ispifIrqStatus0, out->ispifIrqStatus1,
		out->ispifIrqStatus2);
	if (out->ispifIrqStatus0 & ISPIF_IRQ_STATUS_MASK) {
		if (out->ispifIrqStatus0 & (0x1 << RESET_DONE_IRQ))
			complete(&ispif->reset_complete);
		if (out->ispifIrqStatus0 & (0x1 << PIX_INTF_0_OVERFLOW_IRQ))
			pr_err("%s: pix intf 0 overflow.\n", __func__);
		if (out->ispifIrqStatus0 & (0x1 << RAW_INTF_0_OVERFLOW_IRQ))
			pr_err("%s: rdi intf 0 overflow.\n", __func__);
		if (out->ispifIrqStatus1 & (0x1 << RAW_INTF_1_OVERFLOW_IRQ))
			pr_err("%s: rdi intf 1 overflow.\n", __func__);
		if (out->ispifIrqStatus2 & (0x1 << RAW_INTF_2_OVERFLOW_IRQ))
			pr_err("%s: rdi intf 2 overflow.\n", __func__);
		if ((out->ispifIrqStatus0 & ISPIF_IRQ_STATUS_SOF_MASK) ||
			(out->ispifIrqStatus1 &	ISPIF_IRQ_STATUS_SOF_MASK) ||
			(out->ispifIrqStatus2 & ISPIF_IRQ_STATUS_RDI2_SOF_MASK))
			ispif_process_irq(ispif, out);
	}
	if (ispif->csid_version == CSID_VERSION_V3) {
		status0 = msm_camera_io_r(ispif->base +
			ISPIF_IRQ_STATUS_ADDR + 0x200);
		msm_camera_io_w(status0,
			ispif->base + ISPIF_IRQ_CLEAR_ADDR + 0x200);
		status1 = msm_camera_io_r(ispif->base +
			ISPIF_IRQ_STATUS_1_ADDR + 0x200);
		msm_camera_io_w(status1,
			ispif->base + ISPIF_IRQ_CLEAR_1_ADDR + 0x200);
		status2 = msm_camera_io_r(ispif->base +
			ISPIF_IRQ_STATUS_2_ADDR + 0x200);
		msm_camera_io_w(status2,
			ispif->base + ISPIF_IRQ_CLEAR_2_ADDR + 0x200);
		CDBG("%s: irq vfe1 Irq_status0 = 0x%x, 1 = 0x%x, 2 = 0x%x\n",
			__func__, status0, status1, status2);
	}
	msm_camera_io_w(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
		ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);
}

static irqreturn_t msm_io_ispif_irq(int irq_num, void *data)
{
	struct ispif_irq_status irq;
	msm_ispif_read_irq_status(&irq, data);
	return IRQ_HANDLED;
}

static struct msm_cam_clk_info ispif_8960_clk_info[] = {
	{"csi_pix_clk", 0},
	{"csi_rdi_clk", 0},
	{"csi_pix1_clk", 0},
	{"csi_rdi1_clk", 0},
	{"csi_rdi2_clk", 0},
};

static int msm_ispif_init(struct ispif_device *ispif,
	const uint32_t *csid_version)
{
	int rc = 0;
	CDBG("%s called %d\n", __func__, __LINE__);

	if (ispif->ispif_state == ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		rc = -EINVAL;
		return rc;
	}

	spin_lock_init(&ispif_tasklet_lock);
	INIT_LIST_HEAD(&ispif_tasklet_q);
	rc = request_irq(ispif->irq->start, msm_io_ispif_irq,
		IRQF_TRIGGER_RISING, "ispif", ispif);
	ispif->global_intf_cmd_mask = 0xFFFFFFFF;
	init_completion(&ispif->reset_complete);

	tasklet_init(&ispif->ispif_tasklet,
		ispif_do_tasklet, (unsigned long)ispif);

	ispif->csid_version = *csid_version;
	if (ispif->csid_version < CSID_VERSION_V2) {
		rc = msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, 2, 1);
		if (rc < 0)
			return rc;
	} else if (ispif->csid_version == CSID_VERSION_V2) {
		rc = msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, ARRAY_SIZE(ispif_8960_clk_info), 1);
		if (rc < 0)
			return rc;
	}
	rc = msm_ispif_reset(ispif);
	ispif->ispif_state = ISPIF_POWER_UP;
	return rc;
}

static void msm_ispif_release(struct ispif_device *ispif)
{
	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
			ispif->ispif_state);
		return;
	}

	CDBG("%s, free_irq\n", __func__);
	free_irq(ispif->irq->start, ispif);
	tasklet_kill(&ispif->ispif_tasklet);

	if (ispif->csid_version < CSID_VERSION_V2) {
		msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, 2, 0);
	} else if (ispif->csid_version == CSID_VERSION_V2) {
		msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, ARRAY_SIZE(ispif_8960_clk_info), 0);
	}
	ispif->ispif_state = ISPIF_POWER_DOWN;
}

static long msm_ispif_cmd(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct ispif_cfg_data cdata;
	struct ispif_device *ispif =
		(struct ispif_device *)v4l2_get_subdevdata(sd);
	if (copy_from_user(&cdata, (void *)arg, sizeof(struct ispif_cfg_data)))
		return -EFAULT;
	CDBG("%s cfgtype = %d\n", __func__, cdata.cfgtype);
	switch (cdata.cfgtype) {
	case ISPIF_INIT:
		CDBG("%s csid_version = %x\n", __func__,
			cdata.cfg.csid_version);
		rc = msm_ispif_init(ispif, &cdata.cfg.csid_version);
		break;
	case ISPIF_SET_CFG:
		CDBG("%s len = %d, intftype = %d,.cid_mask = %d, csid = %d\n",
			__func__,
			cdata.cfg.ispif_params.len,
			cdata.cfg.ispif_params.params[0].intftype,
			cdata.cfg.ispif_params.params[0].cid_mask,
			cdata.cfg.ispif_params.params[0].csid);
		rc = msm_ispif_config(ispif, &cdata.cfg.ispif_params);
		break;

	case ISPIF_SET_ON_FRAME_BOUNDARY:
	case ISPIF_SET_OFF_FRAME_BOUNDARY:
	case ISPIF_SET_OFF_IMMEDIATELY:
		rc = msm_ispif_subdev_video_s_stream(sd, cdata.cfg.cmd);
		break;
	case ISPIF_RELEASE:
		msm_ispif_release(ispif);
		break;
	default:
		break;
	}

	return rc;
}

static long msm_ispif_subdev_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
								void *arg)
{
	switch (cmd) {
	case VIDIOC_MSM_ISPIF_CFG:
		return msm_ispif_cmd(sd, arg);
	default:
		return -ENOIOCTLCMD;
	}
}

static struct v4l2_subdev_core_ops msm_ispif_subdev_core_ops = {
	.g_chip_ident = &msm_ispif_subdev_g_chip_ident,
	.ioctl = &msm_ispif_subdev_ioctl,
};

static struct v4l2_subdev_video_ops msm_ispif_subdev_video_ops = {
	.s_stream = &msm_ispif_subdev_video_s_stream,
};

static const struct v4l2_subdev_ops msm_ispif_subdev_ops = {
	.core = &msm_ispif_subdev_core_ops,
	.video = &msm_ispif_subdev_video_ops,
};

static const struct v4l2_subdev_internal_ops msm_ispif_internal_ops;

static int __devinit ispif_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cam_subdev_info sd_info;
	struct ispif_device *ispif;

	CDBG("%s\n", __func__);
	ispif = kzalloc(sizeof(struct ispif_device), GFP_KERNEL);
	if (!ispif) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&ispif->subdev, &msm_ispif_subdev_ops);
	ispif->subdev.internal_ops = &msm_ispif_internal_ops;
	ispif->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(ispif->subdev.name,
			ARRAY_SIZE(ispif->subdev.name), "msm_ispif");
	v4l2_set_subdevdata(&ispif->subdev, ispif);
	platform_set_drvdata(pdev, &ispif->subdev);
	snprintf(ispif->subdev.name, sizeof(ispif->subdev.name),
								"ispif");
	mutex_init(&ispif->mutex);

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	ispif->mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ispif");
	if (!ispif->mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto ispif_no_resource;
	}
	ispif->irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "ispif");
	if (!ispif->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto ispif_no_resource;
	}
	ispif->io = request_mem_region(ispif->mem->start,
		resource_size(ispif->mem), pdev->name);
	if (!ispif->io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto ispif_no_resource;
	}
	ispif->base = ioremap(ispif->mem->start,
		resource_size(ispif->mem));
	if (!ispif->base) {
		rc = -ENOMEM;
		goto ispif_no_mem;
	}

	ispif->pdev = pdev;
	sd_info.sdev_type = ISPIF_DEV;
	sd_info.sd_index = pdev->id;
	sd_info.irq_num = ispif->irq->start;
	msm_cam_register_subdev_node(&ispif->subdev, &sd_info);

	media_entity_init(&ispif->subdev.entity, 0, NULL, 0);
	ispif->subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ispif->subdev.entity.group_id = ISPIF_DEV;
	ispif->subdev.entity.name = pdev->name;
	ispif->subdev.entity.revision = ispif->subdev.devnode->num;
	ispif->ispif_state = ISPIF_POWER_DOWN;
	return 0;

ispif_no_mem:
	release_mem_region(ispif->mem->start,
		resource_size(ispif->mem));
ispif_no_resource:
	mutex_destroy(&ispif->mutex);
	kfree(ispif);
	return rc;
}

static const struct of_device_id msm_ispif_dt_match[] = {
	{.compatible = "qcom,ispif"},
};

MODULE_DEVICE_TABLE(of, msm_ispif_dt_match);

static struct platform_driver ispif_driver = {
	.probe = ispif_probe,
	.driver = {
		.name = MSM_ISPIF_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_ispif_dt_match,
	},
};

static int __init msm_ispif_init_module(void)
{
	return platform_driver_register(&ispif_driver);
}

static void __exit msm_ispif_exit_module(void)
{
	platform_driver_unregister(&ispif_driver);
}

module_init(msm_ispif_init_module);
module_exit(msm_ispif_exit_module);
MODULE_DESCRIPTION("MSM ISP Interface driver");
MODULE_LICENSE("GPL v2");
