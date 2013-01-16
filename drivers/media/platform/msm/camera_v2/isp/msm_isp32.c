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

#include <linux/module.h>

#include "msm_isp32.h"
#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp.h"
#include "msm.h"

#define VFE32_BURST_LEN 4
#define VFE32_EQUAL_SLICE_UB 117
#define VFE32_WM_BASE(idx) (0x4C + 0x18 * idx)
#define VFE32_RDI_BASE(idx) (0x734 + 0x4 * idx)
#define VFE32_RDI_MN_BASE(m) (0x734 + 0x4 * m/3)
#define VFE32_RDI_MN_SEL_SHIFT(m) (4*(m%3) + 4)
#define VFE32_RDI_MN_FB_SHIFT(m) ((m%3) + 16)
#define VFE32_XBAR_BASE(idx) (0x40 + 0x4 * (idx / 4))
#define VFE32_XBAR_SHIFT(idx) ((idx % 4) * 8)
#define VFE32_PING_PONG_BASE(wm, ping_pong) \
	(VFE32_WM_BASE(wm) + 0x4 * (1 + (~(ping_pong >> wm) & 0x1)))

/*Temporary use fixed bus vectors in VFE */
static struct msm_bus_vectors msm_vfe32_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors msm_vfe32_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1027648000,
		.ib  = 1105920000,
	},
};

static struct msm_bus_paths msm_vfe32_bus_client_config[] = {
	{
		ARRAY_SIZE(msm_vfe32_init_vectors),
		msm_vfe32_init_vectors,
	},
	{
		ARRAY_SIZE(msm_vfe32_preview_vectors),
		msm_vfe32_preview_vectors,
	},
};

static struct msm_bus_scale_pdata msm_vfe32_bus_client_pdata = {
		msm_vfe32_bus_client_config,
		ARRAY_SIZE(msm_vfe32_bus_client_config),
		.name = "msm_camera_vfe",
};

static struct msm_cam_clk_info msm_vfe32_clk_info[] = {
	{"vfe_clk", 228570000},
	{"vfe_pclk", -1},
	{"csi_vfe_clk", -1},
};

static int msm_vfe32_init_hardware(struct vfe_device *vfe_dev)
{
	int rc = -1;

	vfe_dev->bus_perf_client =
		msm_bus_scale_register_client(&msm_vfe32_bus_client_pdata);
	if (!vfe_dev->bus_perf_client) {
		pr_err("%s: Registration Failed!\n", __func__);
		vfe_dev->bus_perf_client = 0;
		goto bus_scale_register_failed;
	}
	msm_bus_scale_client_update_request(vfe_dev->bus_perf_client, 1);

	if (vfe_dev->fs_vfe) {
		rc = regulator_enable(vfe_dev->fs_vfe);
		if (rc) {
			pr_err("%s: Regulator enable failed\n", __func__);
			goto fs_failed;
		}
	}

	rc = msm_cam_clk_enable(&vfe_dev->pdev->dev, msm_vfe32_clk_info,
		 vfe_dev->vfe_clk, ARRAY_SIZE(msm_vfe32_clk_info), 1);
	if (rc < 0)
		goto clk_enable_failed;

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
	msm_cam_clk_enable(&vfe_dev->pdev->dev, msm_vfe32_clk_info,
		 vfe_dev->vfe_clk, ARRAY_SIZE(msm_vfe32_clk_info), 0);
clk_enable_failed:
	regulator_disable(vfe_dev->fs_vfe);
fs_failed:
	msm_bus_scale_client_update_request(vfe_dev->bus_perf_client, 0);
	msm_bus_scale_unregister_client(vfe_dev->bus_perf_client);
bus_scale_register_failed:
	return rc;
}

