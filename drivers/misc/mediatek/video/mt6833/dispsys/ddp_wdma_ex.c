/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#define ALIGN_TO(x, n)	(((x) + ((n) - 1)) & ~((n) - 1))

/*****************************************************************************/
unsigned int wdma_index(enum DISP_MODULE_ENUM module)
{
	int idx = 0;

	switch (module) {
	case DISP_MODULE_WDMA0:
		idx = 0;
		break;
	default:
		DDPERR("[DDP] error: invalid wdma module=%d\n",
			module); /* invalid module */
		ASSERT(0);
	}
	return idx;
}

int wdma_stop(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = wdma_index(module);
	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;

	DISP_REG_SET(handle, offset + DISP_REG_WDMA_INTEN, 0x00);
	DISP_REG_SET(handle, offset + DISP_REG_WDMA_EN, 0x00);
	DISP_REG_SET(handle, offset + DISP_REG_WDMA_INTSTA, 0x00);

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
				DDPERR("wdma%d reset timeout!\n", idx);
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

/*****************************************************************************/
static char *wdma_get_status(unsigned int status)
{
	switch (status) {
	case 0x1:
		return "idle";
	case 0x2:
		return "clear";
	case 0x4:
		return "prepare";
	case 0x8:
		return "prepare";
	case 0x10:
		return "data_running";
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
	return "unknown";
}

int wdma_start(enum DISP_MODULE_ENUM module, void *handle)
{
	unsigned int idx = wdma_index(module);
	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;

	DISP_REG_SET(handle, offset + DISP_REG_WDMA_INTEN, 0x03);

	DISP_REG_SET_FIELD(handle, WDMA_EN_FLD_ENABLE,
		offset + DISP_REG_WDMA_EN, 0x1);

	DISP_REG_SET_FIELD(handle, WDMA_SHADOW_FLD_READ_SHADOW,
		offset + DISP_REG_WDMA_SHADOW_CTL, 0x1);
	DISP_REG_SET_FIELD(handle, WDMA_SHADOW_FLD_BYPASS_SHADOW,
		offset + DISP_REG_WDMA_SHADOW_CTL, 0x1);
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
			DISP_REG_SET(handle,
				idx_offst + DISP_REG_WDMA_DST_ADDR2,
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

static int wdma_config(enum DISP_MODULE_ENUM module, unsigned int srcWidth,
	unsigned int srcHeight, unsigned int clipX, unsigned int clipY,
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

	DDPDBG(
		"%s,src(%dx%d),clip(%d,%d,%dx%d),fmt=%s,addr=0x%lx,pitch=%d,s_alfa=%d,alpa=%d,hnd=0x%p,sec%d\n",
	     ddp_get_module_name(module), srcWidth, srcHeight,
	     clipX, clipY, clipWidth, clipHeight,
	     unified_color_fmt_name(out_format),
	     dstAddress, dstPitch, useSpecifiedAlpha, alpha,
	     handle, sec);

	/* should use OVL alpha instead of sw config */
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_SRC_SIZE,
		srcHeight << 16 | srcWidth);
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_CLIP_COORD,
		clipY << 16 | clipX);
	DISP_REG_SET(handle, idx_offst + DISP_REG_WDMA_CLIP_SIZE,
		clipHeight << 16 | clipWidth);
	DISP_REG_SET_FIELD(handle, CFG_FLD_OUT_FORMAT,
		idx_offst + DISP_REG_WDMA_CFG, out_fmt_reg);

	if (!is_rgb) {
		/* set DNSP for UYVY and YUV_3P format for better quality */
		wdma_config_yuv420(module, out_format, dstPitch,
			clipHeight, dstAddress, sec, handle);
		/*user internal matrix */
		DISP_REG_SET_FIELD(handle, CFG_FLD_EXT_MTX_EN,
			idx_offst + DISP_REG_WDMA_CFG, 0);
		DISP_REG_SET_FIELD(handle, CFG_FLD_CT_EN,
			idx_offst + DISP_REG_WDMA_CFG, 1);
		DISP_REG_SET_FIELD(handle, CFG_FLD_INT_MTX_SEL,
			idx_offst + DISP_REG_WDMA_CFG, color_matrix);
	} else {
		DISP_REG_SET_FIELD(handle, CFG_FLD_EXT_MTX_EN,
			idx_offst + DISP_REG_WDMA_CFG, 0);
		DISP_REG_SET_FIELD(handle, CFG_FLD_CT_EN,
			idx_offst + DISP_REG_WDMA_CFG, 0);
	}
	DISP_REG_SET_FIELD(handle, CFG_FLD_SWAP,
		idx_offst + DISP_REG_WDMA_CFG, output_swap);
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
	DISP_REG_SET(handle,
		idx_offst + DISP_REG_WDMA_DST_W_IN_BYTE, dstPitch);
	DISP_REG_SET_FIELD(handle,
		ALPHA_FLD_A_SEL, idx_offst + DISP_REG_WDMA_ALPHA,
		useSpecifiedAlpha);
	DISP_REG_SET_FIELD(handle,
		ALPHA_FLD_A_VALUE, idx_offst + DISP_REG_WDMA_ALPHA, alpha);

	return 0;
}

static int wdma_clock_on(enum DISP_MODULE_ENUM module, void *handle)
{
#ifdef ENABLE_CLK_MGR
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
#endif
	return 0;
}

static int wdma_clock_off(enum DISP_MODULE_ENUM module, void *handle)
{
#ifdef ENABLE_CLK_MGR
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
#endif
	return 0;
}

void wdma_dump_golden_setting(enum DISP_MODULE_ENUM module)
{
	unsigned int index = wdma_index(module);
	unsigned int idx_offst = index * DISP_WDMA_INDEX_OFFSET;

	DDPDUMP("dump WDMA golden setting\n");

	DDPDUMP(
		"WDMA_SMI_CON:\n[3:0]:%x [4:4]:%x [7:5]:%x [15:8]:%x [19:16]:%u [23:20]:%u [27:24]:%u\n",
		DISP_REG_GET_FIELD(SMI_CON_FLD_THRESHOLD,
			idx_offst + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SLOW_ENABLE,
			idx_offst + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SLOW_LEVEL,
			idx_offst + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SLOW_COUNT,
			idx_offst + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_Y_REPEAT_NUM,
			idx_offst + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_U_REPEAT_NUM,
			idx_offst + DISP_REG_WDMA_SMI_CON),
		DISP_REG_GET_FIELD(SMI_CON_FLD_SMI_V_REPEAT_NUM,
			idx_offst + DISP_REG_WDMA_SMI_CON));
	DDPDUMP("WDMA_BUF_CON1:\n[31]:%x [30]:%x [28]:%x [8:0]%d\n",
		DISP_REG_GET_FIELD(BUF_CON1_FLD_ULTRA_ENABLE,
			idx_offst + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_PRE_ULTRA_ENABLE,
			idx_offst + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_FRAME_END_ULTRA,
			idx_offst + DISP_REG_WDMA_BUF_CON1),
		DISP_REG_GET_FIELD(BUF_CON1_FLD_FIFO_PSEUDO_SIZE,
			idx_offst + DISP_REG_WDMA_BUF_CON1));
	DDPDUMP("WDMA_BUF_CON5:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON5),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON5));
	DDPDUMP("WDMA_BUF_CON6:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON6),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON6));
	DDPDUMP("WDMA_BUF_CON7:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON7),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON7));
	DDPDUMP("WDMA_BUF_CON8:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON8),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON8));
	DDPDUMP("WDMA_BUF_CON9:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON9),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON9));
	DDPDUMP("WDMA_BUF_CON10:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON10),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON10));
	DDPDUMP("WDMA_BUF_CON11:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON11),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON11));
	DDPDUMP("WDMA_BUF_CON12:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON12),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON12));
	DDPDUMP("WDMA_BUF_CON13:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON13),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON13));
	DDPDUMP("WDMA_BUF_CON14:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON14),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON14));
	DDPDUMP("WDMA_BUF_CON15:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON15),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_LOW,
			idx_offst + DISP_REG_WDMA_BUF_CON15));
	DDPDUMP("WDMA_BUF_CON16:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON_FLD_PRE_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON16),
		DISP_REG_GET_FIELD(BUF_CON_FLD_ULTRA_HIGH,
			idx_offst + DISP_REG_WDMA_BUF_CON16));
	DDPDUMP("WDMA_BUF_CON17:\n[0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON17_FLD_WDMA_DVFS_EN,
			idx_offst + DISP_REG_WDMA_BUF_CON17),
		DISP_REG_GET_FIELD(BUF_CON17_FLD_DVFS_TH_Y,
			idx_offst + DISP_REG_WDMA_BUF_CON17));
	DDPDUMP("WDMA_BUF_CON18:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON18_FLD_DVFS_TH_U,
			idx_offst + DISP_REG_WDMA_BUF_CON18),
		DISP_REG_GET_FIELD(BUF_CON18_FLD_DVFS_TH_V,
			idx_offst + DISP_REG_WDMA_BUF_CON18));
	DDPDUMP("WDMA_DRS_CON0:\n[0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(WDMA_DRS_EN,
			idx_offst + DISP_REG_WDMA_DRS_CON0),
		DISP_REG_GET_FIELD(BUF_DRS_FLD_ENTER_DRS_TH_Y,
			idx_offst + DISP_REG_WDMA_DRS_CON0));
	DDPDUMP("WDMA_DRS_CON1:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_DRS_FLD_ENTER_DRS_TH_U,
			idx_offst + DISP_REG_WDMA_DRS_CON1),
		DISP_REG_GET_FIELD(BUF_DRS_FLD_ENTER_DRS_TH_V,
			idx_offst + DISP_REG_WDMA_DRS_CON1));
	DDPDUMP("WDMA_DRS_CON2:\n[25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_DRS_FLD_LEAVE_DRS_TH_Y,
			idx_offst + DISP_REG_WDMA_DRS_CON2));
	DDPDUMP("WDMA_DRS_CON3:\n[9:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_DRS_FLD_LEAVE_DRS_TH_U,
			idx_offst + DISP_REG_WDMA_DRS_CON3),
		DISP_REG_GET_FIELD(BUF_DRS_FLD_LEAVE_DRS_TH_V,
			idx_offst + DISP_REG_WDMA_DRS_CON3));
	DDPDUMP("WDMA_BUF_CON3:\n[8:0]:%d [25:16]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON3_FLD_ISSUE_REQ_TH_Y,
			idx_offst + DISP_REG_WDMA_BUF_CON3),
		DISP_REG_GET_FIELD(BUF_CON3_FLD_ISSUE_REQ_TH_U,
			idx_offst + DISP_REG_WDMA_BUF_CON3));
	DDPDUMP("WDMA_BUF_CON4:\n[8:0]:%d\n",
		DISP_REG_GET_FIELD(BUF_CON4_FLD_ISSUE_REQ_TH_V,
			idx_offst + DISP_REG_WDMA_BUF_CON4));
}

void wdma_dump_analysis(enum DISP_MODULE_ENUM module)
{
	unsigned int index = wdma_index(module);
	unsigned int idx_offst = index * DISP_WDMA_INDEX_OFFSET;

	DDPDUMP("== DISP WDMA%d ANALYSIS ==\n", index);
	DDPDUMP(
		"wdma%d:en=%d,w=%d,h=%d,clip=(%d,%d,%dx%d),pitch=(W=%d,UV=%d),addr=(0x%x,0x%x,0x%x),fmt=%s\n",
		index, DISP_REG_GET(DISP_REG_WDMA_EN + idx_offst) & 0x01,
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
		"wdma%d:status=%s,in_req=%d(prev sent data),in_ack=%d(ask data to prev),exec=%d,in_pix=(L:%d,P:%d)\n",
		index,
		wdma_get_status(DISP_REG_GET_FIELD
				(FLOW_CTRL_DBG_FLD_WDMA_STA_FLOW_CTRL,
				 DISP_REG_WDMA_FLOW_CTRL_DBG + idx_offst)),
		DISP_REG_GET_FIELD(EXEC_DBG_FLD_WDMA_IN_REQ,
				   DISP_REG_WDMA_FLOW_CTRL_DBG + idx_offst),
		DISP_REG_GET_FIELD(EXEC_DBG_FLD_WDMA_IN_ACK,
				   DISP_REG_WDMA_FLOW_CTRL_DBG + idx_offst),
		DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG + idx_offst) & 0x1f,
		(DISP_REG_GET(DISP_REG_WDMA_CT_DBG + idx_offst) >> 16) & 0xffff,
		DISP_REG_GET(DISP_REG_WDMA_CT_DBG + idx_offst) & 0xffff);

	wdma_dump_golden_setting(module);

}

void wdma_dump_reg(enum DISP_MODULE_ENUM module)
{
	if (disp_helper_get_option(DISP_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned int idx = wdma_index(module);
		unsigned long module_base = DISPSYS_WDMA0_BASE +
			idx * DISP_WDMA_INDEX_OFFSET;

		DDPDUMP("== START: DISP WDMA%d REGS ==\n", idx);
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

		DDPDUMP("== DISP WDMA%d REGS ==\n", idx);

		DDPDUMP("0x000: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_INTEN + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_INTSTA + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_EN + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_RST + off_sft));

		DDPDUMP("0x010: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_SMI_CON + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_CFG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE + off_sft));

		DDPDUMP("0x020=0x%08x,0x028=0x%08x,0x02c=0x%08x,0x038=0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_W_IN_BYTE + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_ALPHA + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON1 + off_sft));

		DDPDUMP("0x03c=0x%08x,0x058=0x%08x,0x05c=0x%08x,0x060=0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_BUF_CON2 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_PRE_ADD0 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_PRE_ADD2 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_POST_ADD0 + off_sft));

		DDPDUMP("0x064=0x%08x,0x078=0x%08x,0x080=0x%08x,0x084=0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_POST_ADD2 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET0 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET1 + off_sft));

		DDPDUMP("0x088=0x%08x,0x0a0=0x%08x,0x0a4=0x%08x,0x0a8=0x%08x\n",
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR_OFFSET2 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_FLOW_CTRL_DBG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_CT_DBG + off_sft));

		DDPDUMP(
			"0x0ac=0x%08x,0xf00=0x%08x,0xf04=0x%08x,0xf08=0x%08x,\n",
			DISP_REG_GET(DISP_REG_WDMA_DEBUG + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR0 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR1 + off_sft),
			DISP_REG_GET(DISP_REG_WDMA_DST_ADDR2 + off_sft));
	}
}

static int wdma_dump(enum DISP_MODULE_ENUM module, int level)
{
	wdma_dump_analysis(module);
	wdma_dump_golden_setting(module);
	wdma_dump_reg(module);

	return 0;
}

static int
wdma_golden_setting(enum DISP_MODULE_ENUM module,
	struct golden_setting_context *p_golden_setting,
	unsigned int is_primary_flag, void *cmdq)
{
	unsigned int regval;
	unsigned int idx = wdma_index(module);
	unsigned long res;
	unsigned int ultra_low_us = 6;
	unsigned int ultra_high_us = 4;
	unsigned int preultra_low_us = 7;
	unsigned int preultra_high_us = ultra_low_us;
	unsigned int fifo_pseudo_size = 288;
	unsigned int frame_rate = 60;
	unsigned int bytes_per_sec = 3;
	/*unsigned int is_primary_flag = 1;*/  /*primary or external*/
	long long temp;

	unsigned int fifo_off_drs_enter = 3;
	unsigned int fifo_off_drs_leave = 2;
	unsigned int fifo_off_dvfs = 2;

	unsigned long long consume_rate = 0;
	unsigned long long ultra_low;
	unsigned long long preultra_low;
	unsigned long long preultra_high;
	unsigned long long ultra_high;

	unsigned long long ultra_low_UV;
	unsigned long long preultra_low_UV;
	unsigned long long preultra_high_UV;
	unsigned long long ultra_high_UV;

	unsigned int offset = idx * DISP_WDMA_INDEX_OFFSET;

	if (!p_golden_setting) {
		DDPERR("golden setting is null, %s,%d\n", __FILE__, __LINE__);
		ASSERT(0);
		return 0;
	}

	frame_rate = p_golden_setting->fps;

	if (is_primary_flag) {
		fifo_off_drs_enter = 3;
		fifo_off_drs_leave = 2;
		fifo_off_dvfs = 2;
		res = p_golden_setting->dst_width *
			p_golden_setting->dst_height;
	} else {
		res = p_golden_setting->ext_dst_width *
				p_golden_setting->ext_dst_height;
		if ((p_golden_setting->ext_dst_width == 3840 &&
			p_golden_setting->ext_dst_height == 2160))
			frame_rate = 30;
	}

	/* DISP_REG_WDMA_SMI_CON */
	regval = 0;
	regval |= REG_FLD_VAL(SMI_CON_FLD_THRESHOLD, 7);
	regval |= REG_FLD_VAL(SMI_CON_FLD_SLOW_ENABLE, 0);
	regval |= REG_FLD_VAL(SMI_CON_FLD_SLOW_LEVEL, 0);
	regval |= REG_FLD_VAL(SMI_CON_FLD_SLOW_COUNT, 0);
	regval |= REG_FLD_VAL(SMI_CON_FLD_SMI_Y_REPEAT_NUM, 4);
	regval |= REG_FLD_VAL(SMI_CON_FLD_SMI_U_REPEAT_NUM, 2);
	regval |= REG_FLD_VAL(SMI_CON_FLD_SMI_V_REPEAT_NUM, 2);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_SMI_CON, regval);

	/* DISP_REG_WDMA_BUF_CON1 */
	regval = 0;
	if (p_golden_setting->is_dc)
		regval |= REG_FLD_VAL(BUF_CON1_FLD_ULTRA_ENABLE, 0);
	else
		regval |= REG_FLD_VAL(BUF_CON1_FLD_ULTRA_ENABLE, 1);

	regval |= REG_FLD_VAL(BUF_CON1_FLD_PRE_ULTRA_ENABLE, 1);

	if (p_golden_setting->is_dc)
		regval |= REG_FLD_VAL(BUF_CON1_FLD_FRAME_END_ULTRA, 0);
	else
		regval |= REG_FLD_VAL(BUF_CON1_FLD_FRAME_END_ULTRA, 1);

	regval |= REG_FLD_VAL(BUF_CON1_FLD_FIFO_PSEUDO_SIZE,
		fifo_pseudo_size);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON1, regval);

	/* DISP_REG_WDMA_BUF_CON3 */
	regval = 0;
	regval |= REG_FLD_VAL(BUF_CON3_FLD_ISSUE_REQ_TH_Y, 16);
	regval |= REG_FLD_VAL(BUF_CON3_FLD_ISSUE_REQ_TH_U, 16);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON3, regval);

	/* DISP_REG_WDMA_BUF_CON4 */
	regval = 0;
	regval |= REG_FLD_VAL(BUF_CON4_FLD_ISSUE_REQ_TH_V, 16);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON4, regval);

	consume_rate = res * frame_rate;
	do_div(consume_rate, 1000);
	consume_rate *= 1250;
	do_div(consume_rate, 16 * 1000);

	preultra_low = preultra_low_us *
		consume_rate * bytes_per_sec;
	preultra_low_UV = preultra_low_us *
		consume_rate;
	do_div(preultra_low, 100);
	preultra_low = DIV_ROUND_UP((unsigned int)preultra_low, 10);
	do_div(preultra_low_UV, 10);
	preultra_low_UV = DIV_ROUND_UP((unsigned int)preultra_low_UV, 100);

	preultra_high = preultra_high_us *
		consume_rate * bytes_per_sec;
	preultra_high_UV = preultra_high_us *
		consume_rate;
	do_div(preultra_high, 100);
	preultra_high = DIV_ROUND_UP((unsigned int)preultra_high, 10);
	do_div(preultra_high_UV, 100);
	preultra_high_UV = DIV_ROUND_UP((unsigned int)preultra_high_UV, 10);

	ultra_high = ultra_high_us *
		consume_rate * bytes_per_sec;
	ultra_high_UV = ultra_high_us *
		consume_rate;
	do_div(ultra_high, 100);
	ultra_high = DIV_ROUND_UP((unsigned int)ultra_high, 10);
	do_div(ultra_high_UV, 100);
	ultra_high_UV = DIV_ROUND_UP((unsigned int)ultra_high_UV, 10);

	ultra_low = preultra_high;
	ultra_low_UV = preultra_high_UV;

	/* DISP_REG_WDMA_BUF_CON5  Y*/
	regval = 0;
	temp = fifo_pseudo_size - preultra_low;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_LOW, temp);
	temp = fifo_pseudo_size - ultra_low;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_LOW, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON5, regval);

	/* DISP_REG_WDMA_BUF_CON6 Y*/
	regval = 0;
	temp = fifo_pseudo_size - preultra_high;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_HIGH, temp);
	temp = fifo_pseudo_size - ultra_high;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_HIGH, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON6, regval);

	/* DISP_REG_WDMA_BUF_CON7 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_LOW, temp);
	temp = DIV_ROUND_UP(ultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_LOW, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON7, regval);

	/* DISP_REG_WDMA_BUF_CON8 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_HIGH, temp);
	temp = DIV_ROUND_UP(ultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_HIGH, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON8, regval);

	/* DISP_REG_WDMA_BUF_CON9 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_LOW, temp);
	temp = DIV_ROUND_UP(ultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_LOW, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON9, regval);

	/* DISP_REG_WDMA_BUF_CON10 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_HIGH, temp);
	temp = DIV_ROUND_UP(ultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_HIGH, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON10, regval);

	/* DVFS */
	preultra_low = (preultra_low_us + fifo_off_dvfs) *
		consume_rate * bytes_per_sec;
	preultra_low_UV = (preultra_low_us + fifo_off_dvfs) *
		consume_rate;
	do_div(preultra_low, 100);
	preultra_low = DIV_ROUND_UP((unsigned int)preultra_low, 10);
	do_div(preultra_low_UV, 100);
	preultra_low_UV = DIV_ROUND_UP((unsigned int)preultra_low_UV, 10);

	preultra_high = (preultra_high_us + fifo_off_dvfs) *
		consume_rate * bytes_per_sec;
	preultra_high_UV = (preultra_high_us + fifo_off_dvfs) *
		consume_rate;
	do_div(preultra_high, 100);
	preultra_high = DIV_ROUND_UP((unsigned int)preultra_high, 10);
	do_div(preultra_high_UV, 100);
	preultra_high_UV = DIV_ROUND_UP((unsigned int)preultra_high_UV, 10);

	ultra_high = (ultra_high_us + fifo_off_dvfs) *
		consume_rate * bytes_per_sec;
	ultra_high_UV = (ultra_high_us + fifo_off_dvfs) * consume_rate;
	do_div(ultra_high, 100);
	ultra_high = DIV_ROUND_UP((unsigned int)ultra_high, 10);
	do_div(ultra_high_UV, 100);
	ultra_high_UV = DIV_ROUND_UP((unsigned int)ultra_high_UV, 10);

	ultra_low = preultra_high;
	ultra_low_UV = preultra_high_UV;

	/* DISP_REG_WDMA_BUF_CON11 */
	regval = 0;
	temp = fifo_pseudo_size - preultra_low;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_LOW, temp);
	temp = fifo_pseudo_size - ultra_low;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_LOW, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON11, regval);

	/* DISP_REG_WDMA_BUF_CON12 */
	regval = 0;
	temp = fifo_pseudo_size - preultra_high;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_HIGH, temp);
	temp = fifo_pseudo_size - ultra_high;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_HIGH, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON12, regval);

	/* DISP_REG_WDMA_BUF_CON13 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_LOW, temp);
	temp = DIV_ROUND_UP(ultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_LOW, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON13, regval);

	/* DISP_REG_WDMA_BUF_CON14 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_HIGH, temp);
	temp = DIV_ROUND_UP(ultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_HIGH, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON14, regval);

	/* DISP_REG_WDMA_BUF_CON15 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_LOW, temp);
	temp = DIV_ROUND_UP(ultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_LOW, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON15, regval);

	/* DISP_REG_WDMA_BUF_CON16 */
	regval = 0;
	temp = DIV_ROUND_UP(preultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_PRE_ULTRA_HIGH, temp);
	temp = DIV_ROUND_UP(ultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON_FLD_ULTRA_HIGH, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON16, regval);

	/* DISP_REG_WDMA_BUF_CON17 */
	regval = 0;
	/* TODO: SET DVFS_EN */
	temp = fifo_pseudo_size - ultra_high;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON17_FLD_DVFS_TH_Y, temp);

	if (p_golden_setting->is_dc)
		regval |= REG_FLD_VAL(BUF_CON17_FLD_WDMA_DVFS_EN, 0);
	else
		regval |= REG_FLD_VAL(BUF_CON17_FLD_WDMA_DVFS_EN, 1);
	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON17, regval);

	/* DISP_REG_WDMA_BUF_CON18 */
	regval = 0;
	temp = DIV_ROUND_UP(ultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON18_FLD_DVFS_TH_U, temp);
	temp = DIV_ROUND_UP(ultra_high_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_CON18_FLD_DVFS_TH_V, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_BUF_CON18, regval);


	/* DRS */
	preultra_high = (preultra_high_us - fifo_off_drs_enter) *
		consume_rate * bytes_per_sec;
	preultra_high_UV = (preultra_high_us - fifo_off_drs_enter) *
		consume_rate;
	do_div(preultra_high, 100);
	preultra_high = DIV_ROUND_UP((unsigned int)preultra_high, 10);
	do_div(preultra_high_UV, 100);
	preultra_high_UV = DIV_ROUND_UP((unsigned int)preultra_high_UV, 10);

	ultra_low = preultra_high;
	ultra_low_UV = preultra_high_UV;

	/* DISP_REG_WDMA_DRS_CON0 */
	regval = 0;
	/* TODO: SET DRS_EN */
	temp = fifo_pseudo_size - ultra_low;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_DRS_FLD_ENTER_DRS_TH_Y, temp);

	if (p_golden_setting->is_dc)
		regval |= REG_FLD_VAL(WDMA_DRS_EN, 0);
	else
		regval |= REG_FLD_VAL(WDMA_DRS_EN, 1);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_DRS_CON0, regval);

	/* DISP_REG_WDMA_DRS_CON1 */
	regval = 0;
	temp = DIV_ROUND_UP(ultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_DRS_FLD_ENTER_DRS_TH_U, temp);
	regval |= REG_FLD_VAL(BUF_DRS_FLD_ENTER_DRS_TH_V, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_DRS_CON1, regval);

	ultra_low = (ultra_low_us - fifo_off_drs_leave) *
		consume_rate * bytes_per_sec;
	ultra_low_UV = (ultra_low_us - fifo_off_drs_leave) *
		consume_rate;
	do_div(ultra_low, 100);
	ultra_low = DIV_ROUND_UP((unsigned int)ultra_low, 10);
	do_div(ultra_low_UV, 100);
	ultra_low_UV = DIV_ROUND_UP((unsigned int)ultra_low_UV, 10);

	regval = 0;
	temp = fifo_pseudo_size - ultra_low;
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_DRS_FLD_LEAVE_DRS_TH_Y, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_DRS_CON2, regval);

	regval = 0;
	temp = DIV_ROUND_UP(ultra_low_UV, 4);
	temp = (long long)fifo_pseudo_size - (temp * 4);
	temp = DIV_ROUND_UP(temp, 4);
	temp = (temp > 0) ? temp : 16;
	regval |= REG_FLD_VAL(BUF_DRS_FLD_LEAVE_DRS_TH_U, temp);
	regval |= REG_FLD_VAL(BUF_DRS_FLD_LEAVE_DRS_TH_V, temp);

	DISP_REG_SET(cmdq, offset + DISP_REG_WDMA_DRS_CON3, regval);

	return 0;
}


