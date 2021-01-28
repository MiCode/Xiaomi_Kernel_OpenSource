// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "MET"

#include "ddp_log.h"
#include "disp_drv_platform.h"

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

#if defined(CONFIG_MTK_MET)
#include <linux/module.h>

extern int met_tag_oneshot_real(unsigned int class_id,
const char *name, unsigned int value);
static struct ovl_info_s {
	unsigned char ovl_idx;
	unsigned char layer_num;
} ovl_infos[OVL_NUM] = {
	{DISP_MODULE_OVL0, 4}, {DISP_MODULE_OVL0_2L, 2},
	{DISP_MODULE_OVL1_2L, 2},
};
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
#if defined(CONFIG_MTK_MET)
	static unsigned long sBufAddr[RDMA_NUM];
	static struct RDMA_BASIC_STRUCT rdmaInfo;
	char tag_name[30] = { '\0' };
	static struct OVL_BASIC_STRUCT old_ovlInfo[4+2+2];
	static struct OVL_BASIC_STRUCT ovlInfo[4+2+2];
	int layer_idx = 0;
	int layer_pos = 0;
	int b_layer_changed = 0;
	int i, j;
	int (*met_tag_oneshot_symbol)(unsigned int class_id,
		const char *name, unsigned int value) = NULL;

	met_tag_oneshot_symbol =
		(void *)symbol_get(met_tag_oneshot_real);

	if (dpp_disp_is_decouple() == 1) {

		rdma_get_info(index, &rdmaInfo);
		if (rdmaInfo.addr == 0 || (rdmaInfo.addr != 0 &&
			sBufAddr[index] != rdmaInfo.addr)) {
			sBufAddr[index] = rdmaInfo.addr;
			sprintf(tag_name, index ?
				"ExtDispRefresh" : "PrimDispRefresh");
			if (met_tag_oneshot_symbol)
				met_tag_oneshot_symbol(DDP_IRQ_FPS_ID,
					tag_name, 1);
		}
		return;
	}


	/*essential for structure comparision*/
	memset(ovlInfo, 0, sizeof(ovlInfo));

	/*Traversal layers and get layer info*/
	for (i = 0; i < OVL_NUM; i++) {
		if (i > 0)
			layer_pos += ovl_infos[i-1].layer_num;

		ovl_get_info(ovl_infos[i].ovl_idx, &(ovlInfo[layer_pos]));

		for (j = 0; j < ovl_infos[i].layer_num; j++) {
			layer_idx++;

			if (memcmp(&(ovlInfo[layer_idx]),
				&(old_ovlInfo[layer_idx]),
				sizeof(struct OVL_BASIC_STRUCT)) == 0)
				continue;

			if (ovlInfo[layer_idx].layer_en)
				b_layer_changed = 1;
		}

		/*store old value*/
		memcpy(&(old_ovlInfo[layer_pos]),
			&(ovlInfo[layer_pos]),
			ovl_infos[i].layer_num *
				sizeof(struct OVL_BASIC_STRUCT));

	}

	if (b_layer_changed) {
		sprintf(tag_name,
			index ? "ExtDispRefresh" : "PrimDispRefresh");
		if (met_tag_oneshot_symbol)
			met_tag_oneshot_symbol(DDP_IRQ_FPS_ID, tag_name, 1);
	}

#endif
}

static void ddp_disp_refresh_tag_end(unsigned int index)
{
#if defined(CONFIG_MTK_MET)
	char tag_name[30] = { '\0' };
	int (*met_tag_oneshot_symbol)(unsigned int class_id,
		const char *name, unsigned int value) = NULL;

	sprintf(tag_name, index ?
		"ExtDispRefresh" : "PrimDispRefresh");
	met_tag_oneshot_symbol =
		(void *)symbol_get(met_tag_oneshot_real);
	if (met_tag_oneshot_symbol)
		met_tag_oneshot_symbol(DDP_IRQ_FPS_ID, tag_name, 0);
#endif
}

/**
 * Represent to OVL0/0VL1 each layer's refresh rate
 */
static void ddp_inout_info_tag(unsigned int index)
{

}

static void ddp_err_irq_met_tag(const char *name)
{
#if defined(CONFIG_MTK_MET)
	int (*met_tag_oneshot_symbol)(unsigned int class_id,
		const char *name, unsigned int value) = NULL;

	met_tag_oneshot_symbol =
		(void *)symbol_get(met_tag_oneshot_real);
	if (met_tag_oneshot_symbol) {
		met_tag_oneshot_symbol(DDP_IRQ_EER_ID, name, 1);
		met_tag_oneshot_symbol(DDP_IRQ_EER_ID, name, 0);
	}
#endif
}

static void met_irq_handler(enum DISP_MODULE_ENUM module,
		unsigned int reg_val)
{
	int index = 0;
	char tag_name[30] = { '\0' };

	switch (module) {
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		index = module - DISP_MODULE_RDMA0;
		/*Always process eof prior to sof*/
		if (reg_val & (1 << 2))
			ddp_disp_refresh_tag_end(index);

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
		index = module - DISP_MODULE_OVL0;
		if (reg_val & (1 << 1))	/*EOF*/
			ddp_inout_info_tag(index);
		break;

	case DISP_MODULE_MUTEX:
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
