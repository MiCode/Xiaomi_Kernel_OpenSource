/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include <linux/ratelimit.h>

#include "msm_isp44.h"
#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp_stats_util.h"
#include "msm_isp.h"
#include "msm.h"
#include "msm_camera_io_util.h"
#include "msm_isp47.h"
#include "linux/iopoll.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#define STATS_IDX_BF_SCALE  0
#define STATS_IDX_BE        1
#define STATS_IDX_BG        2
#define STATS_IDX_BF        3
#define STATS_IDX_AWB       4
#define STATS_IDX_RS        5
#define STATS_IDX_CS        6
#define STATS_IDX_IHIST     7
#define STATS_IDX_BHIST     8

#define VFE44_8084V1_VERSION   0x4000000A

#define VFE44_BURST_LEN 3
#define VFE44_FETCH_BURST_LEN 3
#define VFE44_STATS_BURST_LEN 2
#define VFE44_UB_SIZE 2048
#define MSM_ISP44_TOTAL_IMAGE_UB 1528
#define VFE44_WM_BASE(idx) (0x6C + 0x24 * idx)
#define VFE44_RDI_BASE(idx) (0x2E8 + 0x4 * idx)
#define VFE44_XBAR_BASE(idx) (0x58 + 0x4 * (idx / 2))
#define VFE44_XBAR_SHIFT(idx) ((idx%2) ? 16 : 0)
#define VFE44_PING_PONG_BASE(wm, ping_pong) \
	(VFE44_WM_BASE(wm) + 0x4 * (1 + ((~ping_pong) & 0x1)))

#define VFE44_BUS_RD_CGC_OVERRIDE_BIT 16

static uint8_t stats_pingpong_offset_map[] = {
	7, 8, 9, 10, 11, 12, 13, 14, 15};

#define SHIFT_BF_SCALE_BIT 1
#define VFE44_NUM_STATS_COMP 2
#define VFE44_NUM_STATS_TYPE 9
#define VFE44_STATS_BASE(idx) \
	((idx) == STATS_IDX_BF_SCALE ? 0xA0C : (0x168 + 0x18 * (idx-1)))
#define VFE44_STATS_PING_PONG_BASE(idx, ping_pong) \
	(VFE44_STATS_BASE(idx) + 0x4 * \
	(~(ping_pong >> (stats_pingpong_offset_map[idx])) & 0x1))

#define VFE44_CLK_IDX 2

static uint32_t msm_vfe44_ub_reg_offset(struct vfe_device *vfe_dev, int wm_idx)
{
	return (VFE44_WM_BASE(wm_idx) + 0x10);
}

static uint32_t msm_vfe44_get_ub_size(struct vfe_device *vfe_dev)
{
	return MSM_ISP44_TOTAL_IMAGE_UB;
}

static void msm_vfe44_config_irq(struct vfe_device *vfe_dev,
		uint32_t irq0_mask, uint32_t irq1_mask,
		enum msm_isp_irq_operation oper)
{
	switch (oper) {
	case MSM_ISP_IRQ_ENABLE:
		vfe_dev->irq0_mask |= irq0_mask;
		vfe_dev->irq1_mask |= irq1_mask;
		msm_camera_io_w(irq0_mask, vfe_dev->vfe_base + 0x30);
		msm_camera_io_w(irq1_mask, vfe_dev->vfe_base + 0x34);
		msm_camera_io_w(0x1, vfe_dev->vfe_base + 0x24);
		break;
	case MSM_ISP_IRQ_DISABLE:
		vfe_dev->irq0_mask &= ~irq0_mask;
		vfe_dev->irq1_mask &= ~irq1_mask;
		break;
	case MSM_ISP_IRQ_SET:
		vfe_dev->irq0_mask = irq0_mask;
		vfe_dev->irq1_mask = irq1_mask;
		msm_camera_io_w(irq0_mask, vfe_dev->vfe_base + 0x30);
		msm_camera_io_w(irq1_mask, vfe_dev->vfe_base + 0x34);
		msm_camera_io_w(0x1, vfe_dev->vfe_base + 0x24);
		break;
	}
	msm_camera_io_w_mb(irq0_mask, vfe_dev->vfe_base + 0x28);
	msm_camera_io_w_mb(irq1_mask, vfe_dev->vfe_base + 0x2C);
}

static int32_t msm_vfe44_init_dt_parms(struct vfe_device *vfe_dev,
				struct msm_vfe_hw_init_parms *dt_parms)
{
	void __iomem *vfebase = vfe_dev->vfe_base;
	struct device_node *of_node;
	int32_t i = 0 , rc = 0;
	uint32_t *dt_settings = NULL, *dt_regs = NULL, dt_entries = 0;

	of_node = vfe_dev->pdev->dev.of_node;

	rc = of_property_read_u32(of_node, dt_parms->entries,
		&dt_entries);
	if (rc < 0 || !dt_entries) {
		pr_err("%s: NO QOS entries found\n", __func__);
		return -EINVAL;
	} else {
		dt_settings = kzalloc(sizeof(uint32_t) * dt_entries,
			GFP_KERNEL);
		if (!dt_settings) {
			pr_err("%s:%d No memory\n", __func__, __LINE__);
			return -ENOMEM;
		}
		dt_regs = kzalloc(sizeof(uint32_t) * dt_entries,
			GFP_KERNEL);
		if (!dt_regs) {
			pr_err("%s:%d No memory\n", __func__, __LINE__);
			kfree(dt_settings);
			return -ENOMEM;
		}
		rc = of_property_read_u32_array(of_node, dt_parms->regs,
			dt_regs, dt_entries);
		if (rc < 0) {
			pr_err("%s: NO QOS BUS BDG info\n", __func__);
			kfree(dt_settings);
			kfree(dt_regs);
			return -EINVAL;
		} else {
			if (dt_parms->settings) {
				rc = of_property_read_u32_array(of_node,
					dt_parms->settings,
					dt_settings, dt_entries);
				if (rc < 0) {
					pr_err("%s: NO QOS settings\n",
						__func__);
					kfree(dt_settings);
					kfree(dt_regs);
				} else {
					for (i = 0; i < dt_entries; i++) {
						msm_camera_io_w(dt_settings[i],
							vfebase + dt_regs[i]);
					}
					kfree(dt_settings);
					kfree(dt_regs);
				}
			} else {
				kfree(dt_settings);
				kfree(dt_regs);
			}
		}
	}
	return 0;
}

static void msm_vfe44_init_hardware_reg(struct vfe_device *vfe_dev)
{
	struct msm_vfe_hw_init_parms qos_parms;
	struct msm_vfe_hw_init_parms vbif_parms;
	struct msm_vfe_hw_init_parms ds_parms;

	qos_parms.entries = "qos-entries";
	qos_parms.regs = "qos-regs";
	qos_parms.settings = "qos-settings";
	vbif_parms.entries = "vbif-entries";
	vbif_parms.regs = "vbif-regs";
	vbif_parms.settings = "vbif-settings";
	ds_parms.entries = "ds-entries";
	ds_parms.regs = "ds-regs";
	ds_parms.settings = "ds-settings";

	msm_vfe44_init_dt_parms(vfe_dev, &qos_parms);
	msm_vfe44_init_dt_parms(vfe_dev, &ds_parms);
	msm_vfe44_init_dt_parms(vfe_dev, &vbif_parms);

	/* BUS_CFG */
	msm_camera_io_w(0x10000001, vfe_dev->vfe_base + 0x50);
	msm_vfe44_config_irq(vfe_dev, 0x800000E0, 0xFFFFFF7E,
			MSM_ISP_IRQ_ENABLE);

}

static void msm_vfe44_clear_status_reg(struct vfe_device *vfe_dev)
{
	msm_vfe44_config_irq(vfe_dev, 0x80000000, 0,
			MSM_ISP_IRQ_SET);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x30);
	msm_camera_io_w_mb(0xFFFFFFFF, vfe_dev->vfe_base + 0x34);
	msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x24);
}

static void msm_vfe44_process_reset_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status0 & (1 << 31)) {
		complete(&vfe_dev->reset_complete);
		vfe_dev->reset_pending = 0;
	}
}

static void msm_vfe44_process_halt_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status1 & (1 << 8)) {
		complete(&vfe_dev->halt_complete);
		msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x2C0);
	}
}

static void msm_vfe44_process_input_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	if (!(irq_status0 & 0x1000003))
		return;

	if (irq_status0 & (1 << 0)) {
		ISP_DBG("%s: SOF IRQ\n", __func__);
		msm_isp_increment_frame_id(vfe_dev, VFE_PIX_0, ts);
	}

	if (irq_status0 & (1 << 24)) {
		ISP_DBG("%s: Fetch Engine Read IRQ\n", __func__);
		msm_isp_fetch_engine_done_notify(vfe_dev,
			&vfe_dev->fetch_engine_info);
	}

	if (irq_status0 & (1 << 1))
		ISP_DBG("%s: EOF IRQ\n", __func__);
}

static void msm_vfe44_process_violation_status(
	struct vfe_device *vfe_dev)
{
	uint32_t violation_status = vfe_dev->error_info.violation_status;
	if (!violation_status)
		return;

