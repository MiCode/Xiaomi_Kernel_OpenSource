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

#define LOG_TAG "WDMA"
#include "ddp_log.h"
#include "ddp_clkmgr.h"
#include <linux/delay.h>
#include "ddp_reg.h"
#include "ddp_matrix_para.h"
#include "ddp_info.h"
#include "ddp_wdma.h"
#include "ddp_wdma_ex.h"
#include "primary_display.h"
#include "ddp_m4u.h"
#include "ddp_mmp.h"
#include "ddp_reg_mmsys.h"

#define ALIGN_TO(x, n)	(((x) + ((n) - 1)) & ~((n) - 1))

struct WDMA_CONFIG_STRUCT g_wdma_cfg;

unsigned int wdma_index(enum DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_WDMA0:
		idx = 0;
		break;
	default:
		/* invalid module */
		DDP_PR_ERR("[DDP] error: invalid wdma module=%d\n", module);
		ASSERT(0);
		break;
	}
	return idx;
}

int wdma_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = wdma_index(module);
	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;

	DISP_REG_SET(handle, offset + DISP_REG_WDMA_INTEN, 0x00);
	DISP_REG_SET(handle, offset + DISP_REG_WDMA_EN, 0x80000000);
	DISP_REG_SET(handle, offset + DISP_REG_WDMA_INTSTA, 0x00);
	DISP_REG_SET_FIELD(handle, FLD_DVFS_HALT_MASK_WDMA,
		DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK, 0);

	return 0;
}

int wdma_reset(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int delay_cnt = 0;
	unsigned int idx = wdma_index(module);
	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;

	/* trigger soft reset */
	DISP_REG_SET(handle, offset + DISP_REG_WDMA_RST, 0x01);
	if (!handle) {
		while ((DISP_REG_GET(offset + DISP_REG_WDMA_FLOW_CTRL_DBG) &
				0x1) == 0) {
			delay_cnt++;
			udelay(10);
			if (delay_cnt > 2000) {
				DDP_PR_ERR("wdma%d reset timeout!\n", idx);
				break;
			}
		}
	} else {
		/* add comdq polling */
	}
	/* trigger soft reset */
	DISP_REG_SET(handle, offset + DISP_REG_WDMA_RST, 0x0);

	return 0;
}

unsigned int ddp_wdma_get_cur_addr(enum DISP_MODULE_ENUM module)
{
	return INREG32(DISP_REG_WDMA_DST_ADDR0);
}

static char *wdma_get_state(unsigned int status)
{
	switch (status) {
	case 0x1:
		return "idle";
	case 0x2:
		return "clear";
	case 0x4:
		return "prepare1";
	case 0x8:
		return "prepare2";
	case 0x10:
		return "data_transmit";
	case 0x20:
		return "eof_wait";
	case 0x40:
		return "soft_reset_wait";
	case 0x80:
		return "eof_done";
	case 0x100:
		return "soft_reset_done";
	case 0x200:
		return "frame_complete";
	}
	return "unknown-state";
}

int wdma_start(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = wdma_index(module);
	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;

	DISP_REG_SET(handle, offset + DISP_REG_WDMA_INTEN, 0x03);

	DISP_REG_SET(handle, offset + DISP_REG_WDMA_EN, 0x80000001);

	DISP_REG_SET_FIELD(handle, FLD_DVFS_HALT_MASK_WDMA,
		DISP_REG_CONFIG_MMSYS_SODI_REQ_MASK, 1);

	return 0;
}

static int wdma_config_yuv420(enum DISP_MODULE_ENUM module,
			      enum UNIFIED_COLOR_FMT fmt, unsigned int dstPitch,
			      unsigned int Height, unsigned long dstAddress,
			      enum DISP_BUFFER_TYPE sec, void *handle)
{
	unsigned int idx = wdma_index(module);
	unsigned int idx_offst = idx * DISP_WDMA_INDEX_OFFSET;
	/* size_t size; */
	unsigned int u_off = 0;
	unsigned int v_off = 0;
	unsigned int u_stride = 0;
	unsigned int y_size = 0;
	unsigned int u_size = 0;
	/* unsigned int v_size = 0; */
	unsigned int stride = dstPitch;
	int has_v = 1;

	if (fmt != UFMT_YV12 && fmt != UFMT_I420 &&
	    fmt != UFMT_NV12 && fmt != UFMT_NV21)
		return 0;

	if (fmt == UFMT_YV12) {
		y_size = stride * Height;
		u_stride = ALIGN_TO(stride / 2, 16);
		u_size = u_stride * Height / 2;
		u_off = y_size;
		v_off = y_size + u_size;
	} else if (fmt == UFMT_I420) {
		y_size = stride * Height;
		u_stride = ALIGN_TO(stride / 2, 16);
		u_size = u_stride * Height / 2;
		v_off = y_size;
		u_off = y_size + u_size;
	} else if (fmt == UFMT_NV12 || fmt == UFMT_NV21) {
		y_size = stride * Height;
		u_stride = stride / 2;
		u_size = u_stride * Height / 2;
		u_off = y_size;
		has_v = 0;
	}

	if (sec != DISP_SECURE_BUFFER) {
		DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_DST_ADDR1,
			     dstAddress + u_off);
		if (has_v)
			DISP_REG_SET(handle, idx_offst +
				     DISP_REG_WDMA_DST_ADDR2,
				     dstAddress + v_off);
	} else {
		int m4u_port;

		m4u_port = DISP_M4U_PORT_DISP_WDMA0;

		cmdqRecWriteSecure(handle,
			disp_addr_convert(idx_offst + DISP_REG_WDMA_DST_ADDR1),
			CMDQ_SAM_H_2_MVA, dstAddress, u_off, u_size, m4u_port);
		if (has_v)
			cmdqRecWriteSecure(handle,
					disp_addr_convert(idx_offst +
						DISP_REG_WDMA_DST_ADDR2),
					CMDQ_SAM_H_2_MVA, dstAddress,
					v_off, u_size, m4u_port);
	}
	DISP_REG_SET_FIELD(handle, DST_W_IN_BYTE_FLD_DST_W_IN_BYTE,
			   idx_offst + DISP_REG_WDMA_DST_UV_PITCH, u_stride);
	return 0;
}

