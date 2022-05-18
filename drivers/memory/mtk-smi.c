// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <soc/mediatek/smi.h>
#include <dt-bindings/memory/mt2701-larb-port.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include <../misc/mediatek/include/mt-plat/aee.h>

#include <linux/kthread.h>

/* mt8173 */
#define SMI_LARB_MMU_EN		0xf00

/* mt8167 */
#define MT8167_SMI_LARB_MMU_EN	0xfc0

/* mt2701 */
#define REG_SMI_SECUR_CON_BASE		0x5c0

/* every register control 8 port, register offset 0x4 */
#define REG_SMI_SECUR_CON_OFFSET(id)	(((id) >> 3) << 2)
#define REG_SMI_SECUR_CON_ADDR(id)	\
	(REG_SMI_SECUR_CON_BASE + REG_SMI_SECUR_CON_OFFSET(id))

/*
 * every port have 4 bit to control, bit[port + 3] control virtual or physical,
 * bit[port + 2 : port + 1] control the domain, bit[port] control the security
 * or non-security.
 */
#define SMI_SECUR_CON_VAL_MSK(id)	(~(0xf << (((id) & 0x7) << 2)))
#define SMI_SECUR_CON_VAL_VIRT(id)	BIT((((id) & 0x7) << 2) + 3)
/* mt2701 domain should be set to 3 */
#define SMI_SECUR_CON_VAL_DOMAIN(id)	(0x3 << ((((id) & 0x7) << 2) + 1))

/* mt2712 */
#define SMI_LARB_NONSEC_CON(id)	(0x380 + ((id) * 4))
#define F_MMU_EN		BIT(0)

#define BANK_SEL(id)		({			\
	u32 _id = (id) & 0x3;				\
	(_id << 8 | _id << 10 | _id << 12 | _id << 14);	\
})

/* mt6873 */
#define SMI_LARB_OSTDL_PORT		0x200
#define SMI_LARB_OSTDL_PORTx(id)	(SMI_LARB_OSTDL_PORT + ((id) << 2))

#define SMI_LARB_SLP_CON		0x00c
#define SLP_PROT_EN			BIT(0)
#define SLP_PROT_RDY			BIT(16)
#define SMI_LARB_CMD_THRT_CON		0x24
#define SMI_LARB_SW_FLAG		0x40
#define SMI_LARB_WRR_PORT		0x100
#define SMI_LARB_WRR_PORTx(id)		(SMI_LARB_WRR_PORT + ((id) << 2))
#define SMI_LARB_DISABLE_ULTRA		0x70
#define SMI_LARB_FORCE_ULTRA		0x78

/* mt6893 */
#define SMI_LARB_VC_PRI_MODE		(0x20)
#define SMI_LARB_DBG_CON			(0xf0)
#define INT_SMI_LARB_DBG_CON		(0x500 + (SMI_LARB_DBG_CON))
#define INT_SMI_LARB_CMD_THRT_CON	(0x500 + (SMI_LARB_CMD_THRT_CON))
#define SMI_LARB_OSTD_MON_PORT(p)	(0x280 + ((p) << 2))
#define INT_SMI_LARB_OSTD_MON_PORT(p)	(0x500 + SMI_LARB_OSTD_MON_PORT(p))
#define INT_SMI_LARB_OSTDL_PORTx(id)	(0x500 + SMI_LARB_OSTDL_PORT + ((id) << 2))
#define SMI_LARB_STAT			(0x0)
/* SMI COMMON */
#define SMI_BUS_SEL			0x220
#define SMI_BUS_LARB_SHIFT(larbid)	((larbid) << 1)
#define SMI_CLAMP_EN			(0x3C0)
#define SMI_CLAMP_EN_SET		(0x3C4)
#define SMI_CLAMP_EN_CLR		(0x3C8)
#define SMI_DEBUG_MISC			(0x440)
#define SMI_DEBUG_S(s)		(0x400 + ((s) << 2))
/* All are MMU0 defaultly. Only specialize mmu1 here. */
#define F_MMU1_LARB(larbid)		(0x1 << SMI_BUS_LARB_SHIFT(larbid))
#define SMI_L1LEN			0x100
#define SMI_L1ARB0			0x104
#define SMI_L1ARB(id)			(SMI_L1ARB0 + ((id) << 2))
#define SMI_M4U_TH			0x234
#define SMI_FIFO_TH1			0x238
#define SMI_FIFO_TH2			0x23c
#define SMI_PREULTRA_MASK1	0x244
#define SMI_DCM				0x300
#define SMI_DUMMY			0x444
#define SMI_LARB_PORT_NR_MAX		32
#define SMI_COMMON_LARB_NR_MAX		8
#define MTK_COMMON_NR_MAX	20
#define SMI_LARB_MISC_NR		5
#define SMI_COMMON_MISC_NR		8
struct mtk_smi_reg_pair {
	u16	offset;
	u32	value;
};


#define SMI_L1LEN			0x100
#define SMI_L1ARB0			0x104
#define SMI_L1ARB(id)			(SMI_L1ARB0 + ((id) << 2))

enum mtk_smi_gen {
	MTK_SMI_GEN1,
	MTK_SMI_GEN2,
	MTK_SMI_GEN3
};

struct mtk_smi_common_plat {
	enum mtk_smi_gen gen;
	bool             has_gals;
	u32              bus_sel; /* Balance some larbs to enter mmu0 or mmu1 */
	bool		has_bwl;
	u16		*bwl;
	struct mtk_smi_reg_pair *misc;
};

struct mtk_smi_larb_gen {
	int port_in_larb[MTK_LARB_NR_MAX + 1];
	int port_in_larb_gen2[MTK_LARB_NR_MAX + 1];
	void (*config_port)(struct device *);
	void (*sleep_ctrl)(struct device *dev, bool toslp);
	unsigned int			larb_direct_to_common_mask;
	bool				has_gals;
	bool		has_bwl;
	bool		has_grouping;
	bool		has_bw_thrt;
	u8		*bwl;
	u8		*cmd_group; /* ovl with large ostd */
	u8		*bw_thrt_en; /* non-HRT */
	struct mtk_smi_reg_pair *misc;
};

struct mtk_smi {
	struct device			*dev;
	struct clk			*clk_apb, *clk_smi;
	struct clk			*clk_gals0, *clk_gals1;
	struct clk			*clk_async; /*only needed by mt2701*/
	union {
		void __iomem		*smi_ao_base; /* only for gen1 */
		void __iomem		*base;	      /* only for gen2 */
	};
	const struct mtk_smi_common_plat *plat;
	int			commid;
	atomic_t		ref_count;
	atomic_t		api_ref_count;
};

#define LARB_MAX_COMMON		(2)
struct mtk_smi_larb { /* larb: local arbiter */
	struct mtk_smi			smi;
	void __iomem			*base;
	struct device			*smi_common_dev[LARB_MAX_COMMON];
	struct device			*smi_common;
	const struct mtk_smi_larb_gen	*larb_gen;
	int				larbid;
	int				comm_port_id[LARB_MAX_COMMON];
	u32				*mmu;

	unsigned char			*bank;
};

#define MAX_COMMON_FOR_CLAMP		(3)
#define MAX_LARB_FOR_CLAMP		(6)
#define RESET_CELL_NUM			(2)
#define MAX_PD_CHECK_DEV_NUM	(2)
#define SMI_OSTD_CNT_MASK	(0x1FFE000)

struct mtk_smi_pd_log {
	u8 power_status;
	ktime_t time;
	u32 clamp_status[MAX_COMMON_FOR_CLAMP];
	u32 reset_status[MAX_LARB_FOR_CLAMP];
};

struct mtk_smi_pd {
	struct device			*dev;
	struct device			*smi_common_dev[MAX_COMMON_FOR_CLAMP];
	u32				set_comm_port_range[MAX_COMMON_FOR_CLAMP];
	struct device			*suspend_check_dev[MAX_PD_CHECK_DEV_NUM];
	u32				suspend_check_port[MAX_PD_CHECK_DEV_NUM];
	u32				power_reset_pa[MAX_LARB_FOR_CLAMP];
	void __iomem			*power_reset_reg[MAX_LARB_FOR_CLAMP];
	u32				power_reset_value[MAX_LARB_FOR_CLAMP];
	struct notifier_block nb;
	bool	is_main;
	bool	suspend_check;
	u32	pre_off_check_result;
	struct mtk_smi_pd_log	last_pd_log;
	struct mtk_smi			smi;
};

static u32 log_level;
enum smi_log_level {
	log_config_bit = 0,
	log_set_bw,
	log_pd_callback,
};

#define MAX_INIT_POWER_ON_DEV	(5)
static struct mtk_smi *init_power_on_dev[MAX_INIT_POWER_ON_DEV];
static unsigned int init_power_on_num;

#define MAX_PD_CTRL_NUM	(8)
static struct mtk_smi_pd *smi_pd_ctrl[MAX_PD_CTRL_NUM];
static unsigned int smi_pd_ctrl_num;

static void power_reset_imp(struct mtk_smi_pd *smi_pd)
{
	int i;

	for (i = 0; (i < MAX_LARB_FOR_CLAMP) && smi_pd->power_reset_reg[i]; i++) {
		writel(smi_pd->power_reset_value[i], smi_pd->power_reset_reg[i]);
		writel(0, smi_pd->power_reset_reg[i]);
		smi_pd->last_pd_log.reset_status[i] = readl(smi_pd->power_reset_reg[i]);
	}
}

void mtk_smi_common_bw_set(struct device *dev, const u32 port, const u32 val)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	struct mtk_smi *common = dev_get_drvdata(larb->smi_common);

	if (port >= SMI_COMMON_LARB_NR_MAX) { /* max: 8 input larbs. */
		dev_err(dev, "%s port invalid:%d, val:%u.\n", __func__,
			port, val);
		return;
	}

	if (common->plat->gen == MTK_SMI_GEN3)
		common->plat->bwl[common->commid * SMI_COMMON_LARB_NR_MAX + port] = val;
	else if (common->plat->gen == MTK_SMI_GEN2)
		common->plat->bwl[port] = val;
	if (atomic_read(&common->ref_count)) {
		writel(val, common->base + SMI_L1ARB(port));
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_common_bw_set);

