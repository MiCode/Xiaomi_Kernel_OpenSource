/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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

#include "msm_isp46.h"
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
#define STATS_IDX_HDR_BE    1
#define STATS_IDX_BG        2
#define STATS_IDX_BF        3
#define STATS_IDX_HDR_BHIST 4
#define STATS_IDX_RS        5
#define STATS_IDX_CS        6
#define STATS_IDX_IHIST     7
#define STATS_IDX_BHIST     8

#define VFE46_8994V1_VERSION   0x60000000

#define VFE46_BURST_LEN 3
#define VFE46_FETCH_BURST_LEN 3
#define VFE46_STATS_BURST_LEN 3
#define VFE46_UB_SIZE_VFE0 2048
#define VFE46_UB_SIZE_VFE1 1536
#define VFE46_UB_STATS_SIZE 144
#define MSM_ISP46_TOTAL_IMAGE_UB_VFE0 (VFE46_UB_SIZE_VFE0 - VFE46_UB_STATS_SIZE)
#define MSM_ISP46_TOTAL_IMAGE_UB_VFE1 (VFE46_UB_SIZE_VFE1 - VFE46_UB_STATS_SIZE)
#define VFE46_WM_BASE(idx) (0xA0 + 0x24 * idx)
#define VFE46_RDI_BASE(idx) (0x39C + 0x4 * idx)
#define VFE46_XBAR_BASE(idx) (0x90 + 0x4 * (idx / 2))
#define VFE46_XBAR_SHIFT(idx) ((idx%2) ? 16 : 0)
#define VFE46_PING_PONG_BASE(wm, ping_pong) \
	(VFE46_WM_BASE(wm) + 0x4 * (1 + ((~ping_pong) & 0x1)))
#define SHIFT_BF_SCALE_BIT 1
#define VFE46_NUM_STATS_COMP 2
#define VFE46_BUS_RD_CGC_OVERRIDE_BIT 16

static uint32_t stats_base_addr[] = {
	0x1E4, /* BF_SCALE */
	0x19C, /* HDR_BE */
	0x1F0, /* BG */
	0x1CC, /* BF */
	0x1B4, /* HDR_BHIST */
	0x220, /* RS */
	0x238, /* CS */
	0x250, /* IHIST */
	0x208, /* BHIST (SKIN_BHIST) */
};

static uint8_t stats_pingpong_offset_map[] = {
	11, /* BF_SCALE */
	 8, /* HDR_BE */
	12, /* BG */
	10, /* BF */
	 9, /* HDR_BHIST */
	14, /* RS */
	15, /* CS */
	16, /* IHIST */
	13, /* BHIST (SKIN_BHIST) */
};

#define VFE46_NUM_STATS_TYPE 9
#define VFE46_STATS_BASE(idx) (stats_base_addr[idx])
#define VFE46_STATS_PING_PONG_BASE(idx, ping_pong) \
	(VFE46_STATS_BASE(idx) + 0x4 * \
	(~(ping_pong >> (stats_pingpong_offset_map[idx])) & 0x1))

#define VFE46_CLK_IDX 2

uint32_t msm_vfe46_ub_reg_offset(struct vfe_device *vfe_dev, int wm_idx)
{
	return (VFE46_WM_BASE(wm_idx) + 0x10);
}

uint32_t msm_vfe46_get_ub_size(struct vfe_device *vfe_dev)
{
	if (vfe_dev->pdev->id == ISP_VFE0)
		return MSM_ISP46_TOTAL_IMAGE_UB_VFE0;
	return MSM_ISP46_TOTAL_IMAGE_UB_VFE1;
}

static void msm_vfe46_config_irq(struct vfe_device *vfe_dev,
		uint32_t irq0_mask, uint32_t irq1_mask,
		enum msm_isp_irq_operation oper)
{
	switch (oper) {
	case MSM_ISP_IRQ_ENABLE:
		vfe_dev->irq0_mask |= irq0_mask;
		vfe_dev->irq1_mask |= irq1_mask;
		msm_camera_io_w(irq0_mask, vfe_dev->vfe_base + 0x64);
		msm_camera_io_w_mb(irq1_mask, vfe_dev->vfe_base + 0x68);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x58);
		break;
	case MSM_ISP_IRQ_DISABLE:
		vfe_dev->irq0_mask &= ~irq0_mask;
		vfe_dev->irq1_mask &= ~irq1_mask;
		break;
	case MSM_ISP_IRQ_SET:
		vfe_dev->irq0_mask = irq0_mask;
		vfe_dev->irq1_mask = irq1_mask;
		msm_camera_io_w(irq1_mask, vfe_dev->vfe_base + 0x64);
		msm_camera_io_w_mb(irq1_mask, vfe_dev->vfe_base + 0x68);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x58);
		break;
	}
	msm_camera_io_w_mb(vfe_dev->irq0_mask,
				vfe_dev->vfe_base + 0x5C);
	msm_camera_io_w_mb(vfe_dev->irq1_mask,
				vfe_dev->vfe_base + 0x60);
}

static int32_t msm_vfe46_init_dt_parms(struct vfe_device *vfe_dev,
	struct msm_vfe_hw_init_parms *dt_parms, void __iomem *dev_mem_base)
{
	struct device_node *of_node;
	int32_t i = 0 , rc = 0;
	uint32_t *dt_settings = NULL, *dt_regs = NULL, num_dt_entries = 0;

	of_node = vfe_dev->pdev->dev.of_node;

	rc = of_property_read_u32(of_node, dt_parms->entries,
		&num_dt_entries);
	if (rc < 0 || !num_dt_entries) {
		pr_err("%s: NO QOS entries found\n", __func__);
		return -EINVAL;
	} else {
		dt_settings = kzalloc(sizeof(uint32_t) * num_dt_entries,
			GFP_KERNEL);
		if (!dt_settings) {
			pr_err("%s:%d No memory\n", __func__, __LINE__);
			return -ENOMEM;
		}
		dt_regs = kzalloc(sizeof(uint32_t) * num_dt_entries,
			GFP_KERNEL);
		if (!dt_regs) {
			pr_err("%s:%d No memory\n", __func__, __LINE__);
			kfree(dt_settings);
			return -ENOMEM;
		}
		rc = of_property_read_u32_array(of_node, dt_parms->regs,
			dt_regs, num_dt_entries);
		if (rc < 0) {
			pr_err("%s: NO QOS BUS BDG info\n", __func__);
			kfree(dt_settings);
			kfree(dt_regs);
			return -EINVAL;
		} else {
			if (dt_parms->settings) {
				rc = of_property_read_u32_array(of_node,
					dt_parms->settings,
					dt_settings, num_dt_entries);
				if (rc < 0) {
					pr_err("%s: NO QOS settings\n",
						__func__);
					kfree(dt_settings);
					kfree(dt_regs);
				} else {
					for (i = 0; i < num_dt_entries; i++) {
						msm_camera_io_w(dt_settings[i],
							dev_mem_base +
								dt_regs[i]);
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

static void msm_vfe46_init_hardware_reg(struct vfe_device *vfe_dev)
{
	struct msm_vfe_hw_init_parms qos_parms;
	struct msm_vfe_hw_init_parms vbif_parms;
	struct msm_vfe_hw_init_parms ds_parms;

	memset(&qos_parms, 0, sizeof(struct msm_vfe_hw_init_parms));
	memset(&vbif_parms, 0, sizeof(struct msm_vfe_hw_init_parms));
	memset(&ds_parms, 0, sizeof(struct msm_vfe_hw_init_parms));

	qos_parms.entries = "qos-entries";
	qos_parms.regs = "qos-regs";
	qos_parms.settings = "qos-settings";
	vbif_parms.entries = "vbif-entries";
	vbif_parms.regs = "vbif-regs";
	vbif_parms.settings = "vbif-settings";
	ds_parms.entries = "ds-entries";
	ds_parms.regs = "ds-regs";
	ds_parms.settings = "ds-settings";

	msm_vfe46_init_dt_parms(vfe_dev, &qos_parms, vfe_dev->vfe_base);
	msm_vfe46_init_dt_parms(vfe_dev, &ds_parms, vfe_dev->vfe_base);
	msm_vfe46_init_dt_parms(vfe_dev, &vbif_parms, vfe_dev->vfe_vbif_base);

	/* BUS_CFG */
	msm_camera_io_w(0x00000001, vfe_dev->vfe_base + 0x84);
	/* IRQ_MASK/CLEAR */
	msm_vfe46_config_irq(vfe_dev, 0x810000E0, 0xFFFFFF7E,
			MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe46_clear_status_reg(struct vfe_device *vfe_dev)
{
	msm_vfe46_config_irq(vfe_dev, 0x80000000, 0, MSM_ISP_IRQ_SET);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x64);
	msm_camera_io_w_mb(0xFFFFFFFF, vfe_dev->vfe_base + 0x68);
	msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x58);
}

static void msm_vfe46_process_reset_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status0 & (1 << 31)) {
		complete(&vfe_dev->reset_complete);
		vfe_dev->reset_pending = 0;
	}
}

static void msm_vfe46_process_halt_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status1 & (1 << 8)) {
		complete(&vfe_dev->halt_complete);
		msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x374);
	}
}

static void msm_vfe46_process_input_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	if (!(irq_status0 & 0x1000003))
		return;