static void msm_vfe32_release_hardware(struct vfe_device *vfe_dev)
{
	free_irq(vfe_dev->vfe_irq->start, vfe_dev);
	tasklet_kill(&vfe_dev->vfe_tasklet);
	iounmap(vfe_dev->vfe_base);
	msm_cam_clk_enable(&vfe_dev->pdev->dev, msm_vfe32_clk_info,
		 vfe_dev->vfe_clk, ARRAY_SIZE(msm_vfe32_clk_info), 0);
	regulator_disable(vfe_dev->fs_vfe);
	msm_bus_scale_client_update_request(vfe_dev->bus_perf_client, 0);
	msm_bus_scale_unregister_client(vfe_dev->bus_perf_client);
}

static void msm_vfe32_init_hardware_reg(struct vfe_device *vfe_dev)
{
	/* CGC_OVERRIDE */
	msm_camera_io_w(0x07FFFFFF, vfe_dev->vfe_base + 0xC);
	/* BUS_CFG */
	msm_camera_io_w(0x00000001, vfe_dev->vfe_base + 0x3C);
	msm_camera_io_w(0x00000025, vfe_dev->vfe_base + 0x1C);
	msm_camera_io_w_mb(0x1DFFFFFF, vfe_dev->vfe_base + 0x20);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x24);
	msm_camera_io_w_mb(0x1FFFFFFF, vfe_dev->vfe_base + 0x28);
}

static void msm_vfe32_process_reset_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status1 & (1 << 23))
		complete(&vfe_dev->reset_complete);
}

static void msm_vfe32_process_halt_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status1 & (1 << 24))
		complete(&vfe_dev->halt_complete);
}

static void msm_vfe32_process_camif_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (!(irq_status0 & 0x1F))
		return;

	if (irq_status0 & (1 << 0)) {
		ISP_DBG("%s: PIX0 frame id: %lu\n", __func__,
			   vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		msm_isp_update_framedrop_count(vfe_dev);
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id++;
	}
}

static void msm_vfe32_process_violation_irq(struct vfe_device *vfe_dev)
{
	uint32_t violation_status;
	violation_status = msm_camera_io_r(vfe_dev->vfe_base + 0x48);
	if (!violation_status)
		return;

	if (violation_status & (1 << 0))
		pr_err("%s: black violation\n", __func__);
	if (violation_status & (1 << 1))
		pr_err("%s: rolloff violation\n", __func__);
	if (violation_status & (1 << 2))
		pr_err("%s: demux violation\n", __func__);
	if (violation_status & (1 << 3))
		pr_err("%s: demosaic violation\n", __func__);
	if (violation_status & (1 << 4))
		pr_err("%s: crop violation\n", __func__);
	if (violation_status & (1 << 5))
		pr_err("%s: scale violation\n", __func__);
	if (violation_status & (1 << 6))
		pr_err("%s: wb violation\n", __func__);
	if (violation_status & (1 << 7))
		pr_err("%s: clf violation\n", __func__);
	if (violation_status & (1 << 8))
		pr_err("%s: matrix violation\n", __func__);
	if (violation_status & (1 << 9))
		pr_err("%s: rgb lut violation\n", __func__);
	if (violation_status & (1 << 10))
		pr_err("%s: la violation\n", __func__);
	if (violation_status & (1 << 11))
		pr_err("%s: chroma enhance violation\n", __func__);
	if (violation_status & (1 << 12))
		pr_err("%s: chroma supress mce violation\n", __func__);
	if (violation_status & (1 << 13))
		pr_err("%s: skin enhance violation\n", __func__);
	if (violation_status & (1 << 14))
		pr_err("%s: asf violation\n", __func__);
	if (violation_status & (1 << 15))
		pr_err("%s: scale y violation\n", __func__);
	if (violation_status & (1 << 16))
		pr_err("%s: scale cbcr violation\n", __func__);
	if (violation_status & (1 << 17))
		pr_err("%s: chroma subsample violation\n", __func__);
	if (violation_status & (1 << 18))
		pr_err("%s: framedrop enc y violation\n", __func__);
	if (violation_status & (1 << 19))
		pr_err("%s: framedrop enc cbcr violation\n", __func__);
	if (violation_status & (1 << 20))
		pr_err("%s: framedrop view y violation\n", __func__);
	if (violation_status & (1 << 21))
		pr_err("%s: framedrop view cbcr violation\n", __func__);
	if (violation_status & (1 << 22))
		pr_err("%s: realign buf y violation\n", __func__);
	if (violation_status & (1 << 23))
		pr_err("%s: realign buf cb violation\n", __func__);
	if (violation_status & (1 << 24))
		pr_err("%s: realign buf cr violation\n", __func__);
}