void mtk_smi_larb_bw_set(struct device *dev, const u32 port, const u32 val)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);

	if (port >= SMI_LARB_PORT_NR_MAX) { /* max: 32 ports for a larb */
		dev_err(dev, "%s port invalid:%d, val:%u.\n", __func__,
			port, val);
		return;
	}
	if (val) {
		larb->larb_gen->bwl[larb->larbid * SMI_LARB_PORT_NR_MAX + port] = val;
		if (atomic_read(&larb->smi.ref_count)) {
			writel(val, larb->base + SMI_LARB_OSTDL_PORTx(port));
			//writel(val, larb->base + INT_SMI_LARB_OSTDL_PORTx(port));
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_bw_set);

void mtk_smi_check_comm_ref_cnt(struct device *dev)
{
	struct mtk_smi *common = dev_get_drvdata(dev);
	int ref_count, api_ref_count;

	if (common) {
		ref_count = atomic_read(&common->ref_count);
		api_ref_count = atomic_read(&common->api_ref_count);
		if ((ref_count > 0) || (api_ref_count > 0))
			pr_notice("%s comm:%u ref_cnt=%d api_ref_cnt=%d\n",
				__func__, common->commid, ref_count, api_ref_count);
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_check_comm_ref_cnt);

void mtk_smi_check_larb_ref_cnt(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	int ref_count, api_ref_count;

	if (larb) {
		ref_count = atomic_read(&larb->smi.ref_count);
		api_ref_count = atomic_read(&larb->smi.api_ref_count);
		if ((ref_count > 0) || (api_ref_count > 0))
			pr_notice("%s larb:%u ref_cnt=%d api_ref_cnt=%d\n",
				__func__, larb->larbid, ref_count, api_ref_count);
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_check_larb_ref_cnt);

void mtk_smi_init_power_off(void)
{
	int i;
	struct mtk_smi *smi;

	for (i = 0; i < init_power_on_num; i++) {
		smi = init_power_on_dev[i];
		pm_runtime_put_sync(smi->dev);
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_init_power_off);

void mtk_smi_dump_last_pd(const char *user)
{
	int i, j;
	struct mtk_smi_pd *smi_pd;

	pr_info("%s: check caller:%s\n", __func__, user);
	for (i = 0; i < smi_pd_ctrl_num; i++) {
		smi_pd = smi_pd_ctrl[i];
		dev_notice(smi_pd->dev, "power status = %d\n", smi_pd->last_pd_log.power_status);
		dev_notice(smi_pd->dev, "time = %18llu\n", smi_pd->last_pd_log.time);
		for (j = 0; j < MAX_COMMON_FOR_CLAMP; j++) {
			if (!smi_pd->smi_common_dev[j])
				break;
			dev_notice(smi_pd->dev, "%s: clamp status = %#x\n"
						, dev_name(smi_pd->smi_common_dev[j])
						, smi_pd->last_pd_log.clamp_status[j]);
		}
		if (!smi_pd->is_main) {
			for (j = 0; (j < MAX_LARB_FOR_CLAMP) && smi_pd->power_reset_reg[j]; j++) {
				dev_notice(smi_pd->dev, "reset status: %#x = %#x\n"
							, smi_pd->power_reset_pa[j]
							, smi_pd->last_pd_log.reset_status[j]);
			}
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_dump_last_pd);

static int mtk_smi_clk_enable(const struct mtk_smi *smi)
{
	int ret;

	ret = clk_prepare_enable(smi->clk_apb);
	if (ret)
		return ret;

	ret = clk_prepare_enable(smi->clk_smi);
	if (ret)
		goto err_disable_apb;

	ret = clk_prepare_enable(smi->clk_gals0);
	if (ret)
		goto err_disable_smi;

	ret = clk_prepare_enable(smi->clk_gals1);
	if (ret)
		goto err_disable_gals0;

	return 0;

err_disable_gals0:
	clk_disable_unprepare(smi->clk_gals0);
err_disable_smi:
	clk_disable_unprepare(smi->clk_smi);
err_disable_apb:
	clk_disable_unprepare(smi->clk_apb);
	return ret;
}

static void mtk_smi_clk_disable(const struct mtk_smi *smi)
{
	clk_disable_unprepare(smi->clk_gals1);
	clk_disable_unprepare(smi->clk_gals0);
	clk_disable_unprepare(smi->clk_smi);
	clk_disable_unprepare(smi->clk_apb);
}

int mtk_smi_larb_ultra_dis(struct device *larbdev, bool is_dis)
{
	struct mtk_smi_larb *larb;
	u32 val;

	if (unlikely(!larbdev))
		return -EINVAL;
	larb = dev_get_drvdata(larbdev);

	if (unlikely(!larb))
		return -ENODEV;

	val = is_dis ? 0xffffffff : 0x0;
	writel(val, larb->base + SMI_LARB_DISABLE_ULTRA);
	pr_info("[SMI]larb:%d set dis_ultra:%d\n", larb->larbid, is_dis);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_ultra_dis);

int mtk_smi_larb_get(struct device *larbdev)
{
	struct mtk_smi_larb *larb;
	int ret;

	if (unlikely(!larbdev))
		return -EINVAL;
	larb = dev_get_drvdata(larbdev);

	if (unlikely(!larb))
		return -ENODEV;

	atomic_inc(&larb->smi.api_ref_count);
	if (log_level & 1 << log_config_bit)
		pr_info("[SMI]larb:%d get ref_count: %d api_ref_count:%d\n",
			larb->larbid, atomic_read(&larb->smi.ref_count),
			atomic_read(&larb->smi.api_ref_count));
	ret = pm_runtime_resume_and_get(larbdev);
	if (ret < 0) {
		dev_notice(larbdev, "Unable to enable SMI LARB%d. ret:%d\n",
			larb->larbid, ret);
		pm_runtime_put_sync(larbdev);
	}

	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_get);

void mtk_smi_larb_put(struct device *larbdev)
{
	struct mtk_smi_larb *larb;

	if (unlikely(!larbdev))
		return;
	larb = dev_get_drvdata(larbdev);

	if (unlikely(!larb))
		return;

	atomic_dec(&larb->smi.api_ref_count);
	if (log_level & 1 << log_config_bit)
		pr_info("[SMI]larb:%d put ref_count: %d api_ref_count:%d\n",
			larb->larbid, atomic_read(&larb->smi.ref_count),
			atomic_read(&larb->smi.api_ref_count));

	pm_runtime_put_sync(larbdev);
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_put);

void mtk_smi_add_device_link(struct device *dev, struct device *larbdev)
{
	struct device_link *link;
	struct mtk_smi_larb *larb = dev_get_drvdata(larbdev);

	link = device_link_add(dev, larbdev,
				DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
	if (!link)
		dev_notice(dev, "Unable to link SMI LARB%d\n", larb->larbid);

}
EXPORT_SYMBOL_GPL(mtk_smi_add_device_link);

s32 smi_sysram_enable(struct device *larbdev, const u32 master_id,
	const bool enable, const char *user)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(larbdev);
	u32 larbid = MTK_M4U_TO_LARB(master_id);
	u32 port = MTK_M4U_TO_PORT(master_id);
	u32 ostd[2], val;

	ostd[0] = readl_relaxed(larb->base + SMI_LARB_OSTD_MON_PORT(port));
	ostd[1] = readl_relaxed(larb->base + INT_SMI_LARB_OSTD_MON_PORT(port));
	if (ostd[0] || ostd[1]) {
		aee_kernel_exception(user,
			"%s set larb%u port%u sysram %d failed ostd:%u %u\n",
			user, larbid, port, enable, ostd[0], ostd[1]);
		return (ostd[1] << 16) | ostd[0];
	}

	val = readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(port));
	if (enable)
		writel(val | (0xf << 16),
			larb->base + SMI_LARB_NONSEC_CON(port));
	else
		writel(val & 0xfff0ffff,
			larb->base + SMI_LARB_NONSEC_CON(port));
	wmb(); /* make sure settings are written */

	return 0;
}
EXPORT_SYMBOL_GPL(smi_sysram_enable);

static int
mtk_smi_larb_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	struct mtk_smi_larb_iommu *larb_mmu = data;
	unsigned int         i;

	if (log_level & 1 << log_config_bit)
		pr_info("[SMI] start larb bind\n");
	for (i = 0; i < MTK_LARB_NR_MAX; i++) {
		if (dev == larb_mmu[i].dev) {
			larb->larbid = i;
			larb->mmu = &larb_mmu[i].mmu;
			larb->bank = larb_mmu[i].bank;
			if (log_level & 1 << log_config_bit)
				dev_notice(dev,
					"[SMI]larb%d bind ptr_mmu:0x%x val_mmu_32:0x%x bit32:%u\n",
					i, larb->mmu, *((unsigned int *)(larb->mmu)),
					larb->bank[i]);
			return 0;
		}
	}
	if (log_level & 1 << log_config_bit)
		dev_notice(dev, "failed to find any larb matched\n");
	return -ENODEV;
}

static void mtk_smi_larb_config_port_gen2_general(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	u32 reg;
	int i;

	if (BIT(larb->larbid) & larb->larb_gen->larb_direct_to_common_mask)
		return;

	if (log_level & 1 << log_config_bit)
		dev_notice(dev, "[SMI]larb%d config port\n", larb->larbid);
	if (larb->mmu) {
		if (log_level & 1 << log_config_bit)
			dev_notice(dev, "[SMI]larb%d config mmu:0x%lx mmu_32:0x%x\n",
				larb->larbid, *((unsigned long *)(larb->mmu)),
				*((unsigned int *)(larb->mmu)));
		for_each_set_bit(i, (unsigned long *)larb->mmu, 32) {
			reg = readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(i));
			reg |= F_MMU_EN;
			reg |= BANK_SEL(larb->bank[i]);
			writel(reg, larb->base + SMI_LARB_NONSEC_CON(i));
			if (log_level & 1 << log_config_bit)
				dev_notice(dev,
					"[SMI]larb:%d port:%d mmu:%u bit32:%u offset:%#x reg:%#x\n",
					larb->larbid, i, larb->mmu, larb->bank[i],
					SMI_LARB_NONSEC_CON(i), reg);
		}
	}
	if (!larb->larb_gen->has_bwl)
		return;
	for (i = 0; i < larb->larb_gen->port_in_larb_gen2[larb->larbid]; i++)
		mtk_smi_larb_bw_set(larb->smi.dev, i, larb->larb_gen->bwl[
			larb->larbid * SMI_LARB_PORT_NR_MAX + i]);
	for (i = 0; i < SMI_LARB_MISC_NR; i++)
		writel_relaxed(larb->larb_gen->misc[
			larb->larbid * SMI_LARB_MISC_NR + i].value,
			larb->base + larb->larb_gen->misc[
			larb->larbid * SMI_LARB_MISC_NR + i].offset);
	if (!larb->larb_gen->has_grouping || !larb->larb_gen->has_bw_thrt)
		return;
	for (i = larb->larb_gen->cmd_group[larb->larbid * 2];
		i < larb->larb_gen->cmd_group[larb->larbid * 2 + 1]; i++) {
		writel_relaxed(readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(i)) | 0x2,
			larb->base + SMI_LARB_NONSEC_CON(i));
	}
	for (i = larb->larb_gen->bw_thrt_en[larb->larbid * 2];
		i < larb->larb_gen->bw_thrt_en[larb->larbid * 2 + 1]; i++) {
		writel_relaxed(readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(i)) | 0x8,
			larb->base + SMI_LARB_NONSEC_CON(i));
	}

	wmb(); /* make sure settings are written */

}

static void mtk_smi_larb_config_port_mt8173(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);

	writel(*larb->mmu, larb->base + SMI_LARB_MMU_EN);
}

static void mtk_smi_larb_config_port_mt8167(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);

	writel(*larb->mmu, larb->base + MT8167_SMI_LARB_MMU_EN);
}

static void mtk_smi_larb_config_port_gen1(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;
	struct mtk_smi *common = dev_get_drvdata(larb->smi_common_dev[0]);
	int i, m4u_port_id, larb_port_num;
	u32 sec_con_val, reg_val;

	m4u_port_id = larb_gen->port_in_larb[larb->larbid];
	larb_port_num = larb_gen->port_in_larb[larb->larbid + 1]
			- larb_gen->port_in_larb[larb->larbid];

	for (i = 0; i < larb_port_num; i++, m4u_port_id++) {
		if (*larb->mmu & BIT(i)) {
			/* bit[port + 3] controls the virtual or physical */
			sec_con_val = SMI_SECUR_CON_VAL_VIRT(m4u_port_id);
		} else {
			/* do not need to enable m4u for this port */
			continue;
		}
		reg_val = readl(common->smi_ao_base
			+ REG_SMI_SECUR_CON_ADDR(m4u_port_id));
		reg_val &= SMI_SECUR_CON_VAL_MSK(m4u_port_id);
		reg_val |= sec_con_val;
		reg_val |= SMI_SECUR_CON_VAL_DOMAIN(m4u_port_id);
		writel(reg_val,
			common->smi_ao_base
			+ REG_SMI_SECUR_CON_ADDR(m4u_port_id));
	}
}

