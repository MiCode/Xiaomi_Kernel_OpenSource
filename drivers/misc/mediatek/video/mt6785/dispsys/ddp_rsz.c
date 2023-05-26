/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#define LOG_TAG "RSZ"

#include "ddp_info.h"
#include "ddp_reg.h"
#include "ddp_reg_rsz.h"
#include "ddp_log.h"
#include "disp_helper.h"
#include "ddp_clkmgr.h"
#include "ddp_hal.h"
#include <linux/delay.h>
#include "ddp_rsz.h"
#include "primary_display.h"
#include "disp_rect.h"

#define UNIT 32768
#define TILE_LOSS 4
#define TILE_LOSS_LEFT 4
#define TILE_LOSS_RIGHT 4

int rsz_calc_tile_params(u32 frm_in_len, u32 frm_out_len,
			 bool tile_mode, struct rsz_tile_params *t)
{
	u32 tile_loss = 0;
	u32 step = 0;
	s32 init_phase = 0;
	s32 offset[2] = { 0 };
	s32 int_offset[2] = { 0 };
	s32 sub_offset[2] = { 0 };
	u32 tile_in_len[2] = { 0 };
	u32 tile_out_len = 0;

	if (tile_mode)
		tile_loss = TILE_LOSS;

	if (frm_out_len == 1)
		step = UNIT;
	else
		step = (UNIT * (frm_in_len - 1) + (frm_out_len - 2)) /
						(frm_out_len - 1);

	/* left half */
	offset[0] = (step * (frm_out_len - 1) - UNIT * (frm_in_len - 1)) / 2;
	init_phase = UNIT - offset[0];
	sub_offset[0] = -offset[0];
	if (sub_offset[0] < 0) {
		int_offset[0]--;
		sub_offset[0] = UNIT + sub_offset[0];
	}
	if (sub_offset[0] >= UNIT) {
		int_offset[0]++;
		sub_offset[0] = sub_offset[0] - UNIT;
	}
	if (tile_mode) {
		tile_in_len[0] = frm_in_len / 2 + tile_loss;
		tile_out_len = frm_out_len / 2;
	} else {
		tile_in_len[0] = frm_in_len;
		tile_out_len = frm_out_len;
	}

	t[0].step = step;
	t[0].int_offset = (u32)(int_offset[0] & 0xffff);
	t[0].sub_offset = (u32)(sub_offset[0] & 0x1fffff);
	t[0].in_len = tile_in_len[0];
	t[0].out_len = tile_out_len;

	DDPDBG("%s:%s:step:%u,offset:%u.%u,len:%u->%u\n", __func__,
	       tile_mode ? "dual" : "single", t[0].step, t[0].int_offset,
	       t[0].sub_offset, t[0].in_len, t[0].out_len);

	if (!tile_mode)
		return 0;

	/* right half */
	offset[1] = (init_phase + frm_out_len / 2 * step) -
			(frm_in_len / 2 - tile_loss -
			 (offset[0] ? 1 : 0) + 1) * UNIT + UNIT;
	int_offset[1] = offset[1] / UNIT;
	sub_offset[1] = offset[1] - UNIT * int_offset[1];
	tile_in_len[1] = frm_in_len / 2 + tile_loss + (offset[0] ? 1 : 0);

	if (int_offset[1] & 0x1) {
		int_offset[1]++;
		tile_in_len[1]++;
		DDPMSG("right tile int_offset: make odd to even\n");
	}

	t[1].step = step;
	t[1].int_offset = (u32)(int_offset[1] & 0xffff);
	t[1].sub_offset = (u32)(sub_offset[1] & 0x1fffff);
	t[1].in_len = tile_in_len[1];
	t[1].out_len = tile_out_len;

	DDPDBG("%s:%s:step:%u,offset:%u.%u,len:%u->%u\n", __func__,
	       tile_mode ? "dual" : "single", t[1].step, t[1].int_offset,
	       t[1].sub_offset, t[1].in_len, t[1].out_len);

	return 0;
}

static unsigned long rsz_base_addr(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_RSZ0:
		return DISPSYS_RSZ0_BASE;
	default:
		DDP_PR_ERR("invalid rsz module:%s\n",
			   ddp_get_module_name(module));
		break;
	}
	return 0;
}

static int rsz_clock_on(enum DISP_MODULE_ENUM module, void *qhandle)
{
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
	return 0;
}

static int rsz_clock_off(enum DISP_MODULE_ENUM module, void *qhandle)
{
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
	return 0;
}

