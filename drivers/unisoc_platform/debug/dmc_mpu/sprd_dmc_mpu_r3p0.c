/*
 *copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#define REG_PUB_APB_DMC_PORTS_MPU_SEL			0x31b4
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_SEL		0x31b8
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_RID_MASK	0x31bC
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_WID_MASK	0x31c0
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_RID_VAL	0x31c4
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_WID_VAL	0x31c8
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_RHIGH_RANGE	0x31cc
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_RLOW_RANGE	0x31d0
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_WHIGH_RANGE	0x31d4
#define REG_PUB_APB_DMC_PORTS_MPU_SHARED_WLOW_RANGE	0x31d8
#define REG_PUB_APB_DMC_PORTS_MPU_CH0_RID_MASK_VAL	0x31dc
#define REG_PUB_APB_DMC_PORTS_MPU_CH0_WID_MASK_VAL	0x31e0
#define REG_PUB_APB_DMC_PORT_MPU_CH0_RLOW_RANGE		0x321c
#define REG_PUB_APB_DMC_PORT_MPU_CH0_WLOW_RANGE		0x3220
#define REG_PUB_APB_DMC_PORT_MPU_CH0_RHIGH_RANGE	0x325c
#define REG_PUB_APB_DMC_PORT_MPU_CH0_WHIGH_RANGE	0x3260
#define REG_PUB_APB_DMC_MPU_VIO_ADDR			0x329c
#define REG_PUB_APB_DMC_MPU_VIO_CMD			0x32a0
#define REG_PUB_APB_DMC_MPU_VIO_USERID			0x32a4
#define REG_PUB_APB_DMC_MPU_DUMP_ADDR			0x32e4
#define REG_PUB_APB_PUB_DMC_MPU_INT			0x32e8

#define SPRD_MPU_RID_MASK_VAL(n) \
	(REG_PUB_APB_DMC_PORTS_MPU_CH0_RID_MASK_VAL + (n) * 0x8)
#define SPRD_MPU_WID_MASK_VAL(n) \
	(REG_PUB_APB_DMC_PORTS_MPU_CH0_WID_MASK_VAL + (n) * 0x8)
#define SPRD_MPU_RLOW_RANGE(n) \
	(REG_PUB_APB_DMC_PORT_MPU_CH0_RLOW_RANGE + (n) * 0x8)
#define SPRD_MPU_WLOW_RANGE(n) \
	(REG_PUB_APB_DMC_PORT_MPU_CH0_WLOW_RANGE + (n) * 0x8)
#define SPRD_MPU_RHIGH_RANGE(n) \
	(REG_PUB_APB_DMC_PORT_MPU_CH0_RHIGH_RANGE + (n) * 0x8)
#define SPRD_MPU_WHIGH_RANGE(n) \
	(REG_PUB_APB_DMC_PORT_MPU_CH0_WHIGH_RANGE + (n) * 0x8)

#define SPRD_MPU_VIO_ADDR \
	REG_PUB_APB_DMC_MPU_VIO_ADDR
#define SPRD_MPU_VIO_CMD \
	REG_PUB_APB_DMC_MPU_VIO_CMD
#define SPRD_MPU_VIO_UID \
	REG_PUB_APB_DMC_MPU_VIO_USERID
#define SPRD_MPU_INT		REG_PUB_APB_PUB_DMC_MPU_INT
#define SPRD_MPU_SEL		REG_PUB_APB_DMC_PORTS_MPU_SEL
#define SPRD_MPU_SHARED_SEL \
	REG_PUB_APB_DMC_PORTS_MPU_SHARED_SEL

#define SPRD_MPU_VIO_MPU_ID(cmd) \
		(((cmd) & GENMASK(31, 16)) >>  16)
#define SPRD_MPU_VIO_PORT(cmd)	(((cmd) & GENMASK(15, 12)) >> 12)
#define SPRD_MPU_VIO_BURST(cmd)	(((cmd) & GENMASK(9, 8)) >> 8)
#define SPRD_MPU_VIO_WR(cmd)	(((cmd) & BIT(7)) >> 7)
#define SPRD_MPU_VIO_SIZE(cmd)	(((cmd) & GENMASK(6, 4)) >> 4)
#define SPRD_MPU_VIO_LEN(cmd)	((cmd) & GENMASK(3, 0))

#define SPRD_MPU_ID_MASK_VAL(mask, id) \
		((((mask) & GENMASK(15, 0)) << 16) | ((id) & GENMASK(15, 0)))
#define SPRD_MPU_SHARED_ID_TYPE(id)	((id) << 2)

#define BIT_PUB_APB_DMC_MPU_VIO_INT_EN         BIT(0)
#define BIT_PUB_APB_MPU_SHARED_EN              BIT(0)
#define BIT_PUB_APB_MPU_EN                     BIT(0)
#define SPRD_MPU_VIO_INT_EN		BIT(0)
#define SPRD_MPU_VIO_INT_CLR		BIT(0)
#define BIT_PUB_APB_DMC_MPU_VIO_INT_CLR        BIT(1)
#define BIT_PUB_APB_MPU_SHARED_SEL             BIT(1)
#define BIT_PUB_APB_MPU_SHARED_USRID_SEL       BIT(2)
#define SPRD_MPU_DUMP_SIZE		(128 << 4)
#define SPRD_MPU_PRE_WORD		4
#define SPRD_MPU_BURST(a, b)		((((a) * (b)) >> SPRD_MPU_PRE_WORD) + 1)
#define BIT_PUB_APB_MPU_SEL(x)                 (((x) & 0xFF) << 9)
#define BIT_PUB_APB_MPU_PORT_EN(x)             (((x) & 0xFF) << 1)
#define BIT_PUB_APB_MPU_SHARED_PORT(x)         (((x) & 0xF) << 3)
#define BIT_PUB_APB_USRID_SEL(x)               (((x) & 0xFF) << 17)
#define SPRD_MPU_W_MODE		0x1
#define SPRD_MPU_R_MODE		0x2

#define SPRD_MPU_CHN_PROP_SIZE		3
#define SPRD_MPU_ID_PROP_SIZE		3
#define SPRD_MPU_RANGE_ADDRS_SIZE		2
#define SPRD_MPU_CHN_CFG_CELL_SIZE		3
#define SPRD_MPU_MAX_ADDR		GENMASK(31, 0)

struct sprd_dmc_mpu;
struct sprd_channel_cfg {
	/* sprd,chn_property */
	u32 en;
	u32 include;
	 /* 0x1 W_MODE, 0x2 R_MODE, RW 0x3 */
	u32 mode;
	/* sprd,id_property */
	/* Chose record id type matser id or userid */
	u32 id_type;
	u32 userid;
	u32 id_mask;
	/* sprd,range_addrs */
	u32  addr_start;
	u32  addr_end;
};

