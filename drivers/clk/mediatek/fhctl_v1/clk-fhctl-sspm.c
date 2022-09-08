// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */

#include "clk-fhctl.h"
#include "sspm_ipi.h"

#define FHCTL_D_LEN  9
#define MAX_SSC_RATE 8


/* SSPM IPI CMD. Should sync with mt_freqhopping.h in tinysys driver. */
enum FH_DEVCTL_CMD_ID {
	FH_DCTL_CMD_SSC_ENABLE = 0x1004,
	FH_DCTL_CMD_SSC_DISABLE = 0x1005,
	FH_DCTL_CMD_GENERAL_DFS = 0x1006,
	FH_DCTL_CMD_ARM_DFS = 0x1007,
	FH_DCTL_CMD_SSC_TBL_CONFIG = 0x100A,
	FH_DCTL_CMD_PLL_PAUSE = 0x100E,
	FH_DCTL_CMD_MAX
};

struct freqhopping_ioctl {
	unsigned int pll_id;
	struct freqhopping_ssc {
		unsigned int idx_pattern; /* idx_pattern: Deprecated Field */
		unsigned int dt;
		unsigned int df;
		unsigned int upbnd;
		unsigned int lowbnd;
		unsigned int dds; /* dds: Deprecated Field */
	} ssc_setting;    /* used only when user-define */
	int result;
};

struct fhctl_ipi_data {
	unsigned int cmd;
	union {
		struct freqhopping_ioctl fh_ctl;
		unsigned int args[8];
	} u;
};


static int fhctl_to_sspm_command(unsigned int cmd,
				struct fhctl_ipi_data *ipi_data)
{
	int ret = 0;
	unsigned int ack_data = 0;

	pr_debug("send ipi command %x", cmd);

	switch (cmd) {
	case FH_DCTL_CMD_SSC_ENABLE:
	case FH_DCTL_CMD_SSC_DISABLE:
	case FH_DCTL_CMD_GENERAL_DFS:
	case FH_DCTL_CMD_ARM_DFS:
	case FH_DCTL_CMD_SSC_TBL_CONFIG:
	case FH_DCTL_CMD_PLL_PAUSE:
		ipi_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_FHCTL, IPI_OPT_POLLING,
					ipi_data, FHCTL_D_LEN, &ack_data, 1);
		if (ret != 0)
			pr_info("sspm_ipi_send_sync error(%d) ret:%d - %d",
						cmd, ret, ack_data);
		else if (ack_data < 0)
			pr_info("cmd(%d) return error(%d)", cmd, ack_data);
		break;
	default:
		pr_info("[Error]Undefined IPI command");
		break;
	} /* switch */

	pr_debug("send ipi command %x, response: ack_data: %d",
			cmd, ack_data);

	return ack_data;
}

static int clk_mt_fh_sspm_pll_init(struct clk_mt_fhctl *fh)
{
	struct fhctl_ipi_data ipi_data;
	int pll_id;

	pll_id = fh->pll_data->pll_id;

	/* Check default enable SSC */
	if (fh->pll_data->pll_default_ssc_rate > 0) {
		/* Init SSPM g_pll_ssc_setting_tbl table */
		ipi_data.u.fh_ctl.pll_id = pll_id;
		ipi_data.u.fh_ctl.ssc_setting.dt = 0;
		ipi_data.u.fh_ctl.ssc_setting.df = 0;
		ipi_data.u.fh_ctl.ssc_setting.upbnd = 0;
		ipi_data.u.fh_ctl.ssc_setting.lowbnd =
			fh->pll_data->pll_default_ssc_rate;
		fhctl_to_sspm_command(FH_DCTL_CMD_SSC_TBL_CONFIG, &ipi_data);

		pr_debug("Default Enable SSC PLL_ID:%d SSC_RATE:0~-%d",
				pll_id, fh->pll_data->pll_default_ssc_rate);

		/* Default Enable SSC to 0~-N%; */
		fh->hal_ops->pll_ssc_enable(fh,
				fh->pll_data->pll_default_ssc_rate);
	}

	return 0;
}

