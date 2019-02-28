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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
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
