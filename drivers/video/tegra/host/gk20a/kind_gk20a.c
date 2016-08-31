/*
 * drivers/video/tegra/host/gk20a/kind_gk20a.c
 *
 * GK20A memory kind management
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <linux/bitops.h>

#include "hw_gmmu_gk20a.h"
#include "kind_gk20a.h"

/* TBD: generate these from kind_macros.h */

/* TBD: not sure on the work creation for gk20a, doubtful */
static inline bool gk20a_kind_work_creation_sked(u8 k)
{
	return false;
}
static inline bool gk20a_kind_work_creation_host(u8 k)
{
	return false;
}

static inline bool gk20a_kind_work_creation(u8 k)
{
	return gk20a_kind_work_creation_sked(k) ||
		gk20a_kind_work_creation_host(k);
}

/* note: taken from the !2cs_compression case */
static inline bool gk20a_kind_supported(u8 k)
{
	return gk20a_kind_work_creation(k) ||
		(k == gmmu_pte_kind_invalid_v()) ||
		(k == gmmu_pte_kind_pitch_v()) ||
		(k >= gmmu_pte_kind_z16_v() &&
		 k <= gmmu_pte_kind_z16_ms8_2c_v()) ||
		(k >= gmmu_pte_kind_z16_2z_v() &&
		 k <= gmmu_pte_kind_z16_ms8_2z_v()) ||
		(k == gmmu_pte_kind_s8z24_v()) ||
		(k >= gmmu_pte_kind_s8z24_2cz_v() &&
		 k <= gmmu_pte_kind_s8z24_ms8_2cz_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_2zv_v()) ||
		(k == gmmu_pte_kind_z24s8_v()) ||
		(k >= gmmu_pte_kind_z24s8_2cz_v() &&
		 k <= gmmu_pte_kind_z24s8_ms8_2cz_v()) ||
		(k == gmmu_pte_kind_zf32_v()) ||
		(k >= gmmu_pte_kind_zf32_2cz_v() &&
		 k <= gmmu_pte_kind_zf32_ms8_2cz_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_v() &&
		 k <= gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_2cszv_v() &&
		 k <= gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_v()) ||
		(k >= gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_2cszv_v() &&
		 k <= gmmu_pte_kind_zf32_x24s8_v()) ||
		(k >= gmmu_pte_kind_zf32_x24s8_2cszv_v() &&
		 k <= gmmu_pte_kind_zf32_x24s8_ms8_2cszv_v()) ||
		(k == gmmu_pte_kind_generic_16bx2_v()) ||
		(k == gmmu_pte_kind_c32_2c_v()) ||
		(k == gmmu_pte_kind_c32_2cra_v()) ||
		(k == gmmu_pte_kind_c32_ms2_2c_v()) ||
		(k == gmmu_pte_kind_c32_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c32_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c32_ms4_2cbr_v()) ||
		(k >= gmmu_pte_kind_c32_ms4_2cra_v() &&
		 k <= gmmu_pte_kind_c64_2c_v()) ||
		(k == gmmu_pte_kind_c64_2cra_v()) ||
		(k == gmmu_pte_kind_c64_ms2_2c_v()) ||
		(k == gmmu_pte_kind_c64_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c64_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c64_ms4_2cbr_v()) ||
		(k >= gmmu_pte_kind_c64_ms4_2cra_v() &&
		 k <= gmmu_pte_kind_c128_ms8_ms16_2cr_v()) ||
		(k == gmmu_pte_kind_pitch_no_swizzle_v());
		}

static inline bool gk20a_kind_z(u8 k)
{
	return (k >= gmmu_pte_kind_z16_v() &&
		k <= gmmu_pte_kind_v8z24_ms8_vc24_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_2cs_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_2cs_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_1cs_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_1cs_v()) ||
		(k >= gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_zf32_x24s8_ms16_1cs_v())
		/* ||
		(k >= gmmu_pte_kind_zv32_x24s8_2cszv_v() &&
		k <= gmmu_pte_kind_xf32_x24s8_ms16_2cs_v())*/;
}

static inline bool gk20a_kind_c(u8 k)
{
	return gk20a_kind_work_creation(k) ||
		(k == gmmu_pte_kind_pitch_v()) ||
		(k == gmmu_pte_kind_generic_16bx2_v()) ||
		(k >= gmmu_pte_kind_c32_2c_v() &&
		 k <= gmmu_pte_kind_c32_ms2_2cbr_v()) ||
		(k == gmmu_pte_kind_c32_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c32_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c64_ms2_2cbr_v()) ||
		(k == gmmu_pte_kind_c64_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c64_ms4_2c_v() &&
		 k <= gmmu_pte_kind_pitch_no_swizzle_v());
}

