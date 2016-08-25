/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ADRENO_GPU_H__
#define __ADRENO_GPU_H__

#include <linux/firmware.h>

#include "msm_gpu.h"

#include "adreno_common.xml.h"
#include "adreno_pm4.xml.h"

#define ADRENO_REG_UNUSED	0xFFFFFFFF
#define ADRENO_REG_SKIP	0xFFFFFFFE
#define REG_ADRENO_DEFINE(_offset, _reg) [_offset] = (_reg) + 1

/**
 * adreno_regs: List of registers that are used in across all
 * 3D devices. Each device type has different offset value for the same
 * register, so an array of register offsets are declared for every device
 * and are indexed by the enumeration values defined in this enum
 */
enum adreno_regs {
	REG_ADRENO_CP_DEBUG,
	REG_ADRENO_CP_CNTL,		/* added in a5 */
	REG_ADRENO_CP_ME_RAM_WADDR,
	REG_ADRENO_CP_ME_RAM_DATA,
	REG_ADRENO_CP_PFP_UCODE_DATA,
	REG_ADRENO_CP_PFP_UCODE_ADDR,
	REG_ADRENO_CP_WFI_PEND_CTR,
	REG_ADRENO_CP_RB_BASE,
	REG_ADRENO_CP_RB_BASE_HI,	/* added in a5 */
	REG_ADRENO_CP_RB_RPTR_ADDR,
	REG_ADRENO_CP_RB_RPTR_ADDR_HI,
	REG_ADRENO_CP_RB_RPTR,
	REG_ADRENO_CP_RB_WPTR,
	REG_ADRENO_CP_PROTECT_CTRL,
	REG_ADRENO_CP_ME_CNTL,
	REG_ADRENO_CP_RB_CNTL,
	REG_ADRENO_CP_IB1_BASE,
	REG_ADRENO_CP_IB1_BASE_HI,	/* added in a5 */
	REG_ADRENO_CP_IB1_BUFSZ,
	REG_ADRENO_CP_IB2_BASE,
	REG_ADRENO_CP_IB2_BASE_HI,	/* added in a5 */
	REG_ADRENO_CP_IB2_BUFSZ,
	REG_ADRENO_CP_TIMESTAMP,
	REG_ADRENO_CP_ME_RAM_RADDR,
	REG_ADRENO_CP_ROQ_ADDR,
	REG_ADRENO_CP_ROQ_DATA,
	REG_ADRENO_CP_MERCIU_ADDR,
	REG_ADRENO_CP_MERCIU_DATA,
	REG_ADRENO_CP_MERCIU_DATA2,
	REG_ADRENO_CP_MEQ_ADDR,
	REG_ADRENO_CP_MEQ_DATA,
	REG_ADRENO_CP_HW_FAULT,
	REG_ADRENO_CP_PROTECT_STATUS,

	/* added in a5 */
	REG_ADRENO_CP_PROTECT_REG_0,
	REG_ADRENO_CP_PREEMPT,
	REG_ADRENO_CP_PREEMPT_DEBUG,
	REG_ADRENO_CP_PREEMPT_DISABLE,
	REG_ADRENO_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
	REG_ADRENO_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
	REG_ADRENO_RBBM_STATUS,
	REG_ADRENO_RBBM_STATUS3,	/* added in a5 */
	REG_ADRENO_RBBM_PERFCTR_CTL,
	REG_ADRENO_RBBM_PERFCTR_LOAD_CMD0,
	REG_ADRENO_RBBM_PERFCTR_LOAD_CMD1,
	REG_ADRENO_RBBM_PERFCTR_LOAD_CMD2,
	REG_ADRENO_RBBM_PERFCTR_LOAD_CMD3,	/* added in a5 */
	REG_ADRENO_RBBM_PERFCTR_PWR_1_LO,
	REG_ADRENO_RBBM_INT_0_MASK,
	REG_ADRENO_RBBM_INT_0_STATUS,
	REG_ADRENO_RBBM_AHB_ERROR_STATUS,
	REG_ADRENO_RBBM_PM_OVERRIDE2,
	REG_ADRENO_RBBM_AHB_CMD,
	REG_ADRENO_RBBM_INT_CLEAR_CMD,
	REG_ADRENO_RBBM_SW_RESET_CMD,

	/* added in a5 */
	REG_ADRENO_RBBM_BLOCK_SW_RESET_CMD,
	REG_ADRENO_RBBM_BLOCK_SW_RESET_CMD2,

