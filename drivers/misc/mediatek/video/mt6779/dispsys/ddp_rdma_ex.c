// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "RDMA"
#include "ddp_log.h"
#include "ddp_clkmgr.h"
#include <linux/delay.h>
#include "ddp_info.h"
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_rdma.h"
#include "ddp_rdma_ex.h"
#include "ddp_dump.h"
#include "lcm_drv.h"
#include "primary_display.h"
#include "ddp_m4u.h"
#include "disp_lowpower.h"
#include "ddp_mmp.h"
#include "ddp_misc.h"
#include "ddp_reg_mmsys.h"

#include <ion_sec_heap.h>
#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
#include "trusted_mem_api.h"
#endif

#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)

static unsigned int rdma_fps[RDMA_INSTANCES] = { 60, 60 };
static struct golden_setting_context *rdma_golden_setting;

struct RDMA_CONFIG_STRUCT g_primary_rdma_cfg;

unsigned int rdma_index(enum DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_RDMA0:
		idx = 0;
		break;
	case DISP_MODULE_RDMA1:
		idx = 1;
		break;
	default:
		DDP_PR_ERR("invalid rdma module=%d\n", module);
		ASSERT(0);
		break;
	}
	ASSERT((idx >= 0) && (idx < RDMA_INSTANCES));
	return idx;
}

void rdma_set_target_line(enum DISP_MODULE_ENUM module, unsigned int line,
			  void *handle)
{
	unsigned int idx = rdma_index(module);

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		     DISP_REG_RDMA_TARGET_LINE, line);
}

int rdma_init(enum DISP_MODULE_ENUM module, void *handle)
{
	return rdma_clock_on(module, handle);
}

int rdma_deinit(enum DISP_MODULE_ENUM module, void *handle)
{
	return rdma_clock_off(module, handle);
}

void rdma_get_address(enum DISP_MODULE_ENUM module, unsigned long *addr)
{
	unsigned int idx = rdma_index(module);

	*addr = DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR +
			     DISP_RDMA_INDEX_OFFSET * idx);
}

static inline unsigned long rdma_to_cmdq_engine(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_RDMA0:
		return CMDQ_ENG_DISP_RDMA0;
	case DISP_MODULE_RDMA1:
		return CMDQ_ENG_DISP_RDMA1;
	default:
		DDP_PR_ERR("invalid rdma module=%d,rdma to cmdq engine fail\n",
			   module);
		ASSERT(0);
		return DISP_MODULE_UNKNOWN;
	}
	return 0;
}

static inline unsigned long
rdma_to_cmdq_event_nonsec_end(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_RDMA0:
		return CMDQ_SYNC_DISP_RDMA0_2NONSEC_END;
	case DISP_MODULE_RDMA1:
		return CMDQ_SYNC_DISP_RDMA1_2NONSEC_END;
	default:
		DDP_PR_ERR("invalid rdma module=%d,rmda to cmdq event fail\n",
			   module);
		ASSERT(0);
		return DISP_MODULE_UNKNOWN;
	}

	return 0;
}

int rdma_enable_irq(enum DISP_MODULE_ENUM module, void *handle,
		    enum DDP_IRQ_LEVEL irq_level)
{
	unsigned int idx = rdma_index(module);

	ASSERT(idx <= RDMA_INSTANCES);

	switch (irq_level) {
	case DDP_IRQ_LEVEL_ALL:
		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			     DISP_REG_RDMA_INT_ENABLE, 0x1E);
		break;
	case DDP_IRQ_LEVEL_ERROR:
		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			     DISP_REG_RDMA_INT_ENABLE, 0x18);
		break;
	case DDP_IRQ_LEVEL_NONE:
		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			     DISP_REG_RDMA_INT_ENABLE, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

int rdma_start(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = rdma_index(module);
	unsigned int regval;
	unsigned int value = 0, mask = 0;

	ASSERT(idx <= RDMA_INSTANCES);

	regval = REG_FLD_VAL(INT_STATUS_FLD_REG_UPDATE_INT_FLAG, 0) |
			REG_FLD_VAL(INT_STATUS_FLD_FRAME_START_INT_FLAG, 1) |
			REG_FLD_VAL(INT_STATUS_FLD_FRAME_END_INT_FLAG, 1) |
			REG_FLD_VAL(INT_STATUS_FLD_EOF_ABNORMAL_INT_FLAG, 1) |
			REG_FLD_VAL(INT_STATUS_FLD_FIFO_UNDERFLOW_INT_FLAG, 1) |
			REG_FLD_VAL(INT_STATUS_FLD_TARGET_LINE_INT_FLAG, 0) |
			REG_FLD_VAL(INT_STATUS_FLD_FIFO_EMPTY_INT_FLAG, 0);

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		     DISP_REG_RDMA_INT_ENABLE, regval);
	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_ENGINE_EN,
			   idx * DISP_RDMA_INDEX_OFFSET +
			   DISP_REG_RDMA_GLOBAL_CON, 1);

	if (idx == 0) {
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 0, FLD_SODI_REQ_MASKEN_CG_RDMA0);
		SET_VAL_MASK(value, mask, 1, FLD_DVFS_HALT_MASK_RDMA0);
		DISP_REG_MASK(handle, DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK,
			value, mask);
	} else {
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 0, FLD_SODI_REQ_MASKEN_CG_RDMA1);
		SET_VAL_MASK(value, mask, 1, FLD_DVFS_HALT_MASK_RDMA1);
		DISP_REG_MASK(handle, DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK,
			value, mask);
	}

	return 0;
}

int rdma_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = rdma_index(module);
	unsigned int value = 0, mask = 0;

	ASSERT(idx <= RDMA_INSTANCES);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_ENGINE_EN,
			   idx * DISP_RDMA_INDEX_OFFSET +
			   DISP_REG_RDMA_GLOBAL_CON, 0);
	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		     DISP_REG_RDMA_INT_ENABLE, 0);
	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		     DISP_REG_RDMA_INT_STATUS, 0);

	if (idx == 0) {
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 1, FLD_SODI_REQ_MASKEN_CG_RDMA0);
		SET_VAL_MASK(value, mask, 0, FLD_DVFS_HALT_MASK_RDMA0);
		DISP_REG_MASK(handle, DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK,
			value, mask);
	} else {
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 1, FLD_SODI_REQ_MASKEN_CG_RDMA1);
		SET_VAL_MASK(value, mask, 0, FLD_DVFS_HALT_MASK_RDMA1);
		DISP_REG_MASK(handle, DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK,
			value, mask);
	}
	return 0;
}

int rdma_reset_by_cmdq(enum DISP_MODULE_ENUM module, void *handle)
{
	int ret = 0;
	unsigned int idx = rdma_index(module);

	ASSERT(idx <= RDMA_INSTANCES);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET,
			   idx * DISP_RDMA_INDEX_OFFSET +
			   DISP_REG_RDMA_GLOBAL_CON, 1);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET,
			   idx * DISP_RDMA_INDEX_OFFSET +
			   DISP_REG_RDMA_GLOBAL_CON, 0);

	DISP_REG_CMDQ_POLLING(handle, idx * DISP_RDMA_INDEX_OFFSET +
			      DISP_REG_RDMA_GLOBAL_CON, 0x100, 0x700);

	return ret;
}

