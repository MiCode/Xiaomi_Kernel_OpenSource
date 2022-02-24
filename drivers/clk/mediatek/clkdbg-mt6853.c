// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "clkchk-mt6853.h"
#include "clk-fmeter.h"

#define DUMP_INIT_STATE		0
#define CHECK_VCORE_FREQ		1

int __attribute__((weak)) get_sw_req_vcore_opp(void)
{
	return -1;
}

static const struct regname *get_all_regnames(void)
{
	return get_mt6853_all_reg_names();
}

static void __init init_regbase(void)
{
	struct regbase *rb = get_mt6853_all_reg_bases();

	for (; rb->name; rb++) {
		if (!rb->phys)
			continue;

		rb->virt = ioremap_nocache(rb->phys, 0x1000);
	}
}

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
	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	return get_mt6853_all_clk_names();
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
		[6] = "(Reserved)",
		[7] = "MFG5",
		[8] = "(Reserved)",
		[9] = "(Reserved)",
		[10] = "(Reserved)",
		[11] = "(Reserved)",
		[12] = "ISP",
		[13] = "ISP2",
		[14] = "IPE",
		[15] = "VDEC",
		[16] = "(Reserved)",
		[17] = "VEN",
		[18] = "(Reserved)",
		[19] = "(Reserved)",
		[20] = "DISP",
		[21] = "AUDIO",
		[22] = "ADSP",
		[23] = "CAM",
		[24] = "CAM_RAWA",
		[25] = "CAM_RAWB",
		[26] = "(Reserved)",
		[27] = "(Reserved)",
		[28] = "(Reserved)",
		[29] = "(Reserved)",
		[30] = "(Reserved)",
		[31] = "(Reserved)",
		[32] = "VPU",
	};

	return pwr_names;
}

/*
 * clkdbg dump_clks
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
 * chip_ver functions
 */

static int clkdbg_chip_ver(struct seq_file *s, void *v)
{
	static const char * const sw_ver_name[] = {
		"CHIP_SW_VER_01",
		"CHIP_SW_VER_02",
		"CHIP_SW_VER_03",
		"CHIP_SW_VER_04",
	};

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", 0, sw_ver_name[0]);

	return 0;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6853_ops = {
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
		CMDFN("chip_ver", clkdbg_chip_ver),
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt6853_init(void)
{
	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6853_ops);

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
subsys_initcall(clkdbg_mt6853_init);
