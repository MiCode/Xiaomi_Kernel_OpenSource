/*
 * omap-abe-dsp.h
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP_ABE_DSP_H__
#define __OMAP_ABE_DSP_H__

#define ABE_MIXER(x)		(x)

#define MIX_DL1_TONES		ABE_MIXER(0)
#define MIX_DL1_VOICE		ABE_MIXER(1)
#define MIX_DL1_CAPTURE		ABE_MIXER(2)
#define MIX_DL1_MEDIA		ABE_MIXER(3)
#define MIX_DL2_TONES		ABE_MIXER(4)
#define MIX_DL2_VOICE		ABE_MIXER(5)
#define MIX_DL2_CAPTURE		ABE_MIXER(6)
#define MIX_DL2_MEDIA		ABE_MIXER(7)
#define MIX_AUDUL_TONES		ABE_MIXER(8)
#define MIX_AUDUL_MEDIA		ABE_MIXER(9)
#define MIX_AUDUL_CAPTURE		ABE_MIXER(10)
#define MIX_VXREC_TONES		ABE_MIXER(11)
#define MIX_VXREC_VOICE_PLAYBACK		ABE_MIXER(12)
#define MIX_VXREC_VOICE_CAPTURE		ABE_MIXER(13)
#define MIX_VXREC_MEDIA		ABE_MIXER(14)
#define MIX_SDT_CAPTURE		ABE_MIXER(15)
#define MIX_SDT_PLAYBACK		ABE_MIXER(16)
#define MIX_SWITCH_PDM_DL		ABE_MIXER(17)
#define MIX_SWITCH_BT_VX_DL		ABE_MIXER(18)
#define MIX_SWITCH_MM_EXT_DL		ABE_MIXER(19)

#define ABE_NUM_MIXERS		(MIX_SWITCH_MM_EXT_DL + 1)

#define ABE_MUX(x)		(x + ABE_NUM_MIXERS)

#define MUX_MM_UL10		ABE_MUX(0)
#define MUX_MM_UL11		ABE_MUX(1)
#define MUX_MM_UL12		ABE_MUX(2)
#define MUX_MM_UL13		ABE_MUX(3)
#define MUX_MM_UL14		ABE_MUX(4)
#define MUX_MM_UL15		ABE_MUX(5)
#define MUX_MM_UL16		ABE_MUX(6)
#define MUX_MM_UL17		ABE_MUX(7)
#define MUX_MM_UL20		ABE_MUX(8)
#define MUX_MM_UL21		ABE_MUX(9)
#define MUX_VX_UL0		ABE_MUX(10)
#define MUX_VX_UL1		ABE_MUX(11)

#define ABE_NUM_MUXES		(MUX_VX_UL1 - MUX_MM_UL10)

#define ABE_WIDGET(x)		(x + ABE_NUM_MIXERS + ABE_NUM_MUXES)

/* ABE AIF Frontend Widgets */
#define W_AIF_TONES_DL		ABE_WIDGET(0)
#define W_AIF_VX_DL		ABE_WIDGET(1)
#define W_AIF_VX_UL		ABE_WIDGET(2)
#define W_AIF_MM_UL1		ABE_WIDGET(3)
#define W_AIF_MM_UL2		ABE_WIDGET(4)
#define W_AIF_MM_DL		ABE_WIDGET(5)
#define W_AIF_MM_DL_LP		W_AIF_MM_DL
#define W_AIF_VIB_DL		ABE_WIDGET(6)
#define W_AIF_MODEM_DL		ABE_WIDGET(7)
#define W_AIF_MODEM_UL		ABE_WIDGET(8)

