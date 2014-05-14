/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <mach/iommu.h>

#include "msm_isp32.h"
#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp_stats_util.h"
#include "msm_isp.h"
#include "msm.h"
#include "msm_camera_io_util.h"

#define VFE32_BURST_LEN 3
#define VFE32_UB_SIZE 1024
#define VFE32_EQUAL_SLICE_UB 194
#define VFE32_AXI_SLICE_UB 792
#define VFE32_WM_BASE(idx) (0x4C + 0x18 * idx)
#define VFE32_RDI_BASE(idx) (idx ? 0x734 + 0x4 * (idx - 1) : 0x06FC)
#define VFE32_XBAR_BASE(idx) (0x40 + 0x4 * (idx / 4))
#define VFE32_XBAR_SHIFT(idx) ((idx % 4) * 8)
#define VFE32_PING_PONG_BASE(wm, ping_pong) \
	(VFE32_WM_BASE(wm) + 0x4 * (1 + (~(ping_pong >> wm) & 0x1)))

#define VFE32_NUM_STATS_TYPE 7
#define VFE32_STATS_PING_PONG_OFFSET 7
#define VFE32_STATS_BASE(idx) (0xF4 + 0xC * idx)
#define VFE32_STATS_PING_PONG_BASE(idx, ping_pong) \
	(VFE32_STATS_BASE(idx) + 0x4 * \
	(~(ping_pong >> (idx + VFE32_STATS_PING_PONG_OFFSET)) & 0x1))

#define VFE32_CLK_IDX 0
#define MSM_ISP32_TOTAL_WM_UB 792

static struct msm_cam_clk_info msm_vfe32_1_clk_info[] = {
	/*vfe32 clock info for B-family: 8610 */
	{"vfe_clk_src", 266670000},
	{"vfe_clk", -1},
	{"vfe_ahb_clk", -1},
	{"csi_vfe_clk", -1},
	{"bus_clk", -1},
};

static struct msm_cam_clk_info msm_vfe32_2_clk_info[] = {
	/*vfe32 clock info for A-family: 8960 */
	{"vfe_clk", 266667000},
	{"vfe_pclk", -1},
	{"csi_vfe_clk", -1},
};

static int msm_vfe32_init_hardware(struct vfe_device *vfe_dev)
{
	int rc = -1;
	vfe_dev->vfe_clk_idx = 0;
	rc = msm_isp_init_bandwidth_mgr(ISP_VFE0 + vfe_dev->pdev->id);
	if (rc < 0) {
		pr_err("%s: Bandwidth registration Failed!\n", __func__);
		goto bus_scale_register_failed;
	}

	if (vfe_dev->fs_vfe) {
		rc = regulator_enable(vfe_dev->fs_vfe);
		if (rc) {
			pr_err("%s: Regulator enable failed\n", __func__);
			goto fs_failed;
		}
	}
	else
		goto fs_failed;

	rc = msm_cam_clk_enable(&vfe_dev->pdev->dev, msm_vfe32_1_clk_info,
		 vfe_dev->vfe_clk, ARRAY_SIZE(msm_vfe32_1_clk_info), 1);
	if (rc < 0) {
		rc = msm_cam_clk_enable(&vfe_dev->pdev->dev,
			 msm_vfe32_2_clk_info, vfe_dev->vfe_clk,
			ARRAY_SIZE(msm_vfe32_2_clk_info), 1);
		if (rc < 0)
			goto clk_enable_failed;
		else
			vfe_dev->vfe_clk_idx = 2;
	} else
		vfe_dev->vfe_clk_idx = 1;

	vfe_dev->vfe_base = ioremap(vfe_dev->vfe_mem->start,
		resource_size(vfe_dev->vfe_mem));
	if (!vfe_dev->vfe_base) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto vfe_remap_failed;
	}

	rc = request_irq(vfe_dev->vfe_irq->start, msm_isp_process_irq,
					 IRQF_TRIGGER_RISING, "vfe", vfe_dev);
	if (rc < 0) {
		pr_err("%s: irq request failed\n", __func__);
		goto irq_req_failed;
	}

	return rc;
irq_req_failed:
	iounmap(vfe_dev->vfe_base);
vfe_remap_failed:
	if (vfe_dev->vfe_clk_idx == 1)
		msm_cam_clk_enable(&vfe_dev->pdev->dev,
				msm_vfe32_1_clk_info, vfe_dev->vfe_clk,
				ARRAY_SIZE(msm_vfe32_1_clk_info), 0);
	if (vfe_dev->vfe_clk_idx == 2)
		msm_cam_clk_enable(&vfe_dev->pdev->dev,
				msm_vfe32_2_clk_info, vfe_dev->vfe_clk,
				ARRAY_SIZE(msm_vfe32_2_clk_info), 0);
clk_enable_failed:
	regulator_disable(vfe_dev->fs_vfe);
fs_failed:
	msm_isp_deinit_bandwidth_mgr(ISP_VFE0 + vfe_dev->pdev->id);
bus_scale_register_failed:
	return rc;
}

static void msm_vfe32_release_hardware(struct vfe_device *vfe_dev)
{
	free_irq(vfe_dev->vfe_irq->start, vfe_dev);
	tasklet_kill(&vfe_dev->vfe_tasklet);
	iounmap(vfe_dev->vfe_base);
	if (vfe_dev->vfe_clk_idx == 1)
		msm_cam_clk_enable(&vfe_dev->pdev->dev,
				msm_vfe32_1_clk_info, vfe_dev->vfe_clk,
				ARRAY_SIZE(msm_vfe32_1_clk_info), 0);
	if (vfe_dev->vfe_clk_idx == 2)
		msm_cam_clk_enable(&vfe_dev->pdev->dev,
				msm_vfe32_2_clk_info, vfe_dev->vfe_clk,
				ARRAY_SIZE(msm_vfe32_2_clk_info), 0);
	regulator_disable(vfe_dev->fs_vfe);
	msm_isp_deinit_bandwidth_mgr(ISP_VFE0 + vfe_dev->pdev->id);
}

