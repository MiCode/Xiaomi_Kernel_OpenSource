/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include "hab_qvm.h"

inline void habhyp_notify(void *commdev)
{
	struct qvm_channel *dev = (struct qvm_channel *)commdev;

	if (dev && dev->guest_ctrl)
		dev->guest_ctrl->notify = ~0;
}