int rdma_reset(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int delay_cnt = 0;
	int ret = 0, offset = 0;
	unsigned int idx = rdma_index(module);
	const int len = 160;
	char msg[len];
	int n = 0;

	ASSERT(idx <= RDMA_INSTANCES);

	offset = idx * DISP_RDMA_INDEX_OFFSET;
	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET, offset +
			   DISP_REG_RDMA_GLOBAL_CON, 1);
	while ((DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON) & 0x700) ==
	       0x100) {
		delay_cnt++;
		udelay(10);
		if (delay_cnt > 10000) {
			ret = -1;
			n = scnprintf(msg, len,
				      "rdma%d_reset timeout, stage 1! ", idx);
			n += scnprintf(msg + n, len - n,
				"DISP_REG_RDMA_GLOBAL_CON=0x%x\n",
				DISP_REG_GET(offset +
				DISP_REG_RDMA_GLOBAL_CON));
			DDP_PR_ERR("%s", msg);
			break;
		}
	}
	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET, offset +
			   DISP_REG_RDMA_GLOBAL_CON, 0);
	delay_cnt = 0;
	while ((DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON) & 0x700) !=
	       0x100) {
		delay_cnt++;
		udelay(10);
		if (delay_cnt <= 10000)
			continue;

		ret = -1;
		n = scnprintf(msg, len,
			     "rdma%d_reset timeout, stage 2! ", idx);
		n += scnprintf(msg + n, len - n,
			      "DISP_REG_RDMA_GLOBAL_CON=0x%x\n",
			      DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON));
		DDP_PR_ERR("%s", msg);
		break;
	}
	return ret;
}

/* TODO RDMA1, wrot sram */
void rdma_cal_golden_setting(unsigned int idx, unsigned int bpp,
	struct golden_setting_context *gsc, unsigned int *gs,
	unsigned int is_vdo)
{
	/* fixed variable */
	unsigned int mmsys_clk = 315;
	unsigned int pre_ultra_low_us = 245, pre_ultra_high_us = 255;
	unsigned int ultra_low_us = 230, ultra_high_us = 245;
	unsigned int if_fps = 60;
	unsigned int FP = 1000;

	/* input variable */
	unsigned int is_wrot_sram = gsc->is_wrot_sram;
	unsigned long long width = gsc->dst_width, height = gsc->dst_height;
	unsigned int Bpp = bpp / 8;
	bool is_dc = gsc->is_dc;

	unsigned int fill_rate = 0; /* 100 times */
	unsigned long long consume_rate = 0; /* 100 times */

	/* critical variable calc */
	if (is_dc)
		fill_rate = 96 * mmsys_clk; /* FIFO depth / us */
	else
		fill_rate = 96 * mmsys_clk * 3 / 16; /* FIFO depth / us */

	consume_rate = width * height * if_fps * Bpp;
	do_div(consume_rate, 1000);
	consume_rate *= 125;
	do_div(consume_rate, 16 * 1000);

	/* RDMA golden setting calculation */
	/* DISP_RDMA_MEM_GMC_SETTING_0 */
	gs[GS_RDMA_PRE_ULTRA_TH_LOW] = DIV_ROUND_UP(
		consume_rate * (pre_ultra_low_us), FP);
	gs[GS_RDMA_PRE_ULTRA_TH_HIGH] = DIV_ROUND_UP(
	    consume_rate * (pre_ultra_high_us), FP);
	if (is_vdo) {
		gs[GS_RDMA_VALID_TH_FORCE_PRE_ULTRA] = 0;
		gs[GS_RDMA_VDE_FORCE_PRE_ULTRA] = 1;
	} else {
		gs[GS_RDMA_VALID_TH_FORCE_PRE_ULTRA] = 1;
		gs[GS_RDMA_VDE_FORCE_PRE_ULTRA] = 0;
	}

	/* DISP_RDMA_MEM_GMC_SETTING_1 */
	gs[GS_RDMA_ULTRA_TH_LOW] = DIV_ROUND_UP(
	    consume_rate * (ultra_low_us), FP);
	gs[GS_RDMA_ULTRA_TH_HIGH] = gs[GS_RDMA_PRE_ULTRA_TH_LOW];
	if (is_vdo) {
		gs[GS_RDMA_VALID_TH_BLOCK_ULTRA] = 0;
		gs[GS_RDMA_VDE_BLOCK_ULTRA] = 1;
	} else {
		gs[GS_RDMA_VALID_TH_BLOCK_ULTRA] = 1;
		gs[GS_RDMA_VDE_BLOCK_ULTRA] = 0;
	}

	/* DISP_RDMA_FIFO_CON */
	if (is_vdo)
		gs[GS_RDMA_OUTPUT_VALID_FIFO_TH] = 0;
	else
		gs[GS_RDMA_OUTPUT_VALID_FIFO_TH] =
		    gs[GS_RDMA_PRE_ULTRA_TH_LOW];
	if (is_wrot_sram == 0)
		gs[GS_RDMA_FIFO_SIZE] = 1536;
	else
		gs[GS_RDMA_FIFO_SIZE] = 3584;
	gs[GS_RDMA_FIFO_UNDERFLOW_EN] = 1;

	/* DISP_RDMA_MEM_GMC_SETTING_2 */
	/* do not min this value with 256 to avoid hrt fail in
	 * dc mode under SODI CG mode
	 */
	gs[GS_RDMA_ISSUE_REQ_TH] =
		gs[GS_RDMA_FIFO_SIZE] - gs[GS_RDMA_PRE_ULTRA_TH_LOW];

	/* DISP_RDMA_THRESHOLD_FOR_SODI */
	gs[GS_RDMA_TH_LOW_FOR_SODI] = DIV_ROUND_UP(
		consume_rate * (ultra_low_us + 50), FP);
	gs[GS_RDMA_TH_HIGH_FOR_SODI] = DIV_ROUND_UP(gs[GS_RDMA_FIFO_SIZE] *
		FP - (fill_rate - consume_rate) * 12, FP);
	if (gs[GS_RDMA_TH_HIGH_FOR_SODI] < gs[GS_RDMA_PRE_ULTRA_TH_HIGH])
		gs[GS_RDMA_TH_HIGH_FOR_SODI] = gs[GS_RDMA_PRE_ULTRA_TH_HIGH];

	/* DISP_RDMA_THRESHOLD_FOR_DVFS */
	gs[GS_RDMA_TH_LOW_FOR_DVFS] = gs[GS_RDMA_PRE_ULTRA_TH_LOW];
	gs[GS_RDMA_TH_HIGH_FOR_DVFS] = gs[GS_RDMA_PRE_ULTRA_TH_LOW] + 1;

	/* DISP_RDMA_SRAM_SEL */
	if (is_wrot_sram == 0)
		gs[GS_RDMA_SRAM_SEL] = 0;
	else
		gs[GS_RDMA_SRAM_SEL] = 7;

	/* DISP_RDMA_DVFS_SETTING_PREULTRA */
	gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW] =  DIV_ROUND_UP(
		consume_rate * (pre_ultra_low_us + 40), FP);
	gs[GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH] = DIV_ROUND_UP(
	    consume_rate * (pre_ultra_high_us + 40), FP);

	/* DISP_RDMA_DVFS_SETTING_ULTRA */
	gs[GS_RDMA_DVFS_ULTRA_TH_LOW] = DIV_ROUND_UP(
	    consume_rate * (ultra_low_us + 40), FP);
	gs[GS_RDMA_DVFS_ULTRA_TH_HIGH] = gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW];

	/* DISP_RDMA_LEAVE_DRS_SETTING */
	gs[GS_RDMA_IS_DRS_STATUS_TH_LOW] = DIV_ROUND_UP(
		consume_rate * (pre_ultra_low_us + 20), FP);
	gs[GS_RDMA_IS_DRS_STATUS_TH_HIGH] = DIV_ROUND_UP(
		consume_rate * (pre_ultra_low_us + 20), FP);

	/* DISP_RDMA_ENTER_DRS_SETTING */
	gs[GS_RDMA_NOT_DRS_STATUS_TH_LOW] = DIV_ROUND_UP(
		consume_rate * (ultra_high_us + 40), FP);
	gs[GS_RDMA_NOT_DRS_STATUS_TH_HIGH] = DIV_ROUND_UP(
		consume_rate * (ultra_high_us + 40), FP);

	/* DISP_RDMA_MEM_GMC_SETTING_3 */
	gs[GS_RDMA_URGENT_TH_LOW] = DIV_ROUND_UP(
		consume_rate * 113, FP);
	gs[GS_RDMA_URGENT_TH_HIGH] = DIV_ROUND_UP(
		consume_rate * 117, FP);

	/* DISP_RDMA_SRAM_CASCADE */
	gs[GS_RDMA_SELF_FIFO_SIZE] = 1536;
	gs[GS_RDMA_RSZ_FIFO_SIZE] = 1536;
}