	if (irq_status0 & (1 << 24)) {
		ISP_DBG("%s: Fetch Engine Read IRQ\n", __func__);
		msm_isp_fetch_engine_done_notify(vfe_dev,
			&vfe_dev->fetch_engine_info);
	}

	if (irq_status0 & (1 << 0)) {
		ISP_DBG("%s: SOF IRQ\n", __func__);
		msm_isp_increment_frame_id(vfe_dev, VFE_PIX_0, ts);
	}

	if (irq_status0 & (1 << 1))
		ISP_DBG("%s: EOF IRQ\n", __func__);
}

static void msm_vfe46_process_violation_status(
	struct vfe_device *vfe_dev)
{
	uint32_t violation_status = vfe_dev->error_info.violation_status;

	if (violation_status > 39) {
		pr_err("%s: invalid violation status %d\n",
			__func__, violation_status);
		return;
	}

	pr_err("%s: VFE pipeline violation status %d\n", __func__,
		violation_status);
}

static void msm_vfe46_process_error_status(struct vfe_device *vfe_dev)
{
	uint32_t error_status1 = vfe_dev->error_info.error_mask1;

	if (error_status1 & (1 << 0)) {
		pr_err("%s: camif error status: 0x%x\n",
			__func__, vfe_dev->error_info.camif_status);
		msm_camera_io_dump(vfe_dev->vfe_base + 0x3A8, 0x30, 1);
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
		msm_vfe46_process_violation_status(vfe_dev);
	}
	if (error_status1 & (1 << 9))
		pr_err("%s: image master 0 bus overflow\n", __func__);
	if (error_status1 & (1 << 10))
		pr_err("%s: image master 1 bus overflow\n", __func__);
	if (error_status1 & (1 << 11))
		pr_err("%s: image master 2 bus overflow\n", __func__);
	if (error_status1 & (1 << 12))
		pr_err("%s: image master 3 bus overflow\n", __func__);
	if (error_status1 & (1 << 13))
		pr_err("%s: image master 4 bus overflow\n", __func__);
	if (error_status1 & (1 << 14))
		pr_err("%s: image master 5 bus overflow\n", __func__);
	if (error_status1 & (1 << 15))
		pr_err("%s: image master 6 bus overflow\n", __func__);
	if (error_status1 & (1 << 16))
		pr_err("%s: status hdr be bus overflow\n", __func__);
	if (error_status1 & (1 << 17))
		pr_err("%s: status bg bus overflow\n", __func__);
	if (error_status1 & (1 << 18))
		pr_err("%s: status bf bus overflow\n", __func__);
	if (error_status1 & (1 << 19))
		pr_err("%s: status hdr bhist bus overflow\n", __func__);
	if (error_status1 & (1 << 20))
		pr_err("%s: status rs bus overflow\n", __func__);
	if (error_status1 & (1 << 21))
		pr_err("%s: status cs bus overflow\n", __func__);
	if (error_status1 & (1 << 22))
		pr_err("%s: status ihist bus overflow\n", __func__);
	if (error_status1 & (1 << 23))
		pr_err("%s: status skin bhist bus overflow\n", __func__);
	if (error_status1 & (1 << 24))
		pr_err("%s: status bf scale bus overflow\n", __func__);
}

static void msm_vfe46_read_and_clear_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x6C);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x70);
	msm_camera_io_w(*irq_status0, vfe_dev->vfe_base + 0x64);
	msm_camera_io_w(*irq_status1, vfe_dev->vfe_base + 0x68);
	msm_camera_io_w_mb(1, vfe_dev->vfe_base + 0x58);

	*irq_status0 &= vfe_dev->irq0_mask;
	*irq_status1 &= vfe_dev->irq1_mask;

	if (*irq_status1 & (1 << 0)) {
		vfe_dev->error_info.camif_status =
		msm_camera_io_r(vfe_dev->vfe_base + 0x3D0);
		msm_vfe46_config_irq(vfe_dev, 0, (1 << 0), MSM_ISP_IRQ_DISABLE);
	}

	if (*irq_status1 & (1 << 7))
		vfe_dev->error_info.violation_status =
		msm_camera_io_r(vfe_dev->vfe_base + 0x7C);

}

static void msm_vfe46_read_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x6C);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x70);
}