static int rsz_init(enum DISP_MODULE_ENUM module, void *qhandle)
{
	rsz_clock_on(module, qhandle);
	return 0;
}

static int rsz_deinit(enum DISP_MODULE_ENUM module, void *qhandle)
{
	rsz_clock_off(module, qhandle);
	return 0;
}

static int rsz_power_on(enum DISP_MODULE_ENUM module, void *qhandle)
{
	rsz_clock_on(module, qhandle);
	return 0;
}

static int rsz_power_off(enum DISP_MODULE_ENUM module, void *qhandle)
{
	rsz_clock_off(module, qhandle);
	return 0;
}

static int rsz_get_in_out_roi(struct disp_ddp_path_config *pconfig,
			     u32 *in_w, u32 *in_h, u32 *out_w, u32 *out_h)
{
	struct disp_rect *in = &pconfig->rsz_src_roi;
	struct disp_rect *out = &pconfig->rsz_dst_roi;

	if (!pconfig->rsz_enable)
		return 0;

	*in_w = in->width;
	*in_h = in->height;
	*out_w = out->width;
	*out_h = out->height;

	DDPDBG("[RPO] module=%s,(%dx%d)->(%dx%d)\n",
	       ddp_get_module_name(DISP_MODULE_RSZ0),
	       *in_w, *in_h, *out_w, *out_h);

	return 0;
}

static int rsz_check_params(struct RSZ_CONFIG_STRUCT *rsz_config)
{
	if ((rsz_config->frm_in_w != rsz_config->frm_out_w ||
	     rsz_config->frm_in_h != rsz_config->frm_out_h) &&
	    rsz_config->frm_in_w > RSZ_TILE_LENGTH) {
		DISP_PR_ERR("%s:need rsz but input width(%u) > limit(%u)\n",
			    __func__, rsz_config->frm_in_w, RSZ_TILE_LENGTH);
		return -EINVAL;
	}

	return 0;
}

static int disp_rsz_set_color_format(enum RSZ_COLOR_FORMAT fmt)
{
	u32 reg_val = 0;

	switch (fmt) {
	case ARGB8101010:
		reg_val = REG_FLD_VAL(FLD_RSZ_POWER_SAVING, 0x0);
		reg_val |= REG_FLD_VAL(FLD_RSZ_RGB_BIT_MODE, 0x0);
		break;
	case RGB999:
		reg_val = REG_FLD_VAL(FLD_RSZ_POWER_SAVING, 0x1);
		reg_val |= REG_FLD_VAL(FLD_RSZ_RGB_BIT_MODE, 0x0);
		break;
	case RGB888:
		reg_val = REG_FLD_VAL(FLD_RSZ_POWER_SAVING, 0x1);
		reg_val |= REG_FLD_VAL(FLD_RSZ_RGB_BIT_MODE, 0x1);
		break;
	default:
		DDPMSG("unknown resize color format\n");
		break;
	}
	return reg_val;
}

static int rsz_config(enum DISP_MODULE_ENUM module,
		      struct disp_ddp_path_config *pconfig, void *qhandle)
{
	unsigned long baddr = rsz_base_addr(module);
	struct RSZ_CONFIG_STRUCT *rsz_config = &pconfig->rsz_config;
	u32 in_w = 0, in_h = 0, out_w = 0, out_h = 0;
	u32 reg_val = 0;
	u32 tile_idx = 0;
	u32 frm_in_w = pconfig->dst_w, frm_in_h = pconfig->dst_h;
	u32 frm_out_w = pconfig->dst_w, frm_out_h = pconfig->dst_h;

	/* platform dependent: color_fmt, tile_mode */
	enum RSZ_COLOR_FORMAT fmt = ARGB8101010;
	bool tile_mode = false;

	if (!pconfig->ovl_dirty && !pconfig->dst_dirty)
		return 0;

	if (pconfig->dst_dirty) {
		rsz_config->frm_out_w = pconfig->dst_w;
		rsz_config->frm_out_h = pconfig->dst_h;
	}

	rsz_get_in_out_roi(pconfig, &frm_in_w, &frm_in_h,
			   &frm_out_w, &frm_out_h);
	rsz_config->frm_in_w = frm_in_w;
	rsz_config->frm_in_h = frm_in_h;
	rsz_config->frm_out_w = frm_out_w;
	rsz_config->frm_out_h = frm_out_h;

