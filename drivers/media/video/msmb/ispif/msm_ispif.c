/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include "msm_ispif.h"
#include "msm.h"
#include "msm_ispif_hwreg.h"
#include "msm_sd.h"
#include "msm_camera_io_util.h"

#define V4L2_IDENT_ISPIF                      50001
#define MSM_ISPIF_DRV_NAME                    "msm_ispif"
#define DUMP_BUFF_SIZE_128                    128

#define ISPIF_INTF_CMD_ENABLE_FRAME_BOUNDARY  0x01
#define ISPIF_INTF_CMD_DISABLE_FRAME_BOUNDARY 0x00
#define ISPIF_INTF_CMD_DISABLE_IMMEDIATELY    0x02

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static void msm_ispif_io_dump(void __iomem *addr, int size)
{
	char line_str[DUMP_BUFF_SIZE_128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;


	CDBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			snprintf(p_str, 12, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			CDBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CDBG("%s\n", line_str);
}

static void msm_ispif_io_dump_reg(struct ispif_device *ispif)
{
	int size;

	if (!ispif->enb_dump_reg)
		return;
	size = 0x250;
	msm_ispif_io_dump(ispif->base+0x100, size);
}

static int msm_ispif_intf_reset(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{

	int i, rc = 0;
	enum msm_ispif_intftype intf_type;
	uint32_t data = (0x1 << STROBED_RST_EN);

	for (i = 0; i < params->num; i++) {
		intf_type = params->entries[i].intftype;
		ispif->sof_count[params->vfe_intf].sof_cnt[intf_type] = 0;
		switch (intf_type) {
		case PIX0:
			data |= (0x1 << PIX_0_VFE_RST_STB) |
				(0x1 << PIX_0_CSID_RST_STB);
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
	}
	if (data > 0x1) {
		unsigned long jiffes = msecs_to_jiffies(500);
		long lrc = 0;
		if (params->vfe_intf == VFE0)
			msm_camera_io_w(data, ispif->base + ISPIF_RST_CMD_ADDR);
		else
			msm_camera_io_w(data, ispif->base +
				ISPIF_RST_CMD_1_ADDR);
		lrc = wait_for_completion_interruptible_timeout(
			&ispif->reset_complete, jiffes);
		if (lrc < 0 || !lrc) {
			pr_err("%s: wait timeout ret = %ld\n", __func__, lrc);
			rc = -EIO;
		}
	}
	return rc;
}

static int msm_ispif_reset(struct ispif_device *ispif)
{
	int rc = 0;
	unsigned long jiffes = msecs_to_jiffies(500);
	long lrc = 0;

	memset(ispif->sof_count,  0,  sizeof(ispif->sof_count));
	msm_camera_io_w(ISPIF_RST_CMD_MASK, ispif->base + ISPIF_RST_CMD_ADDR);
	if (ispif->csid_version == CSID_VERSION_V3)
		msm_camera_io_w_mb(ISPIF_RST_CMD_1_MASK, ispif->base +
			ISPIF_RST_CMD_1_ADDR);
	CDBG("%s: Sending reset\n", __func__);
	lrc = wait_for_completion_interruptible_timeout(
		&ispif->reset_complete, jiffes);
	if (lrc < 0 || !lrc) {
		pr_err("%s: wait timeout ret = %ld\n", __func__, lrc);
		rc = -EIO;
	}
	CDBG("%s: reset returned\n", __func__);
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
			CDBG("%s: ispif NULL clk\n", __func__);
			return;
		}
		rc = clk_set_rate(ispif->ispif_clk[intftype], csid);
		if (rc < 0)
			pr_err("%s: clk_set_rate failed %d\n", __func__, rc);
		return;
	}
	data = msm_camera_io_r(ispif->base + ISPIF_INPUT_SEL_ADDR +
		(0x200 * vfe_intf));
	switch (intftype) {
	case PIX0:
		data &= ~(0x3);        /* clear old setting */
		data |= csid;          /* add new setting */
		break;
	case RDI0:
		data &= ~(0x3 << 4);   /* clear old setting */
		data |= (csid << 4);   /* add new setting */
		break;
	case PIX1:
		data &= ~(0x3 << 8);   /* clear old setting */
		data |= (csid << 8);   /* add new setting */
		break;
	case RDI1:
		data &= ~(0x3 << 12);  /* clear old setting */
		data |= (csid << 12);  /* add new setting */
		break;
	case RDI2:
		data &= ~(0x3 << 20);  /* clear old setting */
		data |= (csid << 20);  /* add new setting */
		break;
	}
	if (data) {
		msm_camera_io_w_mb(data, ispif->base + ISPIF_INPUT_SEL_ADDR +
			(0x200 * vfe_intf));
	}
}

static void msm_ispif_enable_intf_cids(struct ispif_device *ispif,
	uint8_t intftype, uint16_t cid_mask,
	uint8_t vfe_intf, uint8_t enable)
{
	uint32_t data = 0;

	switch (intftype) {
	case PIX0:
		data = msm_camera_io_r(ispif->base +
		ISPIF_PIX_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		if (enable)
			data |= cid_mask;  /* add new config */
		else
			data &= ~cid_mask;  /* remove CID bit */
		msm_camera_io_w_mb(data, ispif->base +
		ISPIF_PIX_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;
	case RDI0:
		data = msm_camera_io_r(ispif->base +
		ISPIF_RDI_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		if (enable)
			data |= cid_mask;  /* add new config */
		else
			data &= ~cid_mask;  /* remove CID bit */
		msm_camera_io_w_mb(data, ispif->base +
			ISPIF_RDI_0_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;
	case PIX1:
		data = msm_camera_io_r(ispif->base +
		ISPIF_PIX_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		if (enable)
			data |= cid_mask;  /* add new config */
		else
			data &= ~cid_mask;  /* remove CID bit */
		msm_camera_io_w_mb(data, ispif->base +
			ISPIF_PIX_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;
	case RDI1:
		data = msm_camera_io_r(ispif->base +
			ISPIF_RDI_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		if (enable)
			data |= cid_mask;  /* add new config */
		else
			data &= ~cid_mask;  /* remove CID bit */
		msm_camera_io_w_mb(data, ispif->base +
			ISPIF_RDI_1_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;
	case RDI2:
		data = msm_camera_io_r(ispif->base +
			ISPIF_RDI_2_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		if (enable)
			data |= cid_mask;  /* add new config */
		else
			data &= ~cid_mask;  /* remove CID bit */
		msm_camera_io_w_mb(data, ispif->base +
			ISPIF_RDI_2_INTF_CID_MASK_ADDR + (0x200 * vfe_intf));
		break;
	}
}

static int32_t msm_ispif_validate_intf_status(struct ispif_device *ispif,
	uint8_t intftype, uint8_t vfe_intf)
{
	int32_t rc = 0;
	uint32_t data = 0;
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
	return rc;
}

static uint16_t msm_ispif_get_cids_mask_from_cfg(
	struct msm_ispif_params_entry *entry)
{
	int i;
	uint16_t cids_mask = 0;

	for (i = 0; i < entry->num_cids; i++)
		cids_mask |= (1 << entry->cids[i]);
	return cids_mask;
}

static int msm_ispif_config(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int rc = 0, i = 0;
	enum msm_ispif_intftype intftype;
	enum msm_ispif_vfe_intf vfe_intf = params->vfe_intf;
	uint16_t cid_mask;

	msm_camera_io_w(0x00000000, ispif->base + ISPIF_IRQ_MASK_ADDR);
	msm_camera_io_w(0x00000000, ispif->base + ISPIF_IRQ_MASK_1_ADDR);
	msm_camera_io_w_mb(0x00000000, ispif->base + ISPIF_IRQ_MASK_2_ADDR);
	for (i = 0; i < params->num; i++) {
		intftype = params->entries[i].intftype;
		vfe_intf = params->vfe_intf;
		CDBG("%s intftype %x, vfe_intf %d, csid %d\n", __func__,
		intftype, vfe_intf, params->entries[i].csid);
		if ((intftype >= INTF_MAX) ||
				(ispif->csid_version <= CSID_VERSION_V2 &&
				vfe_intf > VFE0) ||
				(ispif->csid_version == CSID_VERSION_V3 &&
				vfe_intf >= VFE_MAX)) {
			pr_err("%s: VFEID %d and CSID version %d mismatch\n",
				__func__, vfe_intf, ispif->csid_version);
			return -EINVAL;
		}
		rc = msm_ispif_validate_intf_status(ispif, intftype, vfe_intf);
		if (rc < 0) {
			pr_err("%s:validate_intf_status failed, rc = %d\n",
				__func__, rc);
			return rc;
		}
		msm_ispif_sel_csid_core(ispif, intftype,
			params->entries[i].csid, vfe_intf);
		cid_mask = msm_ispif_get_cids_mask_from_cfg(
				&params->entries[i]);
		msm_ispif_enable_intf_cids(ispif, intftype,
			cid_mask, vfe_intf, 1);
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

	msm_camera_io_w_mb(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
		ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);
	return rc;
}

static void msm_ispif_intf_cmd(struct ispif_device *ispif,
	uint32_t cmd_bits,
	struct msm_ispif_param_data *params)
{
	uint8_t vc = 0;
	int i, k;
	enum msm_ispif_intftype intf_type;
	enum msm_ispif_cid cid;
	enum msm_ispif_vfe_intf vfe_intf = params->vfe_intf;

	for (i = 0; i < params->num; i++) {
		intf_type = params->entries[i].intftype;
		for (k = 0; k < params->entries[i].num_cids; k++) {
			cid = params->entries[i].cids[k];
			vc = cid % 4;
			if (intf_type == RDI2) {
				/* zero out two bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd1 &=
					~(0x3 << (vc * 2 + 8));
				ispif->applied_intf_cmd[vfe_intf].intf_cmd1 |=
				(cmd_bits << (vc * 2 + 8)); /* set cmd bits */
			} else {
				/* zero 2 bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd &=
					~(0x3 << (vc * 2 + vfe_intf * 8));
				/* set cmd bits */
				ispif->applied_intf_cmd[vfe_intf].intf_cmd |=
				(cmd_bits << (vc * 2 + vfe_intf * 8));
			}
		}
	}
	/* cmd for PIX0, PIX1, RDI0, RDI1 */
	if (ispif->applied_intf_cmd[vfe_intf].intf_cmd != 0xFFFFFFFF) {
		msm_camera_io_w_mb(ispif->applied_intf_cmd[vfe_intf].intf_cmd,
			ispif->base + ISPIF_INTF_CMD_ADDR +
			(0x200 * vfe_intf));
	}
	/* cmd for RDI2 */
	if (ispif->applied_intf_cmd[vfe_intf].intf_cmd1 != 0xFFFFFFFF)
		msm_camera_io_w_mb(ispif->applied_intf_cmd[vfe_intf].intf_cmd1,
			ispif->base + ISPIF_INTF_CMD_1_ADDR +
			(0x200 * vfe_intf));
}

static int msm_ispif_stop_immediately(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int i, rc = 0;
	uint16_t cid_mask = 0;

	msm_ispif_intf_cmd(ispif, ISPIF_INTF_CMD_DISABLE_IMMEDIATELY, params);

	/* after stop the interface we need to unmask the CID enable bits */
	for (i = 0; i < params->num; i++) {
		cid_mask = msm_ispif_get_cids_mask_from_cfg(
			&params->entries[i]);
		msm_ispif_enable_intf_cids(ispif, params->entries[i].intftype,
		cid_mask, params->vfe_intf, 0);
	}
	return rc;
}

static int msm_ispif_start_frame_boundary(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int rc = 0;

	rc = msm_ispif_intf_reset(ispif, params);
	msm_ispif_intf_cmd(ispif, ISPIF_INTF_CMD_ENABLE_FRAME_BOUNDARY, params);
	return rc;
}

static int msm_ispif_stop_frame_boundary(struct ispif_device *ispif,
	struct msm_ispif_param_data *params)
{
	int i, rc = 0;
	uint16_t cid_mask = 0;

	msm_ispif_intf_cmd(ispif,
		ISPIF_INTF_CMD_DISABLE_FRAME_BOUNDARY, params);
	for (i = 0; i < params->num; i++) {
		cid_mask =
			msm_ispif_get_cids_mask_from_cfg(
				&params->entries[i]);
		switch (params->entries[i].intftype) {
		case PIX0:
			while ((msm_camera_io_r(ispif->base +
				ISPIF_PIX_0_STATUS_ADDR +
				(0x200 * params->vfe_intf)) & 0xf) != 0xf) {
				CDBG("Wait for pix0 Idle\n");
			}
			break;
		case RDI0:
			while ((msm_camera_io_r(ispif->base +
				ISPIF_RDI_0_STATUS_ADDR +
				(0x200 * params->vfe_intf)) & 0xf) != 0xf) {
				CDBG("Wait for rdi0 Idle\n");
			}
			break;
		case PIX1:
			while ((msm_camera_io_r(ispif->base +
				ISPIF_PIX_1_STATUS_ADDR +
				(0x200 * params->vfe_intf)) & 0xf) != 0xf) {
				CDBG("Wait for pix1 Idle\n");
			}
			break;
		case RDI1:
			while ((msm_camera_io_r(ispif->base +
				ISPIF_RDI_1_STATUS_ADDR +
				(0x200 * params->vfe_intf)) & 0xf) != 0xf) {
				CDBG("Wait for rdi1 Idle\n");
			}
			break;
		case RDI2:
			while ((msm_camera_io_r(ispif->base +
				ISPIF_RDI_2_STATUS_ADDR +
				(0x200 * params->vfe_intf)) & 0xf) != 0xf) {
				CDBG("Wait for rdi2 Idle\n");
			}
			break;
		default:
			break;
		}
		/* disable CIDs in CID_MASK register */
		msm_ispif_enable_intf_cids(ispif, params->entries[i].intftype,
		cid_mask, params->vfe_intf, 0);
	}
	return rc;
}

static void ispif_process_irq(struct ispif_device *ispif,
	struct ispif_irq_status *out, enum msm_ispif_vfe_intf vfe_id)
{
	if (out[vfe_id].ispifIrqStatus0 &
			ISPIF_IRQ_STATUS_PIX_SOF_MASK) {
		ispif->sof_count[vfe_id].sof_cnt[PIX0]++;
	}
	if (out[vfe_id].ispifIrqStatus0 &
			ISPIF_IRQ_STATUS_RDI0_SOF_MASK) {
		ispif->sof_count[vfe_id].sof_cnt[RDI0]++;
	}
	if (out[vfe_id].ispifIrqStatus1 &
			ISPIF_IRQ_STATUS_RDI1_SOF_MASK) {
		ispif->sof_count[vfe_id].sof_cnt[RDI1]++;
	}
	if (out[vfe_id].ispifIrqStatus2 &
			ISPIF_IRQ_STATUS_RDI2_SOF_MASK) {
		ispif->sof_count[vfe_id].sof_cnt[RDI2]++;
	}
	return;
}

static inline void msm_ispif_read_irq_status(struct ispif_irq_status *out,
	void *data)
{
	struct ispif_device *ispif = (struct ispif_device *)data;

	out[VFE0].ispifIrqStatus0 = msm_camera_io_r(ispif->base +
		ISPIF_IRQ_STATUS_ADDR);
	out[VFE0].ispifIrqStatus1 = msm_camera_io_r(ispif->base +
		ISPIF_IRQ_STATUS_1_ADDR);
	out[VFE0].ispifIrqStatus2 = msm_camera_io_r(ispif->base +
		ISPIF_IRQ_STATUS_2_ADDR);
	msm_camera_io_w(out[VFE0].ispifIrqStatus0,
		ispif->base + ISPIF_IRQ_CLEAR_ADDR);
	msm_camera_io_w(out[VFE0].ispifIrqStatus1,
		ispif->base + ISPIF_IRQ_CLEAR_1_ADDR);
	msm_camera_io_w_mb(out[VFE0].ispifIrqStatus2,
		ispif->base + ISPIF_IRQ_CLEAR_2_ADDR);

	if (out[VFE0].ispifIrqStatus0 & ISPIF_IRQ_STATUS_MASK) {
		if (out[VFE0].ispifIrqStatus0 & (0x1 <<
				RESET_DONE_IRQ))
			complete(&ispif->reset_complete);
		if (out[VFE0].ispifIrqStatus0 & (0x1 <<
				PIX_INTF_0_OVERFLOW_IRQ))
			pr_err("%s: VFE0 pix0 overflow.\n", __func__);
		if (out[VFE0].ispifIrqStatus0 & (0x1 <<
				RAW_INTF_0_OVERFLOW_IRQ))
			pr_err("%s: VFE0 rdi0 overflow.\n", __func__);
		if (out[VFE0].ispifIrqStatus1 & (0x1 <<
				RAW_INTF_1_OVERFLOW_IRQ))
			pr_err("%s: VFE0 rdi1 overflow.\n", __func__);
		if (out[VFE0].ispifIrqStatus2 & (0x1 <<
				RAW_INTF_2_OVERFLOW_IRQ))
			pr_err("%s: VFE0 rdi2 overflow.\n", __func__);
		if ((out[VFE0].ispifIrqStatus0 &
			ISPIF_IRQ_STATUS_SOF_MASK) ||
				(out[VFE0].ispifIrqStatus1 &
					ISPIF_IRQ_STATUS_SOF_MASK) ||
				(out[VFE0].ispifIrqStatus2 &
					ISPIF_IRQ_STATUS_RDI2_SOF_MASK))
			ispif_process_irq(ispif, out, VFE0);
	}
	if (ispif->csid_version == CSID_VERSION_V3) {
		out[VFE1].ispifIrqStatus0 = msm_camera_io_r(ispif->base +
			ISPIF_IRQ_STATUS_ADDR + 0x200);
		msm_camera_io_w(out[VFE1].ispifIrqStatus0,
			ispif->base + ISPIF_IRQ_CLEAR_ADDR + 0x200);
		out[VFE1].ispifIrqStatus1 = msm_camera_io_r(ispif->base +
			ISPIF_IRQ_STATUS_1_ADDR + 0x200);
		msm_camera_io_w(out[VFE1].ispifIrqStatus1,
			ispif->base + ISPIF_IRQ_CLEAR_1_ADDR + 0x200);
		out[VFE1].ispifIrqStatus2 = msm_camera_io_r(ispif->base +
			ISPIF_IRQ_STATUS_2_ADDR + 0x200);
		msm_camera_io_w_mb(out[VFE1].ispifIrqStatus2,
			ispif->base + ISPIF_IRQ_CLEAR_2_ADDR + 0x200);
		if (out[VFE1].ispifIrqStatus0 & (0x1 <<
				PIX_INTF_0_OVERFLOW_IRQ))
			pr_err("%s: VFE1 pix0 overflow.\n", __func__);
		if (out[VFE1].ispifIrqStatus0 & (0x1 <<
				RAW_INTF_0_OVERFLOW_IRQ))
			pr_err("%s: VFE1 rdi0 overflow.\n", __func__);
		if (out[VFE1].ispifIrqStatus1 & (0x1 <<
				RAW_INTF_1_OVERFLOW_IRQ))
			pr_err("%s: VFE1 rdi1 overflow.\n", __func__);
		if (out[VFE1].ispifIrqStatus2 & (0x1 <<
				RAW_INTF_2_OVERFLOW_IRQ))
			pr_err("%s: VFE1 rdi2 overflow.\n", __func__);
		if ((out[VFE1].ispifIrqStatus0 & ISPIF_IRQ_STATUS_SOF_MASK) ||
				(out[VFE1].ispifIrqStatus1 &
					ISPIF_IRQ_STATUS_SOF_MASK) ||
				(out[VFE1].ispifIrqStatus2 &
					ISPIF_IRQ_STATUS_RDI2_SOF_MASK))
			ispif_process_irq(ispif, out, VFE1);
	}
	msm_camera_io_w_mb(ISPIF_IRQ_GLOBAL_CLEAR_CMD, ispif->base +
		ISPIF_IRQ_GLOBAL_CLEAR_CMD_ADDR);
}

static irqreturn_t msm_io_ispif_irq(int irq_num, void *data)
{
	struct ispif_irq_status irq[VFE_MAX];

	msm_ispif_read_irq_status(irq, data);
	return IRQ_HANDLED;
}

static struct msm_cam_clk_info ispif_8960_clk_info[] = {
	{"csi_pix_clk", 0},
	{"csi_rdi_clk", 0},
	{"csi_pix1_clk", 0},
	{"csi_rdi1_clk", 0},
	{"csi_rdi2_clk", 0},
};
static struct msm_cam_clk_info ispif_8974_clk_info[] = {
	{"camss_vfe_vfe_clk", -1},
	{"camss_csi_vfe_clk", -1},
	{"camss_vfe_vfe_clk1", -1},
	{"camss_csi_vfe_clk1", -1},
};

static int msm_ispif_init(struct ispif_device *ispif,
	uint32_t csid_version)
{
	int rc = 0;

	if (ispif->ispif_state == ISPIF_POWER_UP) {
		CDBG("%s: ispif already initted state = %d\n", __func__,
		ispif->ispif_state);
		rc = -EAGAIN;
		return rc;
	}

	/* can we set to zero? */
	ispif->applied_intf_cmd[VFE0].intf_cmd  = 0xFFFFFFFF;
	ispif->applied_intf_cmd[VFE0].intf_cmd1 = 0xFFFFFFFF;
	ispif->applied_intf_cmd[VFE1].intf_cmd  = 0xFFFFFFFF;
	ispif->applied_intf_cmd[VFE1].intf_cmd1 = 0xFFFFFFFF;
	memset(ispif->sof_count, 0, sizeof(ispif->sof_count));

	ispif->csid_version = csid_version;
	if (ispif->csid_version < CSID_VERSION_V2) {
		rc = msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, 2, 1);
		if (rc < 0) {
			pr_err("%s: cannot enable clock, error = %d\n",
				__func__, rc);
			goto end;
		}
	} else if (ispif->csid_version == CSID_VERSION_V2) {
		rc = msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, ARRAY_SIZE(ispif_8960_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: cannot enable clock, error = %d\n",
				__func__, rc);
			goto end;
		}
	} else {
		rc = msm_cam_clk_enable(&ispif->pdev->dev, ispif_8974_clk_info,
			ispif->ispif_clk, ARRAY_SIZE(ispif_8974_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: cannot enable clock, error = %d\n",
				__func__, rc);
			goto end;
		}
	}
	ispif->base = ioremap(ispif->mem->start,
		resource_size(ispif->mem));
	if (!ispif->base) {
		rc = -ENOMEM;
		pr_err("%s: nomem\n", __func__);
		goto error_clk;
	}
	rc = request_irq(ispif->irq->start, msm_io_ispif_irq,
		IRQF_TRIGGER_RISING, "ispif", ispif);
	init_completion(&ispif->reset_complete);
	if (rc < 0) {
		pr_err("%s: request_irq error = %d\n", __func__, rc);
		goto error_irq;
	}
	rc = msm_ispif_reset(ispif);
	if (rc == 0) {
		ispif->ispif_state = ISPIF_POWER_UP;
		CDBG("%s: power up done\n", __func__);
		goto end;
	}
	free_irq(ispif->irq->start, ispif);
error_irq:
	iounmap(ispif->base);
error_clk:
	if (ispif->csid_version < CSID_VERSION_V2) {
		msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
		ispif->ispif_clk, 2, 0);
	} else if (ispif->csid_version == CSID_VERSION_V2) {
		msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
		ispif->ispif_clk, ARRAY_SIZE(ispif_8960_clk_info), 0);
	}
end:
	return rc;
}

static void msm_ispif_release(struct ispif_device *ispif)
{
	if (ispif->ispif_state != ISPIF_POWER_UP) {
		pr_err("%s: ispif invalid state %d\n", __func__,
		ispif->ispif_state);
		return;
	}
	/* make sure no streaming going on */
	msm_ispif_reset(ispif);
	free_irq(ispif->irq->start, ispif);
	iounmap(ispif->base);
	if (ispif->csid_version < CSID_VERSION_V2) {
		msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
		ispif->ispif_clk, 2, 0);
	} else if (ispif->csid_version == CSID_VERSION_V2) {
		msm_cam_clk_enable(&ispif->pdev->dev, ispif_8960_clk_info,
			ispif->ispif_clk, ARRAY_SIZE(ispif_8960_clk_info), 0);
	}
	ispif->ispif_state = ISPIF_POWER_DOWN;
}

static int msm_ispif_clk_enable(struct ispif_device *ispif,
	uint32_t csid_version, int enable)
{
	int rc = 0;

	if (csid_version != CSID_VERSION_V3)
		goto end;
	rc = msm_cam_clk_enable(&ispif->pdev->dev, ispif_8974_clk_info,
		ispif->ispif_clk, ARRAY_SIZE(ispif_8974_clk_info), enable);
	if (rc < 0)
		pr_err("%s: cannot enable clock, error = %d\n", __func__, rc);
end:
	return rc;
}

static long msm_ispif_cmd(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct ispif_cfg_data *pcdata = (struct ispif_cfg_data *)arg;
	struct ispif_device *ispif =
		(struct ispif_device *)v4l2_get_subdevdata(sd);
	mutex_lock(&ispif->mutex);
	switch (pcdata->cfg_type) {
	case ISPIF_CLK_ENABLE:
		rc = msm_ispif_clk_enable(ispif, pcdata->csid_version, 1);
		break;
	case ISPIF_CLK_DISABLE:
		rc = msm_ispif_clk_enable(ispif, pcdata->csid_version, 0);
		break;
	case ISPIF_ENABLE_REG_DUMP:
		ispif->enb_dump_reg = pcdata->reg_dump; /* save dump config */
		break;
	case ISPIF_INIT:
		/* need to move back to CDBG */
		rc = msm_ispif_init(ispif, pcdata->csid_version);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_CFG:
		rc = msm_ispif_config(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_START_FRAME_BOUNDARY:
		rc = msm_ispif_start_frame_boundary(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_STOP_FRAME_BOUNDARY:
		rc = msm_ispif_stop_frame_boundary(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_STOP_IMMEDIATELY:
		rc = msm_ispif_stop_immediately(ispif, &pcdata->params);
		msm_ispif_io_dump_reg(ispif);
		break;
	case ISPIF_RELEASE:
		msm_ispif_release(ispif);
		break;
	default:
		break;
	}
	mutex_unlock(&ispif->mutex);
	return rc;
}

static long msm_ispif_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	switch (cmd) {
	case VIDIOC_MSM_ISPIF_CFG:
		return msm_ispif_cmd(sd, arg);
	default:
		pr_err("%s: invalid cmd received\n", __func__);
		return -ENOIOCTLCMD;
	}
}

static int ispif_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ispif_device *ispif = v4l2_get_subdevdata(sd);
	int rc = 0;

	mutex_lock(&ispif->mutex);
	if (ispif->open_cnt > 0) {
		CDBG("%s: dev already open\n", __func__);
		goto end;
	}
	/* mem remap is done in init when the clock is on */
	ispif->open_cnt++;
end:
	mutex_unlock(&ispif->mutex);
	return rc;
}

static int ispif_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ispif_device *ispif = v4l2_get_subdevdata(sd);
	int rc = 0;

	CDBG("%s\n", __func__);
	mutex_lock(&ispif->mutex);
	if (ispif->open_cnt == 0) {
		pr_err("Invalid close\n");
		rc = -ENODEV;
		goto end;
	}
	ispif->open_cnt--;
	if (ispif->open_cnt == 0)
		msm_ispif_release(ispif);
end:
	mutex_unlock(&ispif->mutex);
	return rc;
}

static struct v4l2_subdev_core_ops msm_ispif_subdev_core_ops = {
	.g_chip_ident = &msm_ispif_subdev_g_chip_ident,
	.ioctl = &msm_ispif_subdev_ioctl,
};

static const struct v4l2_subdev_ops msm_ispif_subdev_ops = {
	.core = &msm_ispif_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_ispif_internal_ops = {
	.open = ispif_open_node,
	.close = ispif_close_node,
};
static int __devinit ispif_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct ispif_device *ispif;

	CDBG("%s\n", __func__);
	ispif = kzalloc(sizeof(struct ispif_device), GFP_KERNEL);
	if (!ispif) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}

	v4l2_subdev_init(&ispif->msm_sd.sd, &msm_ispif_subdev_ops);
	ispif->msm_sd.sd.internal_ops = &msm_ispif_internal_ops;
	ispif->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	snprintf(ispif->msm_sd.sd.name,
		ARRAY_SIZE(ispif->msm_sd.sd.name), "msm_ispif");
	v4l2_set_subdevdata(&ispif->msm_sd.sd, ispif);
	platform_set_drvdata(pdev, &ispif->msm_sd.sd);
	mutex_init(&ispif->mutex);
	ispif->pdev = pdev;
	media_entity_init(&ispif->msm_sd.sd.entity, 0, NULL, 0);
	ispif->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ispif->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ISPIF;
	ispif->msm_sd.sd.entity.name = pdev->name;
	rc = msm_sd_register(&ispif->msm_sd);
	if (rc != 0) {
		pr_err("%s: msm_sd_register error = %d\n", __func__, rc);
		goto error;
	}
	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
		"cell-index", &pdev->id);

	ispif->mem = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "ispif");
	if (!ispif->mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto error;
	}
	ispif->irq = platform_get_resource_byname(pdev,
		IORESOURCE_IRQ, "ispif");
	if (!ispif->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto error;
	}
	ispif->io = request_mem_region(ispif->mem->start,
		resource_size(ispif->mem), pdev->name);
	if (!ispif->io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto error;
	}
	ispif->pdev = pdev;
	ispif->ispif_state = ISPIF_POWER_DOWN;
	ispif->open_cnt = 0;
	return 0;

error:
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