static void msm_vfe46_process_reg_update(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	enum msm_vfe_input_src i;
	uint32_t shift_irq;
	uint8_t reg_updated = 0;
	unsigned long flags;

	if (!(irq_status0 & 0xF0))
		return;
	/* Shift status bits so that PIX REG UPDATE is 1st bit */
	shift_irq = ((irq_status0 & 0xF0) >> 4);

	for (i = VFE_PIX_0; i <= VFE_RAW_2; i++) {
		if (shift_irq & BIT(i)) {
			reg_updated |= BIT(i);
			ISP_DBG("%s Reg Update IRQ %x\n", __func__,
				(uint32_t)BIT(i));

			switch (i) {
			case VFE_PIX_0:
				msm_isp_notify(vfe_dev, ISP_EVENT_REG_UPDATE,
					VFE_PIX_0, ts);
				msm_isp_process_reg_upd_epoch_irq(vfe_dev, i,
						MSM_ISP_COMP_IRQ_REG_UPD, ts);
				msm_isp_process_stats_reg_upd_epoch_irq(vfe_dev,
					MSM_ISP_COMP_IRQ_REG_UPD);
				msm_isp_stats_stream_update(vfe_dev);
				if (vfe_dev->axi_data.src_info[i].stream_count
									== 0 &&
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
				/* reg upd is also epoch for rdi */
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

static void msm_vfe46_process_epoch_irq(struct vfe_device *vfe_dev,
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
			msm_isp_process_reg_upd_epoch_irq(vfe_dev, VFE_PIX_0,
				MSM_ISP_COMP_IRQ_REG_UPD, ts);
			vfe_dev->hw_info->vfe_ops.core_ops.reg_update(
				vfe_dev, VFE_PIX_0);
		}
	}
}

static void msm_vfe46_reg_update(struct vfe_device *vfe_dev,
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
	 * If frame_src == VFE_SRC_MAX request reg_update on all
	 *  supported INTF
	 */
	if (frame_src == VFE_SRC_MAX)
		update_mask = 0xF;
	else
		update_mask = BIT((uint32_t)frame_src);
	ISP_DBG("%s update_mask %x\n", __func__, update_mask);

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
			+ 0x3D8);
		msm_camera_io_w_mb(update_mask,
			vfe_dev->vfe_base + 0x3D8);
	} else if (!vfe_dev->is_split ||
		((frame_src == VFE_PIX_0) &&
		(vfe_dev->axi_data.src_info[VFE_PIX_0].stream_count == 0) &&
		(vfe_dev->axi_data.src_info[VFE_PIX_0].
				raw_stream_count == 0)) ||
		(frame_src >= VFE_RAW_0 && frame_src <= VFE_SRC_MAX)) {
		msm_camera_io_w_mb(update_mask,
			vfe_dev->vfe_base + 0x3D8);
	}
	spin_unlock_irqrestore(&vfe_dev->reg_update_lock, flags);
}

static long msm_vfe46_reset_hardware(struct vfe_device *vfe_dev,
	uint32_t first_start, uint32_t blocking_call)
{
	long rc = 0;
	init_completion(&vfe_dev->reset_complete);

	if (blocking_call)
		vfe_dev->reset_pending = 1;

	if (first_start) {
		msm_camera_io_w_mb(0x1FF, vfe_dev->vfe_base + 0x18);
	} else {
		msm_camera_io_w_mb(0x1EF, vfe_dev->vfe_base + 0x18);
		msm_camera_io_w(0x7FFFFFFF, vfe_dev->vfe_base + 0x64);
		msm_camera_io_w(0xFFFFFEFF, vfe_dev->vfe_base + 0x68);
		msm_camera_io_w(0x1, vfe_dev->vfe_base + 0x58);
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

static void msm_vfe46_axi_reload_wm(struct vfe_device *vfe_dev,
	void __iomem *vfe_base, uint32_t reload_mask)
{
	msm_camera_io_w_mb(reload_mask, vfe_base + 0x80);
}

static void msm_vfe46_axi_update_cgc_override(struct vfe_device *vfe_dev,
	uint8_t wm_idx, uint8_t enable)
{
	uint32_t val;

	/* Change CGC override */
	val = msm_camera_io_r(vfe_dev->vfe_base + 0x3C);
	if (enable)
		val |= (1 << wm_idx);
	else
		val &= ~(1 << wm_idx);
	msm_camera_io_w_mb(val, vfe_dev->vfe_base + 0x3C);
}

static void msm_vfe46_axi_enable_wm(void __iomem *vfe_base,
	uint8_t wm_idx, uint8_t enable)
{
	uint32_t val;

	val = msm_camera_io_r(vfe_base + VFE46_WM_BASE(wm_idx));
	if (enable)
		val |= 0x1;
	else
		val &= ~0x1;
	msm_camera_io_w_mb(val,
		vfe_base + VFE46_WM_BASE(wm_idx));
}

static void msm_vfe46_axi_cfg_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t comp_mask, comp_mask_index;

	comp_mask_index = stream_info->comp_mask_index[vfe_idx];

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x74);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	comp_mask |= (axi_data->composite_info[comp_mask_index].
		stream_composite_mask << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x74);

	msm_vfe46_config_irq(vfe_dev, 1 << (comp_mask_index + 25), 0,
				MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe46_axi_clear_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t comp_mask, comp_mask_index;

	comp_mask_index = stream_info->comp_mask_index[vfe_idx];

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x74);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x74);

	msm_vfe46_config_irq(vfe_dev, 1 << (comp_mask_index + 25), 0,
				MSM_ISP_IRQ_DISABLE);
}

static void msm_vfe46_axi_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	msm_vfe46_config_irq(vfe_dev, 1 << (stream_info->wm[vfe_idx][0] + 8), 0,
				MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe46_axi_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	msm_vfe46_config_irq(vfe_dev, (1 << (stream_info->wm[vfe_idx][0] + 8)),
				0, MSM_ISP_IRQ_DISABLE);
}

static void msm_vfe46_cfg_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t framedrop_pattern,
	uint32_t framedrop_period)
{
	uint32_t i, temp;
	void __iomem *vfe_base = vfe_dev->vfe_base;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++) {
		msm_camera_io_w(framedrop_pattern, vfe_base +
			VFE46_WM_BASE(stream_info->wm[vfe_idx][i]) + 0x1C);
		temp = msm_camera_io_r(vfe_base +
			VFE46_WM_BASE(stream_info->wm[vfe_idx][i]) + 0xC);
		temp &= 0xFFFFFF83;
		msm_camera_io_w(temp | (framedrop_period - 1) << 2,
		vfe_base + VFE46_WM_BASE(stream_info->wm[vfe_idx][i]) + 0xC);
	}
}

static void msm_vfe46_clear_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t i;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++)
		msm_camera_io_w(0, vfe_dev->vfe_base +
			VFE46_WM_BASE(stream_info->wm[vfe_idx][i]) + 0x1C);
}

static int32_t msm_vfe46_convert_bpp_to_reg(int32_t bpp, uint32_t *bpp_reg)
{
	int rc = 0;
	switch (bpp) {
	case 8:
		*bpp_reg = 0;
		break;
	case 10:
		*bpp_reg = 0x1;
		break;
	case 12:
		*bpp_reg = 0x2;
		break;
	case 14:
		*bpp_reg = 0x3;
		break;
	default:
		pr_err("%s:%d invalid bpp %d\n", __func__, __LINE__, bpp);
		return -EINVAL;
	}

	return rc;
}

