/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */


/*******************************************************************************
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
 *------------------------------------------------------------------------------
 *
 ******************************************************************************/

#ifndef _AUDIO_CODEC_63xx_H
#define _AUDIO_CODEC_63xx_H

#define CODEC_MT6358_NAME "mtk-codec-mt6358"

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

#endif