	if (violation_status & (1 << 0))
		pr_err("%s: camif violation\n", __func__);
	if (violation_status & (1 << 1))
		pr_err("%s: black violation\n", __func__);
	if (violation_status & (1 << 2))
		pr_err("%s: rolloff violation\n", __func__);
	if (violation_status & (1 << 3))
		pr_err("%s: demux violation\n", __func__);
	if (violation_status & (1 << 4))
		pr_err("%s: demosaic violation\n", __func__);
	if (violation_status & (1 << 5))
		pr_err("%s: wb violation\n", __func__);
	if (violation_status & (1 << 6))
		pr_err("%s: clf violation\n", __func__);
	if (violation_status & (1 << 7))
		pr_err("%s: color correct violation\n", __func__);
	if (violation_status & (1 << 8))
		pr_err("%s: rgb lut violation\n", __func__);
	if (violation_status & (1 << 9))
		pr_err("%s: la violation\n", __func__);
	if (violation_status & (1 << 10))
		pr_err("%s: chroma enhance violation\n", __func__);
	if (violation_status & (1 << 11))
		pr_err("%s: chroma supress mce violation\n", __func__);
	if (violation_status & (1 << 12))
		pr_err("%s: skin enhance violation\n", __func__);
	if (violation_status & (1 << 13))
		pr_err("%s: color tranform enc violation\n", __func__);
	if (violation_status & (1 << 14))
		pr_err("%s: color tranform view violation\n", __func__);
	if (violation_status & (1 << 15))
		pr_err("%s: scale enc y violation\n", __func__);
	if (violation_status & (1 << 16))
		pr_err("%s: scale enc cbcr violation\n", __func__);
	if (violation_status & (1 << 17))
		pr_err("%s: scale view y violation\n", __func__);
	if (violation_status & (1 << 18))
		pr_err("%s: scale view cbcr violation\n", __func__);
	if (violation_status & (1 << 21))
		pr_err("%s: crop enc y violation\n", __func__);
	if (violation_status & (1 << 22))
		pr_err("%s: crop enc cbcr violation\n", __func__);
	if (violation_status & (1 << 23))
		pr_err("%s: crop view y violation\n", __func__);
	if (violation_status & (1 << 24))
		pr_err("%s: crop view cbcr violation\n", __func__);
	if (violation_status & (1 << 25))
		pr_err("%s: realign buf y violation\n", __func__);
	if (violation_status & (1 << 26))
		pr_err("%s: realign buf cb violation\n", __func__);
	if (violation_status & (1 << 27))
		pr_err("%s: realign buf cr violation\n", __func__);
	if (violation_status & (1 << 28))
		pr_err("%s: ltm violation\n", __func__);
	if (violation_status & (1 << 29))
		pr_err("%s: ltm cov violation\n", __func__);
	if (violation_status & (1 << 30))
		pr_err("%s: abf violation\n", __func__);
	if (violation_status & (1 << 31))
		pr_err("%s: bpc violation\n", __func__);
}