static int wdma_config(enum DISP_MODULE_ENUM module,
		       unsigned int srcWidth, unsigned int srcHeight,
		       unsigned int clipX, unsigned int clipY,
		       unsigned int clipWidth, unsigned int clipHeight,
		       enum UNIFIED_COLOR_FMT out_format,
		       unsigned long dstAddress, unsigned int dstPitch,
		       unsigned int useSpecifiedAlpha, unsigned char alpha,
		       enum DISP_BUFFER_TYPE sec, void *handle)
{
	unsigned int idx = wdma_index(module);
	unsigned int output_swap = ufmt_get_byteswap(out_format);
	unsigned int is_rgb = ufmt_get_rgb(out_format);
	unsigned int out_fmt_reg = ufmt_get_format(out_format);
	int color_matrix = 0x2;	/* 0010 RGB_TO_BT601 */
	unsigned int idx_offst = idx * DISP_WDMA_INDEX_OFFSET;
	size_t size = dstPitch * clipHeight;
	unsigned int value = 0, mask = 0;

	DDPDBG(
		"%s,src(%dx%d),clip(%d,%d,%dx%d),fmt=%s,addr=0x%lx,pitch=%d,s_alfa=%d,alpa=%d,hnd=0x%p,sec%d\n",
	       ddp_get_module_name(module), srcWidth, srcHeight,
	       clipX, clipY, clipWidth, clipHeight,
	       unified_color_fmt_name(out_format),
	       dstAddress, dstPitch, useSpecifiedAlpha, alpha, handle, sec);

	SET_VAL_MASK(value, mask, 0, CFG_FLD_UFO_DCP_ENABLE);

	/* should use OVL alpha instead of SW config */
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_SRC_SIZE,
		     srcHeight << 16 | srcWidth);
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_CLIP_COORD,
		     clipY << 16 | clipX);
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_CLIP_SIZE,
		     clipHeight << 16 | clipWidth);
	SET_VAL_MASK(value, mask, out_fmt_reg, CFG_FLD_OUT_FORMAT);

	if (!is_rgb) {
		/* set DNSP for UYVY and YUV_3P format for better quality */
		wdma_config_yuv420(module, out_format, dstPitch, clipHeight,
				   dstAddress, sec, handle);
		/* user internal matrix */
		SET_VAL_MASK(value, mask, 0, CFG_FLD_EXT_MTX_EN);
		SET_VAL_MASK(value, mask, 1, CFG_FLD_CT_EN);
		SET_VAL_MASK(value, mask, color_matrix, CFG_FLD_INT_MTX_SEL);
	} else {
		SET_VAL_MASK(value, mask, 0, CFG_FLD_EXT_MTX_EN);
		SET_VAL_MASK(value, mask, 0, CFG_FLD_CT_EN);
	}

	SET_VAL_MASK(value, mask, output_swap, CFG_FLD_SWAP);
	DISP_REG_MASK(handle, idx_offst + DISP_REG_WDMA_CFG, value, mask);

	if (sec != DISP_SECURE_BUFFER) {
		DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_DST_ADDR0,
			     dstAddress);
	} else {
		int m4u_port;

		m4u_port = DISP_M4U_PORT_DISP_WDMA0;
		/*
		 * for sec layer, addr variable stores sec handle
		 * we need to pass this handle and offset to cmdq driver
		 * cmdq sec driver will convert handle to correct address
		 */
		cmdqRecWriteSecure(handle,
			disp_addr_convert(idx_offst + DISP_REG_WDMA_DST_ADDR0),
			CMDQ_SAM_H_2_MVA, dstAddress, 0, size, m4u_port);
	}
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_DST_W_IN_BYTE, dstPitch);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, useSpecifiedAlpha, ALPHA_FLD_A_SEL);
	SET_VAL_MASK(value, mask, alpha, ALPHA_FLD_A_VALUE);
	DISP_REG_MASK(handle, idx_offst + DISP_REG_WDMA_ALPHA, value, mask);

	return 0;
}

static int wdma_clock_on(enum DISP_MODULE_ENUM module, void *handle)
{
	ddp_clk_enable_by_module(module);
	return 0;
}

static int wdma_clock_off(enum DISP_MODULE_ENUM module, void *handle)
{
	ddp_clk_disable_by_module(module);
	return 0;
}

