// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/soc/qcom/llcc-qcom.h>

#define ACTIVATE                      BIT(0)
#define DEACTIVATE                    BIT(1)
#define ACT_CLEAR                     BIT(0)
#define ACT_COMPLETE                  BIT(4)
#define ACT_CTRL_OPCODE_ACTIVATE      BIT(0)
#define ACT_CTRL_OPCODE_DEACTIVATE    BIT(1)
#define ACT_CTRL_ACT_TRIG             BIT(0)
#define LLCC_CFG_SCID_EN(n)           BIT(n)
#define ACT_CTRL_OPCODE_SHIFT         0x01
#define ATTR1_PROBE_TARGET_WAYS_SHIFT 0x02
#define ATTR1_FIXED_SIZE_SHIFT        0x03
#define ATTR1_PRIORITY_SHIFT          0x04
#define ATTR1_MAX_CAP_SHIFT           0x10
#define ATTR0_RES_WAYS_MASK           GENMASK(15, 0)
#define ATTR0_BONUS_WAYS_MASK         GENMASK(31, 16)
#define ATTR0_BONUS_WAYS_SHIFT        0x10
#define ATTR2_PROBE_TARGET_WAYS_SHIFT 0x4
#define ATTR2_FIXED_SIZE_SHIFT        0x8
#define ATTR2_PRIORITY_SHIFT          0xc
#define ATTR2_PARENT_SLICE_ID_SHIFT	  0x10
#define ATTR2_IN_A_GROUP_SHIFT		  0x18
#define LLCC_STATUS_READ_DELAY        100

#define CACHE_LINE_SIZE_SHIFT         6

#define LLCC_LB_CNT_MASK              GENMASK(31, 28)
#define LLCC_LB_CNT_SHIFT             28

#define MAX_CAP_TO_BYTES(n)           (n * SZ_1K)
#define LLCC_TRP_ACT_CTRLn(n)         (n * SZ_4K)
#define LLCC_TRP_ACT_CLEARn(n)        (8 + n * SZ_4K)
#define LLCC_TRP_STATUSn(n)           (4 + n * SZ_4K)
#define LLCC_TRP_STAL_ATTR0_CFGn(n)   (0xC + SZ_4K * n)
#define STALING_TRIGGER_MASK          0x1

#define LLCC_TRP_STAL_ATTR1_CFGn(n)   (0x10 + SZ_4K * n)
#define NOTIFCN_BASED_INVDTN_EN_SHIFT 12
#define STALING_ENABLE_MASK           0x1001
#define FRAME_DISTANCE_SHIFT          4
#define STALING_NUM_FRAMES_MASK       GENMASK(2 + FRAME_DISTANCE_SHIFT,\
					FRAME_DISTANCE_SHIFT)

#define LLCC_TRP_ATTR0_CFGn(n)        (0x21000 + SZ_8 * n)
#define LLCC_TRP_ATTR1_CFGn(n)        (0x21004 + SZ_8 * n)
#define LLCC_TRP_ATTR2_CFGn(n)        (0x21100 + SZ_4 * n)

#define LLCC_V6_TRP_ATTR0_CFGn(n)     (cfg->reg_offset[LLCC_TRP_ATTR0_CFG] + SZ_64 * n)
#define LLCC_V6_TRP_ATTR1_CFGn(n)     (cfg->reg_offset[LLCC_TRP_ATTR1_CFG] + SZ_64 * n)
#define LLCC_V6_TRP_ATTR2_CFGn(n)     (cfg->reg_offset[LLCC_TRP_ATTR2_CFG] + SZ_64 * n)
#define LLCC_V6_TRP_ATTR3_CFGn(n)     (cfg->reg_offset[LLCC_TRP_ATTR3_CFG] + SZ_64 * n)

#define LLCC_TRP_SCID_DIS_CAP_ALLOC   0x21f00
#define LLCC_TRP_PCB_ACT              0x21f04
#define LLCC_TRP_ALGO_CFG1	      0x21f0c
#define LLCC_TRP_ALGO_CFG2	      0x21f10
#define LLCC_TRP_ALGO_CFG3	      0x21f14
#define LLCC_TRP_ALGO_CFG4	      0x21f18
#define LLCC_TRP_ALGO_CFG5	      0x21f1c
#define LLCC_TRP_WRSC_EN              0x21f20
#define LLCC_TRP_ALGO_CFG6	      0x21f24
#define LLCC_TRP_ALGO_CFG7	      0x21f28
#define LLCC_TRP_WRSC_CACHEABLE_EN    0x21f2c
#define LLCC_TRP_ALGO_CFG8	      0x21f30

/**
 * llcc_slice_config - Data associated with the llcc slice
 * @usecase_id: Unique id for the client's use case
 * @slice_id: llcc slice id for each client
 * @max_cap: The maximum capacity of the cache slice provided in KB
 * @priority: Priority of the client used to select victim line for replacement
 * @fixed_size: Boolean indicating if the slice has a fixed capacity
 * @bonus_ways: Bonus ways are additional ways to be used for any slice,
 *		if client ends up using more than reserved cache ways. Bonus
 *		ways are allocated only if they are not reserved for some
 *		other client.
 * @res_ways: Reserved ways for the cache slice, the reserved ways cannot
 *		be used by any other client than the one its assigned to.
 * @cache_mode: Each slice operates as a cache, this controls the mode of the
 *             slice: normal or TCM(Tightly Coupled Memory)
 * @probe_target_ways: Determines what ways to probe for access hit. When
 *                    configured to 1 only bonus and reserved ways are probed.
 *                    When configured to 0 all ways in llcc are probed.
 * @dis_cap_alloc: Disable capacity based allocation for a client
 * @retain_on_pc: If this bit is set and client has maintained active vote
 *               then the ways assigned to this client are not flushed on power
 *               collapse.
 * @activate_on_init: Activate the slice immediately after it is programmed
 * @write_scid_en: Enables write cache support for a given scid.
 * @write_scid_cacheable_en: Enables write cache cacheable support for a
 *                          given scid.(Not supported on V2 or older hardware)
 * @stale_en: Enable global staling for the Clients.
 * @stale_cap_en: Enable global staling on over capacity for the Clients
 * @mru_uncap_en: Enable roll over on reserved ways if the current SCID is under capacity.
 * @mru_rollover: Roll over on reserved ways for the client.
 * @alloc_oneway_en: Always allocate one way on over capacity even if there
 *			is no same scid lines for replacement.
 * @ovcap_en: Once current scid is over capacity, allocate other over capacity scid.
 * @ovcap_prio: Once current scid is over capacity, allocate other lower priority
 *			over capacity scid. This setting is ignored if ovcap_en is not set.
 * @vict_prio: When current SCID is under capacity, allocate over other lower than
 *		VICTIM_PL_THRESHOLD priority SCID.
 * @in_a_group: Enable SCID grouping for a given client.
 * @parent_slice_id: Parent SCID for a given client if SCID grouping enabled.
 */
struct llcc_slice_config {
	u32 usecase_id;
	u32 slice_id;
	u32 max_cap;
	u32 priority;
	bool fixed_size;
	u32 bonus_ways;
	u32 res_ways;
	u32 cache_mode;
	u32 probe_target_ways;
	bool dis_cap_alloc;
	bool retain_on_pc;
	bool activate_on_init;
	bool write_scid_en;
	bool write_scid_cacheable_en;
	bool stale_en;
	bool stale_cap_en;
	bool mru_uncap_en;
	bool mru_rollover;
	bool alloc_oneway_en;
	bool ovcap_en;
	bool ovcap_prio;
	bool vict_prio;
	bool in_a_group;
	u32 parent_slice_id;
};

struct qcom_llcc_config {
	const struct llcc_slice_config *sct_data;
	const u32 *reg_offset;
	const struct llcc_edac_reg_offset *edac_reg_offset;
	int size;
	bool need_llcc_cfg;
	bool no_edac;
};

enum llcc_reg_offset {
	LLCC_COMMON_HW_INFO,
	LLCC_COMMON_STATUS0,
	LLCC_TRP_ATTR0_CFG,
	LLCC_TRP_ATTR1_CFG,
	LLCC_TRP_ATTR2_CFG,
	LLCC_TRP_ATTR3_CFG,
	LLCC_TRP_SID_DIS_CAP_ALLOC,
	LLCC_TRP_ALGO_STALE_EN,
	LLCC_TRP_ALGO_STALE_CAP_EN,
	LLCC_TRP_ALGO_MRU0,
	LLCC_TRP_ALGO_MRU1,
	LLCC_TRP_ALGO_ALLOC0,
	LLCC_TRP_ALGO_ALLOC1,
	LLCC_TRP_ALGO_ALLOC2,
	LLCC_TRP_ALGO_ALLOC3,
	LLCC_TRP_WRS_EN,
	LLCC_TRP_WRS_CACHEABLE_EN,
};

