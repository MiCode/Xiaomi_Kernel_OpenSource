/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ADV7520_H_
#define _ADV7520_H_
#define ADV7520_DRV_NAME 		"adv7520"

/* Configure the 20-bit 'N' used with the CTS to
regenerate the audio clock in the receiver
Pixel clock: 74.25 Mhz, Audio sampling: 44.1 Khz -> N
value = 6272 */
#define ADV7520_AUDIO_CTS_20BIT_N   6272

#endif
