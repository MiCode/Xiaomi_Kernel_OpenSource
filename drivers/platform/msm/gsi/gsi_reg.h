/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __GSI_REG_H__
#define __GSI_REG_H__

enum gsi_register_ver {
	GSI_REGISTER_VER_1 = 0,
	GSI_REGISTER_VER_2 = 1,
	GSI_REGISTER_MAX,
};

#ifdef GSI_REGISTER_VER_CURRENT
#error GSI_REGISTER_VER_CURRENT already defined
#endif

#ifdef CONFIG_GSI_REGISTER_VERSION_2
#include "gsi_reg_v2.h"
#define GSI_REGISTER_VER_CURRENT GSI_REGISTER_VER_2
#endif

/* The default is V1 */
#ifndef GSI_REGISTER_VER_CURRENT
#include "gsi_reg_v1.h"
#define GSI_REGISTER_VER_CURRENT GSI_REGISTER_VER_1
#endif

#endif /* __GSI_REG_H__ */
