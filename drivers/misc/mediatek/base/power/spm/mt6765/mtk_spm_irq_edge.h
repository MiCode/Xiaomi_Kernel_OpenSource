/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_SPM_IRQ_EDGE_H__
#define __MTK_SPM_IRQ_EDGE_H__

/*************************************************************
 * Define spm edge trigger interrupt list according to chip.
 * Only included by mtk_spm_irq.c
 ************************************************************/

struct edge_trigger_irq_list {
	const char *name;
	int order;
	unsigned int wakesrc;
};

static struct edge_trigger_irq_list list[] = {
	{ "mediatek,infracfg_ao",   0,	0 },
	{ "mediatek,kp",            0,  WAKE_SRC_R12_KP_IRQ_B },
	{ "mediatek,mdcldma",       3,  WAKE_SRC_R12_CLDMA_EVENT_B },
};

#endif /* __MTK_SPM_IRQ_EDGE_H__ */
