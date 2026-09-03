// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>
#include <linux/types.h>
#include <drm/gsp_r9p0_cfg.h>
#include "../gsp_core.h"
#include "../gsp_kcfg.h"
#include "../gsp_debug.h"
#include "../gsp_dev.h"
#include "../gsp_workqueue.h"
#include "../gsp_layer.h"
#include "gsp_r9p0_core.h"
#include "gsp_r9p0_reg.h"
#include "gsp_coef_cal.h"
#include "../gsp_interface.h"
#include "gsp_hdr_param.h"

#define CORE_STS_NO_CHG 0
#define CORE_FROM_2_TO_1 1
#define CORE_FROM_1_TO_2 2

static int zorder_used[R9P0_IMGL_NUM + R9P0_OSDL_NUM] = {0};
int gsp_r9p0_layer_num;

static void print_image_layer_cfg(struct gsp_r9p0_img_layer *layer)
{
	struct gsp_r9p0_img_layer_params *params = NULL;

	if (layer->common.enable == 0)
		return;

	params = &layer->params;
	gsp_layer_common_print(&layer->common);

	GSP_DEBUG("zoder[%d]\n", params->zorder);
	GSP_DEBUG("img_format[%d,%d], scaling_en[%d], pitch[%d, %d]\n",
		params->img_format, params->fbcd_mod, params->scaling_en,
		params->pitch, params->height);

	GSP_DEBUG("clip_rect[%d, %d, %d, %d], des_rect[%d, %d, %d, %d]\n",
		params->clip_rect.st_x, params->clip_rect.st_y,
		params->clip_rect.rect_w, params->clip_rect.rect_h,
		params->des_rect.st_x, params->des_rect.st_y,
		params->des_rect.rect_w, params->des_rect.rect_h);

	GSP_DEBUG("grey[%d, %d, %d, %d], colorkey[%d, %d, %d, %d]\n",
		params->grey.r_val, params->grey.g_val,
		params->grey.b_val, params->grey.a_val,
		params->colorkey.r_val, params->colorkey.g_val,
		params->colorkey.b_val, params->colorkey.a_val);

	GSP_DEBUG("endian[%d, %d, %d, %d], alpha[%d], colorkey_en[%d]\n",
		params->endian.y_rgb_word_endn, params->endian.uv_word_endn,
		params->endian.rgb_swap_mode, params->endian.a_swap_mode,
		params->alpha, params->colorkey_en);
	GSP_DEBUG("pallet_en[%d], pmargb_en[%d], pmargb_mod[%d]\n",
		params->pallet_en, params->pmargb_en, params->pmargb_mod);
}

static void print_osd_layer_cfg(struct gsp_r9p0_osd_layer *layer)
{
	struct gsp_r9p0_osd_layer_params *params = NULL;

	if (layer->common.enable == 0)
		return;

	params = &layer->params;
	gsp_layer_common_print(&layer->common);

	GSP_DEBUG("osd_format[%d,%d], zorder[%d], pitch[%d, %d]\n",
		params->osd_format, params->fbcd_mod, params->zorder,
		params->pitch, params->height);

	GSP_DEBUG("clip_rect[%d, %d, %d, %d], des_pos[%d, %d]\n",
		params->clip_rect.st_x, params->clip_rect.st_y,
		params->clip_rect.rect_w, params->clip_rect.rect_h,
		params->des_pos.pt_x, params->des_pos.pt_y);

	GSP_DEBUG("grey[%d, %d, %d, %d], colorkey[%d, %d, %d, %d]\n",
		params->grey.r_val, params->grey.g_val,
		params->grey.b_val, params->grey.a_val,
		params->colorkey.r_val, params->colorkey.g_val,
		params->colorkey.b_val, params->colorkey.a_val);

	GSP_DEBUG("endian[%d, %d, %d, %d], alpha[%d], colorkey_en[%d]\n",
		params->endian.y_rgb_word_endn, params->endian.uv_word_endn,
		params->endian.rgb_swap_mode, params->endian.a_swap_mode,
		params->alpha, params->colorkey_en);

	GSP_DEBUG("pallet_en[%d], pmargb_en[%d], pmargb_mod[%d]\n",
		params->pallet_en, params->pmargb_en, params->pmargb_mod);
}

static void print_des_layer_cfg(struct gsp_r9p0_des_layer *layer)
{
	struct gsp_r9p0_des_layer_params *params = NULL;

	params = &layer->params;
	gsp_layer_common_print(&layer->common);

	GSP_DEBUG("des_format[%d, %d], pitch[%d, %d]\n",
		params->img_format, params->fbc_mod,
		params->pitch, params->height);

	GSP_DEBUG("endian: [%d, %d, %d, %d]\n",
		params->endian.y_rgb_word_endn, params->endian.uv_word_endn,
		params->endian.rgb_swap_mode, params->endian.a_swap_mode);
}

static void gsp_r9p0_core_cfg_print(struct gsp_r9p0_cfg *cfg)
{
	int icnt = 0;

	for (icnt = 0; icnt < 2; icnt++)
		print_image_layer_cfg(&cfg->limg[icnt]);
	for (icnt = 0; icnt < 2; icnt++)
		print_osd_layer_cfg(&cfg->losd[icnt]);

	print_des_layer_cfg(&cfg->ld1);
}

