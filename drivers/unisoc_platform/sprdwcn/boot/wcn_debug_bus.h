/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __WCN_DEBUG_BUS_H__
#define __WCN_DEBUG_BUS_H__

#include <misc/wcn_integrate_platform.h>
#include "wcn_integrate_dev.h"

#define DEBUGBUS_TO_DDR_BASE	0x87210000
#define DEBUGBUS_TO_DDR_LEN	0x2800

#define DEBUGBUS_REG_BASE	0x7C00A000
#define DEBUGBUS_REG_LEN	0x74

#define READ_AON_DEBUGBUS	0xA
#define READ_AP_DEBUGBUS	0x0
#define READ_APCPU_DEBUGBUS	0x9
#define READ_AUDCP_DEBUGBUS	0x3
#define READ_GPU_DEBUGBUS	0x7
#define READ_AONLP_DEBUGBUS	0x6
#define READ_MM_DEBUGBUS	0x5
#define READ_PUB_DEBUGBUS	0x4
#define READ_PUBCP_DEBUGBUS	0x1
#define READ_WCN_DEBUGBUS	0xb
#define READ_WTLCP_DEBUGBUS	0x2

#define SYSSEL_CFG0_OFFSET	0x18
#define SYSSEL_CFG1_OFFSET	0x1c
#define SYSSEL_CFG2_OFFSET	0x20
#define SYSSEL_CFG3_OFFSET	0x24
#define SYSSEL_CFG4_OFFSET	0x28
#define SYSSEL_CFG5_OFFSET	0x74
#define PAD_DBUS_DATA_OUT_OFFSET	0x50

void wcn_debug_bus_show(struct wcn_device *wcn_dev, char *show);
void debug_bus_show(char *show);

#endif