static void
mtk_smi_larb_unbind(struct device *dev, struct device *master, void *data)
{
	/* Do nothing as the iommu is always enabled. */
}

static const struct component_ops mtk_smi_larb_component_ops = {
	.bind = mtk_smi_larb_bind,
	.unbind = mtk_smi_larb_unbind,
};

static u8
mtk_smi_larb_mt6873_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x2, 0x28, 0xa, 0xc, 0x28,},
	{0x2, 0x2, 0x18, 0x18, 0x18, 0xa, 0xc, 0x28,},
	{0x5, 0x5, 0x5, 0x5, 0x1,},
	{},
	{0x28, 0x19, 0xb, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4, 0x1,},
	{0x1, 0x1, 0x4, 0x1, 0x1, 0x1, 0x1, 0x16,},
	{},
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x5, 0x2, 0x12, 0x13, 0x4, 0x4, 0x1,
	 0x4, 0x2, 0x1,},
	{},
	{0xa, 0x7, 0xf, 0x8, 0x1, 0x8, 0x9, 0x3, 0x3, 0x6, 0x7, 0x4,
	 0xa, 0x3, 0x4, 0xe, 0x1, 0x7, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0x1, 0x1,},
	{},
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0xe, 0x1, 0x7, 0x8, 0x7, 0x7, 0x1, 0x6, 0x2,
	 0xf, 0x8, 0x1, 0x1, 0x1,},
	{},
	{0x2, 0xc, 0xc, 0xe, 0x6, 0x6, 0x6, 0x6, 0x6, 0x12, 0x6, 0x28,},
	{0x2, 0xc, 0xc, 0x28, 0x12, 0x6,},
	{},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x2, 0x2, 0x4, 0x2,},
	{0x9, 0x9, 0x5, 0x5, 0x1, 0x1,},
};

static u8
mtk_smi_larb_mt6853_cmd_group[MTK_LARB_NR_MAX][2] = {
	{2, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6853_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x2, 0x28, 0x1e,},
	{0x2, 0x18, 0xa, 0xc, 0x1e,},
	{0x5, 0x5, 0x5, 0x5, 0x1,},
	{},
	{0x28, 0x19, 0xb, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4, 0x1, 0x16,},
	{},
	{},
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x5, 0x2, 0x12, 0x13, 0x4, 0x4, 0x1, 0x4,},
	{},
	{0xa, 0x7, 0xf, 0x8, 0x1, 0x8, 0x9, 0x3, 0x3, 0x6, 0x7, 0x4,
	 0xa, 0x3, 0x4, 0xe, 0x1, 0x7, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0x1, 0x1,},
	{},
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0xe, 0x1, 0x7, 0x8, 0x7, 0x7, 0x1, 0x6, 0x2,
	 0xf, 0x8, 0x1, 0x1, 0x1,},
	{},
	{0x2, 0xc, 0xc, 0x1, 0x1, 0x1, 0x6, 0x6, 0x6, 0x12, 0x6, 0x1,},
	{0x1, 0x1, 0x1, 0x28, 0x12, 0x6,},
	{0x28, 0x1, 0x2, 0x28, 0x1,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x2, 0x2, 0x4, 0x2,},
	{0x9, 0x9, 0x5, 0x5, 0x1, 0x1,},
};

static u8
mtk_smi_larb_mt6853_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{0, 0}, {0, 0},
	{0, 5}, {0, 0},
	{0, 12}, {0, 0}, {0, 0},
	{0, 13}, {0, 0},
	{0, 29}, {0, 0}, {0, 29}, {0, 0},
	{0, 0}, {0, 0},
	{0, 19}, {0, 0}, {0, 0}, {0, 0},
	{0, 4}, {0, 6}, /*20*/
};

static u8
mtk_smi_larb_mt6893_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x6, 0x2, 0x2, 0x2, 0x28, 0x18, 0x18, 0x1, 0x1, 0x1, 0x8, 0x8, 0x1, 0x3f,},
	{0x2, 0x6, 0x2, 0x2, 0x2, 0x28, 0x18, 0x18, 0x1, 0x1, 0x1, 0x8, 0x8, 0x1, 0x3f,},
	{0x5, 0x5, 0x5, 0x5, 0x1, 0x3f,},
	{0x5, 0x5, 0x5, 0x5, 0x1, 0x3f,},
	{0x28, 0x19, 0xb, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4, 0x1,},
	{0x1, 0x1, 0x4, 0x1, 0x1, 0x1, 0x1, 0x16,},
	{},
	{0x1, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1,
	 0x4, 0x1, 0xa, 0x6, 0x1, 0xa, 0x6, 0x1, 0x1, 0x1, 0x1, 0x5,
	 0x3, 0x3, 0x4,},
	{0x1, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1,
	 0x4, 0x1, 0xa, 0x6, 0x1, 0xa, 0x6, 0x1, 0x1, 0x1, 0x1, 0x5,
	 0x3, 0x3, 0x4,},
	{0x9, 0x7, 0xf, 0x8, 0x1, 0x8, 0x9, 0x3, 0x3, 0x6, 0x7, 0x4,
	 0x9, 0x3, 0x4, 0xe, 0x1, 0x7, 0x8, 0x7, 0x7, 0x1, 0x6, 0x2,
	 0xf, 0x8, 0x1, 0x1, 0x1,},
	{},
	{0x9, 0x7, 0xf, 0x8, 0x1, 0x8, 0x9, 0x3, 0x3, 0x6, 0x7, 0x4,
	 0x9, 0x3, 0x4, 0xe, 0x1, 0x7, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0x1, 0x1,},
	{},
	{0x2, 0xc, 0xc, 0xe, 0x6, 0x6, 0x6, 0x6, 0x6, 0x12, 0x6, 0x1,},
	{0x2, 0xc, 0xc, 0x28, 0x12, 0x6,},
	{0x28, 0x1, 0x2, 0x28, 0x1,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x2, 0x14, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x2, 0x14, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x2, 0x14, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x2, 0x2, 0x4, 0x2,},
	{0x9, 0x9, 0x5, 0x5, 0x1, 0x1,},
};

static u8
mtk_smi_larb_mt6893_cmd_group[MTK_LARB_NR_MAX][2] = {
	{2, 3}, {1, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6893_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{14, 15}, {4, 5},
	{0, 6}, {0, 0},
	{0, 11}, {0, 0}, {0, 0},
	{0, 27}, {0, 0},
	{0, 29}, {0, 0}, {0, 29}, {0, 0},
	{11, 12}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 4}, {0, 6},
};

static u8
mtk_smi_larb_mt6983_cmd_group[MTK_LARB_NR_MAX][2] = {
	{7, 12}, {0, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {7, 12}, {0, 5}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6983_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{14, 16}, {7, 8},
	{0, 9}, {0, 0},
	{0, 11}, {0, 9}, {0, 4},
	{0, 31}, {0, 31},
	{0, 25}, {0, 20}, {0, 30}, {0, 16},
	{0, 0}, {0, 0},
	{0, 19}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {14, 16}, /*20*/
	{7, 8}, {0, 30}, {0, 30}, {0, 0},
	{0, 0}, {0, 0}, {0, 0},/*26*/
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6983_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x6, 0x6, 0x4, 0x4, 0x2, 0x2, 0x40, 0x26, 0x26,
	 0x20, 0x20, 0xe, 0xe, 0x7, 0x1,}, /* LARB0 */
	{0x40, 0x26, 0x26, 0x20, 0x20, 0xe, 0xe, 0x1,}, /* LARB1 */
	{0x4, 0x4, 0x4, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1,}, /* LARB2 */
	{0x6, 0x6, 0x5, 0x5, 0x1, 0x1, 0x1, 0x2, 0x2,}, /* LARB3 */
	{0x40, 0x28, 0x11, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x5, 0x1,}, /* LARB4 */
	{0x1, 0x1, 0x5, 0x1, 0x1, 0x2, 0x17, 0xc, 0x24,}, /* LARB5 */
	{0x1, 0x1, 0x1, 0x1,}, /* LARB6 */
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1,
	 0x1, 0x1, 0x1, 0x2, 0x1, 0x1, 0x3, 0x8, 0x5, 0x1,
	 0x1, 0x4, 0x2, 0x2, 0x3, 0x1, 0x1, 0x8, 0x5, 0x1, 0x1,}, /* LARB7 */
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1,
	 0x1, 0x1, 0x1, 0x2, 0x1, 0x1, 0x3, 0x8, 0x5, 0x1,
	 0x1, 0x4, 0x2, 0x2, 0x3, 0x1, 0x1, 0x8, 0x5, 0x1, 0x1,}, /* LARB8 */
	{0x13, 0x1, 0x10, 0x8, 0x3, 0x5, 0x1, 0x1, 0x10, 0x10,
	 0x10, 0x1f, 0x30, 0x2, 0x13, 0x10, 0x8, 0x3, 0x2, 0x7,
	 0x5, 0x1, 0x30, 0x2, 0x7,}, /* LARB9 */
	{0x2f, 0x1, 0x8, 0x8, 0x1, 0x1, 0x4, 0x18, 0x18, 0x19,
	 0xb, 0x1, 0x10, 0x9, 0x4, 0x4, 0x15, 0x1, 0x6, 0x40,}, /* LARB10 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB11 */
	{0x6, 0x2, 0x4, 0x2, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4,
	 0x6, 0x2, 0x4, 0x2, 0x4, 0x4,}, /* LARB12 */
	{0x2, 0x2, 0xa, 0xa, 0xa, 0xa, 0x1, 0x1, 0x2, 0x2,
	 0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2,
	 0x2, 0x2, 0x1, 0x1,}, /* LARB13 */
	{0xa, 0xa, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0xa, 0xa,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x8,
	 0x8, 0x1, 0x1,}, /* LARB14 */
	{0x1f, 0x1f, 0x4, 0x1, 0x1, 0x4, 0x10, 0x10, 0x10, 0x14,
	 0x4, 0x7, 0x15, 0x1, 0x1, 0x7, 0x9, 0x14, 0x1,}, /* LARB15 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4,
	 0x8, 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB16 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB17 */
	{0x2, 0x2, 0x12, 0x1, 0x2, 0x2, 0xa, 0x1,}, /* LARB18 */
	{0x2, 0x2, 0x2, 0x2,}, /* LARB19 */
	{0x2, 0x6, 0x6, 0x4, 0x4, 0x2, 0x2, 0x40, 0x26, 0x26,
	 0x20, 0x20, 0xe, 0xe, 0x7, 0x1,}, /* LARB20 */
	{0x40, 0x26, 0x26, 0x20, 0x20, 0xe, 0xe, 0x1,}, /* LARB21 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB22 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB23 */
	{}, /* LARB24 */
	{0x2, 0x2, 0x2, 0x8, 0x4, 0x2, 0x2, 0x2, 0x8, 0x4,
	 0x1, 0x4, 0x4, 0x2,}, /* LARB25 */
	{0x2, 0x2, 0x2, 0x8, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x2, 0x4, 0x4, 0x2,}, /* LARB26 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4,
	 0x8, 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB27 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4,
	 0x8, 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB28 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB29 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB30 */
};