static void msm_vfe44_process_error_status(struct vfe_device *vfe_dev)
{
	uint32_t error_status1 = vfe_dev->error_info.error_mask1;
	if (error_status1 & (1 << 0)) {
		pr_err("%s: camif error status: 0x%x\n",
			__func__, vfe_dev->error_info.camif_status);
		msm_camera_io_dump(vfe_dev->vfe_base + 0x2f4, 0x30, 1);
	}
	if (error_status1 & (1 << 1))
		pr_err("%s: stats bhist overwrite\n", __func__);
	if (error_status1 & (1 << 2))
		pr_err("%s: stats cs overwrite\n", __func__);
	if (error_status1 & (1 << 3))
		pr_err("%s: stats ihist overwrite\n", __func__);
	if (error_status1 & (1 << 4))
		pr_err("%s: realign buf y overflow\n", __func__);
	if (error_status1 & (1 << 5))
		pr_err("%s: realign buf cb overflow\n", __func__);
	if (error_status1 & (1 << 6))
		pr_err("%s: realign buf cr overflow\n", __func__);
	if (error_status1 & (1 << 7)) {
		msm_vfe44_process_violation_status(vfe_dev);
	}
	if (error_status1 & (1 << 9)) {
		vfe_dev->stats->imagemaster0_overflow++;
		pr_err("%s: image master 0 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 10)) {
		vfe_dev->stats->imagemaster1_overflow++;
		pr_err("%s: image master 1 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 11)) {
		vfe_dev->stats->imagemaster2_overflow++;
		pr_err("%s: image master 2 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 12)) {
		vfe_dev->stats->imagemaster3_overflow++;
		pr_err("%s: image master 3 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 13)) {
		vfe_dev->stats->imagemaster4_overflow++;
		pr_err("%s: image master 4 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 14)) {
		vfe_dev->stats->imagemaster5_overflow++;
		pr_err("%s: image master 5 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 15)) {
		vfe_dev->stats->imagemaster6_overflow++;
		pr_err("%s: image master 6 bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 16)) {
		vfe_dev->stats->be_overflow++;
		pr_err("%s: status be bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 17)) {
		vfe_dev->stats->bg_overflow++;
		pr_err("%s: status bg bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 18)) {
		vfe_dev->stats->bf_overflow++;
		pr_err("%s: status bf bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 19)) {
		vfe_dev->stats->awb_overflow++;
		pr_err("%s: status awb bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 20)) {
		vfe_dev->stats->rs_overflow++;
		pr_err("%s: status rs bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 21)) {
		vfe_dev->stats->cs_overflow++;
		pr_err("%s: status cs bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 22)) {
		vfe_dev->stats->ihist_overflow++;
		pr_err("%s: status ihist bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 23)) {
		vfe_dev->stats->skinbhist_overflow++;
		pr_err("%s: status skin bhist bus overflow\n", __func__);
	}
	if (error_status1 & (1 << 24)) {
		vfe_dev->stats->bfscale_overflow++;
		pr_err("%s: status bf scale bus overflow\n", __func__);
	}
}

static void msm_vfe44_read_and_clear_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x38);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x3C);

	msm_camera_io_w(*irq_status0, vfe_dev->vfe_base + 0x30);
	msm_camera_io_w(*irq_status1, vfe_dev->vfe_base + 0x34);
	msm_camera_io_w_mb(1, vfe_dev->vfe_base + 0x24);
	*irq_status0 &= vfe_dev->irq0_mask;
	*irq_status1 &= vfe_dev->irq1_mask;
	if (*irq_status0 & 0x10000000) {
		pr_err_ratelimited("%s: Protection triggered\n", __func__);
		*irq_status0 &= ~(0x10000000);
	}

	if (*irq_status1 & (1 << 0)) {
		vfe_dev->error_info.camif_status =
		msm_camera_io_r(vfe_dev->vfe_base + 0x31C);
		msm_vfe44_config_irq(vfe_dev, 0, (1 << 0), MSM_ISP_IRQ_DISABLE);
	}

	if (*irq_status1 & (1 << 7))
		vfe_dev->error_info.violation_status =
		msm_camera_io_r(vfe_dev->vfe_base + 0x48);

}

static void msm_vfe44_read_irq_status(struct vfe_device *vfe_dev,
	 uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x38);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x3C);
}

static void msm_vfe44_process_reg_update(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	enum msm_vfe_input_src i;
	uint32_t shift_irq;
	uint8_t reg_updated = 0;
	unsigned long flags;

	if (!(irq_status0 & 0xF0))
		return;
	/* Shift status bits so that PIX SOF is 1st bit */
	shift_irq = ((irq_status0 & 0xF0) >> 4);

	for (i = VFE_PIX_0; i <= VFE_RAW_2; i++) {
		if (shift_irq & BIT(i)) {
			reg_updated |= BIT(i);
			ISP_DBG("%s REG_UPDATE IRQ %x\n", __func__,
				(uint32_t)BIT(i));
			switch (i) {
			case VFE_PIX_0:
				msm_isp_notify(vfe_dev, ISP_EVENT_REG_UPDATE,
					VFE_PIX_0, ts);
				msm_isp_process_reg_upd_epoch_irq(vfe_dev, i,
						MSM_ISP_COMP_IRQ_REG_UPD, ts);
				msm_isp_process_stats_reg_upd_epoch_irq(vfe_dev,
					MSM_ISP_COMP_IRQ_REG_UPD);
				if (vfe_dev->axi_data.src_info[i].stream_count
									== 0 &&
					vfe_dev->axi_data.src_info[i].
						raw_stream_count == 0 &&
					vfe_dev->axi_data.src_info[i].active)
					vfe_dev->hw_info->vfe_ops.core_ops.
						reg_update(vfe_dev, i);
				break;
			case VFE_RAW_0:
			case VFE_RAW_1:
			case VFE_RAW_2:
				msm_isp_increment_frame_id(vfe_dev, i, ts);
				msm_isp_notify(vfe_dev, ISP_EVENT_SOF, i, ts);
				msm_isp_process_reg_upd_epoch_irq(vfe_dev, i,
						MSM_ISP_COMP_IRQ_REG_UPD, ts);
				/*
				 * Reg Update is pseudo SOF for RDI,
				 * so request every frame
				 */
				vfe_dev->hw_info->vfe_ops.core_ops.reg_update(
					vfe_dev, i);
				/* reg upd is epoch for rdi */
				msm_isp_process_reg_upd_epoch_irq(vfe_dev, i,
						MSM_ISP_COMP_IRQ_EPOCH, ts);
				break;
			default:
				pr_err("%s: Error case\n", __func__);
				return;
			}
		}
	}

	spin_lock_irqsave(&vfe_dev->reg_update_lock, flags);
	if (reg_updated & BIT(VFE_PIX_0))
		vfe_dev->reg_updated = 1;

	vfe_dev->reg_update_requested &= ~reg_updated;
	spin_unlock_irqrestore(&vfe_dev->reg_update_lock, flags);
}

static void msm_vfe44_process_epoch_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	if (!(irq_status0 & 0xc))
		return;

	if (irq_status0 & BIT(2)) {
		msm_isp_notify(vfe_dev, ISP_EVENT_SOF, VFE_PIX_0, ts);
		ISP_DBG("%s: EPOCH0 IRQ\n", __func__);
		msm_isp_process_reg_upd_epoch_irq(vfe_dev, VFE_PIX_0,
					MSM_ISP_COMP_IRQ_EPOCH, ts);
		msm_isp_process_stats_reg_upd_epoch_irq(vfe_dev,
					MSM_ISP_COMP_IRQ_EPOCH);
		msm_isp_update_error_frame_count(vfe_dev);
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].raw_stream_count > 0
			&& vfe_dev->axi_data.src_info[VFE_PIX_0].
			stream_count == 0) {
			ISP_DBG("%s: SOF IRQ\n", __func__);
			msm_isp_notify(vfe_dev, ISP_EVENT_SOF, VFE_PIX_0, ts);
			msm_isp_process_reg_upd_epoch_irq(vfe_dev, VFE_PIX_0,
						MSM_ISP_COMP_IRQ_REG_UPD, ts);
			vfe_dev->hw_info->vfe_ops.core_ops.reg_update(
				   vfe_dev, VFE_PIX_0);
		}
	}
}

static void msm_vfe44_reg_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src)
{
	uint32_t update_mask = 0;
	unsigned long flags;

	/* This HW supports upto VFE_RAW_2 */
	if (frame_src > VFE_RAW_2 && frame_src != VFE_SRC_MAX) {
		pr_err("%s Error case\n", __func__);
		return;
	}

	/*
	 * If frame_src == VFE_SRC_MAX request reg_update on
	 * all supported INTF
	 */
	if (frame_src == VFE_SRC_MAX)
		update_mask = 0xF;
	else
		update_mask = BIT((uint32_t)frame_src);

	spin_lock_irqsave(&vfe_dev->reg_update_lock, flags);
	vfe_dev->axi_data.src_info[VFE_PIX_0].reg_update_frame_id =
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
	vfe_dev->reg_update_requested |= update_mask;
	vfe_dev->common_data->dual_vfe_res->reg_update_mask[vfe_dev->pdev->id] =
		vfe_dev->reg_update_requested;
	if ((vfe_dev->is_split && vfe_dev->pdev->id == ISP_VFE1) &&
		((frame_src == VFE_PIX_0) || (frame_src == VFE_SRC_MAX))) {
		if (!vfe_dev->common_data->dual_vfe_res->vfe_base[ISP_VFE0]) {
			pr_err("%s vfe_base for ISP_VFE0 is NULL\n", __func__);
			spin_unlock_irqrestore(&vfe_dev->reg_update_lock,
				flags);
			return;
		}
		msm_camera_io_w_mb(update_mask,
			vfe_dev->common_data->dual_vfe_res->vfe_base[ISP_VFE0]
			+ 0x378);
		msm_camera_io_w_mb(update_mask,
			vfe_dev->vfe_base + 0x378);
	} else if (!vfe_dev->is_split ||
		((frame_src == VFE_PIX_0) &&
		(vfe_dev->axi_data.src_info[VFE_PIX_0].stream_count == 0) &&
		(vfe_dev->axi_data.src_info[VFE_PIX_0].
					raw_stream_count == 0)) ||
		(frame_src >= VFE_RAW_0 && frame_src <= VFE_SRC_MAX)) {
		msm_camera_io_w_mb(update_mask,
			vfe_dev->vfe_base + 0x378);
	}
	spin_unlock_irqrestore(&vfe_dev->reg_update_lock, flags);
}

static long msm_vfe44_reset_hardware(struct vfe_device *vfe_dev,
	uint32_t first_start, uint32_t blocking_call)
{
	long rc = 0;
	init_completion(&vfe_dev->reset_complete);

	if (blocking_call)
		vfe_dev->reset_pending = 1;

	if (first_start) {
		msm_camera_io_w_mb(0x1FF, vfe_dev->vfe_base + 0xC);
	} else {
		msm_camera_io_w_mb(0x1EF, vfe_dev->vfe_base + 0xC);
		msm_camera_io_w(0x7FFFFFFF, vfe_dev->vfe_base + 0x30);
		msm_camera_io_w(0xFEFFFEFF, vfe_dev->vfe_base + 0x34);
		msm_camera_io_w(0x1, vfe_dev->vfe_base + 0x24);
		vfe_dev->hw_info->vfe_ops.axi_ops.
			reload_wm(vfe_dev, vfe_dev->vfe_base, 0x0031FFFF);
	}

	if (blocking_call) {
		rc = wait_for_completion_timeout(
			&vfe_dev->reset_complete, msecs_to_jiffies(50));
		if (rc <= 0) {
			pr_err("%s:%d failed: reset timeout\n", __func__,
				__LINE__);
			vfe_dev->reset_pending = 0;
		}
	}

	return rc;
}

static void msm_vfe44_axi_reload_wm(struct vfe_device *vfe_dev,
	void __iomem *vfe_base, uint32_t reload_mask)
{
	msm_camera_io_w_mb(reload_mask, vfe_base + 0x4C);
}

static void msm_vfe44_axi_enable_wm(void __iomem *vfe_base,
	uint8_t wm_idx, uint8_t enable)
{
	uint32_t val;
	val = msm_camera_io_r(vfe_base + VFE44_WM_BASE(wm_idx));
	if (enable)
		val |= 0x1;
	else
		val &= ~0x1;
	msm_camera_io_w_mb(val,
		vfe_base + VFE44_WM_BASE(wm_idx));
}

static void msm_vfe44_axi_update_cgc_override(struct vfe_device *vfe_dev,
	uint8_t wm_idx, uint8_t cgc_override)
{
	uint32_t val = 0;

	/* Change CGC override */
	val = msm_camera_io_r(vfe_dev->vfe_base + 0x974);
	if (cgc_override)
		val |= (1 << wm_idx);
	else
		val &= ~(1 << wm_idx);
	msm_camera_io_w_mb(val, vfe_dev->vfe_base + 0x974);
}

static void msm_vfe44_axi_cfg_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t comp_mask, comp_mask_index;

	comp_mask_index = stream_info->comp_mask_index[vfe_idx];

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x40);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	comp_mask |= (axi_data->composite_info[comp_mask_index].
		stream_composite_mask << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x40);

	msm_vfe44_config_irq(vfe_dev, 1 << (comp_mask_index + 25), 0,
			MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe44_axi_clear_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t comp_mask, comp_mask_index;

	comp_mask_index = stream_info->comp_mask_index[vfe_idx];

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x40);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x40);

	msm_vfe44_config_irq(vfe_dev, (1 << (comp_mask_index + 25)), 0,
			MSM_ISP_IRQ_DISABLE);
}

static void msm_vfe44_axi_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	msm_vfe44_config_irq(vfe_dev, 1 << (stream_info->wm[vfe_idx][0] + 8), 0,
			MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe44_axi_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	msm_vfe44_config_irq(vfe_dev, (1 << (stream_info->wm[vfe_idx][0] + 8)),
			0, MSM_ISP_IRQ_DISABLE);
}

static void msm_vfe44_cfg_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t framedrop_pattern,
	uint32_t framedrop_period)
{
	void __iomem *vfe_base = vfe_dev->vfe_base;
	uint32_t i, temp;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++) {
		msm_camera_io_w(framedrop_pattern, vfe_base +
			VFE44_WM_BASE(stream_info->wm[vfe_idx][i]) + 0x1C);
		temp = msm_camera_io_r(vfe_base +
			VFE44_WM_BASE(stream_info->wm[vfe_idx][i]) + 0xC);
		temp &= 0xFFFFFF83;
		msm_camera_io_w(temp | (framedrop_period - 1) << 2,
			vfe_base +
			VFE44_WM_BASE(stream_info->wm[vfe_idx][i]) + 0xC);
	}
}

static void msm_vfe44_clear_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t i;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++)
		msm_camera_io_w(0, vfe_dev->vfe_base +
			VFE44_WM_BASE(stream_info->wm[vfe_idx][i]) + 0x1C);
}

static int32_t msm_vfe44_convert_bpp_to_reg(int32_t bpp, uint32_t *bpp_reg)
{
	int rc = 0;
	switch (bpp) {
	case 8:
		*bpp_reg = 0;
		break;
	case 10:
		*bpp_reg = 1 << 0;
		break;
	case 12:
		*bpp_reg = 1 << 1;
		break;
	default:
		pr_err("%s:%d invalid bpp %d", __func__, __LINE__, bpp);
		return -EINVAL;
	}

	return rc;
}

static int32_t msm_vfe44_convert_io_fmt_to_reg(
	enum msm_isp_pack_fmt pack_format, uint32_t *pack_reg)
{
	int rc = 0;