static void msm_vfe32_init_hardware_reg(struct vfe_device *vfe_dev)
{
	uint32_t wm_base, i = 0;
	/* CGC_OVERRIDE */
	msm_camera_io_w(0x07FFFFFF, vfe_dev->vfe_base + 0xC);
	/* BUS_CFG */
	msm_camera_io_w(0x00000009, vfe_dev->vfe_base + 0x3C);
	msm_camera_io_w(0x01000025, vfe_dev->vfe_base + 0x1C);
	msm_camera_io_w_mb(0x1CFFFFFF, vfe_dev->vfe_base + 0x20);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x24);
	msm_camera_io_w_mb(0x1FFFFFFF, vfe_dev->vfe_base + 0x28);
	msm_camera_io_w(0x0, vfe_dev->vfe_base+0x6FC);
	msm_camera_io_w( 0x10000000,vfe_dev->vfe_base + VFE32_RDI_BASE(1));
	msm_camera_io_w( 0x10000000,vfe_dev->vfe_base + VFE32_RDI_BASE(2));
	msm_camera_io_w(0x0, vfe_dev->vfe_base + VFE32_XBAR_BASE(0));
	msm_camera_io_w(0x0, vfe_dev->vfe_base + VFE32_XBAR_BASE(4));
	for (i = 0; i<=6; i++) {
		wm_base = VFE32_WM_BASE(i);
		msm_camera_io_w(0x0, vfe_dev->vfe_base + wm_base);
		wm_base = VFE32_WM_BASE(i);
		msm_camera_io_w(0x0, vfe_dev->vfe_base + wm_base);
	}

}

static void msm_vfe32_process_reset_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status1 & BIT(23))
		complete(&vfe_dev->reset_complete);
}

static void msm_vfe32_process_halt_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
}

static void msm_vfe32_process_camif_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	if (!(irq_status0 & 0x1F))
		return;

	if (irq_status0 & BIT(0)) {
		ISP_DBG("%s: SOF IRQ\n", __func__);
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].raw_stream_count > 0
			&& vfe_dev->axi_data.src_info[VFE_PIX_0].
			pix_stream_count == 0) {
			msm_isp_sof_notify(vfe_dev, VFE_PIX_0, ts);
			if (vfe_dev->axi_data.stream_update)
				msm_isp_axi_stream_update(vfe_dev);
			msm_isp_update_framedrop_reg(vfe_dev);
		}
	}
}

static void msm_vfe32_process_violation_status(struct vfe_device *vfe_dev)
{
	uint32_t violation_status = vfe_dev->error_info.violation_status;
	if (!violation_status)
		return;

	if (violation_status & BIT(0))
		pr_err("%s: black violation\n", __func__);
	if (violation_status & BIT(1))
		pr_err("%s: rolloff violation\n", __func__);
	if (violation_status & BIT(2))
		pr_err("%s: demux violation\n", __func__);
	if (violation_status & BIT(3))
		pr_err("%s: demosaic violation\n", __func__);
	if (violation_status & BIT(4))
		pr_err("%s: crop violation\n", __func__);
	if (violation_status & BIT(5))
		pr_err("%s: scale violation\n", __func__);
	if (violation_status & BIT(6))
		pr_err("%s: wb violation\n", __func__);
	if (violation_status & BIT(7))
		pr_err("%s: clf violation\n", __func__);
	if (violation_status & BIT(8))
		pr_err("%s: matrix violation\n", __func__);
	if (violation_status & BIT(9))
		pr_err("%s: rgb lut violation\n", __func__);
	if (violation_status & BIT(10))
		pr_err("%s: la violation\n", __func__);
	if (violation_status & BIT(11))
		pr_err("%s: chroma enhance violation\n", __func__);
	if (violation_status & BIT(12))
		pr_err("%s: chroma supress mce violation\n", __func__);
	if (violation_status & BIT(13))
		pr_err("%s: skin enhance violation\n", __func__);
	if (violation_status & BIT(14))
		pr_err("%s: asf violation\n", __func__);
	if (violation_status & BIT(15))
		pr_err("%s: scale y violation\n", __func__);
	if (violation_status & BIT(16))
		pr_err("%s: scale cbcr violation\n", __func__);
	if (violation_status & BIT(17))
		pr_err("%s: chroma subsample violation\n", __func__);
	if (violation_status & BIT(18))
		pr_err("%s: framedrop enc y violation\n", __func__);
	if (violation_status & BIT(19))
		pr_err("%s: framedrop enc cbcr violation\n", __func__);
	if (violation_status & BIT(20))
		pr_err("%s: framedrop view y violation\n", __func__);
	if (violation_status & BIT(21))
		pr_err("%s: framedrop view cbcr violation\n", __func__);
	if (violation_status & BIT(22))
		pr_err("%s: realign buf y violation\n", __func__);
	if (violation_status & BIT(23))
		pr_err("%s: realign buf cb violation\n", __func__);
	if (violation_status & BIT(24))
		pr_err("%s: realign buf cr violation\n", __func__);
}

