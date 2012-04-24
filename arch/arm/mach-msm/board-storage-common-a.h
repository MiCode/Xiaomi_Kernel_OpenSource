/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _BOARD_STORAGE_A_H
#define _BOARD_STORAGE_A_H

#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>

#define MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(num, _ib) \
static struct msm_bus_vectors sps_to_ddr_perf_vectors_##num[] = { \
	{ \
		.src = MSM_BUS_MASTER_SPS, \
		.dst = MSM_BUS_SLAVE_EBI_CH0, \
		.ib = (_ib), \
		.ab = ((_ib) / 2), \
	} \
}

#define MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(num) \
	{ \
		ARRAY_SIZE(sps_to_ddr_perf_vectors_##num), \
		sps_to_ddr_perf_vectors_##num, \
	}

/* no bandwidth required */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(0, 0);
/*
 * 13 MB/s bandwidth
 * 4-bit MMC_TIMING_LEGACY
 * 4-bit MMC_TIMING_UHS_SDR12
 */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(1, 13 * 1024 * 1024);
/*
 * 26 MB/s bandwidth
 * 8-bit MMC_TIMING_LEGACY
 * 4-bit MMC_TIMING_MMC_HS / MMC_TIMING_SD_HS /
 *	 MMC_TIMING_UHS_SDR25
 */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(2, 26 * 1024 * 1024);
/*
 * 52 MB/s bandwidth
 * 8-bit MMC_TIMING_MMC_HS
 * 4-bit MMC_TIMING_UHS_SDR50 / MMC_TIMING_UHS_DDR50
 */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(3, 52 * 1024 * 1024);
/*
 * 104 MB/s bandwidth
 * 8-bit MMC_TIMING_UHS_DDR50
 * 4-bit MMC_TIMING_UHS_SDR104 / MMC_TIMING_MMC_HS200
 */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(4, 104 * 1024 * 1024);
/*
 * 200 MB/s bandwidth
 * 8-bit MMC_TIMING_MMC_HS200
 */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(5, 200 * 1024 * 1024);
/* max. possible bandwidth */
MSM_BUS_SPS_TO_DDR_VOTE_VECTOR(6, UINT_MAX);

static unsigned int sdcc_bw_vectors[] = {0, (13 * 1024 * 1024),
				(26 * 1024 * 1024), (52 * 1024 * 1024),
				(104 * 1024 * 1024), (200 * 1024 * 1024),
				UINT_MAX};

static struct msm_bus_paths sps_to_ddr_bus_scale_usecases[] = {
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(0),
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(1),
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(2),
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(3),
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(4),
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(5),
	MSM_BUS_SPS_TO_DDR_VOTE_VECTOR_USECASE(6),
};

static struct msm_bus_scale_pdata sps_to_ddr_bus_scale_data = {
	sps_to_ddr_bus_scale_usecases,
	ARRAY_SIZE(sps_to_ddr_bus_scale_usecases),
	.name = "msm_sdcc",
};

static struct msm_mmc_bus_voting_data sps_to_ddr_bus_voting_data = {
	.use_cases = &sps_to_ddr_bus_scale_data,
	.bw_vecs = sdcc_bw_vectors,
	.bw_vecs_size = sizeof(sdcc_bw_vectors),
};

#endif /* _BOARD_STORAGE_A_H */