void wdma_dump_golden_setting(enum DISP_MODULE_ENUM module)
{
	unsigned int idx = wdma_index(module);
	unsigned int off_sft = idx * DISP_WDMA_INDEX_OFFSET;
	int i = 0;

	DDPDUMP("-- %s Golden Setting --\n", ddp_get_module_name(module));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
		0x10, DISP_REG_GET(DISP_REG_WDMA_SMI_CON + off_sft),
		0x38, DISP_REG_GET(DISP_REG_WDMA_BUF_CON1 + off_sft));
	for (i = 0; i < 3; i++)
		DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
			0x200 + i * 0x10,
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON5 +
				     off_sft + i * 0x10),
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON6 +
				     off_sft + i * 0x10),
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON7 +
				     off_sft + i * 0x10),
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON8 +
				     off_sft + i * 0x10));
	DDPDUMP("0x%03x:0x%08x 0x%08x\n",
		0x230, DISP_REG_GET(DISP_REG_WDMA_BUF_CON17 + off_sft),
		DISP_REG_GET(DISP_REG_WDMA_BUF_CON18 + off_sft));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
		0x250, DISP_REG_GET(DISP_REG_WDMA_DRS_CON0 + off_sft),
		DISP_REG_GET(DISP_REG_WDMA_DRS_CON1 + off_sft),
		DISP_REG_GET(DISP_REG_WDMA_DRS_CON2 + off_sft),
		DISP_REG_GET(DISP_REG_WDMA_DRS_CON3 + off_sft));
	DDPDUMP("0x%03x:0x%08x 0x%08x\n",
		0x104, DISP_REG_GET(DISP_REG_WDMA_BUF_CON3 + off_sft),
		DISP_REG_GET(DISP_REG_WDMA_BUF_CON4 + off_sft));

	DDPDUMP("WDMA_SMI_CON:[3:0]:%x [4:4]:%x [7:5]:%x [15:8]:%x\n",
		DISP_REG_GET_FIELD(SMI_CON_FLD_THRESHOLD,
		    off_sft + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SLOW_ENABLE,
		    off_sft + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SLOW_LEVEL,
		    off_sft + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SLOW_COUNT,
		    off_sft + DISP_REG_WDMA_SMI_CON));

	DDPDUMP("WDMA_SMI_CON:[19:16]:%u [23:20]:%u [27:24]:%u [28]:%u\n",
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_Y_REPEAT_NUM,
		    off_sft + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_U_REPEAT_NUM,
		    off_sft + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_V_REPEAT_NUM,
		    off_sft + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_OBUF_FULL_REQ,
		    off_sft + DISP_REG_WDMA_SMI_CON));

	DDPDUMP("WDMA_BUF_CON1:[31]:%x [30]:%x [28]:%x [26]%d\n",
		DISP_REG_GET_FIELD(BUF_CON1_FLD_ULTRA_ENABLE,
		    off_sft + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_PRE_ULTRA_ENABLE,
		    off_sft + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_FRAME_END_ULTRA,
		    off_sft + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_URGENT_EN,
		    off_sft + DISP_REG_WDMA_BUF_CON1));

	DDPDUMP("WDMA_BUF_CON1:[18:10]:%d [9:0]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON1_FLD_FIFO_PSEUDO_SIZE_UV,
		    off_sft + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_FIFO_PSEUDO_SIZE,
		    off_sft + DISP_REG_WDMA_BUF_CON1));

	DDPDUMP("WDMA_BUF_CON5:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON5),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON5));

	DDPDUMP("WDMA_BUF_CON6:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON6),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON6));

	DDPDUMP("WDMA_BUF_CON7:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON7),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON7));

	DDPDUMP("WDMA_BUF_CON8:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON8),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON8));

	DDPDUMP("WDMA_BUF_CON9:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON9),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON9));

	DDPDUMP("WDMA_BUF_CON10:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON10),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON10));

	DDPDUMP("WDMA_BUF_CON11:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON11),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON11));

	DDPDUMP("WDMA_BUF_CON12:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON12),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON12));

	DDPDUMP("WDMA_BUF_CON13:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON13),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON13));

	DDPDUMP("WDMA_BUF_CON14:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON14),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON14));

	DDPDUMP("WDMA_BUF_CON15:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON15),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
		    off_sft + DISP_REG_WDMA_BUF_CON15));

	DDPDUMP("WDMA_BUF_CON16:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON16),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
		    off_sft + DISP_REG_WDMA_BUF_CON16));

	DDPDUMP("WDMA_BUF_CON17:[0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON17_FLD_WDMA_DVFS_EN,
		    off_sft + DISP_REG_WDMA_BUF_CON17),
		DISP_REG_GET_FIELD(BUF_CON17_FLD_DVFS_TH_Y,
		    off_sft + DISP_REG_WDMA_BUF_CON17));

	DDPDUMP("WDMA_BUF_CON18:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON18_FLD_DVFS_TH_U,
		    off_sft + DISP_REG_WDMA_BUF_CON18),
		DISP_REG_GET_FIELD(BUF_CON18_FLD_DVFS_TH_V,
		    off_sft + DISP_REG_WDMA_BUF_CON18));

	DDPDUMP("WDMA_URGENT_CON0:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(FLD_WDMA_URGENT_LOW_Y,
		    off_sft + DISP_REG_WDMA_URGENT_CON0),
		DISP_REG_GET_FIELD(FLD_WDMA_URGENT_HIGH_Y,
		    off_sft + DISP_REG_WDMA_URGENT_CON0));

	DDPDUMP("WDMA_URGENT_CON1:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(FLD_WDMA_URGENT_LOW_U,
		    off_sft + DISP_REG_WDMA_URGENT_CON1),
		DISP_REG_GET_FIELD(FLD_WDMA_URGENT_HIGH_U,
		    off_sft + DISP_REG_WDMA_URGENT_CON1));

	DDPDUMP("WDMA_URGENT_CON2:[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(FLD_WDMA_URGENT_LOW_V,
		    off_sft + DISP_REG_WDMA_URGENT_CON2),
		DISP_REG_GET_FIELD(FLD_WDMA_URGENT_HIGH_V,
		    off_sft + DISP_REG_WDMA_URGENT_CON2));

	DDPDUMP("WDMA_BUF_CON3:[8:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON3_FLD_ISSUE_REQ_TH_Y,
		    off_sft + DISP_REG_WDMA_BUF_CON3),
		DISP_REG_GET_FIELD(BUF_CON3_FLD_ISSUE_REQ_TH_U,
		    off_sft + DISP_REG_WDMA_BUF_CON3));

	DDPDUMP("WDMA_BUF_CON4:[8:0]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON4_FLD_ISSUE_REQ_TH_V,
		    off_sft + DISP_REG_WDMA_BUF_CON4));

}