	if (rsz_check_params(rsz_config)) {
		static bool dump;
		struct OVL_CONFIG_STRUCT *c = &pconfig->ovl_config[0];

		DISP_PR_ERR("%s:L%d:en%d:(%u,%u,%ux%u)->(%u,%u,%ux%u)\n",
			    __func__, c->layer, c->layer_en,
			    c->src_x, c->src_y, c->src_w, c->src_h,
			    c->dst_x, c->dst_y, c->dst_w, c->dst_h);
		if (!dump) {
			dump = true;
			primary_display_diagnose(__func__, __LINE__);
			disp_aee_print("need rsz but input_w(%u) > limit(%u)\n",
				       rsz_config->frm_in_w, RSZ_TILE_LENGTH);
		}
		return -EINVAL;
	}

	rsz_calc_tile_params(rsz_config->frm_in_w, rsz_config->frm_out_w,
			     tile_mode, rsz_config->tw);
	rsz_calc_tile_params(rsz_config->frm_in_h, rsz_config->frm_out_h,
			     tile_mode, &rsz_config->th);

	in_w = rsz_config->tw[tile_idx].in_len;
	in_h = rsz_config->th.in_len;
	out_w = rsz_config->tw[tile_idx].out_len;
	out_h = rsz_config->th.out_len;

	if (in_w > out_w || in_h > out_h) {
		static bool dump;

		DDP_PR_ERR("DISP_RSZ only supports scale-up,(%ux%u)->(%ux%u)\n",
			   in_w, in_h, out_w, out_h);
		if (!dump) {
			dump = true;
			primary_display_diagnose(__func__, __LINE__);
			disp_aee_print("NOT scaling-up,(%ux%u)>(%ux%u)\n",
				       in_w, in_h, out_w, out_h);
		}
		return -EINVAL;
	}

	reg_val = 0;
	reg_val |= REG_FLD_VAL(FLD_RSZ_HORIZONTAL_EN, (in_w != out_w));
	reg_val |= REG_FLD_VAL(FLD_RSZ_VERTICAL_EN, (in_h != out_h));
	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_CONTROL_1, reg_val);
	DDPDBG("%s:CONTROL_1:0x%x\n", __func__, reg_val);

	reg_val = disp_rsz_set_color_format(fmt);

	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_CONTROL_2, reg_val);
	DDPDBG("%s:CONTROL_2:0x%x\n", __func__, reg_val);

	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_INPUT_IMAGE,
		     in_h << 16 | in_w);
	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_OUTPUT_IMAGE,
		     out_h << 16 | out_w);
	DDPDBG("%s:%s:(%ux%u)->(%ux%u)\n",
	       __func__, ddp_get_module_name(module), in_w, in_h, out_w, out_h);

	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_HORIZONTAL_COEFF_STEP,
		     rsz_config->tw[tile_idx].step);
	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_VERTICAL_COEFF_STEP,
		     rsz_config->th.step);
	DISP_REG_SET(qhandle,
		     baddr + DISP_REG_RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET,
		     rsz_config->tw[tile_idx].int_offset);
	DISP_REG_SET(qhandle,
		     baddr + DISP_REG_RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET,
		     rsz_config->tw[tile_idx].sub_offset);
	DISP_REG_SET(qhandle,
		     baddr + DISP_REG_RSZ_LUMA_VERTICAL_INTEGER_OFFSET,
		     rsz_config->th.int_offset);
	DISP_REG_SET(qhandle,
		     baddr + DISP_REG_RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET,
		     rsz_config->th.sub_offset);

	return 0;
}

static int rsz_start(enum DISP_MODULE_ENUM module, void *qhandle)
{
	unsigned long baddr = rsz_base_addr(module);
	u32 reg_val = 0;

	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		reg_val = 0;
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode, read shadow reg */
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit, read shadow reg */
			reg_val |= REG_FLD_VAL(FLD_RSZ_FORCE_COMMIT, 0x1);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow, read shadow reg */
			reg_val |= REG_FLD_VAL(FLD_RSZ_BYPASS_SHADOW, 0x1);
		}
		reg_val |= REG_FLD_VAL(FLD_RSZ_READ_WRK_REG, 0x0);
		DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_SHADOW_CTRL,
			     reg_val);
	}

	DISP_REG_SET_FIELD(qhandle, FLD_RSZ_EN,
			   baddr + DISP_REG_RSZ_ENABLE, 0x1);

	DISP_REG_SET(qhandle, baddr + DISP_REG_RSZ_DEBUG_SEL, 0x3);

	return 0;
}

