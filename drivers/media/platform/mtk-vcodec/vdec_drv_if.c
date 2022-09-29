// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vdec_drv_if.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_base.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
#include "mtk_vcu.h"
const struct vdec_common_if *get_dec_vcu_if(void);
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
const struct vdec_common_if *get_dec_vcp_if(void);
#endif


static const struct vdec_common_if *get_data_path_ptr(void)
{
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (VCU_FPTR(vcu_get_plat_device)) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		if (mtk_vcodec_vcp & (1 << MTK_INST_DECODER))
			return get_dec_vcp_if();
#endif
		return get_dec_vcu_if();
	}
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	return get_dec_vcp_if();
#else
	return NULL;
#endif
}

int vdec_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;
	mtk_dec_init_ctx_pm(ctx);

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
	case V4L2_PIX_FMT_HEIF:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_VP8:
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_WMV1:
	case V4L2_PIX_FMT_WMV2:
	case V4L2_PIX_FMT_WMV3:
	case V4L2_PIX_FMT_WVC1:
	case V4L2_PIX_FMT_WMVA:
	case V4L2_PIX_FMT_RV30:
	case V4L2_PIX_FMT_RV40:
	case V4L2_PIX_FMT_AV1:
		ctx->dec_if = get_data_path_ptr();
		break;
	default:
		return -EINVAL;
	}

	if (ctx->dec_if == NULL)
		return -EINVAL;

	ret = ctx->dec_if->init(ctx, &ctx->drv_handle);

	return ret;
}

int vdec_if_decode(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
				   struct vdec_fb *fb, unsigned int *src_chg)
{
	int ret = 0;
	unsigned int i = 0;

	if (bs && !ctx->dec_params.svp_mode) {
		if ((bs->dma_addr & 63UL) != 0UL) {
			mtk_v4l2_err("bs dma_addr should 64 byte align");
			return -EINVAL;
		}
	}

	if (fb && !ctx->dec_params.svp_mode) {
		for (i = 0; i < fb->num_planes; i++) {
			if ((fb->fb_base[i].dma_addr & 511UL) != 0UL) {
				mtk_v4l2_err("fb addr should 512 byte align");
				return -EINVAL;
			}
		}
	}

	if (ctx->drv_handle == 0)
		return -EIO;

	//vcodec_trace_begin
	ret = ctx->dec_if->decode(ctx->drv_handle, bs, fb, src_chg);
	//vcodec_trace_end();

	return ret;
}

int vdec_if_get_param(struct mtk_vcodec_ctx *ctx, enum vdec_get_param_type type,
					  void *out)
{
	struct vdec_inst *inst = NULL;
	int ret = 0;
	int drv_handle_exist = 1;

	if (!ctx->drv_handle) {
		inst = kzalloc(sizeof(struct vdec_inst), GFP_KERNEL);
		if (inst == NULL)
			return -ENOMEM;
		inst->ctx = ctx;
		inst->vcu.ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = 0;
	}

	if (ctx->dec_if != NULL)
		ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);
	else
		ret = -EINVAL;

	if (!drv_handle_exist) {
		inst->vcu.abort = 1;
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->dec_if = NULL;
	}

	return ret;
}

int vdec_if_set_param(struct mtk_vcodec_ctx *ctx, enum vdec_set_param_type type,
					  void *in)
{
	struct vdec_inst *inst = NULL;
	int ret = 0;
	int drv_handle_exist = 1;

	if (!ctx->drv_handle) {
		inst = kzalloc(sizeof(struct vdec_inst), GFP_KERNEL);
		if (inst == NULL)
			return -ENOMEM;
		inst->ctx = ctx;
		inst->vcu.ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = 0;
	}

	if (ctx->dec_if != NULL)
		ret = ctx->dec_if->set_param(ctx->drv_handle, type, in);
	else
		ret = -EINVAL;

	if (!drv_handle_exist) {
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->dec_if = NULL;
	}

	return ret;
}

void vdec_if_deinit(struct mtk_vcodec_ctx *ctx)
{
	if (ctx->drv_handle == 0)
		return;

	ctx->dec_if->deinit(ctx->drv_handle);

	ctx->drv_handle = 0;
}

int vdec_if_flush(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
				   struct vdec_fb *fb, enum vdec_flush_type type)
{
	if (ctx->drv_handle == 0)
		return -EIO;

	if (ctx->dec_if->flush == NULL) {
		unsigned int src_chg;

		return vdec_if_decode(ctx, bs, fb, &src_chg);
	}

	return ctx->dec_if->flush(ctx->drv_handle, fb, type);
}

