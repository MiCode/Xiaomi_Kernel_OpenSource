/* include/asm/mach-msm/htc_acoustic_qsd.h
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _ARCH_ARM_MACH_MSM_HTC_ACOUSTIC_QSD_H_
#define _ARCH_ARM_MACH_MSM_HTC_ACOUSTIC_QSD_H_

struct qsd_acoustic_ops {
	void (*enable_mic_bias)(int en);
};

void acoustic_register_ops(struct qsd_acoustic_ops *ops);

int turn_mic_bias_on(int on);
int force_headset_speaker_on(int enable);
int enable_aux_loopback(uint32_t enable);

#endif

