/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include "debug.h"

#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)

static unsigned int rdma_fps[RDMA_INSTANCES] = { 60, 60 };
static struct golden_setting_context *rdma_golden_setting;

/*****************************************************************************/
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
		DDPERR("invalid rdma module=%d\n", module); /* invalid module */
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

	DISP_REG_SET(handle,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_TARGET_LINE,
		line);
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

	*addr = DISP_REG_GET(
		DISP_REG_RDMA_MEM_START_ADDR + DISP_RDMA_INDEX_OFFSET * idx);
}

/*****************************************************************************/

static inline unsigned long rdma_to_cmdq_engine(enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_RDMA0:
		return CMDQ_ENG_DISP_RDMA0;
	case DISP_MODULE_RDMA1:
		return CMDQ_ENG_DISP_RDMA1;
	default:
		DDPERR("invalid rdma module=%d,rdma to cmdq engine fail\n",
			module);
		ASSERT(0);
		return DISP_MODULE_UNKNOWN;
	}
	return 0;
}

static inline unsigned long rdma_to_cmdq_event_nonsec_end(
	enum DISP_MODULE_ENUM module)
{
	switch (module) {
	case DISP_MODULE_RDMA0:
		return CMDQ_SYNC_DISP_RDMA0_2NONSEC_END;
	case DISP_MODULE_RDMA1:
		return CMDQ_SYNC_DISP_RDMA1_2NONSEC_END;
	default:
		DDPERR("invalid rdma module=%d,rmda to cmdq event fail\n",
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
		DISP_REG_SET(handle,
			idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE,
			0x1E);
		break;
	case DDP_IRQ_LEVEL_ERROR:
		DISP_REG_SET(handle,
			idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE,
			0x18);
		break;
	case DDP_IRQ_LEVEL_NONE:
		DISP_REG_SET(handle,
			idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE,
			0x0);
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

	ASSERT(idx <= RDMA_INSTANCES);

	regval = REG_FLD_VAL(INT_STATUS_FLD_REG_UPDATE_INT_FLAG, 0) |
		REG_FLD_VAL(INT_STATUS_FLD_FRAME_START_INT_FLAG, 1) |
		REG_FLD_VAL(INT_STATUS_FLD_FRAME_END_INT_FLAG, 1) |
		REG_FLD_VAL(INT_STATUS_FLD_EOF_ABNORMAL_INT_FLAG, 1) |
		REG_FLD_VAL(INT_STATUS_FLD_FIFO_UNDERFLOW_INT_FLAG, 1) |
		REG_FLD_VAL(INT_STATUS_FLD_TARGET_LINE_INT_FLAG, 0) |
		REG_FLD_VAL(INT_STATUS_FLD_FIFO_EMPTY_INT_FLAG, 0);

	DISP_REG_SET(handle,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE,
		regval);
	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_ENGINE_EN,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 1);

	return 0;
}

int rdma_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = rdma_index(module);

	ASSERT(idx <= RDMA_INSTANCES);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_ENGINE_EN,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 0);
	DISP_REG_SET(handle,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_INT_ENABLE, 0);
	DISP_REG_SET(handle,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_INT_STATUS, 0);
	return 0;
}

int rdma_reset_by_cmdq(enum DISP_MODULE_ENUM module, void *handle)
{
	int ret = 0;
	unsigned int idx = rdma_index(module);

	ASSERT(idx <= RDMA_INSTANCES);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 1);

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON, 0);

	DISP_REG_CMDQ_POLLING(handle,
		idx * DISP_RDMA_INDEX_OFFSET + DISP_REG_RDMA_GLOBAL_CON,
		0x100, 0x700);

	return ret;
}

int rdma_reset(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int delay_cnt = 0;
	int ret = 0, offset = 0;
	unsigned int idx = rdma_index(module);

	ASSERT(idx <= RDMA_INSTANCES);

	offset = idx * DISP_RDMA_INDEX_OFFSET;
	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET,
		offset + DISP_REG_RDMA_GLOBAL_CON, 1);
	while ((DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON) & 0x700) ==
			0x100) {
		delay_cnt++;
		udelay(10);
		if (delay_cnt > 10000) {
			ret = -1;
			DDPERR(
				"rdma%d_reset timeout, stage 1! RDMA_GLO_CON_REG=0x%x\n",
			idx,
			DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON));
			break;
		}
	}
	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_SOFT_RESET,
			   offset + DISP_REG_RDMA_GLOBAL_CON, 0);
	delay_cnt = 0;
	while ((DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON) & 0x700) !=
			0x100) {
		delay_cnt++;
		udelay(10);
		if (delay_cnt <= 10000)
			continue;

		ret = -1;
		DDPERR("rdma%d_reset timeout, stage 2! RDMA_GLO_CON_REG=0x%x\n",
			idx,
			DISP_REG_GET(offset + DISP_REG_RDMA_GLOBAL_CON));
		break;
	}
	return ret;
}

#if 0
/* old from vinson */
/* set ultra registers */
void rdma_set_ultra_l(unsigned int idx, unsigned int bpp, void *handle,
	struct golden_setting_context *p_golden_setting)
{
	/* rdma golden setting variables */
	unsigned int mmsysclk = 230;
	unsigned int is_wrot_sram = 0;
	unsigned int is_rsz_sram = 0;
	unsigned int fifo_mode = 1;

	unsigned int ultra_low_us = 4;
	unsigned int ultra_high_us = 6;
	unsigned int preultra_low_us = ultra_high_us;
	unsigned int preultra_high_us = 7;
/*
 *
 *	unsigned int urgent_low_us = 4;
 *	unsigned int urgent_high_us = 45;  10 times
 */

	unsigned long long fill_rate = 0;
	unsigned long long consume_rate = 0;

	unsigned int fifo_valid_size = 384;
	unsigned int fifo_off_drs_enter = 0;
	unsigned int fifo_off_drs_leave = 0;
	unsigned int fifo_off_spm = 0; /* SPM latency */
	unsigned int fifo_off_dvfs = 0;

	/* working variables */
	unsigned int preultra_low;
	unsigned int preultra_high;
	unsigned int ultra_low;
	unsigned int ultra_high;

	unsigned int drs_enter = 0;
	unsigned int drs_leave = 0;

	unsigned int issue_req_threshold;
	unsigned int output_valid_fifo_threshold;