void wdma_dump_analysis(enum DISP_MODULE_ENUM module)
{
	unsigned int index = wdma_index(module);
	unsigned int idx_offst = index * DISP_WDMA_INDEX_OFFSET;

	DDPDUMP("== DISP %s ANALYSIS ==\n", ddp_get_module_name(module));
	DDPDUMP(
		"en=%d,src(%dx%d),clip=(%d,%d,%dx%d),pitch=(W=%d,UV=%d),addr=(0x%08x,0x%08x,0x%08x),fmt=%s\n",
		DISP_REG_GET(DISP_REG_WDMA_EN + idx_offst) & 0x01,
		DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE + idx_offst) & 0x3fff,
		(DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE + idx_offst) >> 16) &
			0x3fff,
		DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD + idx_offst) & 0x3fff,
		(DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD + idx_offst) >> 16) &
			0x3fff,
		DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE + idx_offst) & 0x3fff,
		(DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE + idx_offst) >> 16) &
			0x3fff,
		DISP_REG_GET(DISP_REG_WDMA_DST_W_IN_BYTE + idx_offst),
		DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH + idx_offst),
		DISP_REG_GET(DISP_REG_WDMA_DST_ADDR0 + idx_offst),
		DISP_REG_GET(DISP_REG_WDMA_DST_ADDR1 + idx_offst),
		DISP_REG_GET(DISP_REG_WDMA_DST_ADDR2 + idx_offst),
		unified_color_fmt_name(display_fmt_reg_to_unified_fmt
			((DISP_REG_GET(DISP_REG_WDMA_CFG + idx_offst) >> 4) &
				0xf,
			(DISP_REG_GET(DISP_REG_WDMA_CFG + idx_offst) >> 10) &
				0x1, 0))
		);
	DDPDUMP(
		"state=%s,in_req=%d(prev sent data),in_ack=%d(ask data to prev),start=%d,end=%d,pos:in(%d,%d)\n",
		wdma_get_state(DISP_REG_GET_FIELD
				(FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL,
				 DISP_REG_WDMA_FLOW_CTRL_DBG + idx_offst)),
		DISP_REG_GET_FIELD(FLOW_CTRL_DBG_FLD_WDMA_IN_VALID,
				   DISP_REG_WDMA_FLOW_CTRL_DBG + idx_offst),
		DISP_REG_GET_FIELD(FLOW_CTRL_DBG_FLD_WDMA_IN_READY,
				   DISP_REG_WDMA_FLOW_CTRL_DBG + idx_offst),
		DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG + idx_offst) & 0x3f,
		DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG + idx_offst) >> 16 & 0x3f,
		DISP_REG_GET(DISP_REG_WDMA_INPUT_CNT_DBG + idx_offst) & 0x3fff,
		(DISP_REG_GET(DISP_REG_WDMA_INPUT_CNT_DBG + idx_offst) >> 16) &
		0x3fff);
}