struct sprd_mpu_violate_info {
	u32 addr;
	u32 mpu_id;
	u32 mpu_port;
	u32 mpu_burst;
	bool mpu_wr;
	u32 mpu_size;
	u32 mpu_len;
	u32 userid;
};

struct sprd_mpu_info {
	struct sprd_dmc_mpu *dmc_mpu;
	u32 pub_id;
	int pub_irq;
	void __iomem *pub_apb;
	dma_addr_t dump_paddr;
	void *dump_vaddr;
	struct sprd_mpu_violate_info info;
};

struct sprd_dmc_mpu {
	struct device *dev;
	struct miscdevice misc;
	u32 chn_num;
	bool interleaved;
	u32 ddr_addr_offset;
	bool panic;
	const char **channel;
	u32 shared_chan;
	u32 irq_count;
	struct sprd_mpu_info *mpu_info;
	struct sprd_channel_cfg *cfg;
};

static int sprd_dmc_mpu_enable(struct sprd_dmc_mpu *sprd_mpu,
			       bool enable)
{
	struct sprd_mpu_info *mpu_info;
	/* share channel config */
	struct sprd_channel_cfg *cfg =
		&sprd_mpu->cfg[sprd_mpu->chn_num - 1];
	u32 val;
	int i;

	for (i = 0; i <= sprd_mpu->interleaved; i++) {
		mpu_info = &sprd_mpu->mpu_info[i];
		val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SEL);
		if (enable)
			writel_relaxed(val | BIT_PUB_APB_MPU_EN,
				       mpu_info->pub_apb + SPRD_MPU_SEL);
		else
			writel_relaxed(val & ~BIT_PUB_APB_MPU_EN,
					mpu_info->pub_apb + SPRD_MPU_SEL);

		val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);
		if (cfg->en) {
			if (enable)
				writel_relaxed(val | BIT_PUB_APB_MPU_SHARED_EN,
					mpu_info->pub_apb +
					SPRD_MPU_SHARED_SEL);
			else
				writel_relaxed(val & ~BIT_PUB_APB_MPU_SHARED_EN,
					mpu_info->pub_apb +
					SPRD_MPU_SHARED_SEL);
		}
	}

	return 0;
}

static void sprd_dmc_mpu_clr_irq(struct sprd_mpu_info *mpu_info)
{
	u32 val;

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_INT);
	writel_relaxed(val | BIT_PUB_APB_DMC_MPU_VIO_INT_CLR,
		mpu_info->pub_apb + SPRD_MPU_INT);

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_INT);
	writel_relaxed(val & ~BIT_PUB_APB_DMC_MPU_VIO_INT_CLR,
			mpu_info->pub_apb + SPRD_MPU_INT);
}