/* Set register with value from rdma_cal_golden_setting.
 * Do not do any math here!
 */
static void rdma_set_ultra_l(unsigned int idx, unsigned int bpp, void *handle,
			     struct golden_setting_context *gsc)
{
	unsigned int gs[GS_RDMA_FLD_NUM] = {0};
	unsigned int val = 0;
	unsigned int offset = idx * DISP_RDMA_INDEX_OFFSET;

	if (idx == 1) {
		DDPMSG("RDMA1 golden setting not support yet\n");
		return;
	}

	if (!gsc) {
		DDPMSG("golden setting is null, %s,%d\n",
			__FILE__, __LINE__);
		ASSERT(0);
		return;
	}

	rdma_golden_setting = gsc;

	/* calculate golden setting */
	rdma_cal_golden_setting(idx, bpp, gsc, gs,
		primary_display_is_video_mode());

	/* set golden setting */
	val = gs[GS_RDMA_PRE_ULTRA_TH_LOW] +
	    (gs[GS_RDMA_PRE_ULTRA_TH_HIGH] << 16) +
	    (gs[GS_RDMA_VALID_TH_FORCE_PRE_ULTRA] << 30) +
	    (gs[GS_RDMA_VDE_FORCE_PRE_ULTRA] << 31);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_0, val);

	val = gs[GS_RDMA_ULTRA_TH_LOW] +
	    (gs[GS_RDMA_ULTRA_TH_HIGH] << 16) +
	    (gs[GS_RDMA_VALID_TH_BLOCK_ULTRA] << 30) +
	    (gs[GS_RDMA_VDE_BLOCK_ULTRA] << 31);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_1, val);

	val = gs[GS_RDMA_ISSUE_REQ_TH];
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_2, val);

	val = gs[GS_RDMA_OUTPUT_VALID_FIFO_TH] +
	    (gs[GS_RDMA_FIFO_SIZE] << 16) +
	    (gs[GS_RDMA_FIFO_UNDERFLOW_EN] << 31);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_FIFO_CON, val);

	val = gs[GS_RDMA_TH_LOW_FOR_SODI] +
	    (gs[GS_RDMA_TH_HIGH_FOR_SODI] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_THRESHOLD_FOR_SODI, val);

	val = gs[GS_RDMA_TH_LOW_FOR_DVFS] +
	    (gs[GS_RDMA_TH_HIGH_FOR_DVFS] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_THRESHOLD_FOR_DVFS, val);

	if (idx == 0)
		DISP_REG_SET(handle, DISP_REG_RDMA_SRAM_SEL,
			gs[GS_RDMA_SRAM_SEL]);


	val = gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW] +
	    (gs[GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_DVFS_SETTING_PRE, val);

	val = gs[GS_RDMA_DVFS_ULTRA_TH_LOW] +
	    (gs[GS_RDMA_DVFS_ULTRA_TH_HIGH] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_DVFS_SETTING_ULTRA, val);

	val = gs[GS_RDMA_IS_DRS_STATUS_TH_LOW] +
	    (gs[GS_RDMA_IS_DRS_STATUS_TH_HIGH] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_LEAVE_DRS_SETTING, val);

	val = gs[GS_RDMA_NOT_DRS_STATUS_TH_LOW] +
	    (gs[GS_RDMA_NOT_DRS_STATUS_TH_HIGH] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_ENTER_DRS_SETTING, val);

	val = gs[GS_RDMA_URGENT_TH_LOW] +
	    (gs[GS_RDMA_URGENT_TH_HIGH] << 16);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_3, val);

	val = gs[GS_RDMA_SELF_FIFO_SIZE] +
	    (gs[GS_RDMA_RSZ_FIFO_SIZE] << 16);
	DISP_REG_SET(handle, offset + DISP_RDMA_SRAM_CASCADE, val);
}

