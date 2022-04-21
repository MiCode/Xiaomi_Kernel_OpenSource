// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTK_DEVAPC
#include <mt-plat/devapc_public.h>
#endif

#include "clkdbg.h"
#include "clkchk.h"
#include "clkchk-mt6885.h"
#include "clk-fmeter.h"
#include <clk-mux.h>

static const struct regname *get_all_regnames(void)
{
	return get_mt6893_all_reg_names();
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
	else if (fclk->type == CKGEN)
		return mt_get_ckgen_freq(fclk->id);
	return 0;
}

/*
 * clkdbg dump_clks
 */

static const char * const *get_all_clk_names(void)
{
	return get_mt6893_all_clk_names();
}

/*
 * clkdbg pwr_status
 */
static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0] = "MD",
		[1] = "CONN",
		[2] = "MFG0",
		[3] = "MFG1",
		[4] = "MFG2",
		[5] = "MFG3",
		[6] = "MFG4",
		[7] = "MFG5",
		[8] = "MFG6",
		[9] = "INFRA",
		[10] = "SUB_INFRA",
		[11] = "DDRPHY",
		[12] = "ISP",
		[13] = "ISP2",
		[14] = "IPE",
		[15] = "VDEC",
		[16] = "VDEC2",
		[17] = "VEN",
		[18] = "VEN_CORE1",
		[19] = "MDP",
		[20] = "DISP",
		[21] = "AUDIO",
		[22] = "ADSP",
		[23] = "CAM",
		[24] = "CAM_RAWA",
		[25] = "CAM_RAWB",
		[26] = "CAM_RAWC",
		[27] = "DP_TX",
		[28] = "DDRPHY2",
		[29] = "(Reserved)",
		[30] = "(Reserved)",
		[31] = "(Reserved)",
	};

	return pwr_names;
}

/*
 * clkdbg setup clks
 */

void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
	};

	int i;
	const char *pvdname = pvdck->provider_name;

	if (!pvdname)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * pwr stat check functions
 */
static  bool is_pwr_on(struct provider_clk *pvdck)
{
	struct clk *c = pvdck->ck;
	struct clk_hw *c_hw = __clk_get_hw(c);

	return clk_hw_is_prepared(c_hw);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6893_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
	.is_pwr_on = is_pwr_on,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt6893_init(void)
{
	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6893_ops);

	return 0;
}
subsys_initcall(clkdbg_mt6893_init);