void wdma_dump_reg(enum DISP_MODULE_ENUM module)
{
	if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned int idx = wdma_index(module);
		unsigned long module_base = DISPSYS_WDMA0_BASE +
						idx * DISP_WDMA_INDEX_OFFSET;

		DDPDUMP("== START: DISP %s REGS ==\n",
			ddp_get_module_name(module));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x0, INREG32(module_base + 0x0),
			0x4, INREG32(module_base + 0x4),
			0x8, INREG32(module_base + 0x8),
			0xC, INREG32(module_base + 0xC));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x10, INREG32(module_base + 0x10),
			0x14, INREG32(module_base + 0x14),
			0x18, INREG32(module_base + 0x18),
			0x1C, INREG32(module_base + 0x1C));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x20, INREG32(module_base + 0x20),
			0x28, INREG32(module_base + 0x28),
			0x2C, INREG32(module_base + 0x2C),
			0x38, INREG32(module_base + 0x38));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x3C, INREG32(module_base + 0x3C),
			0x40, INREG32(module_base + 0x40),
			0x44, INREG32(module_base + 0x44),
			0x48, INREG32(module_base + 0x48));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x4C, INREG32(module_base + 0x4C),
			0x50, INREG32(module_base + 0x50),
			0x54, INREG32(module_base + 0x54),
			0x58, INREG32(module_base + 0x58));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x5C, INREG32(module_base + 0x5C),
			0x60, INREG32(module_base + 0x60),
			0x64, INREG32(module_base + 0x64),
			0x78, INREG32(module_base + 0x78));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x80, INREG32(module_base + 0x80),
			0x84, INREG32(module_base + 0x84),
			0x88, INREG32(module_base + 0x88),
			0x90, INREG32(module_base + 0x90));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0x94, INREG32(module_base + 0x94),
			0x98, INREG32(module_base + 0x98),
			0xA0, INREG32(module_base + 0xA0),
			0xA4, INREG32(module_base + 0xA4));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xA8, INREG32(module_base + 0xA8),
			0xAC, INREG32(module_base + 0xAC),
			0xB0, INREG32(module_base + 0xB0),
			0xB4, INREG32(module_base + 0xB4));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xB8, INREG32(module_base + 0xB8),
			0x100, INREG32(module_base + 0x100),
			0xE00, INREG32(module_base + 0xE00),
			0xE14, INREG32(module_base + 0xE14));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xE18, INREG32(module_base + 0xE18),
			0xE1C, INREG32(module_base + 0xE1C),
			0xE20, INREG32(module_base + 0xE20),
			0xE24, INREG32(module_base + 0xE24));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xE28, INREG32(module_base + 0xE28),
			0xE2C, INREG32(module_base + 0xE2C),
			0xE30, INREG32(module_base + 0xE30),
			0xE34, INREG32(module_base + 0xE34));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xE38, INREG32(module_base + 0xE38),
			0xE3C, INREG32(module_base + 0xE3C),
			0xE40, INREG32(module_base + 0xE40),
			0xE44, INREG32(module_base + 0xE44));
		DDPDUMP(
			"WDMA0: 0x%04x=0x%08x, 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			0xF00, INREG32(module_base + 0xF00),
			0xF04, INREG32(module_base + 0xF04),
			0xF08, INREG32(module_base + 0xF08));
		DDPDUMP("-- END: DISP WDMA0 REGS --\n");
	} else {
		unsigned int idx = wdma_index(module);
		unsigned int off_sft = idx * DISP_WDMA_INDEX_OFFSET;

		DDPDUMP("== DISP %s REGS ==\n", ddp_get_module_name(module));

		DDPDUMP("0x000:0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_INTEN + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_INTSTA + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_EN + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_RST + off_sft));

		DDPDUMP("0x010:0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_SMI_CON + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_CFG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE + off_sft));

		DDPDUMP(
			"0x020:0x%08x 0x028:0x%08x 0x%08x 0x038:0x%08x 0x078:0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_W_IN_BYTE + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_ALPHA + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON1 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH + off_sft));

		DDPDUMP("0x080:0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET0 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET1 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET2 + off_sft));

		DDPDUMP("0x0a0:0x%08x 0x%08x 0x%08x 0x0b8:0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_FLOW_CTRL_DBG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_INPUT_CNT_DBG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DEBUG + off_sft));

		DDPDUMP("0xf00:0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR0 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR1 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR2 + off_sft));

		wdma_dump_golden_setting(module);
	}
}

static int wdma_dump(enum DISP_MODULE_ENUM module, int level)
{
	wdma_dump_analysis(module);
	wdma_dump_golden_setting(module);
	wdma_dump_reg(module);

	return 0;
}

void wdma_calc_golden_setting(struct golden_setting_context *gsc,
	unsigned int is_primary_flag, unsigned int *gs,
	enum UNIFIED_COLOR_FMT format)
{
	unsigned int preultra_low_us = 7, preultra_high_us = 6;
	unsigned int ultra_low_us = 6, ultra_high_us = 4;
	unsigned int dvfs_offset = 2;
	unsigned int urgent_low_offset = 4, urgent_high_offset = 3;
	unsigned int Bpp = 3;
	unsigned int FP = 100;
	unsigned int res = 0;
	unsigned int frame_rate = 0;
	unsigned long long consume_rate = 0;
	unsigned int fifo_size = 297;
	unsigned int fifo_size_uv = 1;
	unsigned int fifo;
	unsigned int factor1 = 4;
	unsigned int factor2 = 4;
	unsigned int tmp;

	frame_rate = 60;
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	frame_rate = gsc->fps;
#endif
	if (is_primary_flag)
		res = gsc->dst_width * gsc->dst_height;
	else {
		res = gsc->ext_dst_width * gsc->ext_dst_height;
		if (gsc->ext_dst_width == 3840 && gsc->ext_dst_height == 2160)
			frame_rate = 30;
	}

	consume_rate = res * frame_rate;
	do_div(consume_rate, 1000);
	consume_rate *= 125; /* PF = 100 */
	do_div(consume_rate, 16 * 1000);


	/* WDMA_SMI_CON */
	if (format == UFMT_YV12 || format == UFMT_I420)
		gs[GS_WDMA_SMI_CON] = 0x11140007;
	else
		gs[GS_WDMA_SMI_CON] = 0x12240007;

	/* WDMA_BUF_CON1 */
	if (!gsc->is_dc)
		gs[GS_WDMA_BUF_CON1] = 0xD4000000;
	else
		gs[GS_WDMA_BUF_CON1] = 0x40000000;

	if (format == UFMT_YV12 || format == UFMT_I420) /* 3 plane */
		gs[GS_WDMA_BUF_CON1] += 0xBCC5;
	else if (format == UFMT_NV12 || format == UFMT_NV21) /* 2 plane */
		gs[GS_WDMA_BUF_CON1] += 0x184C5;
	else /* 1 plane */
		gs[GS_WDMA_BUF_CON1] += 0x529;

	switch (format) {
	case UFMT_YV12:
	case UFMT_I420:
		/* 3 plane */
		fifo_size = 197;
		fifo_size_uv = 47;
		fifo = fifo_size_uv;
		factor1 = 4;
		factor2 = 4;
		Bpp = 1;

		break;
	case UFMT_NV12:
	case UFMT_NV21:
		/* 2 plane */
		fifo_size = 197;
		fifo_size_uv = 97;
		fifo = fifo_size_uv;
		factor1 = 2;
		factor2 = 4;
		Bpp = 1;

		break;
	default:
		/* 1 plane */
		/* fifo_size keep default */
		/* Bpp keep default */
		factor1 = 4;
		factor2 = 4;
		fifo = fifo_size/4;

		break;
	}

	/* WDMA_BUF_CON5 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * preultra_low_us, FP);
	gs[GS_WDMA_PRE_ULTRA_LOW_Y] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * Bpp * ultra_low_us, FP);
	gs[GS_WDMA_ULTRA_LOW_Y] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON6 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * preultra_high_us, FP);
	gs[GS_WDMA_PRE_ULTRA_HIGH_Y] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * Bpp * ultra_high_us, FP);
	gs[GS_WDMA_ULTRA_HIGH_Y] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON7 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_low_us, FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_LOW_U] = (fifo > tmp) ?
	(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_low_us, FP * factor1);
	gs[GS_WDMA_ULTRA_LOW_U] = (fifo > tmp) ?
	(fifo - tmp) : 1;

	/* WDMA_BUF_CON8 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_high_us, FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_HIGH_U] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_high_us, FP * factor1);
	gs[GS_WDMA_ULTRA_HIGH_U] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA_BUF_CON9 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_low_us, FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_LOW_V] = (fifo > tmp) ?
	(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_low_us, FP * factor2);
	gs[GS_WDMA_ULTRA_LOW_V] = (fifo > tmp) ?
	(fifo - tmp) : 1;

	/* WDMA_BUF_CON10 */
	tmp = DIV_ROUND_UP(consume_rate * preultra_high_us, FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_HIGH_V] = (fifo > tmp) ?
	(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * ultra_high_us, FP * factor2);
	gs[GS_WDMA_ULTRA_HIGH_V] = (fifo > tmp) ?
	(fifo - tmp) : 1;

	/* WDMA_BUF_CON11 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp *
		(preultra_low_us + dvfs_offset), FP);
	gs[GS_WDMA_PRE_ULTRA_LOW_Y_DVFS] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;
	tmp = DIV_ROUND_UP(consume_rate * Bpp *
		(ultra_low_us + dvfs_offset), FP);
	gs[GS_WDMA_ULTRA_LOW_Y_DVFS] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON12 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp *
		(preultra_high_us + dvfs_offset), FP);
	gs[GS_WDMA_PRE_ULTRA_HIGH_Y_DVFS] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;
	tmp = DIV_ROUND_UP(consume_rate * Bpp *
		(ultra_high_us + dvfs_offset), FP);
	gs[GS_WDMA_ULTRA_HIGH_Y_DVFS] = (fifo_size > tmp) ?
	    (fifo_size - tmp) : 1;

	/* WDMA_BUF_CON13 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_low_us + dvfs_offset),
		FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_LOW_U_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_low_us + dvfs_offset),
		FP * factor1);
	gs[GS_WDMA_ULTRA_LOW_U_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA_BUF_CON14 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_high_us + dvfs_offset),
		FP * factor1);
	gs[GS_WDMA_PRE_ULTRA_HIGH_U_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_high_us + dvfs_offset),
		FP * factor1);
	gs[GS_WDMA_ULTRA_HIGH_U_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA_BUF_CON15 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_low_us + dvfs_offset),
		FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_LOW_V_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_low_us + dvfs_offset),
		FP * factor2);
	gs[GS_WDMA_ULTRA_LOW_V_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA_BUF_CON16 */
	tmp = DIV_ROUND_UP(consume_rate * (preultra_high_us + dvfs_offset),
		FP * factor2);
	gs[GS_WDMA_PRE_ULTRA_HIGH_V_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * (ultra_high_us + dvfs_offset),
		FP * factor2);
	gs[GS_WDMA_ULTRA_HIGH_V_DVFS] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA_BUF_CON17 */
	gs[GS_WDMA_DVFS_EN] = 1;
	gs[GS_WDMA_DVFS_TH_Y] = gs[GS_WDMA_ULTRA_HIGH_Y_DVFS];

	/* WDMA_BUF_CON18 */
	gs[GS_WDMA_DVFS_TH_U] = gs[GS_WDMA_ULTRA_HIGH_U_DVFS];
	gs[GS_WDMA_DVFS_TH_V] = gs[GS_WDMA_ULTRA_HIGH_V_DVFS];

	/* WDMA URGENT CONTROL 0 */
	tmp = DIV_ROUND_UP(consume_rate * Bpp * urgent_low_offset, FP);
	gs[GS_WDMA_URGENT_LOW_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * Bpp * urgent_high_offset, FP);
	gs[GS_WDMA_URGENT_HIGH_Y] = (fifo_size > tmp) ? (fifo_size - tmp) : 1;

	/* WDMA URGENT CONTROL 1 */
	tmp = DIV_ROUND_UP(consume_rate * urgent_low_offset, FP * factor1);
	gs[GS_WDMA_URGENT_LOW_U] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * urgent_high_offset, FP * factor1);
	gs[GS_WDMA_URGENT_HIGH_U] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA URGENT CONTROL 2 */
	tmp = DIV_ROUND_UP(consume_rate * urgent_low_offset, FP * factor2);
	gs[GS_WDMA_URGENT_LOW_V] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	tmp = DIV_ROUND_UP(consume_rate * urgent_high_offset, FP * factor2);
	gs[GS_WDMA_URGENT_HIGH_V] = (fifo > tmp) ?
		(fifo - tmp) : 1;

	/* WDMA Buf Constant 3 */
	gs[GS_WDMA_ISSUE_REG_TH_Y] = 16;
	gs[GS_WDMA_ISSUE_REG_TH_U] = 16;

	/* WDMA Buf Constant 4 */
	gs[GS_WDMA_ISSUE_REG_TH_V] = 16;
}

static int wdma_golden_setting(enum DISP_MODULE_ENUM module,
			       struct golden_setting_context *gsc,
			       unsigned int is_primary_flag,
			       enum UNIFIED_COLOR_FMT format,
			       void *cmdq)
{
	unsigned int idx = wdma_index(module);
	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;
	unsigned int gs[GS_WDMA_FLD_NUM];
	unsigned int value = 0;

	if (!gsc) {
		DDP_PR_ERR("%s:%d: golden setting is null\n",
			   __FILE__, __LINE__);
		ASSERT(0);
		return 0;
	}

	wdma_calc_golden_setting(gsc, is_primary_flag, gs, format);

	/* WDMA_SMI_CON */
	value = gs[GS_WDMA_SMI_CON];
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_SMI_CON, value);

	/* WDMA_BUF_CON1 */
	value = gs[GS_WDMA_BUF_CON1];
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON1, value);

	/* WDMA BUF CONST 5 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_Y] +
	    (gs[GS_WDMA_ULTRA_LOW_Y] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON5, value);

	/* WDMA BUF CONST 6 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_Y] +
	    (gs[GS_WDMA_ULTRA_HIGH_Y] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON6, value);

	/* WDMA BUF CONST 7 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_U] +
	    (gs[GS_WDMA_ULTRA_LOW_U] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON7, value);

	/* WDMA BUF CONST 8 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_U] +
	    (gs[GS_WDMA_ULTRA_HIGH_U] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON8, value);

	/* WDMA BUF CONST 9 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_V] +
	    (gs[GS_WDMA_ULTRA_LOW_V] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON9, value);

	/* WDMA BUF CONST 10 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_V] +
	    (gs[GS_WDMA_ULTRA_HIGH_V] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON10, value);

	/* WDMA BUF CONST 11 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_Y_DVFS] +
	    (gs[GS_WDMA_ULTRA_LOW_Y_DVFS] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON11, value);

	/* WDMA BUF CONST 12 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_Y_DVFS] +
	    (gs[GS_WDMA_ULTRA_HIGH_Y_DVFS] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON12, value);

	/* WDMA BUF CONST 13 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_U_DVFS] +
	    (gs[GS_WDMA_ULTRA_LOW_U_DVFS] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON13, value);

	/* WDMA BUF CONST 14 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_U_DVFS] +
	    (gs[GS_WDMA_ULTRA_HIGH_U_DVFS] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON14, value);

	/* WDMA BUF CONST 15 */
	value = gs[GS_WDMA_PRE_ULTRA_LOW_V_DVFS] +
	    (gs[GS_WDMA_ULTRA_LOW_V_DVFS] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON15, value);

	/* WDMA BUF CONST 16 */
	value = gs[GS_WDMA_PRE_ULTRA_HIGH_V_DVFS] +
	    (gs[GS_WDMA_ULTRA_HIGH_V_DVFS] <<  16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON16, value);

	/* WDMA BUF CONST 17 */
	value = gs[GS_WDMA_DVFS_EN] +
	    (gs[GS_WDMA_DVFS_TH_Y] << 16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON17, value);

	/* WDMA BUF CONST 18 */
	value = gs[GS_WDMA_DVFS_TH_U] +
	    (gs[GS_WDMA_DVFS_TH_V] << 16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON18, value);

	/* WDMA URGENT CON0 */
	value = gs[GS_WDMA_URGENT_LOW_Y] +
	    (gs[GS_WDMA_URGENT_HIGH_Y] << 16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_URGENT_CON0, value);

	/* WDMA URGENT CON1 */
	value = gs[GS_WDMA_URGENT_LOW_U] +
	    (gs[GS_WDMA_URGENT_HIGH_U] << 16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_URGENT_CON1, value);

	/* WDMA URGENT CON2 */
	value = gs[GS_WDMA_URGENT_LOW_V] +
	    (gs[GS_WDMA_URGENT_HIGH_V] << 16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_URGENT_CON2, value);

	/* WDMA_BUF_CON3 */
	value = gs[GS_WDMA_ISSUE_REG_TH_Y] +
	    (gs[GS_WDMA_ISSUE_REG_TH_U] << 16);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON3, value);

	/* WDMA_BUF_CON4 */
	value = gs[GS_WDMA_ISSUE_REG_TH_V];
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON4, value);

	return 0;
}