void gsp_r9p0_core_dump(struct gsp_core *c)
{
	int icnt = 0;
	struct R9P0_GSP_CTL_REG_T reg_struct;

	memset(&reg_struct, 0, sizeof(struct R9P0_GSP_CTL_REG_T));

	reg_struct.glb_cfg.value = gsp_core_reg_read(R9P0_GSP_GLB_CFG(c->base));
	reg_struct.mod_cfg.value = gsp_core_reg_read(R9P0_GSP_MOD_CFG(c->base));
	reg_struct.secure_cfg.value =
		gsp_core_reg_read(R9P0_GSP_SECURE_CFG(c->base));

	reg_struct.des_data_cfg.value =
		gsp_core_reg_read(R9P0_DES_DATA_CFG(c->base));
	reg_struct.des_y_addr.value =
		gsp_core_reg_read(R9P0_DES_Y_ADDR(c->base));
	reg_struct.des_u_addr.value =
		gsp_core_reg_read(R9P0_DES_U_ADDR(c->base));
	reg_struct.des_v_addr.value =
		gsp_core_reg_read(R9P0_DES_V_ADDR(c->base));
	reg_struct.des_pitch.value =
		gsp_core_reg_read(R9P0_DES_PITCH(c->base));
	reg_struct.back_rgb.value =
		gsp_core_reg_read(R9P0_BACK_RGB(c->base));
	reg_struct.work_area_size.value =
		gsp_core_reg_read(R9P0_WORK_AREA_SIZE(c->base));
	reg_struct.work_area_xy.value =
		gsp_core_reg_read(R9P0_WORK_AREA_XY(c->base));

	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++) {
		reg_struct.limg_cfg[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_CFG(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_y_addr[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_Y_ADDR(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_u_addr[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_U_ADDR(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_v_addr[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_V_ADDR(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_pitch[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_PITCH(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_clip_start[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_CLIP_START(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_clip_size[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_CLIP_SIZE(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_des_start[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_DES_START(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_pallet_rgb[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_PALLET_RGB(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_ck[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_CK(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.y2y_y_param[icnt].value =
			gsp_core_reg_read(R9P0_Y2Y_Y_PARAM(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.y2y_u_param[icnt].value =
			gsp_core_reg_read(R9P0_Y2Y_U_PARAM(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.y2y_v_param[icnt].value =
			gsp_core_reg_read(R9P0_Y2Y_V_PARAM(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
		reg_struct.limg_des_scl_size[icnt].value =
			gsp_core_reg_read(R9P0_LIMG_DES_SIZE(
				(c->base + icnt * R9P0_LIMG_OFFSET)));
	}

	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++) {
		reg_struct.losd_cfg[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_CFG(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_r_addr[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_R_ADDR(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_pitch[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_PITCH(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_clip_start[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_CLIP_START(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_clip_size[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_CLIP_SIZE(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_des_start[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_DES_START(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_pallet_rgb[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_PALLET_RGB(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
		reg_struct.losd_ck[icnt].value =
			gsp_core_reg_read(R9P0_LOSD_CK(
				(c->base + icnt * R9P0_LOSD_OFFSET)));
	}

	reg_struct.ip_rev.value =
		gsp_core_reg_read(R9P0_GSP_IP_REV(c->base));
	reg_struct.debug_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG_CFG(c->base));
	reg_struct.debug1_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG1(c->base));
	reg_struct.debug2_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG2(c->base));
	reg_struct.debug3_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG3(c->base));
	reg_struct.debug4_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG4(c->base));
		reg_struct.debug5_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG5(c->base));
	reg_struct.debug6_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG6(c->base));
	reg_struct.debug7_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG7(c->base));
	reg_struct.debug8_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG8(c->base));
	reg_struct.debug9_cfg.value =
		gsp_core_reg_read(R9P0_GSP_DEBUG9(c->base));


	/* core's ctl reg parsed */
	GSP_DUMP("GSP_BUSY[0x%x], ERR_CODE[0x%x]\n",
	reg_struct.glb_cfg.GSP_BUSY0, reg_struct.glb_cfg.ERR_CODE);

	GSP_DUMP("ERR_FLAG[0x%x], GSP_RUN[0x%x], GSP_SECURE_CFG[0x%x]\n",
		reg_struct.glb_cfg.ERR_FLG, reg_struct.glb_cfg.GSP_RUN0,
		reg_struct.secure_cfg.value);

	GSP_DUMP("GSP_DEBUG_CFG[0x%x]\n", reg_struct.debug_cfg.value);

	GSP_DUMP("GSP_DEBUG1[0x%x], GSP_DEBUG2[0x%x]\n",
		reg_struct.debug1_cfg.value, reg_struct.debug2_cfg.value);
	GSP_DUMP("GSP_DEBUG1[0x%x], GSP_DEBUG2[0x%x]\n",
		reg_struct.debug3_cfg.value, reg_struct.debug4_cfg.value);
	GSP_DUMP("GSP_DEBUG1[0x%x], GSP_DEBUG2[0x%x]\n",
		reg_struct.debug5_cfg.value, reg_struct.debug6_cfg.value);
	GSP_DUMP("GSP_DEBUG1[0x%x], GSP_DEBUG2[0x%x]\n",
		reg_struct.debug7_cfg.value, reg_struct.debug8_cfg.value);
	GSP_DUMP("GSP_DEBUG9[0x%x]\n", reg_struct.debug9_cfg.value);

		/* des layer cfg */
	GSP_DUMP("bk_bld[%d], bk_en[%d], dither_en[%d]\n",
		reg_struct.des_data_cfg.BK_BLD,
		reg_struct.des_data_cfg.BK_EN,
		reg_struct.des_data_cfg.DITHER_EN);

	GSP_DUMP("rswap_mod[%d], rot_mod[%d], des_format[%d, %d]\n",
		reg_struct.des_data_cfg.RSWAP_MOD,
		reg_struct.des_data_cfg.ROT_MOD,
		reg_struct.des_data_cfg.DES_IMG_FORMAT,
		reg_struct.des_data_cfg.FBCE_MOD);
	GSP_DUMP("pitch[%d, %d], work_area1[%d, %d, %d, %d]\n",
		reg_struct.des_pitch.DES_PITCH,
		reg_struct.des_pitch.DES_HEIGHT,
		reg_struct.work_area_xy.WORK_AREA_X,
		reg_struct.work_area_xy.WORK_AREA_Y,
		reg_struct.work_area_size.WORK_AREA_W,
		reg_struct.work_area_size.WORK_AREA_H);

	GSP_DUMP("endian[%d, %d, %d], r2y_mod[%d]\n",
		reg_struct.des_data_cfg.Y_ENDIAN_MOD,
		reg_struct.des_data_cfg.UV_ENDIAN_MOD,
		reg_struct.des_data_cfg.RSWAP_MOD,
		reg_struct.des_data_cfg.R2Y_MOD);

	GSP_DUMP("pmargb_en[%d], BACK_RGB[0x%x]\n",
		reg_struct.mod_cfg.PMARGB_EN,
		reg_struct.back_rgb.value);
	GSP_DUMP("Dst layer y addr[0x%x], v addr: [0x%x]\n",
		reg_struct.des_y_addr.DES_Y_BASE_ADDR1,
		reg_struct.des_u_addr.DES_U_BASE_ADDR1);

	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++) {
		if (reg_struct.limg_cfg[icnt].Limg_en) {
			GSP_DUMP("img_format[%d, %d], pitch[%d, %d]\n",
			reg_struct.limg_cfg[icnt].IMG_FORMAT,
			reg_struct.limg_cfg[icnt].FBCD_MOD,
			reg_struct.limg_pitch[icnt].PITCH,
			reg_struct.limg_pitch[icnt].HEIGHT);

			GSP_DUMP("IMG Y addr:[0x%x], u addr; 0x[%x]\n",
			reg_struct.limg_y_addr[icnt].Y_BASE_ADDR,
			reg_struct.limg_u_addr[icnt].U_BASE_ADDR);

			GSP_DUMP("clip_rect[%d, %d, %d, %d]\n",
			reg_struct.limg_clip_start[icnt].CLIP_START_X,
			reg_struct.limg_clip_start[icnt].CLIP_START_Y,
			reg_struct.limg_clip_size[icnt].CLIP_SIZE_X,
			reg_struct.limg_clip_size[icnt].CLIP_SIZE_Y);

			GSP_DUMP("ZNUM_L[%d], des_rect[%d, %d]\n",
			reg_struct.limg_cfg[icnt].ZNUM_L,
			reg_struct.limg_des_start[icnt].DES_START_X,
			reg_struct.limg_des_start[icnt].DES_START_Y);

			GSP_DUMP("pmargb_mod[%d], pallet[%d, 0x%x]\n",
				reg_struct.limg_cfg[icnt].PMARGB_MOD,
				reg_struct.limg_cfg[icnt].PALLET_EN,
				reg_struct.limg_pallet_rgb[icnt].value);

			GSP_DUMP("colorkey[%d, 0x%x], Y2R_MOD[0x%x]\n",
				reg_struct.limg_cfg[icnt].CK_EN,
				reg_struct.limg_ck[icnt].value,
				reg_struct.limg_cfg[icnt].Y2R_MOD);

			GSP_DUMP("endian[%d, %d, %d, %d]\n",
				reg_struct.limg_cfg[icnt].Y_ENDIAN_MOD,
				reg_struct.limg_cfg[icnt].UV_ENDIAN_MOD,
				reg_struct.limg_cfg[icnt].RGB_SWAP_MOD,
				reg_struct.limg_cfg[icnt].A_SWAP_MOD);

			GSP_DUMP("Y2Y[%d, 0x%x, 0x%x, 0x%x]\n",
				reg_struct.limg_cfg[icnt].Y2Y_MOD,
				reg_struct.y2y_y_param[icnt].value,
				reg_struct.y2y_u_param[icnt].value,
				reg_struct.y2y_v_param[icnt].value);

			GSP_DUMP("des_scl[%d, %d],TAP_MOD[%d, %d]\n",
				reg_struct.limg_des_scl_size[icnt].DES_SCL_W,
				reg_struct.limg_des_scl_size[icnt].DES_SCL_H,
				reg_struct.limg_des_scl_size[icnt].HTAP_MOD,
				reg_struct.limg_des_scl_size[icnt].VTAP_MOD);
		}
	}

	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++) {
		if (reg_struct.losd_cfg[icnt].Losd_en) {
			GSP_DUMP("img_format[%d, %d], pitch[%d, %d]\n",
			reg_struct.losd_cfg[icnt].IMG_FORMAT,
			reg_struct.losd_cfg[icnt].FBCD_MOD,
			reg_struct.losd_pitch[icnt].PITCH,
			reg_struct.losd_pitch[icnt].HEIGHT);

			GSP_DUMP("clip_rect[%d, %d, %d, %d]\n",
			reg_struct.losd_clip_start[icnt].CLIP_START_X,
			reg_struct.losd_clip_start[icnt].CLIP_START_Y,
			reg_struct.losd_clip_size[icnt].CLIP_SIZE_X,
			reg_struct.losd_clip_size[icnt].CLIP_SIZE_Y);

			GSP_DUMP("ZNUM_L[%d], des_rect[%d, %d]\n",
			reg_struct.losd_cfg[icnt].ZNUM_L,
			reg_struct.losd_des_start[icnt].DES_START_X,
			reg_struct.losd_des_start[icnt].DES_START_Y);

			GSP_DUMP("pmargb_mod[%d]\n",
				reg_struct.losd_cfg[icnt].PMARGB_MOD);

			GSP_DUMP("pallet[%d, 0x%x]\n",
				reg_struct.losd_cfg[icnt].PALLET_EN,
				reg_struct.losd_pallet_rgb[icnt].value);

			GSP_DUMP("colorkey[%d, 0x%x]\n",
				reg_struct.losd_cfg[icnt].CK_EN,
				reg_struct.losd_ck[icnt].value);

			GSP_DUMP("endian[%d, %d, %d]\n",
				reg_struct.losd_cfg[icnt].ENDIAN,
				reg_struct.losd_cfg[icnt].RGB_SWAP,
				reg_struct.losd_cfg[icnt].A_SWAP);
		}
	}
}

static void gsp_r9p0_core_cfg_reinit(struct gsp_r9p0_cfg *cfg)
{
	struct gsp_layer *layer;
	struct gsp_kcfg *kcfg = NULL;
	int icnt = 0;

	if (IS_ERR_OR_NULL(cfg)) {
		GSP_ERR("cfg init params error\n");
		return;
	}

	kcfg = cfg->common.kcfg;
	GSP_DEBUG("gsp_r9p0 cfg parent kcfg[%d]\n", gsp_kcfg_to_tag(kcfg));
	if (cfg->common.init != 1) {
		GSP_ERR("gsp_r9p0 cfg[%d] has not been initialized\n",
			cfg->common.tag);
		return;
	}

	/* first to reset layer common attributes */
	list_for_each_entry(layer, &cfg->common.layers, list) {
		layer->type = -1;
		layer->enable = -1;
		layer->wait_fd = -1;
		layer->sig_fd = -1;
		memset(&layer->src_addr, 0, sizeof(struct gsp_addr_data));
		memset(&layer->mem_data, 0, sizeof(struct gsp_mem_data));
	}

	/* second to reset layer params */
	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++)
		memset(&cfg->limg[icnt].params, 0,
			sizeof(struct gsp_r9p0_img_layer_params));
	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++)
		memset(&cfg->losd[icnt].params, 0,
			sizeof(struct gsp_r9p0_osd_layer_params));
	memset(&cfg->ld1.params, 0, sizeof(struct gsp_r9p0_des_layer_params));
}

static int gsp_r9p0_core_capa_init(struct gsp_core *core)
{
	struct gsp_r9p0_capability *capa = NULL;
	struct gsp_rect max = {0, 0, 8191, 8191};
	struct gsp_rect min = {0, 0, 4, 4};

	capa = (struct gsp_r9p0_capability *)core->capa;

	/* common information initialize */
	capa->common.magic = GSP_CAPABILITY_MAGIC;
	strcpy(capa->common.version, "R9P0");
	capa->common.capa_size = sizeof(struct gsp_r9p0_capability);

	capa->common.max_layer = R9P0_IMGL_NUM + R9P0_OSDL_NUM;
	capa->common.max_img_layer = R9P0_IMGL_NUM;

	capa->common.crop_max = max;
	capa->common.crop_min = min;
	capa->common.out_max = max;
	capa->common.out_min = min;

	capa->common.buf_type = GSP_ADDR_TYPE_IOVIRTUAL;

	/* private information initialize */
	capa->scale_range_up = 64;
	capa->scale_range_down = 1;
	capa->yuv_xywh_even = 0;
	capa->scale_updown_sametime = 1;
	capa->OSD_scaling = 0;
	capa->blend_video_with_OSD = 1;
	capa->max_yuvLayer_cnt = 1;
	capa->max_scaleLayer_cnt = 1;
	capa->seq0_scale_range_up = 64;
	capa->seq0_scale_range_down = 1;
	capa->seq1_scale_range_up = 64;
	capa->seq1_scale_range_down = 4;
	capa->src_yuv_xywh_even_limit = 1;
	capa->max_video_size = 2;
	capa->csc_matrix_in = 0x3;
	capa->csc_matrix_out = 0x3;

	capa->block_alpha_limit = 0;
	capa->max_throughput = 256;

	capa->max_gspmmu_size = 80 * 1024 * 1024;
	capa->max_gsp_bandwidth = 1920 * 1080 * 4 * 5 / 2;

	return 0;
}

static void gsp_r9p0_int_clear_and_disable(struct gsp_core *core)
{
	struct R9P0_GSP_INT_REG gsp_int_value;
	struct R9P0_GSP_INT_REG gsp_int_mask;

	if (core == NULL) {
		GSP_ERR("r9p0 interrupt clear with null core\n");
		return;
	}

	gsp_int_value.value = 0;
	gsp_int_value.INT_GSP_EN = 0;
	gsp_int_value.INT_GERR_EN = 0;
	gsp_int_value.INT_FBCDPL_EN = 0;
	gsp_int_value.INT_FBCDHD_EN = 0;
	gsp_int_value.INT_GSP_CLR = 1;
	gsp_int_value.INT_GERR_CLR = 1;
	gsp_int_value.INT_FBCDPL_CLR = 1;
	gsp_int_value.INT_FBCDHD_CLR = 1;
	gsp_int_mask.value = 0;
	gsp_int_mask.INT_GSP_EN = 1;
	gsp_int_mask.INT_GERR_EN = 1;
	gsp_int_mask.INT_FBCDPL_EN = 1;
	gsp_int_mask.INT_FBCDHD_EN = 1;
	gsp_int_mask.INT_GSP_CLR = 1;
	gsp_int_mask.INT_GERR_CLR = 1;
	gsp_int_mask.INT_FBCDPL_CLR = 1;
	gsp_int_mask.INT_FBCDHD_CLR = 1;
	gsp_core_reg_update(R9P0_GSP_INT(core->base),
			gsp_int_value.value, gsp_int_mask.value);

}

static void gsp_r9p0_int_clear(struct gsp_core *core)
{
	struct R9P0_GSP_INT_REG gsp_int_value;
	struct R9P0_GSP_INT_REG gsp_int_mask;

	if (IS_ERR_OR_NULL(core)) {
		GSP_ERR("r9p0 interrupt clear with null core\n");
		return;
	}

	gsp_int_value.value = 0;
	gsp_int_value.INT_GSP_CLR = 1;
	gsp_int_value.INT_GERR_CLR = 1;
	gsp_int_value.INT_FBCDPL_CLR = 1;
	gsp_int_value.INT_FBCDHD_CLR = 1;
	gsp_int_mask.value = 0;
	gsp_int_mask.INT_GSP_CLR = 1;
	gsp_int_mask.INT_GERR_CLR = 1;
	gsp_int_mask.INT_FBCDPL_CLR = 1;
	gsp_int_mask.INT_FBCDHD_CLR = 1;
	gsp_core_reg_update(R9P0_GSP_INT(core->base),
			gsp_int_value.value, gsp_int_mask.value);
}

static void gsp_r9p0_coef_cache_init(struct gsp_r9p0_core *core)
{
	uint32_t i = 0;

	if (core->cache_coef_init_flag == 0) {
		i = 0;
		INIT_LIST_HEAD(&core->coef_list);
		while (i < GSP_COEF_CACHE_MAX) {
			list_add_tail(&core->coef_cache[i].list,
				&core->coef_list);
			i++;
		}
		core->cache_coef_init_flag = 1;
	}
}

static void gsp_r9p0_core_cfg_init(struct gsp_r9p0_cfg *cfg,
					struct gsp_kcfg *kcfg)
{
	/* to work around ERROR: do not initialise statics to 0 or NULL */
	static int tag = 1;
	int icnt = 0;

	if (IS_ERR_OR_NULL(cfg)) {
		GSP_ERR("cfg init params error\n");
		return;
	}

	INIT_LIST_HEAD(&cfg->common.layers);
	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++)
		INIT_LIST_HEAD(&cfg->limg[icnt].common.list);
	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++)
		INIT_LIST_HEAD(&cfg->losd[icnt].common.list);
	INIT_LIST_HEAD(&cfg->ld1.common.list);

	cfg->common.layer_num = R9P0_IMGL_NUM + R9P0_OSDL_NUM;
	cfg->common.init = 1;
	cfg->common.kcfg = kcfg;
	cfg->common.tag = tag++ - 1;

	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++)
		list_add_tail(&cfg->limg[icnt].common.list,
			&cfg->common.layers);
	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++)
		list_add_tail(&cfg->losd[icnt].common.list,
			&cfg->common.layers);

	list_add_tail(&cfg->ld1.common.list, &cfg->common.layers);
}

int gsp_r9p0_core_init(struct gsp_core *core)
{
	int ret = 0;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_r9p0_core *c = (struct gsp_r9p0_core *)core;

	if (gsp_core_verify(core)) {
		GSP_ERR("core init params error");
		ret = -1;
		return ret;
	}

	gsp_r9p0_core_capa_init(core);

	gsp_r9p0_coef_cache_init(c);

	/* to do some specific initialize operations here */
	list_for_each_entry(kcfg, &core->kcfgs, sibling) {
		gsp_r9p0_core_cfg_init(
			(struct gsp_r9p0_cfg *)kcfg->cfg, kcfg);
	}

	return ret;
}

int gsp_r9p0_core_alloc(struct gsp_core **core, struct device_node *node)
{
	void *capa = NULL;
	struct platform_device *pdev = NULL;

	if (IS_ERR_OR_NULL(core)) {
		GSP_ERR("r9p0 core alloc params error\n");
		return -1;
	}

	/* device utility has been registered at of_device_add() */
	pdev = of_find_device_by_node(node);
	if (IS_ERR_OR_NULL(pdev)) {
		GSP_ERR("find gsp_r9p0 device by child node failed\n");
		return -1;
	}
	*core = devm_kzalloc(&pdev->dev, sizeof(struct gsp_r9p0_core),
						GFP_KERNEL);
	if (IS_ERR_OR_NULL(*core)) {
		GSP_ERR("no enough memory for core alloc\n");
		return -1;
	}

	memset(*core, 0, sizeof(struct gsp_r9p0_core));
	(*core)->cfg_size = sizeof(struct gsp_r9p0_cfg);
	(*core)->dev = &pdev->dev;
	if (dev_name(&pdev->dev))
		GSP_INFO("core[%d] device name: %s\n", (*core)->id,
			 dev_name(&pdev->dev));

	capa = kzalloc(sizeof(struct gsp_r9p0_capability), GFP_KERNEL);
	if (IS_ERR_OR_NULL(capa)) {
		GSP_ERR("gsp_r9p0 capability memory allocated failed\n");
		devm_kfree(&pdev->dev, *core);
		return -1;
	}
	(*core)->capa = capa;
	(*core)->capa->capa_size = sizeof(struct gsp_r9p0_capability);
	(*core)->node = node;
	GSP_DEBUG("gsp_r9p0_cap_size = %d\n", (int)(*core)->capa->capa_size);
	GSP_INFO("gsp_r9p0 core allocate success\n");
	return 0;
}


static void gsp_r9p0_core_irq_enable(struct gsp_core *core)
{
	struct R9P0_GSP_INT_REG gsp_int_value;
	struct R9P0_GSP_INT_REG gsp_int_mask;

	gsp_int_value.value = 0;
	gsp_int_value.INT_GSP_EN = 1;
	gsp_int_value.INT_GERR_EN = 1;
	gsp_int_value.INT_FBCDPL_EN = 1;
	gsp_int_value.INT_FBCDHD_EN = 1;
	gsp_int_mask.value = 0;
	gsp_int_mask.INT_GSP_EN = 0x1;
	gsp_int_mask.INT_GERR_EN = 0x1;
	gsp_int_mask.INT_FBCDPL_EN = 0x1;
	gsp_int_mask.INT_FBCDHD_EN = 0x1;
	gsp_core_reg_update(R9P0_GSP_INT(core->base),
			gsp_int_value.value, gsp_int_mask.value);
}

static irqreturn_t gsp_r9p0_core_irq_handler(int irq, void *data)
{
	struct gsp_core *core = NULL;
	struct R9P0_GSP_INT_REG gsp_int_value;
	enum gsp_core_state core_state = CORE_STATE_IRQ_HANDLED;

	core = (struct gsp_core *)data;
	if (gsp_core_verify(core)) {
		GSP_ERR("core irq handler not match\n");
		return IRQ_NONE;
	}

	gsp_int_value.value =
		gsp_core_reg_read(R9P0_GSP_INT(core->base));
	if (!gsp_int_value.INT_GSP_RAW &&
		!gsp_int_value.INT_GERR_RAW &&
		!gsp_int_value.INT_FBCDPL_RAW &&
		!gsp_int_value.INT_FBCDHD_RAW) {
		GSP_ERR("not gsp irq, return\n");
		return IRQ_NONE;
	}

	if (gsp_int_value.INT_GERR_RAW) {
		GSP_ERR("gsp error irq, GSP_INT[0x%x]\n",
			gsp_int_value.value);
		core_state = CORE_STATE_IRQ_ERR;
	}

	if (gsp_int_value.INT_FBCDPL_RAW || gsp_int_value.INT_FBCDHD_RAW) {
		GSP_ERR("gsp afbc error, payload L0-L3[%d %d %d %d]\n",
		gsp_int_value.INT_FBCDPL0_STS, gsp_int_value.INT_FBCDPL1_STS,
		gsp_int_value.INT_FBCDPL2_STS, gsp_int_value.INT_FBCDPL3_STS);

		GSP_ERR("gsp afbc error, head L0-L3[%d %d %d %d]\n",
		gsp_int_value.INT_FBCDHD0_STS, gsp_int_value.INT_FBCDHD1_STS,
		gsp_int_value.INT_FBCDHD2_STS, gsp_int_value.INT_FBCDHD3_STS);

		gsp_r9p0_int_clear(core);

		return IRQ_HANDLED;
	}

	gsp_r9p0_int_clear_and_disable(core);

	gsp_core_state_set(core, core_state);
	kthread_queue_work(&core->kworker, &core->release);

	return IRQ_HANDLED;
}

int gsp_r9p0_core_enable(struct gsp_core *c)
{
	int ret = -1;
	struct gsp_r9p0_core *core = NULL;

	core = (struct gsp_r9p0_core *)c;

	ret = clk_prepare_enable(core->gsp_dpuvsp_eb);
	if (ret) {
		GSP_ERR("enable gsp_dpuvsp_eb failed !\n");
		return ret;
	}

	ret = clk_prepare_enable(core->gsp_eb);
	if (ret) {
		GSP_ERR("enable gsp_eb failed !\n");
		goto gsp_eb_unprepare;
	}

	clk_set_parent(core->gsp_clk, NULL);
	ret = clk_set_parent(core->gsp_clk, core->gsp_clk_parent);
	if (ret) {
		GSP_ERR("select gsp clk source failed !\n");
		goto exit;
	}

	ret = clk_prepare_enable(core->gsp_clk);
	if (ret) {
		GSP_ERR("enable gsp_clk failed !\n");
		goto gsp_clk_unprepare;
	}

	gsp_r9p0_int_clear(c);
	gsp_r9p0_core_irq_enable(c);

	sprd_iommu_restore(c->dev);

	goto exit;

gsp_clk_unprepare:
	clk_disable_unprepare(core->gsp_clk);
gsp_eb_unprepare:
	clk_disable_unprepare(core->gsp_eb);

exit:
	return ret;
}

void gsp_r9p0_core_disable(struct gsp_core *c)
{
	struct gsp_r9p0_core *core = NULL;

	core = (struct gsp_r9p0_core *)c;
	gsp_r9p0_int_clear_and_disable(c);
	clk_disable_unprepare(core->gsp_clk);
	clk_disable_unprepare(core->gsp_eb);
	clk_disable_unprepare(core->gsp_dpuvsp_eb);
}

static int gsp_r9p0_core_parse_clk(struct gsp_r9p0_core *core)
{
	int status = 0;

	core->gsp_eb = of_clk_get_by_name(core->common.node,
			"clk_gsp_eb");

	core->gsp_dpuvsp_eb = of_clk_get_by_name(core->common.node,
			"clk_dpuvsp_eb");

	core->gsp_clk = of_clk_get_by_name(core->common.node,
			"clk_gsp");

	core->gsp_clk_parent = of_clk_get_by_name(core->common.node,
			"clk_src_512m");

	if (IS_ERR_OR_NULL(core->gsp_eb)
	       || IS_ERR_OR_NULL(core->gsp_dpuvsp_eb)
	       || IS_ERR_OR_NULL(core->gsp_clk)
	       || IS_ERR_OR_NULL(core->gsp_clk_parent)) {
		GSP_ERR("parse gsp clk failed\n");
		status = -1;
	}

	return status;
}

static int gsp_r9p0_core_parse_irq(struct gsp_core *core)
{
	int ret = -1;
	struct device *dev = NULL;
	static const char * const r9p0_irq_name[] = {
		"GSP0", "GSP1"
	};

	dev = gsp_core_to_device(core);
	core->irq = irq_of_parse_and_map(core->node, 0);

	if (core->irq)
		ret = devm_request_irq(dev, core->irq,
				gsp_r9p0_core_irq_handler,
				IRQF_SHARED, r9p0_irq_name[core->id], core);

	if (ret)
		GSP_ERR("r9p0 core[%d] request irq failed! %d\n",
		core->id, core->irq);

	return ret;
}

int gsp_r9p0_core_parse_dt(struct gsp_core *core)
{
	int ret = -1;
	struct device *dev = NULL;
	struct gsp_r9p0_core *r9p0_core = NULL;

	dev = container_of(&core->node, struct device, of_node);
	r9p0_core = (struct gsp_r9p0_core *)core;

	r9p0_core->gsp_ctl_reg_base = core->base;
	GSP_INFO("gsp_ctl_reg_base: 0x%p\n", core->base);

	ret = gsp_r9p0_core_parse_irq(core);
	if (ret) {
		GSP_ERR("core[%d] parse irq failed\n", core->id);
		return ret;
	}

	gsp_r9p0_core_parse_clk(r9p0_core);

	return ret;
}

static void gsp_r9p0_bit10_set(void __iomem *base, struct gsp_r9p0_cfg *cfg, int icnt)
{
	if (IS_ERR_OR_NULL(base)) {
		GSP_ERR("base address error\n");
		return;
	}

	gsp_core_reg_write(R9P0_HDR0_CFG(base, icnt), hdr_bypass_param[0]);
	gsp_core_reg_write(R9P0_HDR1_CFG(base, icnt), hdr_bypass_param[1]);
	gsp_core_reg_write(R9P0_HDR2_CFG(base, icnt), hdr_bypass_param[2]);
	gsp_core_reg_write(R9P0_HDR3_CFG(base, icnt), hdr_bypass_param[3]);
	gsp_core_reg_write(R9P0_HDR4_CFG(base, icnt), hdr_bypass_param[4]);
	gsp_core_reg_write(R9P0_HDR5_CFG(base, icnt), hdr_bypass_param[5]);
	gsp_core_reg_write(R9P0_HDR6_CFG(base, icnt), hdr_bypass_param[6]);
	gsp_core_reg_write(R9P0_HDR7_CFG(base, icnt), hdr_bypass_param[7]);
	gsp_core_reg_write(R9P0_HDR8_CFG(base, icnt), hdr_bypass_param[8]);

	gsp_core_reg_write(R9P0_HDR12_CFG(base, icnt), hdr_bypass_param[12]);
	gsp_core_reg_write(R9P0_HDR13_CFG(base, icnt), hdr_bypass_param[13]);
	gsp_core_reg_write(R9P0_HDR14_CFG(base, icnt), hdr_bypass_param[14]);
	gsp_core_reg_write(R9P0_HDR15_CFG(base, icnt), hdr_bypass_param[15]);
	gsp_core_reg_write(R9P0_HDR16_CFG(base, icnt), hdr_bypass_param[16]);
	gsp_core_reg_write(R9P0_HDR17_CFG(base, icnt), hdr_bypass_param[17]);
	gsp_core_reg_write(R9P0_HDR18_CFG(base, icnt), hdr_bypass_param[18]);
	gsp_core_reg_write(R9P0_HDR19_CFG(base, icnt), hdr_bypass_param[19]);
	gsp_core_reg_write(R9P0_HDR20_CFG(base, icnt), hdr_bypass_param[20]);
	gsp_core_reg_write(R9P0_HDR21_CFG(base, icnt), hdr_bypass_param[21]);
	gsp_core_reg_write(R9P0_HDR22_CFG(base, icnt), hdr_bypass_param[22]);
	gsp_core_reg_write(R9P0_HDR23_CFG(base, icnt), hdr_bypass_param[23]);
	gsp_core_reg_write(R9P0_HDR24_CFG(base, icnt), hdr_bypass_param[24]);
	gsp_core_reg_write(R9P0_HDR25_CFG(base, icnt), hdr_bypass_param[25]);
	gsp_core_reg_write(R9P0_HDR27_CFG(base, icnt), hdr_bypass_param[27]);
	gsp_core_reg_write(R9P0_HDR28_CFG(base, icnt), hdr_bypass_param[28]);
	gsp_core_reg_write(R9P0_HDR29_CFG(base, icnt), hdr_bypass_param[29]);

	gsp_core_reg_write(R9P0_HDR31_CFG(base, icnt), hdr_bypass_param[31]);
	gsp_core_reg_write(R9P0_HDR32_CFG(base, icnt), hdr_bypass_param[32]);
	gsp_core_reg_write(R9P0_HDR33_CFG(base, icnt), hdr_bypass_param[33]);
	gsp_core_reg_write(R9P0_HDR34_CFG(base, icnt), hdr_bypass_param[34]);
	gsp_core_reg_write(R9P0_HDR35_CFG(base, icnt), hdr_bypass_param[35]);

	gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), hdr_bypass_param[26]);
}

static void gsp_r9p0_hdr10_set(void __iomem *base, struct gsp_r9p0_cfg *cfg, int icnt)
{
	uint32_t cmd;
	int i;
	struct gsp_r9p0_hdr10_cfg *para = &(cfg->misc.hdr10_para[icnt]);

	if (IS_ERR_OR_NULL(base)) {
		GSP_ERR("base address error\n");
		return;
	}

	cmd = (para->reg_hdr_csc1_ycr & 0xFFFF);
	cmd |= ((para->reg_hdr_csc1_ucr & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR2_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc1_vcr & 0xFFFF);
	cmd |= ((para->reg_hdr_csc1_ycg & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR3_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc1_ucg & 0xFFFF);
	cmd |= ((para->reg_hdr_csc1_vcg & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR4_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc1_ycb & 0xFFFF);
	cmd |= ((para->reg_hdr_csc1_ucb & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR5_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc1_vcb & 0xFFFF);
	gsp_core_reg_write(R9P0_HDR6_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc1_ucb2 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc1_vcr2 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR7_CFG(base, icnt), cmd);

	cmd = ((para->reg_hdr_csc1_yls & 0x3FF) <<  0);
	cmd |= ((para->reg_hdr_csc1_uls & 0x3FF) << 10);
	cmd |= ((para->reg_hdr_csc1_vls & 0x3FF) << 20);
	gsp_core_reg_write(R9P0_HDR8_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c11 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c12 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR12_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c13 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c21 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR13_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c22 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c23 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR14_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c31 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c32 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR15_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c33 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c11_2 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR16_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c12_2 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c13_2 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR17_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c21_2 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c22_2 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR18_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c23_2 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c31_2 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR19_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_c32_2 & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_c33_2 & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR20_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_offset_r & 0xFFFF);
	cmd |= ((para->reg_hdr_csc2_offset_g & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR21_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_offset_b & 0xFFFF);
	gsp_core_reg_write(R9P0_HDR22_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc2_gain & 0xFFFF);
	gsp_core_reg_write(R9P0_HDR23_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc3_a11 & 0xFFF);
	cmd |= ((para->reg_hdr_csc3_a12 & 0xFFF) << 16);
	gsp_core_reg_write(R9P0_HDR31_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc3_a13 & 0xFFF);
	cmd |= ((para->reg_hdr_csc3_a21 & 0xFFF) << 16);
	gsp_core_reg_write(R9P0_HDR32_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc3_a22 & 0xFFF);
	cmd |= ((para->reg_hdr_csc3_a23 & 0xFFF) << 16);
	gsp_core_reg_write(R9P0_HDR33_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc3_a31 & 0xFFF);
	cmd |= ((para->reg_hdr_csc3_a32 & 0xFFF) << 16);
	gsp_core_reg_write(R9P0_HDR34_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_csc3_a33 & 0xFFF);
	gsp_core_reg_write(R9P0_HDR35_CFG(base, icnt), cmd);

	gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), HDR_DR_LUT_WRITE);

	for (i = 0; i < HDR_DEGAMMA_LUT_SIZE; ++i) {
		gsp_core_reg_write(R9P0_HDR9_CFG(base, icnt), i);
		if (para->transfer_char == PQ_TYPE)
			cmd = hdr_pq_degamma_tbl[i];
		else if (para->transfer_char == HLG_TYPE)
			cmd = hdr_hlg_degamma_tbl[i];
		else
			cmd = hdr_default_degamma_tbl[i];
		gsp_core_reg_write(R9P0_HDR10_CFG(base, icnt), cmd);
	}

	for (i = 0; i < HDR_REGAMMA_LUT_SIZE; ++i) {
		gsp_core_reg_write(R9P0_HDR36_CFG(base, icnt), i);
		gsp_core_reg_write(R9P0_HDR11_CFG(base, icnt), para->hdr_regamma_lut_table[i]);
	}

	gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), HDR_DR_LUT_WRITE_FINISH);


	if (para->tone_map_en == 1) {
		gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), HDR_TM_LUT_WRITE);

		for (i = 0; i < HDR_TM_LUT_SIZE; ++i) {
			gsp_core_reg_write(R9P0_HDR37_CFG(base, icnt), i);
			gsp_core_reg_write(R9P0_HDR30_CFG(base, icnt), hdr_default_tm_tbl[i]);
		}

		gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), HDR_TM_LUT_WRITE_FINISH);
	}

	cmd = (para->reg_hdr_slp & 0x1);
	cmd |= ((para->reg_hdr_bypass_csc1 & 0x1) << 8);
	cmd |= ((para->reg_hdr_bypass_degamma & 0x1) << 9);
	cmd |= ((para->reg_hdr_bypass_csc2 & 0x1) << 10);
	cmd |= ((para->reg_hdr_maxcll_gain_bypass & 0x1) << 11);
	cmd |= ((para->reg_hdr_bypass_gamma & 0x1) << 12);
	cmd |= ((para->reg_hdr_bypass_csc3 & 0x1) << 13);
	cmd |= ((para->reg_hdr_force_in_range_csc1 & 0x1) << 16);
	cmd |= ((para->reg_hdr_force_in_range_csc3 & 0x1) << 17);
	cmd |= ((para->reg_hdr_gamut_map_en & 0x1) << 18);
	cmd |= ((para->reg_hdr_csc1_cl_en & 0x1) << 19);
	cmd |= ((para->reg_hdr_avg_en & 0x1) << 20);
	gsp_core_reg_write(R9P0_HDR0_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_alpha_gain & 0xFF);
	cmd |= ((para->reg_hdr_sat_thr & 0xFFFF) << 8);
	gsp_core_reg_write(R9P0_HDR1_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_rg_step1 & 0x1F);
	cmd |= ((para->reg_hdr_rg_step2 & 0x1F) << 8);
	cmd |= ((para->reg_hdr_rg_step3 & 0x1F) << 16);
	cmd |= ((para->reg_hdr_rg_step4 & 0x1F) << 24);
	gsp_core_reg_write(R9P0_HDR24_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_diff_sat_thr & 0xFFFFFF);
	gsp_core_reg_write(R9P0_HDR25_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_tm_step1 & 0x1F);
	cmd |= ((para->reg_hdr_tm_step2 & 0x1F) << 8);
	cmd |= ((para->reg_hdr_tm_step3 & 0x1F) << 16);
	cmd |= ((para->reg_hdr_tm_step4 & 0x1F) << 24);
	gsp_core_reg_write(R9P0_HDR27_CFG(base, icnt), cmd);

	cmd = (para->reg_hdr_tm_norm_gain & 0xFFFF);
	cmd |= ((para->reg_hdr_tm1_beta_gain & 0xFFFF) << 16);
	gsp_core_reg_write(R9P0_HDR28_CFG(base, icnt), cmd);

	cmd = HDR_TM23_BETA_GAIN;
	gsp_core_reg_write(R9P0_HDR29_CFG(base, icnt), cmd);
}

static void gsp_r9p0_hdr10plus_set(void __iomem *base, struct gsp_r9p0_cfg *cfg, int icnt)
{
	int i;
	struct gsp_r9p0_hdr10_cfg *para = &(cfg->misc.hdr10_para[icnt]);

	if (IS_ERR_OR_NULL(base)) {
		GSP_ERR("base address error\n");
		return;
	}

	if (para->tone_map_en == 1) {
		gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), HDR_TM_LUT_WRITE);

		for (i = 0; i < HDR_TM_LUT_SIZE; ++i) {
			gsp_core_reg_write(R9P0_HDR37_CFG(base, icnt), i);
			gsp_core_reg_write(R9P0_HDR30_CFG(base, icnt), para->hdr_tone_mapping_lut_table[i]);
		}

		gsp_core_reg_write(R9P0_HDR23_CFG(base, icnt), (para->reg_hdr_csc2_gain & 0xFFFF));
		gsp_core_reg_write(R9P0_HDR26_CFG(base, icnt), HDR_TM_LUT_WRITE_FINISH);
	}
}

static int coreNumSwitch(struct gsp_r9p0_cfg *cfg)
{
	/*judge gsp core num status*/
	int core_switch_status;
	static bool last_is_2core_enable;
	bool cur_is_2core_enable = cfg->misc.core_num;

	if (last_is_2core_enable && !cur_is_2core_enable)
		core_switch_status = CORE_FROM_2_TO_1;
	else if (!last_is_2core_enable && cur_is_2core_enable)
		core_switch_status = CORE_FROM_1_TO_2;
	else
		core_switch_status = CORE_STS_NO_CHG;

	last_is_2core_enable = cur_is_2core_enable;

	return core_switch_status;
}

static void gsp_r9p0_core_hdr_reg_set(struct gsp_core *core,
			struct gsp_r9p0_cfg *cfg)
{
	void __iomem *base = NULL;
	int icnt = 0;
	static struct gsp_r9p0_hdr10_cfg para_backup_2core;
	static struct gsp_r9p0_hdr10_cfg para_backup_1core[2];
	int core_status = 0;

	base = core->base;

	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++) {
		if (icnt == 1 && core_status != CORE_STS_NO_CHG
			&& cfg->limg[icnt].params.hdr2rgb_mod == 0) {
			/*recover hdr set of layer1 when core status changed
			 * but layer1 is not YCBCR10*/
			memcpy(&(cfg->misc.hdr10_para[icnt]), &(para_backup_1core[icnt]),
				sizeof(struct gsp_r9p0_hdr10_cfg));
			if (para_backup_1core[icnt].transfer_char == 16
				|| para_backup_1core[icnt].transfer_char == 18)
				gsp_r9p0_hdr10_set(base, cfg, icnt);
			else
				gsp_r9p0_bit10_set(base, cfg, icnt);
		}

		if (cfg->limg[icnt].params.hdr2rgb_mod == 1) {
			if (icnt == 0)
				core_status = coreNumSwitch(cfg);
			if (cfg->misc.first10bit_frame[icnt] == 1) {
				/*backup hdr para when first frame of hdr video played*/
				if (cfg->misc.core_num == 1)
					memcpy(&para_backup_2core,
						&(cfg->misc.hdr10_para[icnt]),
						sizeof(struct gsp_r9p0_hdr10_cfg));
				else
					memcpy(&(para_backup_1core[icnt]),
						&(cfg->misc.hdr10_para[icnt]),
						sizeof(struct gsp_r9p0_hdr10_cfg));

				if (cfg->misc.hdr_flag[icnt] == 1) {
					gsp_r9p0_hdr10_set(base, cfg, icnt);
					gsp_r9p0_hdr10plus_set(base, cfg, icnt);
				} else
					gsp_r9p0_bit10_set(base, cfg, icnt);
			} else {
				/*recover hdr set when core status changed
				 * and layer format is YCBCR10*/
				if (core_status != CORE_STS_NO_CHG) {
					if (core_status == CORE_FROM_2_TO_1)
						memcpy(&(cfg->misc.hdr10_para[icnt]),
							&(para_backup_1core[icnt]),
							sizeof(struct gsp_r9p0_hdr10_cfg));
					else if (core_status == CORE_FROM_1_TO_2)
						memcpy(&(cfg->misc.hdr10_para[icnt]),
							&para_backup_2core,
							sizeof(struct gsp_r9p0_hdr10_cfg));

					if (cfg->misc.hdr_flag[icnt] == 1)
						gsp_r9p0_hdr10_set(base, cfg, icnt);
					else
						gsp_r9p0_bit10_set(base, cfg, icnt);
				} else if (cfg->misc.hdr10plus_update[icnt] == 1)
					gsp_r9p0_hdr10plus_set(base, cfg, icnt);
			}
		}
	}
}

static void dual_core_size_align_ops(struct gsp_r9p0_cfg *cfg)
{
	if (cfg->ld1.params.fbc_mod == 2) {
		if (cfg->ld1.params.img_format == GSP_R9P0_IMG_FMT_YUV420_2P) {

		if (cfg->limg[0].params.scale_para.scale_rect_out.rect_w % 32)
			cfg->limg[0].params.scale_para.scale_rect_out.rect_w +=
				(32 - cfg->limg[0].params.scale_para.scale_rect_out.rect_w % 32);
		if (cfg->limg[0].params.scale_para.scale_rect_out.rect_h % 8)
			cfg->limg[0].params.scale_para.scale_rect_out.rect_h +=
				(8 - cfg->limg[0].params.scale_para.scale_rect_out.rect_h % 8);
		if (cfg->limg[0].params.des_rect.st_x % 32)
			cfg->limg[0].params.des_rect.st_x +=
				(32 - cfg->limg[0].params.des_rect.st_x % 32);
		if (cfg->limg[0].params.des_rect.st_y % 8)
			cfg->limg[0].params.des_rect.st_y +=
				(8 - cfg->limg[0].params.des_rect.st_y % 8);
		} else if (cfg->ld1.params.img_format == GSP_R9P0_IMG_FMT_ARGB888 ||
				cfg->ld1.params.img_format == GSP_R9P0_IMG_FMT_RGB888) {

		if (cfg->limg[0].params.scale_para.scale_rect_out.rect_w % 16)
			cfg->limg[0].params.scale_para.scale_rect_out.rect_w +=
				(16 - cfg->limg[0].params.scale_para.scale_rect_out.rect_w % 16);
		if (cfg->limg[0].params.scale_para.scale_rect_out.rect_h % 16)
			cfg->limg[0].params.scale_para.scale_rect_out.rect_h +=
				(16 - cfg->limg[0].params.scale_para.scale_rect_out.rect_h % 16);
		if (cfg->limg[0].params.des_rect.st_x % 16)
			cfg->limg[0].params.des_rect.st_x +=
				(16 - cfg->limg[0].params.des_rect.st_x % 16);
		if (cfg->limg[0].params.des_rect.st_y % 16)
			cfg->limg[0].params.des_rect.st_y +=
				(16 - cfg->limg[0].params.des_rect.st_y % 16);
		}
	}
}

static void gsp_r9p0_core_misc_reg_set(struct gsp_core *core,
			struct gsp_r9p0_cfg *cfg)
{
	void __iomem *base = NULL;
	struct R9P0_WORK_AREA_XY_REG  work_area_xy_value;
	struct R9P0_WORK_AREA_XY_REG work_area_xy_mask;
	struct R9P0_WORK_AREA_SIZE_REG work_area_size_value;
	struct R9P0_WORK_AREA_SIZE_REG work_area_size_mask;
	struct R9P0_GSP_MOD_CFG_REG gsp_mod_cfg_value;
	struct R9P0_GSP_MOD_CFG_REG gsp_mod_cfg_mask;

	base = core->base;

	gsp_mod_cfg_value.value = 0x0;
	gsp_mod_cfg_value.CORE_NUM = cfg->misc.core_num;
	gsp_mod_cfg_value.PMARGB_EN = cfg->misc.pmargb_en;
	gsp_mod_cfg_mask.value = 0x0;
	gsp_mod_cfg_mask.CORE_NUM = 0x1;
	gsp_mod_cfg_mask.PMARGB_EN = 0x1;
	gsp_core_reg_update(R9P0_GSP_MOD_CFG(base),
		gsp_mod_cfg_value.value, gsp_mod_cfg_mask.value);

	work_area_xy_value.value = 0;
	work_area_xy_value.WORK_AREA_Y =
		cfg->misc.workarea_src_rect.st_y;
	work_area_xy_value.WORK_AREA_X =
		cfg->misc.workarea_src_rect.st_x;
	work_area_xy_mask.value = 0;
	work_area_xy_mask.WORK_AREA_Y = 0x1FFF;
	work_area_xy_mask.WORK_AREA_X = 0x1FFF;

	work_area_size_value.value = 0;
	work_area_size_value.WORK_AREA_H =
		cfg->misc.workarea_src_rect.rect_h;
	work_area_size_value.WORK_AREA_W =
		cfg->misc.workarea_src_rect.rect_w;
	work_area_size_mask.value = 0;
	work_area_size_mask.WORK_AREA_H = 0x1FFF;
	work_area_size_mask.WORK_AREA_W = 0x1FFF;

	if (cfg->misc.core_num == 1) {
		dual_core_size_align_ops(cfg);
		work_area_xy_value.WORK_AREA_Y =
			cfg->limg[0].params.des_rect.st_y;
		work_area_xy_value.WORK_AREA_X =
			cfg->limg[0].params.des_rect.st_x;
		work_area_size_value.WORK_AREA_H =
			cfg->limg[0].params.scale_para.scale_rect_out.rect_h;
		work_area_size_value.WORK_AREA_W =
			cfg->limg[0].params.scale_para.scale_rect_out.rect_w;

		if (!cfg->limg[0].params.scaling_en) {
			cfg->limg[0].params.clip_rect.rect_h =
				work_area_size_value.WORK_AREA_H;
			cfg->limg[0].params.clip_rect.rect_w =
				work_area_size_value.WORK_AREA_W;
		}
	}

	gsp_core_reg_update(R9P0_WORK_AREA_XY(base),
		work_area_xy_value.value, work_area_xy_mask.value);
	gsp_core_reg_update(R9P0_WORK_AREA_SIZE(base),
		work_area_size_value.value, work_area_size_mask.value);

	gsp_r9p0_core_hdr_reg_set(core, cfg);
}

static void gsp_r9p0_core_limg_reg_set(void __iomem *base,
			   struct gsp_r9p0_img_layer *layer, int layer_index)
{
	struct R9P0_LAYERIMG_CFG_REG limg_cfg_value;
	struct R9P0_LAYERIMG_CFG_REG limg_cfg_mask;
	struct R9P0_LAYERIMG_Y_ADDR_REG limg_y_addr_value;
	struct R9P0_LAYERIMG_Y_ADDR_REG limg_y_addr_mask;
	struct R9P0_LAYERIMG_U_ADDR_REG limg_u_addr_value;
	struct R9P0_LAYERIMG_U_ADDR_REG limg_u_addr_mask;
	struct R9P0_LAYERIMG_V_ADDR_REG limg_v_addr_value;
	struct R9P0_LAYERIMG_V_ADDR_REG limg_v_addr_mask;
	struct R9P0_LAYERIMG_PITCH_REG limg_pitch_value;
	struct R9P0_LAYERIMG_PITCH_REG limg_pitch_mask;
	struct R9P0_LAYERIMG_CLIP_SIZE_REG limg_clip_size_value;
	struct R9P0_LAYERIMG_CLIP_SIZE_REG limg_clip_size_mask;
	struct R9P0_LAYERIMG_CLIP_START_REG limg_clip_start_value;
	struct R9P0_LAYERIMG_CLIP_START_REG limg_clip_start_mask;
	struct R9P0_LAYERIMG_DES_START_REG limg_des_start_value;
	struct R9P0_LAYERIMG_DES_START_REG limg_des_start_mask;
	struct R9P0_LAYERIMG_CK_REG limg_ck_value;
	struct R9P0_LAYERIMG_CK_REG limg_ck_mask;
	struct R9P0_LAYERIMG_DES_SCL_SIZE_REG limg_des_size_value;
	struct R9P0_LAYERIMG_DES_SCL_SIZE_REG limg_des_size_mask;
	struct gsp_r9p0_img_layer_params *limg_params = NULL;
	int i = 0;

	if (IS_ERR_OR_NULL(base) || IS_ERR_OR_NULL(layer)) {
		GSP_ERR("layer0 reg set params error\n");
		return;
	}

	limg_params = &layer->params;

	if (layer->common.enable != 1) {
		limg_cfg_value.value = 0;
		limg_cfg_value.Limg_en = 0;
		for (i = 0; i < R9P0_IMGL_NUM; i++) {
			if (!zorder_used[i]) {
				limg_cfg_value.ZNUM_L = i;
				zorder_used[i] = 1;
				break;
			}
		}
		limg_cfg_mask.value = 0;
		limg_cfg_mask.Limg_en = 0x1;
		limg_cfg_mask.ZNUM_L = 0x3;
		gsp_core_reg_update(R9P0_LIMG_CFG(
			(base + layer_index * R9P0_LIMG_OFFSET)),
			limg_cfg_value.value, limg_cfg_mask.value);
		GSP_DEBUG("no need to set layer0\n");
		return;
	}

	/* img layer address set */
	limg_y_addr_value.value = 0;
	limg_y_addr_value.Y_BASE_ADDR = layer->common.src_addr.addr_y>>4;
	limg_y_addr_mask.value = 0;
	limg_y_addr_mask.Y_BASE_ADDR = 0xFFFFFFF;

	gsp_core_reg_update(R9P0_LIMG_Y_ADDR(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_y_addr_value.value, limg_y_addr_mask.value);

	limg_u_addr_value.value = 0;
	limg_u_addr_value.U_BASE_ADDR = layer->common.src_addr.addr_uv>>4;
	limg_u_addr_mask.value = 0;
	limg_u_addr_mask.U_BASE_ADDR = 0xFFFFFFF;
	gsp_core_reg_update(R9P0_LIMG_U_ADDR(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_u_addr_value.value, limg_u_addr_mask.value);

	limg_v_addr_value.value = 0;
	limg_v_addr_value.V_BASE_ADDR = layer->common.src_addr.addr_va>>4;
	limg_v_addr_mask.value = 0;
	limg_v_addr_mask.V_BASE_ADDR = 0xFFFFFFF;
	gsp_core_reg_update(R9P0_LIMG_V_ADDR(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_v_addr_value.value, limg_v_addr_mask.value);

	/* limg pitch and height set */
	limg_pitch_value.value = 0;
	limg_pitch_value.PITCH = limg_params->pitch;
	limg_pitch_value.HEIGHT = limg_params->height;
	limg_pitch_mask.value = 0;
	limg_pitch_mask.PITCH = 0x1FFF;
	limg_pitch_mask.HEIGHT = 0x1FFF;
	gsp_core_reg_update(R9P0_LIMG_PITCH(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_pitch_value.value, limg_pitch_mask.value);

	/* limg clip rect size set */
	limg_clip_size_value.value = 0;
	limg_clip_size_value.CLIP_SIZE_X = limg_params->clip_rect.rect_w;
	limg_clip_size_value.CLIP_SIZE_Y = limg_params->clip_rect.rect_h;
	limg_clip_size_mask.value = 0;
	limg_clip_size_mask.CLIP_SIZE_X = 0x1FFF;
	limg_clip_size_mask.CLIP_SIZE_Y = 0x1FFF;
	gsp_core_reg_update(R9P0_LIMG_CLIP_SIZE(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_clip_size_value.value, limg_clip_size_mask.value);

	/* limg clip start position set */
	limg_clip_start_value.value = 0;
	limg_clip_start_value.CLIP_START_X = limg_params->clip_rect.st_x;
	limg_clip_start_value.CLIP_START_Y = limg_params->clip_rect.st_y;
	limg_clip_start_mask.value = 0;
	limg_clip_start_mask.CLIP_START_X = 0x1FFF;
	limg_clip_start_mask.CLIP_START_Y = 0x1FFF;
	gsp_core_reg_update(R9P0_LIMG_CLIP_START(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_clip_start_value.value, limg_clip_start_mask.value);

	/* limg destination clip start position set */
	limg_des_start_value.value = 0;
	limg_des_start_value.DES_START_X = limg_params->des_rect.st_x;
	limg_des_start_value.DES_START_Y = limg_params->des_rect.st_y;
	limg_des_start_mask.value = 0;
	limg_des_start_mask.DES_START_X = 0x1FFF;
	limg_des_start_mask.DES_START_Y = 0x1FFF;
	gsp_core_reg_update(R9P0_LIMG_DES_START(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_des_start_value.value, limg_des_start_mask.value);

	limg_des_size_value.value = 0;
	limg_des_size_value.VTAP_MOD =
		limg_params->scale_para.vtap_mod;
	limg_des_size_value.DES_SCL_H =
		limg_params->scale_para.scale_rect_out.rect_h;
	limg_des_size_value.HTAP_MOD =
		limg_params->scale_para.htap_mod;
	limg_des_size_value.DES_SCL_W =
		limg_params->scale_para.scale_rect_out.rect_w;
	limg_des_size_mask.value = 0;
	limg_des_size_mask.VTAP_MOD = 0x3;
	limg_des_size_mask.DES_SCL_H = 0x1FFF;
	limg_des_size_mask.HTAP_MOD = 0x3;
	limg_des_size_mask.DES_SCL_W = 0x1FFF;
	gsp_core_reg_update(R9P0_LIMG_DES_SIZE(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_des_size_value.value, limg_des_size_mask.value);

	/* limg pallet rgb set */
	if (limg_params->pallet_en) {
		struct R9P0_LAYERIMG_PALLET_RGB_REG limg_pallet_value;
		struct R9P0_LAYERIMG_PALLET_RGB_REG limg_pallet_mask;

		limg_pallet_value.value = 0;
		limg_pallet_value.PALLET_A = limg_params->pallet.a_val;
		limg_pallet_value.PALLET_B = limg_params->pallet.b_val;
		limg_pallet_value.PALLET_G = limg_params->pallet.g_val;
		limg_pallet_value.PALLET_R = limg_params->pallet.r_val;
		limg_pallet_mask.value = 0;
		limg_pallet_mask.PALLET_A = 0xFF;
		limg_pallet_mask.PALLET_B = 0xFF;
		limg_pallet_mask.PALLET_G = 0xFF;
		limg_pallet_mask.PALLET_R = 0xFF;
		gsp_core_reg_update(R9P0_LIMG_PALLET_RGB(
			(base + layer_index * R9P0_LIMG_OFFSET)),
			limg_pallet_value.value, limg_pallet_mask.value);
	}


	/* limg block alpha set */
	limg_ck_value.value = 0;
	limg_ck_value.BLOCK_ALPHA = limg_params->alpha;
	limg_ck_mask.value = 0;
	limg_ck_mask.BLOCK_ALPHA = 0xFF;
	gsp_core_reg_update(R9P0_LIMG_CK(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_ck_value.value, limg_ck_mask.value);

	/* limg color key set */
	if (limg_params->colorkey_en) {
		limg_ck_value.value = 0;
		limg_ck_value.CK_B = limg_params->colorkey.b_val;
		limg_ck_value.CK_G = limg_params->colorkey.g_val;
		limg_ck_value.CK_R = limg_params->colorkey.r_val;
		limg_ck_mask.value = 0;
		limg_ck_mask.CK_B = 0xFF;
		limg_ck_mask.CK_G = 0xFF;
		limg_ck_mask.CK_R = 0xFF;
		gsp_core_reg_update(R9P0_LIMG_CK(
			(base + layer_index * R9P0_LIMG_OFFSET)),
			limg_ck_value.value, limg_ck_mask.value);
	}

	/*  yuv2yuv adjust */
	if (limg_params->y2y_mod == 0) {
		struct R9P0_Y2Y_Y_PARAM_REG y2y_y_para_value;
		struct R9P0_Y2Y_Y_PARAM_REG y2y_y_para_mask;
		struct R9P0_Y2Y_U_PARAM_REG y2y_u_para_value;
		struct R9P0_Y2Y_U_PARAM_REG y2y_u_para_mask;
		struct R9P0_Y2Y_V_PARAM_REG y2y_v_para_value;
		struct R9P0_Y2Y_V_PARAM_REG y2y_v_para_mask;

		y2y_y_para_value.value = 0;
		y2y_y_para_value.Y_BRIGHTNESS =
			limg_params->yuv_adjust.y_brightness;
		y2y_y_para_value.Y_CONTRAST =
			limg_params->yuv_adjust.y_contrast;
		y2y_y_para_mask.value = 0;
		y2y_y_para_mask.Y_BRIGHTNESS = 0x1FF;
		y2y_y_para_mask.Y_CONTRAST = 0x3FF;
		gsp_core_reg_update(R9P0_Y2Y_Y_PARAM(
			(base + layer_index * R9P0_LIMG_OFFSET)),
			y2y_y_para_value.value, y2y_y_para_mask.value);

		y2y_u_para_value.value = 0;
		y2y_u_para_value.U_OFFSET = limg_params->yuv_adjust.u_offset;
		y2y_u_para_value.U_SATURATION =
			limg_params->yuv_adjust.u_saturation;
		y2y_u_para_mask.value = 0;
		y2y_u_para_mask.U_OFFSET = 0xFF;
		y2y_u_para_mask.U_SATURATION = 0x3FF;
		gsp_core_reg_update(R9P0_Y2Y_U_PARAM(
			(base + layer_index * R9P0_LIMG_OFFSET)),
			y2y_u_para_value.value, y2y_u_para_mask.value);

		y2y_v_para_value.value = 0;
		y2y_v_para_value.V_OFFSET = limg_params->yuv_adjust.v_offset;
		y2y_v_para_value.V_SATURATION =
			limg_params->yuv_adjust.v_saturation;
		y2y_v_para_mask.value = 0;
		y2y_v_para_mask.V_OFFSET = 0xFF;
		y2y_v_para_mask.V_SATURATION = 0x3FF;
		gsp_core_reg_update(R9P0_Y2Y_V_PARAM(
			(base + layer_index * R9P0_LIMG_OFFSET)),
			y2y_v_para_value.value, y2y_v_para_mask.value);
	}

	/* limg cfg set */
	limg_cfg_value.value = 0;
	limg_cfg_value.Y_ENDIAN_MOD = limg_params->endian.y_rgb_word_endn;
	limg_cfg_value.UV_ENDIAN_MOD = limg_params->endian.uv_word_endn;
	limg_cfg_value.RGB_SWAP_MOD = limg_params->endian.rgb_swap_mode;
	limg_cfg_value.A_SWAP_MOD = limg_params->endian.a_swap_mode;
	limg_cfg_value.PMARGB_MOD = limg_params->pmargb_mod;
	limg_cfg_value.IMG_FORMAT = limg_params->img_format;
	limg_cfg_value.ROT_SRC = limg_params->rot_angle;
	limg_cfg_value.CK_EN = limg_params->colorkey_en;
	limg_cfg_value.PALLET_EN = limg_params->pallet_en;
	limg_cfg_value.FBCD_MOD = limg_params->fbcd_mod;
	limg_cfg_value.Y2R_MOD = limg_params->y2r_mod;
	limg_cfg_value.Y2Y_MOD = limg_params->y2y_mod;
	limg_cfg_value.ZNUM_L = limg_params->zorder;
	zorder_used[limg_cfg_value.ZNUM_L] = 1;
	limg_cfg_value.H2R_MOD = limg_params->hdr2rgb_mod;
	limg_cfg_value.SCALE_EN = limg_params->scaling_en;
	limg_cfg_value.Limg_en = layer->common.enable;
	limg_cfg_mask.value = 0;
	limg_cfg_mask.Y_ENDIAN_MOD = 0xF;
	limg_cfg_mask.UV_ENDIAN_MOD = 0xF;
	limg_cfg_mask.RGB_SWAP_MOD = 0x7;
	limg_cfg_mask.A_SWAP_MOD = 0x1;
	limg_cfg_mask.PMARGB_MOD = 0x1;
	limg_cfg_mask.IMG_FORMAT = 0x7;
	limg_cfg_mask.ROT_SRC = 0x7;
	limg_cfg_mask.CK_EN = 0x1;
	limg_cfg_mask.PALLET_EN = 0x1;
	limg_cfg_mask.FBCD_MOD = 0x3;
	limg_cfg_mask.Y2R_MOD = 0x7;
	limg_cfg_mask.Y2Y_MOD = 0x1;
	limg_cfg_mask.ZNUM_L = 0x3;
	limg_cfg_mask.H2R_MOD = 0x1;
	limg_cfg_mask.SCALE_EN = 0x1;
	limg_cfg_mask.Limg_en = 0x1;
	gsp_core_reg_update(R9P0_LIMG_CFG(
		(base + layer_index * R9P0_LIMG_OFFSET)),
		limg_cfg_value.value, limg_cfg_mask.value);

}

static void gsp_r9p0_core_losd_reg_set(void __iomem *base,
			   struct gsp_r9p0_osd_layer *layer, int layer_index)
{
	struct R9P0_LAYEROSD_CFG_REG losd_cfg_value;
	struct R9P0_LAYEROSD_CFG_REG losd_cfg_mask;
	struct R9P0_LAYEROSD_R_ADDR_REG losd_r_addr_value;
	struct R9P0_LAYEROSD_R_ADDR_REG losd_r_addr_mask;
	struct R9P0_LAYEROSD_PITCH_REG losd_pitch_value;
	struct R9P0_LAYEROSD_PITCH_REG losd_pitch_mask;
	struct R9P0_LAYEROSD_CLIP_SIZE_REG losd_clip_size_value;
	struct R9P0_LAYEROSD_CLIP_SIZE_REG losd_clip_size_mask;
	struct R9P0_LAYEROSD_CLIP_START_REG losd_clip_start_value;
	struct R9P0_LAYEROSD_CLIP_START_REG losd_clip_start_mask;
	struct R9P0_LAYEROSD_DES_START_REG losd_des_start_value;
	struct R9P0_LAYEROSD_DES_START_REG losd_des_start_mask;
	struct R9P0_LAYEROSD_CK_REG losd_ck_value;
	struct R9P0_LAYEROSD_CK_REG losd_ck_mask;
	struct gsp_r9p0_osd_layer_params *losd_params = NULL;
	int i = 0;

	if (IS_ERR_OR_NULL(base) || IS_ERR_OR_NULL(layer)) {
		GSP_ERR("LAYER1 reg set params error\n");
		return;
	}

	losd_params = &layer->params;

	if (layer->common.enable != 1) {
		losd_cfg_value.value = 0;
		losd_cfg_value.Losd_en = 0;
		for (i = R9P0_IMGL_NUM; i < R9P0_IMGL_NUM + R9P0_OSDL_NUM; i++) {
			if (!zorder_used[i]) {
				losd_cfg_value.ZNUM_L = i;
				zorder_used[i] = 1;
				break;
			}
		}
		losd_cfg_mask.value = 0;
		losd_cfg_mask.Losd_en = 0x1;
		losd_cfg_mask.ZNUM_L = 0x3;
		gsp_core_reg_update(R9P0_LOSD_CFG(
			(base + layer_index * R9P0_LOSD_OFFSET)),
			losd_cfg_value.value, losd_cfg_mask.value);
		GSP_DEBUG("no need to set LAYER1\n");
		return;
	}

	/* losd address set */
	losd_r_addr_value.value = 0;
	losd_r_addr_value.R_BASE_ADDR = layer->common.src_addr.addr_y>>4;
	losd_r_addr_mask.value = 0;
	losd_r_addr_mask.R_BASE_ADDR = 0xFFFFFFF;
	gsp_core_reg_update(R9P0_LOSD_R_ADDR(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_r_addr_value.value, losd_r_addr_mask.value);

	/* losd pitch set */
	losd_pitch_value.value = 0;
	losd_pitch_value.PITCH = losd_params->pitch;
	losd_pitch_value.HEIGHT = losd_params->height;
	losd_pitch_mask.value = 0;
	losd_pitch_mask.PITCH = 0x1FFF;
	losd_pitch_mask.HEIGHT = 0x1FFF;
	gsp_core_reg_update(R9P0_LOSD_PITCH(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_pitch_value.value, losd_pitch_mask.value);

	/* losd clip start position set */
	losd_clip_start_value.value = 0;
	losd_clip_start_value.CLIP_START_X = losd_params->clip_rect.st_x;
	losd_clip_start_value.CLIP_START_Y = losd_params->clip_rect.st_y;
	losd_clip_start_mask.value = 0;
	losd_clip_start_mask.CLIP_START_X = 0x1FFF;
	losd_clip_start_mask.CLIP_START_Y = 0x1FFF;
	gsp_core_reg_update(R9P0_LOSD_CLIP_START(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_clip_start_value.value, losd_clip_start_mask.value);

	/* losd clip rect size set */
	losd_clip_size_value.value = 0;
	losd_clip_size_value.CLIP_SIZE_X = losd_params->clip_rect.rect_w;
	losd_clip_size_value.CLIP_SIZE_Y = losd_params->clip_rect.rect_h;
	losd_clip_size_mask.value = 0;
	losd_clip_size_mask.CLIP_SIZE_X = 0x1FFF;
	losd_clip_size_mask.CLIP_SIZE_Y = 0x1FFF;
	gsp_core_reg_update(R9P0_LOSD_CLIP_SIZE(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_clip_size_value.value, losd_clip_size_mask.value);

	/* LAYER1 destination start position set */
	losd_des_start_value.value = 0;
	losd_des_start_value.DES_START_X = losd_params->des_pos.pt_x;
	losd_des_start_value.DES_START_Y = losd_params->des_pos.pt_y;
	losd_des_start_mask.value = 0;
	losd_des_start_mask.DES_START_X = 0x1FFF;
	losd_des_start_mask.DES_START_Y = 0x1FFF;
	gsp_core_reg_update(R9P0_LOSD_DES_START(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_des_start_value.value, losd_des_start_mask.value);

	/* losd pallet set */
	if (losd_params->pallet_en) {
		struct R9P0_LAYEROSD_PALLET_RGB_REG losd_pallet_value;
		struct R9P0_LAYEROSD_PALLET_RGB_REG losd_pallet_mask;

		losd_pallet_value.value = 0;
		losd_pallet_value.PALLET_A = losd_params->pallet.a_val;
		losd_pallet_value.PALLET_B = losd_params->pallet.b_val;
		losd_pallet_value.PALLET_G = losd_params->pallet.g_val;
		losd_pallet_value.PALLET_R = losd_params->pallet.r_val;
		losd_pallet_mask.value = 0;
		losd_pallet_mask.PALLET_A = 0xFF;
		losd_pallet_mask.PALLET_B = 0xFF;
		losd_pallet_mask.PALLET_G = 0xFF;
		losd_pallet_mask.PALLET_R = 0xFF;
		gsp_core_reg_update(R9P0_LOSD_PALLET_RGB(
			(base + layer_index * R9P0_LOSD_OFFSET)),
			losd_pallet_value.value, losd_pallet_mask.value);
	}

	/* LAYER1 alpha set */
	losd_ck_value.value = 0;
	losd_ck_value.BLOCK_ALPHA = losd_params->alpha;
	losd_ck_mask.value = 0;
	losd_ck_mask.BLOCK_ALPHA = 0xFF;
	gsp_core_reg_update(R9P0_LOSD_CK(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_ck_value.value, losd_ck_mask.value);

	/* LAYER1 color key set */
	if (losd_params->colorkey_en) {
		losd_ck_value.value = 0;
		losd_ck_value.CK_B = losd_params->colorkey.b_val;
		losd_ck_value.CK_G = losd_params->colorkey.g_val;
		losd_ck_value.CK_R = losd_params->colorkey.r_val;
		losd_ck_mask.value = 0;
		losd_ck_mask.CK_B = 0xFF;
		losd_ck_mask.CK_G = 0xFF;
		losd_ck_mask.CK_R = 0xFF;
		gsp_core_reg_update(R9P0_LOSD_CK(
			(base + layer_index * R9P0_LOSD_OFFSET)),
			losd_ck_value.value, losd_ck_mask.value);
	}

	/* losd cfg */
	losd_cfg_value.value = 0;
	losd_cfg_value.ENDIAN = losd_params->endian.y_rgb_word_endn;
	losd_cfg_value.RGB_SWAP = losd_params->endian.rgb_swap_mode;
	losd_cfg_value.A_SWAP = losd_params->endian.a_swap_mode;
	losd_cfg_value.PMARGB_MOD = losd_params->pmargb_mod;
	losd_cfg_value.IMG_FORMAT = losd_params->osd_format;
	losd_cfg_value.CK_EN = losd_params->colorkey_en;
	losd_cfg_value.PALLET_EN = losd_params->pallet_en;
	losd_cfg_value.FBCD_MOD = losd_params->fbcd_mod;
	losd_cfg_value.ZNUM_L = losd_params->zorder;
	zorder_used[losd_cfg_value.ZNUM_L] = 1;
	losd_cfg_value.Losd_en = layer->common.enable;
	losd_cfg_mask.value = 0;
	losd_cfg_mask.ENDIAN = 0xF;
	losd_cfg_mask.RGB_SWAP = 0x7;
	losd_cfg_mask.A_SWAP = 0x1;
	losd_cfg_mask.PMARGB_MOD = 0x1;
	losd_cfg_mask.IMG_FORMAT = 0x3;
	losd_cfg_mask.CK_EN = 0x1;
	losd_cfg_mask.PALLET_EN = 0x1;
	losd_cfg_mask.FBCD_MOD = 0x1;
	losd_cfg_mask.ZNUM_L = 0x3;
	losd_cfg_mask.Losd_en = 0x1;
	gsp_core_reg_update(R9P0_LOSD_CFG(
		(base + layer_index * R9P0_LOSD_OFFSET)),
		losd_cfg_value.value, losd_cfg_mask.value);

}

static void gsp_r9p0_core_ld1_reg_set(void __iomem *base,
			   struct gsp_r9p0_des_layer *layer,
			   struct gsp_r9p0_cfg *cfg)
{
	struct R9P0_DES_DATA_CFG_REG des_cfg_value;
	struct R9P0_DES_DATA_CFG_REG des_cfg_mask;
	struct R9P0_DES_Y_ADDR_REG des_y_addr_value;
	struct R9P0_DES_Y_ADDR_REG des_y_addr_mask;
	struct R9P0_DES_U_ADDR_REG des_u_addr_value;
	struct R9P0_DES_U_ADDR_REG des_u_addr_mask;
	struct R9P0_DES_V_ADDR_REG des_v_addr_value;
	struct R9P0_DES_V_ADDR_REG des_v_addr_mask;
	struct R9P0_DES_PITCH_REG des_pitch_value;
	struct R9P0_DES_PITCH_REG des_pitch_mask;
	struct gsp_r9p0_des_layer_params *ld1_params = NULL;

	if (IS_ERR_OR_NULL(base) || IS_ERR_OR_NULL(layer)) {
		GSP_ERR("LAYER Dest reg set params error\n");
		return;
	}

	ld1_params = &layer->params;

	if (layer->common.enable != 1)
		GSP_WARN("do no need to set LAYERD ? force set dest layer\n");

	/* dest layer address set */
	des_y_addr_value.value = 0;
	des_y_addr_value.DES_Y_BASE_ADDR1 = layer->common.src_addr.addr_y >> 4;
	des_y_addr_mask.value = 0;
	des_y_addr_mask.DES_Y_BASE_ADDR1 = 0xFFFFFFF;
	gsp_core_reg_update(R9P0_DES_Y_ADDR(base),
		des_y_addr_value.value, des_y_addr_mask.value);

	des_u_addr_value.value = 0;
	des_u_addr_value.DES_U_BASE_ADDR1 =
		layer->common.src_addr.addr_uv >> 4;
	des_u_addr_mask.value = 0;
	des_u_addr_mask.DES_U_BASE_ADDR1 = 0xFFFFFFF;
	if (ld1_params->fbc_mod)
		des_u_addr_value.DES_U_BASE_ADDR1 =
			ld1_params->header_size_r >> 4;

	gsp_core_reg_update(R9P0_DES_U_ADDR(base),
		des_u_addr_value.value, des_u_addr_mask.value);

	des_v_addr_value.value = 0;
	des_v_addr_value.DES_V_BASE_ADDR1 = layer->common.src_addr.addr_va;
	des_v_addr_mask.value = 0;
	des_v_addr_mask.DES_V_BASE_ADDR1 = 0xFFFFFFFF;
	gsp_core_reg_update(R9P0_DES_V_ADDR(base),
		des_v_addr_value.value, des_v_addr_mask.value);

	/* layerd pitch set - work plane configure */
	des_pitch_value.value = 0;
	des_pitch_value.DES_PITCH = ld1_params->pitch;
	des_pitch_value.DES_HEIGHT = ld1_params->height;
	des_pitch_mask.value = 0;
	des_pitch_mask.DES_PITCH = 0x1FFF;
	des_pitch_mask.DES_HEIGHT = 0x1FFF;
	gsp_core_reg_update(R9P0_DES_PITCH(base),
		des_pitch_value.value, des_pitch_mask.value);

	if (ld1_params->bk_para.bk_enable) {
		struct R9P0_BACK_RGB_REG back_rgb_value;
		struct R9P0_BACK_RGB_REG back_rgb_mask;

		back_rgb_value.value = 0;
		back_rgb_value.BACKGROUND_A =
			ld1_params->bk_para.background_rgb.a_val;
		back_rgb_value.BACKGROUND_B =
			ld1_params->bk_para.background_rgb.b_val;
		back_rgb_value.BACKGROUND_G =
			ld1_params->bk_para.background_rgb.g_val;
		back_rgb_value.BACKGROUND_R =
			ld1_params->bk_para.background_rgb.r_val;
		back_rgb_mask.value = 0;
		back_rgb_mask.BACKGROUND_A = 0xFF;
		back_rgb_mask.BACKGROUND_B = 0xFF;
		back_rgb_mask.BACKGROUND_G = 0xFF;
		back_rgb_mask.BACKGROUND_R = 0xFF;
		gsp_core_reg_update(R9P0_BACK_RGB(base),
			back_rgb_value.value, back_rgb_mask.value);
	}

	/* layerd cfg set */
	des_cfg_value.value = 0;
	des_cfg_value.Y_ENDIAN_MOD = ld1_params->endian.y_rgb_word_endn;
	des_cfg_value.UV_ENDIAN_MOD = ld1_params->endian.uv_word_endn;
	des_cfg_value.A_SWAP_MOD = ld1_params->endian.a_swap_mode;
	des_cfg_value.ROT_MOD = ld1_params->rot_angle;
	des_cfg_value.R2Y_MOD = ld1_params->r2y_mod;
	des_cfg_value.DES_IMG_FORMAT = ld1_params->img_format;
	des_cfg_value.RSWAP_MOD =
			ld1_params->endian.rgb_swap_mode;
	des_cfg_value.FBCE_MOD = ld1_params->fbc_mod;
	des_cfg_value.DITHER_EN = ld1_params->dither_en;
	if (cfg->misc.core_num == 1) {
		des_cfg_value.BK_EN = 0;
		des_cfg_value.BK_BLD = 0;
	} else {
		des_cfg_value.BK_EN = ld1_params->bk_para.bk_enable;
		des_cfg_value.BK_BLD = ld1_params->bk_para.bk_blend_mod;
	}
	des_cfg_mask.value = 0;
	des_cfg_mask.Y_ENDIAN_MOD = 0xF;
	des_cfg_mask.UV_ENDIAN_MOD = 0xF;
	des_cfg_mask.A_SWAP_MOD = 0x1;
	des_cfg_mask.ROT_MOD = 0x7;
	des_cfg_mask.R2Y_MOD = 0x7;
	des_cfg_mask.DES_IMG_FORMAT = 0x7;
	des_cfg_mask.RSWAP_MOD = 0x7;
	des_cfg_mask.FBCE_MOD = 0x3;
	des_cfg_mask.DITHER_EN = 0x1;
	des_cfg_mask.BK_EN = 0x1;
	des_cfg_mask.BK_BLD = 0x1;
	gsp_core_reg_update(R9P0_DES_DATA_CFG(base),
		des_cfg_value.value, des_cfg_mask.value);
}

static void gsp_r9p0_coef_gen_and_cfg(struct gsp_r9p0_core *core,
						struct gsp_r9p0_cfg *cmd)
{
	int i = 0;
	int jcnt = 0;
	uint32_t src_w = 0, src_h = 0;
	uint32_t dst_w = 0, dst_h = 0;
	uint32_t ver_tap, hor_tap;
	uint32_t *ret_coef = NULL;

	for (jcnt = 0; jcnt < R9P0_IMGL_NUM; jcnt++)
		if (cmd->limg[jcnt].params.scaling_en) {
			src_w =
			cmd->limg[jcnt].params.scale_para.scale_rect_in.rect_w;
			src_h =
			cmd->limg[jcnt].params.scale_para.scale_rect_in.rect_h;
			dst_w =
			cmd->limg[jcnt].params.scale_para.scale_rect_out.rect_w;
			dst_h =
			cmd->limg[jcnt].params.scale_para.scale_rect_out.rect_h;
			hor_tap = cmd->limg[jcnt].params.scale_para.htap_mod;
			ver_tap = cmd->limg[jcnt].params.scale_para.vtap_mod;

			ret_coef = gsp_r9p0_gen_block_scaler_coef(core,
								src_w, src_h,
								dst_w, dst_h,
								hor_tap,
								ver_tap);
			/*
			 * config the coef to register of the bind core of cmd
			 */
			if (ret_coef) {
				for (i = 0; i < 128; i++)
					gsp_core_reg_update(
						R9P0_SCALE_COEF_ADDR(
						(core->gsp_ctl_reg_base +
						jcnt * R9P0_SCALE_COEF_OFFSET
						+ i * 4)),
						*ret_coef++, 0xffffffff);

				GSP_DEBUG("config coef reg success.\n");
			} else {
				GSP_ERR("get scale coef failed...\n");
			}
		}
}

static void gsp_r9p0_core_run(struct gsp_core *core)
{
	struct R9P0_GSP_GLB_CFG_REG gsp_mod1_cfg_value;
	struct R9P0_GSP_GLB_CFG_REG gsp_mod1_cfg_mask;

	gsp_mod1_cfg_value.value = 0;
	gsp_mod1_cfg_value.GSP_RUN0 = 1;
	gsp_mod1_cfg_mask.value = 0;
	gsp_mod1_cfg_mask.GSP_RUN0 = 0x1;
	gsp_core_reg_update(R9P0_GSP_GLB_CFG(core->base),
		gsp_mod1_cfg_value.value, gsp_mod1_cfg_mask.value);
}

static int gsp_r9p0_core_run_precheck(struct gsp_core *c)
{
	return 0;
}
int gsp_r9p0_core_trigger(struct gsp_core *c)
{
	int ret = -1;
	int icnt = 0;
	void __iomem *base = NULL;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_r9p0_cfg *cfg = NULL;
	struct gsp_r9p0_core *core = (struct gsp_r9p0_core *)c;
	struct R9P0_GSP_GLB_CFG_REG gsp_mod1_cfg_value;

	if (gsp_core_verify(c)) {
		GSP_ERR("gsp_r9p0 core trigger params error\n");
		return ret;
	}

	kcfg = c->current_kcfg;
	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("gsp_r9p0 trigger invalidate kcfg\n");
		return ret;
	}

	/* hardware status check */
	gsp_mod1_cfg_value.value =
		gsp_core_reg_read(R9P0_GSP_GLB_CFG(c->base));
	if (gsp_mod1_cfg_value.GSP_BUSY0) {
		GSP_ERR("core is still busy, can't trigger\n");
		return GSP_K_HW_BUSY_ERR;
	}
	memset(zorder_used, 0, sizeof(zorder_used));
	base = c->base;
	cfg = (struct gsp_r9p0_cfg *)kcfg->cfg;

	gsp_r9p0_coef_gen_and_cfg(core, cfg);

	gsp_r9p0_core_misc_reg_set(c, cfg);
	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++)
		gsp_r9p0_core_limg_reg_set(base, &cfg->limg[icnt], icnt);
	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++)
		gsp_r9p0_core_losd_reg_set(base, &cfg->losd[icnt], icnt);
	gsp_r9p0_core_ld1_reg_set(base, &cfg->ld1, cfg);

	if (gsp_r9p0_core_run_precheck(c)) {
		GSP_ERR("r9p0 core run precheck fail !\n");
		return GSP_K_CLK_CHK_ERR;
	}

	gsp_r9p0_core_run(c);

	return 0;
};

int gsp_r9p0_core_release(struct gsp_core *c)
{
	struct gsp_r9p0_core *core = NULL;

	core = (struct gsp_r9p0_core *)c;

	return 0;
}

int gsp_r9p0_core_copy_cfg(struct gsp_kcfg *kcfg,
			void *arg, int index)
{
	struct gsp_r9p0_cfg_user *cfg_user_arr = NULL;
	struct gsp_r9p0_cfg_user *cfg_user;
	struct gsp_r9p0_cfg *cfg = NULL;
	int icnt = 0;

	gsp_r9p0_layer_num = 0;

	if (IS_ERR_OR_NULL(arg)
		|| 0 > index) {
		GSP_ERR("core copy params error\n");
		return -1;
	}

	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("core copy kcfg param error\n");
		return -1;
	}

	cfg_user_arr = (struct gsp_r9p0_cfg_user *)arg;
	cfg_user = &cfg_user_arr[index];

	cfg = (struct gsp_r9p0_cfg *)kcfg->cfg;

	/* we must reinitialize cfg again in case data is residual */
	gsp_r9p0_core_cfg_reinit(cfg);

	/* first copy common gsp layer params from user */
	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++)
		gsp_layer_common_copy_from_user(
			&cfg->limg[icnt], &cfg_user->limg[icnt]);

	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++)
		gsp_layer_common_copy_from_user(
			&cfg->losd[icnt], &cfg_user->losd[icnt]);

	gsp_layer_common_copy_from_user(&cfg->ld1, &cfg_user->ld1);

	/* second copy specific gsp layer params from user */
	for (icnt = 0; icnt < R9P0_IMGL_NUM; icnt++) {
		memcpy(&cfg->limg[icnt].params, &cfg_user->limg[icnt].params,
			   sizeof(struct gsp_r9p0_img_layer_params));
		gsp_layer_set_filled(&cfg->limg[icnt].common);
		if (cfg->limg[icnt].common.enable == 1)
			gsp_r9p0_layer_num++;
	}

	for (icnt = 0; icnt < R9P0_OSDL_NUM; icnt++) {
		memcpy(&cfg->losd[icnt].params, &cfg_user->losd[icnt].params,
			   sizeof(struct gsp_r9p0_osd_layer_params));
		gsp_layer_set_filled(&cfg->losd[icnt].common);
		if (cfg->losd[icnt].common.enable == 1)
			gsp_r9p0_layer_num++;
	}

	memcpy(&cfg->ld1.params, &cfg_user->ld1.params,
		   sizeof(struct gsp_r9p0_des_layer_params));
	gsp_layer_set_filled(&cfg->ld1.common);

	memcpy(&cfg->misc, &cfg_user->misc,
		   sizeof(struct gsp_r9p0_misc_cfg));

	gsp_r9p0_core_cfg_print(cfg);

	return 0;

}

int __user *gsp_r9p0_core_intercept(void __user *arg, int index)
{
	struct gsp_r9p0_cfg_user  __user *cfgs = NULL;

	cfgs = (struct gsp_r9p0_cfg_user __user *)arg;

	return &cfgs[index].ld1.common.sig_fd;
}

void gsp_r9p0_core_reset(struct gsp_core *core)
{

}