static void sprd_dmc_mpu_irq_enable(struct sprd_dmc_mpu *sprd_mpu)
{
	struct sprd_mpu_info *mpu_info = sprd_mpu->mpu_info;
	u32 val;
	int i;

	for (i = 0; i <= sprd_mpu->interleaved; i++) {
		val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_INT);
		writel_relaxed(val | BIT_PUB_APB_DMC_MPU_VIO_INT_EN,
		       mpu_info[i].pub_apb + SPRD_MPU_INT);
	}
}

static void sprd_dmc_mpu_irq_disable(struct sprd_dmc_mpu *sprd_mpu)
{
	struct sprd_mpu_info *mpu_info = sprd_mpu->mpu_info;
	u32 val;
	int i;

	for (i = 0; i <= sprd_mpu->interleaved; i++) {
		val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_INT);
		writel_relaxed(val & ~BIT_PUB_APB_DMC_MPU_VIO_INT_EN,
		       mpu_info->pub_apb + SPRD_MPU_INT);
	}
}

static void sprd_dmc_mpu_vio_cmd(struct sprd_mpu_info *mpu_info)
{
	struct sprd_mpu_violate_info *info = &mpu_info->info;
	u32 vio_cmd = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_VIO_CMD);

	info->mpu_id = SPRD_MPU_VIO_MPU_ID(vio_cmd);
	info->mpu_port = SPRD_MPU_VIO_PORT(vio_cmd);
	info->mpu_wr = SPRD_MPU_VIO_WR(vio_cmd);
	info->mpu_size = SPRD_MPU_VIO_SIZE(vio_cmd);
	info->mpu_len = SPRD_MPU_VIO_LEN(vio_cmd);
	info->mpu_burst = SPRD_MPU_VIO_BURST(vio_cmd);
	info->userid = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_VIO_UID);
	info->addr = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_VIO_ADDR);
}

static irqreturn_t sprd_dmc_mpu_irq(int irq_num, void *dev)
{
	struct sprd_mpu_info *mpu_info = (struct sprd_mpu_info *) dev;
	struct sprd_mpu_violate_info *info = &mpu_info->info;
	struct sprd_dmc_mpu *sprd_mpu = mpu_info->dmc_mpu;
	u32 *dump_vaddr = (u32 *)mpu_info->dump_vaddr;
	u32 len;
	int i;

	if (!sprd_mpu)
		return IRQ_RETVAL(-EINVAL);

	sprd_dmc_mpu_clr_irq(mpu_info);
	sprd_dmc_mpu_vio_cmd(mpu_info);
	len = SPRD_MPU_BURST(info->mpu_len, info->mpu_size);
	dev_emerg(sprd_mpu->dev,
		  "warning! dmc mpu detected violated transaction!!!\n");
	dev_emerg(sprd_mpu->dev, "pub%d: chn%d: %s\n", mpu_info->pub_id,
		  info->mpu_port, sprd_mpu->channel[info->mpu_port]);
	dev_emerg(sprd_mpu->dev, "%s: 0x%08X - mpuid: 0x%08X userid:0x%08X\n",
		  info->mpu_wr ? "waddr" : "raddr",
		  info->addr + sprd_mpu->ddr_addr_offset,
		  info->mpu_id, info->userid);
	dev_emerg(sprd_mpu->dev, "data:\n");
	for (i = 0; i < len; i++)
		dev_emerg(sprd_mpu->dev, "0x%08X\n", dump_vaddr[i]);

	if (sprd_mpu->panic)
		BUG_ON(1);

	/* mpu clear interrupt info */
	sprd_dmc_mpu_enable(sprd_mpu, false);
	sprd_dmc_mpu_enable(sprd_mpu, true);
	sprd_mpu->irq_count++;
	return IRQ_HANDLED;
}

static int
sprd_dmc_mpu_config_channel(struct sprd_dmc_mpu *sprd_mpu,
			    struct sprd_mpu_info *mpu_info, u32 n)
{
	struct sprd_channel_cfg *cfg = sprd_mpu->cfg;
	u32 raddr_min = cfg[n].include ? SPRD_MPU_MAX_ADDR : 0;
	u32 raddr_max = cfg[n].include ? 0 : SPRD_MPU_MAX_ADDR;
	u32 waddr_min = raddr_min, waddr_max = raddr_max;
	u32 val;

	if (cfg[n].addr_start > cfg[n].addr_end ||
	    cfg[n].addr_start < sprd_mpu->ddr_addr_offset) {
		dev_err(sprd_mpu->dev,
			"channel[%d] config address fail\n", n);
		return -EINVAL;
	}

	if (cfg[n].mode & SPRD_MPU_W_MODE) {
		waddr_min = cfg[n].addr_start - sprd_mpu->ddr_addr_offset;
		waddr_max = cfg[n].addr_end - sprd_mpu->ddr_addr_offset;
		writel_relaxed(SPRD_MPU_ID_MASK_VAL(cfg[n].id_mask,
						    cfg[n].userid),
			       mpu_info->pub_apb + SPRD_MPU_WID_MASK_VAL(n));
	}