static int wdma_check_input_param(struct WDMA_CONFIG_STRUCT *config)
{
	if (!is_unified_color_fmt_supported(config->outputFormat)) {
		DDP_PR_ERR("wdma parameter invalidate: outfmt %s:0x%x\n",
			   unified_color_fmt_name(config->outputFormat),
			   config->outputFormat);
		return -1;
	}

	if (config->dstAddress == 0 || config->srcWidth == 0 ||
		config->srcHeight == 0) {
		DDP_PR_ERR(
			"wdma parameter invalidate: addr=0x%lx, w=%d, h=%d\n",
			   config->dstAddress, config->srcWidth,
			   config->srcHeight);
		return -1;
	}
	return 0;
}

static int wdma_is_sec[2];

static inline int wdma_switch_to_sec(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int wdma_idx = wdma_index(module);
	/* int *wdma_is_sec = svp_pgc->module_sec.wdma_sec; */
	enum CMDQ_ENG_ENUM cmdq_engine;
	enum CMDQ_EVENT_ENUM cmdq_event;

	/* cmdq_engine = module_to_cmdq_engine(module); */
	cmdq_engine = wdma_idx == 0 ?  CMDQ_ENG_DISP_WDMA0 :
						CMDQ_ENG_DISP_WDMA1;
	cmdq_event  = wdma_idx == 0 ?  CMDQ_EVENT_DISP_WDMA0_EOF :
						CMDQ_EVENT_DISP_WDMA1_EOF;

	cmdqRecSetSecure(handle, 1);
	/* set engine as sec */
	cmdqRecSecureEnablePortSecurity(handle, (1LL << cmdq_engine));
	cmdqRecSecureEnableDAPC(handle, (1LL << cmdq_engine));
	if (wdma_is_sec[wdma_idx] == 0) {
		DDPSVPMSG("[SVP] switch wdma%d to sec\n", wdma_idx);
		mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
				 MMPROFILE_FLAG_START, 0, 0);
	}
	wdma_is_sec[wdma_idx] = 1;

	return 0;
}