static const struct llcc_slice_config sc7180_data[] =  {
	{ LLCC_CPUSS,    1,  256, 1, 0, 0xf, 0x0, 0, 0, 0, 1, 1 },
	{ LLCC_MDM,      8,  128, 1, 0, 0xf, 0x0, 0, 0, 0, 1, 0 },
	{ LLCC_GPUHTW,   11, 128, 1, 0, 0xf, 0x0, 0, 0, 0, 1, 0 },
	{ LLCC_GPU,      12, 128, 1, 0, 0xf, 0x0, 0, 0, 0, 1, 0 },
};

static const struct llcc_slice_config sc7280_data[] =  {
	{ LLCC_CPUSS,    1,  768, 1, 0, 0x3f, 0x0, 0, 0, 0, 1, 1, 0},
	{ LLCC_MDMHPGRW, 7,  512, 2, 1, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
	{ LLCC_CMPT,     10, 768, 1, 1, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
	{ LLCC_GPUHTW,   11, 256, 1, 1, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
	{ LLCC_GPU,      12, 512, 1, 0, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
	{ LLCC_MMUHWT,   13, 256, 1, 1, 0x3f, 0x0, 0, 0, 0, 0, 1, 0},
	{ LLCC_MDMPNG,   21, 768, 0, 1, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
	{ LLCC_WLHW,     24, 256, 1, 1, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
	{ LLCC_MODPE,    29, 64,  1, 1, 0x3f, 0x0, 0, 0, 0, 1, 0, 0},
};

static const struct llcc_slice_config sc8180x_data[] = {
	{ LLCC_CPUSS,    1, 6144,  1, 1, 0xfff, 0x0,   0, 0, 0, 1, 1 },
	{ LLCC_VIDSC0,   2, 512,   2, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_VIDSC1,   3, 512,   2, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_AUDIO,    6, 1024,  1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_MDMHPGRW, 7, 3072,  1, 1, 0x3ff, 0xc00, 0, 0, 0, 1, 0 },
	{ LLCC_MDM,      8, 3072,  1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_MODHW,    9, 1024,  1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_CMPT,     10, 6144, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_GPUHTW,   11, 1024, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_GPU,      12, 5120, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_MMUHWT,   13, 1024, 1, 1, 0xfff, 0x0,   0, 0, 0, 0, 1 },
	{ LLCC_CMPTDMA,  15, 6144, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_DISP,     16, 6144, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_VIDFW,    17, 1024, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_MDMHPFX,  20, 1024, 2, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_MDMPNG,   21, 1024, 0, 1, 0xc,   0x0,   0, 0, 0, 1, 0 },
	{ LLCC_AUDHW,    22, 1024, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_NPU,      23, 6144, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_WLHW,     24, 6144, 1, 1, 0xfff, 0x0,   0, 0, 0, 1, 0 },
	{ LLCC_MODPE,    29, 512,  1, 1, 0xc,   0x0,   0, 0, 0, 1, 0 },
	{ LLCC_APTCM,    30, 512,  3, 1, 0x0,   0x1,   1, 0, 0, 1, 0 },
	{ LLCC_WRCACHE,  31, 128,  1, 1, 0xfff, 0x0,   0, 0, 0, 0, 0 },
};

static const struct llcc_slice_config sc8280xp_data[] = {
	{ LLCC_CPUSS,    1,  6144, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 1, 0 },
	{ LLCC_VIDSC0,   2,  512,  3, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_AUDIO,    6,  1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 0, 0 },
	{ LLCC_CMPT,     10, 6144, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 0, 0 },
	{ LLCC_GPUHTW,   11, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_GPU,      12, 4096, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 1 },
	{ LLCC_MMUHWT,   13, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_DISP,     16, 6144, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_AUDHW,    22, 2048, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_DRE,      26, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_CVP,      28, 512,  3, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_APTCM,    30, 1024, 3, 1, 0x0,   0x1, 1, 0, 0, 1, 0, 0 },
	{ LLCC_WRCACHE,  31, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_CVPFW,    17, 512,  1, 0, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_CPUSS1,   3, 2048, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_CPUHWT,   5, 512,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
};

static const struct llcc_slice_config sdm845_data[] =  {
	{ LLCC_CPUSS,    1,  2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 1 },
	{ LLCC_VIDSC0,   2,  512,  2, 1, 0x0,   0x0f0, 0, 0, 1, 1, 0 },
	{ LLCC_VIDSC1,   3,  512,  2, 1, 0x0,   0x0f0, 0, 0, 1, 1, 0 },
	{ LLCC_ROTATOR,  4,  563,  2, 1, 0x0,   0x00e, 2, 0, 1, 1, 0 },
	{ LLCC_VOICE,    5,  2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_AUDIO,    6,  2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_MDMHPGRW, 7,  1024, 2, 0, 0xfc,  0xf00, 0, 0, 1, 1, 0 },
	{ LLCC_MDM,      8,  2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_CMPT,     10, 2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_GPUHTW,   11, 512,  1, 1, 0xc,   0x0,   0, 0, 1, 1, 0 },
	{ LLCC_GPU,      12, 2304, 1, 0, 0xff0, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_MMUHWT,   13, 256,  2, 0, 0x0,   0x1,   0, 0, 1, 0, 1 },
	{ LLCC_CMPTDMA,  15, 2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_DISP,     16, 2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_VIDFW,    17, 2816, 1, 0, 0xffc, 0x2,   0, 0, 1, 1, 0 },
	{ LLCC_MDMHPFX,  20, 1024, 2, 1, 0x0,   0xf00, 0, 0, 1, 1, 0 },
	{ LLCC_MDMPNG,   21, 1024, 0, 1, 0x1e,  0x0,   0, 0, 1, 1, 0 },
	{ LLCC_AUDHW,    22, 1024, 1, 1, 0xffc, 0x2,   0, 0, 1, 1, 0 },
};

static const struct llcc_slice_config sm6350_data[] =  {
	{ LLCC_CPUSS,    1,  768, 1, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 1 },
	{ LLCC_MDM,      8,  512, 2, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_GPUHTW,   11, 256, 1, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_GPU,      12, 512, 1, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_MDMPNG,   21, 768, 0, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_NPU,      23, 768, 1, 0, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_MODPE,    29,  64, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 0 },
};

static const struct llcc_slice_config sm7150_data[] =  {
	{ LLCC_CPUSS,    1,  512, 1, 0, 0xF, 0x0, 0, 0, 0, 1, 1 },
	{ LLCC_MDM,      8,  128, 2, 0, 0xF, 0x0, 0, 0, 0, 1, 0 },
	{ LLCC_GPUHTW,   11, 256, 1, 1, 0xF, 0x0, 0, 0, 0, 1, 0 },
	{ LLCC_GPU,      12, 256, 1, 1, 0xF, 0x0, 0, 0, 0, 1, 0 },
	{ LLCC_NPU,      23, 512, 1, 0, 0xF, 0x0, 0, 0, 0, 1, 0 },
};

static const struct llcc_slice_config sm8150_data[] =  {
	{  LLCC_CPUSS,    1, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 1 },
	{  LLCC_VIDSC0,   2, 512,  2, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_VIDSC1,   3, 512,  2, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_AUDIO,    6, 1024, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_MDMHPGRW, 7, 3072, 1, 0, 0xFF,  0xF00, 0, 0, 0, 1, 0 },
	{  LLCC_MDM,      8, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_MODHW,    9, 1024, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_CMPT,    10, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_GPUHTW , 11, 512,  1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_GPU,     12, 2560, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_MMUHWT,  13, 1024, 1, 1, 0xFFF, 0x0,   0, 0, 0, 0, 1 },
	{  LLCC_CMPTDMA, 15, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_DISP,    16, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_MDMHPFX, 20, 1024, 2, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_MDMHPFX, 21, 1024, 0, 1, 0xF,   0x0,   0, 0, 0, 1, 0 },
	{  LLCC_AUDHW,   22, 1024, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_NPU,     23, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_WLHW,    24, 3072, 1, 1, 0xFFF, 0x0,   0, 0, 0, 1, 0 },
	{  LLCC_MODPE,   29, 256,  1, 1, 0xF,   0x0,   0, 0, 0, 1, 0 },
	{  LLCC_APTCM,   30, 256,  3, 1, 0x0,   0x1,   1, 0, 0, 1, 0 },
	{  LLCC_WRCACHE, 31, 128,  1, 1, 0xFFF, 0x0,   0, 0, 0, 0, 0 },
};

static const struct llcc_slice_config sm8250_data[] =  {
	{ LLCC_CPUSS,    1, 3072, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 1, 0 },
	{ LLCC_VIDSC0,   2, 512,  3, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_AUDIO,    6, 1024, 1, 0, 0xfff, 0x0, 0, 0, 0, 0, 0, 0 },
	{ LLCC_CMPT,    10, 1024, 1, 0, 0xfff, 0x0, 0, 0, 0, 0, 0, 0 },
	{ LLCC_GPUHTW,  11, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_GPU,     12, 1024, 1, 0, 0xfff, 0x0, 0, 0, 0, 1, 0, 1 },
	{ LLCC_MMUHWT,  13, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_CMPTDMA, 15, 1024, 1, 0, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_DISP,    16, 3072, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_VIDFW,   17, 512,  1, 0, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_AUDHW,   22, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_NPU,     23, 3072, 1, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_WLHW,    24, 1024, 1, 0, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_CVP,     28, 256,  3, 1, 0xfff, 0x0, 0, 0, 0, 1, 0, 0 },
	{ LLCC_APTCM,   30, 128,  3, 0, 0x0,   0x3, 1, 0, 0, 1, 0, 0 },
	{ LLCC_WRCACHE, 31, 256,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
};

static const struct llcc_slice_config sm8350_data[] =  {
	{ LLCC_CPUSS,    1, 3072,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 1 },
	{ LLCC_VIDSC0,   2, 512,   3, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_AUDIO,    6, 1024,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 0, 0 },
	{ LLCC_MDMHPGRW, 7, 1024,  3, 0, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_MODHW,    9, 1024,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_CMPT,     10, 3072, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_GPUHTW,   11, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_GPU,      12, 1024, 1, 0, 0xfff, 0x0, 0, 0, 0, 1, 1, 0 },
	{ LLCC_MMUHWT,   13, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 0, 1 },
	{ LLCC_DISP,     16, 3072, 2, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_MDMPNG,   21, 1024, 0, 1, 0xf,   0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_AUDHW,    22, 1024, 1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_CVP,      28, 512,  3, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_MODPE,    29, 256,  1, 1, 0xf,   0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_APTCM,    30, 1024, 3, 1, 0x0,   0x1, 1, 0, 0, 0, 1, 0 },
	{ LLCC_WRCACHE,  31, 512,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 0, 1 },
	{ LLCC_CVPFW,    17, 512,  1, 0, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_CPUSS1,   3, 1024,  1, 1, 0xfff, 0x0, 0, 0, 0, 0, 1, 0 },
	{ LLCC_CPUHWT,   5, 512,   1, 1, 0xfff, 0x0, 0, 0, 0, 0, 0, 1 },
};

static const struct llcc_slice_config sm8450_data[] =  {
	{LLCC_CPUSS,     1, 3072, 1, 0, 0xFFFF, 0x0,   0, 0, 0, 1, 1, 0, 0 },
	{LLCC_VIDSC0,    2,  512, 3, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_AUDIO,     6, 1024, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0 },
	{LLCC_MDMHPGRW,  7, 1024, 3, 0, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_MODHW,     9, 1024, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_CMPT,     10, 4096, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_GPUHTW,   11,  512, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_GPU,      12, 2048, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 1, 0 },
	{LLCC_MMUHWT,   13,  768, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 0, 1, 0, 0 },
	{LLCC_DISP,     16, 4096, 2, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_MDMPNG,   21, 1024, 1, 1, 0xF000, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_AUDHW,    22, 1024, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0 },
	{LLCC_CVP,      28,  256, 3, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_MODPE,    29,   64, 1, 1, 0xF000, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_APTCM,    30, 1024, 3, 1, 0x0,    0xF0,  1, 0, 0, 1, 0, 0, 0 },
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 0, 1, 0, 0 },
	{LLCC_CVPFW,    17,  512, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_CPUSS1,    3, 1024, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_CAMEXP0,   4,  256, 3, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_CPUMTE,   23,  256, 1, 1, 0x0FFF, 0x0,   0, 0, 0, 0, 1, 0, 0 },
	{LLCC_CPUHWT,    5,  512, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 1, 0, 0 },
	{LLCC_CAMEXP1,  27,  256, 3, 1, 0xFFFF, 0x0,   0, 0, 0, 1, 0, 0, 0 },
	{LLCC_AENPU,     8, 2048, 1, 1, 0xFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0 },
};

static const struct llcc_slice_config sm8550_data[] =  {
	{LLCC_CPUSS,     1, 5120, 1, 0, 0xFFFFFF, 0x0,   0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_VIDSC0,    2,  512, 4, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_AUDIO,     6, 1024, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_MDMHPGRW, 25, 1024, 4, 0, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_MODHW,    26, 1024, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CMPT,     10, 4096, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_GPUHTW,   11,  512, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_GPU,       9, 3096, 1, 0, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_MMUHWT,   18,  768, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_DISP,     16, 6144, 1, 1, 0xFFFFFF, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_MDMPNG,   27, 1024, 0, 1, 0xF00000, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_AUDHW,    22, 1024, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CVP,       8,  256, 4, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_MODPE,    29,   64, 1, 1, 0xF00000, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, },
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CAMEXP0,   4,  256, 4, 1,      0xF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CPUHWT,    5,  512, 1, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CAMEXP1,   7, 3200, 3, 1, 0xFFFFF0, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CMPTHCP,  17,  256, 4, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_LCPDARE,  30,  128, 4, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, },
	{LLCC_AENPU,     3, 3072, 1, 1, 0xFE01FF, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_ISLAND1,  12, 1792, 7, 1,   0xFE00, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_ISLAND4,  15,  256, 7, 1,  0x10000, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CAMEXP2,  19, 3200, 3, 1, 0xFFFFF0, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CAMEXP3,  20, 3200, 2, 1, 0xFFFFF0, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_CAMEXP4,  21, 3200, 2, 1, 0xFFFFF0, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_DISP_WB,  23, 1024, 4, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_DISP_1,   24, 6144, 1, 1, 0xFFFFFF, 0x0,   2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{LLCC_VIDVSP,   28,  256, 4, 1, 0xFFFFFF, 0x0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

static const struct llcc_slice_config pineapple_data[] = {
	{LLCC_CPUSS,     1, 5120, 1, 0, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDSC0,    2,  512, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AUDIO,     6,  512, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPGRW, 25, 1024, 3, 0, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MODHW,    26, 1024, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPT,     10, 4096, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPUHTW,   11,  512, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPU,       9, 3096, 1, 0, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MMUHWT,   18,  768, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_DISP,     16, 6144, 1, 1, 0xFFFFFF, 0x0, 2, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPFX,  24, 1024, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMPNG,   27,  256, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AUDHW,    22, 1024, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVP,       8,  256, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MODPE,    29,  128, 1, 1, 0xF00000, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMEXP0,   4,  256, 3, 1,      0xF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMEXP1,   7, 3200, 3, 1, 0xFFFFF0, 0x0, 2, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPTHCP,  17,  256, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_LCPDARE,  30,  128, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
	{LLCC_AENPU,     3, 3072, 1, 1, 0xFFFFFF, 0x0, 2, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_ISLAND1,  12, 5888, 7, 1,      0x0, 0x7FFFFF, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_DISP_WB,  23, 1024, 1, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDVSP,   28,  256, 3, 1, 0xFFFFFF, 0x0, 0, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const struct llcc_slice_config sun_data[] = {
	{LLCC_CPUSS,     1, 5120, 1, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 1,
						1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPFX,  24, 1024, 5, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDSC0,    2,  512, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AUDIO,    35,  512, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPGRW, 25, 1024, 5, 0, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MODHW,    26, 1024, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPT,     34, 4096, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPUHTW,   11,  512, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPU,       9, 5632, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MMUHWT,   18,  768, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 1,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_DISP,     16, 7168, 1, 1, 0xFFFFFFFF, 0, 2, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDFW,    17,    0, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMFW,    20,    0, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMPNG,   27,  256, 5, 1, 0xF0000000, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AUDHW,    22,  512, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVP,       8,  800, 5, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_MODPE,    29,  256, 1, 1, 0xF0000000, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 1,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVPFW,    19,   64, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPTHCP,  15,  256, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_LCPDARE,  30,  128, 5, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 1,
						0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_AENPU,     3, 3072, 1, 1, 0xFFFFFFFF, 0, 2, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_ISLAND1,  12, 7936, 7, 1, 0, 0x7FFFFFFF, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_DISP_WB,  23,  512, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDVSP,    4,  256, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDDEC,    5, 6144, 4, 1, 0xFFFFFFFF, 0, 2, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_CAMOFE,   33, 6144, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_CAMRTIP,  13, 1024, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_CAMSRTIP, 14, 6144, 4, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_CAMRTRF,   7, 3584, 3, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_CAMSRTRF, 21, 6144, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 33},
	{LLCC_CPUSSMPAM, 6, 2048, 1, 1, 0xFFFFFFFF, 0, 0, 0, 0, 0, 1,
						1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const struct llcc_slice_config tuna_data[] = {
	{LLCC_CPUSS,     1, 4992, 1, 0, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPFX,  24, 1024, 5, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDSC0,    2,  512, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPGRW, 25, 1024, 5, 0, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPT,     15, 4096, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPUHTW,   11,  256, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPU,       9, 4736, 1, 0, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MMUHWT,   18,  512, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_DISP,     16, 4096, 1, 1, 0xFFFFFF,
			0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMFW,    20,    0, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMPNG,   27,  256, 5, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVP,       8,  800, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_MODPE,    29,  256, 1, 1, 0xF00000,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVPFW,    19,   64, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_LCPDARE,  30,  128, 5, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_ISLAND1,  12, 5888, 7, 1, 0x0,
			0x7FFFFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDVSP,    4,  256, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMOFE,   33, 2912, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMRTIP,  13, 2912, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMSRTIP, 14, 1024, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMRTRF,   7, 2912, 3, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMSRTRF, 21, 2912, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CPUSSMPAM, 6, 2048, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const struct llcc_slice_config tuna7_data[] = {
	{LLCC_CPUSS,     1, 4992, 1, 0, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPFX,  24, 1024, 5, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDSC0,    2,  512, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPGRW, 25, 1024, 5, 0, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPT,     15, 4096, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPUHTW,   11,  256, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPU,       9, 4736, 1, 0, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MMUHWT,   18,  512, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_DISP,     16, 4096, 1, 1, 0xFFFFFF,
			0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMFW,    20,    0, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMPNG,   27,  256, 5, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVP,       8,  800, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_MODPE,    29,  256, 1, 1, 0xF00000,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVPFW,    19,   64, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_LCPDARE,  30,  128, 5, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_ISLAND1,  12, 3072, 7, 1,      0x0,
			0xFFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDVSP,    4,  256, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMOFE,   33, 2912, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMRTIP,  13, 2912, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMSRTIP, 14, 1024, 4, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMRTRF,   7, 2912, 3, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMSRTRF, 21, 2912, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CPUSSMPAM, 6, 2048, 1, 1, 0xFFFFFF,
			0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const struct llcc_slice_config kera_data[] = {
	{LLCC_CPUSS,     1,  896, 0, 0, 0xFFF, 0, 0, 0, 0, 0, 1,
						1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPFX,  24, 1024, 5, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDSC0,    2,  128, 5, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMHPGRW, 25, 1024, 5, 0, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPUHTW,   11,  256, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPU,       9,  896, 1, 0, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MMUHWT,   18,  256, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 1,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MDMPNG,   27,  256, 5, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MODPE,    29,  256, 1, 1, 0xF00, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_WRCACHE,  31,  256, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 1,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_LCPDARE,  30,  128, 5, 1, 0xFFF, 0, 0, 0, 0, 0, 1,
						0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{LLCC_ISLAND1,  12, 1280, 7, 1, 0, 0x3FF, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMOFE,   33, 1024, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMRTIP,  13, 1024, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMSRTIP, 14, 512, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMRTRF,   7, 1024, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CAMSRTRF, 21, 1024, 1, 1, 0xFFF, 0, 0, 0, 0, 0, 0,
						0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 13},
	{LLCC_CPUSSMPAM, 6, 512, 0, 1, 0xFFF, 0, 0, 0, 0, 0, 1,
						1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const struct llcc_slice_config x1e80100_data[] = {
	{LLCC_CPUSS,	 1, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_VIDSC0,	 2,  512, 3, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AUDIO,	 6, 3072, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CMPT,     10, 6144, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPUHTW,   11, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_GPU,       9, 4096, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_MMUHWT,   18,  512, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AUDHW,    22, 1024, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CVP,       8,  512, 3, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_WRCACHE,  31,  512, 1, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMEXP1,   7, 3072, 2, 1, 0xFFF, 0x0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_LCPDARE,  30,  512, 3, 1, 0xFFF, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_AENPU,     3, 3072, 1, 1, 0xFFF, 0x0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_ISLAND1,  12,  512, 7, 1,   0x1, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_ISLAND2,  13,  512, 7, 1,   0x2, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_ISLAND3,  14,  512, 7, 1,   0x3, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_ISLAND4,  15,  512, 7, 1,   0x4, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMEXP2,  19, 3072, 3, 1, 0xFFF, 0x0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMEXP3,  20, 3072, 3, 1, 0xFFF, 0x0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{LLCC_CAMEXP4,  21, 3072, 3, 1, 0xFFF, 0x0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const struct llcc_edac_reg_offset llcc_v1_edac_reg_offset = {
	.trp_ecc_error_status0 = 0x20344,
	.trp_ecc_error_status1 = 0x20348,
	.trp_ecc_sb_err_syn0 = 0x2304c,
	.trp_ecc_db_err_syn0 = 0x20370,
	.trp_ecc_error_cntr_clear = 0x20440,
	.trp_interrupt_0_status = 0x20480,
	.trp_interrupt_0_clear = 0x20484,
	.trp_interrupt_0_enable = 0x20488,

	/* LLCC Common registers */
	.cmn_status0 = 0x3000c,
	.cmn_interrupt_0_enable = 0x3001c,
	.cmn_interrupt_2_enable = 0x3003c,

	/* LLCC DRP registers */
	.drp_ecc_error_cfg = 0x40000,
	.drp_ecc_error_cntr_clear = 0x40004,
	.drp_interrupt_status = 0x41000,
	.drp_interrupt_clear = 0x41008,
	.drp_interrupt_enable = 0x4100c,
	.drp_ecc_error_status0 = 0x42044,
	.drp_ecc_error_status1 = 0x42048,
	.drp_ecc_sb_err_syn0 = 0x4204c,
	.drp_ecc_db_err_syn0 = 0x42070,
};

static const struct llcc_edac_reg_offset llcc_v2_1_edac_reg_offset = {
	.trp_ecc_error_status0 = 0x20344,
	.trp_ecc_error_status1 = 0x20348,
	.trp_ecc_sb_err_syn0 = 0x2034c,
	.trp_ecc_db_err_syn0 = 0x20370,
	.trp_ecc_error_cntr_clear = 0x20440,
	.trp_interrupt_0_status = 0x20480,
	.trp_interrupt_0_clear = 0x20484,
	.trp_interrupt_0_enable = 0x20488,

	/* LLCC Common registers */
	.cmn_status0 = 0x3400c,
	.cmn_interrupt_0_enable = 0x3401c,
	.cmn_interrupt_2_enable = 0x3403c,

	/* LLCC DRP registers */
	.drp_ecc_error_cfg = 0x50000,
	.drp_ecc_error_cntr_clear = 0x50004,
	.drp_interrupt_status = 0x50020,
	.drp_interrupt_clear = 0x50028,
	.drp_interrupt_enable = 0x5002c,
	.drp_ecc_error_status0 = 0x520f4,
	.drp_ecc_error_status1 = 0x520f8,
	.drp_ecc_sb_err_syn0 = 0x520fc,
	.drp_ecc_db_err_syn0 = 0x52120,
};

static const struct llcc_edac_reg_offset llcc_v6_edac_reg_offset = {
	.trp_ecc_error_status0 = 0x47448,
	.trp_ecc_error_status1 = 0x47450,
	.trp_ecc_sb_err_syn0 = 0x47490,
	.trp_ecc_db_err_syn0 = 0x474d0,
	.trp_ecc_error_cntr_clear = 0x47444,
	.trp_interrupt_0_status = 0x47600,
	.trp_interrupt_0_clear = 0x47604,
	.trp_interrupt_0_enable = 0x47608,

	/* LLCC Common registers */
	.cmn_status0 = 0x6400c,
	.cmn_interrupt_0_enable = 0x6401c,
	.cmn_interrupt_2_enable = 0x6403c,

	/* LLCC DRP registers */
	.drp_ecc_error_cfg = 0x80000,
	.drp_ecc_error_cntr_clear = 0x80004,
	.drp_interrupt_status = 0x80020,
	.drp_interrupt_clear = 0x80028,
	.drp_interrupt_enable = 0x8002c,
	.drp_ecc_error_status0 = 0x820f4,
	.drp_ecc_error_status1 = 0x820f8,
	.drp_ecc_sb_err_syn0 = 0x820fc,
	.drp_ecc_db_err_syn0 = 0x82120,
};

/* LLCC register offset starting from v1.0.0 */
static const u32 llcc_v1_reg_offset[] = {
	[LLCC_COMMON_HW_INFO]	= 0x00030000,
	[LLCC_COMMON_STATUS0]	= 0x0003000c,
};

/* LLCC register offset starting from v2.0.1 */
static const u32 llcc_v2_1_reg_offset[] = {
	[LLCC_COMMON_HW_INFO]	= 0x00034000,
	[LLCC_COMMON_STATUS0]	= 0x0003400c,
};

/* LLCC register offset starting from v6.0.0 */
static const u32 llcc_v6_reg_offset[] = {
	[LLCC_COMMON_HW_INFO]		= 0x00064000,
	[LLCC_COMMON_STATUS0]		= 0x0006400c,
	[LLCC_TRP_ATTR0_CFG]		= 0x00041000,
	[LLCC_TRP_ATTR1_CFG]		= 0x00041008,
	[LLCC_TRP_ATTR2_CFG]		= 0x00041010,
	[LLCC_TRP_ATTR3_CFG]		= 0x00041014,
	[LLCC_TRP_SID_DIS_CAP_ALLOC]	= 0x00042000,
	[LLCC_TRP_ALGO_STALE_EN]	= 0x00042008,
	[LLCC_TRP_ALGO_STALE_CAP_EN]	= 0x00042010,
	[LLCC_TRP_ALGO_MRU0]		= 0x00042018,
	[LLCC_TRP_ALGO_MRU1]		= 0x00042020,
	[LLCC_TRP_ALGO_ALLOC0]		= 0x00042028,
	[LLCC_TRP_ALGO_ALLOC1]		= 0x00042030,
	[LLCC_TRP_ALGO_ALLOC2]		= 0x00042038,
	[LLCC_TRP_ALGO_ALLOC3]		= 0x00042040,
	[LLCC_TRP_WRS_EN]		= 0x00042080,
	[LLCC_TRP_WRS_CACHEABLE_EN]	= 0x00042088,
};

static const struct llcc_slice_config sdxpinn_data[] = {
	{LLCC_MDMHPGRW,     25,  128, 1, 1,      0xC, 0x0, 0, 0x0, 0, 0, 0, 0},
	{LLCC_MODHW,        26,  128, 1, 1,      0xC, 0x0, 0, 0x0, 0, 0, 0, 0},
	{LLCC_MODPE,        29,   64, 1, 1,      0xC, 0x0, 0, 0x0, 0, 0, 0, 0},
	{LLCC_APTCM,        30,  128, 3, 1,      0x0, 0x1, 1, 0x0, 1, 0, 0, 0},
	{LLCC_MDMCLAD2,     20,  128, 1, 1,      0x3, 0x0, 0, 0x0, 0, 0, 0, 0},
};

static const struct qcom_llcc_config sc7180_cfg = {
	.sct_data	= sc7180_data,
	.size		= ARRAY_SIZE(sc7180_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sc7280_cfg = {
	.sct_data	= sc7280_data,
	.size		= ARRAY_SIZE(sc7280_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sc8180x_cfg = {
	.sct_data	= sc8180x_data,
	.size		= ARRAY_SIZE(sc8180x_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sc8280xp_cfg = {
	.sct_data	= sc8280xp_data,
	.size		= ARRAY_SIZE(sc8280xp_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sdm845_cfg = {
	.sct_data	= sdm845_data,
	.size		= ARRAY_SIZE(sdm845_data),
	.need_llcc_cfg	= false,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
	.no_edac	= true,
};

static const struct qcom_llcc_config sm6350_cfg = {
	.sct_data	= sm6350_data,
	.size		= ARRAY_SIZE(sm6350_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sm7150_cfg = {
	.sct_data       = sm7150_data,
	.size           = ARRAY_SIZE(sm7150_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sm8150_cfg = {
	.sct_data       = sm8150_data,
	.size           = ARRAY_SIZE(sm8150_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sm8250_cfg = {
	.sct_data       = sm8250_data,
	.size           = ARRAY_SIZE(sm8250_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sm8350_cfg = {
	.sct_data       = sm8350_data,
	.size           = ARRAY_SIZE(sm8350_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v1_reg_offset,
	.edac_reg_offset = &llcc_v1_edac_reg_offset,
};

static const struct qcom_llcc_config sm8450_cfg = {
	.sct_data       = sm8450_data,
	.size           = ARRAY_SIZE(sm8450_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v2_1_reg_offset,
	.edac_reg_offset = &llcc_v2_1_edac_reg_offset,
};

static const struct qcom_llcc_config sm8550_cfg = {
	.sct_data       = sm8550_data,
	.size           = ARRAY_SIZE(sm8550_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v2_1_reg_offset,
	.edac_reg_offset = &llcc_v2_1_edac_reg_offset,
};

static const struct qcom_llcc_config pineapple_cfg = {
	.sct_data	    = pineapple_data,
	.size		    = ARRAY_SIZE(pineapple_data),
	.need_llcc_cfg  = true,
	.reg_offset = llcc_v2_1_reg_offset,
	.edac_reg_offset = &llcc_v2_1_edac_reg_offset,
};

static const struct qcom_llcc_config sun_cfg = {
	.sct_data       = sun_data,
	.size           = ARRAY_SIZE(sun_data),
	.need_llcc_cfg	= true,
	.reg_offset	= llcc_v6_reg_offset,
	.edac_reg_offset = &llcc_v6_edac_reg_offset,
};

static const struct qcom_llcc_config tuna_cfg = {
	.sct_data = tuna_data,
	.size = ARRAY_SIZE(tuna_data),
	.need_llcc_cfg = true,
	.reg_offset = llcc_v6_reg_offset,
	.edac_reg_offset = &llcc_v6_edac_reg_offset,
};

static const struct qcom_llcc_config tuna7_cfg = {
	.sct_data = tuna7_data,
	.size = ARRAY_SIZE(tuna7_data),
	.need_llcc_cfg = true,
	.reg_offset = llcc_v6_reg_offset,
	.edac_reg_offset = &llcc_v6_edac_reg_offset,
};

static const struct qcom_llcc_config kera_cfg = {
	.sct_data       = kera_data,
	.size           = ARRAY_SIZE(kera_data),
	.need_llcc_cfg  = true,
	.reg_offset     = llcc_v6_reg_offset,
	.edac_reg_offset = &llcc_v6_edac_reg_offset,
};

static const struct qcom_llcc_config x1e80100_cfg = {
		.sct_data	= x1e80100_data,
		.size		= ARRAY_SIZE(x1e80100_data),
		.need_llcc_cfg	= true,
		.reg_offset	= llcc_v2_1_reg_offset,
		.edac_reg_offset = &llcc_v2_1_edac_reg_offset,
};

static const struct qcom_llcc_config sdxpinn_cfg = {
	.sct_data   = sdxpinn_data,
	.size       = ARRAY_SIZE(sdxpinn_data),
	.need_llcc_cfg  = true,
	.reg_offset = llcc_v2_1_reg_offset,
};

static struct llcc_drv_data *drv_data = (void *) -EPROBE_DEFER;

/**
 * llcc_slice_getd - get llcc slice descriptor
 * @uid: usecase_id for the client
 *
 * A pointer to llcc slice descriptor will be returned on success
 * and error pointer is returned on failure
 */
struct llcc_slice_desc *llcc_slice_getd(u32 uid)
{
	const struct llcc_slice_config *cfg;
	u32 sz, count;

	if (IS_ERR(drv_data))
		return ERR_CAST(drv_data);

	cfg = drv_data->cfg;
	sz = drv_data->cfg_size;

	for (count = 0; cfg && count < sz; count++, cfg++)
		if (cfg->usecase_id == uid)
			break;

	if (count == sz || !cfg  || IS_ERR_OR_NULL(drv_data->desc))
		return ERR_PTR(-ENODEV);

	return &drv_data->desc[count];
}
EXPORT_SYMBOL_GPL(llcc_slice_getd);

/**
 * llcc_slice_putd - llcc slice descritpor
 * @desc: Pointer to llcc slice descriptor
 */
void llcc_slice_putd(struct llcc_slice_desc *desc)
{
	if (!IS_ERR_OR_NULL(desc))
		WARN(atomic_read(&desc->refcount), " Slice %d is still active\n", desc->slice_id);
}
EXPORT_SYMBOL_GPL(llcc_slice_putd);

static int llcc_update_act_ctrl(u32 sid,
				u32 act_ctrl_reg_val, u32 status)
{
	u32 act_ctrl_reg;
	u32 act_clear_reg;
	u32 status_reg;
	u32 slice_status;
	int ret;

	if (IS_ERR(drv_data))
		return PTR_ERR(drv_data);

	act_ctrl_reg = LLCC_TRP_ACT_CTRLn(sid);
	act_clear_reg = LLCC_TRP_ACT_CLEARn(sid);
	status_reg = LLCC_TRP_STATUSn(sid);

	/* Set the ACTIVE trigger */
	act_ctrl_reg_val |= ACT_CTRL_ACT_TRIG;
	ret = regmap_write(drv_data->bcast_regmap, act_ctrl_reg,
				act_ctrl_reg_val);
	if (ret)
		return ret;

	/* Clear the ACTIVE trigger */
	act_ctrl_reg_val &= ~ACT_CTRL_ACT_TRIG;
	ret = regmap_write(drv_data->bcast_regmap, act_ctrl_reg,
				act_ctrl_reg_val);
	if (ret)
		return ret;

	if (drv_data->version >= LLCC_VERSION_4_1_0_0) {
		ret = regmap_read_poll_timeout(drv_data->bcast_and_regmap, status_reg,
				      slice_status, (slice_status & ACT_COMPLETE),
				      0, LLCC_STATUS_READ_DELAY);
		if (ret)
			return ret;
	}

	ret = regmap_read_poll_timeout(drv_data->bcast_regmap, status_reg,
				      slice_status, !(slice_status & status),
				      0, LLCC_STATUS_READ_DELAY);
	if (ret)
		return ret;

	if (drv_data->version >= LLCC_VERSION_4_1_0_0)
		ret = regmap_write(drv_data->bcast_regmap, act_clear_reg,
					ACT_CLEAR);

	return ret;
}

/**
 * llcc_slice_activate - Activate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 *
 * A value of zero will be returned on success and a negative errno will
 * be returned in error cases
 */
int llcc_slice_activate(struct llcc_slice_desc *desc)
{
	int ret;
	u32 act_ctrl_val;

	if (IS_ERR(drv_data))
		return PTR_ERR(drv_data);

	if (IS_ERR_OR_NULL(desc))
		return -EINVAL;

	mutex_lock(&drv_data->lock);
	if ((atomic_read(&desc->refcount)) >= 1) {
		atomic_inc_return(&desc->refcount);
		mutex_unlock(&drv_data->lock);
		return 0;
	}

	if (test_bit(desc->slice_id, drv_data->bitmap)) {
		mutex_unlock(&drv_data->lock);
		return 0;
	}

	act_ctrl_val = ACT_CTRL_OPCODE_ACTIVATE << ACT_CTRL_OPCODE_SHIFT;

	ret = llcc_update_act_ctrl(desc->slice_id, act_ctrl_val,
				  DEACTIVATE);
	if (ret) {
		mutex_unlock(&drv_data->lock);
		return ret;
	}

	atomic_inc_return(&desc->refcount);
	__set_bit(desc->slice_id, drv_data->bitmap);
	mutex_unlock(&drv_data->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(llcc_slice_activate);

/**
 * llcc_slice_deactivate - Deactivate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 *
 * A value of zero will be returned on success and a negative errno will
 * be returned in error cases
 */
int llcc_slice_deactivate(struct llcc_slice_desc *desc)
{
	u32 act_ctrl_val;
	int ret;

	if (IS_ERR(drv_data))
		return PTR_ERR(drv_data);

	if (IS_ERR_OR_NULL(desc))
		return -EINVAL;

	mutex_lock(&drv_data->lock);
	if ((atomic_read(&desc->refcount)) > 1) {
		atomic_dec_return(&desc->refcount);
		mutex_unlock(&drv_data->lock);
		return 0;
	}

	if (!test_bit(desc->slice_id, drv_data->bitmap)) {
		mutex_unlock(&drv_data->lock);
		return 0;
	}
	act_ctrl_val = ACT_CTRL_OPCODE_DEACTIVATE << ACT_CTRL_OPCODE_SHIFT;

	ret = llcc_update_act_ctrl(desc->slice_id, act_ctrl_val,
				  ACTIVATE);
	if (ret) {
		mutex_unlock(&drv_data->lock);
		return ret;
	}

	atomic_set(&desc->refcount, 0);
	__clear_bit(desc->slice_id, drv_data->bitmap);
	mutex_unlock(&drv_data->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(llcc_slice_deactivate);

/**
 * llcc_get_slice_id - return the slice id
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_get_slice_id(struct llcc_slice_desc *desc)
{
	if (IS_ERR_OR_NULL(desc))
		return -EINVAL;

	return desc->slice_id;
}
EXPORT_SYMBOL_GPL(llcc_get_slice_id);

/**
 * llcc_get_slice_size - return the slice id
 * @desc: Pointer to llcc slice descriptor
 */
size_t llcc_get_slice_size(struct llcc_slice_desc *desc)
{
	if (IS_ERR_OR_NULL(desc))
		return 0;

	return desc->slice_size;
}
EXPORT_SYMBOL_GPL(llcc_get_slice_size);

static int llcc_staling_conf_capacity(u32 sid, struct llcc_staling_mode_params *p)
{
	u32 notif_staling_reg;

	notif_staling_reg = LLCC_TRP_STAL_ATTR1_CFGn(sid);

	return regmap_update_bits(drv_data->bcast_regmap, notif_staling_reg,
				 STALING_ENABLE_MASK,
				 LLCC_STALING_MODE_CAPACITY);
}

static int llcc_staling_conf_notify(u32 sid, struct llcc_staling_mode_params *p)
{
	u32 notif_staling_reg, staling_distance, config;
	int ret;

	if (p->notify_params.op >= LLCC_NOTIFY_STALING_OPS_MAX)
		return -EINVAL;

	config = LLCC_STALING_MODE_NOTIFY;

	if (drv_data->version >= LLCC_VERSION_6_0_0_0)
		config |= p->notify_params.op << NOTIFCN_BASED_INVDTN_EN_SHIFT;

	notif_staling_reg = LLCC_TRP_STAL_ATTR1_CFGn(sid);

	ret = regmap_update_bits(drv_data->bcast_regmap, notif_staling_reg,
				 STALING_ENABLE_MASK,
				 config);
	if (ret)
		return ret;

	staling_distance = p->notify_params.staling_distance << FRAME_DISTANCE_SHIFT;

	return regmap_update_bits(drv_data->bcast_regmap, notif_staling_reg,
				  STALING_NUM_FRAMES_MASK, staling_distance);
}

static int (*staling_mode_ops[LLCC_STALING_MODE_MAX])(u32, struct llcc_staling_mode_params *) = {
	[LLCC_STALING_MODE_CAPACITY]	= llcc_staling_conf_capacity,
	[LLCC_STALING_MODE_NOTIFY]	= llcc_staling_conf_notify,
};

/**
 * llcc_configure_staling_mode - Configure cache staling mode by setting the
 *				 staling_mode and corresponding
 *				 mode-specific params
 *
 * @desc: Pointer to llcc slice descriptor
 * @p: Staling mode-specific params
 *
 * Returns: zero on success or negative errno.
 */
int llcc_configure_staling_mode(struct llcc_slice_desc *desc,
				struct llcc_staling_mode_params *p)

{
	u32 sid;
	enum llcc_staling_mode m;

	if (IS_ERR(drv_data))
		return PTR_ERR(drv_data);

	if (drv_data->version < LLCC_VERSION_5_0_0_0)
		return -EOPNOTSUPP;

	if (IS_ERR_OR_NULL(desc) || !p)
		return -EINVAL;

	sid = desc->slice_id;
	m = p->staling_mode;

	/*
	 * Look up op corresponding to staling mode and call it
	 * with the params passed
	 */
	return (*staling_mode_ops[m])(sid, p);

}
EXPORT_SYMBOL(llcc_configure_staling_mode);

/**
 * llcc_notif_staling_inc_counter - Trigger the staling of the sub-cache frame.
 *
 * @desc: Pointer to llcc slice descriptor
 *
 * Returns: zero on success or negative errno.
 */
int llcc_notif_staling_inc_counter(struct llcc_slice_desc *desc)
{
	u32 sid, stale_trigger_reg, discard;
	int ret;

	if (IS_ERR(drv_data))
		return PTR_ERR(drv_data);

	if (drv_data->version < LLCC_VERSION_5_0_0_0)
		return -EOPNOTSUPP;

	if (IS_ERR_OR_NULL(desc))
		return -EINVAL;

	sid = desc->slice_id;
	stale_trigger_reg = LLCC_TRP_STAL_ATTR0_CFGn(sid);

	ret = regmap_update_bits(drv_data->bcast_regmap, stale_trigger_reg,
				 STALING_TRIGGER_MASK, STALING_TRIGGER_MASK);
	if (ret)
		return ret;

	/*
	 * stale_trigger_reg is a self-clearing reg. Read it anyway to ensure
	 * that the write went through. We don't care about the value being
	 * read, so discard it.
	 */
	return regmap_read(drv_data->bcast_regmap, stale_trigger_reg, &discard);
}
EXPORT_SYMBOL(llcc_notif_staling_inc_counter);

static int _qcom_llcc_cfg_program(const struct llcc_slice_config *config,
				  const struct qcom_llcc_config *cfg)
{
	int ret;
	u32 attr2_cfg;
	u32 attr1_cfg;
	u32 attr0_cfg;
	u32 attr2_val;
	u32 attr1_val;
	u32 attr0_val;
	u32 max_cap_cacheline;
	struct llcc_slice_desc *desc;

	attr1_val = config->cache_mode;
	attr1_val |= config->probe_target_ways << ATTR1_PROBE_TARGET_WAYS_SHIFT;
	attr1_val |= config->fixed_size << ATTR1_FIXED_SIZE_SHIFT;
	attr1_val |= config->priority << ATTR1_PRIORITY_SHIFT;

	max_cap_cacheline = MAX_CAP_TO_BYTES(config->max_cap);

	/*
	 * LLCC instances can vary for each target.
	 * The SW writes to broadcast register which gets propagated
	 * to each llcc instance (llcc0,.. llccN).
	 * Since the size of the memory is divided equally amongst the
	 * llcc instances, we need to configure the max cap accordingly.
	 */
	max_cap_cacheline = max_cap_cacheline / drv_data->num_banks;
	max_cap_cacheline >>= CACHE_LINE_SIZE_SHIFT;
	attr1_val |= max_cap_cacheline << ATTR1_MAX_CAP_SHIFT;

	attr1_cfg = LLCC_TRP_ATTR1_CFGn(config->slice_id);

	ret = regmap_write(drv_data->bcast_regmap, attr1_cfg, attr1_val);
	if (ret)
		return ret;

	if (drv_data->version >= LLCC_VERSION_4_1_0_0) {
		attr2_cfg = LLCC_TRP_ATTR2_CFGn(config->slice_id);
		attr0_val = config->res_ways;
		attr2_val = config->bonus_ways;
	} else {
		attr0_val = config->res_ways & ATTR0_RES_WAYS_MASK;
		attr0_val |= config->bonus_ways << ATTR0_BONUS_WAYS_SHIFT;
	}

	attr0_cfg = LLCC_TRP_ATTR0_CFGn(config->slice_id);

	ret = regmap_write(drv_data->bcast_regmap, attr0_cfg, attr0_val);
	if (ret)
		return ret;

	if (drv_data->version >= LLCC_VERSION_4_1_0_0) {
		ret = regmap_write(drv_data->bcast_regmap, attr2_cfg, attr2_val);
		if (ret)
			return ret;
	}

	if (cfg->need_llcc_cfg) {
		u32 disable_cap_alloc, retain_pc;

		disable_cap_alloc = config->dis_cap_alloc << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_SCID_DIS_CAP_ALLOC,
					 BIT(config->slice_id), disable_cap_alloc);
		if (ret)
			return ret;

		if (drv_data->version < LLCC_VERSION_4_1_0_0) {
			retain_pc = config->retain_on_pc << config->slice_id;
			ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_PCB_ACT,
						 BIT(config->slice_id), retain_pc);
			if (ret)
				return ret;
		}
	}

	if (drv_data->version >= LLCC_VERSION_2_0_0_0) {
		u32 wren;

		wren = config->write_scid_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_WRSC_EN,
					 BIT(config->slice_id), wren);
		if (ret)
			return ret;
	}

	if (drv_data->version >= LLCC_VERSION_2_1_0_0) {
		u32 wr_cache_en;

		wr_cache_en = config->write_scid_cacheable_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_WRSC_CACHEABLE_EN,
					 BIT(config->slice_id), wr_cache_en);
		if (ret)
			return ret;
	}

	if (drv_data->version >= LLCC_VERSION_4_1_0_0) {
		u32 stale_en;
		u32 stale_cap_en;
		u32 mru_uncap_en;
		u32 mru_rollover;
		u32 alloc_oneway_en;
		u32 ovcap_en;
		u32 ovcap_prio;
		u32 vict_prio;

		stale_en = config->stale_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG1,
					 BIT(config->slice_id), stale_en);
		if (ret)
			return ret;

		stale_cap_en = config->stale_cap_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG2,
					 BIT(config->slice_id), stale_cap_en);
		if (ret)
			return ret;

		mru_uncap_en = config->mru_uncap_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG3,
					 BIT(config->slice_id), mru_uncap_en);
		if (ret)
			return ret;

		mru_rollover = config->mru_rollover << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG4,
					 BIT(config->slice_id), mru_rollover);
		if (ret)
			return ret;

		alloc_oneway_en = config->alloc_oneway_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG5,
					 BIT(config->slice_id), alloc_oneway_en);
		if (ret)
			return ret;

		ovcap_en = config->ovcap_en << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG6,
					 BIT(config->slice_id), ovcap_en);
		if (ret)
			return ret;

		ovcap_prio = config->ovcap_prio << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG7,
					 BIT(config->slice_id), ovcap_prio);
		if (ret)
			return ret;

		vict_prio = config->vict_prio << config->slice_id;
		ret = regmap_update_bits(drv_data->bcast_regmap, LLCC_TRP_ALGO_CFG8,
					 BIT(config->slice_id), vict_prio);
		if (ret)
			return ret;
	}

	if (config->activate_on_init) {
		desc = llcc_slice_getd(config->usecase_id);
		if (PTR_ERR_OR_ZERO(desc))
			return -EINVAL;

		ret = llcc_slice_activate(desc);
	}

	return ret;
}

static int _qcom_llcc_cfg_program_v6(const struct llcc_slice_config *config,
				  const struct qcom_llcc_config *cfg)
{
	int ret;
	u32 attr0_cfg, attr1_cfg, attr2_cfg, attr3_cfg;
	u32 attr0_val, attr1_val, attr2_val, attr3_val;
	u32 disable_cap_alloc, wren, wr_cache_en;
	u32 stale_en, stale_cap_en, mru_uncap_en, mru_rollover;
	u32 alloc_oneway_en, ovcap_en, ovcap_prio, vict_prio;
	u32 slice_offset, reg_offset;
	struct llcc_slice_desc *desc;
	const struct llcc_slice_config *slice_cfg;
	u32 sz, slice = 0;

	slice_cfg = cfg->sct_data;
	sz = cfg->size;

	attr0_cfg = LLCC_V6_TRP_ATTR0_CFGn(config->slice_id);
	attr1_cfg = LLCC_V6_TRP_ATTR1_CFGn(config->slice_id);
	attr2_cfg = LLCC_V6_TRP_ATTR2_CFGn(config->slice_id);
	attr3_cfg = LLCC_V6_TRP_ATTR3_CFGn(config->slice_id);

	attr0_val = config->res_ways;
	attr1_val = config->bonus_ways;
	attr2_val = config->cache_mode;
	attr2_val |= config->probe_target_ways << ATTR2_PROBE_TARGET_WAYS_SHIFT;
	attr2_val |= config->fixed_size << ATTR2_FIXED_SIZE_SHIFT;
	attr2_val |= config->priority << ATTR2_PRIORITY_SHIFT;
	if (config->in_a_group) {
		if (!(config->parent_slice_id) || !(config->fixed_size)) {
			pr_err("SCID grouping failed for SCID:%d parent_SCID:%d FIXED_SIZE:%d\n",
				config->slice_id, config->parent_slice_id, config->fixed_size);
		} else {
			for (slice = 0; slice_cfg && slice < sz; slice++, slice_cfg++) {
				if (slice_cfg->slice_id == config->parent_slice_id)
					break;
			}
			if (slice == sz || !slice_cfg) {
				pr_err("SCID grouping failed for SCID:%d, invalid parent_SCID:%d\n",
					config->slice_id, config->parent_slice_id);
			} else if (config->max_cap > slice_cfg->max_cap) {
				pr_err("SCID grouping failed for SCID:%d, invalid MAX_CAP:%x, PARENT_MAXCAP:%x\n",
					config->slice_id, config->max_cap, slice_cfg->max_cap);
			} else {
				attr2_val |= config->parent_slice_id << ATTR2_PARENT_SLICE_ID_SHIFT;
				attr2_val |= config->in_a_group << ATTR2_IN_A_GROUP_SHIFT;
			}
		}
	}

	attr3_val = MAX_CAP_TO_BYTES(config->max_cap);
	attr3_val /= drv_data->num_banks;
	attr3_val >>= CACHE_LINE_SIZE_SHIFT;

	ret = regmap_write(drv_data->bcast_regmap, attr0_cfg, attr0_val);
	if (ret)
		return ret;

	ret = regmap_write(drv_data->bcast_regmap, attr1_cfg, attr1_val);
	if (ret)
		return ret;

	ret = regmap_write(drv_data->bcast_regmap, attr2_cfg, attr2_val);
	if (ret)
		return ret;

	ret = regmap_write(drv_data->bcast_regmap, attr3_cfg, attr3_val);
	if (ret)
		return ret;

	slice_offset = config->slice_id % 32;
	reg_offset = (config->slice_id / 32) * 4;

	if (cfg->need_llcc_cfg) {
		disable_cap_alloc = config->dis_cap_alloc << slice_offset;
		ret = regmap_write(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_SID_DIS_CAP_ALLOC] + reg_offset,
			disable_cap_alloc);

		if (ret)
			return ret;
	}

	wren = config->write_scid_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_WRS_EN] + reg_offset,
			BIT(slice_offset), wren);
	if (ret)
		return ret;

	wr_cache_en = config->write_scid_cacheable_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_WRS_CACHEABLE_EN] + reg_offset,
			BIT(slice_offset), wr_cache_en);
	if (ret)
		return ret;

	stale_en = config->stale_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_STALE_EN] + reg_offset,
			BIT(slice_offset), stale_en);
	if (ret)
		return ret;

	stale_cap_en = config->stale_cap_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_STALE_CAP_EN] + reg_offset,
			BIT(slice_offset), stale_cap_en);
	if (ret)
		return ret;

	mru_uncap_en = config->mru_uncap_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_MRU0] + reg_offset,
			BIT(slice_offset), mru_uncap_en);
	if (ret)
		return ret;

	mru_rollover = config->mru_rollover << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_MRU1] + reg_offset,
			BIT(slice_offset), mru_rollover);
	if (ret)
		return ret;

	alloc_oneway_en = config->alloc_oneway_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_ALLOC0] + reg_offset,
			BIT(slice_offset), alloc_oneway_en);
	if (ret)
		return ret;

	ovcap_en = config->ovcap_en << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_ALLOC1] + reg_offset,
			BIT(slice_offset), ovcap_en);
	if (ret)
		return ret;

	ovcap_prio = config->ovcap_prio << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_ALLOC2] + reg_offset,
			BIT(slice_offset), ovcap_prio);
	if (ret)
		return ret;

	vict_prio = config->vict_prio << slice_offset;
	ret = regmap_update_bits(drv_data->bcast_regmap,
			cfg->reg_offset[LLCC_TRP_ALGO_ALLOC3] + reg_offset,
			BIT(slice_offset), vict_prio);
	if (ret)
		return ret;

	if (config->activate_on_init) {
		desc = llcc_slice_getd(config->usecase_id);
		if (PTR_ERR_OR_ZERO(desc))
			return -EINVAL;

		ret = llcc_slice_activate(desc);
	}

	return ret;
}
static int qcom_llcc_cfg_program(struct platform_device *pdev,
				 const struct qcom_llcc_config *cfg)
{
	int i;
	u32 sz;
	int ret = 0;
	const struct llcc_slice_config *llcc_table;

	sz = drv_data->cfg_size;
	llcc_table = drv_data->cfg;

	for (i = 0; i < sz; i++) {
		drv_data->desc[i].slice_id = llcc_table[i].slice_id;
		drv_data->desc[i].slice_size = llcc_table[i].max_cap;
		atomic_set(&drv_data->desc[i].refcount, 0);
	}
	if (drv_data->version < LLCC_VERSION_6_0_0_0) {
		for (i = 0; i < sz; i++) {
			ret = _qcom_llcc_cfg_program(&llcc_table[i], cfg);
			if (ret)
				return ret;
		}
	} else {
		for (i = 0; i < sz; i++) {
			ret = _qcom_llcc_cfg_program_v6(&llcc_table[i], cfg);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int qcom_llcc_remove(struct platform_device *pdev)
{
	/* Set the global pointer to a error code to avoid referencing it */
	drv_data = ERR_PTR(-ENODEV);
	return 0;
}

static struct regmap *qcom_llcc_init_mmio(struct platform_device *pdev, u8 index,
					  const char *name)
{
	void __iomem *base;
	struct regmap_config llcc_regmap_config = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
		.fast_io = true,
	};

	base = devm_platform_ioremap_resource(pdev, index);
	if (IS_ERR(base))
		return ERR_CAST(base);

	llcc_regmap_config.name = name;
	return devm_regmap_init_mmio(&pdev->dev, base, &llcc_regmap_config);
}

static int qcom_llcc_probe(struct platform_device *pdev)
{
	u32 num_banks;
	struct device *dev = &pdev->dev;
	int ret, i;
	struct platform_device *llcc_edac;
	const struct qcom_llcc_config *cfg;
	const struct llcc_slice_config *llcc_cfg;
	u32 sz;
	u32 version;
	struct regmap *regmap;

	if (!IS_ERR(drv_data))
		return -EBUSY;

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data) {
		ret = -ENOMEM;
		goto err;
	}

	/* Initialize the first LLCC bank regmap */
	regmap = qcom_llcc_init_mmio(pdev, 0, "llcc0_base");
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		goto err;
	}

	cfg = of_device_get_match_data(&pdev->dev);
	if (!cfg) {
		ret = -EINVAL;
		goto err;
	}

	ret = regmap_read(regmap, cfg->reg_offset[LLCC_COMMON_STATUS0], &num_banks);
	if (ret)
		goto err;

	num_banks &= LLCC_LB_CNT_MASK;
	num_banks >>= LLCC_LB_CNT_SHIFT;
	drv_data->num_banks = num_banks;

	drv_data->regmaps = devm_kcalloc(dev, num_banks, sizeof(*drv_data->regmaps), GFP_KERNEL);
	if (!drv_data->regmaps) {
		ret = -ENOMEM;
		goto err;
	}

	drv_data->regmaps[0] = regmap;

	/* Initialize rest of LLCC bank regmaps */
	for (i = 1; i < num_banks; i++) {
		char *base = kasprintf(GFP_KERNEL, "llcc%d_base", i);

		drv_data->regmaps[i] = qcom_llcc_init_mmio(pdev, i, base);
		if (IS_ERR(drv_data->regmaps[i])) {
			ret = PTR_ERR(drv_data->regmaps[i]);
			kfree(base);
			goto err;
		}

		kfree(base);
	}

	drv_data->bcast_regmap = qcom_llcc_init_mmio(pdev, i, "llcc_broadcast_or_base");
	if (IS_ERR(drv_data->bcast_regmap)) {
		ret = PTR_ERR(drv_data->bcast_regmap);
		goto err;
	}

	drv_data->bcast_and_regmap = qcom_llcc_init_mmio(pdev, i+1, "llcc_broadcast_and_base");
	if (IS_ERR(drv_data->bcast_and_regmap)) {
		ret = PTR_ERR(drv_data->bcast_and_regmap);
		goto err;
	}

	/* Extract version of the IP */
	ret = regmap_read(drv_data->bcast_regmap, cfg->reg_offset[LLCC_COMMON_HW_INFO],
			  &version);
	if (ret)
		goto err;

	drv_data->version = version;

	llcc_cfg = cfg->sct_data;
	sz = cfg->size;

	drv_data->desc = devm_kzalloc(dev, sizeof(struct llcc_slice_desc)*sz, GFP_KERNEL);
	if (IS_ERR_OR_NULL(drv_data->desc)) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < sz; i++)
		if (llcc_cfg[i].slice_id > drv_data->max_slices)
			drv_data->max_slices = llcc_cfg[i].slice_id;

	drv_data->bitmap = devm_bitmap_zalloc(dev, drv_data->max_slices,
					      GFP_KERNEL);
	if (!drv_data->bitmap) {
		ret = -ENOMEM;
		goto err;
	}

	drv_data->cfg = llcc_cfg;
	drv_data->cfg_size = sz;
	drv_data->edac_reg_offset = cfg->edac_reg_offset;
	mutex_init(&drv_data->lock);
	platform_set_drvdata(pdev, drv_data);

	ret = qcom_llcc_cfg_program(pdev, cfg);
	if (ret)
		goto err;

	drv_data->ecc_irq = platform_get_irq_optional(pdev, 0);

	/*
	 * On some platforms, the access to EDAC registers will be locked by
	 * the bootloader. So probing the EDAC driver will result in a crash.
	 * Hence, disable the creation of EDAC platform device for the
	 * problematic platforms.
	 */
	if (!cfg->no_edac) {
		llcc_edac = platform_device_register_data(&pdev->dev,
						"qcom_llcc_edac", -1, drv_data,
						sizeof(*drv_data));
		if (IS_ERR(llcc_edac))
			dev_err(dev, "Failed to register llcc edac driver\n");
	}

	if (of_platform_populate(dev->of_node, NULL, NULL, dev) < 0)
		dev_err(dev, "llcc populate failed!!\n");

	return 0;
err:
	drv_data = ERR_PTR(-ENODEV);
	return ret;
}

static const struct of_device_id qcom_llcc_of_match[] = {
	{ .compatible = "qcom,sc7180-llcc", .data = &sc7180_cfg },
	{ .compatible = "qcom,sc7280-llcc", .data = &sc7280_cfg },
	{ .compatible = "qcom,sc8180x-llcc", .data = &sc8180x_cfg },
	{ .compatible = "qcom,sc8280xp-llcc", .data = &sc8280xp_cfg },
	{ .compatible = "qcom,sdm845-llcc", .data = &sdm845_cfg },
	{ .compatible = "qcom,sm6350-llcc", .data = &sm6350_cfg },
	{ .compatible = "qcom,sm7150-llcc", .data = &sm7150_cfg },
	{ .compatible = "qcom,sm8150-llcc", .data = &sm8150_cfg },
	{ .compatible = "qcom,sm8250-llcc", .data = &sm8250_cfg },
	{ .compatible = "qcom,sm8350-llcc", .data = &sm8350_cfg },
	{ .compatible = "qcom,sm8450-llcc", .data = &sm8450_cfg },
	{ .compatible = "qcom,sm8550-llcc", .data = &sm8550_cfg },
	{ .compatible = "qcom,pineapple-llcc", .data = &pineapple_cfg },
	{ .compatible = "qcom,sun-llcc", .data = &sun_cfg },
	{ .compatible = "qcom,tuna-llcc", .data = &tuna_cfg },
	{ .compatible = "qcom,tuna7-llcc", .data = &tuna7_cfg },
	{ .compatible = "qcom,kera-llcc", .data = &kera_cfg },
	{ .compatible = "qcom,x1e80100-llcc", .data = &x1e80100_cfg },
	{ .compatible = "qcom,sdxpinn-llcc", .data = &sdxpinn_cfg },
	{ }
};
MODULE_DEVICE_TABLE(of, qcom_llcc_of_match);

static struct platform_driver qcom_llcc_driver = {
	.driver = {
		.name = "qcom-llcc",
		.of_match_table = qcom_llcc_of_match,
	},
	.probe = qcom_llcc_probe,
	.remove = qcom_llcc_remove,
};
module_platform_driver(qcom_llcc_driver);

MODULE_DESCRIPTION("Qualcomm Last Level Cache Controller");
MODULE_LICENSE("GPL v2");
