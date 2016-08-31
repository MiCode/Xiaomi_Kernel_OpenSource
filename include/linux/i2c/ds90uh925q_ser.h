/*
 * FPDLink Serializer driver
 *
 * Copyright (C) 2012 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_DS90UH925Q_SER_H
#define __LINUX_DS90UH925Q_SER_H

#include <linux/types.h>

/* The platform data for the FPDLink Serializer driver */
struct ds90uh925q_platform_data {
	bool has_lvds_en_gpio;  /* has GPIO to enable */
	int  lvds_en_gpio; /* GPIO */

	bool is_fpdlinkII;
	bool support_hdcp;
	bool clk_rise_edge;
};

#endif /* __LINUX_DS90UH925Q_SER_H */