static void msm_vfe32_process_error_status(struct vfe_device *vfe_dev)
{
	uint32_t error_status1 = vfe_dev->error_info.error_mask1;

	if (error_status1 & BIT(0))
		pr_err("%s: camif error status: 0x%x\n",
			__func__, vfe_dev->error_info.camif_status);
	if (error_status1 & BIT(1))
		pr_err("%s: stats bhist overwrite\n", __func__);
	if (error_status1 & BIT(2))
		pr_err("%s: stats cs overwrite\n", __func__);
	if (error_status1 & BIT(3))
		pr_err("%s: stats ihist overwrite\n", __func__);
	if (error_status1 & BIT(4))
		pr_err("%s: realign buf y overflow\n", __func__);
	if (error_status1 & BIT(5))
		pr_err("%s: realign buf cb overflow\n", __func__);
	if (error_status1 & BIT(6))
		pr_err("%s: realign buf cr overflow\n", __func__);
	if (error_status1 & BIT(7)) {
		pr_err("%s: violation\n", __func__);
		msm_vfe32_process_violation_status(vfe_dev);
	}
	if (error_status1 & BIT(8))
		pr_err("%s: image master 0 bus overflow\n", __func__);
	if (error_status1 & BIT(9))
		pr_err("%s: image master 1 bus overflow\n", __func__);
	if (error_status1 & BIT(10))
		pr_err("%s: image master 2 bus overflow\n", __func__);
	if (error_status1 & BIT(11))
		pr_err("%s: image master 3 bus overflow\n", __func__);
	if (error_status1 & BIT(12))
		pr_err("%s: image master 4 bus overflow\n", __func__);
	if (error_status1 & BIT(13))
		pr_err("%s: image master 5 bus overflow\n", __func__);
	if (error_status1 & BIT(14))
		pr_err("%s: image master 6 bus overflow\n", __func__);
	if (error_status1 & BIT(15))
		pr_err("%s: status ae/bg bus overflow\n", __func__);
	if (error_status1 & BIT(16))
		pr_err("%s: status af/bf bus overflow\n", __func__);
	if (error_status1 & BIT(17))
		pr_err("%s: status awb bus overflow\n", __func__);
	if (error_status1 & BIT(18))
		pr_err("%s: status rs bus overflow\n", __func__);
	if (error_status1 & BIT(19))
		pr_err("%s: status cs bus overflow\n", __func__);
	if (error_status1 & BIT(20))
		pr_err("%s: status ihist bus overflow\n", __func__);
	if (error_status1 & BIT(21))
		pr_err("%s: status skin bhist bus overflow\n", __func__);
	if (error_status1 & BIT(22))
		pr_err("%s: axi error\n", __func__);
}

static void msm_vfe32_read_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x2C);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x30);
	msm_camera_io_w(*irq_status0, vfe_dev->vfe_base + 0x24);
	msm_camera_io_w_mb(*irq_status1, vfe_dev->vfe_base + 0x28);
	msm_camera_io_w_mb(1, vfe_dev->vfe_base + 0x18);

	if (*irq_status1 & BIT(0))
		vfe_dev->error_info.camif_status =
			msm_camera_io_r(vfe_dev->vfe_base + 0x204);

	if (*irq_status1 & BIT(7))
		vfe_dev->error_info.violation_status |=
			msm_camera_io_r(vfe_dev->vfe_base + 0x7B4);
}

static void msm_vfe32_process_reg_update(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	int flag_rdi = 0;
	int flag_pix = 0;

	if (!(irq_status0 & 0x20) && !(irq_status1 & 0x1C000000))
		return;

	if (irq_status0 & BIT(5)) {
		flag_pix = 1;
		msm_isp_sof_notify(vfe_dev, VFE_PIX_0, ts);
	}
	if (irq_status1 & BIT(26)) {
		flag_rdi = (1 << VFE_RAW_0) | flag_rdi;
		msm_isp_sof_notify(vfe_dev, VFE_RAW_0, ts);
	}
	if (irq_status1 & BIT(27)) {
		flag_rdi = (1 << VFE_RAW_1) | flag_rdi;
		msm_isp_sof_notify(vfe_dev, VFE_RAW_1, ts);
	}
	if (irq_status1 & BIT(28)) {
		flag_rdi = (1 << VFE_RAW_2) | flag_rdi;
		msm_isp_sof_notify(vfe_dev, VFE_RAW_2, ts);
	}

	if (vfe_dev->axi_data.stream_update)
		msm_isp_axi_stream_update(vfe_dev);

	if (atomic_read(&vfe_dev->stats_data.stats_update))
		msm_isp_stats_stream_update(vfe_dev);

	if (flag_rdi)
		msm_isp_update_framedrop_rdi_reg(vfe_dev, flag_rdi);

	if (flag_pix)
		msm_isp_update_framedrop_reg(vfe_dev);

	msm_isp_update_error_frame_count(vfe_dev);

	vfe_dev->hw_info->vfe_ops.core_ops.
		reg_update(vfe_dev);
	return;
}

static void msm_vfe32_reg_update(
	struct vfe_device *vfe_dev)
{
	msm_camera_io_w_mb(0xF, vfe_dev->vfe_base + 0x260);
}

static uint32_t msm_vfe32_reset_values[ISP_RST_MAX] =
{
	0x3FF, /* ISP_RST_HARD reset everything */
	0x3EF /* ISP_RST_SOFT same as HARD RESET */
};

static long msm_vfe32_reset_hardware(struct vfe_device *vfe_dev ,
				enum msm_isp_reset_type reset_type)
{

	uint32_t rst_val;
	if (reset_type >= ISP_RST_MAX) {
		pr_err("%s: Error Invalid parameter\n", __func__);
		reset_type = ISP_RST_HARD;
	}
	rst_val = msm_vfe32_reset_values[reset_type];
	init_completion(&vfe_dev->reset_complete);
	msm_camera_io_w_mb(rst_val, vfe_dev->vfe_base + 0x4);
	return wait_for_completion_timeout(
	   &vfe_dev->reset_complete, msecs_to_jiffies(50));
}

static void msm_vfe32_axi_reload_wm(
	struct vfe_device *vfe_dev, uint32_t reload_mask)
{
	if (!vfe_dev->pdev->dev.of_node) {
		/*vfe32 A-family: 8960*/
		msm_camera_io_w_mb(reload_mask, vfe_dev->vfe_base + 0x38);
	} else {
		/*vfe32 B-family: 8610*/
		msm_camera_io_w(0x0, vfe_dev->vfe_base + 0x28);
		msm_camera_io_w(0x1C800000, vfe_dev->vfe_base + 0x20);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x18);
		msm_camera_io_w(0x9AAAAAAA , vfe_dev->vfe_base + 0x600);
		msm_camera_io_w(reload_mask, vfe_dev->vfe_base + 0x38);
	}
}

