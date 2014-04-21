/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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

#define ANY_ID (~0)
#define NO_VER (~0)

/**
 * struct adreno_gpudev - GPU device definition
 * @gpurev: Unique GPU revision identifier
 * @core: Match for the core version of the GPU
 * @major: Match for the major version of the GPU
 * @minor: Match for the minor version of the GPU
 * @patchid: Match for the patch revision of the GPU
 * @features: Common adreno features supported by this core
 * @pm4fw: Filename for th PM4 firmware
 * @pfpfw: Filename for the PFP firmware
 * @gpudev: Pointer to the GPU family specific functions for this core
 * @gmem_size: Amount of binning memory (GMEM/OCMEM) to reserve for the core
 * @sync_lock_pm4_ver: For IOMMUv0 cores the version of PM4 microcode that
 * supports the sync lock mechanism
 * @sync_lock_pfp_ver: For IOMMUv0 cores the version of PFP microcode that
 * supports the sync lock mechanism
 * @pm4_jt_idx: Index of the jump table in the PM4 microcode
 * @pm4_jt_addr: Address offset to load the jump table for the PM4 microcode
 * @pfp_jt_idx: Index of the jump table in the PFP microcode
 * @pfp_jt_addr: Address offset to load the jump table for the PFP microcode
 * @pm4_bstrp_size: Size of the bootstrap loader for PM4 microcode
 * @pfp_bstrp_size: Size of the bootstrap loader for PFP microcde
 * @pfp_bstrp_ver: Version of the PFP microcode that supports bootstraping
 */
static const struct adreno_gpulist {
	enum adreno_gpurev gpurev;
	unsigned int core, major, minor, patchid;
	unsigned long features;
	const char *pm4fw;
	const char *pfpfw;
	struct adreno_gpudev *gpudev;
	unsigned int gmem_size;
	unsigned int sync_lock_pm4_ver;
	unsigned int sync_lock_pfp_ver;
	unsigned int pm4_jt_idx;
	unsigned int pm4_jt_addr;
	unsigned int pfp_jt_idx;
	unsigned int pfp_jt_addr;
	unsigned int pm4_bstrp_size;
	unsigned int pfp_bstrp_size;
	unsigned int pfp_bstrp_ver;
} adreno_gpulist[] = {
	{
		.gpurev = ADRENO_REV_A305,
		.core = 3,
		.major = 0,
		.minor = 5,
		.patchid = 0,
		.pm4fw = "a300_pm4.fw",
		.pfpfw = "a300_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_256K,
		.sync_lock_pm4_ver = 0x3FF037,
		.sync_lock_pfp_ver = 0x3FF016,
	},
	{
		.gpurev = ADRENO_REV_A320,
		.core = 3,
		.major = 2,
		.minor = ANY_ID,
		.patchid = ANY_ID,
		.pm4fw = "a300_pm4.fw",
		.pfpfw = "a300_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_512K,
		.sync_lock_pm4_ver = 0x3FF037,
		.sync_lock_pfp_ver = 0x3FF016,
	},
	{
		.gpurev = ADRENO_REV_A330,
		.core = 3,
		.major = 3,
		.minor = 0,
		.patchid = ANY_ID,
		.features = ADRENO_USES_OCMEM | IOMMU_FLUSH_TLB_ON_MAP,
		.pm4fw = "a330_pm4.fw",
		.pfpfw = "a330_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_1M,
		.sync_lock_pm4_ver = NO_VER,
		.sync_lock_pfp_ver = NO_VER,
		.pm4_jt_idx = 0x8AD,
		.pm4_jt_addr = 0x2E4,
		.pfp_jt_idx = 0x201,
		.pfp_jt_addr = 0x200,
		.pm4_bstrp_size = 0x6,
		.pfp_bstrp_size = 0x20,
		.pfp_bstrp_ver = 0x330020,
	},
	/* 8226v1 */
	{
		.gpurev = ADRENO_REV_A305B,
		.core = 3,
		.major = 0,
		.minor = 5,
		.patchid = 0x10,
		.pm4fw = "a330_pm4.fw",
		.pfpfw = "a330_pfp.fw",
		.features = ADRENO_USES_OCMEM | IOMMU_FLUSH_TLB_ON_MAP,
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_128K,
		.sync_lock_pm4_ver = NO_VER,
		.sync_lock_pfp_ver = NO_VER,
		.pm4_jt_idx = 0x8AD,
		.pm4_jt_addr = 0x2E4,
		.pfp_jt_idx = 0x201,
		.pfp_jt_addr = 0x200,
	},
	/* 8226v2 */
	{
		.gpurev = ADRENO_REV_A305B,
		.core = 3,
		.major = 0,
		.minor = 5,
		.patchid = 0x12,
		.features = ADRENO_USES_OCMEM  | IOMMU_FLUSH_TLB_ON_MAP,
		.pm4fw = "a330_pm4.fw",
		.pfpfw = "a330_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_128K,
		.sync_lock_pm4_ver = NO_VER,
		.sync_lock_pfp_ver = NO_VER,
		.pm4_jt_idx = 0x8AD,
		.pm4_jt_addr = 0x2E4,
		.pfp_jt_idx = 0x201,
		.pfp_jt_addr = 0x200,
	},
	/*8x10 */
	{
		.gpurev = ADRENO_REV_A305C,
		.core = 3,
		.major = 0,
		.minor = 5,
		.patchid = 0x20,
		.pm4fw = "a300_pm4.fw",
		.pfpfw = "a300_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_128K,
		.sync_lock_pm4_ver = 0x3FF037,
		.sync_lock_pfp_ver = 0x3FF016,
	},
	{
		.gpurev = ADRENO_REV_A306,
		.core = 3,
		.major = 0,
		.minor = 6,
		.patchid = 0x00,
		.pm4fw = "a300_pm4.fw",
		.pfpfw = "a300_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_128K,
		.sync_lock_pm4_ver = NO_VER,
		.sync_lock_pfp_ver = NO_VER,
	},
	{
		.gpurev = ADRENO_REV_A310,
		.core = 3,
		.major = 1,
		.minor = 0,
		.patchid = 0x10,
		.features = ADRENO_USES_OCMEM,
		.pm4fw = "a330_pm4.fw",
		.pfpfw = "a330_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_512K,
		.sync_lock_pm4_ver = NO_VER,
		.sync_lock_pfp_ver = NO_VER,
		.pm4_jt_idx = 0x8AD,
		.pm4_jt_addr = 0x2E4,
		.pfp_jt_idx = 0x201,
		.pfp_jt_addr = 0x200,
	},
	{
		.gpurev = ADRENO_REV_A420,
		.core = 4,
		.major = 2,
		.minor = 0,
		.patchid = ANY_ID,
		.features = ADRENO_USES_OCMEM  | IOMMU_FLUSH_TLB_ON_MAP,
		.pm4fw = "a420_pm4.fw",
		.pfpfw = "a420_pfp.fw",
		.gpudev = &adreno_a4xx_gpudev,
		.gmem_size = (SZ_1M + SZ_512K),
		.sync_lock_pm4_ver = NO_VER,
		.sync_lock_pfp_ver = NO_VER,
	},
};
