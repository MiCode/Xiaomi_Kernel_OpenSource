/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/******************************************************************************
 *
 * Filename:
 * ---------
 *  mtk-soc-codec-63xx.h
 *
 * Project:
 * --------
 *    Audio codec header file
 *
 * Description:
 * ------------
 *   Audio codec function
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *-----------------------------------------------------------------------------
 *
 *****************************************************************************/

#ifndef _AUDIO_CODEC_63xx_H
#define _AUDIO_CODEC_63xx_H

#define CODEC_MT6357_NAME "mtk-codec-mt6357"

struct mtk_codec_ops {
	int (*enable_dc_compensation)(bool enable);
	int (*set_lch_dc_compensation)(int value);
	int (*set_rch_dc_compensation)(int value);

	int (*set_ap_dmic)(bool enable);

	int (*set_hp_impedance_ctl)(bool enable);
};

void audckbufEnable(bool enable);

void SetAnalogSuspend(bool bEnable);

int set_codec_ops(struct mtk_codec_ops *ops);

#ifdef CONFIG_MTK_ACCDET
extern void accdet_late_init(unsigned long a);
#endif

/*AKITA-5 M8 audio bring up begin */
#ifdef SND_SOC_AW87519
extern unsigned char aw87519_amp_lch_on(void);
extern unsigned char aw87519_amp_lch_off(void);
extern unsigned char aw87519_amp_rch_on(void);
extern unsigned char aw87519_amp_rch_off(void);
#endif
/*AKITA-5 - end */

#endif