static u8
mtk_smi_larb_mt6879_cmd_group[MTK_LARB_NR_MAX][2] = {
	{2, 3}, {1, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6879_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{4, 6}, {7, 8},
	{0, 6}, {0, 0},
	{0, 14}, {0, 0}, {0, 0},
	{0, 15}, {0, 0},
	{0, 25}, {0, 20}, {0, 30}, {0, 10},
	{0, 0}, {0, 0},
	{0, 19}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, /*20*/
	{0, 0}, {0, 30}, {0, 30}, {0, 0},
	{0, 0}, {0, 0}, {0, 0},/*27*/
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6879_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x4, 0x3e, 0xc, 0x6, 0x1,}, /* LARB0 */
	{0x2, 0x20, 0x2, 0x4, 0x2, 0xc, 0x4, 0x1,}, /* LARB1 */
	{0x2, 0x2, 0x4, 0x4, 0x1, 0x1,}, /* LARB2 */
	{}, /* LARB3 */
	{0x2f, 0x28, 0xc, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x5, 0x1,
	 0x17, 0x2, 0x4,}, /* LARB4 */
	{}, /* LARB5 */
	{}, /* LARB6 */
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1, 0x3,
	 0x4, 0x4, 0x2, 0x16, 0x17,}, /* LARB7 */
	{}, /* LARB8 */
	{0x13, 0x1, 0x10, 0x8, 0x3, 0x5, 0x1, 0x1, 0x10, 0x10,
	 0x10, 0x1f, 0x30, 0x2, 0x13, 0x10, 0x8, 0x3, 0x2, 0x7,
	 0x5, 0x1, 0x30, 0x2, 0x7,}, /* LARB9 */
	{0x2f, 0x1, 0x8, 0x8, 0x1, 0x1, 0x4, 0x18, 0x18, 0x19,
	 0xb, 0x1, 0x10, 0x9, 0x4, 0x4, 0x15, 0x1, 0x6, 0x40,}, /* LARB10 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB11 */
	{0x6, 0x2, 0x4, 0x2, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4,}, /* LARB12 */
	{0x2, 0x2, 0xa, 0xa, 0xa, 0xa, 0x4, 0x4, 0x2, 0x2,
	 0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2,
	 0x2, 0x2, 0x2, 0x2,}, /* LARB13 */
	{0xa, 0xa, 0x2, 0x2, 0x2, 0x2, 0x4, 0x4, 0xa, 0xa,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x8,
	 0x8, 0x1, 0x1,}, /* LARB14 */
	{0x1f, 0x1f, 0x4, 0x1, 0x1, 0x4, 0x10, 0x10, 0x10, 0x14,
	 0x4, 0x7, 0x15, 0x1, 0x1, 0x7, 0x9, 0x14, 0x1,}, /* LARB15 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4, 0x8,
	 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB16 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB17 */
	{}, /* LARB18 */
	{0x2, 0x2, 0x2, 0x2,}, /* LARB19 */
	{}, /* LARB20 */
	{}, /* LARB21 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB22 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB23 */
	{}, /* LARB24 */
	{0x2, 0x2, 0x2, 0x8, 0x4, 0x2, 0x2, 0x2, 0x8, 0x4, 0x1,}, /* LARB25 */
	{0x2, 0x2, 0x2, 0x8, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2,}, /* LARB26 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4, 0x8,
	 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB27 */
	{}, /* LARB28 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB29 */
};

static u8
mtk_smi_larb_mt6895_cmd_group[MTK_LARB_NR_MAX][2] = {
	{7, 12}, {0, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {7, 12}, {0, 5}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6895_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{14, 16}, {7, 8},
	{0, 9}, {0, 0},
	{0, 11}, {0, 9}, {0, 4},
	{0, 31}, {0, 31},
	{0, 25}, {0, 20}, {0, 30}, {0, 16},
	{0, 0}, {0, 0},
	{0, 19}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {14, 16}, /*20*/
	{7, 8}, {0, 30}, {0, 30}, {0, 0},
	{0, 0}, {0, 0}, {0, 0},/*26*/
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6895_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x6, 0x6, 0x4, 0x4, 0x2, 0x2, 0x40, 0x26, 0x26,
	 0x20, 0x20, 0xe, 0xe, 0x7, 0x1,}, /* LARB0 */
	{0x40, 0x26, 0x26, 0x20, 0x20, 0xe, 0xe, 0x1,}, /* LARB1 */
	{0x4, 0x4, 0x4, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1,}, /* LARB2 */
	{0x6, 0x6, 0x5, 0x5, 0x1, 0x1, 0x1, 0x2, 0x2,}, /* LARB3 */
	{0x40, 0x28, 0x11, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x5, 0x1,}, /* LARB4 */
	{0x1, 0x1, 0x5, 0x1, 0x1, 0x2, 0x17, 0xc, 0x24,}, /* LARB5 */
	{0x1, 0x1, 0x1, 0x1,}, /* LARB6 */
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1,
	 0x1, 0x1, 0x1, 0x2, 0x1, 0x1, 0x3, 0x8, 0x5, 0x1,
	 0x1, 0x4, 0x2, 0x2, 0x3, 0x1, 0x1, 0x8, 0x5, 0x1, 0x1,}, /* LARB7 */
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x1, 0x4, 0x4, 0x1,
	 0x1, 0x1, 0x1, 0x2, 0x1, 0x1, 0x3, 0x8, 0x5, 0x1,
	 0x1, 0x4, 0x2, 0x2, 0x3, 0x1, 0x1, 0x8, 0x5, 0x1, 0x1,}, /* LARB8 */
	{0x13, 0x1, 0x10, 0x8, 0x3, 0x5, 0x1, 0x1, 0x10, 0x10,
	 0x10, 0x1f, 0x30, 0x2, 0x13, 0x10, 0x8, 0x3, 0x2, 0x7,
	 0x5, 0x1, 0x30, 0x2, 0x7,}, /* LARB9 */
	{0x2f, 0x1, 0x8, 0x8, 0x1, 0x1, 0x4, 0x18, 0x18, 0x19,
	 0xb, 0x1, 0x10, 0x9, 0x4, 0x4, 0x15, 0x1, 0x6, 0x40,}, /* LARB10 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB11 */
	{0x6, 0x2, 0x4, 0x2, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4,
	 0x6, 0x2, 0x4, 0x2, 0x4, 0x4,}, /* LARB12 */
	{0x2, 0x2, 0xa, 0xa, 0xa, 0xa, 0x1, 0x1, 0x2, 0x2,
	 0x2, 0x2, 0xa, 0xa, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2,
	 0x2, 0x2, 0x1, 0x1,}, /* LARB13 */
	{0xa, 0xa, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0xa, 0xa,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x8,
	 0x8, 0x1, 0x1,}, /* LARB14 */
	{0x1f, 0x1f, 0x4, 0x1, 0x1, 0x4, 0x10, 0x10, 0x10, 0x14,
	 0x4, 0x7, 0x15, 0x1, 0x1, 0x7, 0x9, 0x14, 0x1,}, /* LARB15 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4,
	 0x8, 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB16 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB17 */
	{0x2, 0x2, 0x12, 0x1, 0x2, 0x2, 0xa, 0x1,}, /* LARB18 */
	{0x2, 0x2, 0x2, 0x2,}, /* LARB19 */
	{0x2, 0x6, 0x6, 0x4, 0x4, 0x2, 0x2, 0x40, 0x26, 0x26,
	 0x20, 0x20, 0xe, 0xe, 0x7, 0x1,}, /* LARB20 */
	{0x40, 0x26, 0x26, 0x20, 0x20, 0xe, 0xe, 0x1,}, /* LARB21 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB22 */
	{0x4, 0x32, 0x1, 0x32, 0x1, 0x1, 0x1f, 0x10, 0x10, 0x13,
	 0x10, 0x8, 0x3, 0x5, 0x3, 0x1f, 0x10, 0x10, 0x1f, 0x1f,
	 0x10, 0x1, 0x1, 0x1f, 0x18, 0x2, 0x7, 0x5, 0x3, 0x40,}, /* LARB23 */
	{}, /* LARB24 */
	{0x2, 0x2, 0x2, 0x8, 0x4, 0x2, 0x2, 0x2, 0x8, 0x4,
	 0x1, 0x4, 0x4, 0x2,}, /* LARB25 */
	{0x2, 0x2, 0x2, 0x8, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x2, 0x4, 0x4, 0x2,}, /* LARB26 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4,
	 0x8, 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB27 */
	{0x1e, 0x2, 0x2, 0x8, 0x2, 0x8, 0x6, 0x2, 0x2, 0x4,
	 0x8, 0x4, 0x2, 0x4, 0x8, 0x2, 0x12,}, /* LARB28 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB29 */
	{0x30, 0x10, 0xe, 0x14, 0x6, 0x2, 0x6,}, /* LARB30 */
};

static u8
mtk_smi_larb_mt6855_cmd_group[MTK_LARB_NR_MAX][2] = {
	{1, 2}, {1, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6855_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{4, 5}, {4, 5},
	{0, 4}, {0, 0},
	{0, 13}, {0, 0}, {0, 0},
	{0, 13}, {0, 0},
	{0, 29}, {0, 0}, {0, 29}, {0, 0},
	{0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 6}, /*20*/
};

static u8
mtk_smi_larb_mt6855_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x40, 0x4, 0xa, 0x26,}, /* LARB0 */
	{0x2, 0x20, 0x2, 0xa, 0x26,}, /* LARB1 */
	{0x2, 0x4, 0x4, 0x1,}, /* LARB2 */
	{}, /* LARB3 */
	{0x2f, 0xc, 0x1, 0x1, 0x1, 0x1, 0x2, 0x2, 0x5, 0x2,
	 0x4}, /* LARB4 */
	{}, /* LARB5 */
	{}, /* LARB6 */
	{0x1, 0x3, 0x2, 0x1, 0x1, 0x8, 0x8, 0x16, 0x17, 0x4,
	 0x4, 0x1, 0x4,}, /* LARB7 */
	{}, /* LARB8 */
	{0x6, 0x3, 0xc, 0x6, 0x1, 0x4, 0x4, 0x2, 0x2, 0x5,
	 0x5, 0x2, 0x6, 0x3, 0x3, 0xb, 0x1, 0x6, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,}, /* LARB9 */
	{}, /* LARB10 */
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0x1, 0x1, 0xb, 0x1, 0x6, 0x6, 0x5,
	 0x6, 0x1, 0x5, 0x2, 0xc, 0x6, 0x1, 0x1, 0x1,}, /* LARB11 */
	{}, /* LARB12 */
	{0x2, 0xa, 0xa, 0xc, 0x6, 0x6, 0x6, 0x6, 0x6, 0xe,
	 0x4, 0x1, 0x6, 0x6, 0x2,}, /* LARB13 */
	{0x1, 0x1, 0x1, 0x2a, 0xe, 0x4, 0xc, 0xc, 0x6, 0x6,}, /* LARB14 */
	{}, /* LARB15 */
	{0x28, 0x10, 0x2, 0x8, 0x14, 0x2, 0x1e, 0x10, 0x4, 0x2,
	 0x4, 0x2, 0x4, 0x2, 0x6, 0x2, 0x4,}, /* LARB16 */
	{0x28, 0x10, 0x2, 0x8, 0x14, 0x2, 0x1e, 0x10, 0x4, 0x2,
	 0x4, 0x2, 0x4, 0x2, 0x6, 0x2, 0x4,}, /* LARB17 */
	{}, /* LARB18 */
	{}, /* LARB19 */
	{0x7, 0x7, 0x4, 0x4, 0x1, 0x1,}, /* LARB20 */
};

