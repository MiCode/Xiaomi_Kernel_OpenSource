/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	{ "mediatek,kp",		0,	R12_KP_IRQ_B },
	{ "mediatek,mddriver",		0,	R12_MD1_WDT_B },
};

#endif /* __MTK_SPM_IRQ_EDGE_H__ */
