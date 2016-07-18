/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_RPMH_REGULATOR_H
#define __QCOM_RPMH_REGULATOR_H

/* This offset is needed as 0 is considered an invalid voltage. */
#define RPMH_REGULATOR_LEVEL_OFFSET	1

#define RPMH_REGULATOR_LEVEL_MIN	(0 + RPMH_REGULATOR_LEVEL_OFFSET)

#define RPMH_REGULATOR_LEVEL_OFF	(0 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_RETENTION	(16 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_MIN_SVS	(48 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_LOW_SVS	(64 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_SVS	(128 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_SVS_L1	(192 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_NOM	(256 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_NOM_L1	(320 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_TURBO	(384 + RPMH_REGULATOR_LEVEL_OFFSET)

#define RPMH_REGULATOR_LEVEL_MAX	(65535 + RPMH_REGULATOR_LEVEL_OFFSET)

#endif