static u8
mtk_smi_larb_mt6833_cmd_group[MTK_LARB_NR_MAX][2] = {
	{5, 8}, {5, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6833_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{0, 0}, {0, 0},
	{0, 5}, {0, 0},
	{0, 12}, {0, 0}, {0, 0},
	{0, 13}, {0, 0},
	{0, 29}, {0, 0}, {0, 30}, {0, 3},
	{0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 4}, {0, 6}, /*20*/
};

static u8
mtk_smi_larb_mt6833_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x2, 0x28, 0x28,},
	{0x2, 0x18, 0x6, 0x6, 0x28,},
	{0x6, 0x6, 0x6, 0x6, 0x1,},
	{},
	{0x28, 0x1, 0xc, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x1, 0x18,},
	{},
	{},
	{0x1, 0x3, 0x1, 0x1, 0x1, 0x3, 0x2, 0xd, 0x7, 0x5, 0x3, 0x1, 0x5,},
	{},
	{0xa, 0x7, 0xf, 0x8, 0x1, 0x8, 0x9, 0x3, 0x3, 0x6, 0x7, 0x4,
	 0xa, 0x3, 0x4, 0xe, 0x1, 0x7, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0x1, 0x1,},
	{},
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	 0x1, 0x1, 0x1, 0xe, 0x1, 0x7, 0x8, 0x7, 0x7, 0x1, 0x6, 0x2,
	 0xf, 0x8, 0x1, 0x1, 0x1,},
	{},
	{0x2, 0xc, 0xc, 0x1, 0x1, 0x1, 0x6, 0x6, 0x6, 0x12, 0x6, 0x1,},
	{0x1, 0x1, 0x1, 0x1, 0x12, 0x6,},
	{0x28, 0x1, 0x2, 0x28, 0x1,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x2, 0x2, 0x4, 0x2,},
	{0x9, 0x9, 0x5, 0x5, 0x1, 0x1,},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6833_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
};

static u8
mtk_smi_larb_mt6789_cmd_group[MTK_LARB_NR_MAX][2] = {
	{5, 8}, {5, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u8
mtk_smi_larb_mt6789_bw_thrt_en[MTK_LARB_NR_MAX][2] = {
	{0, 0}, {0, 0},
	{0, 5}, {0, 0},
	{0, 14}, {0, 0}, {0, 0},
	{0, 13}, {0, 0},
	{0, 29}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 4}, {0, 6}, /*20*/
};

static u8
mtk_smi_larb_mt6789_bwl[MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{0x2, 0x2, 0x28, 0x6,},
	{0x2, 0x18, 0x6, 0x6, 0x6,},
	{0x6, 0x6, 0x6, 0x6, 0x1,},
	{},
	{0x28, 0x1, 0xc, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x18, 0x1,
	 0x2, 0x2},
	{},
	{},
	{0x1, 0x3, 0x1, 0x1, 0x1, 0x3, 0x2, 0xd, 0x7, 0x5, 0x3, 0x1, 0x5,},
	{},
	{0xa, 0x7, 0xf, 0x8, 0x1, 0x8, 0x9, 0x3, 0x3, 0x6, 0x7, 0x4,
	 0xa, 0x3, 0x4, 0xe, 0x1, 0x7, 0x8, 0x7, 0x7, 0x1, 0x6, 0x2,
	 0xf, 0x8, 0x1, 0x1, 0x1,},
	{},
	{},
	{},
	{0x2, 0xc, 0xc, 0x1, 0x1, 0x1, 0x6, 0x6, 0x6, 0x12, 0x6, 0x1,},
	{0x1, 0x1, 0x1, 0x1, 0x12, 0x6,},
	{},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{0x28, 0x14, 0x2, 0xc, 0x18, 0x4, 0x28, 0x14, 0x4, 0x4, 0x4, 0x2,
	 0x4, 0x2, 0x8, 0x4, 0x4,},
	{},
	{0x2, 0x2, 0x4, 0x2,},
	{0x9, 0x9, 0x5, 0x5, 0x1, 0x1,},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6789_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6873_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_FORCE_ULTRA, 0x8000},
	{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_FORCE_ULTRA, 0x8000},
	{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_FORCE_ULTRA, 0x8000},
	{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6853_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256},  {SMI_LARB_FORCE_ULTRA, 0x8000},
		{SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6893_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_VC_PRI_MODE, 0x1}, {SMI_LARB_CMD_THRT_CON, 0x370256},
	{SMI_LARB_SW_FLAG, 0x1}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_VC_PRI_MODE, 0x1}, {SMI_LARB_CMD_THRT_CON, 0x370256},
	{SMI_LARB_SW_FLAG, 0x1}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},
	{INT_SMI_LARB_CMD_THRT_CON, 0x300256},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},
	{INT_SMI_LARB_CMD_THRT_CON, 0x300256},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_DBG_CON, 0x1}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	{INT_SMI_LARB_DBG_CON, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_DBG_CON, 0x1}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	{INT_SMI_LARB_DBG_CON, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_FORCE_ULTRA, 0x8000}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_FORCE_ULTRA, 0x8000}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
	{SMI_LARB_FORCE_ULTRA, 0x8000}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6983_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
};


static struct mtk_smi_reg_pair
mtk_smi_larb_mt6879_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
};