	REG_ADRENO_RBBM_CLOCK_CTL,
	REG_ADRENO_RBBM_AHB_ME_SPLIT_STATUS,
	REG_ADRENO_RBBM_AHB_PFP_SPLIT_STATUS,
	REG_ADRENO_VPC_DEBUG_RAM_SEL,
	REG_ADRENO_VPC_DEBUG_RAM_READ,
	REG_ADRENO_VSC_SIZE_ADDRESS,
	REG_ADRENO_VFD_CONTROL_0,
	REG_ADRENO_VFD_INDEX_MAX,
	REG_ADRENO_SP_VS_PVT_MEM_ADDR_REG,
	REG_ADRENO_SP_FS_PVT_MEM_ADDR_REG,
	REG_ADRENO_SP_VS_OBJ_START_REG,
	REG_ADRENO_SP_FS_OBJ_START_REG,
	REG_ADRENO_PA_SC_AA_CONFIG,
	REG_ADRENO_SQ_GPR_MANAGEMENT,
	REG_ADRENO_SQ_INST_STORE_MANAGMENT,
	REG_ADRENO_TP0_CHICKEN,
	REG_ADRENO_RBBM_RBBM_CTL,
	REG_ADRENO_UCHE_INVALIDATE0,
	REG_ADRENO_RBBM_PERFCTR_LOAD_VALUE_LO,
	REG_ADRENO_RBBM_PERFCTR_LOAD_VALUE_HI,

	/* added in a5 */
	REG_ADRENO_RBBM_SECVID_TRUST_CONTROL,
	REG_ADRENO_RBBM_SECVID_TRUST_CONFIG,
	REG_ADRENO_RBBM_SECVID_TSB_CONTROL,
	REG_ADRENO_RBBM_SECVID_TSB_TRUSTED_BASE,
	REG_ADRENO_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
	REG_ADRENO_RBBM_SECVID_TSB_TRUSTED_SIZE,
	REG_ADRENO_RBBM_ALWAYSON_COUNTER_LO,
	REG_ADRENO_RBBM_ALWAYSON_COUNTER_HI,
	REG_ADRENO_VBIF_XIN_HALT_CTRL0,
	REG_ADRENO_VBIF_XIN_HALT_CTRL1,
	REG_ADRENO_VBIF_VERSION,

	REG_ADRENO_REGISTER_MAX,
};

enum adreno_version {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A304 = 304,
	ADRENO_REV_A305 = 305,
	ADRENO_REV_A305C = 306,
	ADRENO_REV_A306 = 307,
	ADRENO_REV_A306A = 308,
	ADRENO_REV_A310 = 310,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
	ADRENO_REV_A305B = 335,
	ADRENO_REV_A405 = 405,
	ADRENO_REV_A418 = 418,
	ADRENO_REV_A420 = 420,
	ADRENO_REV_A430 = 430,
	ADRENO_REV_A505 = 505,
	ADRENO_REV_A506 = 506,
	ADRENO_REV_A510 = 510,
	ADRENO_REV_A530 = 530,
	ADRENO_REV_A540 = 540,
};

struct adreno_rev {
	uint8_t  core;
	uint8_t  major;
	uint8_t  minor;
	uint8_t  patchid;
};

#define ADRENO_REV(core, major, minor, patchid) \
	((struct adreno_rev){ core, major, minor, patchid })

struct adreno_gpu_funcs {
	struct msm_gpu_funcs base;
};

struct adreno_info {
	struct adreno_rev rev;
	uint32_t revn;
	const char *name;
	const char *pm4fw, *pfpfw;
	const char *zap_name;
	const char *regfw_name;
	uint32_t gmem;
	struct msm_gpu *(*init)(struct drm_device *dev);
};

const struct adreno_info *adreno_info(struct adreno_rev rev);

struct adreno_rbmemptrs {
	uint32_t rptr;
	uint32_t wptr;
	uint32_t fence;
};

struct adreno_gpu {
	struct msm_gpu base;
	struct adreno_rev rev;
	const struct adreno_info *info;
	uint32_t gmem;  /* actual gmem size */
	uint32_t revn;  /* numeric revision name */
	const struct adreno_gpu_funcs *funcs;

	/* interesting register offsets to dump: */
	const unsigned int *registers;

	/* firmware: */
	const struct firmware *pm4, *pfp;

	size_t pm4_size;
	unsigned int pm4_version;
	struct drm_gem_object *pm4_bo;
	void *pm4_vaddr;

	size_t pfp_size;
	unsigned int pfp_version;
	struct drm_gem_object *pfp_bo;
	void *pfp_vaddr;

	/* ringbuffer rptr/wptr: */
	// TODO should this be in msm_ringbuffer?  I think it would be
	// different for z180..
	struct adreno_rbmemptrs *memptrs;
	struct drm_gem_object *memptrs_bo;
	uint32_t memptrs_iova;