static int rdma_config(enum DISP_MODULE_ENUM module, enum RDMA_MODE mode,
		       unsigned long address, enum UNIFIED_COLOR_FMT inFormat,
		       unsigned int pitch, unsigned int width,
		       unsigned int height, unsigned int ufoe_enable,
		       enum DISP_BUFFER_TYPE sec, struct ion_handle *ion_hnd,
		       unsigned int yuv_range,
		       struct rdma_bg_ctrl_t *bg_ctrl, void *handle,
		       struct golden_setting_context *p_golden_setting,
		       unsigned int bpp)
{
	unsigned int output_is_yuv = 0;
	unsigned int input_is_yuv = !UFMT_GET_RGB(inFormat);
	unsigned int input_swap = UFMT_GET_BYTESWAP(inFormat);
	unsigned int input_format = UFMT_GET_FORMAT(inFormat);
	unsigned int idx = rdma_index(module);
	unsigned int matrix_en;
	unsigned int color_matrix;
	unsigned int val, offset;
	unsigned int size_con_0_value = 0, size_con_0_mask = 0;
	unsigned int value = 0, mask = 0;

	DDPDBG("%s:%s,mode:%s,addr:0x%08lx,fmt:%s,pitch:%u,wh(%ux%u),sec:%d\n",
	       __func__, ddp_get_module_name(module), mode ? "MEM" : "DL",
	       address, unified_color_fmt_name(inFormat), pitch,
	       width, height, sec);

	ASSERT(idx <= RDMA_INSTANCES);
	if ((width > RDMA_MAX_WIDTH) || (height > RDMA_MAX_HEIGHT))
		DDP_PR_ERR("RDMA input overflow,w=%d,h=%d,max_w=%d,max_h=%d\n",
			   width, height, RDMA_MAX_WIDTH, RDMA_MAX_HEIGHT);

	offset = idx * DISP_RDMA_INDEX_OFFSET;
	if (input_is_yuv == 1 && output_is_yuv == 0) {
		matrix_en = 1;

		switch (yuv_range) {
		case 0:
			color_matrix = 4;
			break; /* BT601_full */
		case 1:
			color_matrix = 6;
			break; /* BT601 */
		case 2:
			color_matrix = 7;
			break; /* BT709 */
		default:
			DDP_PR_ERR("%s,un-recognized yuv_range=%d!\n",
				   __func__, yuv_range);
			color_matrix = 4;
			break;
		}
	} else if (input_is_yuv == 0 && output_is_yuv == 1) {
		matrix_en = 1;
		color_matrix = 0x2; /* 0x0010, RGB_TO_BT601 */
	} else {
		matrix_en = 0;
		color_matrix = 0;
	}

	SET_VAL_MASK(size_con_0_value, size_con_0_mask, matrix_en,
		SIZE_CON_0_FLD_MATRIX_ENABLE);
	SET_VAL_MASK(size_con_0_value, size_con_0_mask, color_matrix,
		SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL);

	if (mode == RDMA_MODE_DIRECT_LINK) {
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 0,
			MEM_CON_FLD_MEM_MODE_INPUT_FORMAT);
		SET_VAL_MASK(value, mask, 0,
			MEM_CON_FLD_MEM_MODE_INPUT_SWAP);
	} else {
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, (input_format & 0xf),
			MEM_CON_FLD_MEM_MODE_INPUT_FORMAT);
		SET_VAL_MASK(value, mask, input_swap,
			MEM_CON_FLD_MEM_MODE_INPUT_SWAP);
	}

	DISP_REG_MASK(handle, offset + DISP_REG_RDMA_MEM_CON, value, mask);

	if (sec != DISP_SECURE_BUFFER) {
		DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_START_ADDR,
			     address);
	} else {
		int m4u_port;
		unsigned int size = pitch * height;
		int sec = -1, sec_id = -1;
		ion_phys_addr_t sec_hdl = 0;
		enum TRUSTED_MEM_REQ_TYPE mem_type;

		m4u_port = idx == 0 ?  DISP_M4U_PORT_DISP_RDMA0 :
						DISP_M4U_PORT_DISP_RDMA1;
		/*
		 * for sec layer, addr variable stores sec handle
		 * we need to pass this handle and offset to cmdq driver
		 * cmdq sec driver will help to convert handle to
		 * correct address
		 */
		if (unlikely(!ion_hnd)) {
			DISP_LOG_E("%s #%d NULL handle for secure layer\n",
				   __func__, __LINE__);
			return 0;
		}
		mem_type = ion_hdl2sec_type(ion_hnd, &sec, &sec_id, &sec_hdl);

		if (unlikely(mem_type < 0)) {
			DISP_LOG_E("normal memory set as secure\n");
			return 0;
		}
		cmdqRecWriteSecureMetaData(handle, disp_addr_convert(offset +
						DISP_REG_RDMA_MEM_START_ADDR),
				CMDQ_SAM_H_2_MVA, address, 0, size, m4u_port, mem_type);
	}

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_SRC_PITCH, pitch);

	SET_VAL_MASK(size_con_0_value, size_con_0_mask, width,
		SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH);
	DISP_REG_MASK(handle, offset + DISP_REG_RDMA_SIZE_CON_0,
		size_con_0_value, size_con_0_mask);

	DISP_REG_SET_FIELD(handle, SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT,
			   offset + DISP_REG_RDMA_SIZE_CON_1, height);

	/* RDMA bg control */
	/* DISP_REG_RDMA_BG_CON_0 */
	val = REG_FLD_VAL(RDMA_BG_CON_0_LEFT, bg_ctrl->left);
	val |= REG_FLD_VAL(RDMA_BG_CON_0_RIGHT, bg_ctrl->right);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_BG_CON_0, val);

	/* DISP_REG_RDMA_BG_CON_1 */
	val = REG_FLD_VAL(RDMA_BG_CON_1_TOP, bg_ctrl->top);
	val |= REG_FLD_VAL(RDMA_BG_CON_1_BOTTOM, bg_ctrl->bottom);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_BG_CON_1, val);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_MODE_SEL,
			   offset + DISP_REG_RDMA_GLOBAL_CON, mode);

	set_rdma_width_height(width, height);
	rdma_set_ultra_l(idx, bpp, handle, p_golden_setting);

	return 0;
}

int rdma_clock_on(enum DISP_MODULE_ENUM module, void *handle)
{
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
	return 0;
}

int rdma_clock_off(enum DISP_MODULE_ENUM module, void *handle)
{
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
	return 0;
}

void rdma_dump_golden_setting_context(enum DISP_MODULE_ENUM module)
{
	if (!rdma_golden_setting)
		return;

	DDPDUMP("-- %s Golden Setting Context --\n",
		ddp_get_module_name(module));
	DDPDUMP("fifo_mode=%d\n", rdma_golden_setting->fifo_mode);
	DDPDUMP("hrt_num=%d\n", rdma_golden_setting->hrt_num);
	DDPDUMP("is_display_idle=%d\n", rdma_golden_setting->is_display_idle);
	DDPDUMP("is_wrot_sram=%d\n", rdma_golden_setting->is_wrot_sram);
	DDPDUMP("is_dc=%d\n", rdma_golden_setting->is_dc);
	DDPDUMP("mmsys_clk=%d\n", rdma_golden_setting->mmsys_clk);
	DDPDUMP("fps=%d\n", rdma_golden_setting->fps);
	DDPDUMP("is_one_layer=%d\n", rdma_golden_setting->is_one_layer);
	DDPDUMP("rdma_width=%d\n", rdma_golden_setting->dst_width);
	DDPDUMP("rdma_height=%d\n", rdma_golden_setting->dst_height);
}

