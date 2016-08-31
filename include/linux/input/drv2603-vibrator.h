/*
 * DRV2603 haptic driver for LRA and ERM vibrator motor
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _INPUT_DRV2603_H
#define _INPUT_DRV2603_H

enum drv2603_mode {
	LRA_MODE,
	ERM_MODE,
};

struct drv2603_platform_data {
	int pwm_id;
	enum drv2603_mode vibrator_mode;
	int gpio;
	int duty_cycle;
};

#endif
