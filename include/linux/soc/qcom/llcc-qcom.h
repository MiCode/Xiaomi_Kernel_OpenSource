/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023,  Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/types.h>
#ifndef __LLCC_QCOM__
#define __LLCC_QCOM__

#define LLCC_CPUSS       1
#define LLCC_VIDSC0      2
#define LLCC_VIDSC1      3
#define LLCC_ROTATOR     4
#define LLCC_VOICE       5
#define LLCC_AUDIO       6
#define LLCC_MDMHPGRW    7
#define LLCC_MDM         8
#define LLCC_MDMHW       9
#define LLCC_CMPT        10
#define LLCC_GPUHTW      11
#define LLCC_GPU         12
#define LLCC_MMUHWT      13
#define LLCC_CMPTDMA     15
#define LLCC_DISP        16
#define LLCC_VIDFW       17
#define LLCC_MDMHPFX     20
#define LLCC_MDMPNG      21
#define LLCC_AUDHW       22
#define LLCC_ECC         26
#define LLCC_CVP         28
#define LLCC_MDMVPE      29
#define LLCC_APTCM       30
#define LLCC_WRTCH       31
#define LLCC_CVPFW       32
#define LLCC_CPUSS1      33
#define LLCC_CAMEXP0     34
#define LLCC_CPUMTE      35
#define LLCC_CPUHWT      36
#define LLCC_MDMCLAD2    37
#define LLCC_CAMEXP1     38
#define LLCC_LCPDARE     40
#define LLCC_AENPU       45
#define LLCC_VIEYE       57
#define LLCC_VIDPTH      58
#define LLCC_GPUMV       59
#define LLCC_EVALFT      60
#define LLCC_EVARGHT     61
#define LLCC_EVAGAIN     62
#define LLCC_VIPTH       63
#define LLCC_DISLFT      65
#define LLCC_DISRGHT     66
#define LLCC_EVCSLFT     67
#define LLCC_EVCSRGHT    68
#define LLCC_SPAD        69


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
};


/**
 * llcc_slice_desc - Cache slice descriptor
 * @slice_id: llcc slice id
 * @slice_size: Size allocated for the llcc slice
 */
struct llcc_slice_desc {
	u32 slice_id;
	size_t slice_size;
	atomic_t refcount;
};

/**
 * llcc_edac_reg_data - llcc edac registers data for each error type
 * @name: Name of the error
 * @synd_reg: Syndrome register address
 * @count_status_reg: Status register address to read the error count
 * @ways_status_reg: Status register address to read the error ways
 * @reg_cnt: Number of registers
 * @count_mask: Mask value to get the error count
 * @ways_mask: Mask value to get the error ways
 * @count_shift: Shift value to get the error count
 * @ways_shift: Shift value to get the error ways
 */
struct llcc_edac_reg_data {
	char *name;
	u64 synd_reg;
	u64 count_status_reg;
	u64 ways_status_reg;
	u32 reg_cnt;
	u32 count_mask;
	u32 ways_mask;
	u8  count_shift;
	u8  ways_shift;
};

/**
 * llcc_drv_data - Data associated with the llcc driver
 * @regmap: regmap associated with the llcc device
 * @bcast_regmap: regmap associated with llcc broadcast offset
 * @cfg: pointer to the data structure for slice configuration
 * @lock: mutex associated with each slice
 * @cfg_index: index of config table if multiple configs present for a target
 * @cfg_size: size of the config data table
 * @max_slices: max slices as read from device tree
 * @num_banks: Number of llcc banks
 * @bitmap: Bit map to track the active slice ids
 * @offsets: Pointer to the bank offsets array
 * @ecc_irq: interrupt for llcc cache error detection and reporting
 * @llcc_ver: hardware version (20 for V2.0)
 * @desc: Array pointer of llcc_slice_desc
 */