void rdma_dump_golden_setting(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;

	DDPDUMP("-- %s Golden Setting --\n", ddp_get_module_name(module));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x\n",
		0x30, DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0 + offset),
		0x34, DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_1 + offset),
		0x3c, DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_2 + offset),
		0x40, DISP_REG_GET(DISP_REG_RDMA_FIFO_CON + offset));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x\n",
		0xa8, DISP_REG_GET(DISP_REG_RDMA_THRESHOLD_FOR_SODI + offset),
		0xac, DISP_REG_GET(DISP_REG_RDMA_THRESHOLD_FOR_DVFS + offset),
		0xb0, DISP_REG_GET(DISP_REG_RDMA_SRAM_SEL + offset),
		0xc8, DISP_REG_GET(DISP_RDMA_SRAM_CASCADE + offset));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
		0xd0, DISP_REG_GET(DISP_REG_RDMA_DVFS_SETTING_PRE + offset),
		DISP_REG_GET(DISP_REG_RDMA_DVFS_SETTING_ULTRA + offset),
		DISP_REG_GET(DISP_REG_RDMA_LEAVE_DRS_SETTING + offset),
		DISP_REG_GET(DISP_REG_RDMA_ENTER_DRS_SETTING + offset));
	DDPDUMP("0x%03x:0x%08x\n",
		0xe8, DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_3 + offset));

	DDPDUMP("GMC_SETTING_0 [11:0]:%u [27:16]:%u [30]:%u [31]:%u\n",
		DISP_REG_GET_FIELD(
			MEM_GMC_SETTING_0_FLD_PRE_ULTRA_THRESHOLD_LOW,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_0),
		DISP_REG_GET_FIELD(
			MEM_GMC_SETTING_0_FLD_PRE_ULTRA_THRESHOLD_HIGH,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_0),
		DISP_REG_GET_FIELD(
		MEM_GMC_SETTING_0_FLD_RG_VALID_THRESHOLD_FORCE_PREULTRA,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_0),
		DISP_REG_GET_FIELD(
			MEM_GMC_SETTING_0_FLD_RG_VDE_FORCE_PREULTRA,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_0));
	DDPDUMP("GMC_SETTING_1 [11:0]:%u [27:16]:%u [30]:%u [31]:%u\n",
		DISP_REG_GET_FIELD(MEM_GMC_SETTING_1_FLD_ULTRA_THRESHOLD_LOW,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_1),
		DISP_REG_GET_FIELD(MEM_GMC_SETTING_1_FLD_ULTRA_THRESHOLD_HIGH,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_1),
		DISP_REG_GET_FIELD(
		    MEM_GMC_SETTING_1_FLD_RG_VALID_THRESHOLD_BLOCK_ULTRA,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_1),
		DISP_REG_GET_FIELD(MEM_GMC_SETTING_1_FLD_RG_VDE_BLOCK_ULTRA,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_1));
	DDPDUMP("GMC_SETTING_2 [11:0]:%u\n",
		DISP_REG_GET_FIELD(MEM_GMC_SETTING_2_FLD_ISSUE_REQ_THRESHOLD,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_2));
	DDPDUMP("FIFO_CON [11:0]:%u [27:16]:%d [31]:%u\n",
		DISP_REG_GET_FIELD(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD,
			offset + DISP_REG_RDMA_FIFO_CON),
		DISP_REG_GET_FIELD(FIFO_CON_FLD_FIFO_PSEUDO_SIZE,
			offset + DISP_REG_RDMA_FIFO_CON),
		DISP_REG_GET_FIELD(FIFO_CON_FLD_FIFO_UNDERFLOW_EN,
			offset + DISP_REG_RDMA_FIFO_CON));
	DDPDUMP("THRSHOLD_SODI [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW,
			offset + DISP_REG_RDMA_THRESHOLD_FOR_SODI),
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH,
			offset + DISP_REG_RDMA_THRESHOLD_FOR_SODI));
	DDPDUMP("THRSHOLD_DVFS [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW,
			offset + DISP_REG_RDMA_THRESHOLD_FOR_DVFS),
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH,
			offset + DISP_REG_RDMA_THRESHOLD_FOR_DVFS));
	DDPDUMP("SRAM_SEL [0]:%u\n",
		DISP_REG_GET(offset + DISP_REG_RDMA_SRAM_SEL));
	DDPDUMP("SRAM_CASCADE [13:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RG_DISP_RDMA_FIFO_SIZE,
			offset + DISP_RDMA_SRAM_CASCADE),
		DISP_REG_GET_FIELD(RG_DISP_RDMA_RSZ_FIFO_SIZE,
			offset + DISP_RDMA_SRAM_CASCADE));
	DDPDUMP("DVFS_SETTING_PREULTRA [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW,
			offset + DISP_REG_RDMA_DVFS_SETTING_PRE),
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH,
			offset + DISP_REG_RDMA_DVFS_SETTING_PRE));
	DDPDUMP("DVFS_SETTING_ULTRA [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW,
			offset + DISP_REG_RDMA_DVFS_SETTING_ULTRA),
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH,
			offset + DISP_REG_RDMA_DVFS_SETTING_ULTRA));
	DDPDUMP("LEAVE_DRS_SETTING [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW,
			offset + DISP_REG_RDMA_LEAVE_DRS_SETTING),
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH,
			offset + DISP_REG_RDMA_LEAVE_DRS_SETTING));
	DDPDUMP("ENTER_DRS_SETTING [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW,
			offset + DISP_REG_RDMA_ENTER_DRS_SETTING),
		DISP_REG_GET_FIELD(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH,
			offset + DISP_REG_RDMA_ENTER_DRS_SETTING));
	DDPDUMP("GMC_SETTING_3 [11:0]:%u [27:16]:%u\n",
		DISP_REG_GET_FIELD(FLD_LOW_FOR_URGENT,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_3),
		DISP_REG_GET_FIELD(FLD_HIGH_FOR_URGENT,
			offset + DISP_REG_RDMA_MEM_GMC_SETTING_3));
}

void rdma_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;

	DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));
	DDPDUMP("0x%03x:0x%08x 0x%08x\n",
		0x00, DISP_REG_GET(DISP_REG_RDMA_INT_ENABLE + offset),
		DISP_REG_GET(DISP_REG_RDMA_INT_STATUS + offset));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
		0x10, DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON + offset),
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset),
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1 + offset),
		DISP_REG_GET(DISP_REG_RDMA_TARGET_LINE + offset));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x\n",
		0xf00, DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + offset),
		0x24, DISP_REG_GET(DISP_REG_RDMA_MEM_CON + offset),
		0x2c, DISP_REG_GET(DISP_REG_RDMA_MEM_SRC_PITCH + offset));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
		0x38, DISP_REG_GET(DISP_REG_RDMA_MEM_SLOW_CON + offset),
		0x44, DISP_REG_GET(DISP_REG_RDMA_FIFO_LOG + offset));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%03x:0x%08x\n",
		0x78, DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_0 + offset),
		DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_1 + offset),
		0x80, DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_2 + offset));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x\n",
		0x84, DISP_REG_GET(DISP_REG_RDMA_POST_ADD_0 + offset),
		DISP_REG_GET(DISP_REG_RDMA_POST_ADD_1 + offset),
		DISP_REG_GET(DISP_REG_RDMA_POST_ADD_2 + offset));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%03x:0x%08x 0x%08x\n",
		0x90, DISP_REG_GET(DISP_REG_RDMA_DUMMY + offset),
		DISP_REG_GET(DISP_REG_RDMA_DEBUG_OUT_SEL + offset),
		0xa0, DISP_REG_GET(DISP_REG_RDMA_BG_CON_0 + offset),
		DISP_REG_GET(DISP_REG_RDMA_BG_CON_1 + offset));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
		0xb4, DISP_REG_GET(DISP_REG_RDMA_STALL_CG_CON + offset),
		0xbc, DISP_REG_GET(DISP_REG_RDMA_SHADOW_UPDATE + offset));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
		0xf0, DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT + offset),
		DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT + offset),
		DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT + offset),
		DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT + offset));

	rdma_dump_golden_setting(module);
}

