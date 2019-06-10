/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2019, The Linux Foundation. All rights reserved.
 */

#define ANY_ID (~0)

#define DEFINE_ADRENO_REV(_rev, _core, _major, _minor, _patchid) \
	.gpurev = _rev, .core = _core, .major = _major, .minor = _minor, \
	.patchid = _patchid

#define DEFINE_DEPRECATED_CORE(_name, _rev, _core, _major, _minor, _patchid) \
static const struct adreno_gpu_core adreno_gpu_core_##_name = { \
	DEFINE_ADRENO_REV(_rev, _core, _major, _minor, _patchid), \
	.features = ADRENO_DEPRECATED, \
}

static const struct adreno_a3xx_core adreno_gpu_core_a306 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A306, 3, 0, 6, 0),
		.features = ADRENO_SOFT_FAULT_DETECT,
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_base = 0,
		.gmem_size = SZ_128K,
		.busy_mask = 0x7ffffffe,
	},
	.pm4fw_name = "a300_pm4.fw",
	.pfpfw_name = "a300_pfp.fw",
};

static const struct adreno_a3xx_core adreno_gpu_core_a306a = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A306A, 3, 0, 6, 0x20),
		.features = ADRENO_SOFT_FAULT_DETECT,
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_base = 0,
		.gmem_size = SZ_128K,
		.busy_mask = 0x7ffffffe,
	},
	.pm4fw_name = "a300_pm4.fw",
	.pfpfw_name = "a300_pfp.fw",
};

static const struct adreno_a3xx_core adreno_gpu_core_a304 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A304, 3, 0, 4, 0),
		.features = ADRENO_SOFT_FAULT_DETECT,
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_base = 0,
		.gmem_size = (SZ_64K + SZ_32K),
		.busy_mask = 0x7ffffffe,
	},
	.pm4fw_name = "a300_pm4.fw",
	.pfpfw_name = "a300_pfp.fw",
};

DEFINE_DEPRECATED_CORE(a405, ADRENO_REV_A405, 4, 0, 5, ANY_ID);
DEFINE_DEPRECATED_CORE(a418, ADRENO_REV_A418, 4, 1, 8, ANY_ID);
DEFINE_DEPRECATED_CORE(a420, ADRENO_REV_A420, 4, 2, 0, ANY_ID);
DEFINE_DEPRECATED_CORE(a430, ADRENO_REV_A430, 4, 3, 0, ANY_ID);
DEFINE_DEPRECATED_CORE(a530v1, ADRENO_REV_A530, 5, 3, 0, 0);

static const struct adreno_a5xx_core adreno_gpu_core_a530v2 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A530, 5, 3, 0, 1),
		.features = ADRENO_GPMU | ADRENO_SPTP_PC | ADRENO_LM |
			ADRENO_PREEMPTION | ADRENO_64BIT |
			ADRENO_CONTENT_PROTECTION,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_1M,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.gpmu_tsens = 0x00060007,
	.max_power = 5448,
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
	.gpmufw_name = "a530_gpmu.fw2",
	.regfw_name = "a530v2_seq.fw2",
	.zap_name = "a530_zap",
};

static const struct adreno_a5xx_core adreno_gpu_core_a530v3 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A530, 5, 3, 0, ANY_ID),
		.features = ADRENO_GPMU | ADRENO_SPTP_PC | ADRENO_LM |
			ADRENO_PREEMPTION | ADRENO_64BIT |
			ADRENO_CONTENT_PROTECTION,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_1M,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.gpmu_tsens = 0x00060007,
	.max_power = 5448,
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
	.gpmufw_name = "a530v3_gpmu.fw2",
	.regfw_name = "a530v3_seq.fw2",
	.zap_name = "a530_zap",
};

static const struct adreno_a5xx_core adreno_gpu_core_a505 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A505, 5, 0, 5, ANY_ID),
		.features = ADRENO_PREEMPTION | ADRENO_64BIT,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = (SZ_128K + SZ_8K),
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
};