	if (cfg[n].mode & SPRD_MPU_R_MODE) {
		raddr_min = cfg[n].addr_start - sprd_mpu->ddr_addr_offset;
		raddr_max = cfg[n].addr_end - sprd_mpu->ddr_addr_offset;
		writel_relaxed(SPRD_MPU_ID_MASK_VAL(cfg[n].id_mask,
						    cfg[n].userid),
			       mpu_info->pub_apb + SPRD_MPU_RID_MASK_VAL(n));
	}

	writel_relaxed(waddr_min, mpu_info->pub_apb + SPRD_MPU_WLOW_RANGE(n));
	writel_relaxed(waddr_max, mpu_info->pub_apb + SPRD_MPU_WHIGH_RANGE(n));
	writel_relaxed(raddr_min, mpu_info->pub_apb + SPRD_MPU_RLOW_RANGE(n));
	writel_relaxed(raddr_max, mpu_info->pub_apb + SPRD_MPU_RHIGH_RANGE(n));

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SEL);
	if (cfg[n].include)
		writel_relaxed(val | BIT_PUB_APB_MPU_SEL(BIT(n)),
			       mpu_info->pub_apb + SPRD_MPU_SEL);
	else
		writel_relaxed(val & ~BIT_PUB_APB_MPU_SEL(BIT(n)),
			       mpu_info->pub_apb + SPRD_MPU_SEL);

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SEL);
	if (cfg[n].id_type)
		writel_relaxed(val | BIT_PUB_APB_USRID_SEL(BIT(n)),
			       mpu_info->pub_apb + SPRD_MPU_SEL);
	else
		writel_relaxed(val & ~BIT_PUB_APB_USRID_SEL(BIT(n)),
			       mpu_info->pub_apb + SPRD_MPU_SEL);

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SEL);
	writel_relaxed(val | BIT_PUB_APB_MPU_PORT_EN(BIT(n)),
		       mpu_info->pub_apb + SPRD_MPU_SEL);

	return 0;
}

static int
sprd_dmc_mpu_config_shared_channel(struct sprd_dmc_mpu *sprd_mpu,
				   struct sprd_mpu_info *mpu_info, u32 n)
{
	struct sprd_channel_cfg *cfg = sprd_mpu->cfg;
	u32 raddr_min = cfg[n].include ? SPRD_MPU_MAX_ADDR : 0;
	u32 raddr_max = cfg[n].include ? 0 : SPRD_MPU_MAX_ADDR;
	u32 waddr_min = raddr_min, waddr_max = raddr_max;
	u32 val;

	if ((cfg[n].addr_start > cfg[n].addr_end) ||
	    (cfg[n].addr_start < sprd_mpu->ddr_addr_offset)) {
		dev_err(sprd_mpu->dev, "shared channel config address fail\n");
		return -EINVAL;
	}

	if (cfg[n].mode & SPRD_MPU_W_MODE) {
		waddr_min = cfg[n].addr_start - sprd_mpu->ddr_addr_offset;
		waddr_max = cfg[n].addr_end - sprd_mpu->ddr_addr_offset;
		writel_relaxed(cfg[n].id_mask & GENMASK(15, 0),
			       mpu_info->pub_apb +
			       REG_PUB_APB_DMC_PORTS_MPU_SHARED_WID_MASK);
		writel_relaxed(cfg[n].userid & GENMASK(15, 0),
			       mpu_info->pub_apb +
			       REG_PUB_APB_DMC_PORTS_MPU_SHARED_WID_VAL);
	}

	if (cfg[n].mode & SPRD_MPU_R_MODE) {
		raddr_min = cfg[n].addr_start - sprd_mpu->ddr_addr_offset;
		raddr_max = cfg[n].addr_end - sprd_mpu->ddr_addr_offset;
		writel_relaxed(cfg[n].id_mask & GENMASK(15, 0),
			       mpu_info->pub_apb +
			       REG_PUB_APB_DMC_PORTS_MPU_SHARED_RID_MASK);
		writel_relaxed(cfg[n].userid & GENMASK(15, 0),
			       mpu_info->pub_apb +
			       REG_PUB_APB_DMC_PORTS_MPU_SHARED_RID_VAL);
	}