static void msm_vfe32_process_error_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	uint32_t camif_status;
	if (!(irq_status1 & 0x7FFFFF))
		return;

	if (irq_status1 & (1 << 0)) {
		camif_status = msm_camera_io_r(vfe_dev->vfe_base + 0x204);
		pr_err("%s: camif error status: 0x%x\n",
			__func__, camif_status);
	}
	if (irq_status1 & (1 << 1))
		pr_err("%s: stats bhist overwrite\n", __func__);
	if (irq_status1 & (1 << 2))
		pr_err("%s: stats cs overwrite\n", __func__);
	if (irq_status1 & (1 << 3))
		pr_err("%s: stats ihist overwrite\n", __func__);
	if (irq_status1 & (1 << 4))
		pr_err("%s: realign buf y overflow\n", __func__);
	if (irq_status1 & (1 << 5))
		pr_err("%s: realign buf cb overflow\n", __func__);
	if (irq_status1 & (1 << 6))
		pr_err("%s: realign buf cr overflow\n", __func__);
	if (irq_status1 & (1 << 7)) {
		pr_err("%s: violation\n", __func__);
		msm_vfe32_process_violation_irq(vfe_dev);
	}
	if (irq_status1 & (1 << 8))
		pr_err("%s: image master 0 bus overflow\n", __func__);
	if (irq_status1 & (1 << 9))
		pr_err("%s: image master 1 bus overflow\n", __func__);
	if (irq_status1 & (1 << 10))
		pr_err("%s: image master 2 bus overflow\n", __func__);
	if (irq_status1 & (1 << 11))
		pr_err("%s: image master 3 bus overflow\n", __func__);
	if (irq_status1 & (1 << 12))
		pr_err("%s: image master 4 bus overflow\n", __func__);
	if (irq_status1 & (1 << 13))
		pr_err("%s: image master 5 bus overflow\n", __func__);
	if (irq_status1 & (1 << 14))
		pr_err("%s: image master 6 bus overflow\n", __func__);
	if (irq_status1 & (1 << 15))
		pr_err("%s: status ae/bg bus overflow\n", __func__);
	if (irq_status1 & (1 << 16))
		pr_err("%s: status af/bf bus overflow\n", __func__);
	if (irq_status1 & (1 << 17))
		pr_err("%s: status awb bus overflow\n", __func__);
	if (irq_status1 & (1 << 18))
		pr_err("%s: status rs bus overflow\n", __func__);
	if (irq_status1 & (1 << 19))
		pr_err("%s: status cs bus overflow\n", __func__);
	if (irq_status1 & (1 << 20))
		pr_err("%s: status ihist bus overflow\n", __func__);
	if (irq_status1 & (1 << 21))
		pr_err("%s: status skin bhist bus overflow\n", __func__);
	if (irq_status1 & (1 << 22))
		pr_err("%s: axi error\n", __func__);
}

static void msm_vfe32_read_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x2C);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x30);
	msm_camera_io_w(*irq_status0, vfe_dev->vfe_base + 0x24);
	msm_camera_io_w(*irq_status1, vfe_dev->vfe_base + 0x28);
	msm_camera_io_w_mb(1, vfe_dev->vfe_base + 0x18);
}