void rdma_dump_analysis(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = idx * DISP_RDMA_INDEX_OFFSET;
	unsigned int global_ctrl;
	unsigned int bg0 = DISP_REG_GET(offset + DISP_REG_RDMA_BG_CON_0);
	unsigned int bg1 = DISP_REG_GET(offset + DISP_REG_RDMA_BG_CON_1);
	unsigned int fifo = DISP_REG_GET(offset + DISP_REG_RDMA_FIFO_CON);
	const int len = 200;
	char msg[len];
	int n = 0;

	global_ctrl = DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON + offset);
	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	n = scnprintf(msg, len,
		     "en=%d,mode:%s,smi_busy:%d,wh(%dx%d),pitch=%d,",
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_ENGINE_EN + offset, global_ctrl),
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_MODE_SEL + offset,
				global_ctrl) ? "mem" : "DL",
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_SMI_BUSY + offset, global_ctrl),
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset) & 0xfff,
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1 + offset) & 0xfffff,
		DISP_REG_GET(DISP_REG_RDMA_MEM_SRC_PITCH + offset));
	n += scnprintf(msg + n, len - n,
		      "addr=0x%08x,fmt=%s,fifo_sz=%u,",
		DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + offset),
		unified_color_fmt_name(display_fmt_reg_to_unified_fmt(
					(DISP_REG_GET(DISP_REG_RDMA_MEM_CON +
						      offset) >> 4) & 0xf,
					(DISP_REG_GET(DISP_REG_RDMA_MEM_CON +
						      offset) >> 8) & 0x1, 0)),
		REG_FLD_VAL_GET(FIFO_CON_FLD_FIFO_PSEUDO_SIZE, fifo));
	n += scnprintf(msg + n, len - n,
		      "output_valid_threshold=%u,fifo_min=%d\n",
		REG_FLD_VAL_GET(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD, fifo),
		DISP_REG_GET(DISP_REG_RDMA_FIFO_LOG + offset));
	DDPDUMP("%s", msg);

	n = scnprintf(msg, len,
		     "pos:in(%d,%d)out(%d,%d),bg(t%d,b%d,l%d,r%d),",
		     DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT + offset),
		     DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT + offset),
		     DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT + offset),
		     DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT + offset),
		     REG_FLD_VAL_GET(RDMA_BG_CON_1_TOP + offset, bg1),
		     REG_FLD_VAL_GET(RDMA_BG_CON_1_BOTTOM + offset, bg1),
		     REG_FLD_VAL_GET(RDMA_BG_CON_0_LEFT + offset, bg0),
		     REG_FLD_VAL_GET(RDMA_BG_CON_0_RIGHT + offset, bg0));
	n += scnprintf(msg + n, len - n,
		      "start=%lld ns,end=%lld ns\n",
		      rdma_start_time[idx], rdma_end_time[idx]);
	DDPDUMP("%s", msg);

	DDPDUMP("irq cnt:start=%d,end=%d,underflow=%d,targetline=%d\n",
		rdma_start_irq_cnt[idx], rdma_done_irq_cnt[idx],
		rdma_underflow_irq_cnt[idx], rdma_targetline_irq_cnt[idx]);

	rdma_dump_golden_setting_context(module);
}

static int rdma_dump(enum DISP_MODULE_ENUM module, int level)
{
	rdma_dump_analysis(module);
	rdma_dump_reg(module);

	return 0;
}

void rdma_get_info(int idx, struct RDMA_BASIC_STRUCT *info)
{
	struct RDMA_BASIC_STRUCT *p = info;
	unsigned int offset = idx * DISP_RDMA_INDEX_OFFSET;

	p->addr = DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + offset);
	p->src_w = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset) & 0xfff;
	p->src_h = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1 + offset) & 0xfffff;
	p->bpp = UFMT_GET_bpp(display_fmt_reg_to_unified_fmt(
				(DISP_REG_GET(DISP_REG_RDMA_MEM_CON +
					      offset) >> 4) & 0xf,
				(DISP_REG_GET(DISP_REG_RDMA_MEM_CON +
					      offset) >> 8) & 0x1, 0)) / 8;
}

static inline enum RDMA_MODE get_rdma_mode(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);

	return DISP_REG_GET_FIELD(GLOBAL_CON_FLD_MODE_SEL,
				  (DISP_RDMA_INDEX_OFFSET * idx) +
				  DISP_REG_RDMA_GLOBAL_CON);
}

static inline enum RDMA_MODE rdma_config_mode(unsigned long address)
{
	return address ? RDMA_MODE_MEMORY : RDMA_MODE_DIRECT_LINK;
}

static int do_rdma_config_l(enum DISP_MODULE_ENUM module,
			    struct disp_ddp_path_config *pConfig, void *handle)
{
	struct RDMA_CONFIG_STRUCT *cfg = &pConfig->rdma_config;
	enum RDMA_MODE mode = rdma_config_mode(cfg->address);
	struct LCM_PARAMS *lcm_param = &pConfig->dispif_config;
	unsigned int width;
	unsigned int height;
	struct golden_setting_context *p_golden_setting;
	enum UNIFIED_COLOR_FMT inFormat = cfg->inputFormat;
	enum UNIFIED_COLOR_FMT bwFormat;
	unsigned int bwBpp;
	unsigned long long rdma_bw;

	width = pConfig->dst_dirty ? pConfig->dst_w : cfg->width;
	height = pConfig->dst_dirty ? pConfig->dst_h : cfg->height;
	p_golden_setting = pConfig->p_golden_setting_context;

	if (module == DISP_MODULE_RDMA0)
		memcpy(&g_primary_rdma_cfg, cfg,
			sizeof(struct RDMA_CONFIG_STRUCT));

	if (pConfig->fps)
		rdma_fps[rdma_index(module)] = pConfig->fps / 100;

	if (mode == RDMA_MODE_DIRECT_LINK &&
	    cfg->security != DISP_NORMAL_BUFFER)
		DDP_PR_ERR("%s: rdma directlink BUT is sec ??!!\n", __func__);