	switch (pack_format) {
	case QCOM:
		*pack_reg = 0x0;
		break;
	case MIPI:
		*pack_reg = 0x1;
		break;
	case DPCM6:
		*pack_reg = 0x2;
		break;
	case DPCM8:
		*pack_reg = 0x3;
		break;
	case PLAIN8:
		*pack_reg = 0x4;
		break;
	case PLAIN16:
		*pack_reg = 0x5;
		break;
	default:
		pr_err("%s: invalid pack fmt %d!\n", __func__, pack_format);
		return -EINVAL;
	}

	return rc;
}

static int32_t msm_vfe44_cfg_io_format(struct vfe_device *vfe_dev,
	enum msm_vfe_axi_stream_src stream_src, uint32_t io_format)
{
	int rc = 0;
	int bpp = 0, read_bpp = 0;
	enum msm_isp_pack_fmt pack_fmt = 0, read_pack_fmt = 0;
	uint32_t bpp_reg = 0, pack_reg = 0;
	uint32_t read_bpp_reg = 0, read_pack_reg = 0;
	uint32_t io_format_reg = 0; /*io format register bit*/

	io_format_reg = msm_camera_io_r(vfe_dev->vfe_base + 0x54);

	/*input config*/
	if ((stream_src < RDI_INTF_0) &&
		(vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux ==
		EXTERNAL_READ)) {
		read_bpp = msm_isp_get_bit_per_pixel(
			vfe_dev->axi_data.src_info[VFE_PIX_0].input_format);
		rc = msm_vfe44_convert_bpp_to_reg(read_bpp, &read_bpp_reg);
		if (rc < 0) {
			pr_err("%s: convert_bpp_to_reg err! in_bpp %d rc %d\n",
				__func__, read_bpp, rc);
			return rc;
		}

		read_pack_fmt = msm_isp_get_pack_format(
			vfe_dev->axi_data.src_info[VFE_PIX_0].input_format);
		rc = msm_vfe44_convert_io_fmt_to_reg(
			read_pack_fmt, &read_pack_reg);
		if (rc < 0) {
			pr_err("%s: convert_io_fmt_to_reg err! rc = %d\n",
				__func__, rc);
			return rc;
		}
		/*use input format(v4l2_pix_fmt) to get pack format*/
		io_format_reg &= 0xFFC8FFFF;
		io_format_reg |= (read_bpp_reg << 20 | read_pack_reg << 16);
	}

	bpp = msm_isp_get_bit_per_pixel(io_format);
	rc = msm_vfe44_convert_bpp_to_reg(bpp, &bpp_reg);
	if (rc < 0) {
		pr_err("%s: convert_bpp_to_reg err! bpp %d rc = %d\n",
			__func__, bpp, rc);
		return rc;
	}

	switch (stream_src) {
	case PIX_ENCODER:
	case PIX_VIEWFINDER:
	case CAMIF_RAW:
		io_format_reg &= 0xFFFFCFFF;
		io_format_reg |= bpp_reg << 12;
		break;
	case IDEAL_RAW:
		/*use output format(v4l2_pix_fmt) to get pack format*/
		pack_fmt = msm_isp_get_pack_format(io_format);
		rc = msm_vfe44_convert_io_fmt_to_reg(pack_fmt, &pack_reg);
		if (rc < 0) {
			pr_err("%s: convert_io_fmt_to_reg err! rc = %d\n",
				__func__, rc);
			return rc;
		}
		io_format_reg &= 0xFFFFFFC8;
		io_format_reg |= bpp_reg << 4 | pack_reg;
		break;
	case RDI_INTF_0:
	case RDI_INTF_1:
	case RDI_INTF_2:
	default:
		pr_err("%s: Invalid stream source\n", __func__);
		return -EINVAL;
	}

	msm_camera_io_w(io_format_reg, vfe_dev->vfe_base + 0x54);
	return 0;
}

static int msm_vfe44_fetch_engine_start(struct vfe_device *vfe_dev,
	void *arg)
{
	int rc = 0;
	uint32_t bufq_handle;
	struct msm_isp_buffer *buf = NULL;
	struct msm_vfe_fetch_eng_start *fe_cfg = arg;
	struct msm_isp_buffer_mapped_info mapped_info;

	if (vfe_dev->fetch_engine_info.is_busy == 1) {
		pr_err("%s: fetch engine busy\n", __func__);
		return -EINVAL;
	}

	memset(&mapped_info, 0, sizeof(struct msm_isp_buffer_mapped_info));
	/* There is other option of passing buffer address from user,
		in such case, driver needs to map the buffer and use it*/
	vfe_dev->fetch_engine_info.session_id = fe_cfg->session_id;
	vfe_dev->fetch_engine_info.stream_id = fe_cfg->stream_id;
	vfe_dev->fetch_engine_info.offline_mode = fe_cfg->offline_mode;
	vfe_dev->fetch_engine_info.fd = fe_cfg->fd;

	if (!fe_cfg->offline_mode) {
		bufq_handle = vfe_dev->buf_mgr->ops->get_bufq_handle(
			vfe_dev->buf_mgr, fe_cfg->session_id,
			fe_cfg->stream_id);
		vfe_dev->fetch_engine_info.bufq_handle = bufq_handle;
		rc = vfe_dev->buf_mgr->ops->get_buf_by_index(
			vfe_dev->buf_mgr, bufq_handle, fe_cfg->buf_idx, &buf);
		if (rc < 0) {
			pr_err("%s: No fetch buffer\n", __func__);
			return -EINVAL;
		}
		mapped_info = buf->mapped_info[0];
		buf->state = MSM_ISP_BUFFER_STATE_DISPATCHED;
	} else {
		rc = vfe_dev->buf_mgr->ops->map_buf(vfe_dev->buf_mgr,
			&mapped_info, fe_cfg->fd);
		if (rc < 0) {
			pr_err("%s: can not map buffer\n", __func__);
			return -EINVAL;
		}
	}
	vfe_dev->fetch_engine_info.buf_idx = fe_cfg->buf_idx;
	vfe_dev->fetch_engine_info.is_busy = 1;

	msm_camera_io_w(mapped_info.paddr, vfe_dev->vfe_base + 0x228);

	msm_camera_io_w_mb(0x10000, vfe_dev->vfe_base + 0x4C);
	msm_camera_io_w_mb(0x20000, vfe_dev->vfe_base + 0x4C);

	ISP_DBG("%s: Fetch Engine ready\n", __func__);
	return 0;
}

static void msm_vfe44_cfg_fetch_engine(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint32_t x_size_word;
	struct msm_vfe_fetch_engine_cfg *fe_cfg = NULL;
	uint32_t temp = 0;

	if (pix_cfg->input_mux == EXTERNAL_READ) {
		fe_cfg = &pix_cfg->fetch_engine_cfg;
		pr_debug("%s: fetch_dbg wd x ht buf = %d x %d, fe = %d x %d\n",
			__func__, fe_cfg->buf_width, fe_cfg->buf_height,
			fe_cfg->fetch_width, fe_cfg->fetch_height);

		vfe_dev->hw_info->vfe_ops.axi_ops.update_cgc_override(vfe_dev,
			VFE44_BUS_RD_CGC_OVERRIDE_BIT, 1);

		temp = msm_camera_io_r(vfe_dev->vfe_base + 0x50);
		temp &= 0xFFFFFFFD;
		temp |= (1 << 1);
		msm_camera_io_w(temp, vfe_dev->vfe_base + 0x50);

		msm_vfe44_config_irq(vfe_dev, (1 << 24), 0,
					MSM_ISP_IRQ_SET);
		msm_camera_io_w((fe_cfg->fetch_height - 1) & 0xFFF,
			vfe_dev->vfe_base + 0x238);

		x_size_word = msm_isp_cal_word_per_line(
			vfe_dev->axi_data.src_info[VFE_PIX_0].input_format,
			fe_cfg->fetch_width);
		msm_camera_io_w((x_size_word - 1) << 16,
			vfe_dev->vfe_base + 0x23C);

		msm_camera_io_w(x_size_word << 16 |
			(fe_cfg->buf_height - 1) << 4 | VFE44_FETCH_BURST_LEN,
			vfe_dev->vfe_base + 0x240);

		msm_camera_io_w(0 << 28 | 2 << 25 |
		((fe_cfg->buf_width - 1) & 0x1FFF) << 12 |
		((fe_cfg->buf_height - 1) & 0xFFF), vfe_dev->vfe_base + 0x244);

		/* need to use formulae to calculate MAIN_UNPACK_PATTERN*/
		msm_camera_io_w(0xF6543210, vfe_dev->vfe_base + 0x248);
		msm_camera_io_w(0xF, vfe_dev->vfe_base + 0x264);

		temp = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
		temp |= 2 << 16 | pix_cfg->pixel_pattern;
		msm_camera_io_w(temp, vfe_dev->vfe_base + 0x1C);

	} else {
		pr_err("%s: Invalid mux configuration - mux: %d", __func__,
			pix_cfg->input_mux);
		return;
	}

	return;
}