	writel_relaxed(waddr_min, mpu_info->pub_apb +
		       REG_PUB_APB_DMC_PORTS_MPU_SHARED_WLOW_RANGE);
	writel_relaxed(waddr_max, mpu_info->pub_apb +
		       REG_PUB_APB_DMC_PORTS_MPU_SHARED_WHIGH_RANGE);
	writel_relaxed(raddr_min, mpu_info->pub_apb +
		       REG_PUB_APB_DMC_PORTS_MPU_SHARED_RLOW_RANGE);
	writel_relaxed(raddr_max, mpu_info->pub_apb +
		       REG_PUB_APB_DMC_PORTS_MPU_SHARED_RHIGH_RANGE);

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);
	val = val & ~BIT_PUB_APB_MPU_SHARED_PORT(0xf);
	writel_relaxed(val | BIT_PUB_APB_MPU_SHARED_PORT(sprd_mpu->shared_chan),
		       mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);
	if (cfg[n].id_type)
		writel_relaxed(val | BIT_PUB_APB_MPU_SHARED_USRID_SEL,
			       mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);
	else
		writel_relaxed(val & ~BIT_PUB_APB_MPU_SHARED_USRID_SEL,
			       mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);

	val = readl_relaxed(mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);
	if (cfg[n].include)
		writel_relaxed(val | BIT_PUB_APB_MPU_SHARED_SEL,
			       mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);
	else
		writel_relaxed(val & ~BIT_PUB_APB_MPU_SHARED_SEL,
			       mpu_info->pub_apb + SPRD_MPU_SHARED_SEL);

	return 0;
}

static void sprd_dmc_mpu_channel_dump_cfg(struct sprd_mpu_info *mpu_info)
{
	writel_relaxed(mpu_info->dump_paddr,
		       mpu_info->pub_apb + REG_PUB_APB_DMC_MPU_DUMP_ADDR);
}

static int sprd_dmc_mpu_channel_monitor_cfg(struct sprd_dmc_mpu *sprd_mpu,
					    struct sprd_mpu_info *mpu_info)
{
	struct sprd_channel_cfg *cfg = sprd_mpu->cfg;
	int i;

	for (i = 0; i < sprd_mpu->chn_num - 1; i++)
		if (cfg[i].en)
			sprd_dmc_mpu_config_channel(sprd_mpu, mpu_info, i);
	/* config shared channel */
	if (cfg[i].en)
		sprd_dmc_mpu_config_shared_channel(sprd_mpu, mpu_info, i);

	sprd_dmc_mpu_enable(sprd_mpu, true);
	return 0;
}

static int
sprd_dmc_mpu_get_cfg(struct platform_device *pdev,
		     u32 index, struct sprd_channel_cfg *cfg)
{
	int i, sz, cells_count[SPRD_MPU_CHN_CFG_CELL_SIZE] = {
		SPRD_MPU_CHN_PROP_SIZE,
		SPRD_MPU_ID_PROP_SIZE,
		SPRD_MPU_RANGE_ADDRS_SIZE};
	const char *channel_attrs[SPRD_MPU_CHN_CFG_CELL_SIZE] = {
		"sprd,chn-config", "sprd,id-config",
		"sprd,ranges"};
	u32 *val_p = (u32 *)cfg;
	struct property *prop;
	const __be32 *val;

	for (i = 0; i < SPRD_MPU_CHN_CFG_CELL_SIZE; i++) {
		prop = of_find_property(pdev->dev.of_node, channel_attrs[i],
			NULL);
		if (!prop) {
			dev_err(&pdev->dev, "get %s fail\n",
				channel_attrs[i]);
			return -EINVAL;
		}
		val = (const __be32 *)prop->value + index * cells_count[i];
		sz = cells_count[i];
		while (sz--)
			*val_p++ = be32_to_cpup(val++);
	}

	return 0;
}

static int
sprd_dmc_mpu_get_channel_cfg(struct platform_device *pdev,
			     struct sprd_dmc_mpu *sprd_mpu)
{
	struct sprd_channel_cfg *cfg = sprd_mpu->cfg;
	int ret, i;

