/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __APUSYS_DAPC_CFG_H__
#define __APUSYS_DAPC_CFG_H__

#include <linux/types.h>

struct dapc_driver;

struct dapc_slave {
	int sys_idx;
	int ctrl_idx;
	int vio_idx;
	char *name;
	bool vio_irq_en;
};

struct dapc_exception {
	/* vio_dbg0 */
	uint32_t read_vio;
	uint32_t write_vio;
	uint32_t addr_high;
	uint32_t domain_id;
	/* vio_dbg1 */
	uint32_t trans_id;
	/* vio_dbg2 */
	uint32_t addr;
};

#define dapc_reg_base(d) ((unsigned long)d->reg)

#define dapc_reg_w(d, r, val) \
	iowrite32(val, (void *) (dapc_reg_base(d) + (r)))
#define dapc_reg_r(d, r) \
	ioread32((void *) (dapc_reg_base(d) + (r)))
#define dapc_reg_clr(d, r, mask) \
	dapc_reg_w(d, r, dapc_reg_r(d, r) & ~(mask))
#define dapc_reg_set(d, r, mask) \
	dapc_reg_w(d, r, dapc_reg_r(d, r) | (mask))

#define DAPC_SLAVE_END {-1, -1, -1, NULL, false}

struct dapc_config {
	int irq_enable;  /* enable interrupt */
	struct dapc_slave *slv;
	int slv_cnt;

	uint32_t (*vio_mask)(uint32_t idx);
	uint32_t (*vio_sta)(uint32_t idx);
	int (*excp_info)(struct dapc_driver *d, struct dapc_exception *ex);

	uint32_t apc_con;
	uint32_t apc_con_vio;
	uint32_t vio_shift_sta;
	uint32_t vio_shift_sel;
	uint32_t vio_shift_con;
	uint32_t vio_shift_con_mask;  /* shft_en | shft_done */
	uint32_t vio_dbg0;
	uint32_t vio_dbg1;
	uint32_t vio_dbg2;

	uint32_t slv_per_dapc;
	uint32_t vio_shift_max_bit;
	uint32_t vio_dbg_dmnid;
	uint32_t vio_dbg_dmnid_shift;
	uint32_t vio_dbg_w_vio;
	uint32_t vio_dbg_w_vio_shift;
	uint32_t vio_dbg_r_vio;
	uint32_t vio_dbg_r_vio_shift;
	uint32_t vio_dbg_addr;
	uint32_t vio_dbg_addr_shift;

	uint32_t ut_base;
};

extern struct dapc_config dapc_cfg_mt6885;
extern struct dapc_config dapc_cfg_mt6873;
extern struct dapc_config dapc_cfg_mt6853;
extern struct dapc_config dapc_cfg_mt6893;
extern struct dapc_config dapc_cfg_mt6877;

#define for_each_dapc_slv(cfg, i) \
	for (i = 0; i < cfg->slv_cnt && cfg->slv[i].sys_idx >= 0; i++)

#endif

