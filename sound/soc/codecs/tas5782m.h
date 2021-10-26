/*
 * tas5782m.h - Driver for the TAS5782M Audio Amplifier
 *
 * Copyright (C) 2017 Texas Instruments Incorporated -  http://www.ti.com
 *
 * Author: Andy Liu <andy-liu@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _SND_SOC_CODEC_TAS5782M_H_
#define _SND_SOC_CODEC_TAS5782M_H_

int tas5782m_speaker_amp_probe(struct i2c_client *i2c,
			       const struct i2c_device_id *id);
int tas5782m_speaker_amp_remove(struct i2c_client *i2c);

#endif /* _SND_SOC_CODEC_TAS5782M_H_ */
