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

#define FHCTL_TARGET FHCTL_AP

#define PERCENT_TO_DDSLMT(dds, percent_m10) \
	((((dds) * (percent_m10)) >> 5) / 100)

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
};
static struct pll_dts *_array;
static void set_dts_array(struct pll_dts *array) {_array = array; }
static struct pll_dts *get_dts_array(void) {return _array; }

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

#define FH_ID_MEM_6739 4
#define FH_ID_MM_6739 5
static void __iomem *g_spm_base_6739;
static void __iomem *g_ddrphy_base_6739;
/* 0~8% of 0x1026E8 */
static const int mempll_ssc_pmap1066[9] = {0,
	0x14A, 0x295, 0x3E0, 0x52B, 0x675, 0x7C0, 0x90B, 0xA56};
/* 0~8% of 0x1435E7*/
static const int mempll_ssc_pmap1344[9] = {0,
	0x19D, 0x33B, 0x4D9, 0x677, 0x815, 0x9B3, 0xB51, 0xCEF};
/* 0~8% of 0xDD89D*/
static const int mmpll_ssc_pmap180[9] = {0,
	0x11B, 0x237, 0x352, 0x46E, 0x589, 0x6A5, 0x7C0, 0x8DC};
/* 0~8% of 0x1713B1*/
static const int mmpll_ssc_pmap300[9] = {0,
	0x1D8, 0x3B1, 0x589, 0x762, 0x93B, 0xB13, 0xCEC, 0xEC4};
