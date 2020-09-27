// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "clk-fhctl.h"
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"
#include "mcupm_ipi_id.h"

#define FHCTL_TARGET FHCTL_MCUPM
#define IPI_TIMEOUT_MS 10

struct match {
	char *name;
	struct fh_hdlr *hdlr;
	int (*init)(struct pll_dts *array
			, struct match *match);
};
struct hdlr_data_v1 {
	struct pll_dts *array;
	struct mutex *lock;
	struct fh_pll_domain *domain;
	u8 tr_id;
	void __iomem *reg_tr;
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
enum FH_DEVCTL_CMD_ID {
	FH_DCTL_CMD_SSC_ENABLE = 0x1004,
	FH_DCTL_CMD_SSC_DISABLE = 0x1005,
	FH_DCTL_CMD_GENERAL_DFS = 0x1006,
	FH_DCTL_CMD_ARM_DFS = 0x1007,
	FH_DCTL_CMD_SSC_TBL_CONFIG = 0x100A,
	FH_DCTL_CMD_PLL_PAUSE = 0x100E,
	FH_DCTL_CMD_MAX,
	FH_DBG_CMD_TR_BEGIN_LOW = 0x2001,
	FH_DBG_CMD_TR_BEGIN_HIGH = 0x2002,
	FH_DBG_CMD_TR_END_LOW = 0x2003,
	FH_DBG_CMD_TR_END_HIGH = 0x2004,
};
#define FHCTL_D_LEN (sizeof(struct fhctl_ipi_data)/\
	sizeof(unsigned int))
static unsigned int ack_data;