static inline bool gk20a_kind_compressible(u8 k)
{
	return (k >= gmmu_pte_kind_z16_2c_v() &&
		k <= gmmu_pte_kind_z16_ms16_4cz_v()) ||
		(k >= gmmu_pte_kind_s8z24_1z_v() &&
		 k <= gmmu_pte_kind_s8z24_ms16_4cszv_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_2cs_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_4cszv_v()) ||
		(k >= gmmu_pte_kind_z24s8_1z_v() &&
		 k <= gmmu_pte_kind_z24s8_ms16_4cszv_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_2cs_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_4cszv_v()) ||
		(k >= gmmu_pte_kind_zf32_1z_v() &&
		 k <= gmmu_pte_kind_zf32_ms16_2cz_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1cs_v() &&
		 k <= gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_1cs_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_2cszv_v()) ||
		(k >= gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1cs_v() &&
		 k <= gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_1cs_v()) ||
		(k >= gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1zv_v() &&
		 k <= gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_2cszv_v()) ||
		(k >= gmmu_pte_kind_zf32_x24s8_1cs_v() &&
		 k <= gmmu_pte_kind_zf32_x24s8_ms16_1cs_v()) ||
		(k >= gmmu_pte_kind_zf32_x24s8_2cszv_v() &&
		 k <= gmmu_pte_kind_c32_ms2_2cbr_v()) ||
		(k == gmmu_pte_kind_c32_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c32_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c64_ms2_2cbr_v()) ||
		(k == gmmu_pte_kind_c64_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c64_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c128_ms8_ms16_2cr_v());
}

static inline bool gk20a_kind_zbc(u8 k)
{
	return (k >= gmmu_pte_kind_z16_2c_v() &&
		k <= gmmu_pte_kind_z16_ms16_2c_v()) ||
		(k >= gmmu_pte_kind_z16_4cz_v() &&
		 k <= gmmu_pte_kind_z16_ms16_4cz_v()) ||
		(k >= gmmu_pte_kind_s8z24_2cz_v() &&
		 k <= gmmu_pte_kind_s8z24_ms16_4cszv_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_2cs_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_2cs_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_2czv_v()) ||
		(k >= gmmu_pte_kind_v8z24_ms4_vc12_4cszv_v() &&
		 k <= gmmu_pte_kind_v8z24_ms8_vc24_4cszv_v()) ||
		(k >= gmmu_pte_kind_z24s8_2cs_v() &&
		 k <= gmmu_pte_kind_z24s8_ms16_4cszv_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_2cs_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_2cs_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_2czv_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_2czv_v()) ||
		(k >= gmmu_pte_kind_z24v8_ms4_vc12_4cszv_v() &&
		 k <= gmmu_pte_kind_z24v8_ms8_vc24_4cszv_v()) ||
		(k >= gmmu_pte_kind_zf32_2cs_v() &&
		 k <= gmmu_pte_kind_zf32_ms16_2cz_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1cs_v() &&
		 k <= gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_1cs_v()) ||
		(k >= gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_1czv_v() &&
		 k <= gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_2cszv_v()) ||
		(k >= gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1cs_v() &&
		 k <= gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_1cs_v()) ||
		(k >= gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_1czv_v() &&
		 k <= gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_2cszv_v()) ||
		(k >= gmmu_pte_kind_zf32_x24s8_1cs_v() &&
		 k <= gmmu_pte_kind_zf32_x24s8_ms16_1cs_v()) ||
		(k >= gmmu_pte_kind_zf32_x24s8_2cszv_v() &&
		 k <= gmmu_pte_kind_c32_2cra_v()) ||
		(k >= gmmu_pte_kind_c32_ms2_2c_v() &&
		 k <= gmmu_pte_kind_c32_ms2_2cbr_v())  ||
		(k == gmmu_pte_kind_c32_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c32_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c32_ms4_2cra_v()) ||
		(k >= gmmu_pte_kind_c32_ms8_ms16_2c_v() &&
		 k <= gmmu_pte_kind_c64_2cra_v()) ||
		(k >= gmmu_pte_kind_c64_ms2_2c_v() &&
		 k <= gmmu_pte_kind_c64_ms2_2cbr_v()) ||
		(k == gmmu_pte_kind_c64_ms2_2cra_v()) ||
		(k >= gmmu_pte_kind_c64_ms4_2c_v() &&
		 k <= gmmu_pte_kind_c64_ms4_2cra_v()) ||
		(k >= gmmu_pte_kind_c64_ms8_ms16_2c_v() &&
		 k <= gmmu_pte_kind_c128_ms8_ms16_2cr_v());
}

