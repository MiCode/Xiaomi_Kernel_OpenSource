/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _SNOC_H_
#define _SNOC_H_

#include "hw.h"
#include "ce.h"
#include "pci.h"
#define ATH10K_SNOC_RX_POST_RETRY_MS 50
#define CE_POLL_PIPE 4

/* struct snoc_state: SNOC target state
 * @pipe_cfg_addr: pipe configuration address
 * @svc_to_pipe_map: pipe services
 */
struct snoc_state {
	u32 pipe_cfg_addr;
	u32 svc_to_pipe_map;
};

/* struct ath10k_snoc_pipe: SNOC pipe configuration
 * @ath10k_ce_pipe: pipe handle
 * @pipe_num: pipe number
 * @hif_ce_state: pointer to ce state
 * @buf_sz: buffer size
 * @pipe_lock: pipe lock
 * @ar_snoc: snoc private structure
 * @intr: tasklet structure
 */

struct ath10k_snoc_pipe {
	struct ath10k_ce_pipe *ce_hdl;
	u8 pipe_num;
	struct ath10k *hif_ce_state;
	size_t buf_sz;
	/* protect ce info */
	spinlock_t pipe_lock;
	struct ath10k_snoc *ar_snoc;
	struct tasklet_struct intr;
};

/* struct ath10k_snoc_supp_chip: supported chip set
 * @dev_id: device id
 * @rev_id: revison id
 */
struct ath10k_snoc_supp_chip {
	u32 dev_id;
	u32 rev_id;
};

/* struct ath10k_snoc_info: SNOC info struct
 * @v_addr: base virtual address
 * @p_addr: base physical address
 * @chip_id: chip id
 * @chip_family: chip family
 * @board_id: board id
 * @soc_id: soc id
 * @fw_version: fw version
 */
struct ath10k_snoc_info {
	void __iomem *v_addr;
	phys_addr_t p_addr;
	u32 chip_id;
	u32 chip_family;
	u32 board_id;
	u32 soc_id;
	u32 fw_version;
};

/* struct ath10k_target_info: SNOC target info
 * @target_version: target version
 * @target_type: target type
 * @target_revision: target revision
 * @soc_version: target soc version
 */
struct ath10k_target_info {
	u32 target_version;
	u32 target_type;
	u32 target_revision;
	u32 soc_version;
};

/* struct ath10k_snoc: SNOC info struct
 * @dev: device structure
 * @ar:ath10k base structure
 * @mem: mem base virtual address
 * @mem_pa: mem base physical address
 * @target_info: snoc target info
 * @mem_len: mempry map length
 * @intr_tq: rx tasklet handle
 * @pipe_info: pipe info struct
 * @ce_lock: protect ce structures
 * @ce_states: maps ce id to ce state
 * @rx_post_retry: rx buffer post processing timer
 * @vaddr_rri_on_ddr: virtual address for RRI
 * @is_driver_probed: flag to indicate driver state
 */
struct ath10k_snoc {
	struct platform_device *dev;
	struct ath10k *ar;
	void __iomem *mem;
	dma_addr_t mem_pa;
	struct ath10k_target_info target_info;
	size_t mem_len;
	struct tasklet_struct intr_tq;
	struct ath10k_snoc_pipe pipe_info[CE_COUNT_MAX];
	/* protects CE info */
	spinlock_t ce_lock;
	struct ath10k_ce_pipe ce_states[CE_COUNT_MAX];
	struct timer_list rx_post_retry;
	u32 ce_irqs[CE_COUNT_MAX];
	u32 *vaddr_rri_on_ddr;
	bool is_driver_probed;
};

/* struct ath10k_ce_tgt_pipe_cfg: target pipe configuration
 * @pipe_num: pipe number
 * @pipe_dir: pipe direction
 * @nentries: entries in pipe
 * @nbytes_max: pipe max size
 * @flags: pipe flags
 * @reserved: reserved
 */
struct ath10k_ce_tgt_pipe_cfg {
	u32 pipe_num;
	u32 pipe_dir;
	u32 nentries;
	u32 nbytes_max;
	u32 flags;
	u32 reserved;
};

/* struct ath10k_ce_svc_pipe_cfg: service pipe configuration
 * @service_id: target version
 * @pipe_dir: pipe direction
 * @pipe_num: pipe number
 */
struct ath10k_ce_svc_pipe_cfg {
	u32 service_id;
	u32 pipe_dir;
	u32 pipe_num;
};

/* struct ath10k_shadow_reg_cfg: shadow register configuration
 * @ce_id: copy engine id
 * @reg_offset: offset to copy engine
 */
struct ath10k_shadow_reg_cfg {
	u16 ce_id;
	u16 reg_offset;
};

/* struct ath10k_wlan_enable_cfg: wlan enable configuration
 * @num_ce_tgt_cfg: no of ce target configuration
 * @ce_tgt_cfg: target ce configuration
 * @num_ce_svc_pipe_cfg: no of ce service configuration
 * @ce_svc_cfg: ce service configuration
 * @num_shadow_reg_cfg: no of shadow registers
 * @shadow_reg_cfg: shadow register configuration
 */
struct ath10k_wlan_enable_cfg {
	u32 num_ce_tgt_cfg;
	struct ath10k_ce_tgt_pipe_cfg *ce_tgt_cfg;
	u32 num_ce_svc_pipe_cfg;
	struct ath10k_ce_svc_pipe_cfg *ce_svc_cfg;
	u32 num_shadow_reg_cfg;
	struct ath10k_shadow_reg_cfg *shadow_reg_cfg;
};

/* enum ath10k_driver_mode: ath10k driver mode
 * @ATH10K_MISSION: mission mode
 * @ATH10K_FTM: ftm mode
 * @ATH10K_EPPING: epping mode
 * @ATH10K_OFF: off mode
 */
enum ath10k_driver_mode {
	ATH10K_MISSION,
	ATH10K_FTM,
	ATH10K_EPPING,
	ATH10K_OFF
};

static inline struct ath10k_snoc *ath10k_snoc_priv(struct ath10k *ar)
{
	return (struct ath10k_snoc *)ar->drv_priv;
}

void ath10k_snoc_write32(void *ar, u32 offset, u32 value);
void ath10k_snoc_soc_write32(struct ath10k *ar, u32 addr, u32 val);
void ath10k_snoc_reg_write32(struct ath10k *ar, u32 addr, u32 val);
u32 ath10k_snoc_read32(void *ar, u32 offset);
u32 ath10k_snoc_soc_read32(struct ath10k *ar, u32 addr);
u32 ath10k_snoc_reg_read32(struct ath10k *ar, u32 addr);

#endif /* _SNOC_H_ */