int wdma_switch_to_nonsec(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int wdma_idx = wdma_index(module);

	enum CMDQ_ENG_ENUM cmdq_engine;
	enum CMDQ_EVENT_ENUM cmdq_event;
	enum CMDQ_EVENT_ENUM cmdq_event_nonsec_end;

	cmdq_engine = wdma_idx == 0 ?  CMDQ_ENG_DISP_WDMA0 :
						CMDQ_ENG_DISP_WDMA1;
	cmdq_event  = wdma_idx == 0 ?  CMDQ_EVENT_DISP_WDMA0_EOF :
						CMDQ_EVENT_DISP_WDMA1_EOF;

	if (wdma_is_sec[wdma_idx] == 1) {
		/* wdma is in sec stat, we need to switch it to nonsec */
		struct cmdqRecStruct *nonsec_switch_handle = NULL;
		int ret;

		ret = cmdqRecCreate(
				CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH,
				&(nonsec_switch_handle));
		if (ret)
			DDPAEE("[SVP]fail to create disable handle %s ret=%d\n",
			       __func__, ret);

		cmdqRecReset(nonsec_switch_handle);

		if (wdma_idx == 0) {
			/* Primary Mode */
			if (primary_display_is_decouple_mode())
				cmdqRecWaitNoClear(nonsec_switch_handle,
						   cmdq_event);
			else
				_cmdq_insert_wait_frame_done_token_mira(
							nonsec_switch_handle);
		} else {
			/* External Mode */
			/* ovl1->wdma1 */
			cmdqRecWaitNoClear(nonsec_switch_handle,
					   CMDQ_SYNC_DISP_EXT_STREAM_EOF);
		}

		cmdqRecSetSecure(nonsec_switch_handle, 1);

		/* in fact, dapc/port_sec will be disabled by cmdq */
		cmdqRecSecureEnablePortSecurity(nonsec_switch_handle,
						(1LL << cmdq_engine));
		cmdqRecSecureEnableDAPC(nonsec_switch_handle,
					(1LL << cmdq_engine));
		if (handle) {
			/* Async Flush method */
			cmdq_event_nonsec_end =
					wdma_idx == 0 ?
					CMDQ_SYNC_DISP_WDMA0_2NONSEC_END :
					CMDQ_SYNC_DISP_WDMA1_2NONSEC_END;
			cmdqRecSetEventToken(nonsec_switch_handle,
					     cmdq_event_nonsec_end);
			cmdqRecFlushAsync(nonsec_switch_handle);
			cmdqRecWait(handle, cmdq_event_nonsec_end);
		} else {
			/* Sync Flush method */
			cmdqRecFlush(nonsec_switch_handle);
		}
		cmdqRecDestroy(nonsec_switch_handle);
		DDPSVPMSG("[SVP] switch wdma%d to nonsec\n", wdma_idx);
		mmprofile_log_ex(ddp_mmp_get_events()->svp_module[module],
				 MMPROFILE_FLAG_END, 0, 0);
	}
	wdma_is_sec[wdma_idx] = 0;

	return 0;
}