static const struct adreno_a5xx_core adreno_gpu_core_a506 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A506, 5, 0, 6, ANY_ID),
		.features = ADRENO_PREEMPTION | ADRENO_64BIT |
			ADRENO_CONTENT_PROTECTION | ADRENO_CPZ_RETENTION,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = (SZ_128K + SZ_8K),
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
	.zap_name = "a506_zap",
};

static const struct adreno_a5xx_core adreno_gpu_core_a510 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A510, 5, 1, 0, ANY_ID),
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_256K,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
};

DEFINE_DEPRECATED_CORE(a540v1, ADRENO_REV_A540, 5, 4, 0, 0);

static const struct adreno_a5xx_core adreno_gpu_core_a540v2 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A540, 5, 4, 0, ANY_ID),
		.features = ADRENO_PREEMPTION | ADRENO_64BIT |
			ADRENO_CONTENT_PROTECTION |
			ADRENO_GPMU | ADRENO_SPTP_PC,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_1M,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.gpmu_tsens = 0x000c000d,
	.max_power = 5448,
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
	.gpmufw_name = "a540_gpmu.fw2",
	.zap_name = "a540_zap",
};

static const struct adreno_a5xx_core adreno_gpu_core_a512 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A512, 5, 1, 2, ANY_ID),
		.features = ADRENO_PREEMPTION | ADRENO_64BIT |
			ADRENO_CONTENT_PROTECTION | ADRENO_CPZ_RETENTION,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = (SZ_256K + SZ_16K),
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
	.zap_name = "a512_zap",
};

static const struct adreno_a5xx_core adreno_gpu_core_a508 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A508, 5, 0, 8, ANY_ID),
		.features = ADRENO_PREEMPTION | ADRENO_64BIT |
			ADRENO_CONTENT_PROTECTION | ADRENO_CPZ_RETENTION,
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = (SZ_128K + SZ_8K),
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.pm4fw_name = "a530_pm4.fw",
	.pfpfw_name = "a530_pfp.fw",
	.zap_name = "a508_zap",
};

DEFINE_DEPRECATED_CORE(a630v1, ADRENO_REV_A630, 6, 3, 0, 0);

