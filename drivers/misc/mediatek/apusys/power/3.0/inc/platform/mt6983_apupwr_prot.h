/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6983_APUPWR_PROT_H__
#define __MT6983_APUPWR_PROT_H__

#include "apu_top.h"
#include "mt6983_apupwr.h"

#define ACX0_LIMIT_OPP_REG      SPARE0_MBOX_DUMMY_0_ADDR
#define ACX1_LIMIT_OPP_REG      SPARE0_MBOX_DUMMY_1_ADDR
#define DEV_OPP_SYNC_REG        SPARE0_MBOX_DUMMY_2_ADDR
#define HW_RES_SYNC_REG         SPARE0_MBOX_DUMMY_3_ADDR
#define PLAT_CFG_SYNC_REG	SPARE0_MBOX_DUMMY_4_ADDR

enum {
	APUPWR_DBG_DEV_CTL = 0,
	APUPWR_DBG_DEV_SET_OPP,
	APUPWR_DBG_DVFS_DEBUG,
	APUPWR_DBG_DUMP_OPP_TBL,
	APUPWR_DBG_CURR_STATUS,
};

enum apu_opp_limit_type {
	OPP_LIMIT_THERMAL = 0,	// limit by power API
	OPP_LIMIT_HAL,		// limit by i/o ctl
	OPP_LIMIT_DEBUG,	// limit by i/o ctl
};

struct plat_cfg_data {
	int8_t aging_flag:4,
	       hw_id:4;
};

struct device_opp_limit {
	int8_t vpu_max:4,
	       vpu_min:4;
	int8_t dla_max:4,
	       dla_min:4;
	int8_t lmt_type; // limit reason
};

struct cluster_dev_opp_info {
	uint32_t opp_lmt_reg;
	struct device_opp_limit dev_opp_lmt;
};

struct hw_resource_status {
	int32_t vapu_opp:4,
		vsram_opp:4,
		vcore_opp:4,
		fconn_opp:4,
		fvpu_opp:4,
		fdla_opp:4,
		fup_opp:4,
		reserved:4;
};

/*
 * due to this struct will be used to do data exchange through rpmsg
 * so the struct size can't over than 256 bytes
 * 4 bytes * 14 struct members = 56 bytes
 */
struct apu_pwr_curr_info {
	int buck_volt[BUCK_NUM];
	int buck_opp[BUCK_NUM];
	int pll_freq[PLL_NUM];
	int pll_opp[PLL_NUM];
};

/*
 * for satisfy size limitation of rpmsg data exchange is 256 bytes
 * we only put necessary information for opp table here
 * opp entries : 4 bytes * 5 struct members * 10 opp entries = 200 bytes
 * tbl_size : 4 bytes
 * total : 200 + 4 = 204 bytes
 */
struct tiny_dvfs_opp_entry {
	int vapu;       // = volt_bin - volt_age + volt_avs
	int pll_freq[PLL_NUM];
};

struct tiny_dvfs_opp_tbl {
	int tbl_size;   // entry number
	struct tiny_dvfs_opp_entry opp[USER_MIN_OPP_VAL + 1];   // entry data
};

void aputop_opp_limit(struct aputop_func_param *aputop,
		enum apu_opp_limit_type type);

#if IS_ENABLED(CONFIG_DEBUG_FS)
int mt6983_apu_top_dbg_open(struct inode *inode, struct file *file);
ssize_t mt6983_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos);
#endif

int init_remote_data_sync(void __iomem *reg_base);
int chip_data_remote_sync(struct plat_cfg_data *plat_cfg);
int mt6983_apu_top_rpmsg_cb(int cmd, void *data, int len, void *priv, u32 src);

#endif
