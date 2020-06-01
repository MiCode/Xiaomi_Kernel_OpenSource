/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