static void msm_vfe44_cfg_camif(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint16_t first_pixel, last_pixel, first_line, last_line;
	struct msm_vfe_camif_cfg *camif_cfg = &pix_cfg->camif_cfg;
	uint32_t val, subsample_period, subsample_pattern;
	struct msm_vfe_camif_subsample_cfg *subsample_cfg =
		&pix_cfg->camif_cfg.subsample_cfg;
	uint16_t bus_sub_en = 0;

	vfe_dev->dual_vfe_enable = camif_cfg->is_split;

	msm_camera_io_w(pix_cfg->input_mux << 16 | pix_cfg->pixel_pattern,
		vfe_dev->vfe_base + 0x1C);

	if (subsample_cfg->pixel_skip || subsample_cfg->line_skip) {
		bus_sub_en = 1;
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x2F8);
		val &= 0xFFFFFFDF;
		val = val | bus_sub_en << 5;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x2F8);
		subsample_cfg->pixel_skip &= 0x0000FFFF;
		subsample_cfg->line_skip  &= 0x0000FFFF;
		msm_camera_io_w((subsample_cfg->line_skip << 16) |
			subsample_cfg->pixel_skip,
			vfe_dev->vfe_base + 0x30C);
	}

	first_pixel = camif_cfg->first_pixel;
	last_pixel = camif_cfg->last_pixel;
	first_line = camif_cfg->first_line;
	last_line = camif_cfg->last_line;
	subsample_period = camif_cfg->subsample_cfg.irq_subsample_period;
	subsample_pattern = camif_cfg->subsample_cfg.irq_subsample_pattern;

	msm_camera_io_w(camif_cfg->lines_per_frame << 16 |
		camif_cfg->pixels_per_line, vfe_dev->vfe_base + 0x300);

	msm_camera_io_w(first_pixel << 16 | last_pixel,
	vfe_dev->vfe_base + 0x304);

	msm_camera_io_w(first_line << 16 | last_line,
	vfe_dev->vfe_base + 0x308);
	if (subsample_period && subsample_pattern) {
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x2F8);
		val &= 0xFFE0FFFF;
		val = (subsample_period - 1) << 16;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x2F8);
		ISP_DBG("%s:camif PERIOD %x PATTERN %x\n",
			__func__,  subsample_period, subsample_pattern);

		val = subsample_pattern;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x314);
	} else {
		msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x314);
	}
	val = msm_camera_io_r(vfe_dev->vfe_base + 0x2E8);
	val |= camif_cfg->camif_input;
	msm_camera_io_w(val, vfe_dev->vfe_base + 0x2E8);

}

static void msm_vfe44_cfg_input_mux(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	switch (pix_cfg->input_mux) {
	case CAMIF:
		msm_vfe44_cfg_camif(vfe_dev, pix_cfg);
		break;
	case EXTERNAL_READ:
		msm_vfe44_cfg_fetch_engine(vfe_dev, pix_cfg);
		break;
	default:
		pr_err("%s: Unsupported input mux %d\n",
			__func__, pix_cfg->input_mux);
	}
	return;
}

static void msm_vfe44_update_camif_state(struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state update_state)
{
	uint32_t val;
	bool bus_en, vfe_en;

	if (update_state == NO_UPDATE)
		return;

	if (update_state == ENABLE_CAMIF) {
		msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x30);
		msm_camera_io_w_mb(0x81, vfe_dev->vfe_base + 0x34);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x24);

		msm_vfe44_config_irq(vfe_dev, 0xF7, 0x81,
				MSM_ISP_IRQ_ENABLE);
		msm_camera_io_w_mb(0x140000, vfe_dev->vfe_base + 0x318);

		bus_en =
			((vfe_dev->axi_data.
			src_info[VFE_PIX_0].raw_stream_count > 0) ? 1 : 0);
		vfe_en =
			((vfe_dev->axi_data.
			src_info[VFE_PIX_0].stream_count > 0) ? 1 : 0);
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x2F8);
		val &= 0xFFFFFF3F;
		val = val | bus_en << 7 | vfe_en << 6;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x2F8);
		msm_camera_io_w_mb(0x4, vfe_dev->vfe_base + 0x2F4);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x2F4);

		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 1;
	} else if (update_state == DISABLE_CAMIF ||
		DISABLE_CAMIF_IMMEDIATELY == update_state) {
		uint32_t poll_val;

		if (vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux == TESTGEN)
			update_state = DISABLE_CAMIF;
		msm_vfe44_config_irq(vfe_dev, 0,
			0x81, MSM_ISP_IRQ_DISABLE);
		val = msm_camera_io_r(vfe_dev->vfe_base + 0xC18);
		/* disable danger signal */
		msm_camera_io_w_mb(val & ~(1 << 8), vfe_dev->vfe_base + 0xC18);
		msm_camera_io_w_mb((update_state == DISABLE_CAMIF ? 0x0 : 0x6),
				vfe_dev->vfe_base + 0x2F4);
		if (readl_poll_timeout_atomic(vfe_dev->vfe_base + 0x31C,
				poll_val, poll_val & 0x80000000, 1000, 2000000))
			pr_err("%s: camif disable failed %x\n",
				__func__, poll_val);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 0;
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x30);
		msm_camera_io_w((1 << 0), vfe_dev->vfe_base + 0x34);
		msm_camera_io_w_mb(1, vfe_dev->vfe_base + 0x24);
		msm_vfe44_config_irq(vfe_dev, vfe_dev->irq0_mask,
			vfe_dev->irq1_mask, MSM_ISP_IRQ_SET);
	}
}

static void msm_vfe44_cfg_rdi_reg(
	struct vfe_device *vfe_dev, struct msm_vfe_rdi_cfg *rdi_cfg,
	enum msm_vfe_input_src input_src)
{
	uint8_t rdi = input_src - VFE_RAW_0;
	uint32_t rdi_reg_cfg;
	rdi_reg_cfg = msm_camera_io_r(
		vfe_dev->vfe_base + VFE44_RDI_BASE(0));
	rdi_reg_cfg &= ~(BIT(16 + rdi));
	rdi_reg_cfg |= rdi_cfg->frame_based << (16 + rdi);
	msm_camera_io_w(rdi_reg_cfg,
		vfe_dev->vfe_base + VFE44_RDI_BASE(0));

	rdi_reg_cfg = msm_camera_io_r(
		vfe_dev->vfe_base + VFE44_RDI_BASE(rdi));
	rdi_reg_cfg &= 0x70003;
	rdi_reg_cfg |= (rdi * 3) << 28 | rdi_cfg->cid << 4 | 0x4;
	msm_camera_io_w(
		rdi_reg_cfg, vfe_dev->vfe_base + VFE44_RDI_BASE(rdi));
}

static void msm_vfe44_axi_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx)
{
	uint32_t val;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t wm_base;

	wm_base = VFE44_WM_BASE(stream_info->wm[vfe_idx][plane_idx]);

	if (!stream_info->frame_based) {
		msm_camera_io_w(0x0, vfe_dev->vfe_base + wm_base);
		/*WR_IMAGE_SIZE*/
		val =
			((msm_isp_cal_word_per_line(
				stream_info->output_format,
				stream_info->plane_cfg[vfe_idx][plane_idx].
				output_width)+1)/2 - 1) << 16 |
				(stream_info->plane_cfg[vfe_idx][plane_idx].
				output_height - 1);
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);

		/*WR_BUFFER_CFG*/
		val = (stream_info->plane_cfg[vfe_idx][plane_idx].
				output_height - 1);
		val = (((val & 0xfff) << 2) | ((val >> 12) & 0x3));
		val = val << 2 |
			msm_isp_cal_word_per_line(stream_info->output_format,
			stream_info->plane_cfg[vfe_idx][
				plane_idx].output_stride) << 16 |
			VFE44_BURST_LEN;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x18);
	} else {
		msm_camera_io_w(0x2, vfe_dev->vfe_base + wm_base);
		val = (stream_info->plane_cfg[vfe_idx][plane_idx].
							output_height - 1);
		val = (((val & 0xfff) << 2) | ((val >> 12) & 0x3));
		val = val << 2 |
			msm_isp_cal_word_per_line(stream_info->output_format,
			stream_info->plane_cfg[vfe_idx][
				plane_idx].output_width) << 16 |
			VFE44_BURST_LEN;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x18);
	}

	/*WR_IRQ_SUBSAMPLE_PATTERN*/
	msm_camera_io_w(0xFFFFFFFF,
		vfe_dev->vfe_base + wm_base + 0x20);
	/* TD: Add IRQ subsample pattern */
}