static struct mtk_smi_reg_pair
mtk_smi_larb_mt6895_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1}, {SMI_LARB_DISABLE_ULTRA, 0xffffffff},},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {INT_SMI_LARB_CMD_THRT_CON, 0x370256},
	 {SMI_LARB_SW_FLAG, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_larb_mt6855_misc[MTK_LARB_NR_MAX][SMI_LARB_MISC_NR] = {
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB0*/
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB1*/
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB2*/
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB4*/
	{},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB7*/
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB9*/
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB11*/
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB13*/
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB14*/
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_FORCE_ULTRA, 0x8000},
	 {SMI_LARB_SW_FLAG, 0x1},}, /*LARB16*/
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_FORCE_ULTRA, 0x8000},
	 {SMI_LARB_SW_FLAG, 0x1},}, /*LARB17*/
	{},
	{},
	{{SMI_LARB_CMD_THRT_CON, 0x370223}, {SMI_LARB_SW_FLAG, 0x1},}, /*LARB20*/
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt8173 = {
	/* mt8173 do not need the port in larb */
	.config_port = mtk_smi_larb_config_port_mt8173,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt8167 = {
	/* mt8167 do not need the port in larb */
	.config_port = mtk_smi_larb_config_port_mt8167,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt2701 = {
	.port_in_larb = {
		LARB0_PORT_OFFSET, LARB1_PORT_OFFSET,
		LARB2_PORT_OFFSET, LARB3_PORT_OFFSET
	},
	.config_port = mtk_smi_larb_config_port_gen1,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt2712 = {
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(8) | BIT(9),      /* bdpsys */
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6779 = {
	.config_port  = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask =
		BIT(4) | BIT(6) | BIT(11) | BIT(12) | BIT(13),
		/* DUMMY | IPU0 | IPU1 | CCU | MDLA */
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6873 = {
	.port_in_larb_gen2 = {6, 8, 5, 0, 11, 8, 0, 15, 0, 29, 0, 29,
			      0, 12, 6, 0, 17, 17, 17, 4, 6,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(3) | BIT(6) | BIT(8) |
				      BIT(10) | BIT(12) | BIT(15) | BIT(21) |
					  BIT(22),
				      /*skip larb: 3,6,8,10,12,15,21,22*/
	.has_bwl                    = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6873_bwl,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6873_misc,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6853 = {
	.port_in_larb_gen2 = {4, 5, 5, 0, 12, 0, 0, 13, 0, 29,
				0, 29, 0, 12, 6, 5, 17, 17, 17, 4, 6,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(3) | BIT(5) | BIT(6) | BIT(8) |
				      BIT(10) | BIT(12) | BIT(15) | BIT(18) | BIT(21) |
					  BIT(22),
				      /*skip larb: 3,6,8,10,12,15,21,22*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6853_bwl,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6853_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6853_bw_thrt_en,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6853_misc,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6833 = {
	.port_in_larb_gen2 = {4, 5, 5, 0, 12, 0, 0, 13, 0, 29,
				0, 29, 0, 12, 6, 0, 17, 17, 0, 4, 6,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(3) | BIT(5) | BIT(6) | BIT(8) |
				      BIT(10) | BIT(12) | BIT(15) | BIT(18) | BIT(21) |
					  BIT(22),
				      /*skip larb: 3,6,8,10,12,15,18*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6833_bwl,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6833_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6833_bw_thrt_en,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6833_misc,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6789 = {
	.port_in_larb_gen2 = {4, 5, 5, 0, 14, 0, 0, 13, 0, 29,
				0, 29, 0, 12, 6, 0, 17, 17, 0, 4, 6,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(3) | BIT(5) | BIT(6) | BIT(8) |
				      BIT(10) | BIT(11) | BIT(12) | BIT(15) | BIT(18) |
					  BIT(19) | BIT(21) | BIT(22),
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6789_bwl,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6789_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6789_bw_thrt_en,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6789_misc,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6893 = {
	.port_in_larb_gen2 = {15, 15, 6, 6, 11, 8, 0, 27, 27, 29, 0, 29,
			     0, 12, 6, 5, 17, 17, 17, 4, 6,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(6) | BIT(10) | BIT(12),
				      /*skip larb: 6,10,12*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6893_bwl,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6893_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6893_bw_thrt_en,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6893_misc,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6983 = {
	.port_in_larb_gen2 = {16, 8, 9, 9, 11, 9, 4, 31, 31, 25,
				 20, 30, 16, 24, 23, 19, 17, 7, 8, 4, 16, 8,
				 30, 30, 0, 14, 14, 17, 17, 7, 7,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(24),
				      /*skip larb: 24*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6983_bwl,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6983_misc,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6983_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6983_bw_thrt_en,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6879 = {
	.port_in_larb_gen2 = {6, 8, 6, 0, 14, 0, 0, 15, 0, 25,
				 20, 30, 10, 24, 23, 19, 17, 7, 0, 4, 0, 0,
				 30, 30, 0, 11, 11, 17, 0, 7,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(3) | BIT(5) | BIT(6) | BIT(8) |
					BIT(18) | BIT(20) | BIT(21) | BIT(24) | BIT(28),
				      /*skip larb: 3,5,6,8,18,20,21,24,28*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6879_bwl,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6879_misc,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6879_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6879_bw_thrt_en,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6895 = {
	.port_in_larb_gen2 = {16, 8, 9, 9, 11, 9, 4, 31, 31, 25,
				 20, 30, 16, 24, 23, 19, 17, 7, 8, 4, 16, 8,
				 30, 30, 0, 14, 14, 17, 17, 7, 7,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(24),
				      /*skip larb: 24*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6895_bwl,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6895_misc,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6895_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6895_bw_thrt_en,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6855 = {
	.port_in_larb_gen2 = {5, 5, 4, 0, 13, 0, 0, 13, 0, 29,
				 0, 29, 0, 15, 10, 0, 17, 17, 0, 0, 6,},
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(3) | BIT(5) | BIT(6) | BIT(8) |
					BIT(10) | BIT(12) | BIT(15) | BIT(18) | BIT(19),
				      /*skip larb: 3,5,6,8,10,12,15,18,19*/
	.has_bwl                    = true,
	.has_grouping               = true,
	.has_bw_thrt                = true,
	.bwl                        = (u8 *)mtk_smi_larb_mt6855_bwl,
	.misc = (struct mtk_smi_reg_pair *)mtk_smi_larb_mt6855_misc,
	.cmd_group                  = (u8 *)mtk_smi_larb_mt6855_cmd_group,
	.bw_thrt_en                 = (u8 *)mtk_smi_larb_mt6855_bw_thrt_en,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt8183 = {
	.has_gals                   = true,
	.config_port                = mtk_smi_larb_config_port_gen2_general,
	.larb_direct_to_common_mask = BIT(2) | BIT(3) | BIT(7),
				      /* IPU0 | IPU1 | CCU */
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt8192 = {
	.config_port                = mtk_smi_larb_config_port_gen2_general,
};

static const struct of_device_id mtk_smi_larb_of_ids[] = {
	{
		.compatible = "mediatek,mt8167-smi-larb",
		.data = &mtk_smi_larb_mt8167
	},
	{
		.compatible = "mediatek,mt8173-smi-larb",
		.data = &mtk_smi_larb_mt8173
	},
	{
		.compatible = "mediatek,mt2701-smi-larb",
		.data = &mtk_smi_larb_mt2701
	},
	{
		.compatible = "mediatek,mt6873-smi-larb",
		.data = &mtk_smi_larb_mt6873
	},
	{
		.compatible = "mediatek,mt2712-smi-larb",
		.data = &mtk_smi_larb_mt2712
	},
	{
		.compatible = "mediatek,mt6779-smi-larb",
		.data = &mtk_smi_larb_mt6779
	},
	{
		.compatible = "mediatek,mt8183-smi-larb",
		.data = &mtk_smi_larb_mt8183
	},
	{
		.compatible = "mediatek,mt6853-smi-larb",
		.data = &mtk_smi_larb_mt6853
	},
	{
		.compatible = "mediatek,mt6833-smi-larb",
		.data = &mtk_smi_larb_mt6833
	},
	{
		.compatible = "mediatek,mt6789-smi-larb",
		.data = &mtk_smi_larb_mt6789
	},
	{
		.compatible = "mediatek,mt6893-smi-larb",
		.data = &mtk_smi_larb_mt6893
	},
	{
		.compatible = "mediatek,mt6983-smi-larb",
		.data = &mtk_smi_larb_mt6983
	},
	{
		.compatible = "mediatek,mt6879-smi-larb",
		.data = &mtk_smi_larb_mt6879
	},
	{
		.compatible = "mediatek,mt6895-smi-larb",
		.data = &mtk_smi_larb_mt6895
	},
	{
		.compatible = "mediatek,mt6855-smi-larb",
		.data = &mtk_smi_larb_mt6855
	},
	{
		.compatible = "mediatek,mt8192-smi-larb",
		.data = &mtk_smi_larb_mt8192
	},
	{}
};

static s32 smi_cmdq(void *data)
{
	struct device *dev = (struct device *)data;

	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;

	node = of_parse_phandle(dev->of_node, "mediatek,cmdq", 0);
	if (node) {
		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (pdev) {
			while (!platform_get_drvdata(pdev)) {
				dev_notice(dev, "Failed to get [cmdq]\n");
				msleep(100);
			}
			if (device_link_add(dev, &pdev->dev,
				DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS))
				dev_notice(dev, "Success to link [cmdq]\n");
			else
				dev_notice(dev, "Failed to link [cmdq]\n");
		} else
			dev_notice(dev, "Failed to get [cmdq] pdev\n");
	} else
		dev_notice(dev, "Failed to get [cmdq] node\n");
	return 0;
}

static int mtk_smi_larb_probe(struct platform_device *pdev)
{
	struct mtk_smi_larb *larb;
	struct resource *res;
	struct device *dev = &pdev->dev, *smi_dev = &pdev->dev;
	struct device_node *smi_node;
	struct platform_device *smi_pdev;
	struct device_link *link;
	int ret, i;

	larb = devm_kzalloc(dev, sizeof(*larb), GFP_KERNEL);
	if (!larb)
		return -ENOMEM;

	larb->larb_gen = of_device_get_match_data(dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	larb->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(larb->base))
		return PTR_ERR(larb->base);

	larb->smi.clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(larb->smi.clk_apb))
		return PTR_ERR(larb->smi.clk_apb);

	larb->smi.clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(larb->smi.clk_smi))
		return PTR_ERR(larb->smi.clk_smi);

	if (larb->larb_gen->has_gals) {
		/* The larbs may still haven't gals even if the SoC support.*/
		larb->smi.clk_gals0 = devm_clk_get(dev, "gals");
		if (PTR_ERR(larb->smi.clk_gals0) == -ENOENT)
			larb->smi.clk_gals0 = NULL;
		else if (IS_ERR(larb->smi.clk_gals0))
			return PTR_ERR(larb->smi.clk_gals0);
	}
	larb->smi.dev = dev;
	atomic_set(&larb->smi.ref_count, 0);
	atomic_set(&larb->smi.api_ref_count, 0);

	for (i = 0; i < LARB_MAX_COMMON; i++) {
		smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", i);
		if (!smi_node)
			break;

		smi_pdev = of_find_device_by_node(smi_node);
		of_node_put(smi_node);
		if (smi_pdev) {
			if (!platform_get_drvdata(smi_pdev))
				return -EPROBE_DEFER;
			larb->smi_common_dev[i] = &smi_pdev->dev;
			link = device_link_add(dev, larb->smi_common_dev[i],
					DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
			if (!link) {
				dev_notice(dev, "Unable to link smi_common device %d\n", i);
				return -ENODEV;
			}
		} else {
			dev_notice(dev, "Failed to get the smi_common device %d\n", i);
			return -EINVAL;
		}
		larb->comm_port_id[i] = -1;
		of_property_read_u32_index(dev->of_node, "mediatek,comm-port-id",
						i, &larb->comm_port_id[i]);
	}


	/* find smi common dev for mmqos */
	larb->smi_common = larb->smi_common_dev[0];
	for (;;) {
		smi_node = smi_dev->of_node;
		smi_node = of_parse_phandle(smi_node, "mediatek,smi", 0);
		if (smi_node) {
			smi_pdev = of_find_device_by_node(smi_node);
			of_node_put(smi_node);
			if (smi_pdev) {
				if (of_property_read_bool(smi_pdev->dev.of_node, "smi-common")) {
					larb->smi_common = &smi_pdev->dev;
					dev_notice(dev, "Succeed to get smi-comm dev for mmqos\n");
					break;
					/* find smi-common dev successfully */
				} else
					smi_dev = &smi_pdev->dev;
			} else {
				dev_notice(dev, "Failed to get smi-comm dev for mmqos\n");
				return -EINVAL;
			}
		} else {
			/* skip mmqos fix */
			dev_notice(dev, "Can not find smi-comm for mmqos\n");
			break;
		}
	}

	pm_runtime_enable(dev);
	platform_set_drvdata(pdev, larb);
	ret = component_add(dev, &mtk_smi_larb_component_ops);
	of_property_read_u32(dev->of_node, "mediatek,larb-id", &larb->larbid);

	if (of_property_read_bool(dev->of_node, "init-power-on")) {
		ret = pm_runtime_get_sync(dev);
		init_power_on_dev[init_power_on_num++] = &larb->smi;
		if (ret < 0) {
			dev_notice(dev, "Unable to enable SMI LARB%d. ret:%d\n",
				larb->larbid, ret);
			pm_runtime_put_sync(dev);
		}
	}

	return ret;
}

static int mtk_smi_larb_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	component_del(&pdev->dev, &mtk_smi_larb_component_ops);
	return 0;
}

static int __maybe_unused mtk_smi_larb_resume(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;
	int ret;

	atomic_inc(&larb->smi.ref_count);
	if (log_level & 1 << log_config_bit)
		pr_info("[SMI]larb:%d callback get ref_count:%d api_ref_count:%d\n",
			larb->larbid, atomic_read(&larb->smi.ref_count),
			atomic_read(&larb->smi.api_ref_count));

	ret = mtk_smi_clk_enable(&larb->smi);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clock(%d).\n", ret);
		return ret;
	}

	if (larb_gen->sleep_ctrl)
		larb_gen->sleep_ctrl(dev, false);

	/* Configure the basic setting for this larb */
	larb_gen->config_port(dev);

	return 0;
}

static RAW_NOTIFIER_HEAD(smi_driver_notifier_list);
int mtk_smi_driver_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&smi_driver_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_smi_driver_register_notifier);

int mtk_smi_driver_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&smi_driver_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_smi_driver_unregister_notifier);

static u32 mtk_smi_common_ostd_check(struct mtk_smi *common,
	u32 check_port, unsigned long flags)
{
	u32 i, val, ret = 0;

	for (i = 0; i < SMI_COMMON_LARB_NR_MAX; i++) {
		if ((check_port >> i) & 1) {
			val = readl_relaxed(common->base + SMI_DEBUG_S(i));
			pr_notice("[SMI] common%d: %#x=%#x, power status = %ld\n",
				common->commid, SMI_DEBUG_S(i), val, flags);
			if (val & SMI_OSTD_CNT_MASK) {
				pr_notice("[SMI] common%d suspend check fail, power status = %ld\n",
					common->commid, flags);
				raw_notifier_call_chain(&smi_driver_notifier_list,
					common->commid, NULL);
				ret = ret | (1 << i);
			}
		}
	}
	return ret;
}

static int mtk_smi_power_suspend_check(struct mtk_smi_pd *smi_pd, unsigned long flags)
{
	u32 i, ret;
	struct mtk_smi *common;

	if (flags == GENPD_NOTIFY_PRE_OFF)
		smi_pd->pre_off_check_result = 0;

	for (i = 0; i < MAX_PD_CHECK_DEV_NUM; i++) {
		if (!smi_pd->suspend_check_dev[i])
			break;
		common = dev_get_drvdata(smi_pd->suspend_check_dev[i]);

		ret = mtk_smi_clk_enable(common);
		if (ret) {
			dev_notice(common->dev, "Failed to enable clock(%d)\n", ret);
			return ret;
		}

		ret = mtk_smi_common_ostd_check(common, smi_pd->suspend_check_port[i], flags);
		mtk_smi_clk_disable(common);

		if (!ret)
			continue;

		if (flags == GENPD_NOTIFY_PRE_OFF)
			smi_pd->pre_off_check_result = ret;
		else if (flags == GENPD_NOTIFY_OFF)
			dev_notice(smi_pd->dev, "[SMI] pre-off check result:%d\n",
				smi_pd->pre_off_check_result);
	}

	return 0;
}

static int mtk_smi_pd_callback(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	struct mtk_smi_pd *smi_pd =
		container_of(nb, struct mtk_smi_pd, nb);
	struct mtk_smi *common;
	int i, j;

	if (smi_pd->suspend_check) {
		if (flags == GENPD_NOTIFY_PRE_OFF || flags == GENPD_NOTIFY_OFF)
			mtk_smi_power_suspend_check(smi_pd, flags);
		return NOTIFY_OK;
	}

	if (flags == GENPD_NOTIFY_ON) {
		/* enable related SMI common port */
		if (log_level & 1 << log_pd_callback)
			dev_notice(smi_pd->dev, "[smi] pd enter callback on:\n");
		for (i = 0; i < MAX_COMMON_FOR_CLAMP; i++) {
			if (!smi_pd->smi_common_dev[i])
				break;
			common = dev_get_drvdata(smi_pd->smi_common_dev[i]);
			for (j = 0; j < SMI_COMMON_LARB_NR_MAX; j++) {
				if ((smi_pd->set_comm_port_range[i] >> j) & 1) {
					if (!smi_pd->is_main) {
						power_reset_imp(smi_pd);
						writel(1 << j, common->base + SMI_CLAMP_EN_CLR);
					} else //disable SMI common port for main-power	on
						writel(1 << j, common->base + SMI_CLAMP_EN_SET);
				}
			}
			smi_pd->last_pd_log.clamp_status[i] = readl(common->base + SMI_CLAMP_EN);
		}
		smi_pd->last_pd_log.power_status = GENPD_NOTIFY_ON;
		smi_pd->last_pd_log.time = ktime_get();

	} else if (flags == GENPD_NOTIFY_PRE_OFF) {
		if (smi_pd->is_main)
			return NOTIFY_OK;
		/* disable related SMI common port */
		if (log_level & 1 << log_pd_callback)
			dev_notice(smi_pd->dev, "[smi] pd enter callback pre-off:\n");
		for (i = 0; i < MAX_COMMON_FOR_CLAMP; i++) {
			if (!smi_pd->smi_common_dev[i])
				break;
			common = dev_get_drvdata(smi_pd->smi_common_dev[i]);
			for (j = 0; j < SMI_COMMON_LARB_NR_MAX; j++) {
				if ((smi_pd->set_comm_port_range[i] >> j) & 1)
					writel(1 << j, common->base + SMI_CLAMP_EN_SET);
			}
			smi_pd->last_pd_log.clamp_status[i] = readl(common->base + SMI_CLAMP_EN);
		}
		smi_pd->last_pd_log.power_status = GENPD_NOTIFY_PRE_OFF;
		smi_pd->last_pd_log.time = ktime_get();
	}

	return NOTIFY_OK;
}

void mtk_smi_larb_clamp(struct device *larbdev, bool on)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(larbdev);
	int i;
	u32 clamp_reg = on ? SMI_CLAMP_EN_SET : SMI_CLAMP_EN_CLR;

	if (unlikely(!larb))
		return;
	/* disable/enable related SMI common port */
	for (i = 0; i < LARB_MAX_COMMON; i++) {
		struct mtk_smi *common;

		if (larb->comm_port_id[i] >= 0 && larb->smi_common_dev[i]) {
			common = dev_get_drvdata(larb->smi_common_dev[i]);

			writel(1 << larb->comm_port_id[i],
				common->base + clamp_reg);

			if (log_level & 1 << log_pd_callback)
				pr_notice("[smi] %s on:%d larb%d, comm%d clamp: %#x = %#x\n",
					__func__, on, larb->larbid, common->commid, SMI_CLAMP_EN,
					readl(common->base + SMI_CLAMP_EN));
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_clamp);

static int __maybe_unused mtk_smi_larb_suspend(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;

	atomic_dec(&larb->smi.ref_count);
	if (log_level & 1 << log_config_bit)
		pr_info("[SMI]larb:%d callback put ref_count:%d api_ref_count:%d\n",
			larb->larbid, atomic_read(&larb->smi.ref_count),
			atomic_read(&larb->smi.api_ref_count));
	if (atomic_read(&larb->smi.ref_count)
		|| atomic_read(&larb->smi.api_ref_count)) {
		dev_notice(dev,
			"Error: larb(%d) ref_count=%d api_ref_count=%d on suspend\n",
			larb->larbid, atomic_read(&larb->smi.ref_count),
			atomic_read(&larb->smi.api_ref_count));
	}

	if (readl_relaxed(larb->base + SMI_LARB_STAT)) {
		pr_notice("[SMI]larb:%d, suspend but busy\n", larb->larbid);
		raw_notifier_call_chain(&smi_driver_notifier_list, larb->larbid, larb);
	}

	if (larb_gen->sleep_ctrl)
		larb_gen->sleep_ctrl(dev, true);

	mtk_smi_clk_disable(&larb->smi);

	return 0;
}

static const struct dev_pm_ops smi_larb_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_smi_larb_suspend, mtk_smi_larb_resume, NULL)
};

static struct platform_driver mtk_smi_larb_driver = {
	.probe	= mtk_smi_larb_probe,
	.remove	= mtk_smi_larb_remove,
	.driver	= {
		.name = "mtk-smi-larb",
		.of_match_table = mtk_smi_larb_of_ids,
		.pm             = &smi_larb_pm_ops,
	}
};

static u16 mtk_smi_common_mt6873_bwl[SMI_COMMON_LARB_NR_MAX] = {
	0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
};

static u16 mtk_smi_common_mt6853_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static u16 mtk_smi_common_mt6833_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static u16 mtk_smi_common_mt6789_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static u16 mtk_smi_common_mt6893_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static u16 mtk_smi_common_mt6983_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static u16 mtk_smi_common_mt6879_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};


static u16 mtk_smi_common_mt6895_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static u16 mtk_smi_common_mt6855_bwl[MTK_COMMON_NR_MAX][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6873_misc[SMI_COMMON_MISC_NR] = {
	{SMI_L1LEN, 0xb},
	{SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x90a090a},
	{SMI_FIFO_TH2, 0x506090a},
	{SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6853_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_M4U_TH, 0xe100e10}, {SMI_FIFO_TH1, 0x90a090a},
	 {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1}, {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6833_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4514}, {SMI_M4U_TH, 0xe100e10},
	 {SMI_FIFO_TH1, 0x9100910}, {SMI_FIFO_TH2, 0x5060910},
	 {SMI_DCM, 0x4f1}, {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6789_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4514}, {SMI_M4U_TH, 0xe100e10},
	 {SMI_FIFO_TH1, 0x90a090a}, {SMI_FIFO_TH2, 0x506090a},
	 {SMI_DCM, 0x4f1}, {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6893_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4514}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0x2}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4514}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0x2}, {SMI_BUS_SEL, 0x1111}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6983_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x454}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},}, /* SMI_DISP_COMMON */
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4405}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},}, /* SMI_MDP_COMMON */
	{{SMI_L1LEN, 0x2}, {SMI_BUS_SEL, 0x4444}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYSRAM_COMMON */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP_SUB_COMMON1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_MDP_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_MDP_SUB_COMMON1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYS_SUB_COMMON2 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYS_SUB_COMMON3 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYS_SUB_COMMON4 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP0_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP1_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_CAM_MM_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_CAM_SYS_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_CAM_MDP_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_IMG_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_IMG_SUB_COMMON1 */
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6879_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x1044}, {SMI_M4U_TH, 0xe100e10},
	 {SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},/* 0:SMI_DISP_COMMON */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 1:smi_infra_disp_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 2:smi_infra_disp_subcommon1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 3:smi_mdp_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 4:smi_mdp_subcommon1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 5:smi_img_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 6:smi_img_subcommon1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 7:smi_cam_mm_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 8:smi_cam_mm_subcommon0 */
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6895_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x454}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},}, /* SMI_DISP_COMMON */
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4405}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},}, /* SMI_MDP_COMMON */
	{{SMI_L1LEN, 0x2}, {SMI_BUS_SEL, 0x4444}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYSRAM_COMMON */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP_SUB_COMMON1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_MDP_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_MDP_SUB_COMMON1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYS_SUB_COMMON2 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYS_SUB_COMMON3 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_SYS_SUB_COMMON4 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP0_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_DISP1_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_CAM_MM_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_CAM_SYS_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_CAM_MDP_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_IMG_SUB_COMMON0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DCM, 0x4f1},
	{SMI_DUMMY, 0x1},},/* SMI_IMG_SUB_COMMON1 */
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6855_misc[MTK_COMMON_NR_MAX][SMI_COMMON_MISC_NR] = {
	{{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x1044}, {SMI_M4U_TH, 0xe100e10},
	 {SMI_FIFO_TH1, 0x506090a}, {SMI_FIFO_TH2, 0x506090a}, {SMI_DCM, 0x4f1},
	 {SMI_DUMMY, 0x1},},
	/* 0:SMI_DISP_COMMON */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 1:smi_infra_disp_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105}, {SMI_DUMMY, 0x1},},
	/* 2:smi_infra_disp_subcommon1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2146}, {SMI_DUMMY, 0x1},},
	/* 3:smi_mdp_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2146}, {SMI_DUMMY, 0x1},},
	/* 4:smi_mdp_subcommon1 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2083}, {SMI_DUMMY, 0x1},},
	/* 5:smi_img_subcommon */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2083}, {SMI_DUMMY, 0x1},},
	/* 6:smi_ipe_subcommon */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2146}, {SMI_DUMMY, 0x1},},
	/* 7:smi_cam_mm_subcommon0 */
	{{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2146}, {SMI_DUMMY, 0x1},},
	/* 8:smi_cam_mm_subcommon1 */
};

