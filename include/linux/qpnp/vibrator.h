/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __QPNP_VIBRATOR_H__
#define __QPNP_VIBRATOR_H__

enum qpnp_vib_en_mode {
	QPNP_VIB_MANUAL,
	QPNP_VIB_DTEST1,
	QPNP_VIB_DTEST2,
	QPNP_VIB_DTEST3,
};

struct qpnp_vib_config {
	u16			drive_mV;
	u8			active_low;
	enum qpnp_vib_en_mode	enable_mode;
};
#if defined(CONFIG_QPNP_VIBRATOR)

int qpnp_vibrator_config(struct qpnp_vib_config *vib_config);
#else

static inline int qpnp_vibrator_config(struct qpnp_vib_config *vib_config)
{
	return -ENODEV;
}
#endif

#endif /* __QPNP_VIBRATOR_H__ */