static void msm_vfe44_axi_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint32_t val = 0;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t wm_base;

	wm_base = VFE44_WM_BASE(stream_info->wm[vfe_idx][plane_idx]);
	/*WR_ADDR_CFG*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0xC);
	/*WR_IMAGE_SIZE*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	/*WR_BUFFER_CFG*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x18);
	/*WR_IRQ_SUBSAMPLE_PATTERN*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x20);
}

static void msm_vfe44_axi_cfg_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	struct msm_vfe_axi_plane_cfg *plane_cfg;
	uint8_t wm;
	uint32_t xbar_cfg = 0;
	uint32_t xbar_reg_cfg = 0;

	plane_cfg = &stream_info->plane_cfg[vfe_idx][plane_idx];
	wm = stream_info->wm[vfe_idx][plane_idx];

	switch (stream_info->stream_src) {
	case PIX_ENCODER:
	case PIX_VIEWFINDER: {
		if (plane_cfg->output_plane_format != CRCB_PLANE &&
			plane_cfg->output_plane_format != CBCR_PLANE) {
			/*SINGLE_STREAM_SEL*/
			xbar_cfg |= plane_cfg->output_plane_format << 8;
		} else {
			switch (stream_info->output_format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV14:
			case V4L2_PIX_FMT_NV16:
			case V4L2_PIX_FMT_NV24:
				xbar_cfg |= 0x3 << 4; /*PAIR_STREAM_SWAP_CTRL*/
				break;
			}
			xbar_cfg |= 0x1 << 1; /*PAIR_STREAM_EN*/
		}
		if (stream_info->stream_src == PIX_VIEWFINDER)
			xbar_cfg |= 0x1; /*VIEW_STREAM_EN*/
		break;
	}
	case CAMIF_RAW:
		xbar_cfg = 0x300;
		break;
	case IDEAL_RAW:
		xbar_cfg = 0x400;
		break;
	case RDI_INTF_0:
		xbar_cfg = 0x500;
		break;
	case RDI_INTF_1:
		xbar_cfg = 0x600;
		break;
	case RDI_INTF_2:
		xbar_cfg = 0x700;
		break;
	default:
		pr_err("%s: Invalid stream src\n", __func__);
		break;
	}
	xbar_reg_cfg =
		msm_camera_io_r(vfe_dev->vfe_base + VFE44_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFFFF << VFE44_XBAR_SHIFT(wm));
	xbar_reg_cfg |= (xbar_cfg << VFE44_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg,
		vfe_dev->vfe_base + VFE44_XBAR_BASE(wm));
}

static void msm_vfe44_axi_clear_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint8_t wm;
	uint32_t xbar_reg_cfg = 0;

	wm = stream_info->wm[vfe_idx][plane_idx];

	xbar_reg_cfg =
		msm_camera_io_r(vfe_dev->vfe_base + VFE44_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFFFF << VFE44_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg,
		vfe_dev->vfe_base + VFE44_XBAR_BASE(wm));
}

static void msm_vfe44_read_wm_ping_pong_addr(
	struct vfe_device *vfe_dev)
{
	msm_camera_io_dump(vfe_dev->vfe_base +
		(VFE44_WM_BASE(0) & 0xFFFFFFF0), 0x200, 1);
}

static void msm_vfe44_update_ping_pong_addr(
	void __iomem *vfe_base,
	uint8_t wm_idx, uint32_t pingpong_bit, dma_addr_t paddr,
	int32_t buf_size)
{
	uint32_t paddr32 = (paddr & 0xFFFFFFFF);
	msm_camera_io_w(paddr32, vfe_base +
		VFE44_PING_PONG_BASE(wm_idx, pingpong_bit));
}

static void msm_vfe44_set_halt_restart_mask(struct vfe_device *vfe_dev)
{
	msm_vfe44_config_irq(vfe_dev, BIT(31), BIT(8), MSM_ISP_IRQ_SET);
}


static int msm_vfe44_axi_halt(struct vfe_device *vfe_dev,
	uint32_t blocking)
{
	int rc = 0;
	enum msm_vfe_input_src i;
	struct msm_isp_timestamp ts;

	/* Keep only halt and restart mask */
	msm_vfe44_config_irq(vfe_dev, (1 << 31), (1 << 8),
			MSM_ISP_IRQ_SET);

	if (atomic_read(&vfe_dev->error_info.overflow_state)
		== OVERFLOW_DETECTED)
		pr_err_ratelimited("%s: VFE%d halt for recovery, blocking %d\n",
			__func__, vfe_dev->pdev->id, blocking);

	if (blocking) {
		init_completion(&vfe_dev->halt_complete);
		/* Halt AXI Bus Bridge */
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x2C0);
		rc = wait_for_completion_timeout(
			&vfe_dev->halt_complete, msecs_to_jiffies(500));
		if (rc <= 0)
			pr_err("%s:VFE%d halt timeout rc=%d\n", __func__,
				vfe_dev->pdev->id, rc);
	} else {
		/* Halt AXI Bus Bridge */
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x2C0);
	}

	msm_isp_get_timestamp(&ts, vfe_dev);
	for (i = VFE_PIX_0; i <= VFE_RAW_2; i++) {
		/* if any stream is waiting for update, signal complete */
		msm_isp_axi_stream_update(vfe_dev, i, &ts);
		msm_isp_axi_stream_update(vfe_dev, i, &ts);
	}

	msm_isp_stats_stream_update(vfe_dev);
	msm_isp_stats_stream_update(vfe_dev);

	return rc;
}

static void msm_vfe44_axi_restart(struct vfe_device *vfe_dev,
	uint32_t blocking, uint32_t enable_camif)
{
	msm_vfe44_config_irq(vfe_dev, vfe_dev->recovery_irq0_mask,
			vfe_dev->recovery_irq1_mask, MSM_ISP_IRQ_ENABLE);
	msm_camera_io_w_mb(0x140000, vfe_dev->vfe_base + 0x318);

	/* Start AXI */
	msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x2C0);

	vfe_dev->hw_info->vfe_ops.core_ops.reg_update(vfe_dev, VFE_SRC_MAX);
	memset(&vfe_dev->error_info, 0, sizeof(vfe_dev->error_info));
	atomic_set(&vfe_dev->error_info.overflow_state, NO_OVERFLOW);
	if (enable_camif)
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, ENABLE_CAMIF);
}

static uint32_t msm_vfe44_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 8) & 0x7F;
}

static uint32_t msm_vfe44_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 25) & 0xF;
}

static uint32_t msm_vfe44_get_pingpong_status(
	struct vfe_device *vfe_dev)
{
	return msm_camera_io_r(vfe_dev->vfe_base + 0x268);
}

static int msm_vfe44_get_stats_idx(enum msm_isp_stats_type stats_type)
{
	switch (stats_type) {
	case MSM_ISP_STATS_BE:
		return STATS_IDX_BE;
	case MSM_ISP_STATS_BG:
		return STATS_IDX_BG;
	case MSM_ISP_STATS_BF:
		return STATS_IDX_BF;
	case MSM_ISP_STATS_AWB:
		return STATS_IDX_AWB;
	case MSM_ISP_STATS_RS:
		return STATS_IDX_RS;
	case MSM_ISP_STATS_CS:
		return STATS_IDX_CS;
	case MSM_ISP_STATS_IHIST:
		return STATS_IDX_IHIST;
	case MSM_ISP_STATS_BHIST:
		return STATS_IDX_BHIST;
	case MSM_ISP_STATS_BF_SCALE:
		return STATS_IDX_BF_SCALE;
	default:
		pr_err("%s: Invalid stats type\n", __func__);
		return -EINVAL;
	}
}

static int msm_vfe44_stats_check_streams(
	struct msm_vfe_stats_stream *stream_info)
{
	if (stream_info[STATS_IDX_BF].state ==
		STATS_AVAILABLE &&
		stream_info[STATS_IDX_BF_SCALE].state !=
		STATS_AVAILABLE) {
		pr_err("%s: does not support BF_SCALE while BF is disabled\n",
			__func__);
		return -EINVAL;
	}
	if (stream_info[STATS_IDX_BF].state != STATS_AVAILABLE &&
		stream_info[STATS_IDX_BF_SCALE].state != STATS_AVAILABLE &&
		stream_info[STATS_IDX_BF].composite_flag !=
		stream_info[STATS_IDX_BF_SCALE].composite_flag) {
		pr_err("%s: Different composite flag for BF and BF_SCALE\n",
			__func__);
		return -EINVAL;
	}
	return 0;
}

static void msm_vfe44_stats_cfg_comp_mask(
	struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t request_comp_index, uint8_t enable)
{
	uint32_t comp_mask_reg, mask_bf_scale;
	atomic_t *stats_comp_mask;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;

	if (vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask < 1)
		return;

	if (request_comp_index >= MAX_NUM_STATS_COMP_MASK) {
		pr_err("%s: num of comp masks %d exceed max %d\n",
			__func__, request_comp_index,
			MAX_NUM_STATS_COMP_MASK);
		return;
	}

	if (vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask >
			MAX_NUM_STATS_COMP_MASK) {
		pr_err("%s: num of comp masks %d exceed max %d\n",
			__func__,
			vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask,
			MAX_NUM_STATS_COMP_MASK);
		return;
	}

	/* BF scale is controlled by BF also so ignore bit 0 of BF scale */
	stats_mask = stats_mask & 0x1FF;
	mask_bf_scale = stats_mask >> SHIFT_BF_SCALE_BIT;

	stats_comp_mask = &stats_data->stats_comp_mask[request_comp_index];
	comp_mask_reg = msm_camera_io_r(vfe_dev->vfe_base + 0x44);

	if (enable) {
		comp_mask_reg |= mask_bf_scale << (16 + request_comp_index * 8);
		atomic_set(stats_comp_mask, stats_mask |
				atomic_read(stats_comp_mask));
		msm_vfe44_config_irq(vfe_dev,
			1 << (request_comp_index + 29), 0,
			MSM_ISP_IRQ_ENABLE);
	} else {
		if (!(atomic_read(stats_comp_mask) & stats_mask))
			return;
		if (stats_mask & (1 << STATS_IDX_BF_SCALE) &&
			atomic_read(stats_comp_mask) &
				(1 << STATS_IDX_BF_SCALE))
			atomic_set(stats_comp_mask,
					~(1 << STATS_IDX_BF_SCALE) &
					atomic_read(stats_comp_mask));

		atomic_set(stats_comp_mask,
				~stats_mask & atomic_read(stats_comp_mask));
		comp_mask_reg &= ~(mask_bf_scale <<
			(16 + request_comp_index * 8));
		msm_vfe44_config_irq(vfe_dev,
			1 << (request_comp_index + 29), 0,
			MSM_ISP_IRQ_DISABLE);
	}
	msm_camera_io_w(comp_mask_reg, vfe_dev->vfe_base + 0x44);

	ISP_DBG("%s: comp_mask_reg: %x comp mask0 %x mask1: %x\n",
		__func__, comp_mask_reg,
		atomic_read(&stats_data->stats_comp_mask[0]),
		atomic_read(&stats_data->stats_comp_mask[1]));

	return;
}