	if (mode == RDMA_MODE_DIRECT_LINK) {
		cfg->bg_ctrl.top = 0;
		cfg->bg_ctrl.bottom = 0;
		cfg->bg_ctrl.left = 0;
		cfg->bg_ctrl.right = 0;
	} else if (mode == RDMA_MODE_MEMORY) {
		cfg->bg_ctrl.top = cfg->dst_y;
		cfg->bg_ctrl.bottom = cfg->dst_h - cfg->dst_y - height;
		cfg->bg_ctrl.left = cfg->dst_x;
		cfg->bg_ctrl.right = cfg->dst_w - cfg->dst_x - width;
	}
	DDPDBG("%s:(t%u,b%u,l%u,r%u),r.dst(%u,%u,%ux%u),width=%u,height=%u\n",
	       __func__, cfg->bg_ctrl.top, cfg->bg_ctrl.bottom,
	       cfg->bg_ctrl.left, cfg->bg_ctrl.right,
	       cfg->dst_x, cfg->dst_y,
	       cfg->dst_w, cfg->dst_h, width, height);

	/* PARGB..etc need convert to ARGB..etc */
	ufmt_disable_P(cfg->inputFormat, &inFormat);

	rdma_config(module, mode,
		    (mode == RDMA_MODE_DIRECT_LINK) ? 0 : cfg->address,
		    (mode == RDMA_MODE_DIRECT_LINK) ? UFMT_RGB888 : inFormat,
		    (mode == RDMA_MODE_DIRECT_LINK) ? 0 : cfg->pitch,
		    width, height, lcm_param->dsi.ufoe_enable,
		    cfg->security, cfg->hnd,
		    cfg->yuv_range,
		    &cfg->bg_ctrl, handle, p_golden_setting,
		    pConfig->lcm_bpp);

	/* calculate bandwidth */
	bwFormat = (mode == RDMA_MODE_DIRECT_LINK) ? UFMT_RGB888 : inFormat;
	bwBpp = ufmt_get_Bpp(bwFormat);
	rdma_bw = (unsigned long long)width * height * bwBpp;
	do_div(rdma_bw, 1000);
	rdma_bw *= 1250;
	do_div(rdma_bw, 1000);
	DDPDBG("R:width=%u,height=%u,Bpp:%u,bw:%llu\n",
		width, height, bwBpp, rdma_bw);

	/* bandwidth report */
	if (module == DISP_MODULE_RDMA0)
		DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
			DISP_SLOT_RDMA0_BW, (unsigned int)rdma_bw);

	return 0;
}

static int rdma_is_sec[2];

static inline int rdma_switch_to_sec(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int rdma_idx = rdma_index(module);
	enum CMDQ_ENG_ENUM cmdq_engine;

	cmdq_engine = rdma_to_cmdq_engine(module);
	cmdqRecSetSecure(handle, 1);
	/* set engine as sec port */
	cmdqRecSecureEnablePortSecurity(handle, (1LL << cmdq_engine));
	/* cmdqRecSecureEnableDAPC(handle, (1LL << cmdq_engine)); */
	if (rdma_is_sec[rdma_idx] == 0) {
		DDPSVPMSG("[SVP] switch rdma%d to sec\n", rdma_idx);
		mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
				 MMPROFILE_FLAG_START, 0, 0);
	}
	rdma_is_sec[rdma_idx] = 1;

	return 0;
}

int rdma_switch_to_nonsec(enum DISP_MODULE_ENUM module,
			  struct disp_ddp_path_config *pConfig, void *handle)
{
	unsigned int rdma_idx = rdma_index(module);
	enum CMDQ_ENG_ENUM cmdq_engine;

	cmdq_engine = rdma_to_cmdq_engine(module);
	if (rdma_is_sec[rdma_idx] == 1) {
		/* RDMA is in sec stat, we need to switch it to nonsec */
		struct cmdqRecStruct *nonsec_switch_handle = NULL;
		int ret;

		ret = cmdqRecCreate(
				CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH,
				&(nonsec_switch_handle));
		if (ret)
			DDPAEE("[SVP]fail to create disable handle %s ret=%d\n",
			       __func__, ret);

		cmdqRecReset(nonsec_switch_handle);

		if (rdma_idx == 0) {
			/* Primary Decouple Mode */
			_cmdq_insert_wait_frame_done_token_mira(
							nonsec_switch_handle);
		} else {
			/* External Mode */
			/* OVL1->RDMA1, do not use. */
			_cmdq_insert_wait_frame_done_token_mira(
							nonsec_switch_handle);
		}

		cmdqRecSetSecure(nonsec_switch_handle, 1);
		/*
		 * ugly workaround by kzhang !!
		 * will remove when cmdq delete disable scenario.
		 * To avoid translation fault like OVL (see notes in ovl.c)
		 * check the mode now, bypass the frame during DL->DC(),
		 * avoid hang when VDO mode.
		 */
		if (get_rdma_mode(module) == RDMA_MODE_MEMORY)
			do_rdma_config_l(module, pConfig, nonsec_switch_handle);

		/* in fact, dapc/port_sec will be disabled by cmdq */
		cmdqRecSecureEnablePortSecurity(nonsec_switch_handle,
						(1LL << cmdq_engine));
		if (handle) {
			/* Async Flush method */
			enum CMDQ_EVENT_ENUM cmdq_event_nonsec_end;

			cmdq_event_nonsec_end =
					rdma_to_cmdq_event_nonsec_end(module);
			cmdqRecSetEventToken(nonsec_switch_handle,
					     cmdq_event_nonsec_end);
			cmdqRecFlushAsync(nonsec_switch_handle);
			cmdqRecWait(handle, cmdq_event_nonsec_end);
		} else {
			/* Sync Flush method */
			cmdqRecFlush(nonsec_switch_handle);
		}

		cmdqRecDestroy(nonsec_switch_handle);
		DDPSVPMSG("[SVP] switch rdma%d to nonsec\n", rdma_idx);
		mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
				 MMPROFILE_FLAG_END, 0, 0);
	}

	rdma_is_sec[rdma_idx] = 0;

	return 0;
}