int setup_wdma_sec(enum DISP_MODULE_ENUM module,
		   struct disp_ddp_path_config *pConfig, void *handle)
{
	int ret;
	int is_engine_sec = 0;

	if (pConfig->wdma_config.security == DISP_SECURE_BUFFER)
		is_engine_sec = 1;

	if (!handle) {
		DDPDBG("[SVP] bypass wdma sec setting sec=%d,handle=NULL\n",
		       is_engine_sec);
		return 0;
	}

	/* handle = NULL, use the sync flush method */
	if (is_engine_sec == 1)
		ret = wdma_switch_to_sec(module, handle);
	else
		ret = wdma_switch_to_nonsec(module, NULL);
	if (ret)
		DDPAEE("[SVP]fail to setup_ovl_sec: %s ret=%d\n",
		       __func__, ret);

	return is_engine_sec;
}

static int wdma_config_l(enum DISP_MODULE_ENUM module,
			 struct disp_ddp_path_config *pConfig, void *handle)
{
	struct WDMA_CONFIG_STRUCT *config = &pConfig->wdma_config;
	unsigned int is_primary_flag = 1; /*primary or external*/
	unsigned int bwBpp;
	unsigned long long wdma_bw;

	if (!pConfig->wdma_dirty)
		return 0;

	memcpy(&g_wdma_cfg, config,
		sizeof(struct WDMA_CONFIG_STRUCT));

	setup_wdma_sec(module, pConfig, handle);
	if (wdma_check_input_param(config) == 0) {
		struct golden_setting_context *p_golden_setting;

		if (!ufmt_get_rgb(config->outputFormat)) {
			if ((config->clipX + config->srcWidth) % 2)
				config->clipWidth -= 1;

			if ((config->clipY + config->srcHeight) % 2)
				config->clipHeight -= 1;
		}

		wdma_config(module, config->srcWidth, config->srcHeight,
			    config->clipX, config->clipY, config->clipWidth,
			    config->clipHeight, config->outputFormat,
			    config->dstAddress, config->dstPitch,
			    config->useSpecifiedAlpha, config->alpha,
			    config->security, handle);

		p_golden_setting = pConfig->p_golden_setting_context;
		wdma_golden_setting(module, p_golden_setting, is_primary_flag,
				    config->outputFormat, handle);

		/* calculate bandwidth */
		bwBpp = ufmt_get_Bpp(config->outputFormat);
		wdma_bw = (unsigned long long)config->clipWidth *
				config->clipHeight * bwBpp;
		do_div(wdma_bw, 1000);
		wdma_bw *= 1250;
		do_div(wdma_bw, 1000);
		DDPDBG("W:width=%u,height=%u,Bpp:%u,bw:%llu\n",
			config->clipWidth, config->clipHeight, bwBpp, wdma_bw);

		/* bandwidth report */
		if (module == DISP_MODULE_WDMA0) {
			DDPDBG("%s,bw%llu\n",
				ddp_get_module_name(module), wdma_bw);
			DISP_SLOT_SET(handle, DISPSYS_SLOT_BASE,
				DISP_SLOT_WDMA0_BW, (unsigned int)wdma_bw);
		}
	}
	return 0;
}

unsigned int MMPathTracePrimaryWDMA(char *str, unsigned int strlen,
	unsigned int n)
{
	n += scnprintf(str + n, strlen - n,
		"out=0x%lx, ", g_wdma_cfg.dstAddress);
	n += scnprintf(str + n, strlen - n,
		"out_width=%d, ", g_wdma_cfg.srcWidth);
	n += scnprintf(str + n, strlen - n,
		"out_height=%d ,", g_wdma_cfg.srcHeight);
	n += scnprintf(str + n, strlen - n, "out_fmt=%s, ",
		unified_color_fmt_name(g_wdma_cfg.outputFormat));
	n += scnprintf(str + n, strlen - n, "out_bpp=%u",
		ufmt_get_Bpp(g_wdma_cfg.outputFormat));

	return n;
}

struct DDP_MODULE_DRIVER ddp_driver_wdma = {
	.module = DISP_MODULE_WDMA0,
	.init = wdma_clock_on,
	.deinit = wdma_clock_off,
	.config = wdma_config_l,
	.start = wdma_start,
	.trigger = NULL,
	.stop = wdma_stop,
	.reset = wdma_reset,
	.power_on = wdma_clock_on,
	.power_off = wdma_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = wdma_dump,
	.bypass = NULL,
	.build_cmdq = NULL,
	.set_lcm_utils = NULL,
	.switch_to_nonsec = wdma_switch_to_nonsec,
};