	/*
	 * Register offsets are different between some GPUs.
	 * GPU specific offsets will be exported by GPU specific
	 * code (a3xx_gpu.c) and stored in this common location.
	 */
	const unsigned int *reg_offsets;
};
#define to_adreno_gpu(x) container_of(x, struct adreno_gpu, base)

struct drmgsl_pwrctl;
/* platform config data (ie. from DT, or pdata) */
struct adreno_platform_config {
	struct adreno_rev rev;
	uint32_t fast_rate, slow_rate, bus_freq;
	struct msm_iommu iommu;
#ifdef CONFIG_MSM_BUS_SCALING
	struct msm_bus_scale_pdata *bus_scale_table;
#endif
	struct drmgsl_pwrctl *pwrctl;
	unsigned int speed_bin;

	void __iomem *efuse_base;
	size_t efuse_len;
};

#define ADRENO_UCHE_GMEM_BASE	0x100000

#define ADRENO_IDLE_TIMEOUT msecs_to_jiffies(1000)

#define spin_until(X) ({                                   \
	int __ret = -ETIMEDOUT;                            \
	unsigned long __t = jiffies + ADRENO_IDLE_TIMEOUT; \
	do {                                               \
		if (X) {                                   \
			__ret = 0;                         \
			break;                             \
		}                                          \
	} while (time_before(jiffies, __t));               \
	__ret;                                             \
})


static inline bool adreno_is_a3xx(struct adreno_gpu *gpu)
{
	return (gpu->revn >= 300) && (gpu->revn < 400);
}


static inline int adreno_pre_a5xx(struct adreno_gpu *gpu)
{
	return gpu->revn < 500;
}

static inline int adreno_is_a505_or_a506(struct adreno_gpu *gpu)
{
	return (gpu->revn == 505) || (gpu->revn == 506);
}

static inline int adreno_is_a510(struct adreno_gpu *gpu)
{
	return gpu->revn == 510;
}

static inline int adreno_is_a530(struct adreno_gpu *gpu)
{
	return gpu->revn == 530;
}

static inline int adreno_is_a530v1(struct adreno_gpu *gpu)
{
	return (gpu->revn == 530) && (gpu->rev.patchid == 0);
}

static inline int adreno_is_a530v2(struct adreno_gpu *gpu)
{
	return (gpu->revn == 530) && (gpu->rev.patchid == 1);
}

static inline int adreno_is_a530v3(struct adreno_gpu *gpu)
{
	return (gpu->revn == 530) && (gpu->rev.patchid == 2);
}

static inline int adreno_is_a540(struct adreno_gpu *gpu)
{
	return gpu->revn == 540;
}


static inline int adreno_is_a540v1(struct adreno_gpu *gpu)
{
	return (gpu->revn == 540) || (gpu->rev.patchid == 0);
}

extern bool hang_debug;

int adreno_get_param(struct msm_gpu *gpu, uint32_t param, uint64_t *value);
int adreno_hw_init(struct msm_gpu *gpu);
uint32_t adreno_last_fence(struct msm_gpu *gpu);
void adreno_recover(struct msm_gpu *gpu);
int adreno_submit(struct msm_gpu *gpu, struct msm_gem_submit *submit,
		struct msm_file_private *ctx);
void adreno_flush(struct msm_gpu *gpu);
void adreno_idle(struct msm_gpu *gpu);
#ifdef CONFIG_DEBUG_FS
void adreno_show(struct msm_gpu *gpu, struct seq_file *m);
#endif
void adreno_dump_info(struct msm_gpu *gpu);
void adreno_dump(struct msm_gpu *gpu);
void adreno_wait_ring(struct msm_gpu *gpu, uint32_t ndwords);

int adreno_gpu_init(struct drm_device *drm, struct platform_device *pdev,
		struct adreno_gpu *gpu, const struct adreno_gpu_funcs *funcs);
void adreno_gpu_cleanup(struct adreno_gpu *gpu);

static inline uint calc_odd_parity_bit(uint val)
{
	return (0x9669 >> (0xf & ((val) ^
	((val) >> 4) ^ ((val) >> 8) ^ ((val) >> 12) ^
	((val) >> 16) ^ ((val) >> 20) ^ ((val) >> 24) ^
	((val) >> 28)))) & 1;
}

/* ringbuffer helpers (the parts that are adreno specific) */

static inline void
OUT_PKT0(struct msm_ringbuffer *ring, uint16_t regindx, uint16_t cnt)
{
	adreno_wait_ring(ring->gpu, cnt+1);
	OUT_RING(ring, CP_TYPE0_PKT | ((cnt-1) << 16) | (regindx & 0x7FFF));
}