static void msm_vfe44_stats_cfg_wm_irq_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);

	msm_vfe44_config_irq(vfe_dev,
		1 << (STATS_IDX(stream_info->stream_handle[vfe_idx]) + 15), 0,
		MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe44_stats_clear_wm_irq_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);

	msm_vfe44_config_irq(vfe_dev,
		(1 << (STATS_IDX(stream_info->stream_handle[vfe_idx]) + 15)), 0,
		MSM_ISP_IRQ_DISABLE);
}

static void msm_vfe44_stats_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);
	int stats_idx;
	uint32_t stats_base;

	stats_idx = STATS_IDX(stream_info->stream_handle[vfe_idx]);
	stats_base = VFE44_STATS_BASE(stats_idx);
	/* BF_SCALE does not have its own WR_ADDR_CFG,
	 * IRQ_FRAMEDROP_PATTERN and IRQ_SUBSAMPLE_PATTERN;
	 * it's using the same from BF */
	if (stats_idx == STATS_IDX_BF_SCALE)
		return;
	/*WR_ADDR_CFG*/
	msm_camera_io_w((stream_info->framedrop_period - 1) << 2,
		vfe_dev->vfe_base + stats_base + 0x8);
	/*WR_IRQ_FRAMEDROP_PATTERN*/
	msm_camera_io_w(stream_info->framedrop_pattern,
		vfe_dev->vfe_base + stats_base + 0x10);
	/*WR_IRQ_SUBSAMPLE_PATTERN*/
	msm_camera_io_w(0xFFFFFFFF,
		vfe_dev->vfe_base + stats_base + 0x14);
}

static void msm_vfe44_stats_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);
	uint32_t val = 0;
	int stats_idx;
	uint32_t stats_base;

	stats_idx = STATS_IDX(stream_info->stream_handle[vfe_idx]);
	stats_base = VFE44_STATS_BASE(stats_idx);
	/* BF_SCALE does not have its own WR_ADDR_CFG,
	 * IRQ_FRAMEDROP_PATTERN and IRQ_SUBSAMPLE_PATTERN;
	 * it's using the same from BF */
	if (stats_idx == STATS_IDX_BF_SCALE)
		return;

	/*WR_ADDR_CFG*/
	msm_camera_io_w(val, vfe_dev->vfe_base + stats_base + 0x8);
	/*WR_IRQ_FRAMEDROP_PATTERN*/
	msm_camera_io_w(val, vfe_dev->vfe_base + stats_base + 0x10);
	/*WR_IRQ_SUBSAMPLE_PATTERN*/
	msm_camera_io_w(val, vfe_dev->vfe_base + stats_base + 0x14);
}

static void msm_vfe44_stats_cfg_ub(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = VFE44_UB_SIZE;
	uint32_t ub_size[VFE44_NUM_STATS_TYPE] = {
		128, /*MSM_ISP_STATS_BF_SCALE*/
		64, /*MSM_ISP_STATS_BE*/
		128, /*MSM_ISP_STATS_BG*/
		128, /*MSM_ISP_STATS_BF*/
		16, /*MSM_ISP_STATS_AWB*/
		8,  /*MSM_ISP_STATS_RS*/
		16, /*MSM_ISP_STATS_CS*/
		16, /*MSM_ISP_STATS_IHIST*/
		16, /*MSM_ISP_STATS_BHIST*/
	};

	for (i = 0; i < VFE44_NUM_STATS_TYPE; i++) {
		ub_offset -= ub_size[i];
		msm_camera_io_w(VFE44_STATS_BURST_LEN << 30 |
			ub_offset << 16 | (ub_size[i] - 1),
			vfe_dev->vfe_base + VFE44_STATS_BASE(i) +
			((i == STATS_IDX_BF_SCALE) ? 0x8 : 0xC));
	}
}

static bool msm_vfe44_is_module_cfg_lock_needed(
	uint32_t reg_offset)
{
	if (reg_offset == 0x18)
		return true;
	else
		return false;
}

static void msm_vfe44_stats_enable_module(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	int i;
	uint32_t module_cfg, module_cfg_mask = 0;
	uint32_t stats_cfg, stats_cfg_mask = 0;
	unsigned long flags;

	for (i = 0; i < VFE44_NUM_STATS_TYPE; i++) {
		if ((stats_mask >> i) & 0x1) {
			switch (i) {
			case STATS_IDX_BE:
			case STATS_IDX_BG:
			case STATS_IDX_BF:
			case STATS_IDX_AWB:
			case STATS_IDX_RS:
			case STATS_IDX_CS:
				module_cfg_mask |= 1 << (4 + i);
				break;
			case STATS_IDX_IHIST:
				module_cfg_mask |= 1 << 15;
				break;
			case STATS_IDX_BHIST:
				module_cfg_mask |= 1 << 18;
				break;
			case STATS_IDX_BF_SCALE:
				stats_cfg_mask |= 1 << 2;
				break;
			default:
				pr_err("%s: Invalid stats mask\n", __func__);
				return;
			}
		}
	}

	/*
	 * For vfe44 stats and other modules share module_cfg register.
	 * Hence need to Grab lock.
	 */
	spin_lock_irqsave(&vfe_dev->shared_data_lock, flags);
	module_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x18);
	if (enable)
		module_cfg |= module_cfg_mask;
	else
		module_cfg &= ~module_cfg_mask;
	msm_camera_io_w(module_cfg, vfe_dev->vfe_base + 0x18);
	spin_unlock_irqrestore(&vfe_dev->shared_data_lock, flags);

	stats_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x888);
	if (enable)
		stats_cfg |= stats_cfg_mask;
	else
		stats_cfg &= ~stats_cfg_mask;
	msm_camera_io_w(stats_cfg, vfe_dev->vfe_base + 0x888);
}

static void msm_vfe44_stats_update_cgc_override(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t cgc_override)
{
	int i;
	uint32_t val = 0, cgc_mask = 0;

	for (i = 0; i < VFE44_NUM_STATS_TYPE; i++) {
		if ((stats_mask >> i) & 0x1) {
			switch (i) {
			case STATS_IDX_BE:
				cgc_mask |= (1 << 8);
				break;
			case STATS_IDX_BG:
				cgc_mask |= (1 << 9);
				break;
			case STATS_IDX_BF:
				cgc_mask |= (1 << 10);
				break;
			case STATS_IDX_AWB:
				cgc_mask |= (1 << 11);
				break;
			case STATS_IDX_RS:
				cgc_mask |= (1 << 12);
				break;
			case STATS_IDX_CS:
				cgc_mask |= (1 << 13);
				break;
			case STATS_IDX_IHIST:
				cgc_mask |= (1 << 14);
				break;
			case STATS_IDX_BHIST:
				cgc_mask |= (1 << 15);
				break;
			case STATS_IDX_BF_SCALE:
				cgc_mask |= (1 << 10);
				break;
			default:
				pr_err("%s: Invalid stats mask\n", __func__);
				return;
			}
		}
	}

	/* CGC override */
	val = msm_camera_io_r(vfe_dev->vfe_base + 0x974);
	if (cgc_override)
		val |= cgc_mask;
	else
		val &= ~cgc_mask;
	msm_camera_io_w(val, vfe_dev->vfe_base + 0x974);
}

static void msm_vfe44_stats_update_ping_pong_addr(
	struct vfe_device *vfe_dev, struct msm_vfe_stats_stream *stream_info,
	uint32_t pingpong_status, dma_addr_t paddr, uint32_t buf_sz)
{
	void __iomem *vfe_base = vfe_dev->vfe_base;
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);
	uint32_t paddr32 = (paddr & 0xFFFFFFFF);
	int stats_idx;

	stats_idx = STATS_IDX(stream_info->stream_handle[vfe_idx]);
	msm_camera_io_w(paddr32, vfe_base +
		VFE44_STATS_PING_PONG_BASE(stats_idx, pingpong_status));
}

static uint32_t msm_vfe44_stats_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 15) & 0x1FF;
}

static uint32_t msm_vfe44_stats_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 29) & 0x3;
}

static uint32_t msm_vfe44_stats_get_frame_id(
	struct vfe_device *vfe_dev)
{
	return vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
}

static void msm_vfe44_get_error_mask(
	uint32_t *error_mask0, uint32_t *error_mask1)
{
	*error_mask0 = 0x00000000;
	*error_mask1 = 0x01FFFEFF;
}

static void msm_vfe44_get_overflow_mask(uint32_t *overflow_mask)
{
	*overflow_mask = 0x00FFFE7E;
}