static int32_t msm_vfe46_convert_io_fmt_to_reg(
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
static int32_t msm_vfe46_cfg_io_format(struct vfe_device *vfe_dev,
	enum msm_vfe_axi_stream_src stream_src, uint32_t io_format)
{
	int rc = 0;
	int bpp = 0, read_bpp = 0;
	enum msm_isp_pack_fmt pack_fmt = 0, read_pack_fmt = 0;
	uint32_t bpp_reg = 0, pack_reg = 0;
	uint32_t read_bpp_reg = 0, read_pack_reg = 0;
	uint32_t io_format_reg = 0; /*io format register bit*/

	io_format_reg = msm_camera_io_r(vfe_dev->vfe_base + 0x88);

	/*input config*/
	if ((stream_src < RDI_INTF_0) &&
		(vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux ==
		EXTERNAL_READ)) {
		read_bpp = msm_isp_get_bit_per_pixel(
			vfe_dev->axi_data.src_info[VFE_PIX_0].input_format);
		rc = msm_vfe46_convert_bpp_to_reg(read_bpp, &read_bpp_reg);
		if (rc < 0) {
			pr_err("%s: convert_bpp_to_reg err! in_bpp %d rc %d\n",
				__func__, read_bpp, rc);
			return rc;
	}

		read_pack_fmt = msm_isp_get_pack_format(
			vfe_dev->axi_data.src_info[VFE_PIX_0].input_format);
		rc = msm_vfe46_convert_io_fmt_to_reg(
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
	rc = msm_vfe46_convert_bpp_to_reg(bpp, &bpp_reg);
	if (rc < 0) {
		pr_err("%s: convert_bpp_to_reg err! bpp %d rc = %d\n",
			__func__, bpp, rc);
		return rc;
	}

	switch (stream_src) {
	case PIX_VIDEO:
	case PIX_ENCODER:
	case PIX_VIEWFINDER:
	case CAMIF_RAW:
		io_format_reg &= 0xFFFFCFFF;
		io_format_reg |= bpp_reg << 12;
		break;
	case IDEAL_RAW:
		/*use output format(v4l2_pix_fmt) to get pack format*/
		pack_fmt = msm_isp_get_pack_format(io_format);
		rc = msm_vfe46_convert_io_fmt_to_reg(pack_fmt, &pack_reg);
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
	msm_camera_io_w(io_format_reg, vfe_dev->vfe_base + 0x88);
	return 0;
}

static int msm_vfe46_start_fetch_engine(struct vfe_device *vfe_dev,
	void *arg)
{
	int rc = 0;
	uint32_t bufq_handle = 0;
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

		mutex_lock(&vfe_dev->buf_mgr->lock);
		rc = vfe_dev->buf_mgr->ops->get_buf_by_index(
			vfe_dev->buf_mgr, bufq_handle, fe_cfg->buf_idx, &buf);
		if (rc < 0 || !buf) {
			pr_err("%s: No fetch buffer rc= %d buf= %pK\n",
				__func__, rc, buf);
			mutex_unlock(&vfe_dev->buf_mgr->lock);
			return -EINVAL;
		}
		mapped_info = buf->mapped_info[0];
		buf->state = MSM_ISP_BUFFER_STATE_DISPATCHED;
		mutex_unlock(&vfe_dev->buf_mgr->lock);
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

	msm_camera_io_w(mapped_info.paddr, vfe_dev->vfe_base + 0x268);

	msm_camera_io_w_mb(0x100000, vfe_dev->vfe_base + 0x80);
	msm_camera_io_w_mb(0x200000, vfe_dev->vfe_base + 0x80);

	ISP_DBG("%s:VFE%d Fetch Engine ready\n", __func__, vfe_dev->pdev->id);
	return 0;
}

static void msm_vfe46_cfg_fetch_engine(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint32_t x_size_word, temp;
	struct msm_vfe_fetch_engine_cfg *fe_cfg = NULL;

	if (pix_cfg->input_mux == EXTERNAL_READ) {
		fe_cfg = &pix_cfg->fetch_engine_cfg;
		pr_debug("%s:VFE%d wd x ht buf = %d x %d, fe = %d x %d\n",
			__func__, vfe_dev->pdev->id, fe_cfg->buf_width,
			fe_cfg->buf_height,
			fe_cfg->fetch_width, fe_cfg->fetch_height);

		temp = msm_camera_io_r(vfe_dev->vfe_base + 0x84);
		temp &= 0xFFFFFFFD;
		temp |= (1 << 1);
		msm_camera_io_w(temp, vfe_dev->vfe_base + 0x84);

		msm_vfe46_config_irq(vfe_dev, 1 << 24, 0,
					MSM_ISP_IRQ_ENABLE);

		temp = fe_cfg->fetch_height - 1;
		msm_camera_io_w(temp & 0x3FFF, vfe_dev->vfe_base + 0x278);

		x_size_word = msm_isp_cal_word_per_line(
			vfe_dev->axi_data.src_info[VFE_PIX_0].input_format,
			fe_cfg->fetch_width);
		msm_camera_io_w((x_size_word - 1) << 16,
			vfe_dev->vfe_base + 0x27C);

		msm_camera_io_w(x_size_word << 16 |
			(temp & 0x3FFF) << 2 | VFE46_FETCH_BURST_LEN,
			vfe_dev->vfe_base + 0x280);

		temp = ((fe_cfg->buf_width - 1) & 0x3FFF) << 16 |
			((fe_cfg->buf_height - 1) & 0x3FFF);
		msm_camera_io_w(temp, vfe_dev->vfe_base + 0x284);

		/* need to use formulae to calculate MAIN_UNPACK_PATTERN*/
		msm_camera_io_w(0xF6543210, vfe_dev->vfe_base + 0x288);
		msm_camera_io_w(0xF, vfe_dev->vfe_base + 0x2A4);

		vfe_dev->hw_info->vfe_ops.axi_ops.update_cgc_override(vfe_dev,
			VFE46_BUS_RD_CGC_OVERRIDE_BIT, 1);

		temp = msm_camera_io_r(vfe_dev->vfe_base + 0x50);
		temp |= 2 << 5;
		temp |= 128 << 8;
		temp |= pix_cfg->pixel_pattern;
		msm_camera_io_w(temp, vfe_dev->vfe_base + 0x50);

	} else {
		pr_err("%s: Invalid mux configuration - mux: %d", __func__,
			pix_cfg->input_mux);
	}
}

static void msm_vfe46_cfg_testgen(struct vfe_device *vfe_dev,
	struct msm_vfe_testgen_cfg *testgen_cfg)
{
	uint32_t temp;
	uint32_t bit_per_pixel = 0;
	uint32_t bpp_reg = 0;
	uint32_t bayer_pix_pattern_reg = 0;
	uint32_t unicolorbar_reg = 0;
	uint32_t unicolor_enb = 0;

	bit_per_pixel = msm_isp_get_bit_per_pixel(
		vfe_dev->axi_data.src_info[VFE_PIX_0].input_format);

	switch (bit_per_pixel) {
	case 8:
		bpp_reg = 0x0;
		break;
	case 10:
		bpp_reg = 0x1;
		break;
	case 12:
		bpp_reg = 0x10;
		break;
	case 14:
		bpp_reg = 0x11;
		break;
	default:
		pr_err("%s: invalid bpp %d\n", __func__, bit_per_pixel);
		break;
	}

	msm_camera_io_w(bpp_reg << 16 | testgen_cfg->burst_num_frame,
		vfe_dev->vfe_base + 0xAF8);

	msm_camera_io_w(((testgen_cfg->lines_per_frame - 1) << 16) |
		(testgen_cfg->pixels_per_line - 1), vfe_dev->vfe_base + 0xAFC);

	temp = msm_camera_io_r(vfe_dev->vfe_base + 0x50);
	temp |= (((testgen_cfg->h_blank) & 0x3FFF) << 8);
	temp |= (1 << 24);
	msm_camera_io_w(temp, vfe_dev->vfe_base + 0x50);

	msm_camera_io_w((1 << 16) | testgen_cfg->v_blank,
		vfe_dev->vfe_base + 0xB0C);

	switch (testgen_cfg->pixel_bayer_pattern) {
	case ISP_BAYER_RGRGRG:
		bayer_pix_pattern_reg = 0x0;
		break;
	case ISP_BAYER_GRGRGR:
		bayer_pix_pattern_reg = 0x1;
		break;
	case ISP_BAYER_BGBGBG:
		bayer_pix_pattern_reg = 0x10;
		break;
	case ISP_BAYER_GBGBGB:
		bayer_pix_pattern_reg = 0x11;
		break;
	default:
		pr_err("%s: invalid pix pattern %d\n",
			__func__, bit_per_pixel);
		break;
	}

	if (testgen_cfg->color_bar_pattern == COLOR_BAR_8_COLOR) {
		unicolor_enb = 0x0;
	} else {
		unicolor_enb = 0x1;
		switch (testgen_cfg->color_bar_pattern) {
		case UNICOLOR_WHITE:
			unicolorbar_reg = 0x0;
			break;
		case UNICOLOR_YELLOW:
			unicolorbar_reg = 0x1;
			break;
		case UNICOLOR_CYAN:
			unicolorbar_reg = 0x10;
			break;
		case UNICOLOR_GREEN:
			unicolorbar_reg = 0x11;
			break;
		case UNICOLOR_MAGENTA:
			unicolorbar_reg = 0x100;
			break;
		case UNICOLOR_RED:
			unicolorbar_reg = 0x101;
			break;
		case UNICOLOR_BLUE:
			unicolorbar_reg = 0x110;
			break;
		case UNICOLOR_BLACK:
			unicolorbar_reg = 0x111;
			break;
		default:
			pr_err("%s: invalid colorbar %d\n",
				__func__, testgen_cfg->color_bar_pattern);
			break;
		}
	}

	msm_camera_io_w((testgen_cfg->rotate_period << 8) |
		(bayer_pix_pattern_reg << 6) | (unicolor_enb << 4) |
		(unicolorbar_reg), vfe_dev->vfe_base + 0xB14);
	return;
}

static void msm_vfe46_cfg_camif(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint16_t first_pixel, last_pixel, first_line, last_line;
	struct msm_vfe_camif_cfg *camif_cfg = &pix_cfg->camif_cfg;
	uint32_t val, subsample_period, subsample_pattern;
	struct msm_vfe_camif_subsample_cfg *subsample_cfg =
		&pix_cfg->camif_cfg.subsample_cfg;
	uint16_t bus_sub_en = 0;

	if (subsample_cfg->pixel_skip || subsample_cfg->line_skip)
		bus_sub_en = 1;
	else
		bus_sub_en = 0;

	vfe_dev->dual_vfe_enable = camif_cfg->is_split;

	msm_camera_io_w(pix_cfg->input_mux << 5 | pix_cfg->pixel_pattern,
		vfe_dev->vfe_base + 0x50);

	first_pixel = camif_cfg->first_pixel;
	last_pixel = camif_cfg->last_pixel;
	first_line = camif_cfg->first_line;
	last_line = camif_cfg->last_line;
	subsample_period = camif_cfg->subsample_cfg.irq_subsample_period;
	subsample_pattern = camif_cfg->subsample_cfg.irq_subsample_pattern;

	if (bus_sub_en) {
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x3AC);
		val &= 0xFFFFFFDF;
		val = val | bus_sub_en << 5;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x3AC);
		subsample_cfg->pixel_skip &= 0x0000FFFF;
		subsample_cfg->line_skip  &= 0x0000FFFF;
		msm_camera_io_w((subsample_cfg->line_skip << 16) |
			subsample_cfg->pixel_skip, vfe_dev->vfe_base + 0x3C0);
	}

	msm_camera_io_w(camif_cfg->lines_per_frame << 16 |
		camif_cfg->pixels_per_line, vfe_dev->vfe_base + 0x3B4);

	msm_camera_io_w(first_pixel << 16 | last_pixel,
	vfe_dev->vfe_base + 0x3B8);

	msm_camera_io_w(first_line << 16 | last_line,
	vfe_dev->vfe_base + 0x3BC);

	if (subsample_period && subsample_pattern) {
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x3AC);
		val &= 0xFFE0FFFF;
		val = (subsample_period - 1) << 16;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x3AC);
		ISP_DBG("%s:camif PERIOD %x PATTERN %x\n",
			__func__,  subsample_period, subsample_pattern);

		val = subsample_pattern;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x3C8);
	} else {
		msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x3C8);
	}

	val = msm_camera_io_r(vfe_dev->vfe_base + 0x39C);
	val |= camif_cfg->camif_input;
	msm_camera_io_w(val, vfe_dev->vfe_base + 0x39C);
}