	for (i = 0; i < sprd_mpu->chn_num; i++) {
		ret = sprd_dmc_mpu_get_cfg(pdev, i, &cfg[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int sprd_dmc_mpu_channel_init(struct platform_device *pdev,
				     struct sprd_dmc_mpu *sprd_mpu)
{
	const char *shared_name = pdev->name;
	u32 size;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, "sprd,channel-num",
				   &sprd_mpu->chn_num);
	if (ret) {
		dev_err(&pdev->dev, "get sprd,channel-names count fail\n");
		return ret;
	}

	dev_info(&pdev->dev, "sprd_mpu->chn_num = %d\n", sprd_mpu->chn_num);
	size = sizeof(*(sprd_mpu->channel)) * sprd_mpu->chn_num;
	sprd_mpu->channel = (const char **)devm_kzalloc(&pdev->dev,
							size, GFP_KERNEL);
	if (!sprd_mpu->channel)
		return -ENOMEM;

	ret = of_property_read_string_array(pdev->dev.of_node,
					    "sprd,channel-names",
					    sprd_mpu->channel,
					    sprd_mpu->chn_num);
	if (ret != sprd_mpu->chn_num) {
		dev_err(&pdev->dev, "get channel-names from dt failed\n");
		return ret;
	}

	size = sizeof(struct sprd_channel_cfg) * sprd_mpu->chn_num;
	sprd_mpu->cfg = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!sprd_mpu->cfg)
		return -ENOMEM;

	ret = sprd_dmc_mpu_get_channel_cfg(pdev, sprd_mpu);
	if (ret)
		return ret;

	ret = of_property_read_string(pdev->dev.of_node,
				      "sprd,shared-chn",
				      &shared_name);
	if (ret) {
		dev_err(&pdev->dev, "get shared_chn from dt failed\n");
		return ret;
	}

	ret = of_property_match_string(pdev->dev.of_node,
					"sprd,channel-names", shared_name);
	if (ret < 0)
		sprd_mpu->shared_chan = 0xf;
	else
		sprd_mpu->shared_chan = ret;

	ret = of_property_read_u32(pdev->dev.of_node, "sprd,ddr-offset",
				   &sprd_mpu->ddr_addr_offset);
	if (ret) {
		dev_err(&pdev->dev,
			"get sprd,ddr_offset value from dt failed\n");
		return ret;
	}

	return 0;
}

static ssize_t sprd_dmc_mpu_cfg_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct sprd_dmc_mpu *sprd_mpu = dev_get_drvdata(dev->parent);
	struct sprd_channel_cfg *cfg = sprd_mpu->cfg;
	int i, cnt = 0;

	for (i = 0; i < sprd_mpu->chn_num; i++) {
		if (!cfg[i].en) {
			cnt += sprintf(buf + cnt, "chn%d: closed\n", i);
			continue;
		}

		cnt += sprintf(buf + cnt, "chn%d:0x%08X ~ 0x%08X", i,
			       cfg[i].addr_start, cfg[i].addr_end);
		if (cfg[i].include)
			cnt += sprintf(buf + cnt, "  include");
		else
			cnt += sprintf(buf + cnt, "  exchule");

		if (cfg[i].mode == SPRD_MPU_W_MODE)
			cnt += sprintf(buf + cnt, "  W");
		else if (cfg[i].mode == SPRD_MPU_R_MODE)
			cnt += sprintf(buf + cnt, "  R");
		else
			cnt += sprintf(buf + cnt, "  RW");
		cnt += sprintf(buf + cnt,
			       "  %s userid: 0x%x mask: 0x%x\n",
			       cfg[i].id_type ? "userid" : "mpuid",
			       cfg[i].userid, cfg[i].id_mask);
	}

	cnt += sprintf(buf + cnt, "shared chn:%x\n", sprd_mpu->shared_chan);
	cnt += sprintf(buf + cnt, "panic is %s\n",
		       sprd_mpu->panic ? "open" : "close");

	return cnt;
}

static ssize_t sprd_dmc_mpu_cfg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct sprd_dmc_mpu *sprd_mpu = dev_get_drvdata(dev->parent);
	struct sprd_channel_cfg *cfg = sprd_mpu->cfg;
	struct sprd_mpu_info *mpu_info = sprd_mpu->mpu_info;
	struct sprd_channel_cfg temp_cfg;
	u32 shared_chn = 0, shared_n = sprd_mpu->chn_num - 1;
	u32 pub, chn, ret, panic;

	ret = sscanf(buf, "%d %d %x %x %x %x %x %x %x %x %d",
		&pub, &chn, &temp_cfg.addr_start, &temp_cfg.addr_end,
		&temp_cfg.include, &temp_cfg.mode, &temp_cfg.id_type,
		&temp_cfg.userid, &temp_cfg.id_mask, &panic,
		&shared_chn);

	if (ret != 11) {
		dev_err(dev->parent,
			"enter wrong parameter numeber\n");
		return -EINVAL;
	}

	if (pub > sprd_mpu->interleaved) {
		dev_err(dev->parent,
			"enter wrong pub number parameter\n");
		return -EINVAL;
	}

	if (chn >= shared_n) {
		dev_err(dev->parent,
			"enter wrong channel number parameter\n");
		return -EINVAL;
	}

	if (temp_cfg.addr_start > temp_cfg.addr_end) {
		dev_err(dev->parent,
			"enter wrong address parameter\n");
		return -EINVAL;
	}

	if (temp_cfg.include > 1) {
		dev_err(dev->parent,
			"enter wrong include parameter\n");
		return -EINVAL;
	}

	if (!temp_cfg.mode || temp_cfg.mode >
	    (SPRD_MPU_W_MODE|SPRD_MPU_R_MODE)) {
		dev_err(dev->parent,
			"enter wrong mode parameter\n");
		return -EINVAL;
	}

	if (temp_cfg.id_type > 1) {
		dev_err(dev->parent,
			"enter wrong id_type parameter\n");
		return -EINVAL;
	}

	if (panic > 1) {
		dev_err(dev->parent,
			"enter wrong panic parameter\n");
		return -EINVAL;
	}

	if (shared_chn > 1) {
		dev_err(dev->parent,
			"enter wrong shared_chn parameter\n");
		return -EINVAL;
	}

	sprd_mpu->panic = panic;
	if (shared_chn) {
		cfg = cfg + shared_n;
		sprd_mpu->shared_chan = chn;
	} else {
		cfg = cfg + chn;
	}