	unsigned int sodi_threshold_high;
	unsigned int sodi_threshold_low;
	unsigned int dvfs_threshold_high;
	unsigned int dvfs_threshold_low;

	unsigned int frame_rate;
	unsigned int Bytes_per_sec;
	unsigned int offset;
	long long temp;
	long long temp_for_div;

	if (!p_golden_setting) {
		DDPERR("golden setting is null, %s,%d\n", __FILE__, __LINE__);
		ASSERT(0);
		return;
	}
	rdma_golden_setting = p_golden_setting;

	frame_rate = 60;
	if (idx == 1) {
		/* hardcode bpp & frame_rate for rdma1 */
		bpp = 24;
		frame_rate = 60;

		if ((rdma_golden_setting->ext_dst_width == 1920) &&
		    (rdma_golden_setting->ext_dst_height == 1080))
			frame_rate = 30;
		if ((rdma_golden_setting->ext_dst_width == 3840) &&
		    (rdma_golden_setting->ext_dst_height == 2160))
			frame_rate = 30;
	}

	/* get fifo parameters */
	switch (rdma_golden_setting->mmsys_clk) {
	case MMSYS_CLK_LOW:
		mmsysclk = 230;
		break;
	case MMSYS_CLK_HIGH:
		mmsysclk = 457;
		break;
	default:
		mmsysclk = 230; /* worse case */
		break;
	}

	Bytes_per_sec = bpp / 8;
	if (!Bytes_per_sec) {
		DDPERR("bpp is invalid, bpp=%d\n", bpp);
		return;
	}

	is_wrot_sram = rdma_golden_setting->is_wrot_sram;
	is_rsz_sram = rdma_golden_setting->is_rsz_sram;
	fifo_mode = rdma_golden_setting->fifo_mode;

	if (rdma_golden_setting->is_dc)
		fill_rate = 960 * mmsysclk; /* FIFO depth / us */
	else
		fill_rate = 960 * mmsysclk * 3 / 16; /* FIFO depth / us */

	if (idx == 0) {
		/* only for offset */
		fifo_off_drs_enter = 4;
		fifo_off_drs_leave = 2;
		fifo_off_spm = 50; /* 10 times */
		fifo_off_dvfs = 4;
		consume_rate = rdma_golden_setting->dst_width *
				rdma_golden_setting->dst_height *
				frame_rate * Bytes_per_sec;
		do_div(consume_rate, 1000);
	} else {
		fifo_off_drs_enter = 0;
		fifo_off_drs_leave = 0;
		fifo_off_spm = 14; /* 10 times */
		fifo_off_dvfs = 2;

		consume_rate = rdma_golden_setting->ext_dst_width *
				rdma_golden_setting->ext_dst_height *
				frame_rate * Bytes_per_sec;
		do_div(consume_rate, 1000);
	}
	consume_rate *= 1250;
	do_div(consume_rate, 16000/* 16 * 1000 */);

	preultra_low = (preultra_low_us + fifo_off_drs_enter) * consume_rate;
	preultra_low = DIV_ROUND_UP(preultra_low, 1000);

	preultra_high = (preultra_high_us + fifo_off_drs_enter) * consume_rate;
	preultra_high = DIV_ROUND_UP(preultra_high, 1000);

	ultra_low = (ultra_low_us + fifo_off_drs_enter) * consume_rate;
	ultra_low = DIV_ROUND_UP(ultra_low, 1000);

	ultra_high = preultra_low;

	if (idx == 0) {
		/* only rdma0 can share sram */
		if (is_wrot_sram)
			fifo_valid_size = 2048;
		else if (is_rsz_sram)
			fifo_valid_size = 736;
		else
			fifo_valid_size = 384;
	} else {
		fifo_valid_size = 384;
	}

	issue_req_threshold =
		min(fifo_valid_size - preultra_low, 256U);

	/* output valid should < total rdma data size, or hang will happen */
	temp = rdma_golden_setting->rdma_width *
		rdma_golden_setting->rdma_height * Bytes_per_sec;
	do_div(temp, 16);
	temp -= 1;
	output_valid_fifo_threshold = min(((long long)preultra_low), temp);

	/* SODI threshold */
	sodi_threshold_low = (ultra_low_us * 10 + fifo_off_spm) * consume_rate;
	sodi_threshold_low = DIV_ROUND_UP(sodi_threshold_low, 10000);

	temp_for_div = 1200 * (fill_rate - consume_rate);
	WARN_ON(temp_for_div < 0);
	do_div(temp_for_div, 1000000);
	temp = fifo_valid_size - temp_for_div;
	if (temp < 0)
		sodi_threshold_high = preultra_high;
	else
		sodi_threshold_high = max(((long long)preultra_high), temp);

	dvfs_threshold_low = preultra_low;
	dvfs_threshold_high = preultra_low + 1;

	offset = idx * DISP_RDMA_INDEX_OFFSET;
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_0,
		preultra_low | (preultra_high << 16));

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_1,
		ultra_low | (ultra_high << 16));

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_GMC_SETTING_2,
		issue_req_threshold);

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_FIFO_CON,
		REG_FLD_VAL(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD,
			output_valid_fifo_threshold) |
		REG_FLD_VAL(FIFO_CON_FLD_FIFO_PSEUDO_SIZE, fifo_valid_size) |
		REG_FLD_VAL(FIFO_CON_FLD_FIFO_UNDERFLOW_EN, 1));

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_THRESHOLD_FOR_SODI,
		sodi_threshold_low | (sodi_threshold_high << 16));

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_THRESHOLD_FOR_DVFS,
		dvfs_threshold_low | (dvfs_threshold_high << 16));

	/* DISP_RDMA_DVFS_SETTING_PREULTRA */
	preultra_low = (preultra_low_us + fifo_off_dvfs) * consume_rate;
	preultra_low = DIV_ROUND_UP(preultra_low, 1000);

	preultra_high = (preultra_high_us + fifo_off_dvfs) * consume_rate;
	preultra_high = DIV_ROUND_UP(preultra_high, 1000);

	ultra_low = (ultra_low_us + fifo_off_dvfs) * consume_rate;
	ultra_low = DIV_ROUND_UP(ultra_low, 1000);

	ultra_high = preultra_low;

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_DVFS_SETTING_PRE,
		preultra_low | (preultra_high << 16));

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_DVFS_SETTING_ULTRA,
		ultra_low | (ultra_high << 16));

	/* DISP_REG_RDMA_LEAVE_DRS_SETTING */
	drs_enter = (preultra_low_us + fifo_off_drs_enter) * consume_rate;
	drs_enter = DIV_ROUND_UP(drs_enter, 1000);

	drs_leave = (preultra_low_us + fifo_off_drs_leave) * consume_rate;
	drs_leave = DIV_ROUND_UP(drs_leave, 1000);

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_LEAVE_DRS_SETTING,
		     drs_leave | (drs_leave << 16));

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_ENTER_DRS_SETTING,
		     drs_enter | (drs_enter << 16));