static void msm_vfe46_cfg_input_mux(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint32_t core_cfg = 0;
	uint32_t val = 0;

	core_cfg =  msm_camera_io_r(vfe_dev->vfe_base + 0x50);
	core_cfg &= 0xFFFFFF9F;

	switch (pix_cfg->input_mux) {
	case CAMIF:
		core_cfg |= 0x0 << 5;
		msm_camera_io_w_mb(core_cfg, vfe_dev->vfe_base + 0x50);
		msm_vfe46_cfg_camif(vfe_dev, pix_cfg);
		break;
	case TESTGEN:
		/* Change CGC override */
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x3C);
		val |= (1 << 31);
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x3C);

		/* CAMIF and TESTGEN will both go thorugh CAMIF*/
		core_cfg |= 0x1 << 5;
		msm_camera_io_w_mb(core_cfg, vfe_dev->vfe_base + 0x50);
		msm_vfe46_cfg_camif(vfe_dev, pix_cfg);
		msm_vfe46_cfg_testgen(vfe_dev, &pix_cfg->testgen_cfg);
		break;
	case EXTERNAL_READ:
		core_cfg |= 0x2 << 5;
		msm_camera_io_w_mb(core_cfg, vfe_dev->vfe_base + 0x50);
		msm_vfe46_cfg_fetch_engine(vfe_dev, pix_cfg);
		break;
	default:
		pr_err("%s: Unsupported input mux %d\n",
			__func__, pix_cfg->input_mux);
		break;
	}
	return;
}

static void msm_vfe46_update_camif_state(struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state update_state)
{
	uint32_t val;
	bool bus_en, vfe_en;

	if (update_state == NO_UPDATE)
		return;

	if (update_state == ENABLE_CAMIF) {
		msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x64);
		msm_camera_io_w(0x81, vfe_dev->vfe_base + 0x68);
		msm_camera_io_w(0x1, vfe_dev->vfe_base + 0x58);
		msm_vfe46_config_irq(vfe_dev, 0x15, 0x81,
					MSM_ISP_IRQ_ENABLE);

		bus_en =
			((vfe_dev->axi_data.
			src_info[VFE_PIX_0].raw_stream_count > 0) ? 1 : 0);
		vfe_en =
			((vfe_dev->axi_data.
			src_info[VFE_PIX_0].stream_count > 0) ? 1 : 0);
		val = msm_camera_io_r(vfe_dev->vfe_base + 0x3AC);
		val &= 0xFFFFFF3F;
		val = val | bus_en << 7 | vfe_en << 6;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x3AC);
		msm_camera_io_w_mb(0x4, vfe_dev->vfe_base + 0x3A8);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x3A8);
		/* configure EPOCH0 for 20 lines */
		msm_camera_io_w_mb(0x140000, vfe_dev->vfe_base + 0x3CC);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 1;
		/* testgen GO*/
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux == TESTGEN)
			msm_camera_io_w(1, vfe_dev->vfe_base + 0xAF4);
	} else if (update_state == DISABLE_CAMIF ||
		DISABLE_CAMIF_IMMEDIATELY == update_state) {
		uint32_t poll_val;

		if (vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux == TESTGEN)
			update_state = DISABLE_CAMIF;
		msm_vfe46_config_irq(vfe_dev, 0, 0x81,
				MSM_ISP_IRQ_DISABLE);
		/* disable danger signal */
		val = msm_camera_io_r(vfe_dev->vfe_base + 0xC18);
		val &= ~(1 << 8);
		msm_camera_io_w_mb(val, vfe_dev->vfe_base + 0xC18);
		msm_camera_io_w_mb((update_state == DISABLE_CAMIF ? 0x0 : 0x6),
				vfe_dev->vfe_base + 0x3A8);
		if (readl_poll_timeout_atomic(vfe_dev->vfe_base + 0x3D0,
			poll_val, poll_val & 0x80000000, 1000, 2000000))
			pr_err("%s: camif disable failed %x\n",
				__func__, poll_val);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 0;
		/* testgen OFF*/
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].input_mux == TESTGEN)
			msm_camera_io_w(1 << 1, vfe_dev->vfe_base + 0xAF4);
	}
}

static void msm_vfe46_cfg_rdi_reg(
	struct vfe_device *vfe_dev, struct msm_vfe_rdi_cfg *rdi_cfg,
	enum msm_vfe_input_src input_src)
{
	uint8_t rdi = input_src - VFE_RAW_0;
	uint32_t rdi_reg_cfg;

	rdi_reg_cfg = msm_camera_io_r(
		vfe_dev->vfe_base + VFE46_RDI_BASE(rdi));
	rdi_reg_cfg &= 0x03;
	rdi_reg_cfg |= (rdi * 3) << 28 | rdi_cfg->cid << 4 | 0x4;
	msm_camera_io_w(
		rdi_reg_cfg, vfe_dev->vfe_base + VFE46_RDI_BASE(rdi));
}

static void msm_vfe46_axi_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx)
{
	uint32_t val;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t wm_base;

	wm_base = VFE46_WM_BASE(stream_info->wm[vfe_idx][plane_idx]);

	val = msm_camera_io_r(vfe_dev->vfe_base + wm_base + 0xC);
	val &= ~0x2;
	if (stream_info->frame_based)
		val |= 0x2;
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0xC);
	if (!stream_info->frame_based) {
		/* WR_IMAGE_SIZE */
		val =
			((msm_isp_cal_word_per_line(
				stream_info->output_format,
				stream_info->plane_cfg[vfe_idx][plane_idx].
				output_width)+3)/4 - 1) << 16 |
				(stream_info->plane_cfg[vfe_idx][plane_idx].
				output_height - 1);
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
		/* WR_BUFFER_CFG */
		val = VFE46_BURST_LEN |
			(stream_info->plane_cfg[vfe_idx][plane_idx].
						output_height - 1) <<
			2 |
			((msm_isp_cal_word_per_line(stream_info->output_format,
			stream_info->plane_cfg[vfe_idx][plane_idx].
			output_stride)+1)/2) << 16;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x18);
	}
	/* WR_IRQ_SUBSAMPLE_PATTERN */
	msm_camera_io_w(0xFFFFFFFF,
		vfe_dev->vfe_base + wm_base + 0x20);
	/* TD: Add IRQ subsample pattern */
}

static void msm_vfe46_axi_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint32_t val = 0;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint32_t wm_base;

	wm_base = VFE46_WM_BASE(stream_info->wm[vfe_idx][plane_idx]);

	/* WR_ADDR_CFG */
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0xC);
	/* WR_IMAGE_SIZE */
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	/* WR_BUFFER_CFG */
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x18);
	/* WR_IRQ_SUBSAMPLE_PATTERN */
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x20);
}