struct llcc_drv_data {
	struct regmap *regmap;
	struct regmap *bcast_regmap;
	const struct llcc_slice_config *cfg;
	struct mutex lock;
	u32 cfg_index;
	u32 cfg_size;
	u32 max_slices;
	u32 num_banks;
	unsigned long *bitmap;
	u32 *offsets;
	int ecc_irq;
	int llcc_ver;
	bool cap_based_alloc_and_pwr_collapse;
	struct llcc_slice_desc *desc;
	struct regmap *spad_or_bcast_regmap;
	struct regmap *spad_and_bcast_regmap;
	bool spad_act_slp_wake_enable;
};

/**
 * llcc_tcm_data - Data associated with the llcc tcm driver
 *
 */
struct llcc_tcm_data {
	phys_addr_t phys_addr;
	void __iomem *virt_addr;
	size_t mem_size;
};


#if IS_ENABLED(CONFIG_QCOM_LLCC)
/**
 * llcc_slice_getd - get llcc slice descriptor
 * @uid: usecase_id of the client
 */
struct llcc_slice_desc *llcc_slice_getd(u32 uid);

/**
 * llcc_slice_putd - llcc slice descritpor
 * @desc: Pointer to llcc slice descriptor
 */
void llcc_slice_putd(struct llcc_slice_desc *desc);

/**
 * llcc_get_slice_id - get slice id
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_get_slice_id(struct llcc_slice_desc *desc);

/**
 * llcc_get_slice_size - llcc slice size
 * @desc: Pointer to llcc slice descriptor
 */
size_t llcc_get_slice_size(struct llcc_slice_desc *desc);

/**
 * llcc_slice_activate - Activate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_slice_activate(struct llcc_slice_desc *desc);

/**
 * llcc_slice_deactivate - Deactivate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_slice_deactivate(struct llcc_slice_desc *desc);

/**
 * llcc_tcm_activate - Activate llcc tcm
 */
struct llcc_tcm_data *llcc_tcm_activate(void);

/**
 * llcc_tcm_get_phys_addr - get the physical address of llcc tcm slice
 */
phys_addr_t llcc_tcm_get_phys_addr(struct llcc_tcm_data *tcm_data);

/**
 * llcc_tcm_get_virt_addr - get the virtual address of llcc tcm slice
 */
void __iomem *llcc_tcm_get_virt_addr(struct llcc_tcm_data *tcm_data);

/**
 * llcc_tcm_get_slice_size - get the llcc tcm slice size
 */
size_t llcc_tcm_get_slice_size(struct llcc_tcm_data *tcm_data);

/**
 * llcc_tcm_deactivate - Deactivate the llcc tcm
 */
void llcc_tcm_deactivate(struct llcc_tcm_data *tcm_data);


#else
static inline struct llcc_slice_desc *llcc_slice_getd(u32 uid)
{
	return NULL;
}

static inline void llcc_slice_putd(struct llcc_slice_desc *desc)
{

};

static inline int llcc_get_slice_id(struct llcc_slice_desc *desc)
{
	return -EINVAL;
}

static inline size_t llcc_get_slice_size(struct llcc_slice_desc *desc)
{
	return 0;
}

static inline int llcc_slice_activate(struct llcc_slice_desc *desc)
{
	return -EINVAL;
}

static inline int llcc_slice_deactivate(struct llcc_slice_desc *desc)
{
	return -EINVAL;
}

static inline struct llcc_tcm_data *llcc_tcm_activate(void)
{
	return NULL;
}

static inline phys_addr_t llcc_tcm_get_phys_addr(struct llcc_tcm_data *tcm_data)
{
	return 0;
}

static inline void __iomem *llcc_tcm_get_virt_addr(struct llcc_tcm_data *tcm_data)
{
	return NULL;
}

static inline size_t llcc_tcm_get_slice_size(struct llcc_tcm_data *tcm_data)
{
	return 0;
}

static inline void llcc_tcm_deactivate(struct llcc_tcm_data *tcm_data)
{

}

#endif

#endif