#if 0
	if (idx == 0)
		rdma_dump_golden_setting_context(DISP_MODULE_RDMA0);
	else
		rdma_dump_golden_setting_context(DISP_MODULE_RDMA1);
#endif

	if (rdma_golden_setting->dst_width == 0 ||
		rdma_golden_setting->dst_height == 0 ||
	    bpp == 0 || frame_rate == 0) {
		DDPDUMP("==RDMA Golden Setting Value=============\n");

		DDPDUMP("width		= %d\n",
			rdma_golden_setting->dst_width);
		DDPDUMP("height		= %d\n",
			rdma_golden_setting->dst_height);
		DDPDUMP("bpp		= %d\n", bpp);
		DDPDUMP("frame_rate	= %d\n", frame_rate);

		DDPDUMP("fill_rate	= %lld\n", fill_rate);
		DDPDUMP("consume_rate	= %lld\n", consume_rate);
		DDPDUMP("ultra_low_us	= %d\n", ultra_low_us);
		DDPDUMP("ultra_high_us	= %d\n", ultra_high_us);
		DDPDUMP("preultra_high_us= %d\n", preultra_high_us);

		DDPDUMP("preultra_low	= %d\n", preultra_low);
		DDPDUMP("preultra_high	= %d\n", preultra_high);
		DDPDUMP("ultra_low	= %d\n", ultra_low);
		DDPDUMP("issue_req_threshold		= %d\n",
			issue_req_threshold);
		DDPDUMP("output_valid_fifo_threshold	= %d\n",
			output_valid_fifo_threshold);
		DDPDUMP("sodi_threshold_low	= %d\n", sodi_threshold_low);
		DDPDUMP("sodi_threshold_high	= %d\n", sodi_threshold_high);
		DDPDUMP("dvfs_threshold_low	= %d\n", dvfs_threshold_low);
		DDPDUMP("dvfs_threshold_high	= %d\n", dvfs_threshold_high);
	}
}

#else
/* new from bianco */
/* set ultra registers */
void rdma_set_ultra_l(unsigned int idx, unsigned int bpp, void *handle,
	struct golden_setting_context *p_golden_setting)
{

	/* rdma golden setting variables */
	unsigned int mmsysclk = 230;
	unsigned int is_wrot_sram = 0;
	unsigned int is_rsz_sram = 0;
	unsigned int fifo_mode = 1;

	unsigned int ultra_low_us = 4;
	unsigned int ultra_high_us = 6;
	unsigned int preultra_low_us = ultra_high_us;
	unsigned int preultra_high_us = 7;
	unsigned int urgent_low_us = 30;  /* 10 times */
	unsigned int urgent_high_us = 35;  /* 10 times */

	unsigned long long fill_rate = 0;
	unsigned long long consume_rate = 0;
	unsigned long long consume_rate_div_tmp = 0;
	unsigned long long consume_rate_div = 0;
	unsigned int fifo_valid_size = 384;
	unsigned int fifo_off_drs_enter = 0;
	unsigned int fifo_off_drs_leave = 0;
	unsigned int fifo_off_spm = 0;  /*SPM latency*/
	unsigned int fifo_off_dvfs = 0;
	unsigned int fifo_off_ultra = 0;

	/* working variables */
	unsigned int preultra_low;
	unsigned int preultra_high;
	unsigned int ultra_low;
	unsigned int ultra_high;
	unsigned int urgent_low;
	unsigned int urgent_high;
	unsigned int dvfs_preultra_low;
	unsigned int dvfs_preultra_high;
	unsigned int dvfs_ultra_low;
	unsigned int dvfs_ultra_high;
	unsigned int drs_enter = 0;
	unsigned int drs_leave = 0;

	unsigned int issue_req_threshold;
	unsigned int output_valid_fifo_threshold;

	unsigned int sodi_threshold_high;
	unsigned int sodi_threshold_low;
	unsigned int dvfs_threshold_high;
	unsigned int dvfs_threshold_low;

	unsigned int frame_rate;
	unsigned int Bytes_per_sec;
	unsigned long long temp;
	unsigned long long temp_for_div;

	/* setup threshold for debug */
	if (dbg_ultlow)
		ultra_low_us = dbg_ultlow;
	if (dbg_ulthigh) {
		ultra_high_us = dbg_ulthigh;
		preultra_low_us = ultra_high_us;
	}
	if (dbg_prehigh)
		preultra_high_us = dbg_prehigh;
	if (dbg_urg_low)
		urgent_low_us = dbg_urg_low;
	if (dbg_urg_high)
		urgent_high_us = dbg_urg_high;

	if (!p_golden_setting) {
		DDPERR("golden setting is null, %s,%d\n", __FILE__, __LINE__);
		ASSERT(0);
		return;
	}
	rdma_golden_setting = p_golden_setting;

	frame_rate = rdma_golden_setting->fps;
	if (idx == 1) {
		/* hardcode bpp & frame_rate for rdma1 */
		bpp = 24;
		frame_rate = 60;

		if ((rdma_golden_setting->ext_dst_width == 1920) &&
				(rdma_golden_setting->ext_dst_height == 1080))
			frame_rate = 30;

		if ((rdma_golden_setting->ext_dst_width == 3840) &&
				(rdma_golden_setting->ext_dst_height == 2160))
			frame_rate = 30;
	}

	/* get fifo parameters */
	switch (rdma_golden_setting->mmsys_clk) {
	case MMSYS_CLK_LOW:
		mmsysclk = 230;
		break;
	case MMSYS_CLK_HIGH:
		mmsysclk = 457;
		break;
	default:
		mmsysclk = 230; /* worse case */
		break;
	}