int rdma_wait_sec_done(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *handle)
{
	unsigned int rdma_idx = rdma_index(module);
	enum CMDQ_ENG_ENUM cmdq_engine;
	struct cmdqRecStruct *wait_handle;
	int ret;

	if (rdma_is_sec[rdma_idx] == 0)
		return 0;
	cmdq_engine = rdma_to_cmdq_engine(module);
	/* rdma is in sec stat, we need to switch it to nonsec */
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,
			&(wait_handle));
	if (ret)
		DDPAEE("[SVP]fail to create disable handle %s ret=%d\n",
			__func__, ret);
	cmdqRecReset(wait_handle);
	cmdqRecSetSecure(wait_handle, 1);
	cmdqRecSecureEnablePortSecurity(wait_handle,
		(1LL << cmdq_engine));
	if (handle != NULL) {
		/*Async Flush method*/
		enum CMDQ_EVENT_ENUM cmdq_event_nonsec_end;

		cmdq_event_nonsec_end = rdma_to_cmdq_event_nonsec_end(module);
		cmdqRecSetEventToken(wait_handle, cmdq_event_nonsec_end);
		cmdqRecFlushAsync(wait_handle);
		cmdqRecWait(handle, cmdq_event_nonsec_end);
	} else {
		/*Sync Flush method*/
		cmdqRecFlush(wait_handle);
	}
	cmdqRecDestroy(wait_handle);
	mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
			MMPROFILE_FLAG_END, 0, 0);
	/* MMProfileLogEx(ddp_mmp_get_events()->svp_module[module],
	 * MMProfileFlagPulse, 1, 1);
	 */
	return 0;
}

static int setup_rdma_sec(enum DISP_MODULE_ENUM module,
			  struct disp_ddp_path_config *pConfig, void *handle)
{
	int ret;
	int is_engine_sec = 0;
	enum RDMA_MODE mode = rdma_config_mode(pConfig->rdma_config.address);

	if (pConfig->rdma_config.security == DISP_SECURE_BUFFER)
		is_engine_sec = 1;

	if (!handle) {
		DDPDBG("[SVP] bypass rdma sec setting sec=%d,handle=NULL\n",
		       is_engine_sec);
		return 0;
	}

	/* sec setting makes sense only in memory mode! */
	if (mode == RDMA_MODE_MEMORY) {
		if (is_engine_sec == 1)
			ret = rdma_switch_to_sec(module, handle);
		else
			/* handle = NULL, use the sync flush method */
			ret = rdma_switch_to_nonsec(module, pConfig, NULL);

		if (ret)
			DDPAEE("[SVP]fail to %s: ret=%d\n",
			       __func__, ret);
	} else {
		rdma_wait_sec_done(module, pConfig, NULL);
	}

	return is_engine_sec;
}

static int rdma_config_l(enum DISP_MODULE_ENUM module,
			 struct disp_ddp_path_config *pConfig, void *handle)
{
	if (pConfig->dst_dirty || pConfig->rdma_dirty) {
		setup_rdma_sec(module, pConfig, handle);
		do_rdma_config_l(module, pConfig, handle);
	}
	return 0;
}

void rdma_enable_color_transform(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;
	UINT32 value = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset);

	value = value | REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_EXT_MTX_EN), 1) |
			REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_ENABLE), 1);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_SIZE_CON_0, value);
}

void rdma_disable_color_transform(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;
	UINT32 value = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset);

	value = value & ~(REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_EXT_MTX_EN), 1) |
			  REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_ENABLE), 1));
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_SIZE_CON_0, value);
}

void rdma_set_color_matrix(enum DISP_MODULE_ENUM module,
			   struct rdma_color_matrix *matrix,
			   struct rdma_color_pre *pre,
			   struct rdma_color_post *post)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;

	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C00, matrix->C00);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C01, matrix->C01);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C02, matrix->C02);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C10, matrix->C10);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C11, matrix->C11);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C12, matrix->C12);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C20, matrix->C20);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C21, matrix->C21);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_C22, matrix->C22);

	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_PRE_ADD_0, pre->ADD0);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_PRE_ADD_1, pre->ADD1);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_PRE_ADD_2, pre->ADD2);

	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_POST_ADD_0, post->ADD0);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_POST_ADD_1, post->ADD1);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_POST_ADD_2, post->ADD2);
}

static int _rdma_partial_update(enum DISP_MODULE_ENUM module, void *arg,
				void *handle)
{
	struct disp_rect *roi = (struct disp_rect *)arg;
	int width = roi->width;
	int height = roi->height;
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;

	DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH,
			   offset + DISP_REG_RDMA_SIZE_CON_0, width);
	DISP_REG_SET_FIELD(handle, SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT,
			   offset + DISP_REG_RDMA_SIZE_CON_1, height);
	return 0;
}

int rdma_ioctl(enum DISP_MODULE_ENUM module, void *cmdq_handle,
	       unsigned int ioctl_cmd, unsigned long *params)
{
	int ret = 0;
	enum DDP_IOCTL_NAME ioctl = (enum DDP_IOCTL_NAME)ioctl_cmd;
	unsigned int idx = rdma_index(module);

	switch (ioctl) {
	case DDP_RDMA_GOLDEN_SETTING:
	{
		struct disp_ddp_path_config *pConfig;
		struct golden_setting_context *p_golden_setting;

		pConfig = (struct disp_ddp_path_config *)params;
		p_golden_setting = pConfig->p_golden_setting_context;
		rdma_set_ultra_l(idx, pConfig->lcm_bpp, cmdq_handle,
				 p_golden_setting);
		break;
	}
	case DDP_PARTIAL_UPDATE:
		_rdma_partial_update(module, params, cmdq_handle);
		break;
	default:
		break;
	}

	return ret;
}

static int rdma_build_cmdq(enum DISP_MODULE_ENUM module, void *handle,
			   enum CMDQ_STATE state)
{
	if (handle == NULL) {
		DDP_PR_ERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}
	if (state == CMDQ_RESET_AFTER_STREAM_EOF) {
		/*
		 * if RDMA frame done with underflow,
		 * rdma will hold dvfs request forever
		 * we reset here to solve this issue
		 */
		rdma_reset_by_cmdq(module, handle);
	}

	return 0;
}

/* for mmpath */
inline bool MMPathIsPrimaryDL(void)
{
	return (rdma_golden_setting->is_dc == 0);
}

unsigned int MMPathTracePrimaryRDMA(char *str, unsigned int strlen,
	unsigned int n)
{
	n += scnprintf(str + n, strlen - n,
		"in=0x%lx, in_width=%d, in_height=%d, in_fmt=%s, in_Bpp=%u, ",
		g_primary_rdma_cfg.address,
		g_primary_rdma_cfg.width,
		g_primary_rdma_cfg.height,
		unified_color_fmt_name(g_primary_rdma_cfg.inputFormat),
		ufmt_get_Bpp(g_primary_rdma_cfg.inputFormat));

	return n;
}

struct DDP_MODULE_DRIVER ddp_driver_rdma = {
	.init = rdma_init,
	.deinit = rdma_deinit,
	.config = rdma_config_l,
	.start = rdma_start,
	.trigger = NULL,
	.stop = rdma_stop,
	.reset = rdma_reset,
	.power_on = rdma_clock_on,
	.power_off = rdma_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = rdma_dump,
	.bypass = NULL,
	.build_cmdq = rdma_build_cmdq,
	.set_lcm_utils = NULL,
	.enable_irq = rdma_enable_irq,
	.ioctl = (int (*)(enum DISP_MODULE_ENUM, void *,
			  enum DDP_IOCTL_NAME, void *))rdma_ioctl,
	.switch_to_nonsec = NULL,
};