static void msm_vfe32_axi_enable_wm(struct vfe_device *vfe_dev,
	uint8_t wm_idx, uint8_t enable)
{
	uint32_t val = msm_camera_io_r(
	   vfe_dev->vfe_base + VFE32_WM_BASE(wm_idx));
	if (enable)
		val |= 0x1;
	else
		val &= ~0x1;
	msm_camera_io_w_mb(val,
		vfe_dev->vfe_base + VFE32_WM_BASE(wm_idx));
}

static void msm_vfe32_axi_cfg_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t comp_mask, comp_mask_index =
		stream_info->comp_mask_index;
	uint32_t irq_mask;

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x34);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	comp_mask |= (axi_data->composite_info[comp_mask_index].
		stream_composite_mask << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x34);

	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask |= BIT(comp_mask_index + 21);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_axi_clear_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t comp_mask, comp_mask_index = stream_info->comp_mask_index;
	uint32_t irq_mask;

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x34);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x34);

	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask &= ~BIT(comp_mask_index + 21);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_axi_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask |= BIT(stream_info->wm[0] + 6);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_axi_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask &= ~BIT(stream_info->wm[0] + 6);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_cfg_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t framedrop_pattern = 0, framedrop_period = 0;
	uint32_t rdi_reg_cfg;
	int32_t val = 0;

	if (stream_info->runtime_init_frame_drop == 0) {
		framedrop_pattern = stream_info->framedrop_pattern;
		framedrop_period = stream_info->framedrop_period;
	}

	if (stream_info->stream_type == BURST_STREAM &&
		stream_info->runtime_burst_frame_count == 0) {
		framedrop_pattern = 0;
		framedrop_period = 0;
	}

	if (stream_info->stream_src == PIX_ENCODER) {
		msm_camera_io_w(framedrop_period, vfe_dev->vfe_base + 0x504);
		msm_camera_io_w(framedrop_period, vfe_dev->vfe_base + 0x508);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x50C);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x510);
	} else if (stream_info->stream_src == PIX_VIEWFINDER) {
		msm_camera_io_w(framedrop_period, vfe_dev->vfe_base + 0x514);
		msm_camera_io_w(framedrop_period, vfe_dev->vfe_base + 0x518);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x51C);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x520);
	} else if (stream_info->stream_src == RDI_INTF_0 ||
			stream_info->stream_src == RDI_INTF_1 ||
			stream_info->stream_src == RDI_INTF_2) {
		if (stream_info->runtime_init_frame_drop) {
			rdi_reg_cfg = msm_camera_io_r(
			vfe_dev->vfe_base +
			VFE32_RDI_BASE(stream_info->stream_src - VFE_SRC_MAX));
			val = ((stream_info->runtime_init_frame_drop + 1) << 20)
							| 0x1000000;
			rdi_reg_cfg |= val;
			msm_camera_io_w(rdi_reg_cfg, vfe_dev->vfe_base +
			VFE32_RDI_BASE(stream_info->stream_src - VFE_SRC_MAX));
		} else if (0 == stream_info->runtime_init_frame_drop) {
			rdi_reg_cfg = msm_camera_io_r(
			vfe_dev->vfe_base +
			VFE32_RDI_BASE(stream_info->stream_src - VFE_SRC_MAX));
			if (stream_info->framedrop_period == 0 ||
				stream_info->framedrop_period == 1)
				rdi_reg_cfg = 0xFE0FFFFF & rdi_reg_cfg;
			else
				rdi_reg_cfg = 0xFF0FFFFF & rdi_reg_cfg;
			val = stream_info->framedrop_period << 20;
			rdi_reg_cfg = rdi_reg_cfg | val;
			msm_camera_io_w(rdi_reg_cfg, vfe_dev->vfe_base +
			VFE32_RDI_BASE(stream_info->stream_src - VFE_SRC_MAX));
		}
	}
	msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x260);
}

static void msm_vfe32_clear_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	if (stream_info->stream_src == PIX_ENCODER) {
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x50C);
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x510);
	} else if (stream_info->stream_src == PIX_VIEWFINDER) {
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x51C);
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x520);
	}
}

static int32_t msm_vfe32_cfg_io_format(struct vfe_device *vfe_dev,
	enum msm_vfe_axi_stream_src stream_src, uint32_t io_format)
{
	int bpp, bpp_reg = 0, pack_fmt = 0, pack_reg = 0;
	uint32_t io_format_reg;
	bpp = msm_isp_get_bit_per_pixel(io_format);
	if (bpp < 0) {
		pr_err("%s:%d invalid io_format %d bpp %d", __func__, __LINE__,
			io_format, bpp);
		return -EINVAL;
	}

	switch (bpp) {
	case 8:
		bpp_reg = 0;
		break;
	case 10:
		bpp_reg = 1 << 0;
		break;
	case 12:
		bpp_reg = 1 << 1;
		break;
	default:
		pr_err("%s:%d invalid bpp %d", __func__, __LINE__, bpp);
		return -EINVAL;
	}

	if (stream_src == IDEAL_RAW) {
		pack_fmt = msm_isp_get_pack_format(io_format);
		switch (pack_fmt) {
		case QCOM:
			pack_reg = 0x0;
			break;
		case MIPI:
			pack_reg = 0x1;
			break;
		case DPCM6:
			pack_reg = 0x2;
			break;
		case DPCM8:
			pack_reg = 0x3;
			break;
		case PLAIN8:
			pack_reg = 0x4;
			break;
		case PLAIN16:
			pack_reg = 0x5;
			break;
		default:
			pr_err("%s: invalid pack fmt!\n", __func__);
			return -EINVAL;
		}
	}