static void msm_vfe46_axi_cfg_wm_xbar_reg(
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
	case PIX_VIDEO:
	case PIX_ENCODER:
	case PIX_VIEWFINDER: {
		if (plane_cfg->output_plane_format != CRCB_PLANE &&
			plane_cfg->output_plane_format != CBCR_PLANE) {
			/* SINGLE_STREAM_SEL */
			xbar_cfg |= plane_cfg->output_plane_format << 8;
		} else {
			switch (stream_info->output_format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV14:
			case V4L2_PIX_FMT_NV16:
			case V4L2_PIX_FMT_NV24:
				/* PAIR_STREAM_SWAP_CTRL */
				xbar_cfg |= 0x3 << 4;
				break;
			}
			xbar_cfg |= 0x1 << 2; /* PAIR_STREAM_EN */
		}
		if (stream_info->stream_src == PIX_VIEWFINDER)
			xbar_cfg |= 0x1; /* VIEW_STREAM_EN */
		else if (stream_info->stream_src == PIX_VIDEO)
			xbar_cfg |= 0x2;
		break;
	}
	case CAMIF_RAW:
		xbar_cfg = 0x300;
		break;
	case IDEAL_RAW:
		xbar_cfg = 0x400;
		break;
	case RDI_INTF_0:
		xbar_cfg = 0xC00;
		break;
	case RDI_INTF_1:
		xbar_cfg = 0xD00;
		break;
	case RDI_INTF_2:
		xbar_cfg = 0xE00;
		break;
	default:
		pr_err("%s: Invalid stream src\n", __func__);
		break;
	}
	xbar_reg_cfg =
		msm_camera_io_r(vfe_dev->vfe_base + VFE46_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFFFF << VFE46_XBAR_SHIFT(wm));
	xbar_reg_cfg |= (xbar_cfg << VFE46_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg,
		vfe_dev->vfe_base + VFE46_XBAR_BASE(wm));
}

static void msm_vfe46_axi_clear_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	uint8_t wm;
	uint32_t xbar_reg_cfg = 0;

	wm = stream_info->wm[vfe_idx][plane_idx];

	xbar_reg_cfg =
		msm_camera_io_r(vfe_dev->vfe_base + VFE46_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFFFF << VFE46_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg,
		vfe_dev->vfe_base + VFE46_XBAR_BASE(wm));
}

static void msm_vfe46_read_wm_ping_pong_addr(
	struct vfe_device *vfe_dev)
{
	msm_camera_io_dump(vfe_dev->vfe_base +
		(VFE46_WM_BASE(0) & 0xFFFFFFF0), 0x200, 1);
}

static void msm_vfe46_update_ping_pong_addr(
	void __iomem *vfe_base,
	uint8_t wm_idx, uint32_t pingpong_bit, dma_addr_t paddr,
	int32_t buf_size)
{
	uint32_t paddr32 = (paddr & 0xFFFFFFFF);

	msm_camera_io_w(paddr32, vfe_base +
		VFE46_PING_PONG_BASE(wm_idx, pingpong_bit));
}

static void msm_vfe46_set_halt_restart_mask(struct vfe_device *vfe_dev)
{
	msm_vfe46_config_irq(vfe_dev, BIT(31), BIT(8), MSM_ISP_IRQ_SET);
}

static int msm_vfe46_axi_halt(struct vfe_device *vfe_dev,
	uint32_t blocking)
{
	int rc = 0;
	enum msm_vfe_input_src i;
	struct msm_isp_timestamp ts;

	/* Keep only halt and restart mask */
	msm_vfe46_config_irq(vfe_dev, (1 << 31), (1 << 8),
			MSM_ISP_IRQ_SET);

	if (atomic_read(&vfe_dev->error_info.overflow_state)
		== OVERFLOW_DETECTED)
		pr_err_ratelimited("%s: VFE%d halt for recovery, blocking %d\n",
			__func__, vfe_dev->pdev->id, blocking);

	if (blocking) {
		init_completion(&vfe_dev->halt_complete);
		/* Halt AXI Bus Bridge */
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x374);
		rc = wait_for_completion_timeout(
			&vfe_dev->halt_complete, msecs_to_jiffies(500));
		if (rc <= 0)
			pr_err("%s:VFE%d halt timeout rc=%d\n", __func__,
				vfe_dev->pdev->id, rc);
	} else {
		/* Halt AXI Bus Bridge */
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x374);
	}

	msm_isp_get_timestamp(&ts, vfe_dev);
	for (i = VFE_PIX_0; i <= VFE_RAW_2; i++) {
		msm_isp_axi_stream_update(vfe_dev, i, &ts);
		msm_isp_axi_stream_update(vfe_dev, i, &ts);
	}

	msm_isp_stats_stream_update(vfe_dev);
	msm_isp_stats_stream_update(vfe_dev);

	return rc;
}

static void msm_vfe46_axi_restart(struct vfe_device *vfe_dev,
	uint32_t blocking, uint32_t enable_camif)
{
	msm_vfe46_config_irq(vfe_dev, vfe_dev->recovery_irq0_mask,
			vfe_dev->recovery_irq1_mask,
			MSM_ISP_IRQ_ENABLE);
	msm_camera_io_w_mb(0x20000, vfe_dev->vfe_base + 0x3CC);

	/* Start AXI */
	msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x374);

	vfe_dev->hw_info->vfe_ops.core_ops.reg_update(vfe_dev, VFE_SRC_MAX);
	memset(&vfe_dev->error_info, 0, sizeof(vfe_dev->error_info));
	atomic_set(&vfe_dev->error_info.overflow_state, NO_OVERFLOW);
	if (enable_camif)
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, ENABLE_CAMIF);
}

static uint32_t msm_vfe46_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 8) & 0x7F;
}

static uint32_t msm_vfe46_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 25) & 0xF;
}

static uint32_t msm_vfe46_get_pingpong_status(
	struct vfe_device *vfe_dev)
{
	return msm_camera_io_r(vfe_dev->vfe_base + 0x2A8);
}

static int msm_vfe46_get_stats_idx(enum msm_isp_stats_type stats_type)
{
	switch (stats_type) {
	case MSM_ISP_STATS_HDR_BE:
		return STATS_IDX_HDR_BE;
	case MSM_ISP_STATS_BG:
		return STATS_IDX_BG;
	case MSM_ISP_STATS_BF:
		return STATS_IDX_BF;
	case MSM_ISP_STATS_HDR_BHIST:
		return STATS_IDX_HDR_BHIST;
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

static int msm_vfe46_stats_check_streams(
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

static void msm_vfe46_stats_cfg_comp_mask(
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
	comp_mask_reg = msm_camera_io_r(vfe_dev->vfe_base + 0x78);

	if (enable) {
		comp_mask_reg |= mask_bf_scale << (16 + request_comp_index * 8);
		atomic_set(stats_comp_mask, stats_mask |
				atomic_read(stats_comp_mask));
		msm_vfe46_config_irq(vfe_dev,
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
		msm_vfe46_config_irq(vfe_dev,
			1 << (request_comp_index + 29), 0,
			MSM_ISP_IRQ_DISABLE);
	}
	msm_camera_io_w(comp_mask_reg, vfe_dev->vfe_base + 0x78);

	ISP_DBG("%s: comp_mask_reg: %x comp mask0 %x mask1: %x\n",
		__func__, comp_mask_reg,
		atomic_read(&stats_data->stats_comp_mask[0]),
		atomic_read(&stats_data->stats_comp_mask[1]));

	return;
}

static void msm_vfe46_stats_cfg_wm_irq_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);

	msm_vfe46_config_irq(vfe_dev,
		1 << (STATS_IDX(stream_info->stream_handle[vfe_idx]) + 15), 0,
		MSM_ISP_IRQ_ENABLE);
}

static void msm_vfe46_stats_clear_wm_irq_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);

	msm_vfe46_config_irq(vfe_dev,
		1 << (STATS_IDX(stream_info->stream_handle[vfe_idx]) + 15), 0,
		MSM_ISP_IRQ_DISABLE);
}