void vdec_decode_prepare(void *ctx_prepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_prepare;
	int ret;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	ret = mtk_vdec_lock(ctx, hw_id);
	mtk_vcodec_set_curr_ctx(ctx->dev, ctx, hw_id);
	if (ctx->dev->dec_always_on[hw_id] == 0)
		mtk_vcodec_dec_clock_on(&ctx->dev->pm, hw_id);
	if (ctx->power_type[hw_id] == VDEC_POWER_NORMAL) {
		if (ctx->dec_params.operating_rate >= MTK_VDEC_ALWAYS_ON_OP_RATE)
			ctx->power_type[hw_id] = VDEC_POWER_ALWAYS_OP;
		if (mtk_vdec_is_highest_freq(ctx))
			ctx->power_type[hw_id] = VDEC_POWER_ALWAYS_FREQ;
		if (ctx->power_type[hw_id] >= VDEC_POWER_ALWAYS) {
			ctx->dev->dec_always_on[hw_id]++;
			mtk_v4l2_debug(0, "[%d] hw_id %d power type %d always on %d", ctx->id,
				hw_id, ctx->power_type[hw_id], ctx->dev->dec_always_on[hw_id]);
		}
	}

	if (ret == 0 && !(mtk_vcodec_vcp & (1 << MTK_INST_DECODER)) &&
	    ctx->power_type[hw_id] != VDEC_POWER_RELEASE)
		enable_irq(ctx->dev->dec_irq[hw_id]);
	mtk_vdec_dvfs_begin_frame(ctx, hw_id);
	mtk_vdec_pmqos_begin_frame(ctx);
	if (hw_id == MTK_VDEC_CORE)
		vcodec_trace_count("VDEC_HW_CORE", 1);
	else
		vcodec_trace_count("VDEC_HW_LAT", 1);
	mutex_unlock(&ctx->hw_status);
}

void vdec_decode_unprepare(void *ctx_unprepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unprepare;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	if (ctx->dev->vdec_reg) // per frame mmdvfs in AP
		mtk_vdec_dvfs_end_frame(ctx, hw_id);
	mtk_vdec_pmqos_end_frame(ctx);
	if (ctx->dev->dec_sem[hw_id].count != 0) {
		mtk_v4l2_debug(0, "HW not prepared, dec_sem[%d].count = %d",
			hw_id, ctx->dev->dec_sem[hw_id].count);
		mutex_unlock(&ctx->hw_status);
		return;
	}
	if (hw_id == MTK_VDEC_CORE)
		vcodec_trace_count("VDEC_HW_CORE", 0);
	else
		vcodec_trace_count("VDEC_HW_LAT", 0);

	if (!(mtk_vcodec_vcp & (1 << MTK_INST_DECODER)) &&
	    ctx->power_type[hw_id] != VDEC_POWER_RELEASE)
		disable_irq(ctx->dev->dec_irq[hw_id]);
	if (ctx->power_type[hw_id] == VDEC_POWER_RELEASE ||
	   (ctx->power_type[hw_id] == VDEC_POWER_ALWAYS_FREQ && !mtk_vdec_is_highest_freq(ctx))) {
		mtk_v4l2_debug(0, "[%d] hw_id %d power type %d off always on %d", ctx->id,
			hw_id, ctx->power_type[hw_id], ctx->dev->dec_always_on[hw_id]);
		ctx->dev->dec_always_on[hw_id]--;
		ctx->power_type[hw_id] = VDEC_POWER_NORMAL;
	}
	if (ctx->dev->dec_always_on[hw_id] == 0)
		mtk_vcodec_dec_clock_off(&ctx->dev->pm, hw_id);
	mtk_vcodec_set_curr_ctx(ctx->dev, NULL, hw_id);
	mtk_vdec_unlock(ctx, hw_id);
	mutex_unlock(&ctx->hw_status);

}

void vdec_check_release_lock(void *ctx_check)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_check;
	int i;
	bool is_always_on;

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		is_always_on = false;
		if (ctx->power_type[i] >= VDEC_POWER_ALWAYS) {
			is_always_on = true;
			ctx->power_type[i] = VDEC_POWER_RELEASE;
			if (ctx->hw_locked[i] == 0)
				vdec_decode_prepare(ctx, i); // for mtk_vdec_lock
		}
		if (ctx->hw_locked[i] == 1) {
			vdec_decode_unprepare(ctx, i);
			ctx->power_type[i] = VDEC_POWER_NORMAL;
			if (is_always_on)
				mtk_v4l2_debug(2, "[%d] always power on inst clk off hw_id %d",
					ctx->id, i);
			else
				mtk_v4l2_err("[%d] daemon killed when holding lock %d",
					ctx->id, i);
		}
	}
}

void vdec_suspend_power(void *ctx_check)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_check;
	int hw_id;

	for (hw_id = 0; hw_id < MTK_VDEC_HW_NUM; hw_id++) {
		if (ctx->dev->dec_always_on[hw_id] > 0) {
			mtk_vcodec_dec_clock_off(&ctx->dev->pm, hw_id);
			mtk_v4l2_debug(0, "hw_id %d clock off for is always on %d",
				hw_id, ctx->dev->dec_always_on[hw_id]);
		}
	}
}

void vdec_resume_power(void *ctx_check)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_check;
	int hw_id;

	for (hw_id = 0; hw_id < MTK_VDEC_HW_NUM; hw_id++) {
		if (ctx->dev->dec_always_on[hw_id] > 0) {
			mtk_vcodec_dec_clock_on(&ctx->dev->pm, hw_id);
			mtk_v4l2_debug(0, "hw_id %d clock on for is always on %d",
				hw_id, ctx->dev->dec_always_on[hw_id]);
		}
	}
}