	io_format_reg = msm_camera_io_r(vfe_dev->vfe_base + 0x6F8);
	switch (stream_src) {
	case PIX_ENCODER:
	case PIX_VIEWFINDER:
	case CAMIF_RAW:
		io_format_reg &= 0xFFFFCFFF;
		io_format_reg |= bpp_reg << 12;
		break;
	case IDEAL_RAW:
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
	msm_camera_io_w(io_format_reg, vfe_dev->vfe_base + 0x6F8);
	return 0;
}

static void msm_vfe32_cfg_camif(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint16_t first_pixel, last_pixel, first_line, last_line;
	struct msm_vfe_camif_cfg *camif_cfg = &pix_cfg->camif_cfg;
	uint32_t val;

	first_pixel = camif_cfg->first_pixel;
	last_pixel = camif_cfg->last_pixel;
	first_line = camif_cfg->first_line;
	last_line = camif_cfg->last_line;

	msm_camera_io_w(pix_cfg->input_mux << 16 | pix_cfg->pixel_pattern,
					vfe_dev->vfe_base + 0x14);

	msm_camera_io_w(camif_cfg->lines_per_frame << 16 |
					camif_cfg->pixels_per_line,
					vfe_dev->vfe_base + 0x1EC);

	msm_camera_io_w(first_pixel << 16 | last_pixel,
					vfe_dev->vfe_base + 0x1F0);

	msm_camera_io_w(first_line << 16 | last_line,
					vfe_dev->vfe_base + 0x1F4);

	val = msm_camera_io_r(vfe_dev->vfe_base + 0x6FC);
	val &= 0xFFFFFFFC;
	val |= camif_cfg->camif_input;
	msm_camera_io_w(val, vfe_dev->vfe_base + 0x6FC);
}

static void msm_vfe32_update_camif_state(
	struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state update_state)
{
	uint32_t val;
	bool bus_en, vfe_en;
	if (update_state == NO_UPDATE)
		return;

	val = msm_camera_io_r(vfe_dev->vfe_base + 0x1E4);
	if (update_state == ENABLE_CAMIF) {
		bus_en =
		((vfe_dev->axi_data.src_info[
			VFE_PIX_0].raw_stream_count > 0) ? 1 : 0);
		vfe_en =
		((vfe_dev->axi_data.src_info[
			VFE_PIX_0].pix_stream_count > 0) ? 1 : 0);
		val &= 0xFFFFFF3F;
		val = val | bus_en << 7 | vfe_en << 6;
		msm_camera_io_w(val, vfe_dev->vfe_base + 0x1E4);
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x1E0);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 1;
	} else if (update_state == DISABLE_CAMIF) {
		msm_camera_io_w_mb(0x0, vfe_dev->vfe_base + 0x1E0);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 0;
	} else if (update_state == DISABLE_CAMIF_IMMEDIATELY) {
		msm_camera_io_w_mb(0x6, vfe_dev->vfe_base + 0x1E0);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 0;
	}
}

static void msm_vfe32_cfg_rdi_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_rdi_cfg *rdi_cfg, enum msm_vfe_input_src input_src)
{
	uint8_t rdi = input_src - VFE_RAW_0;
	uint32_t rdi_reg_cfg;
	rdi_reg_cfg = msm_camera_io_r(
		vfe_dev->vfe_base + VFE32_RDI_BASE(0));
	rdi_reg_cfg &= ~(BIT(16 + rdi));
	rdi_reg_cfg |= rdi_cfg->frame_based << (16 + rdi);
	msm_camera_io_w(rdi_reg_cfg,
		vfe_dev->vfe_base + VFE32_RDI_BASE(0));

	rdi_reg_cfg = msm_camera_io_r(
		vfe_dev->vfe_base + VFE32_RDI_BASE(rdi));
	rdi_reg_cfg &= 0x70003;
	rdi_reg_cfg |= (rdi * 3) << 28 | rdi_cfg->cid << 4 | 0x4;
	msm_camera_io_w(
		rdi_reg_cfg, vfe_dev->vfe_base + VFE32_RDI_BASE(rdi));

}

static void msm_vfe32_axi_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx)
{
	uint32_t val;
	uint32_t wm_base = VFE32_WM_BASE(stream_info->wm[plane_idx]);

	if (!stream_info->frame_based) {
		/*WR_IMAGE_SIZE*/
		val =
			((msm_isp_cal_word_per_line(
			stream_info->output_format,
			stream_info->plane_cfg[plane_idx].
			output_width)+1)/2 - 1) << 16 |
			(stream_info->plane_cfg[plane_idx].
			output_height - 1);
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x10);

		/*WR_BUFFER_CFG*/
		val =
			msm_isp_cal_word_per_line(
			stream_info->output_format,
			stream_info->plane_cfg[plane_idx].
			output_stride) << 16 |
			(stream_info->plane_cfg[plane_idx].
			output_height - 1) << 4 | VFE32_BURST_LEN;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	} else {
		msm_camera_io_w(0x2, vfe_dev->vfe_base + wm_base);
		val =
			msm_isp_cal_word_per_line(
			stream_info->output_format,
			stream_info->plane_cfg[plane_idx].
			output_width) << 16 |
			(stream_info->plane_cfg[plane_idx].
			output_height - 1) << 4 | VFE32_BURST_LEN;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	}
	return;
}