	Bytes_per_sec = bpp / 8;
	if (!Bytes_per_sec) {
		DDPERR("bpp is invalid, bpp=%d\n", bpp);
		return;
	}

	is_wrot_sram = rdma_golden_setting->is_wrot_sram;
	is_rsz_sram = rdma_golden_setting->is_rsz_sram;
	fifo_mode = rdma_golden_setting->fifo_mode;


	if (rdma_golden_setting->is_dc)
		fill_rate = 960 * mmsysclk; /* FIFO depth / us  */
	else
		fill_rate = 960 * mmsysclk * 3 / 16; /* FIFO depth / us  */

	do_div(fill_rate, 100);
	fill_rate = DIV_ROUND_UP((unsigned int)fill_rate, 10);

	if (idx == 0) {
		/* only for offset */
		fifo_off_drs_enter = 4;
		fifo_off_drs_leave = 1;
		fifo_off_spm = 50; /* 10 times*/
		fifo_off_dvfs = 2;
		if (is_wrot_sram)
			fifo_off_ultra = 50;
		else if (is_rsz_sram)
			fifo_off_ultra = 10;
		else
			fifo_off_ultra = 0;
		consume_rate = rdma_golden_setting->dst_width;
		consume_rate = consume_rate * rdma_golden_setting->dst_height
				*frame_rate * Bytes_per_sec;
		do_div(consume_rate, 1000);

	} else {
		fifo_off_drs_enter = 0;
		fifo_off_drs_leave = 0;
		fifo_off_spm = 14; /* 10 times*/
		fifo_off_dvfs = 2;
		fifo_off_ultra = 0;
		consume_rate = rdma_golden_setting->ext_dst_width;
		consume_rate = consume_rate *
				rdma_golden_setting->ext_dst_height
				* frame_rate * Bytes_per_sec;

		do_div(consume_rate, 1000);
	}
	consume_rate *= 1250;
	do_div(consume_rate, 16*1000);
	consume_rate_div_tmp = consume_rate;
	do_div(consume_rate_div_tmp, 100);
	consume_rate_div = DIV_ROUND_UP((unsigned int)consume_rate_div_tmp, 10);

	preultra_low = (preultra_low_us + fifo_off_ultra) * consume_rate_div;

	preultra_high = (preultra_high_us + fifo_off_ultra) * consume_rate_div;

	ultra_low = (ultra_low_us + fifo_off_ultra) * consume_rate_div;

	ultra_high = preultra_low;
	if (idx == 0) {
		/* only rdma0 can share sram */
		if (is_wrot_sram)
			fifo_valid_size = 2048;
		else if (is_rsz_sram)
			fifo_valid_size = 736;
		else
			fifo_valid_size = 384;
	} else {
		fifo_valid_size = 128;
	}
	issue_req_threshold =
		(fifo_valid_size - preultra_low) < 255
				? (fifo_valid_size - preultra_low) : 255;


	/* output valid should < total rdma data size, or hang will happen */
	temp = rdma_golden_setting->rdma_width;
	temp = temp * rdma_golden_setting->rdma_height * Bytes_per_sec;
	do_div(temp, 16);
	temp -= 1;
#if 0
	output_valid_fifo_threshold =
		(preultra_low_us * consume_rate_div) < temp
				? (preultra_low_us * consume_rate_div) : temp;
#else
	output_valid_fifo_threshold =
				preultra_low < temp
						? preultra_low : temp;
#endif

	/* SODI threshold */
	sodi_threshold_low = (ultra_low_us * 10 + fifo_off_spm)
							* consume_rate_div;
	sodi_threshold_low = DIV_ROUND_UP(sodi_threshold_low, 10);

	temp_for_div = 5000 * (fill_rate - consume_rate_div);
	WARN_ON((long long)temp_for_div < 0);
	do_div(temp_for_div, 100);
	temp_for_div = DIV_ROUND_UP((unsigned int)temp_for_div, 10);
	temp = (long long)fifo_valid_size - temp_for_div;

	if ((long long)temp < 0)
		sodi_threshold_high = preultra_high;
	else
		sodi_threshold_high =
			preultra_high > temp ? preultra_high : temp;

	dvfs_threshold_low = preultra_low;
	dvfs_threshold_high = preultra_low;