	temp_cfg.en = 1;
	memcpy(cfg, &temp_cfg, sizeof(struct sprd_channel_cfg));

	sprd_dmc_mpu_enable(sprd_mpu, false);
	if (shared_chn)
		sprd_dmc_mpu_config_shared_channel(sprd_mpu, &mpu_info[pub],
						   shared_n);
	else
		sprd_dmc_mpu_config_channel(sprd_mpu, &mpu_info[pub], chn);

	sprd_dmc_mpu_enable(sprd_mpu, true);

	return strnlen(buf, count);
}

static ssize_t sprd_dmc_mpu_chn_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_dmc_mpu *sprd_mpu = dev_get_drvdata(dev->parent);
	int chn, cnt = 0;

	for (chn = 0; chn < sprd_mpu->chn_num; chn++)
		cnt += sprintf(buf + cnt, "%d: %s\n", chn,
			       sprd_mpu->channel[chn]);

	return cnt;
}

static ssize_t sprd_dmc_mpu_dump_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_dmc_mpu *sprd_mpu = dev_get_drvdata(dev->parent);
	struct sprd_mpu_info *mpu_info = sprd_mpu->mpu_info;
	struct sprd_mpu_violate_info *info = &mpu_info->info;
	u32 *dump_vaddr = (u32 *)mpu_info->dump_vaddr;
	int cnt = 0, i;

	if (!sprd_mpu->irq_count)
		return sprintf(buf,
			"dmc mpu do not detect violated transaction\n");

	cnt += sprintf(buf + cnt,
		       "warning! dmc mpu detected violated transaction!!!\n");
	cnt += sprintf(buf + cnt, "pub%d: chn%d: %s\n", mpu_info->pub_id,
		       info->mpu_port, sprd_mpu->channel[info->mpu_port]);
	cnt += sprintf(buf + cnt, "%s: 0x%08X -mpuid: 0x%08X userid:0x%08X\n",
		       info->mpu_wr ? "waddr" : "raddr",
		       info->addr + sprd_mpu->ddr_addr_offset,
		       info->mpu_id, info->userid);
	cnt += sprintf(buf + cnt, "data:\n");
	for (i = 0; i < info->mpu_len; i++)
		cnt += sprintf(buf + cnt, "0x%08X\n", dump_vaddr[i]);

	return cnt;
}

static ssize_t sprd_dmc_mpu_panic_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct sprd_dmc_mpu *sprd_mpu = dev_get_drvdata(dev->parent);

	if (sprd_mpu->panic)
		return sprintf(buf, "panic mode open!!!\n");
	else
		return sprintf(buf, "panic mode closed!!!\n");
}

static DEVICE_ATTR(config, S_IWUSR | S_IRUGO, sprd_dmc_mpu_cfg_show,
		   sprd_dmc_mpu_cfg_store);
static DEVICE_ATTR(channel, S_IRUSR | S_IRGRP, sprd_dmc_mpu_chn_show, NULL);
static DEVICE_ATTR(dump_scene, S_IRUSR | S_IRGRP, sprd_dmc_mpu_dump_show,
		   NULL);
static DEVICE_ATTR(panic, S_IRUSR | S_IRGRP, sprd_dmc_mpu_panic_show, NULL);

static struct attribute *dmc_mpu_attrs[] = {
	&dev_attr_config.attr,
	&dev_attr_channel.attr,
	&dev_attr_dump_scene.attr,
	&dev_attr_panic.attr,
	NULL,
};
static struct attribute_group dmc_mpu_group = {
	.attrs = dmc_mpu_attrs,
};

static int sprd_dmc_mpu_device_register(struct platform_device *pdev,
					struct sprd_dmc_mpu *sprd_mpu)
{
	int ret;

	sprd_mpu->misc.name = "dmc_mpu";
	sprd_mpu->misc.parent = &pdev->dev;
	sprd_mpu->misc.minor = MISC_DYNAMIC_MINOR;
	sprd_mpu->misc.fops = NULL;
	ret = misc_register(&sprd_mpu->misc);
	if (ret) {
		dev_err(&pdev->dev,
			"unable to register mpu misc device!\n");
		return ret;
	}

	ret = sysfs_create_group(&sprd_mpu->misc.this_device->kobj,
				 &dmc_mpu_group);
	if (ret) {
		dev_err(&pdev->dev,
			"unable to export dmc mpu sysfs\n");
		misc_deregister(&sprd_mpu->misc);
		return ret;
	}

	return 0;
}

static void sprd_dmc_mpu_dma_free(struct sprd_dmc_mpu *sprd_mpu,
				  struct sprd_mpu_info *mpu_info)
{
	int i;

	for (i = 0; i <= sprd_mpu->interleaved; i++)
		if (mpu_info[i].dump_vaddr)
			dma_free_coherent(sprd_mpu->dev, SPRD_MPU_DUMP_SIZE,
					  mpu_info[i].dump_vaddr,
					  mpu_info[i].dump_paddr);
}

