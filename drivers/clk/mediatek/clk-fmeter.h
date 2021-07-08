/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __CLK_FMETER_H
#define __CLK_FMETER_H

#define FM_SYS(_id)		((_id & (0xFF00)) >> 8)
#define FM_ID(_id)		(_id & (0xFF))

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN,
	ABIST_2,
	SUBSYS,
	VLPCK,
};

enum FMETER_ID {
	FID_NULL = -1,
	FID_DISP_PWM = 0,
	FID_ULPOSC1,
	FID_ULPOSC2,
	FID_NUM,
};

struct fmeter_clk {
	enum FMETER_TYPE type;
	u32 id;
	const char *name;
	u32 ofs;
	u32 pdn;
	u32 grp;
	u32 ck_div;
};

struct fm_pwr_sta {
	unsigned int ofs;
	unsigned int msk;
};

struct fm_subsys {
	unsigned int id;
	const char *name;
	void __iomem *base;
	unsigned int con0;
	unsigned int con1;
	struct fm_pwr_sta pwr_sta;
};

struct fmeter_ops {
	const struct fmeter_clk *(*get_fmeter_clks)(void);
	unsigned int (*get_ckgen_freq)(unsigned int id);
	unsigned int (*get_abist_freq)(unsigned int id);
	unsigned int (*get_abist2_freq)(unsigned int id);
	unsigned int (*get_vlpck_freq)(unsigned int id);
	unsigned int (*get_subsys_freq)(unsigned int id);
	unsigned int (*get_fmeter_freq)(unsigned int ids, enum  FMETER_TYPE type);
	int (*get_fmeter_id)(enum FMETER_ID fid);
	int (*subsys_freq_register)(struct fm_subsys *fm, unsigned int size);
};

const struct fmeter_clk *mt_get_fmeter_clks(void);
unsigned int mt_get_ckgen_freq(unsigned int id);
unsigned int mt_get_abist_freq(unsigned int id);
unsigned int mt_get_abist2_freq(unsigned int id);
unsigned int mt_get_vlpck_freq(unsigned int id);
unsigned int mt_get_subsys_freq(unsigned int id);
int mt_get_fmeter_id(enum FMETER_ID fid);
unsigned int mt_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type);
int mt_subsys_freq_register(struct fm_subsys *fm, unsigned int size);
void fmeter_set_ops(const struct fmeter_ops *ops);
#endif