static int get_hw_sem_6739(void)
{
	unsigned int i;
	void __iomem *ap_sema_reg;

	ap_sema_reg = g_spm_base_6739 + 0x428;

	for (i = 0; i < 400; i++) {
		writel(0x1, ap_sema_reg);
		if (readl(ap_sema_reg) & 0x1)
			return 0;

		udelay(10);
	}
	FHDBG("ap_sema_reg<%x>, i<%d>\n",
			ap_sema_reg, i);
	return -1;
}
static void release_hw_sem_6739(void)
{
	void __iomem *ap_sema_reg;

	ap_sema_reg = g_spm_base_6739 + 0x428;
	if (readl(ap_sema_reg) & 0x1)
		writel(0x1, ap_sema_reg);
}
static int __ssc_6739(struct fh_pll_regs *regs,
		struct fh_pll_data *data,
		int fh_id, int rate)
{
	unsigned int updnlmt_val;

	if (rate > 0) {
		fh_set_field(regs->reg_cfg, data->frddsx_en, 0);
		fh_set_field(regs->reg_cfg, data->sfstrx_en, 0);
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 0);

		/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
		fh_set_field(regs->reg_cfg, data->msk_frddsx_dys,
				data->df_val);
		fh_set_field(regs->reg_cfg, data->msk_frddsx_dts,
				data->dt_val);

		writel((readl(regs->reg_con_pcw) & data->dds_mask) |
				data->tgl_org, regs->reg_dds);
		/* Calculate UPDNLMT */
		updnlmt_val = PERCENT_TO_DDSLMT((readl(regs->reg_dds) &
					data->dds_mask), rate) << data->updnlmt_shft;

		writel(updnlmt_val, regs->reg_updnlmt);

		/* Switch to FHCTL_CORE controller - Original design */
		fh_set_field(regs->reg_hp_en, BIT(fh_id),
				1);

		if (fh_id == FH_ID_MEM_6739) {
			unsigned int val;

			/* Since SPM cannot do multiplicatio */
			/* we pass DDS lower bound for SSC for 1066MHz and 1344 MHz */

			val = mempll_ssc_pmap1066[rate];
			writel(val, g_spm_base_6739 + 0x4D0);
			val = mempll_ssc_pmap1344[rate];
			writel(val, g_spm_base_6739 + 0x4D4);
		} else if (fh_id == FH_ID_MM_6739) {
			unsigned int val;

			val = mmpll_ssc_pmap180[rate];
			writel(val, g_spm_base_6739 + 0x4D8);
			val = mmpll_ssc_pmap300[rate];
			writel(val, g_spm_base_6739 + 0x4DC);
		}

		/* Enable SSC */
		fh_set_field(regs->reg_cfg, data->frddsx_en, 1);
		/* Enable Hopping control */
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 1);

	} else {
		fh_set_field(regs->reg_cfg, data->frddsx_en, 0);
		fh_set_field(regs->reg_cfg, data->sfstrx_en, 0);
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 0);

		/* Switch to APMIXEDSYS control */
		fh_set_field(regs->reg_hp_en, BIT(fh_id),
				0);

		/* Wait for DDS to be stable */
		udelay(30);
	}

	return 0;
}
static int ap_hopping_6739(void *priv_data, char *domain_name, int fh_id,
		unsigned int new_dds, int postdiv)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	unsigned int dds_mask;
	unsigned int mon_dds = 0;
	int ret = 0;
	unsigned int con_pcw_tmp;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];
	dds_mask = data->dds_mask;

	if (array->ssc_rate)
		__ssc_6739(regs, data, fh_id, 0);

	/* 1. sync ncpo to DDS of FHCTL */
	if (fh_id == FH_ID_MEM_6739)
		writel((((readl(regs->reg_con_pcw) & 0xFFFFFFFE) >> 11) & dds_mask) |
				data->tgl_org,
				regs->reg_dds);
	else
		writel((readl(regs->reg_con_pcw) & dds_mask) |
				data->tgl_org, regs->reg_dds);

	/* 2. enable DVFS and Hopping control */

	/* enable dvfs mode */
	fh_set_field(regs->reg_cfg, data->sfstrx_en, 1);
	/* enable hopping */
	fh_set_field(regs->reg_cfg, data->fhctlx_en, 1);

	/* for slope setting. */
	writel(data->slope0_value, regs->reg_slope0);
	/* SLOPE1 is for MEMPLL */
	if (fh_id == FH_ID_MEM_6739)
		writel(data->slope1_value, regs->reg_slope1);

	get_hw_sem_6739();
	/* make sure hw_sem done */
	mb();
	/* 3. switch to hopping control */
	fh_set_field(regs->reg_hp_en, (0x1U << fh_id),
						1);
	release_hw_sem_6739();
	/* make sure hw_sem done */
	mb();

	/* 4. set DFS DDS */
	writel((new_dds) | (data->dvfs_tri), regs->reg_dvfs);

	/* 4.1 ensure jump to target DDS */
	/* Wait 1000 us until DDS stable */
	ret = readl_poll_timeout_atomic(regs->reg_mon, mon_dds,
			(mon_dds & dds_mask) == new_dds, 10, 1000);
	if (ret) {
		/* dump HW */
		dump_hw(regs, data);

		/* notify user that err */
		mb();
		notify_err();
	}

	if (fh_id == FH_ID_MEM_6739) {
		unsigned int tmp;

		tmp = readl(regs->reg_con_pcw);
		con_pcw_tmp  = ((readl(regs->reg_dds) & dds_mask)
				<< 11) & 0xFFFFF800;
		if (!(tmp & 1))
			con_pcw_tmp |= 0x1;
	} else {
		/* Don't change DIV for fhctl UT */
		con_pcw_tmp = readl(regs->reg_con_pcw) & (~dds_mask);
		con_pcw_tmp = (con_pcw_tmp |
				(readl(regs->reg_mon) & dds_mask) |
				data->pcwchg);
	}
	/* 5. write back to ncpo */
	writel(con_pcw_tmp, regs->reg_con_pcw);

	get_hw_sem_6739();
	/* make sure hw_sem done */
	mb();
	/* 6. switch to APMIXEDSYS control */
	fh_set_field(regs->reg_hp_en, BIT(fh_id),
				0);
	/* make sure hw_sem done */
	mb();
	release_hw_sem_6739();

	if (array->ssc_rate)
		__ssc_6739(regs, data, fh_id, array->ssc_rate);

	mutex_unlock(lock);
	return ret;
}
static int ap_ssc_enable_6739(void *priv_data, char *domain_name, int fh_id,
		int rate)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	FHDBG("rate<%d>\n", rate);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	__ssc_6739(regs, data, fh_id, rate);

	array->ssc_rate = rate;

	mutex_unlock(lock);

	return 0;
}
static int ap_ssc_disable_6739(void *priv_data, char *domain_name, int fh_id)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	FHDBG("\n");

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	__ssc_6739(regs, data, fh_id, 0);

	array->ssc_rate = 0;

	mutex_unlock(lock);

	return 0;
}
struct ssc_work {
	struct delayed_work dwork;
	struct pll_dts *array;
};
static void do_ssc_work(struct work_struct *data)
{
	struct ssc_work *work =
		container_of(data, struct ssc_work, dwork.work);
	struct pll_dts *array = work->array;
	struct fh_hdlr *hdlr = array->hdlr;
	struct pll_dts *dts_array = get_dts_array();
	int i;

	FHDBG_LIMIT(1, "<%s>, rate<%d>",
			array->pll_name,
			array->ssc_rate);

	for (i = 0; i < dts_array->num_pll; i++) {
		if (!(dts_array->hdlr)) {
			schedule_delayed_work(&work->dwork,
					0);
			return;
		}
	}

	if (array->ssc_rate) {
		hdlr->ops->ssc_enable(hdlr->data,
				array->domain,
				array->fh_id,
				array->ssc_rate);
	}

	/* make sure free work is the last step */
	mb();
	kfree(work);
}
void issue_ssc_work(struct pll_dts *array)
{
	struct ssc_work *work;
	int delay_ms = 20000;

	work = kzalloc(sizeof(struct ssc_work), GFP_ATOMIC);
	if (!work) {
		FHDBG("!work\n");
		return;
	}

	INIT_DELAYED_WORK(&work->dwork, do_ssc_work);
	work->array = array;
	schedule_delayed_work(&work->dwork,
			msecs_to_jiffies(delay_ms));
}
static int ap_init_6739(struct pll_dts *array, struct match *match)
{
	static DEFINE_MUTEX(lock);
	struct hdlr_data_v1 *priv_data;
	struct fh_hdlr *hdlr;
	struct fh_pll_domain *domain;
	int fh_id = array->fh_id;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	int mask = BIT(fh_id);

	FHDBG("array<%x>,%s %s\n",
			array,
			array->pll_name,
			array->domain);

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	hdlr = kzalloc(sizeof(*hdlr), GFP_KERNEL);
	init_fh_domain(array->domain,
			array->comp,
			array->fhctl_base,
			array->apmixed_base);

	priv_data->array = array;
	priv_data->lock = &lock;
	priv_data->domain = get_fh_domain(array->domain);

	/* do HW init */
	domain = priv_data->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];
	fh_set_field(regs->reg_clk_con, mask, 1);
	fh_set_field(regs->reg_rst_con, mask, 0);
	fh_set_field(regs->reg_rst_con, mask, 1);
	writel(0x0, regs->reg_cfg);
	writel(0x0, regs->reg_updnlmt);
	writel(0x0, regs->reg_dds);

	/* deal with reg_con_pcw for mempll */
	if (array->fh_id == FH_ID_MEM_6739) {
		struct device_node *node;

		node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
		g_spm_base_6739 = of_iomap(node, 0);
		node = of_find_compatible_node(NULL, NULL, "mediatek,ddrphy");
		g_ddrphy_base_6739 = of_iomap(node, 0);

		regs->reg_con_pcw = g_ddrphy_base_6739 + 0x624;
	}

	/* hook to array */
	hdlr->data = priv_data;
	hdlr->ops = match->hdlr->ops;
	/* hook hdlr to array is the last step */
	mb();
	array->hdlr = hdlr;

	/* do SSC */
	issue_ssc_work(array);

	return 0;
}