static void msm_vfe32_process_reg_update(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (!(irq_status0 & 0x20) && !(irq_status1 & 0x1C000000))
		return;

	if (vfe_dev->axi_data.stream_update)
		msm_isp_axi_stream_update(vfe_dev);

	msm_isp_update_framedrop_reg(vfe_dev);

	return;
}

static void msm_vfe32_reg_update(
	struct vfe_device *vfe_dev, uint32_t update_mask)
{
	msm_camera_io_w_mb(update_mask, vfe_dev->vfe_base + 0x260);
}

static long msm_vfe32_reset_hardware(struct vfe_device *vfe_dev)
{
	init_completion(&vfe_dev->reset_complete);
	msm_camera_io_w_mb(0x3FF, vfe_dev->vfe_base + 0x4);
	return wait_for_completion_interruptible_timeout(
	   &vfe_dev->reset_complete, msecs_to_jiffies(50));
}

static void msm_vfe32_axi_reload_wm(
	struct vfe_device *vfe_dev, uint32_t reload_mask)
{
	msm_camera_io_w_mb(reload_mask, vfe_dev->vfe_base + 0x38);
}

static void msm_vfe32_axi_enable_wm(struct vfe_device *vfe_dev,
	uint8_t wm_idx, uint8_t enable)
{
	if (enable)
		msm_camera_io_w_mb(0x1,
			vfe_dev->vfe_base + VFE32_WM_BASE(wm_idx));
	else
		msm_camera_io_w_mb(0x0,
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
	irq_mask |= 1 << (comp_mask_index + 21);
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
	irq_mask &= ~(1 << (comp_mask_index + 21));
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_axi_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask |= 1 << (stream_info->wm[0] + 6);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_axi_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask &= ~(1 << (stream_info->wm[0] + 6));
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe32_cfg_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t framedrop_pattern = 0;

	if (stream_info->init_frame_drop == 0)
		framedrop_pattern = stream_info->framedrop_pattern;

	if (stream_info->stream_type == BURST_STREAM &&
		stream_info->burst_frame_count == 0)
		framedrop_pattern = 0;

	if (stream_info->stream_src == PIX_ENCODER) {
		msm_camera_io_w(0x1F, vfe_dev->vfe_base + 0x504);
		msm_camera_io_w(0x1F, vfe_dev->vfe_base + 0x508);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x50C);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x510);
	} else if (stream_info->stream_src == PIX_VIEWFINDER) {
		msm_camera_io_w(0x1F, vfe_dev->vfe_base + 0x514);
		msm_camera_io_w(0x1F, vfe_dev->vfe_base + 0x518);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x51C);
		msm_camera_io_w(framedrop_pattern, vfe_dev->vfe_base + 0x520);
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

static void msm_vfe32_cfg_camif(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
	uint16_t first_pixel, last_pixel, first_line, last_line;
	struct msm_vfe_camif_cfg *camif_cfg = &pix_cfg->camif_cfg;
	uint32_t val;

	first_pixel = camif_cfg->left_crop;
	last_pixel = camif_cfg->pixels_per_line -
					 camif_cfg->left_crop -
					 camif_cfg->right_crop;
	first_line = camif_cfg->left_crop;
	last_line = camif_cfg->lines_per_frame -
					 camif_cfg->top_crop -
					 camif_cfg->bottom_crop;

	msm_camera_io_w(pix_cfg->input_mux << 16 | pix_cfg->pixel_pattern,
					vfe_dev->vfe_base + 0x14);

	msm_camera_io_w(camif_cfg->lines_per_frame << 16 |
					camif_cfg->pixels_per_line,
					vfe_dev->vfe_base + 0x1EC);

	msm_camera_io_w(ISP_SUB(first_pixel) << 16 | ISP_SUB(last_pixel),
					vfe_dev->vfe_base + 0x1F0);

	msm_camera_io_w(ISP_SUB(first_line) << 16 | ISP_SUB(last_line),
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
	}
}