static void msm_vfe46_stats_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);
	int stats_idx;
	uint32_t stats_base;

	stats_idx = STATS_IDX(stream_info->stream_handle[vfe_idx]);
	stats_base = VFE46_STATS_BASE(stats_idx);

	/*
	 * BF_SCALE does not have its own WR_ADDR_CFG,
	 * IRQ_FRAMEDROP_PATTERN and IRQ_SUBSAMPLE_PATTERN;
	 * it's using the same from BF.
	 */
	if (stats_idx == STATS_IDX_BF_SCALE)
		return;

	/* WR_ADDR_CFG */
	msm_camera_io_w((stream_info->framedrop_period - 1) << 2,
		vfe_dev->vfe_base + stats_base + 0x8);
	/* WR_IRQ_FRAMEDROP_PATTERN */
	msm_camera_io_w(stream_info->framedrop_pattern,
		vfe_dev->vfe_base + stats_base + 0x10);
	/* WR_IRQ_SUBSAMPLE_PATTERN */
	msm_camera_io_w(0xFFFFFFFF,
		vfe_dev->vfe_base + stats_base + 0x14);
}

static void msm_vfe46_stats_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev,
				stream_info);
	uint32_t val = 0;
	int stats_idx;
	uint32_t stats_base;

	stats_idx = STATS_IDX(stream_info->stream_handle[vfe_idx]);
	stats_base = VFE46_STATS_BASE(stats_idx);
	/*
	 * BF_SCALE does not have its own WR_ADDR_CFG,
	 * IRQ_FRAMEDROP_PATTERN and IRQ_SUBSAMPLE_PATTERN;
	 * it's using the same from BF.
	 */
	if (stats_idx == STATS_IDX_BF_SCALE)
		return;

	/* WR_ADDR_CFG */
	msm_camera_io_w(val, vfe_dev->vfe_base + stats_base + 0x8);
	/* WR_IRQ_FRAMEDROP_PATTERN */
	msm_camera_io_w(val, vfe_dev->vfe_base + stats_base + 0x10);
	/* WR_IRQ_SUBSAMPLE_PATTERN */
	msm_camera_io_w(val, vfe_dev->vfe_base + stats_base + 0x14);
}

static void msm_vfe46_stats_cfg_ub(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = 0;
	uint32_t ub_size[VFE46_NUM_STATS_TYPE] = {
		16, /* MSM_ISP_STATS_BF_SCALE */
		16, /* MSM_ISP_STATS_HDR_BE */
		16, /* MSM_ISP_STATS_BG */
		16, /* MSM_ISP_STATS_BF */
		16, /* MSM_ISP_STATS_HDR_BHIST */
		16, /* MSM_ISP_STATS_RS */
		16, /* MSM_ISP_STATS_CS */
		16, /* MSM_ISP_STATS_IHIST */
		16, /* MSM_ISP_STATS_BHIST */
	};
	if (vfe_dev->pdev->id == ISP_VFE1)
		ub_offset = VFE46_UB_SIZE_VFE1;
	else if (vfe_dev->pdev->id == ISP_VFE0)
		ub_offset = VFE46_UB_SIZE_VFE0;
	else
		pr_err("%s: incorrect VFE device\n", __func__);

	for (i = 0; i < VFE46_NUM_STATS_TYPE; i++) {
		ub_offset -= ub_size[i];
		msm_camera_io_w(VFE46_STATS_BURST_LEN << 30 |
			ub_offset << 16 | (ub_size[i] - 1),
			vfe_dev->vfe_base + VFE46_STATS_BASE(i) +
			((i == STATS_IDX_BF_SCALE) ? 0x8 : 0xC));
	}
}

static void msm_vfe46_stats_update_cgc_override(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	int i;
	uint32_t module_cfg, cgc_mask = 0;

	for (i = 0; i < VFE46_NUM_STATS_TYPE; i++) {
		if ((stats_mask >> i) & 0x1) {
			switch (i) {
			case STATS_IDX_HDR_BE:
				cgc_mask |= 1;
				break;
			case STATS_IDX_HDR_BHIST:
				cgc_mask |= (1 << 1);
				break;
			case STATS_IDX_BF:
				cgc_mask |= (1 << 2);
				break;
			case STATS_IDX_BG:
				cgc_mask |= (1 << 3);
				break;
			case STATS_IDX_BHIST:
				cgc_mask |= (1 << 4);
				break;
			case STATS_IDX_RS:
				cgc_mask |= (1 << 5);
				break;
			case STATS_IDX_CS:
				cgc_mask |= (1 << 6);
				break;
			case STATS_IDX_IHIST:
				cgc_mask |= (1 << 7);
				break;
			case STATS_IDX_BF_SCALE:
				cgc_mask |= (1 << 2);
				break;
			default:
				pr_err("%s: Invalid stats mask\n", __func__);
				return;
			}
		}
	}

	/* CGC override */
	module_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x30);
	if (enable)
		module_cfg |= cgc_mask;
	else
		module_cfg &= ~cgc_mask;
	msm_camera_io_w(module_cfg, vfe_dev->vfe_base + 0x30);
}

static bool msm_vfe46_is_module_cfg_lock_needed(
	uint32_t reg_offset)
{
	return false;
}

static void msm_vfe46_stats_enable_module(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	int i;
	uint32_t module_cfg, module_cfg_mask = 0;
	uint32_t stats_cfg, stats_cfg_mask = 0;

	for (i = 0; i < VFE46_NUM_STATS_TYPE; i++) {
		if ((stats_mask >> i) & 0x1) {
			switch (i) {
			case STATS_IDX_HDR_BE:
				module_cfg_mask |= 1;
				break;
			case STATS_IDX_HDR_BHIST:
				module_cfg_mask |= 1 << 1;
				break;
			case STATS_IDX_BF:
				module_cfg_mask |= 1 << 2;
				break;
			case STATS_IDX_BG:
				module_cfg_mask |= 1 << 3;
				break;
			case STATS_IDX_BHIST:
				module_cfg_mask |= 1 << 4;
				break;
			case STATS_IDX_RS:
				module_cfg_mask |= 1 << 5;
				break;
			case STATS_IDX_CS:
				module_cfg_mask |= 1 << 6;
				break;
			case STATS_IDX_IHIST:
				module_cfg_mask |= 1 << 7;
				break;
			case STATS_IDX_BF_SCALE:
				stats_cfg_mask |= 1 << 5;
				break;
			default:
				pr_err("%s: Invalid stats mask\n", __func__);
				return;
			}
		}
	}

	module_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x44);
	if (enable)
		module_cfg |= module_cfg_mask;
	else
		module_cfg &= ~module_cfg_mask;
	msm_camera_io_w(module_cfg, vfe_dev->vfe_base + 0x44);

	stats_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x9B8);
	if (enable)
		stats_cfg |= stats_cfg_mask;
	else
		stats_cfg &= ~stats_cfg_mask;
	msm_camera_io_w(stats_cfg, vfe_dev->vfe_base + 0x9B8);
}

static void msm_vfe46_stats_update_ping_pong_addr(
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
		VFE46_STATS_PING_PONG_BASE(stats_idx, pingpong_status));
}

static uint32_t msm_vfe46_stats_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 15) & 0x1FF;
}

static uint32_t msm_vfe46_stats_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 29) & 0x3;
}

static uint32_t msm_vfe46_stats_get_frame_id(
	struct vfe_device *vfe_dev)
{
	return vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
}

static void msm_vfe46_get_error_mask(
	uint32_t *error_mask0, uint32_t *error_mask1)
{
	*error_mask0 = 0x00000000;
	*error_mask1 = 0x01FFFEFF;
}

static void msm_vfe46_get_overflow_mask(uint32_t *overflow_mask)
{
	*overflow_mask = 0x01FFFE7E;
}

static void msm_vfe46_get_rdi_wm_mask(struct vfe_device *vfe_dev,
	uint32_t *rdi_wm_mask)
{
	*rdi_wm_mask = vfe_dev->axi_data.rdi_wm_mask;
}

static void msm_vfe46_get_irq_mask(struct vfe_device *vfe_dev,
	uint32_t *irq0_mask, uint32_t *irq1_mask)
{
	*irq0_mask = vfe_dev->irq0_mask;
	*irq1_mask = vfe_dev->irq1_mask;
}