	if (primary_display_is_video_mode()) {
		/* video mode*/
		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			DISP_REG_RDMA_MEM_GMC_SETTING_0,
			preultra_low | (preultra_high << 16) |
			REG_FLD_VAL(
			MEM_GMC_SETTING_0_FLD_RG_VALID_THRESHOLD_FORCE_PREULTRA,
							0) |
			REG_FLD_VAL(MEM_GMC_SETTING_0_FLD_RG_VDE_FORCE_PREULTRA,
							1));

		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			DISP_REG_RDMA_MEM_GMC_SETTING_1,
			ultra_low | (ultra_high << 16) |
			REG_FLD_VAL(
			MEM_GMC_SETTING_1_FLD_RG_VALID_THRESHOLD_BLOCK_ULTRA,
							0) |
			REG_FLD_VAL(MEM_GMC_SETTING_1_FLD_RG_VDE_BLOCK_ULTRA,
							1));

		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			DISP_REG_RDMA_MEM_GMC_SETTING_2,
			issue_req_threshold);
	} else {
		/* cmd mode */
		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			DISP_REG_RDMA_MEM_GMC_SETTING_0,
			preultra_low | (preultra_high << 16) |
			REG_FLD_VAL(
			MEM_GMC_SETTING_0_FLD_RG_VALID_THRESHOLD_FORCE_PREULTRA,
							1) |
			REG_FLD_VAL(MEM_GMC_SETTING_0_FLD_RG_VDE_FORCE_PREULTRA,
							0));

		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			DISP_REG_RDMA_MEM_GMC_SETTING_1,
			ultra_low | (ultra_high << 16) |
			REG_FLD_VAL(
			MEM_GMC_SETTING_1_FLD_RG_VALID_THRESHOLD_BLOCK_ULTRA,
							1) |
			REG_FLD_VAL(MEM_GMC_SETTING_1_FLD_RG_VDE_BLOCK_ULTRA,
							0));

		DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
			DISP_REG_RDMA_MEM_GMC_SETTING_2,
			issue_req_threshold);
	}

	/* In video mode, output_valid_fifo_threshold = 0 */
	if (primary_display_is_video_mode())
		output_valid_fifo_threshold = 0;

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_FIFO_CON,
		REG_FLD_VAL(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD,
			output_valid_fifo_threshold) |
		REG_FLD_VAL(FIFO_CON_FLD_FIFO_PSEUDO_SIZE,
			fifo_valid_size) |
		REG_FLD_VAL(FIFO_CON_FLD_FIFO_UNDERFLOW_EN,
			1));

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_THRESHOLD_FOR_SODI,
		sodi_threshold_low | (sodi_threshold_high << 16));

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_THRESHOLD_FOR_DVFS,
		dvfs_threshold_low | (dvfs_threshold_high << 16));

	/*DISP_REG_RDMA_DRAM_CON*/
	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_DRAM_CON,
		REG_FLD_VAL(DRAM_CON_FLD_FORCE_GCLAST_0, 0) |
		REG_FLD_VAL(DRAM_CON_FLD_BANK_BOUNDARY_SEL, 1));

	/*DISP_RDMA_DVFS_SETTING_PREULTRA*/
	dvfs_preultra_low = (preultra_low_us + fifo_off_ultra + fifo_off_dvfs)
						* consume_rate_div;

	dvfs_preultra_high = (preultra_high_us + fifo_off_ultra + fifo_off_dvfs)
						* consume_rate_div;

	dvfs_ultra_low = (ultra_low_us + fifo_off_ultra + fifo_off_dvfs)
						* consume_rate_div;

	dvfs_ultra_high = dvfs_preultra_low;
	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_DVFS_SETTING_PRE,
		dvfs_preultra_low | (dvfs_preultra_high << 16));

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_DVFS_SETTING_ULTRA,
		dvfs_ultra_low | (dvfs_ultra_high << 16));

	/*DISP_REG_RDMA_LEAVE_DRS_SETTING*/
	urgent_low = urgent_low_us * consume_rate_div;
	urgent_low = DIV_ROUND_UP(urgent_low, 10);

	urgent_high = urgent_high_us * consume_rate_div;
	urgent_high = DIV_ROUND_UP(urgent_high, 10);

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_LEAVE_DRS_SETTING,
		urgent_low | (urgent_high << 16));

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_ENTER_DRS_SETTING,
		urgent_low | (urgent_high << 16));

	DISP_REG_SET(handle, idx * DISP_RDMA_INDEX_OFFSET +
		DISP_REG_RDMA_MEM_GMC_SETTING_3,
		urgent_low | (urgent_high << 16));

	/* only config RDMA0 SRAM_SEL */
	if (idx == 0) {
		if (is_wrot_sram)
			DISP_REG_SET(handle, DISP_REG_RDMA_SRAM_SEL, 1);
		else if (is_rsz_sram)
			DISP_REG_SET(handle, DISP_REG_RDMA_SRAM_SEL, 2);
		else
			DISP_REG_SET(handle, DISP_REG_RDMA_SRAM_SEL, 0);
	}

#if 0
	if (idx == 0)
		rdma_dump_golden_setting_context(DISP_MODULE_RDMA0);
	else
		rdma_dump_golden_setting_context(DISP_MODULE_RDMA1);
#endif
	/* dump golden settings info */
	{
		DDPDBG("==RDMA Golden Setting Value=============\n");
		DDPDBG("width		= %d\n",
			rdma_golden_setting->dst_width);
		DDPDBG("height		= %d\n",
			rdma_golden_setting->dst_height);
		DDPDBG("bpp		= %d\n", bpp);
		DDPDBG("frame_rate	= %d\n", frame_rate);
		DDPDBG("fill_rate	= %lld\n", fill_rate);
		DDPDBG("consume_rate	= %lld\n", consume_rate);
		DDPDBG("ultra_low_us	= %d\n", ultra_low_us);
		DDPDBG("ultra_high_us	= %d\n", ultra_high_us);
		DDPDBG("preultra_high_us = %d\n", preultra_high_us);
		DDPDBG("urgent_low_us = %d\n", urgent_low_us);
		DDPDBG("urgent_high_us = %d\n", urgent_high_us);
		DDPDBG("preultra_threshold_low	 = %d\n", preultra_low);
		DDPDBG("preultra_threshold_high = %d\n", preultra_high);
		DDPDBG("ultra_threshold_low	= %d\n", ultra_low);
		DDPDBG("ultra_threshold_high	= %d\n", ultra_low);
		DDPDBG("issue_req_threshold	= %d\n", issue_req_threshold);
		DDPDBG("output_valid_fifo_threshold = %d\n",
					output_valid_fifo_threshold);
		DDPDBG("fifo_valid_size	 = %d\n", fifo_valid_size);
		DDPDBG("sodi_threshold_low	 = %d\n", sodi_threshold_low);
		DDPDBG("sodi_threshold_high = %d\n", sodi_threshold_high);
		DDPDBG("dvfs_threshold_low	 = %d\n", dvfs_threshold_low);
		DDPDBG("dvfs_threshold_high = %d\n", dvfs_threshold_high);
		DDPDBG("dvfs_preultra_low	= %d\n", dvfs_preultra_low);
		DDPDBG("dvfs_preultra_high	= %d\n", dvfs_preultra_high);
		DDPDBG("dvfs_ultra_low	 = %d\n", dvfs_ultra_low);
		DDPDBG("dvfs_ultra_high = %d\n", dvfs_ultra_high);
		DDPDBG("drs_enter	= %d\n", drs_enter);
		DDPDBG("drs_leave	= %d\n", drs_leave);
		DDPDBG("urgent_low	= %d\n", urgent_low);
		DDPDBG("urgent_high	= %d\n", urgent_high);
	}
}

#endif

