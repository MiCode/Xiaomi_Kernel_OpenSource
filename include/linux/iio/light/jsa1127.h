/*
 * A iio driver for the light sensor JSA-1127.
 *
 * IIO Light driver for monitoring ambient light intensity in lux and proximity
 * ir.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define JSA1127_NAME				"jsa1127"
#define JSA1127_SLAVE_ADDRESS			0x39

struct jsa1127_platform_data {
	u32 rint;
	u32 use_internal_integration_timing;
	u32 integration_time;
	u16 tint_coeff;
	u32 noisy;
};