static int __ssc_v1(struct fh_pll_regs *regs,
		struct fh_pll_data *data,
		int fh_id, int rate)
{
	unsigned int updnlmt_val;

	if (rate > 0) {
		fh_set_field(regs->reg_cfg, data->frddsx_en, 0);
		fh_set_field(regs->reg_cfg, data->sfstrx_en, 0);
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 0);

		/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
		fh_set_field(regs->reg_cfg, data->msk_frddsx_dys,
				data->df_val);
		fh_set_field(regs->reg_cfg, data->msk_frddsx_dts,
				data->dt_val);

		writel((readl(regs->reg_con_pcw) & data->dds_mask) |
				data->tgl_org, regs->reg_dds);
		/* Calculate UPDNLMT */
		updnlmt_val = PERCENT_TO_DDSLMT((readl(regs->reg_dds) &
					data->dds_mask), rate) << data->updnlmt_shft;

		writel(updnlmt_val, regs->reg_updnlmt);

		/* Switch to FHCTL_CORE controller - Original design */
		fh_set_field(regs->reg_hp_en, BIT(fh_id),
				1);

		/* Enable SSC */
		fh_set_field(regs->reg_cfg, data->frddsx_en, 1);
		/* Enable Hopping control */
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 1);

	} else {
		fh_set_field(regs->reg_cfg, data->frddsx_en, 0);
		fh_set_field(regs->reg_cfg, data->sfstrx_en, 0);
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 0);

		/* Switch to APMIXEDSYS control */
		fh_set_field(regs->reg_hp_en, BIT(fh_id),
				0);

		/* Wait for DDS to be stable */
		udelay(30);
	}

	return 0;
}
static int ap_hopping_v1(void *priv_data, char *domain_name, int fh_id,
		unsigned int new_dds, int postdiv)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	unsigned int dds_mask;
	unsigned int mon_dds = 0;
	int ret = 0;
	unsigned int con_pcw_tmp;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];
	dds_mask = data->dds_mask;

	if (array->ssc_rate)
		__ssc_v1(regs, data, fh_id, 0);

	/* 1. sync ncpo to DDS of FHCTL */
	writel((readl(regs->reg_con_pcw) & dds_mask) |
			data->tgl_org, regs->reg_dds);

	/* 2. enable DVFS and Hopping control */

	/* enable dvfs mode */
	fh_set_field(regs->reg_cfg, data->sfstrx_en, 1);
	/* enable hopping */
	fh_set_field(regs->reg_cfg, data->fhctlx_en, 1);

	/* for slope setting. */
	writel(data->slope0_value, regs->reg_slope0);
	/* SLOPE1 is for MEMPLL */
	writel(data->slope1_value, regs->reg_slope1);

	/* 3. switch to hopping control */
	fh_set_field(regs->reg_hp_en, (0x1U << fh_id),
						1);

	/* 4. set DFS DDS */
	writel((new_dds) | (data->dvfs_tri), regs->reg_dvfs);

	/* 4.1 ensure jump to target DDS */
	/* Wait 1000 us until DDS stable */
	ret = readl_poll_timeout_atomic(regs->reg_mon, mon_dds,
			(mon_dds & dds_mask) == new_dds, 10, 1000);
	if (ret) {
		/* dump HW */
		dump_hw(regs, data);

		/* notify user that err */
		mb();
		notify_err();
	}

	/* Don't change DIV for fhctl UT */
	con_pcw_tmp = readl(regs->reg_con_pcw) & (~dds_mask);
	con_pcw_tmp = (con_pcw_tmp |
			(readl(regs->reg_mon) & dds_mask) |
			data->pcwchg);

	/* 5. write back to ncpo */
	writel(con_pcw_tmp, regs->reg_con_pcw);

	/* 6. switch to APMIXEDSYS control */
	fh_set_field(regs->reg_hp_en, BIT(fh_id),
				0);

	if (array->ssc_rate)
		__ssc_v1(regs, data, fh_id, array->ssc_rate);

	mutex_unlock(lock);
	return ret;
}
static int ap_ssc_enable_v1(void *priv_data,
		char *domain_name, int fh_id, int rate)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	FHDBG("rate<%d>\n", rate);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	__ssc_v1(regs, data, fh_id, rate);

	array->ssc_rate = rate;

	mutex_unlock(lock);

	return 0;
}
static int ap_ssc_disable_v1(void *priv_data,
		char *domain_name, int fh_id)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct mutex *lock = d->lock;
	struct pll_dts *array = d->array;

	mutex_lock(lock);

	FHDBG("\n");

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	__ssc_v1(regs, data, fh_id, 0);

	array->ssc_rate = 0;

	mutex_unlock(lock);

	return 0;
}
static int ap_init_v1(struct pll_dts *array, struct match *match)
{
	static DEFINE_MUTEX(lock);
	struct hdlr_data_v1 *priv_data;
	struct fh_hdlr *hdlr;

	FHDBG("array<%x>,%s %s\n",
			array,
			array->pll_name,
			array->domain);

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	hdlr = kzalloc(sizeof(*hdlr), GFP_KERNEL);
	init_fh_domain(array->domain,
			array->comp,
			array->fhctl_base,
			array->apmixed_base);

	priv_data->array = array;
	priv_data->lock = &lock;
	priv_data->domain = get_fh_domain(array->domain);

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

static struct fh_operation ap_ops_v1 = {
	.hopping = ap_hopping_v1,
	.ssc_enable = ap_ssc_enable_v1,
	.ssc_disable = ap_ssc_disable_v1,
};
static struct fh_hdlr ap_hdlr_v1 = {
	.ops = &ap_ops_v1,
};
static struct match mt6853_match = {
	.name = "mediatek,mt6853-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct fh_operation ap_ops_6739 = {
	.hopping = ap_hopping_6739,
	.ssc_enable = ap_ssc_enable_6739,
	.ssc_disable = ap_ssc_disable_6739,
};
static struct fh_hdlr ap_hdlr_6739 = {
	.ops = &ap_ops_6739,
};
static struct match mt6739_match = {
	.name = "mediatek,mt6739-fhctl",
	.hdlr = &ap_hdlr_6739,
	.init = &ap_init_6739,
};
static struct match *matches[] = {
	&mt6853_match,
	&mt6739_match,
	NULL,
};

int fhctl_ap_init(struct platform_device *pdev,
		struct pll_dts *array)
{
	int i;
	int num_pll;
	struct match **match;

	FHDBG("\n");
	match = matches;
	num_pll = array->num_pll;

	set_dts_array(array);

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
