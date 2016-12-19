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

/* These levels may be used for RPMH ARC resource regulators */
#define RPMH_REGULATOR_LEVEL_MIN	(0 + RPMH_REGULATOR_LEVEL_OFFSET)

#define RPMH_REGULATOR_LEVEL_OFF	(0 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_RETENTION	(16 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_MIN_SVS	(48 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_LOW_SVS	(64 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_SVS	(128 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_SVS_L1	(192 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_NOM	(256 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_NOM_L1	(320 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_NOM_L2	(336 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_TURBO	(384 + RPMH_REGULATOR_LEVEL_OFFSET)
#define RPMH_REGULATOR_LEVEL_TURBO_L1	(416 + RPMH_REGULATOR_LEVEL_OFFSET)

#define RPMH_REGULATOR_LEVEL_MAX	(65535 + RPMH_REGULATOR_LEVEL_OFFSET)

/*
 * These set constants may be used as the value for qcom,set of an RPMh
 * resource device.
 */
#define RPMH_REGULATOR_SET_ACTIVE	1
#define RPMH_REGULATOR_SET_SLEEP	2
#define RPMH_REGULATOR_SET_ALL		3

/*
 * These mode constants may be used for qcom,supported-modes and qcom,init-mode
 * properties of an RPMh resource.  Modes should be matched to the physical
 * PMIC regulator type (i.e. LDO, SMPS, or BOB).
 */
#define RPMH_REGULATOR_MODE_LDO_LPM	5
#define RPMH_REGULATOR_MODE_LDO_HPM	7

#define RPMH_REGULATOR_MODE_SMPS_PFM	5
#define RPMH_REGULATOR_MODE_SMPS_AUTO	6
#define RPMH_REGULATOR_MODE_SMPS_PWM	7

#define RPMH_REGULATOR_MODE_BOB_PASS	0
#define RPMH_REGULATOR_MODE_BOB_PFM	1
#define RPMH_REGULATOR_MODE_BOB_AUTO	2
#define RPMH_REGULATOR_MODE_BOB_PWM	3

#endif