static void msm_vfe32_axi_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd,
	uint8_t plane_idx)
{
	uint32_t val;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info =
		&axi_data->stream_info[
			(stream_cfg_cmd->axi_stream_handle & 0xFF)];
	uint32_t wm_base = VFE32_WM_BASE(stream_info->wm[plane_idx]);

	/*WR_IMAGE_SIZE*/
	val =
		((msm_isp_cal_word_per_line(stream_cfg_cmd->output_format,
		stream_cfg_cmd->plane_cfg[
			plane_idx].output_width)+1)/2 - 1) << 16 |
		(stream_cfg_cmd->plane_cfg[plane_idx].output_height - 1);
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x10);

	/*WR_BUFFER_CFG*/
	val =
		msm_isp_cal_word_per_line(stream_cfg_cmd->output_format,
		  stream_cfg_cmd->plane_cfg[plane_idx].output_stride) << 16 |
		(stream_cfg_cmd->plane_cfg[
			plane_idx].output_scan_lines - 1) << 4 |
		VFE32_BURST_LEN >> 2;
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	return;
}

static void msm_vfe32_axi_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint32_t val = 0;
	uint32_t wm_base = VFE32_WM_BASE(stream_info->wm[plane_idx]);
	/*WR_IMAGE_SIZE*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x10);
	/*WR_BUFFER_CFG*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	return;
}

static void msm_vfe32_axi_cfg_rdi_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd,
	uint8_t plane_idx)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info =
	 &axi_data->stream_info[(stream_cfg_cmd->axi_stream_handle & 0xFF)];
	struct msm_vfe_axi_plane_cfg *plane_cfg =
		&stream_cfg_cmd->plane_cfg[plane_idx];
	uint8_t rdi = stream_info->rdi[plane_idx];
	uint8_t rdi_master = stream_info->rdi_master[plane_idx];
	uint32_t rdi_reg_cfg;

	rdi_reg_cfg = msm_camera_io_r(vfe_dev->vfe_base + VFE32_RDI_BASE(rdi));
	rdi_reg_cfg = (rdi_reg_cfg & 0xFFFFFFF) | rdi_master << 28;
	msm_camera_io_w(rdi_reg_cfg, vfe_dev->vfe_base + VFE32_RDI_BASE(rdi));

	rdi_reg_cfg = msm_camera_io_r(
		vfe_dev->vfe_base + VFE32_RDI_MN_BASE(rdi_master));
	rdi_reg_cfg &= ~((0xF << VFE32_RDI_MN_SEL_SHIFT(rdi_master)) |
		(0x1 << VFE32_RDI_MN_FB_SHIFT(rdi_master)));
	rdi_reg_cfg |= (plane_cfg->rdi_cid <<
		VFE32_RDI_MN_SEL_SHIFT(rdi_master) |
		(stream_cfg_cmd->frame_base <<
			VFE32_RDI_MN_FB_SHIFT(rdi_master)));
	msm_camera_io_w(rdi_reg_cfg,
					vfe_dev->vfe_base +
					VFE32_RDI_MN_BASE(rdi_master));
}

static void msm_vfe32_axi_cfg_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd,
	uint8_t plane_idx)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info =
	 &axi_data->stream_info[(stream_cfg_cmd->axi_stream_handle & 0xFF)];
	struct msm_vfe_axi_plane_cfg *plane_cfg =
		&stream_cfg_cmd->plane_cfg[plane_idx];
	uint8_t wm = stream_info->wm[plane_idx];
	uint32_t xbar_cfg = 0;
	uint32_t xbar_reg_cfg = 0;

	switch (stream_cfg_cmd->stream_src) {
	case PIX_ENCODER:
	case PIX_VIEWFINDER: {
		if (plane_cfg->output_plane_format != CRCB_PLANE &&
			plane_cfg->output_plane_format != CBCR_PLANE) {
			/*SINGLE_STREAM_SEL*/
			xbar_cfg |= plane_cfg->output_plane_format << 5;
		} else {
			switch (stream_cfg_cmd->output_format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_NV16:
				xbar_cfg |= 0x3 << 3; /*PAIR_STREAM_SWAP_CTRL*/
				break;
			}
			xbar_cfg |= 0x1 << 1; /*PAIR_STREAM_EN*/
		}
		if (stream_cfg_cmd->stream_src == PIX_VIEWFINDER)
			xbar_cfg |= 0x1; /*VIEW_STREAM_EN*/
		break;
	}
	case CAMIF_RAW:
		xbar_cfg = 0x60;
		break;
	case IDEAL_RAW:
		xbar_cfg = 0x80;
		break;
	case RDI:
		if (stream_info->rdi[plane_idx] == 0)
			xbar_cfg = 0xA0;
		else if (stream_info->rdi[plane_idx] == 1)
			xbar_cfg = 0xC0;
		else if (stream_info->rdi[plane_idx] == 2)
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

