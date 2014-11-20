/*
 *  controls_v2.h - Intel MID Platform driver header file
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Ramesh Babu <ramesh.babu.koul@intel.com>
 *  Author: Omair M Abdullah <omair.m.abdullah@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#ifndef __SST_CONTROLS_V2_H__
#define __SST_CONTROLS_V2_H__

/*
 * This section defines the map for the mixer widgets.
 *
 * Each mixer will be represented by single value and that value will have each
 * bit corresponding to one input
 *
 * Each out_id will correspond to one mixer and one path. Each input will be
 * represented by single bit in the register.
 */

/* mixer register ids here */
#define SST_MIX(x)		(x)

#define SST_MIX_MODEM		SST_MIX(0)
#define SST_MIX_BT		SST_MIX(1)
#define SST_MIX_CODEC0		SST_MIX(2)
#define SST_MIX_CODEC1		SST_MIX(3)
#define SST_MIX_LOOP0		SST_MIX(4)
#define SST_MIX_LOOP1		SST_MIX(5)
#define SST_MIX_LOOP2		SST_MIX(6)
#define SST_MIX_PROBE		SST_MIX(7)
#define SST_MIX_HF_SNS		SST_MIX(8)
#define SST_MIX_HF		SST_MIX(9)
#define SST_MIX_SPEECH		SST_MIX(10)
#define SST_MIX_RXSPEECH	SST_MIX(11)
#define SST_MIX_VOIP		SST_MIX(12)
#define SST_MIX_PCM0		SST_MIX(13)
#define SST_MIX_PCM1		SST_MIX(14)
#define SST_MIX_PCM2		SST_MIX(15)
#define SST_MIX_AWARE		SST_MIX(16)
#define SST_MIX_VAD		SST_MIX(17)
#define SST_MIX_FM		SST_MIX(18)

#define SST_MIX_MEDIA0		SST_MIX(19)
#define SST_MIX_MEDIA1		SST_MIX(20)

#define SST_NUM_MIX		(SST_MIX_MEDIA1 + 1)

#define SST_MIX_SWITCH		(SST_NUM_MIX + 1)
#define SST_OUT_SWITCH		(SST_NUM_MIX + 2)
#define SST_IN_SWITCH		(SST_NUM_MIX + 3)
#define SST_MUX_REG		(SST_NUM_MIX + 4)
#define SST_REG_LAST		(SST_MUX_REG)

/* last entry defines array size */
#define SST_NUM_WIDGETS		(SST_REG_LAST + 1)

int sst_mix_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int sst_mix_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
#endif
