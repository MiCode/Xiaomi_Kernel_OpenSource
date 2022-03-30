/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/platform_device.h>

struct seq_file;

#define CMDFN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

struct cmd_fn {
	const char	*cmd;
	int (*fn)(struct seq_file *s, void *v);
};

struct clkdbg_ops {
	const struct fmeter_clk *(*get_all_fmeter_clks)(void);
	void *(*prepare_fmeter)(void);
	void (*unprepare_fmeter)(void *data);
	u32 (*fmeter_freq)(const struct fmeter_clk *fclk);
	const char * const *(*get_all_clk_names)(void);
	const char * const *(*get_pwr_names)(void);
	u32 (*get_spm_pwr_status)(void);
};

void set_clkdbg_ops(const struct clkdbg_ops *ops);
void clkdbg_set_cfg(void);
int clk_dbg_driver_register(struct platform_driver *drv, const char *name);

extern const struct regname *get_all_regnames(void);
extern struct provider_clk *get_all_provider_clks(void);
