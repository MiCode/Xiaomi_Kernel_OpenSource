// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */

#include <linux/slab.h>
#include "clk-fhctl-pll.h"
#include "clk-fhctl.h"
#include "sspm_ipi.h"
#include "clk-fhctl-util.h"

#define FHCTL_D_LEN  9
#define MAX_SSC_RATE 8
#define FHCTL_TARGET FHCTL_SSPM

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

struct match {
	char *name;
	struct fh_hdlr *hdlr;
	int (*init)(struct pll_dts *array, struct match *match);
};
struct hdlr_data_v1 {
	struct pll_dts *array;
	spinlock_t *lock;
	struct fh_pll_domain *domain;
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

static int sspm_init_v1(struct pll_dts *array, struct match *match)
{
	static DEFINE_SPINLOCK(lock);
	struct hdlr_data_v1 *priv_data;
	struct fhctl_ipi_data ipi_data;
	int pll_id;
	struct fh_hdlr *hdlr;

	pll_id = array->fh_id;

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	hdlr = kzalloc(sizeof(*hdlr), GFP_KERNEL);
	init_fh_domain(array->domain,
			array->comp,
			array->fhctl_base,
			array->apmixed_base);

	priv_data->array = array;
	priv_data->lock = &lock;
	priv_data->domain = get_fh_domain(array->domain);

	/* Check default enable SSC */
	if (array->ssc_rate > 0) {
		/* Init SSPM g_pll_ssc_setting_tbl table */
		ipi_data.u.fh_ctl.pll_id = pll_id;
		ipi_data.u.fh_ctl.ssc_setting.dt = 0;
		ipi_data.u.fh_ctl.ssc_setting.df = 0;
		ipi_data.u.fh_ctl.ssc_setting.upbnd = 0;
		ipi_data.u.fh_ctl.ssc_setting.lowbnd =
			array->ssc_rate;
		fhctl_to_sspm_command(FH_DCTL_CMD_SSC_TBL_CONFIG, &ipi_data);

		pr_debug("Default Enable SSC PLL_ID:%d SSC_RATE:0~-%d",
				pll_id, array->ssc_rate);
	}

	/* hook to array */
	hdlr->data = priv_data;
	hdlr->ops = match->hdlr->ops;
	/* hook hdlr to array is the last step */
	mb();
	array->hdlr = hdlr;

	if (array->ssc_rate) {
		/* Default Enable SSC to 0~-N%; */
		hdlr = array->hdlr;
		hdlr->ops->ssc_enable(hdlr->data,
				array->domain,
				array->fh_id,
				array->ssc_rate);
	}

	return 0;
}

static int sspm_ssc_disable_v1(void *priv_data,
	char *domain_name, int fh_id)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int pll_id;

	pll_id = fh_id;

	fh_ctl.pll_id = pll_id;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	fhctl_to_sspm_command(FH_DCTL_CMD_SSC_DISABLE, &ipi_data);

	return 0;
}


static int sspm_ssc_enable_v1(void *priv_data,
		char *domain_name, int fh_id, int rate)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int pll_id;
	int ssc_rate;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct pll_dts *array = d->array;

	pll_id = fh_id;
	fh_ctl.pll_id = pll_id;
	ssc_rate = rate;

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
			pll_id, array->ssc_rate, ssc_rate);

	/* Update clock ssc rate variable. */

	array->ssc_rate = ssc_rate;

	return 0;
}

static int sspm_hopping_v1(void *priv_data, char *domain_name, int fh_id,
		unsigned int new_dds, int postdiv)
{
	struct fhctl_ipi_data ipi_data;
	int pll_id, cmd_id;


	pll_id = fh_id;

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

static struct fh_operation sspm_ops_v1 = {
	.hopping = sspm_hopping_v1,
	.ssc_enable = sspm_ssc_enable_v1,
	.ssc_disable = sspm_ssc_disable_v1,
};
static struct fh_hdlr sspm_hdlr_v1 = {
	.ops = &sspm_ops_v1,
};

static struct match mt6765_match = {
	.name = "mediatek,mt6765-fhctl",
	.hdlr = &sspm_hdlr_v1,
	.init = &sspm_init_v1,
};

static struct match mt6761_match = {
	.name = "mediatek,mt6761-fhctl",
	.hdlr = &sspm_hdlr_v1,
	.init = &sspm_init_v1,
};

static struct match mt6768_match = {
	.name = "mediatek,mt6768-fhctl",
	.hdlr = &sspm_hdlr_v1,
	.init = &sspm_init_v1,
};

static struct match *matches[] = {
	&mt6765_match,
	&mt6761_match,
	&mt6768_match,
	NULL,
};

int fhctl_sspm_init(struct pll_dts *array)
{
	int i;
	int num_pll;
	struct match **match;

	FHDBG("\n");
	match = matches;
	num_pll = array->num_pll;

	/* find match by compatible */
	while (*match != NULL) {
		char *comp = (*match)->name;
		char *target = array->comp;

		if (strcmp(comp, target) == 0)
			break;

	match++;
	}

	if (*match == NULL) {
		FHDBG("no match!\n");
		return -1;
	}

	/* init flow for every pll */
	for (i = 0; i < num_pll ; i++, array++) {
		char *method = array->method;

		if (strcmp(method, FHCTL_TARGET) == 0) {
			FHDBG("FHCTL SSPM\n");
			(*match)->init(array, *match);
		}
	}

	FHDBG("\n");
	return 0;
}