static int wdma_check_input_param(struct WDMA_CONFIG_STRUCT *config)
{
	if (!is_unified_color_fmt_supported(config->outputFormat)) {
		DDPERR("wdma parameter invalidate outfmt %s:0x%x\n",
			unified_color_fmt_name(config->outputFormat),
			config->outputFormat);
		return -1;
	}

	if (config->dstAddress == 0 || config->srcWidth == 0 ||
		config->srcHeight == 0) {
		DDPERR("wdma parameter invalidate, addr=0x%lx, w=%d, h=%d\n",
			config->dstAddress, config->srcWidth,
			config->srcHeight);
		return -1;
	}
	return 0;
}

static int wdma_is_sec[2];
static inline int wdma_switch_to_sec(enum DISP_MODULE_ENUM module,
	void *handle)
{
	unsigned int wdma_idx = wdma_index(module);
	/*int *wdma_is_sec = svp_pgc->module_sec.wdma_sec;*/
	enum CMDQ_ENG_ENUM cmdq_engine;
	enum CMDQ_EVENT_ENUM cmdq_event;

	/*cmdq_engine = module_to_cmdq_engine(module);*/
	cmdq_engine = wdma_idx == 0 ?
		CMDQ_ENG_DISP_WDMA0 : CMDQ_ENG_DISP_WDMA1;
	cmdq_event  = wdma_idx == 0 ?
		CMDQ_EVENT_DISP_WDMA0_EOF : CMDQ_EVENT_DISP_WDMA1_EOF;

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

	cmdq_engine = wdma_idx == 0 ?
		CMDQ_ENG_DISP_WDMA0 : CMDQ_ENG_DISP_WDMA1;
	cmdq_event  = wdma_idx == 0 ?
		CMDQ_EVENT_DISP_WDMA0_EOF : CMDQ_EVENT_DISP_WDMA1_EOF;

	if (wdma_is_sec[wdma_idx] == 1) {
		/* wdma is in sec stat, we need to switch it to nonsec */
		struct cmdqRecStruct *nonsec_switch_handle;
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
		if (handle != NULL) {
			/* Async Flush method */
			enum CMDQ_EVENT_ENUM cmdq_event_nonsec_end;

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

	/* hadle = NULL, use the sync flush method */
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

	setup_wdma_sec(module, pConfig, handle);
	if (wdma_check_input_param(config) == 0) {
		struct golden_setting_context *p_golden_setting;

		wdma_config(module,
		    config->srcWidth,
		    config->srcHeight,
		    config->clipX,
		    config->clipY,
		    config->clipWidth,
		    config->clipHeight,
		    config->outputFormat,
		    config->dstAddress,
		    config->dstPitch,
		    config->useSpecifiedAlpha,
		    config->alpha, config->security, handle);

		p_golden_setting = pConfig->p_golden_setting_context;
		wdma_golden_setting(module, p_golden_setting,
			is_primary_flag, handle);

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
