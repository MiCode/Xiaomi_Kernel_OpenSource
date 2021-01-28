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

#define LOG_TAG "MET"

#include "ddp_log.h"

#include <mt-plat/met_drv.h>
#include "ddp_irq.h"
#include "ddp_reg.h"
#include "ddp_met.h"
#include "ddp_path.h"
#include "ddp_ovl.h"
#include "ddp_rdma.h"
#include "DpDataType.h"

#define DDP_IRQ_EER_ID				(0xFFFF0000)
#define DDP_IRQ_FPS_ID				(DDP_IRQ_EER_ID + 1)
#define DDP_IRQ_LAYER_FPS_ID		(DDP_IRQ_EER_ID + 2)
#define DDP_IRQ_LAYER_SIZE_ID		(DDP_IRQ_EER_ID + 3)
#define DDP_IRQ_LAYER_FORMAT_ID	(DDP_IRQ_EER_ID + 4)

#define MAX_PATH_NUM (3)
#define RDMA_NUM (2)
#define MAX_OVL_LAYERS (4)
#define OVL_LAYER_NUM_PER_OVL (4)


static unsigned int met_tag_on;
#if 0
static const char *const parse_color_format(DpColorFormat fmt)
{
	switch (fmt) {
	case eBGR565:
		return "eBGR565";
	case eRGB565:
		return "eRGB565";
	case eRGB888:
		return "eRGB888";
	case eBGR888:
		return "eBGR888";
	case eRGBA8888:
		return "eRGBA8888";
	case eBGRA8888:
		return "eBGRA8888";
	case eARGB8888:
		return "eARGB8888";
	case eABGR8888:
		return "eABGR8888";
	case eVYUY:
		return "eVYUY";
	case eUYVY:
		return "eUYVY";
	case eYVYU:
		return "eYVYU";
	case eYUY2:
		return "eYUY2";
	default:
		return "DEFAULT";
	}
}
#endif

/**
 * check if it's decouple mode
 *
 * mutex_id  |  decouple  |  direct-link
 * -------------------------------------
 * OVL_Path  |      1     |       0
 * RDMA_Path |      0     |       X
 *
 */
int dpp_disp_is_decouple(void)
{
	if (ddp_is_moudule_in_mutex(0, DISP_MODULE_OVL0) ||
	    ddp_is_moudule_in_mutex(0, DISP_MODULE_OVL0_2L))
		return 0;
	else
		return 1;
}

/**
 * Represent to LCM display refresh rate
 * Primary Display:  map to RDMA0 sof/eof ISR, for all display mode
 * External Display: map to RDMA1 sof/eof ISR, for all display mode
 * NOTICE:
 *		for WFD, nothing we can do here
 */
static void ddp_disp_refresh_tag_start(unsigned int index)
{
	static unsigned long sBufAddr[RDMA_NUM];

	static struct RDMA_BASIC_STRUCT rdmaInfo;

	char tag_name[30] = { '\0' };

	if (dpp_disp_is_decouple() == 1) {

		rdma_get_info(index, &rdmaInfo);
		if (rdmaInfo.addr == 0 || (rdmaInfo.addr != 0 && sBufAddr[index] != rdmaInfo.addr)) {
			sBufAddr[index] = rdmaInfo.addr;
			sprintf(tag_name, index ? "ExtDispRefresh" : "PrimDispRefresh");
			met_tag_oneshot(DDP_IRQ_FPS_ID, tag_name, 1);
		}

	} else {
		static struct OVL_BASIC_STRUCT old_ovlInfo[OVL_NUM*OVL_LAYER_NUM_PER_OVL];
		static struct OVL_BASIC_STRUCT ovlInfo[OVL_NUM*OVL_LAYER_NUM_PER_OVL];
		int ovl_index;
		int b_layer_changed;
		int i, j;

		b_layer_changed = 0;

		/*Traversal layers and get layer info*/
		memset(ovlInfo, 0, sizeof(ovlInfo));/*essential for structure comparision*/

		for (i = 0; i < OVL_NUM; i++) {

			ovl_get_info(i, &(ovlInfo[i*OVL_LAYER_NUM_PER_OVL]));

			for (j = 0; j < OVL_LAYER_NUM_PER_OVL; j++) {
				ovl_index = (i * OVL_LAYER_NUM_PER_OVL) + j;

				if (memcmp(&(ovlInfo[ovl_index]), &(old_ovlInfo[ovl_index]),
						sizeof(struct OVL_BASIC_STRUCT)) == 0)
					continue;

				if (ovlInfo[ovl_index].layer_en)
					b_layer_changed = 1;
			}

			/*store old value*/
			memcpy(&(old_ovlInfo[i*OVL_LAYER_NUM_PER_OVL]),
				&(ovlInfo[i*OVL_LAYER_NUM_PER_OVL]),
				OVL_LAYER_NUM_PER_OVL*sizeof(struct OVL_BASIC_STRUCT));

		}

		if (b_layer_changed) {
			sprintf(tag_name, index ? "ExtDispRefresh" : "PrimDispRefresh");
			met_tag_oneshot(DDP_IRQ_FPS_ID, tag_name, 1);
		}

	}
}