static int rdma_config(enum DISP_MODULE_ENUM module, enum RDMA_MODE mode,
	unsigned long address, enum UNIFIED_COLOR_FMT inFormat,
	unsigned int pitch, unsigned int width, unsigned int height,
	unsigned int ufoe_enable, enum DISP_BUFFER_TYPE sec,
	unsigned int yuv_range, struct rdma_bg_ctrl_t *bg_ctrl,
	void *handle, struct golden_setting_context *p_golden_setting,
	unsigned int bpp)
{
	unsigned int output_is_yuv = 0;
	unsigned int input_is_yuv = !UFMT_GET_RGB(inFormat);
	unsigned int input_swap = UFMT_GET_BYTESWAP(inFormat);
	unsigned int input_format_reg = UFMT_GET_FORMAT(inFormat);
	unsigned int idx = rdma_index(module);
	unsigned int color_matrix;
	unsigned int regval, offset;

	DDPDBG("%s:%s,mode:%s,addr:0x%lx,fmt:%s,pitch:%u,wxh(%ux%u),sec:%d\n",
		__func__, ddp_get_module_name(module), mode ?	"MEM" : "DL",
		address, unified_color_fmt_name(inFormat), pitch,
		width, height, sec);

	ASSERT(idx <= RDMA_INSTANCES);
	if ((width > RDMA_MAX_WIDTH) || (height > RDMA_MAX_HEIGHT))
		DDPERR("RDMA input overflow, w=%d, h=%d, max_w=%d, max_h=%d\n",
			width, height, RDMA_MAX_WIDTH, RDMA_MAX_HEIGHT);

	offset = idx * DISP_RDMA_INDEX_OFFSET;
	if (input_is_yuv == 1 && output_is_yuv == 0) {
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
			DDPERR("%s,un-recognized yuv_range=%d!\n",
				__func__, yuv_range);
			color_matrix = 4;
			break;
		}

		DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_MATRIX_ENABLE,
			offset + DISP_REG_RDMA_SIZE_CON_0, 1);
		DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL,
			offset + DISP_REG_RDMA_SIZE_CON_0, color_matrix);
	} else if (input_is_yuv == 0 && output_is_yuv == 1) {
		color_matrix = 0x2; /* 0x0010, RGB_TO_BT601 */
		DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_MATRIX_ENABLE,
			offset + DISP_REG_RDMA_SIZE_CON_0, 1);
		DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL,
			offset + DISP_REG_RDMA_SIZE_CON_0, color_matrix);
	} else {
		DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_MATRIX_ENABLE,
			offset + DISP_REG_RDMA_SIZE_CON_0, 0);
		DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_MATRIX_INT_MTX_SEL,
			offset + DISP_REG_RDMA_SIZE_CON_0, 0);
	}

	DISP_REG_SET_FIELD(handle, GLOBAL_CON_FLD_MODE_SEL,
		offset + DISP_REG_RDMA_GLOBAL_CON, mode);
	DISP_REG_SET_FIELD(handle, MEM_CON_FLD_MEM_MODE_INPUT_FORMAT,
		offset + DISP_REG_RDMA_MEM_CON,
		(mode == RDMA_MODE_DIRECT_LINK) ? 0 : input_format_reg & 0xf);
	DISP_REG_SET_FIELD(handle, MEM_CON_FLD_MEM_MODE_INPUT_SWAP,
		offset + DISP_REG_RDMA_MEM_CON,
		((mode == RDMA_MODE_DIRECT_LINK) ? 0 : input_swap));

	if (sec != DISP_SECURE_BUFFER) {
		DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_START_ADDR,
			     address);
	} else {
		int m4u_port;
		unsigned int size = pitch * height;

		m4u_port = DISP_M4U_PORT_DISP_RDMA0;
		/*
		 * for sec layer, addr variable stores sec handle
		 * we need to pass this handle and offset to cmdq driver
		 * cmdq sec driver will help to convert handle to
		 * correct address
		 */
		cmdqRecWriteSecure(handle,
			disp_addr_convert(
				offset + DISP_REG_RDMA_MEM_START_ADDR),
			CMDQ_SAM_H_2_MVA, address, 0, size, m4u_port);
	}

	DISP_REG_SET(handle, offset + DISP_REG_RDMA_MEM_SRC_PITCH, pitch);
	DISP_REG_SET_FIELD(handle, SIZE_CON_0_FLD_OUTPUT_FRAME_WIDTH,
		offset + DISP_REG_RDMA_SIZE_CON_0, width);
	DISP_REG_SET_FIELD(handle, SIZE_CON_1_FLD_OUTPUT_FRAME_HEIGHT,
		offset + DISP_REG_RDMA_SIZE_CON_1, height);

	/* rdma bg control */
	regval = REG_FLD_VAL(RDMA_BG_CON_0_LEFT, bg_ctrl->left);
	regval |= REG_FLD_VAL(RDMA_BG_CON_0_RIGHT, bg_ctrl->right);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_BG_CON_0, regval);

	regval = REG_FLD_VAL(RDMA_BG_CON_1_TOP, bg_ctrl->top);
	regval |= REG_FLD_VAL(RDMA_BG_CON_1_BOTTOM, bg_ctrl->bottom);
	DISP_REG_SET(handle, offset + DISP_REG_RDMA_BG_CON_1, regval);

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

	DDPDUMP("-- RDMA Golden Setting Context --\n");
	DDPDUMP("fifo_mode=%d\n", rdma_golden_setting->fifo_mode);
	DDPDUMP("hrt_num=%d\n", rdma_golden_setting->hrt_num);
	DDPDUMP("is_display_idle=%d\n",
		rdma_golden_setting->is_display_idle);
	DDPDUMP("is_wrot_sram=%d\n", rdma_golden_setting->is_wrot_sram);
	DDPDUMP("is_rsz_sram=%d\n", rdma_golden_setting->is_rsz_sram);
	DDPDUMP("is_dc=%d\n", rdma_golden_setting->is_dc);
	DDPDUMP("mmsys_clk=%d\n", rdma_golden_setting->mmsys_clk);
	DDPDUMP("fps=%d\n", rdma_golden_setting->fps);
	DDPDUMP("is_one_layer=%d\n", rdma_golden_setting->is_one_layer);
	DDPDUMP("rdma_width=%d\n", rdma_golden_setting->dst_width);
	DDPDUMP("rdma_height=%d\n", rdma_golden_setting->dst_height);
}