static int sprd_dmc_mpu_probe(struct platform_device *pdev)
{
	const char *pub_name[2] = {"pub0_dmc_mpu", "pub1_dmc_mpu"};
	struct sprd_dmc_mpu *sprd_mpu;
	struct sprd_mpu_info *mpu_info;
	struct resource *res;
	bool interleaved;
	int i, ret;

	interleaved = of_property_read_bool(pdev->dev.of_node,
					    "sprd,ddr-interleaved");
	sprd_mpu = devm_kzalloc(&pdev->dev, sizeof(*sprd_mpu), GFP_KERNEL);
	if (!sprd_mpu)
		return -ENOMEM;
	sprd_mpu->dev = &pdev->dev;
	sprd_mpu->interleaved = interleaved;
	sprd_mpu->panic = of_property_read_bool(pdev->dev.of_node,
						"sprd,panic");
	ret = sprd_dmc_mpu_channel_init(pdev, sprd_mpu);
	if (ret)
		return ret;

	sprd_mpu->mpu_info =
		devm_kzalloc(&pdev->dev, sizeof(*mpu_info) << interleaved,
			     GFP_KERNEL);
	if (!sprd_mpu->mpu_info)
		return -ENOMEM;
	mpu_info = sprd_mpu->mpu_info;
	mpu_info->dmc_mpu = sprd_mpu;

	for (i = 0; i <= interleaved; i++) {
		mpu_info[i].pub_id = i;
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(&pdev->dev,
				"dmc mpu get io resource %d failed\n", i);
			ret = -ENODEV;
			goto dma_mem_release;
		}

		mpu_info[i].pub_apb =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mpu_info[i].pub_apb)) {
			ret = PTR_ERR(mpu_info[i].pub_apb);
			goto dma_mem_release;
		}

		mpu_info[i].pub_irq = platform_get_irq(pdev, i);
		if (mpu_info[i].pub_irq < 0) {
			dev_err(&pdev->dev,
				"can't get the pub%d mpu irq number\n", i);
			ret = mpu_info[i].pub_irq;
			goto dma_mem_release;
		}

		mpu_info[i].dump_vaddr =
			dma_alloc_coherent(&pdev->dev,
					   SPRD_MPU_DUMP_SIZE,
					   &mpu_info[i].dump_paddr,
					   GFP_KERNEL);
		if (!mpu_info[i].dump_vaddr) {
			ret = -ENOMEM;
			goto dma_mem_release;
		}

		sprd_dmc_mpu_channel_monitor_cfg(sprd_mpu, &mpu_info[i]);
		sprd_dmc_mpu_channel_dump_cfg(&mpu_info[i]);
		ret = devm_request_threaded_irq(&pdev->dev,
						mpu_info[i].pub_irq,
						sprd_dmc_mpu_irq, NULL,
						IRQF_TRIGGER_NONE, pub_name[i],
						&mpu_info[i]);
		if (ret) {
			dev_err(&pdev->dev,
				"can't request %s irq\n", pub_name[i]);
			goto dma_mem_release;
		}
	}

	ret = sprd_dmc_mpu_device_register(pdev, sprd_mpu);
	if (ret)
		goto dma_mem_release;

	platform_set_drvdata(pdev, sprd_mpu);
	sprd_dmc_mpu_irq_enable(sprd_mpu);

	return 0;

dma_mem_release:
	sprd_dmc_mpu_dma_free(sprd_mpu, mpu_info);

	return ret;
}

static int sprd_dmc_mpu_remove(struct platform_device *pdev)
{
	struct sprd_dmc_mpu *sprd_mpu = platform_get_drvdata(pdev);
	struct sprd_mpu_info *mpu_info = sprd_mpu->mpu_info;

	sprd_dmc_mpu_enable(sprd_mpu, false);
	sprd_dmc_mpu_irq_disable(sprd_mpu);
	sprd_dmc_mpu_dma_free(sprd_mpu, mpu_info);
	sysfs_remove_group(&sprd_mpu->misc.this_device->kobj,
		&dmc_mpu_group);
	misc_deregister(&sprd_mpu->misc);

	return 0;
}

static const struct of_device_id sprd_dmc_mpu_of_match[] = {
	{ .compatible = "sprd,dmc-mpu-r3p0", },
	{},
};
MODULE_DEVICE_TABLE(of, sprd_dmc_mpu_of_match);

static struct platform_driver sprd_dmc_mpu_driver = {
	.probe    = sprd_dmc_mpu_probe,
	.remove   = sprd_dmc_mpu_remove,
	.driver = {
		.name = "sprd-dmc-mpu",
		.of_match_table = sprd_dmc_mpu_of_match,
	},
};

module_platform_driver(sprd_dmc_mpu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobs Wang <bobs.wang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc platform dmc mpu driver");