static void ddp_disp_refresh_tag_end(unsigned int index)
{
	char tag_name[30] = { '\0' };

	sprintf(tag_name, index ? "ExtDispRefresh" : "PrimDispRefresh");
	met_tag_oneshot(DDP_IRQ_FPS_ID, tag_name, 0);
}

/**
 * Represent to OVL0/0VL1 each layer's refresh rate
 */
static void ddp_inout_info_tag(unsigned int index)
{
#if 0
	static unsigned long sLayerBufAddr[OVL_NUM][OVL_LAYER_NUM_PER_OVL];
	static unsigned int sLayerBufFmt[OVL_NUM][OVL_LAYER_NUM_PER_OVL];
	static unsigned int sLayerBufWidth[OVL_NUM][OVL_LAYER_NUM_PER_OVL];
	static unsigned int sLayerBufHeight[OVL_NUM][OVL_LAYER_NUM_PER_OVL];

	struct OVL_BASIC_STRUCT ovlInfo[OVL_NUM*OVL_LAYER_NUM_PER_OVL];
	unsigned int flag, i, idx, enLayerCnt, layerCnt;
	unsigned int width, height, bpp, fmt;
	char *fmtStr;
	char tag_name[30] = { '\0' };
	uint32_t layer_change_bits = 0;
	uint32_t layer_enable_bits = 0;

	memset((void *)ovlInfo, 0, sizeof(ovlInfo));
	ovl_get_info(index, ovlInfo);

	/* Any layer enable bit changes , new frame refreshes */
	enLayerCnt = 0;
	if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY)
		layerCnt = OVL_LAYER_NUM;
	else
		layerCnt = OVL_LAYER_NUM_PER_OVL;

	for (i = 0; i < layerCnt; i++) {
		if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY)
			index = 1 - i / OVL_LAYER_NUM_PER_OVL;

		idx = i % OVL_LAYER_NUM_PER_OVL;

		fmtStr = parse_color_format(ovlInfo[i].fmt);

		if (ovlInfo[i].layer_en) {
			enLayerCnt++;
			layer_enable_bits |= (1 << i);
			if (sLayerBufAddr[index][idx] != ovlInfo[i].addr) {
				sLayerBufAddr[index][idx] = ovlInfo[i].addr;
				sprintf(tag_name, "OVL%dL%d_InFps", index, idx);
				met_tag_oneshot(DDP_IRQ_LAYER_FPS_ID, tag_name, i+1);
				layer_change_bits |= 1 << i;
			}
#if 0
			if (sLayerBufFmt[index][idx] != ovlInfo[i].fmt) {
				sLayerBufFmt[index][idx] = ovlInfo[i].fmt;
				sprintf(tag_name, "OVL%dL%d_Fmt_%s", index, idx, fmtStr);
				met_tag_oneshot(DDP_IRQ_LAYER_FORMAT_ID, tag_name, i+1);
				layer_change_bits |= 1 << i;
			}
			if (sLayerBufWidth[index][idx] != ovlInfo[i].src_w) {
				sLayerBufWidth[index][idx] = ovlInfo[i].src_w;
				sprintf(tag_name, "OVL%dL%d_Width", index, idx);
				met_tag_oneshot(DDP_IRQ_LAYER_SIZE_ID, tag_name, ovlInfo[i].src_w);
				layer_change_bits |= 1 << i;
			}
			if (sLayerBufHeight[index][idx] != ovlInfo[i].src_h) {
				sLayerBufHeight[index][idx] = ovlInfo[i].src_h;
				sprintf(tag_name, "OVL%dL%d_Height", index, idx);
				met_tag_oneshot(DDP_IRQ_LAYER_SIZE_ID, tag_name, ovlInfo[i].src_h);
				layer_change_bits |= 1 << i;
			}
#endif
		} else {
			sLayerBufAddr[index][idx] = 0;
			sLayerBufFmt[index][idx] = 0;
			sLayerBufWidth[index][idx] = 0;
			sLayerBufHeight[index][idx] = 0;
		}

		if ((i == (OVL_LAYER_NUM_PER_OVL - 1)) || (i == (OVL_LAYER_NUM - 1))) {
			if (enLayerCnt) {
				enLayerCnt = 0;
				sprintf(tag_name, "OVL%d_OutFps", index);
				met_tag_oneshot(DDP_IRQ_LAYER_FPS_ID, tag_name, index);
			}
		}
	}


	/*CLS:met mmsys profile*/
	{
		int i;

		for (i = 0; i < OVL_LAYER_NUM; i++) {
			if (layer_change_bits & (1 << i)) {
				MET_UDTL_GET_PROP(OVL_LAYER_Props).layer	= i;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).layer_en	= layer_enable_bits;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).fmt	= ovlInfo[i].fmt;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).addr	= ovlInfo[i].addr;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).src_w	= ovlInfo[i].src_w;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).src_h	= ovlInfo[i].src_h;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).src_pitch	= ovlInfo[i].src_pitch;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).bpp	= ovlInfo[i].bpp;

				MET_UDTL_TRACELINE_PROP(MMSYS, OVL_LAYERS__LAYER, OVL_LAYER_Props);
			}
		}
	}