static const struct mtk_smi_common_plat mtk_smi_common_gen1 = {
	.gen = MTK_SMI_GEN1,
};

static const struct mtk_smi_common_plat mtk_smi_common_gen2 = {
	.gen = MTK_SMI_GEN2,
};

static const struct mtk_smi_common_plat mtk_smi_common_gen3 = {
	.gen = MTK_SMI_GEN3,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6779 = {
	.gen		= MTK_SMI_GEN2,
	.has_gals	= true,
	.bus_sel	= F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
			  F_MMU1_LARB(5) | F_MMU1_LARB(6) | F_MMU1_LARB(7),
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6873 = {
	.gen      = MTK_SMI_GEN2,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = mtk_smi_common_mt6873_bwl,
	.misc     = mtk_smi_common_mt6873_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6853 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6853_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6853_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6833 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6833_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6833_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6789 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6789_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6789_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6893 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6893_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6893_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6983 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6983_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6983_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6879 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6879_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6879_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6895 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(4) |
		    F_MMU1_LARB(5) | F_MMU1_LARB(7),
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6895_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6895_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6855 = {
	.gen      = MTK_SMI_GEN3,
	.has_gals = true,
	.has_bwl  = true,
	.bwl      = (u16 *)mtk_smi_common_mt6855_bwl,
	.misc     = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6855_misc,
};

static const struct mtk_smi_common_plat mtk_smi_common_mt8183 = {
	.gen      = MTK_SMI_GEN2,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(5) |
		    F_MMU1_LARB(7),
};

static const struct mtk_smi_common_plat mtk_smi_common_mt8192 = {
	.gen      = MTK_SMI_GEN2,
	.has_gals = true,
	.bus_sel  = F_MMU1_LARB(1) | F_MMU1_LARB(2) | F_MMU1_LARB(5) |
		    F_MMU1_LARB(6),
};

static const struct of_device_id mtk_smi_common_of_ids[] = {
	{
		.compatible = "mediatek,mt8173-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt8167-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt2701-smi-common",
		.data = &mtk_smi_common_gen1,
	},
	{
		.compatible = "mediatek,mt2712-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt6779-smi-common",
		.data = &mtk_smi_common_mt6779,
	},
	{
		.compatible = "mediatek,mt6873-smi-common",
		.data = &mtk_smi_common_mt6873,
	},
	{
		.compatible = "mediatek,mt8183-smi-common",
		.data = &mtk_smi_common_mt8183,
	},
	{
		.compatible = "mediatek,mt6853-smi-common",
		.data = &mtk_smi_common_mt6853,
	},
	{
		.compatible = "mediatek,mt6833-smi-common",
		.data = &mtk_smi_common_mt6833,
	},
	{
		.compatible = "mediatek,mt6789-smi-common",
		.data = &mtk_smi_common_mt6789,
	},
	{
		.compatible = "mediatek,mt6893-smi-common",
		.data = &mtk_smi_common_mt6893,
	},
	{
		.compatible = "mediatek,mt6983-smi-common",
		.data = &mtk_smi_common_mt6983,
	},
	{
		.compatible = "mediatek,mt6879-smi-common",
		.data = &mtk_smi_common_mt6879,
	},
	{
		.compatible = "mediatek,mt6895-smi-common",
		.data = &mtk_smi_common_mt6895,
	},
	{
		.compatible = "mediatek,mt6855-smi-common",
		.data = &mtk_smi_common_mt6855,
	},
	{
		.compatible = "mediatek,mt8192-smi-common",
		.data = &mtk_smi_common_mt8192,
	},
	{}
};

static int mtk_smi_common_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_smi *common;
	struct resource *res;
	struct task_struct *kthr;
	struct device_node *smi_node;
	struct platform_device *smi_pdev;
	struct device_link *link;
	int ret;

	common = devm_kzalloc(dev, sizeof(*common), GFP_KERNEL);
	if (!common)
		return -ENOMEM;
	common->dev = dev;
	common->plat = of_device_get_match_data(dev);
	atomic_set(&common->ref_count, 0);
	atomic_set(&common->api_ref_count, 0);

	common->clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(common->clk_apb))
		return PTR_ERR(common->clk_apb);

	common->clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(common->clk_smi))
		return PTR_ERR(common->clk_smi);

	if (common->plat->has_gals) {
		common->clk_gals0 = devm_clk_get(dev, "gals0");
		if (IS_ERR(common->clk_gals0))
			return PTR_ERR(common->clk_gals0);

		common->clk_gals1 = devm_clk_get(dev, "gals1");
		if (IS_ERR(common->clk_gals1))
			return PTR_ERR(common->clk_gals1);
	}

	/*
	 * for mtk smi gen 1, we need to get the ao(always on) base to config
	 * m4u port, and we need to enable the aync clock for transform the smi
	 * clock into emi clock domain, but for mtk smi gen2, there's no smi ao
	 * base.
	 */
	if (common->plat->gen == MTK_SMI_GEN1) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		common->smi_ao_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(common->smi_ao_base))
			return PTR_ERR(common->smi_ao_base);

		common->clk_async = devm_clk_get(dev, "async");
		if (IS_ERR(common->clk_async))
			return PTR_ERR(common->clk_async);

		ret = clk_prepare_enable(common->clk_async);
		if (ret)
			return ret;
	} else {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		common->base = devm_ioremap_resource(dev, res);
		if (IS_ERR(common->base))
			return PTR_ERR(common->base);
	}

	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 0);
	if (smi_node) {
		smi_pdev = of_find_device_by_node(smi_node);
		of_node_put(smi_node);
		if (smi_pdev) {
			if (!platform_get_drvdata(smi_pdev))
				return -EPROBE_DEFER;
			link = device_link_add(dev, &smi_pdev->dev,
					       DL_FLAG_PM_RUNTIME |
					       DL_FLAG_STATELESS);
			if (!link) {
				dev_notice(dev,
					"Unable to link sram smi-common dev\n");
				return -ENODEV;
			}
		} else {
			dev_notice(dev, "Failed to get sram smi_common device\n");
			return -EINVAL;
		}
	}

	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 1);
	if (smi_node) {
		smi_pdev = of_find_device_by_node(smi_node);
		of_node_put(smi_node);
		if (smi_pdev) {
			if (!platform_get_drvdata(smi_pdev))
				return -EPROBE_DEFER;
			link = device_link_add(dev, &smi_pdev->dev,
					       DL_FLAG_PM_RUNTIME |
					       DL_FLAG_STATELESS);
			if (!link) {
				dev_notice(dev,
					"Unable to link sram smi-common dev\n");
				return -ENODEV;
			}
		} else {
			dev_notice(dev, "Failed to get sram smi_common device\n");
			return -EINVAL;
		}
	}

	of_property_read_u32(dev->of_node, "mediatek,common-id", &common->commid);
	pm_runtime_enable(dev);
	platform_set_drvdata(pdev, common);

	if (of_parse_phandle(dev->of_node, "mediatek,cmdq", 0))
		kthr = kthread_run(smi_cmdq, dev, __func__);

	if (of_property_read_bool(dev->of_node, "init-power-on")) {
		ret = pm_runtime_get_sync(dev);
		init_power_on_dev[init_power_on_num++] = common;
		if (ret < 0) {
			dev_notice(dev, "Unable to enable SMI COMM%d. ret:%d\n",
				common->commid, ret);
			pm_runtime_put_sync(dev);
		}
	}

	return 0;
}