static void msm_vfe32_cfg_axi_ub(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t ub_offset = 0;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		msm_camera_io_w(ub_offset << 16 | (VFE32_EQUAL_SLICE_UB - 1),
			vfe_dev->vfe_base + VFE32_WM_BASE(i) + 0xC);
		ub_offset += VFE32_EQUAL_SLICE_UB;
	}
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
	halt_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x20);
	halt_mask |= (1 << 24);
	msm_camera_io_w_mb(halt_mask, vfe_dev->vfe_base + 0x20);
	init_completion(&vfe_dev->halt_complete);
	/*TD: Need to fix crashes with this*/
	/*msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x1D8);*/
	return wait_for_completion_interruptible_timeout(
		&vfe_dev->halt_complete, msecs_to_jiffies(500));
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

	vfe_dev->iommu_ctx[0] = msm_iommu_get_ctx("vfe_imgwr");
	if (!vfe_dev->iommu_ctx[0]) {
		pr_err("%s: no iommux ctx resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}

	vfe_dev->iommu_ctx[1] = msm_iommu_get_ctx("vfe_misc");
	if (!vfe_dev->iommu_ctx[1]) {
		pr_err("%s: no iommux ctx resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}
	vfe_dev->num_ctx = 2;

vfe_no_resource:
	return rc;
}

struct msm_vfe_axi_hardware_info msm_vfe32_axi_hw_info = {
	.num_wm = 7,
	.num_comp_mask = 3,
	.num_rdi = 3,
	.num_rdi_master = 3,
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
	.vfe_ops = {
		.irq_ops = {
			.read_irq_status = msm_vfe32_read_irq_status,
			.process_camif_irq = msm_vfe32_process_camif_irq,
			.process_reset_irq = msm_vfe32_process_reset_irq,
			.process_halt_irq = msm_vfe32_process_halt_irq,
			.process_reset_irq = msm_vfe32_process_reset_irq,
			.process_error_irq = msm_vfe32_process_error_irq,
			.process_reg_update = msm_vfe32_process_reg_update,
			.process_axi_irq = msm_isp_process_axi_irq,
		},
		.axi_ops = {
			.reload_wm = msm_vfe32_axi_reload_wm,
			.enable_wm = msm_vfe32_axi_enable_wm,
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
			.cfg_rdi_reg = msm_vfe32_axi_cfg_rdi_reg,
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
			.reset_hw = msm_vfe32_reset_hardware,
			.init_hw = msm_vfe32_init_hardware,
			.init_hw_reg = msm_vfe32_init_hardware_reg,
			.release_hw = msm_vfe32_release_hardware,
			.get_platform_data = msm_vfe32_get_platform_data,
		},
	},
	.axi_hw_info = &msm_vfe32_axi_hw_info,
	.subdev_ops = &msm_vfe32_subdev_ops,
	.subdev_internal_ops = &msm_vfe32_internal_ops,
};
EXPORT_SYMBOL(vfe32_hw_info);