static int __clk_mt_fh_sspm_pll_pause(struct clk_mt_fhctl *fh, bool pause)
{
	struct fhctl_ipi_data ipi_data;
	int pll_id;

	pll_id = fh->pll_data->pll_id;

	/* Only for support pause in CPU PLL. */
	if (fh->pll_data->pll_type != FH_PLL_TYPE_CPU)
		return -EPERM;

	ipi_data.u.args[0] = pll_id;
	ipi_data.u.args[1] = (pause) ? 1 : 0;
	fhctl_to_sspm_command(FH_DCTL_CMD_PLL_PAUSE, &ipi_data);

	return 0;
}

static int clk_mt_fh_sspm_pll_unpause(struct clk_mt_fhctl *fh)
{
	return __clk_mt_fh_sspm_pll_pause(fh, false);
}

static int clk_mt_fh_sspm_pll_pause(struct clk_mt_fhctl *fh)
{

	return __clk_mt_fh_sspm_pll_pause(fh, true);
}

static int clk_mt_fh_sspm_pll_ssc_disable(struct clk_mt_fhctl *fh)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int pll_id;

	pll_id = fh->pll_data->pll_id;

	fh_ctl.pll_id = pll_id;
	fh_ctl.result = 0;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	fhctl_to_sspm_command(FH_DCTL_CMD_SSC_DISABLE, &ipi_data);

	return 0;
}


static int clk_mt_fh_sspm_pll_ssc_enable(struct clk_mt_fhctl *fh, int ssc_rate)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int pll_id;

	pll_id = fh->pll_data->pll_id;
	fh_ctl.pll_id = pll_id;
	fh_ctl.result = 0;

	if (fh->pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) {
		pr_info("%s not support SSC.", fh->pll_data->pll_name);
		return -EPERM;
	}

	if (ssc_rate > MAX_SSC_RATE) {
		pr_info("[Error] ssc_rate:%d over spec!!!", ssc_rate);
		return -EINVAL;
	}

	fh_ctl.ssc_setting.dt = 0;  /* default setting */
	fh_ctl.ssc_setting.df = 9;  /* default setting */
	fh_ctl.ssc_setting.upbnd = 0;  /* default setting */
	fh_ctl.ssc_setting.lowbnd = ssc_rate;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	fhctl_to_sspm_command(FH_DCTL_CMD_SSC_ENABLE, &ipi_data);

	pr_info("PLL:%d ssc rate change [O]:%d => [N]:%d ",
			pll_id, fh->pll_data->pll_default_ssc_rate, ssc_rate);

	/* Update clock ssc rate variable. */

	fh->pll_data->pll_default_ssc_rate = ssc_rate;

	return 0;
}

static int clk_mt_fh_sspm_pll_hopping(struct clk_mt_fhctl *fh,
					unsigned int new_dds,
					int postdiv)
{
	struct fhctl_ipi_data ipi_data;
	int pll_id, cmd_id;


	pll_id = fh->pll_data->pll_id;

	/* CPU is forbidden hopping in AP side. (clk driver owner reqest) */
	if ((fh->pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) ||
			(fh->pll_data->pll_type == FH_PLL_TYPE_CPU)) {
		pr_info("%s not support hopping in AP side.",
					fh->pll_data->pll_name);
		return 0;
	}

	cmd_id = FH_DCTL_CMD_GENERAL_DFS;

	pr_info("[Hopping] PLL_ID:%d NEW_DDS:0x%x postdiv:%d",
					pll_id, new_dds, postdiv);

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.u.args[0] = pll_id;
	ipi_data.u.args[1] = new_dds;
	ipi_data.u.args[2] = postdiv;

	fhctl_to_sspm_command(cmd_id, &ipi_data);

	return 0;
}

const struct clk_mt_fhctl_hal_ops mt_fhctl_hal_ops = {
	.pll_init = clk_mt_fh_sspm_pll_init,
	.pll_unpause = clk_mt_fh_sspm_pll_unpause,
	.pll_pause = clk_mt_fh_sspm_pll_pause,
	.pll_ssc_disable = clk_mt_fh_sspm_pll_ssc_disable,
	.pll_ssc_enable = clk_mt_fh_sspm_pll_ssc_enable,
	.pll_hopping = clk_mt_fh_sspm_pll_hopping,
};