/* no-op packet: */
static inline void
OUT_PKT2(struct msm_ringbuffer *ring)
{
	adreno_wait_ring(ring->gpu, 1);
	OUT_RING(ring, CP_TYPE2_PKT);
}

static inline uint
cp_pkt3(uint8_t opcode, uint16_t cnt)
{
	return CP_TYPE3_PKT | ((cnt-1) << 16) | ((opcode & 0xFF) << 8);
}

static inline uint
cp_pkt4(uint32_t offset, uint16_t cnt)
{
	return CP_TYPE4_PKT | ((cnt) << 0) |
			(calc_odd_parity_bit(cnt) << 7) |
			(((offset) & 0x3FFFF) << 8) |
			((calc_odd_parity_bit(offset) << 27));
}

static inline uint
cp_pkt7(uint8_t opcode, uint16_t cnt)
{
	return CP_TYPE7_PKT | ((cnt) << 0) |
			(calc_odd_parity_bit(cnt) << 15) |
			(((opcode) & 0x7F) << 16) |
			((calc_odd_parity_bit(opcode) << 23));
}

static inline uint
cp_pkt(struct adreno_gpu *adreno_gpu, uint8_t opcode, uint16_t cnt)
{
	if (adreno_pre_a5xx(adreno_gpu))
		return cp_pkt3(opcode, cnt);
	else
		return cp_pkt7(opcode, cnt);
}

static inline void
OUT_PKT3(struct msm_ringbuffer *ring, uint8_t opcode, uint16_t cnt)
{
	adreno_wait_ring(ring->gpu, cnt+1);
	OUT_RING(ring, cp_pkt3(opcode, cnt));
}

static inline void
OUT_PKT4(struct msm_ringbuffer *ring, uint8_t offset, uint16_t cnt)
{
	adreno_wait_ring(ring->gpu, cnt+1);
	OUT_RING(ring, cp_pkt7(offset, cnt));
}

static inline void
OUT_PKT7(struct msm_ringbuffer *ring, uint opcode, uint cnt)
{
	adreno_wait_ring(ring->gpu, cnt+1);
	OUT_RING(ring, cp_pkt7(opcode, cnt));

}

static inline void
OUT_PKT(struct adreno_gpu *device, struct msm_ringbuffer *ring,
		uint opcode, uint cnt)
{
	OUT_RING(ring, cp_pkt(device, opcode, cnt));
}

/*
 * adreno_checkreg_off() - Checks the validity of a register enum
 * @gpu:		Pointer to struct adreno_gpu
 * @offset_name:	The register enum that is checked
 */
static inline bool adreno_reg_check(struct adreno_gpu *gpu,
		enum adreno_regs offset_name)
{
	if (offset_name > REG_ADRENO_REGISTER_MAX) {
		pr_warn("offset_name:%d\n", offset_name);
		pr_warn("REG_MAX:%d\n", REG_ADRENO_REGISTER_MAX);
		goto error;
	}

	if (!gpu->reg_offsets[offset_name]) {
		pr_warn("the offset:%d is null!\n", offset_name);
		goto error;
	}

	return true;
error:
	BUG();
}

static inline u32 adreno_gpu_off(struct adreno_gpu *gpu,
		enum adreno_regs offset_name)
{
	return gpu->reg_offsets[offset_name];
}

static inline u32 adreno_gpu_read(struct adreno_gpu *gpu,
		enum adreno_regs offset_name)
{
	u32 reg = gpu->reg_offsets[offset_name];
	u32 val = 0;

	if (adreno_reg_check(gpu, offset_name))
		val = gpu_read(&gpu->base, reg - 1);
	return val;
}

static inline void adreno_gpu_write(struct adreno_gpu *gpu,
		enum adreno_regs offset_name, u32 data)
{
	u32 reg = gpu->reg_offsets[offset_name];

	if (adreno_reg_check(gpu, offset_name))
		gpu_write(&gpu->base, reg - 1, data);
}


static inline u64 adreno_gpu_read64(struct adreno_gpu *gpu,
		enum adreno_regs offset_l,
		enum adreno_regs offset_h)
{
	u32 reg;
	u32 vall = 0;
	u32 valh = 0;

	if (adreno_reg_check(gpu, offset_l)) {
		reg = gpu->reg_offsets[offset_l];
		vall = gpu_read(&gpu->base, reg - 1);
	}

	if (adreno_reg_check(gpu, offset_h)) {
		reg = gpu->reg_offsets[offset_h];
		valh = gpu_read(&gpu->base, reg - 1);
	}

	return vall || ((u64)valh << 32);
}

#endif /* __ADRENO_GPU_H__ */
