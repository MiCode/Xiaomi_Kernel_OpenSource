/*
 * Copyright (C) 2021 MediaTek Inc.
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>

#ifdef CONFIG_MTK_DEVAPC
#include <mt-plat/devapc_public.h>
#endif
#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clkchk-mt6877.h"
#include "clk-fmeter.h"

static const struct regname *get_all_regnames(void)
{
	return get_mt6877_all_reg_names();
}

/*
 * clkdbg dump all fmeter clks
 */
static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type == ABIST)
		return mt_get_abist_freq(fclk->id);
	else if (fclk->type == ABIST_2)
		return mt_get_abist2_freq(fclk->id);
	else if (fclk->type == CKGEN)
		return mt_get_ckgen_freq(fclk->id);
	else if (fclk->type == SUBSYS)
		return mt_get_subsys_freq(fclk->id);
	return 0;
}

/*
 * clkdbg dump_clks
 */

static const char * const *get_all_clk_names(void)
{
	return get_mt6877_all_clk_names();
}

/*
 * clkchk pwr_state
 */
struct pvd_msk {
	const char *pvdname;
	enum PWR_STA_TYPE sta_type;
	u32 pwr_mask;
};

static struct pvd_msk pvd_pwr_mask[] = {
	{"adspsys", PWR_STA, 0x00400000},
	{"audiosys", PWR_STA, 0x00200000},
	{"gpu_pll_ctrl", XPU_PWR_STA, 0x0000003F},
	{"mfgcfg", XPU_PWR_STA, 0x0000003F},
	{"dispsys", PWR_STA, 0x00040000},
	{"imgsys1", PWR_STA, 0x00000200},
	{"imgsys2", PWR_STA, 0x00000400},
	{"vdecsys", PWR_STA, 0x00001000},
	{"vencsys", PWR_STA, 0x00004000},
	{"apu_conn2", OTHER_PWR_STA, 0x00000020},
	{"apu_conn1", OTHER_PWR_STA, 0x00000020},
	{"apu_vcore", OTHER_PWR_STA, 0x00000020},
	{"apu0", OTHER_PWR_STA, 0x00000020},
	{"apu1", OTHER_PWR_STA, 0x00000020},
	{"apu_mdla0", OTHER_PWR_STA, 0x00000020},
	{"camsys_main", PWR_STA, 0x00800000},
	{"camsys_rawa", PWR_STA, 0x01000000},
	{"camsys_rawb", PWR_STA, 0x02000000},
	{"ipesys", PWR_STA, 0x00000800},
	{"mdpsys", PWR_STA, 0x00040000},
	{},
};

/*
 * clkdbg setup clks
 */

void setup_provider_clk(struct provider_clk *pvdck)
{
	int i;
	const char *pvdname = pvdck->provider_name;

	if (!pvdname)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask) - 1; i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			pvdck->sta_type = pvd_pwr_mask[i].sta_type;
			return;
		}
	}

	pvdck->pwr_mask = 0;
	pvdck->sta_type = PWR_STA;
}

/*
 * clkdbg pwr_status
 */
static u32 pwr_ofs[STA_NUM] = {
	[XPU_PWR_STA] = 0xEF8,
	[XPU_PWR_STA2] = 0xEFC,
	[PWR_STA] = 0xEF0,
	[PWR_STA2] = 0xEF4,
	[OTHER_PWR_STA] = 0x178,
};

static u32 pwr_sta[STA_NUM];

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *scpsys_base, *pwr_addr[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (!scpsys_base)
			scpsys_base = ioremap(0x10006000, PAGE_SIZE);

		if (pwr_ofs[i]) {
			pwr_addr[i] = scpsys_base + pwr_ofs[i];
			pwr_sta[i] = clk_readl(pwr_addr[i]);
		}
	}

	return pwr_sta;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6877_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = NULL,
	.setup_provider_clk = setup_provider_clk,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt6877_init(void)
{
	if (!of_machine_is_compatible("mediatek,MT6877"))
		return -ENODEV;

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6877_ops);

	return 0;
}
subsys_initcall(clkdbg_mt6877_init);