static int rsz_stop(enum DISP_MODULE_ENUM module, void *qhandle)
{
	unsigned long baddr = rsz_base_addr(module);

	DISP_REG_SET_FIELD(qhandle, FLD_RSZ_EN,
			   baddr + DISP_REG_RSZ_ENABLE, 0x0);

	return 0;
}

static int rsz_reset(enum DISP_MODULE_ENUM module, void *qhandle)
{
	unsigned long baddr = rsz_base_addr(module);

	/* reset=1 -> wait 0.1 msec -> reset=0 */
	if (qhandle == NULL) {
		DISP_REG_SET_FIELD(NULL, FLD_RSZ_RST,
				   baddr + DISP_REG_RSZ_ENABLE, 0x1);
		usleep_range(100, 200);
		DISP_REG_SET_FIELD(NULL, FLD_RSZ_RST,
				   baddr + DISP_REG_RSZ_ENABLE, 0x0);
	} else {
		DDPDBG("DISP_RSZ does not support cmdq reset\n");
	}

	return 0;
}

static int rsz_suspend(enum DISP_MODULE_ENUM module, void *qhandle)
{
	rsz_clock_off(module, qhandle);
	return 0;
}

static int rsz_resume(enum DISP_MODULE_ENUM module, void *qhandle)
{
	rsz_clock_on(module, qhandle);
	return 0;
}

void rsz_dump_analysis(enum DISP_MODULE_ENUM module)
{
	unsigned long baddr = rsz_base_addr(module);
	u32 enable = 0;
	u32 con1 = 0;
	u32 con2 = 0;
	u32 int_flag = 0;
	u32 in_size = 0;
	u32 out_size = 0;
	u32 in_pos = 0;
	u32 shadow = 0;
	const int len = 100;
	char msg[len];
	int n = 0;

	enable = DISP_REG_GET(baddr + DISP_REG_RSZ_ENABLE);
	con1 = DISP_REG_GET(baddr + DISP_REG_RSZ_CONTROL_1);
	con2 = DISP_REG_GET(baddr + DISP_REG_RSZ_CONTROL_2);
	int_flag = DISP_REG_GET(baddr + DISP_REG_RSZ_INT_FLAG);
	in_size = DISP_REG_GET(baddr + DISP_REG_RSZ_INPUT_IMAGE);
	out_size = DISP_REG_GET(baddr + DISP_REG_RSZ_OUTPUT_IMAGE);
	in_pos = DISP_REG_GET(baddr + DISP_REG_RSZ_DEBUG);
	shadow = DISP_REG_GET(baddr + DISP_REG_RSZ_SHADOW_CTRL);

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));

	DISP_REG_SET(NULL, baddr + DISP_REG_RSZ_DEBUG_SEL, 0x3);
	n = snprintf(msg, len,
		     "en:%d,rst:%d,h_en:%d,v_en:%d,h_table:%d,v_table:%d,",
		     REG_FLD_VAL_GET(FLD_RSZ_EN, enable),
		     REG_FLD_VAL_GET(FLD_RSZ_RST, enable),
		     REG_FLD_VAL_GET(FLD_RSZ_HORIZONTAL_EN, con1),
		     REG_FLD_VAL_GET(FLD_RSZ_VERTICAL_EN, con1),
		     REG_FLD_VAL_GET(FLD_RSZ_HORIZONTAL_TABLE_SELECT, con1),
		     REG_FLD_VAL_GET(FLD_RSZ_VERTICAL_TABLE_SELECT, con1));
	n += snprintf(msg + n, len - n,
		      "dcm_dis:%d,int_en:%d,wclr_en:%d\n",
		      REG_FLD_VAL_GET(FLD_RSZ_DCM_DIS, con1),
		      REG_FLD_VAL_GET(FLD_RSZ_INTEN, con1),
		      REG_FLD_VAL_GET(FLD_RSZ_INT_WCLR_EN, con1));
	DDPDUMP("%s", msg);

	n = snprintf(msg, len,
		     "power_saving:%d,rgb_bit_mode:%d,frm_start:%d,frm_end:%d,",
		     REG_FLD_VAL_GET(FLD_RSZ_POWER_SAVING, con2),
		     REG_FLD_VAL_GET(FLD_RSZ_RGB_BIT_MODE, con2),
		     REG_FLD_VAL_GET(FLD_RSZ_FRAME_START, int_flag),
		     REG_FLD_VAL_GET(FLD_RSZ_FRAME_END, int_flag));
	n += snprintf(msg + n, len - n,
		      "size_err:%d,sof_rst:%d\n",
		      REG_FLD_VAL_GET(FLD_RSZ_SIZE_ERR, int_flag),
		      REG_FLD_VAL_GET(FLD_RSZ_SOF_RESET, int_flag));
	DDPDUMP("%s", msg);

	n = snprintf(msg, len,
		     "in(%ux%u),out(%ux%u),h_step:%d,v_step:%d\n",
		     REG_FLD_VAL_GET(FLD_RSZ_INPUT_IMAGE_W, in_size),
		     REG_FLD_VAL_GET(FLD_RSZ_INPUT_IMAGE_H, in_size),
		     REG_FLD_VAL_GET(FLD_RSZ_OUTPUT_IMAGE_W, out_size),
		     REG_FLD_VAL_GET(FLD_RSZ_OUTPUT_IMAGE_H, out_size),
		     DISP_REG_GET(baddr + DISP_REG_RSZ_HORIZONTAL_COEFF_STEP),
		     DISP_REG_GET(baddr + DISP_REG_RSZ_VERTICAL_COEFF_STEP));
	DDPDUMP("%s", msg);

	n = snprintf(msg, len,
		     "luma_h:%d.%d,luma_v:%d.%d\n",
		     DISP_REG_GET(baddr +
				  DISP_REG_RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET),
		     DISP_REG_GET(baddr +
				  DISP_REG_RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET),
		     DISP_REG_GET(baddr +
				  DISP_REG_RSZ_LUMA_VERTICAL_INTEGER_OFFSET),
		     DISP_REG_GET(baddr +
				  DISP_REG_RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET));
	DDPDUMP("%s", msg);

	n = snprintf(msg, len,
		     "dbg_sel:%d, in(%u,%u);shadow_ctrl:bypass:%d,force:%d,",
		     DISP_REG_GET(baddr + DISP_REG_RSZ_DEBUG_SEL),
		     in_pos & 0xFFFF, (in_pos >> 16) & 0xFFFF,
		     REG_FLD_VAL_GET(FLD_RSZ_BYPASS_SHADOW, shadow),
		     REG_FLD_VAL_GET(FLD_RSZ_FORCE_COMMIT, shadow));
	n += snprintf(msg + n, len - n, "read_working:%d\n",
		      REG_FLD_VAL_GET(FLD_RSZ_READ_WRK_REG, shadow));
	DDPDUMP("%s", msg);
}