static void msm_vfe44_get_rdi_wm_mask(struct vfe_device *vfe_dev,
	uint32_t *rdi_wm_mask)
{
	*rdi_wm_mask = vfe_dev->axi_data.rdi_wm_mask;
}

static void msm_vfe44_get_irq_mask(struct vfe_device *vfe_dev,
	uint32_t *irq0_mask, uint32_t *irq1_mask)
{
	*irq0_mask = vfe_dev->irq0_mask;
	*irq1_mask = vfe_dev->irq1_mask;
}

static void msm_vfe44_get_halt_restart_mask(uint32_t *irq0_mask,
	uint32_t *irq1_mask)
{
	*irq0_mask = BIT(31);
	*irq1_mask = BIT(8);
}

static struct msm_vfe_axi_hardware_info msm_vfe44_axi_hw_info = {
	.num_wm = 6,
	.num_comp_mask = 3,
	.num_rdi = 3,
	.num_rdi_master = 3,
	.min_wm_ub = 96,
	.scratch_buf_range = SZ_32M + SZ_4M,
};

static struct msm_vfe_stats_hardware_info msm_vfe44_stats_hw_info = {
	.stats_capability_mask =
		1 << MSM_ISP_STATS_BE | 1 << MSM_ISP_STATS_BF |
		1 << MSM_ISP_STATS_BG | 1 << MSM_ISP_STATS_BHIST |
		1 << MSM_ISP_STATS_AWB | 1 << MSM_ISP_STATS_IHIST |
		1 << MSM_ISP_STATS_RS | 1 << MSM_ISP_STATS_CS |
		1 << MSM_ISP_STATS_BF_SCALE,
	.stats_ping_pong_offset = stats_pingpong_offset_map,
	.num_stats_type = VFE44_NUM_STATS_TYPE,
	.num_stats_comp_mask = VFE44_NUM_STATS_COMP,
};

struct msm_vfe_hardware_info vfe44_hw_info = {
	.num_iommu_ctx = 1,
	.num_iommu_secure_ctx = 1,
	.vfe_clk_idx = VFE44_CLK_IDX,
	.runtime_axi_update = 0,
	.min_ab = 100000000,
	.min_ib = 100000000,
	.vfe_ops = {
		.irq_ops = {
			.read_and_clear_irq_status =
				msm_vfe44_read_and_clear_irq_status,
			.read_irq_status = msm_vfe44_read_irq_status,
			.process_camif_irq = msm_vfe44_process_input_irq,
			.process_reset_irq = msm_vfe44_process_reset_irq,
			.process_halt_irq = msm_vfe44_process_halt_irq,
			.process_reset_irq = msm_vfe44_process_reset_irq,
			.process_reg_update = msm_vfe44_process_reg_update,
			.process_axi_irq = msm_isp_process_axi_irq,
			.process_stats_irq = msm_isp_process_stats_irq,
			.process_epoch_irq = msm_vfe44_process_epoch_irq,
			.config_irq = msm_vfe44_config_irq,
			.preprocess_camif_irq = msm_isp47_preprocess_camif_irq,
		},
		.axi_ops = {
			.reload_wm = msm_vfe44_axi_reload_wm,
			.enable_wm = msm_vfe44_axi_enable_wm,
			.cfg_io_format = msm_vfe44_cfg_io_format,
			.cfg_comp_mask = msm_vfe44_axi_cfg_comp_mask,
			.clear_comp_mask = msm_vfe44_axi_clear_comp_mask,
			.cfg_wm_irq_mask = msm_vfe44_axi_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe44_axi_clear_wm_irq_mask,
			.cfg_framedrop = msm_vfe44_cfg_framedrop,
			.clear_framedrop = msm_vfe44_clear_framedrop,
			.cfg_wm_reg = msm_vfe44_axi_cfg_wm_reg,
			.clear_wm_reg = msm_vfe44_axi_clear_wm_reg,
			.cfg_wm_xbar_reg = msm_vfe44_axi_cfg_wm_xbar_reg,
			.clear_wm_xbar_reg = msm_vfe44_axi_clear_wm_xbar_reg,
			.cfg_ub = msm_vfe47_cfg_axi_ub,
			.read_wm_ping_pong_addr =
				msm_vfe44_read_wm_ping_pong_addr,
			.update_ping_pong_addr =
				msm_vfe44_update_ping_pong_addr,
			.get_comp_mask = msm_vfe44_get_comp_mask,
			.get_wm_mask = msm_vfe44_get_wm_mask,
			.get_pingpong_status = msm_vfe44_get_pingpong_status,
			.halt = msm_vfe44_axi_halt,
			.restart = msm_vfe44_axi_restart,
			.update_cgc_override =
				msm_vfe44_axi_update_cgc_override,
			.ub_reg_offset = msm_vfe44_ub_reg_offset,
			.get_ub_size = msm_vfe44_get_ub_size,
		},
		.core_ops = {
			.reg_update = msm_vfe44_reg_update,
			.cfg_input_mux = msm_vfe44_cfg_input_mux,
			.update_camif_state = msm_vfe44_update_camif_state,
			.start_fetch_eng = msm_vfe44_fetch_engine_start,
			.cfg_rdi_reg = msm_vfe44_cfg_rdi_reg,
			.reset_hw = msm_vfe44_reset_hardware,
			.init_hw = msm_vfe47_init_hardware,
			.init_hw_reg = msm_vfe44_init_hardware_reg,
			.clear_status_reg = msm_vfe44_clear_status_reg,
			.release_hw = msm_vfe47_release_hardware,
			.get_error_mask = msm_vfe44_get_error_mask,
			.get_overflow_mask = msm_vfe44_get_overflow_mask,
			.get_rdi_wm_mask = msm_vfe44_get_rdi_wm_mask,
			.get_irq_mask = msm_vfe44_get_irq_mask,
			.get_halt_restart_mask =
				msm_vfe44_get_halt_restart_mask,
			.process_error_status = msm_vfe44_process_error_status,
			.is_module_cfg_lock_needed =
				msm_vfe44_is_module_cfg_lock_needed,
			.ahb_clk_cfg = NULL,
			.set_halt_restart_mask =
				msm_vfe44_set_halt_restart_mask,
			.set_bus_err_ign_mask = NULL,
			.get_bus_err_mask = NULL,
		},
		.stats_ops = {
			.get_stats_idx = msm_vfe44_get_stats_idx,
			.check_streams = msm_vfe44_stats_check_streams,
			.cfg_comp_mask = msm_vfe44_stats_cfg_comp_mask,
			.cfg_wm_irq_mask = msm_vfe44_stats_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe44_stats_clear_wm_irq_mask,
			.cfg_wm_reg = msm_vfe44_stats_cfg_wm_reg,
			.clear_wm_reg = msm_vfe44_stats_clear_wm_reg,
			.cfg_ub = msm_vfe44_stats_cfg_ub,
			.enable_module = msm_vfe44_stats_enable_module,
			.update_ping_pong_addr =
				msm_vfe44_stats_update_ping_pong_addr,
			.get_comp_mask = msm_vfe44_stats_get_comp_mask,
			.get_wm_mask = msm_vfe44_stats_get_wm_mask,
			.get_frame_id = msm_vfe44_stats_get_frame_id,
			.get_pingpong_status = msm_vfe44_get_pingpong_status,
			.update_cgc_override =
				msm_vfe44_stats_update_cgc_override,
			.enable_stats_wm = NULL,
		},
		.platform_ops = {
			.get_platform_data = msm_vfe47_get_platform_data,
			.enable_regulators = msm_vfe47_enable_regulators,
			.get_regulators = msm_vfe47_get_regulators,
			.put_regulators = msm_vfe47_put_regulators,
			.enable_clks = msm_vfe47_enable_clks,
			.get_clks = msm_vfe47_get_clks,
			.put_clks = msm_vfe47_put_clks,
			.get_clk_rates = msm_vfe47_get_clk_rates,
			.get_max_clk_rate = msm_vfe47_get_max_clk_rate,
			.set_clk_rate = msm_vfe47_set_clk_rate,
			.init_bw_mgr = msm_vfe47_init_bandwidth_mgr,
			.deinit_bw_mgr = msm_vfe47_deinit_bandwidth_mgr,
			.update_bw = msm_vfe47_update_bandwidth,
		}
	},
	.dmi_reg_offset = 0x918,
	.axi_hw_info = &msm_vfe44_axi_hw_info,
	.stats_hw_info = &msm_vfe44_stats_hw_info,
	.regulator_names = {"vdd"},
};
EXPORT_SYMBOL(vfe44_hw_info);

static const struct of_device_id msm_vfe44_dt_match[] = {
	{
		.compatible = "qcom,vfe44",
		.data = &vfe44_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, msm_vfe44_dt_match);

static struct platform_driver vfe44_driver = {
	.probe = vfe_hw_probe,
	.driver = {
		.name = "msm_vfe44",
		.owner = THIS_MODULE,
		.of_match_table = msm_vfe44_dt_match,
	},
};

static int __init msm_vfe44_init_module(void)
{
	return platform_driver_register(&vfe44_driver);
}

static void __exit msm_vfe44_exit_module(void)
{
	platform_driver_unregister(&vfe44_driver);
}

module_init(msm_vfe44_init_module);
module_exit(msm_vfe44_exit_module);
MODULE_DESCRIPTION("MSM VFE44 driver");
MODULE_LICENSE("GPL v2");