static void msm_vfe32_axi_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint32_t val = 0;
	uint32_t wm_base = VFE32_WM_BASE(stream_info->wm[plane_idx]);
	/* FRAME BASED */
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base);
	/*WR_IMAGE_SIZE*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x10);
	/*WR_BUFFER_CFG*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	return;
}

static void msm_vfe32_axi_cfg_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	struct msm_vfe_axi_plane_cfg *plane_cfg =
		&stream_info->plane_cfg[plane_idx];
	uint8_t wm = stream_info->wm[plane_idx];
	uint32_t xbar_cfg = 0;
	uint32_t xbar_reg_cfg = 0;

	switch (stream_info->stream_src) {
	case PIX_ENCODER:
	case PIX_VIEWFINDER: {
		if (plane_cfg->output_plane_format != CRCB_PLANE &&
			plane_cfg->output_plane_format != CBCR_PLANE) {
			/*SINGLE_STREAM_SEL*/
			xbar_cfg |= plane_cfg->output_plane_format << 5;
		} else {
			switch (stream_info->output_format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV14:
			case V4L2_PIX_FMT_NV16:
				xbar_cfg |= 0x3 << 3; /*PAIR_STREAM_SWAP_CTRL*/
				break;
			}
			xbar_cfg |= BIT(1); /*PAIR_STREAM_EN*/
		}
		if (stream_info->stream_src == PIX_VIEWFINDER)
			xbar_cfg |= 0x1; /*VIEW_STREAM_EN*/
		break;
	}
	case CAMIF_RAW:
		xbar_cfg = 0x60;
		break;
	case IDEAL_RAW:
		xbar_cfg = 0x80;
		break;
	case RDI_INTF_0:
		xbar_cfg = 0xA0;
		break;
	case RDI_INTF_1:
		xbar_cfg = 0xC0;
		break;
	case RDI_INTF_2:
		xbar_cfg = 0xE0;
		break;
	default:
		pr_err("%s: Invalid stream src\n", __func__);
	}
	xbar_reg_cfg = msm_camera_io_r(vfe_dev->vfe_base + VFE32_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFF << VFE32_XBAR_SHIFT(wm));
	xbar_reg_cfg |= (xbar_cfg << VFE32_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg, vfe_dev->vfe_base + VFE32_XBAR_BASE(wm));
	return;
}

static void msm_vfe32_axi_clear_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint8_t wm = stream_info->wm[plane_idx];
	uint32_t xbar_reg_cfg = 0;

	xbar_reg_cfg = msm_camera_io_r(vfe_dev->vfe_base + VFE32_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFF << VFE32_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg, vfe_dev->vfe_base + VFE32_XBAR_BASE(wm));
}

static void msm_vfe32_cfg_axi_ub_equal_default(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = 0;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t total_image_size = 0;
	uint32_t num_used_wms = 0;
	uint32_t prop_size = 0;
	uint32_t wm_ub_size;
	uint64_t delta;
	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		if (axi_data->free_wm[i] > 0) {
			num_used_wms++;
			total_image_size += axi_data->wm_image_size[i];
		}
	}
	prop_size = MSM_ISP32_TOTAL_WM_UB -
		axi_data->hw_info->min_wm_ub * num_used_wms;
	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		if (axi_data->free_wm[i]) {
			delta =
				(uint64_t)(axi_data->wm_image_size[i] *
					prop_size);
			do_div(delta, total_image_size);
			wm_ub_size = axi_data->hw_info->min_wm_ub +
				(uint32_t)delta;
			msm_camera_io_w(ub_offset << 16 |
				(wm_ub_size - 1), vfe_dev->vfe_base +
					VFE32_WM_BASE(i) + 0xC);
			ub_offset += wm_ub_size;
		} else
			msm_camera_io_w(0,
				vfe_dev->vfe_base + VFE32_WM_BASE(i) + 0xC);
	}
}

static void msm_vfe32_cfg_axi_ub_equal_slicing(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = 0;
	uint32_t final_ub_slice_size;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		if (ub_offset + VFE32_EQUAL_SLICE_UB > VFE32_AXI_SLICE_UB) {
			final_ub_slice_size = VFE32_AXI_SLICE_UB - ub_offset;
			msm_camera_io_w(ub_offset << 16 |
				(final_ub_slice_size - 1), vfe_dev->vfe_base +
				VFE32_WM_BASE(i) + 0xC);
			ub_offset += final_ub_slice_size;
		} else {
			msm_camera_io_w(ub_offset << 16 |
				(VFE32_EQUAL_SLICE_UB - 1), vfe_dev->vfe_base +
				VFE32_WM_BASE(i) + 0xC);
			ub_offset += VFE32_EQUAL_SLICE_UB;
		}
	}
}

static void msm_vfe32_cfg_axi_ub(struct vfe_device *vfe_dev)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	axi_data->wm_ub_cfg_policy = MSM_WM_UB_EQUAL_SLICING;
	if (axi_data->wm_ub_cfg_policy == MSM_WM_UB_EQUAL_SLICING)
		msm_vfe32_cfg_axi_ub_equal_slicing(vfe_dev);
	else
		msm_vfe32_cfg_axi_ub_equal_default(vfe_dev);
}

static void msm_vfe32_update_ping_pong_addr(struct vfe_device *vfe_dev,
		uint8_t wm_idx, uint32_t pingpong_status, unsigned long paddr)
{
	msm_camera_io_w(paddr, vfe_dev->vfe_base +
		VFE32_PING_PONG_BASE(wm_idx, pingpong_status));
}

static long msm_vfe32_axi_halt(struct vfe_device *vfe_dev)
{
	uint32_t halt_mask;
	uint32_t axi_busy_flag = true;

	msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x1D8);
	while (axi_busy_flag) {
		if (msm_camera_io_r(
			vfe_dev->vfe_base + 0x1DC) & 0x1)
			axi_busy_flag = false;
	}
	msm_camera_io_w_mb(0, vfe_dev->vfe_base + 0x1D8);
	halt_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x20);
	halt_mask &= 0xFEFFFFFF;
	/* Disable AXI IRQ */
	msm_camera_io_w_mb(halt_mask, vfe_dev->vfe_base + 0x20);
	return 0;
}

static uint32_t msm_vfe32_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 6) & 0x7F;
}

static uint32_t msm_vfe32_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 21) & 0x7;
}

static uint32_t msm_vfe32_get_pingpong_status(struct vfe_device *vfe_dev)
{
	return msm_camera_io_r(vfe_dev->vfe_base + 0x180);
}