void rsz_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned long baddr = rsz_base_addr(module);
	int i = 0;

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	for (i = 0; i < 3; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			i * 0x10, DISP_REG_GET(baddr + i * 0x10),
			DISP_REG_GET(baddr + i * 0x10 + 0x4),
			DISP_REG_GET(baddr + i * 0x10 + 0x8),
			DISP_REG_GET(baddr + i * 0x10 + 0xC));
	}
	DDPDUMP("0x044: 0x%08x 0x%08x; 0x0F0: 0x%08x\n",
		DISP_REG_GET(baddr + 0x44), DISP_REG_GET(baddr + 0x48),
		DISP_REG_GET(baddr + 0xF0));
}

static int rsz_dump_info(enum DISP_MODULE_ENUM module, int level)
{
	rsz_dump_analysis(module);
	rsz_dump_reg(module);

	return 0;
}

static int rsz_bypass(enum DISP_MODULE_ENUM module, int bypass)
{
	unsigned long baddr = rsz_base_addr(module);

	if (bypass) {
		/* use relay mode */
		DISP_REG_SET_FIELD(NULL, FLD_RSZ_EN,
				   baddr + DISP_REG_RSZ_ENABLE, 0x1);
		DISP_REG_SET_FIELD(NULL, FLD_RSZ_HORIZONTAL_EN,
				   baddr + DISP_REG_RSZ_CONTROL_1, 0x0);
		DISP_REG_SET_FIELD(NULL, FLD_RSZ_VERTICAL_EN,
				   baddr + DISP_REG_RSZ_CONTROL_1, 0x0);
	}

	return 0;
}

struct DDP_MODULE_DRIVER ddp_driver_rsz = {
	.init = rsz_init,
	.deinit = rsz_deinit,
	.config = rsz_config,
	.start = rsz_start,
	.stop = rsz_stop,
	.reset = rsz_reset,
	.power_on = rsz_power_on,
	.power_off = rsz_power_off,
	.suspend = rsz_suspend,
	.resume = rsz_resume,
	.dump_info = rsz_dump_info,
	.bypass = rsz_bypass,
};