void rdma_dump_reg(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;

	DDPDUMP("== DISP RDMA%d REGS ==\n", idx);
	DDPDUMP("(0x000)R_INTEN=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_INT_ENABLE + offset));
	DDPDUMP("(0x004)R_INTS=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_INT_STATUS + offset));
	DDPDUMP("(0x010)R_CON=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON + offset));
	DDPDUMP("(0x014)R_SIZE0=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset));
	DDPDUMP("(0x018)R_SIZE1=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1 + offset));
	DDPDUMP("(0x01c)R_TAR_LINE=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_TARGET_LINE + offset));
	DDPDUMP("(0x024)R_M_CON=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_CON + offset));
	DDPDUMP("(0xf00)R_M_S_ADDR=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + offset));
	DDPDUMP("(0x02c)R_M_SRC_PITCH=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_SRC_PITCH + offset));
	DDPDUMP("(0x030)R_M_GMC_SET0=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0 + offset));
	DDPDUMP("(0x034)R_M_GMC_SET1=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_1 + offset));
	DDPDUMP("(0x038)R_M_SLOW_CON=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_SLOW_CON + offset));
	DDPDUMP("(0x03c)R_M_GMC_SET2=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_2 + offset));
	DDPDUMP("(0x040)R_FIFO_CON=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_FIFO_CON + offset));
	DDPDUMP("(0x044)R_FIFO_LOG=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_FIFO_LOG + offset));
	DDPDUMP("(0x078)R_PRE_ADD0=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_0 + offset));
	DDPDUMP("(0x07c)R_PRE_ADD1=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_1 + offset));
	DDPDUMP("(0x080)R_PRE_ADD2=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_PRE_ADD_2 + offset));
	DDPDUMP("(0x084)R_POST_ADD0=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_POST_ADD_0 + offset));
	DDPDUMP("(0x088)R_POST_ADD1=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_POST_ADD_1 + offset));
	DDPDUMP("(0x08c)R_POST_ADD2=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_POST_ADD_2 + offset));
	DDPDUMP("(0x090)R_DUMMY=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_DUMMY + offset));
	DDPDUMP("(0x094)R_OUT_SEL=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_DEBUG_OUT_SEL + offset));
	DDPDUMP("(0x094)R_M_START=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + offset));
	DDPDUMP("(0x0a0)R_BG_CON_0=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_BG_CON_0 + offset));
	DDPDUMP("(0x0a4)R_BG_CON_1=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_BG_CON_1 + offset));
	DDPDUMP("(0x0a8)R_FOR_SODI=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_THRESHOLD_FOR_SODI + offset));
	DDPDUMP("(0x0ac)R_FOR_DVFS=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_THRESHOLD_FOR_DVFS + offset));
	DDPDUMP("(0x0b0)R_FOR_SRAM=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_SRAM_SEL + offset));
	DDPDUMP("(0x0b4)DISP_REG_RDMA_STALL_CG_CON=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_STALL_CG_CON + offset));
	DDPDUMP("(0x0b8)DISP_REG_RDMA_SHADOW_UPDATE=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_SHADOW_UPDATE + offset));
	DDPDUMP("(0x0c0)R_DRAM_CON=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_DRAM_CON + offset));
	DDPDUMP("(0x0d0)R_DVFS_SETTING_PRE=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_DVFS_SETTING_PRE + offset));
	DDPDUMP("(0x0d4)R_DVFS_SETTING_ULTRA=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_DVFS_SETTING_ULTRA + offset));
	DDPDUMP("(0x0d8)R_LEAVE_DRS_SETTING=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_LEAVE_DRS_SETTING + offset));
	DDPDUMP("(0x0dc)R_ENTER_DRS_SETTING=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_ENTER_DRS_SETTING + offset));
	DDPDUMP("(0x0e8)R_M_GMC_SET3=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_3 + offset));
	DDPDUMP("(0x0f0)R_IN_PXL_CNT=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT + offset));
	DDPDUMP("(0x0f4)R_IN_LINE_CNT=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT + offset));
	DDPDUMP("(0x0f8)R_OUT_PXL_CNT=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT + offset));
	DDPDUMP("(0x0fc)R_OUT_LINE_CNT=0x%x\n",
		DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT + offset));
}

void rdma_dump_analysis(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = idx * DISP_RDMA_INDEX_OFFSET;
	unsigned int global_ctrl;
	unsigned int bg0 = DISP_REG_GET(offset + DISP_REG_RDMA_BG_CON_0);
	unsigned int bg1 = DISP_REG_GET(offset + DISP_REG_RDMA_BG_CON_1);

	global_ctrl = DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON + offset);
	DDPDUMP("== DISP RDMA%d ANALYSIS ==\n", idx);
	DDPDUMP("rdma%d: en=%d,memory_mode=%d,smi_busy=%d\n",
		idx, REG_FLD_VAL_GET(GLOBAL_CON_FLD_ENGINE_EN + offset,
			global_ctrl),
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_MODE_SEL + offset, global_ctrl),
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_SMI_BUSY + offset, global_ctrl));
	DDPDUMP("rdma%d: w=%d,h=%d,pitch=%d,addr=0x%x,fmt=%s,fifo_min=%d\n",
		idx, DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset) & 0xfff,
		DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_1 + offset) & 0xfffff,
		DISP_REG_GET(DISP_REG_RDMA_MEM_SRC_PITCH + offset),
		DISP_REG_GET(DISP_REG_RDMA_MEM_START_ADDR + offset),
		unified_color_fmt_name(
			display_fmt_reg_to_unified_fmt(
				(DISP_REG_GET(DISP_REG_RDMA_MEM_CON +
					offset) >> 4) & 0xf,
				(DISP_REG_GET(DISP_REG_RDMA_MEM_CON +
					offset) >> 8) & 0x1, 0)),
		DISP_REG_GET(DISP_REG_RDMA_FIFO_LOG + offset));
	DDPDUMP("in_p=%d,in_l=%d,out_p=%d,out_l=%d,bg(t%d,b%d,l%d,r%d)\n",
		DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT + offset),
		DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT + offset),
		DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT + offset),
		DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT + offset),
		REG_FLD_VAL_GET(RDMA_BG_CON_1_TOP + offset, bg1),
		REG_FLD_VAL_GET(RDMA_BG_CON_1_BOTTOM + offset, bg1),
		REG_FLD_VAL_GET(RDMA_BG_CON_0_LEFT + offset, bg0),
		REG_FLD_VAL_GET(RDMA_BG_CON_0_RIGHT + offset, bg0));
	DDPDUMP("start=%lld ns,end=%lld ns\n",
		rdma_start_time[idx], rdma_end_time[idx]);
	DDPDUMP("irq cnt: start=%d, end=%d, underflow=%d, targetline=%d\n",
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
		(DISP_RDMA_INDEX_OFFSET * idx) + DISP_REG_RDMA_GLOBAL_CON);
}

static inline enum RDMA_MODE rdma_config_mode(unsigned long address)
{
	return address ? RDMA_MODE_MEMORY : RDMA_MODE_DIRECT_LINK;
}

static int do_rdma_config_l(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *handle)
{
	struct RDMA_CONFIG_STRUCT *r_config = &pConfig->rdma_config;
	enum RDMA_MODE mode = rdma_config_mode(r_config->address);
	struct LCM_PARAMS *lcm_param = &(pConfig->dispif_config);
	unsigned int width;
	unsigned int height;
	struct golden_setting_context *p_golden_setting;
	enum UNIFIED_COLOR_FMT inFormat = r_config->inputFormat;
	enum UNIFIED_COLOR_FMT bwFormat;
	unsigned int bwBpp;
	unsigned long long rdma_bw;

	width = pConfig->dst_dirty ? pConfig->dst_w : r_config->width;
	height = pConfig->dst_dirty ? pConfig->dst_h : r_config->height;
	p_golden_setting = pConfig->p_golden_setting_context;

	if (pConfig->fps)
		rdma_fps[rdma_index(module)] = pConfig->fps / 100;

	if (mode == RDMA_MODE_DIRECT_LINK &&
		r_config->security != DISP_NORMAL_BUFFER)
		DDPERR("%s: rdma directlink BUT is sec ??!!\n", __func__);

	if (mode == RDMA_MODE_DIRECT_LINK) {
		pConfig->rdma_config.bg_ctrl.top = 0;
		pConfig->rdma_config.bg_ctrl.bottom = 0;
		pConfig->rdma_config.bg_ctrl.left = 0;
		pConfig->rdma_config.bg_ctrl.right = 0;
	} else if (mode == RDMA_MODE_MEMORY) {
		pConfig->rdma_config.bg_ctrl.top = r_config->dst_y;
		pConfig->rdma_config.bg_ctrl.bottom = r_config->dst_h -
			r_config->dst_y - height;
		pConfig->rdma_config.bg_ctrl.left = r_config->dst_x;
		pConfig->rdma_config.bg_ctrl.right = r_config->dst_w -
			r_config->dst_x - width;
	}
	DDPDBG("top=%d,bottom=%d,left=%d,right=%d\n",
		pConfig->rdma_config.bg_ctrl.top,
		pConfig->rdma_config.bg_ctrl.bottom,
		pConfig->rdma_config.bg_ctrl.left,
		pConfig->rdma_config.bg_ctrl.right);
	DDPDBG("r.dst_x=%d,r.dst_y=%d,r.dst_w=%d,r.dst_h=%d,w=%d,h=%d\n",
		r_config->dst_x, r_config->dst_y,
		r_config->dst_w,
		r_config->dst_h, width, height);
	/*PARGB,etc need convert ARGB,etc*/
	ufmt_disable_P(r_config->inputFormat, &inFormat);
	rdma_config(module,
	    mode,
	    (mode == RDMA_MODE_DIRECT_LINK) ? 0 : r_config->address,
	    (mode == RDMA_MODE_DIRECT_LINK) ? UFMT_RGB888 : inFormat,
	    (mode == RDMA_MODE_DIRECT_LINK) ? 0 : r_config->pitch,
	    width,
	    height,
	    lcm_param->dsi.ufoe_enable,
	    r_config->security, r_config->yuv_range, &(r_config->bg_ctrl),
	    handle, p_golden_setting, pConfig->lcm_bpp);

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

static inline int rdma_switch_to_sec(enum DISP_MODULE_ENUM module,
	void *handle)
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
		/* rdma is in sec stat, we need to switch it to nonsec */
		struct cmdqRecStruct *nonsec_switch_handle;
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
			/* ovl1->Rdma1, do not used. */
			_cmdq_insert_wait_frame_done_token_mira(
				nonsec_switch_handle);
		}

		cmdqRecSetSecure(nonsec_switch_handle, 1);
		/*
		 * will remove when cmdq delete disable scenario.
		 * To avoid translation fault like ovl (see notes in ovl.c)
		 * check the mode now, bypass the frame during DL->DC(),
		 * avoid hang when vdo mode.
		 */
		if (get_rdma_mode(module) == RDMA_MODE_MEMORY)
			do_rdma_config_l(module, pConfig, nonsec_switch_handle);

		/* in fact, dapc/port_sec will be disabled by cmdq */
		cmdqRecSecureEnablePortSecurity(nonsec_switch_handle,
						(1LL << cmdq_engine));
		if (handle != NULL) {
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

	/* sec setting make sence only in memory mode ! */
	if (mode == RDMA_MODE_MEMORY) {
		if (is_engine_sec == 1)
			ret = rdma_switch_to_sec(module, handle);
		else
			/* hadle = NULL, use the sync flush method */
			ret = rdma_switch_to_nonsec(module, pConfig, NULL);
		if (ret)
			DDPAEE("[SVP]fail to setup_ovl_sec: %s ret=%d\n",
				__func__, ret);
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

	value = value |
		REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_EXT_MTX_EN), 1) |
		REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_ENABLE), 1);
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_SIZE_CON_0, value);
}

void rdma_disable_color_transform(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = rdma_index(module);
	unsigned int offset = DISP_RDMA_INDEX_OFFSET * idx;
	UINT32 value = DISP_REG_GET(DISP_REG_RDMA_SIZE_CON_0 + offset);

	value = value &
		~(REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_EXT_MTX_EN), 1) |
		REG_FLD_VAL((SIZE_CON_0_FLD_MATRIX_ENABLE), 1));
	DISP_REG_SET(NULL, offset + DISP_REG_RDMA_SIZE_CON_0, value);
}

void rdma_set_color_matrix(enum DISP_MODULE_ENUM module,
	struct rdma_color_matrix *matrix, struct rdma_color_pre *pre,
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
		DDPERR("cmdq_trigger_handle is NULL\n");
		return -1;
	}
	if (state == CMDQ_RESET_AFTER_STREAM_EOF) {
		/*
		 * if rdma frame done with underflow,
		 * rdma will hold dvfs request forever
		 * we reset here to solve this issue
		 */
		rdma_reset_by_cmdq(module, handle);
	}

	return 0;
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