static int msm_vfe32_get_stats_idx(enum msm_isp_stats_type stats_type)
{
	switch (stats_type) {
	case MSM_ISP_STATS_AEC:
	case MSM_ISP_STATS_BG:
		return 0;
	case MSM_ISP_STATS_AF:
	case MSM_ISP_STATS_BF:
		return 1;
	case MSM_ISP_STATS_AWB:
		return 2;
	case MSM_ISP_STATS_RS:
		return 3;
	case MSM_ISP_STATS_CS:
		return 4;
	case MSM_ISP_STATS_IHIST:
		return 5;
	case MSM_ISP_STATS_SKIN:
	case MSM_ISP_STATS_BHIST:
		return 6;
	default:
		pr_err("%s: Invalid stats type\n", __func__);
		return -EINVAL;
	}
}

static void msm_vfe32_stats_cfg_comp_mask(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	return;
}

static void msm_vfe32_stats_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask |= BIT(STATS_IDX(stream_info->stream_handle) + 13);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
	return;
}

static void msm_vfe32_stats_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask &= ~(BIT(STATS_IDX(stream_info->stream_handle) + 13));
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
	return;
}

static void msm_vfe32_stats_cfg_wm_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	/*Nothing to configure for VFE3.x*/
	return;
}

static void msm_vfe32_stats_clear_wm_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	/*Nothing to configure for VFE3.x*/
	return;
}

static void msm_vfe32_stats_cfg_ub(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = VFE32_UB_SIZE;
	uint32_t ub_size[VFE32_NUM_STATS_TYPE] = {
		107, /*MSM_ISP_STATS_BG*/
		92, /*MSM_ISP_STATS_BF*/
		2, /*MSM_ISP_STATS_AWB*/
		7,  /*MSM_ISP_STATS_RS*/
		16, /*MSM_ISP_STATS_CS*/
		2, /*MSM_ISP_STATS_IHIST*/
		7, /*MSM_ISP_STATS_BHIST*/
	};

	for (i = 0; i < VFE32_NUM_STATS_TYPE; i++) {
		ub_offset -= ub_size[i];
		msm_camera_io_w(ub_offset << 16 | (ub_size[i] - 1),
			vfe_dev->vfe_base + VFE32_STATS_BASE(i) + 0x8);
	}
	return;
}

static void msm_vfe32_stats_enable_module(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	int i;
	uint32_t module_cfg, module_cfg_mask = 0;

	for (i = 0; i < VFE32_NUM_STATS_TYPE; i++) {
		if ((stats_mask >> i) & 0x1) {
			switch (i) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				module_cfg_mask |= 1 << (5 + i);
				break;
			case 5:
				module_cfg_mask |= 1 << 16;
				break;
			case 6:
				module_cfg_mask |= 1 << 19;
				break;
			default:
				pr_err("%s: Invalid stats mask\n", __func__);
				return;
			}
		}
	}

	module_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x10);
	if (enable)
		module_cfg |= module_cfg_mask;
	else
		module_cfg &= ~module_cfg_mask;
	msm_camera_io_w(module_cfg, vfe_dev->vfe_base + 0x10);
}

static void msm_vfe32_stats_update_ping_pong_addr(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info, uint32_t pingpong_status,
	unsigned long paddr)
{
	int stats_idx = STATS_IDX(stream_info->stream_handle);
	msm_camera_io_w(paddr, vfe_dev->vfe_base +
		VFE32_STATS_PING_PONG_BASE(stats_idx, pingpong_status));
}

static uint32_t msm_vfe32_stats_get_wm_mask(uint32_t irq_status0,
	uint32_t irq_status1)
{
	return (irq_status0 >> 13) & 0x7F;
}

static uint32_t msm_vfe32_stats_get_comp_mask(uint32_t irq_status0,
	uint32_t irq_status1)
{
	return (irq_status0 >> 24) & 0x1;
}

static uint32_t msm_vfe32_stats_get_frame_id(struct vfe_device *vfe_dev)
{
	return vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
}