static int mtk_smi_common_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int __maybe_unused mtk_smi_common_resume(struct device *dev)
{
	struct mtk_smi *common = dev_get_drvdata(dev);
	u32 bus_sel = common->plat->bus_sel;
	int i, ret;

	ret = mtk_smi_clk_enable(common);
	if (ret) {
		dev_err(common->dev, "Failed to enable clock(%d).\n", ret);
		return ret;
	}

	atomic_inc(&common->ref_count);
	if ((common->plat->gen == MTK_SMI_GEN2 || common->plat->gen == MTK_SMI_GEN3)
		&& bus_sel)
		writel(bus_sel, common->base + SMI_BUS_SEL);
	if ((common->plat->gen != MTK_SMI_GEN2 && common->plat->gen != MTK_SMI_GEN3)
		|| !common->plat->has_bwl)
		return 0;

	if (common->plat->gen == MTK_SMI_GEN3) {
		for (i = 0; i < SMI_COMMON_LARB_NR_MAX; i++)
			writel_relaxed(
				common->plat->bwl[common->commid * SMI_COMMON_LARB_NR_MAX + i],
				common->base + SMI_L1ARB(i));
		for (i = 0; i < SMI_COMMON_MISC_NR; i++)
			writel_relaxed(common->plat->misc[
				common->commid * SMI_COMMON_MISC_NR + i].value,
				common->base + common->plat->misc[
				common->commid * SMI_COMMON_MISC_NR + i].offset);

	} else {
		for (i = 0; i < SMI_COMMON_LARB_NR_MAX; i++)
			writel_relaxed(common->plat->bwl[i],
				common->base + SMI_L1ARB(i));
		for (i = 0; i < SMI_COMMON_MISC_NR; i++)
			writel_relaxed(common->plat->misc[i].value,
				common->base + common->plat->misc[i].offset);
	}
	wmb(); /* make sure settings are written */

	return 0;
}

static int __maybe_unused mtk_smi_common_suspend(struct device *dev)
{
	struct mtk_smi *common = dev_get_drvdata(dev);

	if (!(readl_relaxed(common->base + SMI_DEBUG_MISC) & 0x1)) {
		pr_notice("[SMI]common:%d suspend but busy\n", common->commid);
		raw_notifier_call_chain(&smi_driver_notifier_list, common->commid, NULL);
	}

	mtk_smi_clk_disable(common);
	atomic_dec(&common->ref_count);

	if (atomic_read(&common->ref_count)
		|| atomic_read(&common->api_ref_count)) {
		dev_notice(dev,
			"Error: comm(%d) ref_count=%d api_ref_count=%d on suspend\n",
			common->commid, atomic_read(&common->ref_count),
			atomic_read(&common->api_ref_count));
	}

	return 0;
}

static const struct dev_pm_ops smi_common_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_smi_common_suspend, mtk_smi_common_resume, NULL)
};

static struct platform_driver mtk_smi_common_driver = {
	.probe	= mtk_smi_common_probe,
	.remove = mtk_smi_common_remove,
	.driver	= {
		.name = "mtk-smi-common",
		.of_match_table = mtk_smi_common_of_ids,
		.pm             = &smi_common_pm_ops,
	}
};

static const struct of_device_id mtk_smi_pd_of_ids[] = {
	{
		.compatible = "mediatek,smi-pd",
	},
	{}
};

static int mtk_smi_pd_probe(struct platform_device *pdev)
{
	struct mtk_smi_pd *smi_pd;
	struct device *dev = &pdev->dev;
	struct device_node *smi_node;
	struct platform_device *smi_pdev;
	int ret, i;
	u32 reset_tmp, reset_num, offset;

	smi_pd = devm_kzalloc(&pdev->dev, sizeof(*smi_pd), GFP_KERNEL);
	if (!smi_pd)
		return -ENOMEM;

	smi_pd->dev = dev;
	smi_pd->smi.dev = dev;

	if (of_property_read_bool(dev->of_node, "main-power"))
		smi_pd->is_main = true;

	if (of_property_read_bool(dev->of_node, "suspend-check"))
		smi_pd->suspend_check = true;

	for (i = 0; i < MAX_COMMON_FOR_CLAMP; i++) {
		smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", i);
		if (!smi_node)
			break;

		smi_pdev = of_find_device_by_node(smi_node);
		of_node_put(smi_node);
		if (smi_pdev) {
			if (!platform_get_drvdata(smi_pdev))
				return -EPROBE_DEFER;
			smi_pd->smi_common_dev[i] = &smi_pdev->dev;
		} else {
			dev_notice(dev, "Failed to get smi_comm dev for setting clamp:%d\n", i);
			return -EINVAL;
		}

		of_property_read_u32_index(dev->of_node, "mediatek,comm-port-range",
						i, &smi_pd->set_comm_port_range[i]);
	}

	for (i = 0; i < MAX_PD_CHECK_DEV_NUM; i++) {
		smi_node = of_parse_phandle(dev->of_node, "mediatek,suspend-check-dev", i);
		if (!smi_node)
			break;

		smi_pdev = of_find_device_by_node(smi_node);
		of_node_put(smi_node);
		if (smi_pdev) {
			if (!platform_get_drvdata(smi_pdev))
				return -EPROBE_DEFER;
			smi_pd->suspend_check_dev[i] = &smi_pdev->dev;
		} else {
			dev_notice(dev, "Failed to get smi_comm dev for suspend check:%d\n", i);
			return -EINVAL;
		}

		of_property_read_u32_index(dev->of_node, "mediatek,suspend-check-port",
						i, &smi_pd->suspend_check_port[i]);
	}

	if (of_get_property(dev->of_node, "power-reset", &reset_tmp)) {
		reset_num = reset_tmp / (sizeof(u32) * RESET_CELL_NUM);
		for (i = 0; i < reset_num; i++) {
			offset = i * RESET_CELL_NUM;
			if (of_property_read_u32_index(dev->of_node,
					"power-reset", offset, &reset_tmp))
				break;
			smi_pd->power_reset_pa[i] = reset_tmp;
			smi_pd->power_reset_reg[i] = ioremap(reset_tmp, 4);

			if (of_property_read_u32_index(dev->of_node,
					"power-reset", offset + 1, &reset_tmp))
				break;
			smi_pd->power_reset_value[i] = 1 << reset_tmp;
		}
	}

	smi_pd->nb.notifier_call = mtk_smi_pd_callback;
	ret = dev_pm_genpd_add_notifier(dev, &smi_pd->nb);
	pm_runtime_enable(dev);

	if (of_property_read_bool(dev->of_node, "init-power-on")) {
		ret = pm_runtime_get_sync(dev);
		init_power_on_dev[init_power_on_num++] = &smi_pd->smi;
		if (ret < 0) {
			dev_notice(dev, "Unable to enable disp power\n");
			pm_runtime_put_sync(dev);
		}
	}

	if (!smi_pd->suspend_check)
		smi_pd_ctrl[smi_pd_ctrl_num++] = smi_pd;

	return 0;
}

static struct platform_driver mtk_smi_pd_driver = {
	.probe	= mtk_smi_pd_probe,
	.driver	= {
		.name = "mtk-smi-pd",
		.of_match_table = mtk_smi_pd_of_ids,
	}
};

static struct platform_driver * const smidrivers[] = {
	&mtk_smi_common_driver,
	&mtk_smi_pd_driver,
	&mtk_smi_larb_driver,
};

static int __init mtk_smi_init(void)
{
	return platform_register_drivers(smidrivers, ARRAY_SIZE(smidrivers));
}
module_init(mtk_smi_init);

static void __exit mtk_smi_exit(void)
{
	platform_unregister_drivers(smidrivers, ARRAY_SIZE(smidrivers));
}
module_exit(mtk_smi_exit);

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "smi log level");

MODULE_DESCRIPTION("MediaTek SMI driver");
MODULE_LICENSE("GPL v2");