static void msm_vfe46_get_halt_restart_mask(uint32_t *irq0_mask,
	uint32_t *irq1_mask)
{
	*irq0_mask = BIT(31);
	*irq1_mask = BIT(8);
}
static struct msm_vfe_axi_hardware_info msm_vfe46_axi_hw_info = {
	.num_wm = 7,
	.num_comp_mask = 3,
	.num_rdi = 3,
	.num_rdi_master = 3,
	.min_wm_ub = 96,
	.scratch_buf_range = SZ_32M + SZ_4M,
};

static struct msm_vfe_stats_hardware_info msm_vfe46_stats_hw_info = {
	.stats_capability_mask =
		1 << MSM_ISP_STATS_HDR_BE    | 1 << MSM_ISP_STATS_BF    |
		1 << MSM_ISP_STATS_BG        | 1 << MSM_ISP_STATS_BHIST |
		1 << MSM_ISP_STATS_HDR_BHIST | 1 << MSM_ISP_STATS_IHIST |
		1 << MSM_ISP_STATS_RS        | 1 << MSM_ISP_STATS_CS    |
		1 << MSM_ISP_STATS_BF_SCALE,
	.stats_ping_pong_offset = stats_pingpong_offset_map,
	.num_stats_type = VFE46_NUM_STATS_TYPE,
	.num_stats_comp_mask = VFE46_NUM_STATS_COMP,
};

struct msm_vfe_hardware_info vfe46_hw_info = {
	.num_iommu_ctx = 1,
	.num_iommu_secure_ctx = 1,
	.vfe_clk_idx = VFE46_CLK_IDX,
	.runtime_axi_update = 0,
	.min_ab = 100000000,
	.min_ib = 100000000,
	.vfe_ops = {
		.irq_ops = {
			.read_and_clear_irq_status =
				msm_vfe46_read_and_clear_irq_status,
			.read_irq_status = msm_vfe46_read_irq_status,
			.process_camif_irq = msm_vfe46_process_input_irq,
			.process_reset_irq = msm_vfe46_process_reset_irq,
			.process_halt_irq = msm_vfe46_process_halt_irq,
			.process_reset_irq = msm_vfe46_process_reset_irq,
			.process_reg_update = msm_vfe46_process_reg_update,
			.process_axi_irq = msm_isp_process_axi_irq,
			.process_stats_irq = msm_isp_process_stats_irq,
			.process_epoch_irq = msm_vfe46_process_epoch_irq,
			.config_irq = msm_vfe46_config_irq,
			.preprocess_camif_irq = msm_isp47_preprocess_camif_irq,
		},
		.axi_ops = {
			.reload_wm = msm_vfe46_axi_reload_wm,
			.enable_wm = msm_vfe46_axi_enable_wm,
			.cfg_io_format = msm_vfe46_cfg_io_format,
			.cfg_comp_mask = msm_vfe46_axi_cfg_comp_mask,
			.clear_comp_mask = msm_vfe46_axi_clear_comp_mask,
			.cfg_wm_irq_mask = msm_vfe46_axi_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe46_axi_clear_wm_irq_mask,
			.cfg_framedrop = msm_vfe46_cfg_framedrop,
			.clear_framedrop = msm_vfe46_clear_framedrop,
			.cfg_wm_reg = msm_vfe46_axi_cfg_wm_reg,
			.clear_wm_reg = msm_vfe46_axi_clear_wm_reg,
			.cfg_wm_xbar_reg = msm_vfe46_axi_cfg_wm_xbar_reg,
			.clear_wm_xbar_reg = msm_vfe46_axi_clear_wm_xbar_reg,
			.cfg_ub = msm_vfe47_cfg_axi_ub,
			.read_wm_ping_pong_addr =
				msm_vfe46_read_wm_ping_pong_addr,
			.update_ping_pong_addr =
				msm_vfe46_update_ping_pong_addr,
			.get_comp_mask = msm_vfe46_get_comp_mask,
			.get_wm_mask = msm_vfe46_get_wm_mask,
			.get_pingpong_status = msm_vfe46_get_pingpong_status,
			.halt = msm_vfe46_axi_halt,
			.restart = msm_vfe46_axi_restart,
			.update_cgc_override =
				msm_vfe46_axi_update_cgc_override,
			.ub_reg_offset = msm_vfe46_ub_reg_offset,
			.get_ub_size = msm_vfe46_get_ub_size,
		},
		.core_ops = {
			.reg_update = msm_vfe46_reg_update,
			.cfg_input_mux = msm_vfe46_cfg_input_mux,
			.update_camif_state = msm_vfe46_update_camif_state,
			.start_fetch_eng = msm_vfe46_start_fetch_engine,
			.cfg_rdi_reg = msm_vfe46_cfg_rdi_reg,
			.reset_hw = msm_vfe46_reset_hardware,
			.init_hw = msm_vfe47_init_hardware,
			.init_hw_reg = msm_vfe46_init_hardware_reg,
			.clear_status_reg = msm_vfe46_clear_status_reg,
			.release_hw = msm_vfe47_release_hardware,
			.get_error_mask = msm_vfe46_get_error_mask,
			.get_overflow_mask = msm_vfe46_get_overflow_mask,
			.get_rdi_wm_mask = msm_vfe46_get_rdi_wm_mask,
			.get_irq_mask = msm_vfe46_get_irq_mask,
			.get_halt_restart_mask =
				msm_vfe46_get_halt_restart_mask,
			.process_error_status = msm_vfe46_process_error_status,
			.is_module_cfg_lock_needed =
				msm_vfe46_is_module_cfg_lock_needed,
			.ahb_clk_cfg = NULL,
			.set_halt_restart_mask =
				msm_vfe46_set_halt_restart_mask,
			.set_bus_err_ign_mask = NULL,
			.get_bus_err_mask = NULL,
		},
		.stats_ops = {
			.get_stats_idx = msm_vfe46_get_stats_idx,
			.check_streams = msm_vfe46_stats_check_streams,
			.cfg_comp_mask = msm_vfe46_stats_cfg_comp_mask,
			.cfg_wm_irq_mask = msm_vfe46_stats_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe46_stats_clear_wm_irq_mask,
			.cfg_wm_reg = msm_vfe46_stats_cfg_wm_reg,
			.clear_wm_reg = msm_vfe46_stats_clear_wm_reg,
			.cfg_ub = msm_vfe46_stats_cfg_ub,
			.enable_module = msm_vfe46_stats_enable_module,
			.update_ping_pong_addr =
				msm_vfe46_stats_update_ping_pong_addr,
			.get_comp_mask = msm_vfe46_stats_get_comp_mask,
			.get_wm_mask = msm_vfe46_stats_get_wm_mask,
			.get_frame_id = msm_vfe46_stats_get_frame_id,
			.get_pingpong_status = msm_vfe46_get_pingpong_status,
			.update_cgc_override =
				msm_vfe46_stats_update_cgc_override,
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
	.dmi_reg_offset = 0xACC,
	.axi_hw_info = &msm_vfe46_axi_hw_info,
	.stats_hw_info = &msm_vfe46_stats_hw_info,
	.regulator_names = {"vdd"},
};
EXPORT_SYMBOL(vfe46_hw_info);

static const struct of_device_id msm_vfe46_dt_match[] = {
	{
		.compatible = "qcom,vfe46",
		.data = &vfe46_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, msm_vfe46_dt_match);

static struct platform_driver vfe46_driver = {
	.probe = vfe_hw_probe,
	.driver = {
		.name = "msm_vfe46",
		.owner = THIS_MODULE,
		.of_match_table = msm_vfe46_dt_match,
	},
};

static int __init msm_vfe46_init_module(void)
{
	return platform_driver_register(&vfe46_driver);
}

static void __exit msm_vfe46_exit_module(void)
{
	platform_driver_unregister(&vfe46_driver);
}

module_init(msm_vfe46_init_module);
module_exit(msm_vfe46_exit_module);
MODULE_DESCRIPTION("MSM VFE46 driver");
MODULE_LICENSE("GPL v2");