static const struct adreno_a6xx_core adreno_gpu_core_a630v2 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A630, 6, 3, 0, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_IFPC |
			ADRENO_GPMU | ADRENO_CONTENT_PROTECTION |
			ADRENO_IOCOHERENT | ADRENO_PREEMPTION,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_1M,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x0018000,
	.pdc_address_offset = 0x00030080,
	.gmu_major = 1,
	.gmu_minor = 3,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a630_gmu.bin",
	.zap_name = "a630_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a615 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A615, 6, 1, 5, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_PREEMPTION |
			ADRENO_GPMU | ADRENO_CONTENT_PROTECTION | ADRENO_IFPC |
			ADRENO_IOCOHERENT,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_512K,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x0018000,
	.pdc_address_offset = 0x00030080,
	.gmu_major = 1,
	.gmu_minor = 3,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a630_gmu.bin",
	.zap_name = "a615_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a618 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A618, 6, 1, 8, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_PREEMPTION |
			ADRENO_GPMU | ADRENO_CONTENT_PROTECTION | ADRENO_IFPC |
			ADRENO_IOCOHERENT,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_512K,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x0018000,
	.pdc_address_offset = 0x00030090,
	.gmu_major = 1,
	.gmu_minor = 7,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a630_gmu.bin",
	.zap_name = "a615_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a620 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A618, 6, 2, 0, 0),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_GPMU |
			ADRENO_CONTENT_PROTECTION | ADRENO_IOCOHERENT |
			ADRENO_IFPC,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0,
		.gmem_size = SZ_512K,
		.num_protected_regs = 0x30,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x0010000,
	.pdc_address_offset = 0x000300a0,
	.gmu_major = 2,
	.gmu_minor = 0,
	.sqefw_name = "a650_sqe.fw",
	.gmufw_name = "a650_gmu.bin",
	.zap_name = "a620_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a640 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A640, 6, 4, 0, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_GPMU |
			ADRENO_CONTENT_PROTECTION | ADRENO_IOCOHERENT |
			ADRENO_IFPC | ADRENO_PREEMPTION,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_1M, //Verified 1MB
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x00200000,
	.pdc_address_offset = 0x00030090,
	.gmu_major = 2,
	.gmu_minor = 0,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a640_gmu.bin",
	.zap_name = "a640_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a650 = {
	{
		DEFINE_ADRENO_REV(ADRENO_REV_A650, 6, 5, 0, 0),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_GPMU |
			ADRENO_IOCOHERENT | ADRENO_CONTENT_PROTECTION |
			ADRENO_IFPC,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0,
		.gmem_size = SZ_1M + SZ_128K, /* verified 1152kB */
		.num_protected_regs = 0x30,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x00300000,
	.pdc_address_offset = 0x000300A0,
	.gmu_major = 2,
	.gmu_minor = 0,
	.sqefw_name = "a650_sqe.fw",
	.gmufw_name = "a650_gmu.bin",
	.zap_name = "a650_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a680 = {
	.base = {
		DEFINE_ADRENO_REV(ADRENO_REV_A680, 6, 8, 0, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_GPMU,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_2M,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x00400000,
	.pdc_address_offset = 0x00030090,
	.gmu_major = 2,
	.gmu_minor = 0,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a640_gmu.bin",
	.zap_name = "a640_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a612 = {
	{
		DEFINE_ADRENO_REV(ADRENO_REV_A612, 6, 1, 2, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_CONTENT_PROTECTION |
			ADRENO_IOCOHERENT | ADRENO_PREEMPTION | ADRENO_GPMU |
			ADRENO_IFPC | ADRENO_PERFCTRL_RETAIN,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = (SZ_128K + SZ_4K),
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x00080000,
	.pdc_address_offset = 0x00030080,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a612_rgmu.bin",
	.zap_name = "a612_zap",
};

static const struct adreno_a6xx_core adreno_gpu_core_a616 = {
	{
		DEFINE_ADRENO_REV(ADRENO_REV_A616, 6, 1, 6, ANY_ID),
		.features = ADRENO_64BIT | ADRENO_RPMH | ADRENO_PREEMPTION |
			ADRENO_GPMU | ADRENO_CONTENT_PROTECTION | ADRENO_IFPC |
			ADRENO_IOCOHERENT,
		.gpudev = &adreno_a6xx_gpudev,
		.gmem_base = 0x100000,
		.gmem_size = SZ_512K,
		.num_protected_regs = 0x20,
		.busy_mask = 0xfffffffe,
	},
	.prim_fifo_threshold = 0x0018000,
	.pdc_address_offset = 0x00030080,
	.gmu_major = 1,
	.gmu_minor = 3,
	.sqefw_name = "a630_sqe.fw",
	.gmufw_name = "a630_gmu.bin",
	.zap_name = "a615_zap",
};

static const struct adreno_gpu_core *adreno_gpulist[] = {
	&adreno_gpu_core_a306.base,
	&adreno_gpu_core_a306a.base,
	&adreno_gpu_core_a304.base,
	&adreno_gpu_core_a405,		/* Deprecated */
	&adreno_gpu_core_a418,		/* Deprecated */
	&adreno_gpu_core_a420,		/* Deprecated */
	&adreno_gpu_core_a430,		/* Deprecated */
	&adreno_gpu_core_a530v1,	/* Deprecated */
	&adreno_gpu_core_a530v2.base,
	&adreno_gpu_core_a530v3.base,
	&adreno_gpu_core_a505.base,
	&adreno_gpu_core_a506.base,
	&adreno_gpu_core_a510.base,
	&adreno_gpu_core_a540v1,	/* Deprecated */
	&adreno_gpu_core_a540v2.base,
	&adreno_gpu_core_a512.base,
	&adreno_gpu_core_a508.base,
	&adreno_gpu_core_a630v1,	/* Deprecated */
	&adreno_gpu_core_a630v2.base,
	&adreno_gpu_core_a615.base,
	&adreno_gpu_core_a618.base,
	&adreno_gpu_core_a620.base,
	&adreno_gpu_core_a640.base,
	&adreno_gpu_core_a650.base,
	&adreno_gpu_core_a680.base,
	&adreno_gpu_core_a612.base,
	&adreno_gpu_core_a616.base,
};