u8 gk20a_uc_kind_map[256];
void gk20a_init_uncompressed_kind_map(void)
{
	int i;
	for (i = 0; i < 256; i++)
		gk20a_uc_kind_map[i] = gmmu_pte_kind_invalid_v();

	gk20a_uc_kind_map[gmmu_pte_kind_z16_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms2_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms4_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms8_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_2z_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms2_2z_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms4_2z_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z16_ms8_2z_v()] =
		gmmu_pte_kind_z16_v();

	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_ms2_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_ms4_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_ms8_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_ms2_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_ms4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_s8z24_ms8_2cs_v()] =
		gmmu_pte_kind_s8z24_v();

	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc4_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc4_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc4_2zv_v()] =
		gmmu_pte_kind_v8z24_ms4_vc4_v();

	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc8_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc8_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc8_2zv_v()] =
		gmmu_pte_kind_v8z24_ms8_vc8_v();

	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc12_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc12_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc12_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms4_vc12_2zv_v()] =
		gmmu_pte_kind_v8z24_ms4_vc12_v();

	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc24_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc24_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc24_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_v8z24_ms8_vc24_2zv_v()] =
		gmmu_pte_kind_v8z24_ms8_vc24_v();

	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_ms2_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_ms4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_ms8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_ms2_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_ms4_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24s8_ms8_2cz_v()] =
		gmmu_pte_kind_z24s8_v();

	gk20a_uc_kind_map[gmmu_pte_kind_zf32_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_ms2_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_ms4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_ms8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_ms2_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_ms4_2cz_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_ms8_2cz_v()] =
		gmmu_pte_kind_zf32_v();

	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_2cszv_v()] =
		gmmu_pte_kind_x8z24_x16v8s8_ms4_vc12_v();

	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_2cszv_v()] =
		gmmu_pte_kind_x8z24_x16v8s8_ms4_vc4_v();

	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_2cszv_v()] =
		gmmu_pte_kind_x8z24_x16v8s8_ms8_vc8_v();

	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_2cszv_v()] =
		gmmu_pte_kind_x8z24_x16v8s8_ms8_vc24_v();

	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_2cszv_v()] =
		gmmu_pte_kind_zf32_x16v8s8_ms4_vc12_v();

	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_2cszv_v()] =
		gmmu_pte_kind_zf32_x16v8s8_ms4_vc4_v();

	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_2cszv_v()] =
		gmmu_pte_kind_zf32_x16v8s8_ms8_vc8_v();

	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_2cszv_v()] =
		gmmu_pte_kind_zf32_x16v8s8_ms8_vc24_v();

	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_2cszv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_ms2_2cszv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_ms4_2cszv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_ms8_2cszv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_ms2_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_ms4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_zf32_x24s8_ms8_2cs_v()] =
		gmmu_pte_kind_zf32_x24s8_v();

	gk20a_uc_kind_map[gmmu_pte_kind_c32_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_2cba_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_2bra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms2_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms2_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms4_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms4_2cbr_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms4_2cba_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms4_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms4_2bra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms8_ms16_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c32_ms8_ms16_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_2cbr_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_2cba_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_2bra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms2_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms2_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms4_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms4_2cbr_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms4_2cba_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms4_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms4_2bra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms8_ms16_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c64_ms8_ms16_2cra_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_2cr_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_ms2_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_ms2_2cr_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_ms4_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_ms4_2cr_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_ms8_ms16_2c_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_c128_ms8_ms16_2cr_v()] =
		gmmu_pte_kind_generic_16bx2_v();

	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms4_vc4_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms4_vc4_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms4_vc4_2zv_v()] =
		gmmu_pte_kind_z24v8_ms4_vc4_v();

	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms4_vc12_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms4_vc12_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms4_vc12_2zv_v()] =
		gmmu_pte_kind_z24v8_ms4_vc12_v();

	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms8_vc8_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms8_vc8_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms8_vc8_2zv_v()] =
		gmmu_pte_kind_z24v8_ms8_vc8_v();

	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms8_vc24_2cs_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms8_vc24_2czv_v()] =
	gk20a_uc_kind_map[gmmu_pte_kind_z24v8_ms8_vc24_2zv_v()] =
		gmmu_pte_kind_z24v8_ms8_vc24_v();

	gk20a_uc_kind_map[gmmu_pte_kind_x8c24_v()] =
		gmmu_pte_kind_x8c24_v();
}

u16 gk20a_kind_attr[256];
void gk20a_init_kind_attr(void)
{
	u16 k;
	for (k = 0; k < 256; k++) {
		gk20a_kind_attr[k] = 0;
		if (gk20a_kind_supported((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_SUPPORTED;
		if (gk20a_kind_compressible((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_COMPRESSIBLE;
		if (gk20a_kind_z((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_Z;
		if (gk20a_kind_c((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_C;
		if (gk20a_kind_zbc((u8)k))
			gk20a_kind_attr[k] |= GK20A_KIND_ATTR_ZBC;
	}
}