static void ipi_get_data(unsigned int cmd)
{
	struct fhctl_ipi_data ipi_data;
	int ret;

	FHDBG("cmd<%x>\n", cmd);
	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.cmd = cmd;

	/* 3 sec for debug */
	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, 3000);
	FHDBG("ret<%d>, ack_data<%x>\n",
			ret, ack_data);
}
static void dump_hw(struct fh_pll_regs *regs,
		struct fh_pll_data *data)
{
	FHDBG("hp_en<%x>,clk_con<%x>,slope0<%x>,slope1<%x>\n",
			readl(regs->reg_hp_en), readl(regs->reg_clk_con),
			readl(regs->reg_slope0), readl(regs->reg_slope1));
	FHDBG("cfg<%x>,lmt<%x>,dds<%x>,dvfs<%x>,mon<%x>\n",
			readl(regs->reg_cfg), readl(regs->reg_updnlmt),
			readl(regs->reg_dds), readl(regs->reg_dvfs),
			readl(regs->reg_mon));
	FHDBG("pcw<%x>\n",
			readl(regs->reg_con_pcw));
}
static int mcupm_hopping_v1(void *priv_data, char *domain_name, int fh_id,
		unsigned int new_dds, int postdiv)
{
	int ret;
	struct fhctl_ipi_data ipi_data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	ktime_t ktime;
	u64 time_ns;
	u8 tr_id_local;
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	unsigned int con_pcw;
	bool has_err = false;

	mutex_lock(lock);

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.cmd = FH_DCTL_CMD_GENERAL_DFS;
	ipi_data.u.args[0] = fh_id;
	ipi_data.u.args[1] = new_dds;
	ipi_data.u.args[2] = postdiv;

	ktime = ktime_get();
	time_ns = ktime_to_ns(ktime);
	d->tr_id ^= (((u8)time_ns) | 0x1);
	ipi_data.u.args[7] = d->tr_id;
	tr_id_local = d->tr_id;
	/* make sure tr_id_local is set before send ipi */
	mb();

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, IPI_TIMEOUT_MS);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];
	con_pcw = readl(regs->reg_con_pcw);
	con_pcw &= data->dds_mask;

	if (con_pcw != new_dds)
		has_err |= true;
	if (ret != 0)
		has_err |= true;
	if (ack_data != d->tr_id)
		has_err |= true;

	if (has_err) {
		u32 val;

		FHDBG("---------------------------\n");

		FHDBG("domain<%s>, fh_id<%d>\n",
				domain_name, fh_id);
		FHDBG("ret<%d>, ack<%x>, tr<%x>\n",
				ret, ack_data, d->tr_id);
		FHDBG("con_pcw<%x>, new_dds<%x>\n",
				con_pcw, new_dds);

		/* dump HW */
		dump_hw(regs, data);

		/* tr/time via HW */
		FHDBG("time_ns<%lx>\n", time_ns);
		if (d->reg_tr) {
			val = readl(d->reg_tr);
			FHDBG("reg_tr<%x>\n", val);
		}

		/* time via SW */
		ipi_get_data(FH_DBG_CMD_TR_BEGIN_LOW);
		ipi_get_data(FH_DBG_CMD_TR_BEGIN_HIGH);
		ipi_get_data(FH_DBG_CMD_TR_END_LOW);
		ipi_get_data(FH_DBG_CMD_TR_END_HIGH);

		FHDBG("tr_id_local<%x>\n",
				++tr_id_local);
		FHDBG("---------------------------\n");

		/* notify user that err */
		mb();
		notify_err();

		ret = -1;
	} else
		ret = 0;

	mutex_unlock(lock);

	return ret;
}
static int mcupm_ssc_enable_v1(void *priv_data,
		char *domain_name, int fh_id, int rate)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int ret;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;
	struct fh_pll_data *data = d->domain->data;

	mutex_lock(lock);

	FHDBG("rate<%d>\n", rate);

	fh_ctl.pll_id = fh_id;
	fh_ctl.result = 0;
	fh_ctl.ssc_setting.dt = data->dt_val;
	fh_ctl.ssc_setting.df = data->df_val;
	fh_ctl.ssc_setting.upbnd = 0;
	fh_ctl.ssc_setting.lowbnd = rate;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	ipi_data.cmd = FH_DCTL_CMD_SSC_ENABLE;
	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, IPI_TIMEOUT_MS);
	FHDBG("ret<%d>\n", ret);

	array->ssc_rate = rate;

	mutex_unlock(lock);
	return 0;
}
static int mcupm_ssc_disable_v1(void *priv_data,
		char *domain_name, int fh_id)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int ret;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	FHDBG("\n");

	fh_ctl.pll_id = fh_id;
	fh_ctl.result = 0;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	ipi_data.cmd = FH_DCTL_CMD_SSC_DISABLE;
	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, IPI_TIMEOUT_MS);
	FHDBG("ret<%d>\n", ret);

	array->ssc_rate = 0;

	mutex_unlock(lock);
	return 0;
}
static int mcupm_init_v1(struct pll_dts *array, struct match *match)
{
	static bool ipi_inited;
	static DEFINE_MUTEX(lock);
	struct hdlr_data_v1 *priv_data, *match_data;
	struct fh_hdlr *hdlr;

	FHDBG("array<%x>, %s\n",
			array,
			array->pll_name);

	if (!ipi_inited) {
		int ret;

		ret = mtk_ipi_register(&mcupm_ipidev, CH_S_FHCTL, NULL,
				NULL, (void *)&ack_data);
		if (ret) {
			FHDBG("[MCUPM] ipi_register fail, ret %d\n", ret);
			return -1;
		}
		ipi_inited = true;
		FHDBG("\n");
	}

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	hdlr = kzalloc(sizeof(*hdlr), GFP_KERNEL);
	init_fh_domain(array->domain,
			array->comp,
			array->fhctl_base,
			array->apmixed_base);

	priv_data->array = array;
	priv_data->lock = &lock;
	priv_data->domain = get_fh_domain(array->domain);

	match_data = match->hdlr->data;
	if (match_data && match_data->reg_tr) {
		priv_data->reg_tr = array->fhctl_base
			+ (unsigned int)match_data->reg_tr;
	}

	/* hook to array */
	hdlr->data = priv_data;
	hdlr->ops = match->hdlr->ops;
	/* hook hdlr to array is the last step */
	mb();
	array->hdlr = hdlr;

	/* do SSC */
	if (array->ssc_rate) {
		struct fh_hdlr *hdlr;

		hdlr = array->hdlr;
		hdlr->ops->ssc_enable(hdlr->data,
				array->domain,
				array->fh_id,
				array->ssc_rate);
	}

	return 0;
}

static struct fh_operation mcupm_ops_v1 = {
	.hopping = mcupm_hopping_v1,
	.ssc_enable = mcupm_ssc_enable_v1,
	.ssc_disable = mcupm_ssc_disable_v1,
};
struct hdlr_data_v1 hdlr_data_6853 = {
	.reg_tr = (void __iomem *)(0xC8 + 0x4),
};
static struct fh_hdlr mcupm_hdlr_6853 = {
	.ops = &mcupm_ops_v1,
	.data = &hdlr_data_6853,
};
static struct match mt6853_match = {
	.name = "mediatek,mt6853-fhctl",
	.hdlr = &mcupm_hdlr_6853,
	.init = &mcupm_init_v1,
};
static struct match *matches[] = {
	&mt6853_match,
	NULL,
};

int fhctl_mcupm_init(struct platform_device *pdev,
		struct pll_dts *array)
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

		if (strcmp(comp,
					target) == 0) {
			break;
		}
		match++;
	}

	if (*match == NULL) {
		FHDBG("no match!\n");
		return -1;
	}

	/* init flow for every pll */
	for (i = 0; i < num_pll ; i++, array++) {
		char *method = array->method;

		if (strcmp(method,
					FHCTL_TARGET) == 0) {
			(*match)->init(array, *match);
		}
	}

	FHDBG("\n");
	return 0;
}
