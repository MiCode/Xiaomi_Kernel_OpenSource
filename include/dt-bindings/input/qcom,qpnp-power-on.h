/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_INPUT_QCOM_POWER_ON_H
#define _DT_BINDINGS_INPUT_QCOM_POWER_ON_H

/* PMIC PON peripheral logical power on types: */
#define PON_POWER_ON_TYPE_KPDPWR		0
#define PON_POWER_ON_TYPE_RESIN			1
#define PON_POWER_ON_TYPE_CBLPWR		2
#define PON_POWER_ON_TYPE_KPDPWR_RESIN		3

/* PMIC PON peripheral physical power off types: */
#define PON_POWER_OFF_TYPE_WARM_RESET		0x01
#define PON_POWER_OFF_TYPE_SHUTDOWN		0x04
#define PON_POWER_OFF_TYPE_DVDD_SHUTDOWN	0x05
#define PON_POWER_OFF_TYPE_HARD_RESET		0x07
#define PON_POWER_OFF_TYPE_DVDD_HARD_RESET	0x08

#endif