/* ABE AIF Backend Widgets */
#define W_AIF_PDM_UL1		ABE_WIDGET(9)
#define W_AIF_PDM_DL1		ABE_WIDGET(10)
#define W_AIF_PDM_DL2		ABE_WIDGET(11)
#define W_AIF_PDM_VIB		ABE_WIDGET(12)
#define W_AIF_BT_VX_UL		ABE_WIDGET(13)
#define W_AIF_BT_VX_DL		ABE_WIDGET(14)
#define W_AIF_MM_EXT_UL	ABE_WIDGET(15)
#define W_AIF_MM_EXT_DL	ABE_WIDGET(16)
#define W_AIF_DMIC0		ABE_WIDGET(17)
#define W_AIF_DMIC1		ABE_WIDGET(18)
#define W_AIF_DMIC2		ABE_WIDGET(19)

/* ABE ROUTE_UL MUX Widgets */
#define W_MUX_UL00		ABE_WIDGET(20)
#define W_MUX_UL01		ABE_WIDGET(21)
#define W_MUX_UL02		ABE_WIDGET(22)
#define W_MUX_UL03		ABE_WIDGET(23)
#define W_MUX_UL04		ABE_WIDGET(24)
#define W_MUX_UL05		ABE_WIDGET(25)
#define W_MUX_UL06		ABE_WIDGET(26)
#define W_MUX_UL07		ABE_WIDGET(27)
#define W_MUX_UL10		ABE_WIDGET(28)
#define W_MUX_UL11		ABE_WIDGET(29)
#define W_MUX_VX00		ABE_WIDGET(30)
#define W_MUX_VX01		ABE_WIDGET(31)

/* ABE Volume and Mixer Widgets */
#define W_MIXER_DL1		ABE_WIDGET(32)
#define W_MIXER_DL2		ABE_WIDGET(33)
#define W_VOLUME_DL1		ABE_WIDGET(34)
#define W_MIXER_AUDIO_UL	ABE_WIDGET(35)
#define W_MIXER_VX_REC		ABE_WIDGET(36)
#define W_MIXER_SDT		ABE_WIDGET(37)
#define W_VSWITCH_DL1_PDM	ABE_WIDGET(38)
#define W_VSWITCH_DL1_BT_VX	ABE_WIDGET(39)
#define W_VSWITCH_DL1_MM_EXT	ABE_WIDGET(40)

#define ABE_NUM_WIDGETS		(W_VSWITCH_DL1_MM_EXT - W_AIF_TONES_DL)
#define ABE_WIDGET_LAST		W_VSWITCH_DL1_MM_EXT

#define ABE_NUM_DAPM_REG		\
	(ABE_NUM_MIXERS + ABE_NUM_MUXES + ABE_NUM_WIDGETS)

#define ABE_VIRTUAL_SWITCH	0
#define ABE_ROUTES_UL		14

// TODO: OPP bitmask - Use HAL version after update
#define ABE_OPP_25		0
#define ABE_OPP_50		1
#define ABE_OPP_100		2

/* TODO: size in bytes of debug options */
#define ABE_DBG_FLAG1_SIZE	0
#define ABE_DBG_FLAG2_SIZE	0
#define ABE_DBG_FLAG3_SIZE	0

/* TODO: Pong start offset of DMEM */
/* Ping pong buffer DMEM offset */
#define ABE_DMEM_BASE_OFFSET_PING_PONG	0x4000

/* Gain value conversion */
#define ABE_MAX_GAIN		12000
#define ABE_GAIN_SCALE		100
#define abe_gain_to_val(gain)	((val + ABE_MAX_GAIN) / ABE_GAIN_SCALE)
#define abe_val_to_gain(val) (-ABE_MAX_GAIN + (val * ABE_GAIN_SCALE))

/* Firmware coefficients and equalizers */
#define ABE_MAX_FW_SIZE		(1024 * 128)
#define ABE_MAX_COEFF_SIZE	(1024 * 4)
#define ABE_COEFF_NAME_SIZE	20
#define ABE_COEFF_TEXT_SIZE	20
#define ABE_COEFF_NUM_TEXTS	10
#define ABE_MAX_EQU		10
#define ABE_MAX_PROFILES	30

void abe_dsp_shutdown(void);
void abe_dsp_pm_get(void);
void abe_dsp_pm_put(void);

#endif	/* End of __OMAP_ABE_DSP_H__ */