#endif

}

static void ddp_err_irq_met_tag(const char *name)
{
	met_tag_oneshot(DDP_IRQ_EER_ID, name, 1);
	met_tag_oneshot(DDP_IRQ_EER_ID, name, 0);
}

static void met_irq_handler(enum DISP_MODULE_ENUM module, unsigned int reg_val)
{
	int index = 0;
	char tag_name[30] = { '\0' };
	int mutexID;
	/* DDPERR("met_irq_handler() module=%d, val=0x%x\n", module, reg_val); */
	switch (module) {
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		index = module - DISP_MODULE_RDMA0;
		if (reg_val & (1 << 2))
			ddp_disp_refresh_tag_end(index);/*Always process eof prior to sof*/

		if (reg_val & (1 << 1))
			ddp_disp_refresh_tag_start(index);

		if (reg_val & (1 << 4)) {
			sprintf(tag_name, "rdma%d_underflow", index);
			ddp_err_irq_met_tag(tag_name);
		}
		if (reg_val & (1 << 3)) {
			sprintf(tag_name, "rdma%d_abnormal", index);
			ddp_err_irq_met_tag(tag_name);
		}
		break;

	case DISP_MODULE_OVL0:
	/*case DISP_MODULE_OVL1:*/
		index = module - DISP_MODULE_OVL0;
		if (reg_val & (1 << 1)) {/*EOF*/
			ddp_inout_info_tag(index);
			if (met_mmsys_event_disp_ovl_eof)
				met_mmsys_event_disp_ovl_eof(index);
		}

		break;

	case DISP_MODULE_MUTEX:
		/*reg_val is  DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & 0x7C1F; */
		for (mutexID = DISP_MUTEX_DDP_FIRST; mutexID <= DISP_MUTEX_DDP_LAST; mutexID++) {
			if (reg_val & (0x1<<mutexID))
				if (met_mmsys_event_disp_sof)
					met_mmsys_event_disp_sof(mutexID);

			if (reg_val & (0x1<<(mutexID+DISP_MUTEX_TOTAL)))
				if (met_mmsys_event_disp_mutex_eof)
					met_mmsys_event_disp_mutex_eof(mutexID);
		}
		break;

	default:
		break;
	}
}

void ddp_init_met_tag(int state, int rdma0_mode, int rdma1_mode)
{
	if ((!met_tag_on) && state) {
		met_tag_on = state;
		disp_register_irq_callback(met_irq_handler);
	}
	if (met_tag_on && (!state)) {
		met_tag_on = state;
		disp_unregister_irq_callback(met_irq_handler);
	}
}
EXPORT_SYMBOL(ddp_init_met_tag);