static int msm_vfe32_get_platform_data(struct vfe_device *vfe_dev)
{
	int rc = 0;
	vfe_dev->vfe_mem = platform_get_resource_byname(vfe_dev->pdev,
					IORESOURCE_MEM, "vfe");
	if (!vfe_dev->vfe_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}

	vfe_dev->vfe_irq = platform_get_resource_byname(vfe_dev->pdev,
					IORESOURCE_IRQ, "vfe");
	if (!vfe_dev->vfe_irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}

	vfe_dev->fs_vfe = regulator_get(&vfe_dev->pdev->dev, "vdd");
	if (IS_ERR(vfe_dev->fs_vfe)) {
		pr_err("%s: Regulator get failed %ld\n", __func__,
			PTR_ERR(vfe_dev->fs_vfe));
		vfe_dev->fs_vfe = NULL;
		rc = -ENODEV;
		goto vfe_no_resource;
	}

	if (!vfe_dev->pdev->dev.of_node)
		vfe_dev->iommu_ctx[0] = msm_iommu_get_ctx("vfe_imgwr");
	else
		vfe_dev->iommu_ctx[0] = msm_iommu_get_ctx("vfe0");

	if (!vfe_dev->iommu_ctx[0]) {
		pr_err("%s: no iommux ctx resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}

	if (!vfe_dev->pdev->dev.of_node)
		vfe_dev->iommu_ctx[1] = msm_iommu_get_ctx("vfe_misc");
	else
		vfe_dev->iommu_ctx[1] = msm_iommu_get_ctx("vfe0");

	if (!vfe_dev->iommu_ctx[1]) {
		pr_err("%s: no iommux ctx resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}

vfe_no_resource:
	return rc;
}

static void msm_vfe32_get_error_mask(uint32_t *error_mask0,
	uint32_t *error_mask1)
{
	*error_mask0 = 0x00000000;
	*error_mask1 = 0x007FFFFF;
}

struct msm_vfe_axi_hardware_info msm_vfe32_axi_hw_info = {
	.num_wm = 6,
	.num_comp_mask = 3,
	.num_rdi = 3,
	.num_rdi_master = 3,
	.min_wm_ub = 64,
};

static struct msm_vfe_stats_hardware_info msm_vfe32_stats_hw_info = {
	.stats_capability_mask =
		1 << MSM_ISP_STATS_AEC | 1 << MSM_ISP_STATS_BG |
		1 << MSM_ISP_STATS_AF | 1 << MSM_ISP_STATS_BF |
		1 << MSM_ISP_STATS_AWB | 1 << MSM_ISP_STATS_IHIST |
		1 << MSM_ISP_STATS_RS | 1 << MSM_ISP_STATS_CS |
		1 << MSM_ISP_STATS_SKIN | 1 << MSM_ISP_STATS_BHIST,
	.stats_ping_pong_offset = VFE32_STATS_PING_PONG_OFFSET,
	.num_stats_type = VFE32_NUM_STATS_TYPE,
	.num_stats_comp_mask = 0,
};

static struct v4l2_subdev_core_ops msm_vfe32_subdev_core_ops = {
	.ioctl = msm_isp_ioctl,
	.subscribe_event = msm_isp_subscribe_event,
	.unsubscribe_event = msm_isp_unsubscribe_event,
};

static struct v4l2_subdev_ops msm_vfe32_subdev_ops = {
	.core = &msm_vfe32_subdev_core_ops,
};

static struct v4l2_subdev_internal_ops msm_vfe32_internal_ops = {
	.open = msm_isp_open_node,
	.close = msm_isp_close_node,
};

struct msm_vfe_hardware_info vfe32_hw_info = {
	.num_iommu_ctx = 2,
	.vfe_clk_idx = VFE32_CLK_IDX,
	.vfe_ops = {
		.irq_ops = {
			.read_irq_status = msm_vfe32_read_irq_status,
			.process_camif_irq = msm_vfe32_process_camif_irq,
			.process_reset_irq = msm_vfe32_process_reset_irq,
			.process_halt_irq = msm_vfe32_process_halt_irq,
			.process_reg_update = msm_vfe32_process_reg_update,
			.process_axi_irq = msm_isp_process_axi_irq,
			.process_stats_irq = msm_isp_process_stats_irq,
		},
		.axi_ops = {
			.reload_wm = msm_vfe32_axi_reload_wm,
			.enable_wm = msm_vfe32_axi_enable_wm,
			.cfg_io_format = msm_vfe32_cfg_io_format,
			.cfg_comp_mask = msm_vfe32_axi_cfg_comp_mask,
			.clear_comp_mask = msm_vfe32_axi_clear_comp_mask,
			.cfg_wm_irq_mask = msm_vfe32_axi_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe32_axi_clear_wm_irq_mask,
			.cfg_framedrop = msm_vfe32_cfg_framedrop,
			.clear_framedrop = msm_vfe32_clear_framedrop,
			.cfg_wm_reg = msm_vfe32_axi_cfg_wm_reg,
			.clear_wm_reg = msm_vfe32_axi_clear_wm_reg,
			.cfg_wm_xbar_reg = msm_vfe32_axi_cfg_wm_xbar_reg,
			.clear_wm_xbar_reg = msm_vfe32_axi_clear_wm_xbar_reg,
			.cfg_ub = msm_vfe32_cfg_axi_ub,
			.update_ping_pong_addr =
				msm_vfe32_update_ping_pong_addr,
			.get_comp_mask = msm_vfe32_get_comp_mask,
			.get_wm_mask = msm_vfe32_get_wm_mask,
			.get_pingpong_status = msm_vfe32_get_pingpong_status,
			.halt = msm_vfe32_axi_halt,
		},
		.core_ops = {
			.reg_update = msm_vfe32_reg_update,
			.cfg_camif = msm_vfe32_cfg_camif,
			.update_camif_state = msm_vfe32_update_camif_state,
			.cfg_rdi_reg = msm_vfe32_cfg_rdi_reg,
			.reset_hw = msm_vfe32_reset_hardware,
			.init_hw = msm_vfe32_init_hardware,
			.init_hw_reg = msm_vfe32_init_hardware_reg,
			.release_hw = msm_vfe32_release_hardware,
			.get_platform_data = msm_vfe32_get_platform_data,
			.get_error_mask = msm_vfe32_get_error_mask,
			.process_error_status = msm_vfe32_process_error_status,
		},
		.stats_ops = {
			.get_stats_idx = msm_vfe32_get_stats_idx,
			.cfg_comp_mask = msm_vfe32_stats_cfg_comp_mask,
			.cfg_wm_irq_mask = msm_vfe32_stats_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe32_stats_clear_wm_irq_mask,
			.cfg_wm_reg = msm_vfe32_stats_cfg_wm_reg,
			.clear_wm_reg = msm_vfe32_stats_clear_wm_reg,
			.cfg_ub = msm_vfe32_stats_cfg_ub,
			.enable_module = msm_vfe32_stats_enable_module,
			.update_ping_pong_addr =
				msm_vfe32_stats_update_ping_pong_addr,
			.get_comp_mask = msm_vfe32_stats_get_comp_mask,
			.get_wm_mask = msm_vfe32_stats_get_wm_mask,
			.get_frame_id = msm_vfe32_stats_get_frame_id,
			.get_pingpong_status = msm_vfe32_get_pingpong_status,
		},
	},
	.dmi_reg_offset = 0x5A0,
	.axi_hw_info = &msm_vfe32_axi_hw_info,
	.stats_hw_info = &msm_vfe32_stats_hw_info,
	.subdev_ops = &msm_vfe32_subdev_ops,
	.subdev_internal_ops = &msm_vfe32_internal_ops,
};
EXPORT_SYMBOL(vfe32_hw_info);
