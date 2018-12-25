/*
 * MTK ECC controller driver.
 * Copyright (C) 2016  MediaTek Inc.
 * Authors:	Xiaolei Li		<xiaolei.li@mediatek.com>
 *		Jorge Ramirez-Ortiz	<jorge.ramirez-ortiz@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/mutex.h>

#include "mtk_ecc.h"

#define ECC_IDLE_MASK		BIT(0)
#define ECC_IRQ_EN		BIT(0)
#define	ECC_PG_IRQ_SEL		BIT(1)
#define ECC_OP_ENABLE		(1)
#define ECC_OP_DISABLE		(0)

#define _ECC_ENCCON		(0x00)
#define _ECC_ENCCNFG		(0x04)
#define		ECC_CNFG_4BIT		(0)
#define		ECC_CNFG_6BIT		(1)
#define		ECC_CNFG_8BIT		(2)
#define		ECC_CNFG_10BIT		(3)
#define		ECC_CNFG_12BIT		(4)
#define		ECC_CNFG_14BIT		(5)
#define		ECC_CNFG_16BIT		(6)
#define		ECC_CNFG_18BIT		(7)
#define		ECC_CNFG_20BIT		(8)
#define		ECC_CNFG_22BIT		(9)
#define		ECC_CNFG_24BIT		(0xa)
#define		ECC_CNFG_28BIT		(0xb)
#define		ECC_CNFG_32BIT		(0xc)
#define		ECC_CNFG_36BIT		(0xd)
#define		ECC_CNFG_40BIT		(0xe)
#define		ECC_CNFG_44BIT		(0xf)
#define		ECC_CNFG_48BIT		(0x10)
#define		ECC_CNFG_52BIT		(0x11)
#define		ECC_CNFG_56BIT		(0x12)
#define		ECC_CNFG_60BIT		(0x13)
#define		ECC_CNFG_68BIT		(0x14)
#define		ECC_CNFG_72BIT		(0x15)
#define		ECC_CNFG_80BIT		(0x16)
#define		ECC_MODE_SHIFT		(5)
#define		ECC_MS_SHIFT		(16)
#define _ECC_ENCDIADDR		(0x08)
#define _ECC_ENCIDLE		(0x0C)
#define _ECC_ENCPAR(x)		(0x10 + (x) * sizeof(u32))
#define _ECC_ENCIRQ_EN		(0x80)
#define _ECC_ENCIRQ_STA		(0x84)
#define _ECC_DECCON		(0x100)
#define _ECC_DECCNFG		(0x104)
#define		DEC_EMPTY_EN		BIT(31)
#define		DEC_CNFG_CORRECT	(0x3 << 12)
#define _ECC_DECIDLE		(0x10C)
#define _ECC_DECENUM0		(0x114)
#define		_ERR_MASK		(0x3f)
#define _ECC_DECDONE		(0x124)
#define _ECC_DECIRQ_EN		(0x200)
#define _ECC_DECIRQ_STA		(0x204)

#define ECC_TIMEOUT		(500000)

#define ECC_REG(ecc, x)		(ecc->regs + ecc->ecc_data->ecc_regs[x])
#define ECC_IDLE_REG(ecc, op)	((op) == ECC_ENCODE ? \
					ECC_REG(ecc, ECC_ENCIDLE) : \
					ECC_REG(ecc, ECC_DECIDLE))
#define ECC_CTL_REG(ecc, op)	((op) == ECC_ENCODE ? \
					ECC_REG(ecc, ECC_ENCCON) : \
					ECC_REG(ecc, ECC_DECCON))
#define ECC_IRQ_REG(ecc, op)	((op) == ECC_ENCODE ? \
					ECC_REG(ecc, ECC_ENCIRQ_EN) : \
					ECC_REG(ecc, ECC_DECIRQ_EN))

enum ecc_regs {
	ECC_ENCCON,
	ECC_ENCCNFG,
	ECC_ENCDIADDR,
	ECC_ENCIDLE,
	ECC_ENCSTA,
	ECC_ENCPAR00,
	ECC_ENCPAR01,
	ECC_ENCPAR02,
	ECC_ENCPAR03,
	ECC_ENCPAR04,
	ECC_ENCPAR05,
	ECC_ENCPAR06,
	ECC_ENCPAR07,
	ECC_ENCPAR08,
	ECC_ENCPAR09,
	ECC_ENCPAR10,
	ECC_ENCPAR11,
	ECC_ENCPAR12,
	ECC_ENCPAR13,
	ECC_ENCPAR14,
	ECC_ENCPAR15,
	ECC_ENCPAR16,
	ECC_ENCPAR17,
	ECC_ENCPAR18,
	ECC_ENCPAR19,
	ECC_ENCPAR20,
	ECC_ENCPAR21,
	ECC_ENCPAR22,
	ECC_ENCPAR23,
	ECC_ENCPAR24,
	ECC_ENCPAR25,
	ECC_ENCPAR26,
	ECC_ENCPAR27,
	ECC_ENCPAR28,
	ECC_ENCPAR29,
	ECC_ENCPAR30,
	ECC_ENCPAR31,
	ECC_ENCPAR32,
	ECC_ENCPAR33,
	ECC_ENCPAR34,
	ECC_ENCIRQ_EN,
	ECC_ENCIRQ_STA,
	ECC_PIO_DIRDY,
	ECC_PIO_DI,
	ECC_DECCON,
	ECC_DECCNFG,
	ECC_DECIDLE,
	ECC_DECENUM0,
	ECC_DECENUM1,
	ECC_DECENUM2,
	ECC_DECENUM3,
	ECC_DECDONE,
	ECC_DECIRQ_EN,
	ECC_DECIRQ_STA,
};

static int mt2701_ecc_regs[] = {
	[ECC_ENCCON] =		0x0,
	[ECC_ENCCNFG] =		0x4,
	[ECC_ENCDIADDR] =	0x8,
	[ECC_ENCIDLE] =		0xc,
	[ECC_ENCSTA] =		0x7c,
	[ECC_ENCPAR00] =	0x10,
	[ECC_ENCPAR01] =	0x14,
	[ECC_ENCPAR02] =	0x18,
	[ECC_ENCPAR03] =	0x1c,
	[ECC_ENCPAR04] =	0x20,
	[ECC_ENCPAR05] =	0x24,
	[ECC_ENCPAR06] =	0x28,
	[ECC_ENCPAR07] =	0x2c,
	[ECC_ENCPAR08] =	0x30,
	[ECC_ENCPAR09] =	0x34,
	[ECC_ENCPAR10] =	0x38,
	[ECC_ENCPAR11] =	0x3c,
	[ECC_ENCPAR12] =	0x40,
	[ECC_ENCPAR13] =	0x44,
	[ECC_ENCPAR14] =	0x48,
	[ECC_ENCPAR15] =	0x4c,
	[ECC_ENCPAR16] =	0x50,
	[ECC_ENCPAR17] =	0x54,
	[ECC_ENCPAR18] =	0x58,
	[ECC_ENCPAR19] =	0x5c,
	[ECC_ENCPAR20] =	0x60,
	[ECC_ENCPAR21] =	0x64,
	[ECC_ENCPAR22] =	0x68,
	[ECC_ENCPAR23] =	0x6c,
	[ECC_ENCPAR24] =	0x70,
	[ECC_ENCPAR25] =	0x74,
	[ECC_ENCPAR26] =	0x78,
	[ECC_ENCIRQ_EN] =	0x80,
	[ECC_ENCIRQ_STA] =	0x84,
	[ECC_PIO_DIRDY] =	0x90,
	[ECC_PIO_DI] =		0x94,
	[ECC_DECCON] =		0x100,
	[ECC_DECCNFG] =		0x104,
	[ECC_DECIDLE] =		0x10c,
	[ECC_DECENUM0] =	0x114,
	[ECC_DECENUM1] =	0x118,
	[ECC_DECENUM2] =	0x11c,
	[ECC_DECENUM3] =	0x120,
	[ECC_DECDONE] =		0x124,
	[ECC_DECIRQ_EN] =	0x200,
	[ECC_DECIRQ_STA] =	0x204,
};

static int mt7622_ecc_regs[] = {
	[ECC_ENCCON] =		0x0,
	[ECC_ENCCNFG] =		0x4,
	[ECC_ENCDIADDR] =	0x8,
	[ECC_ENCIDLE] =		0xc,
	[ECC_ENCPAR00] =	0x10,
	[ECC_ENCPAR01] =	0x14,
	[ECC_ENCPAR02] =	0x18,
	[ECC_ENCPAR03] =	0x1c,
	[ECC_ENCPAR04] =	0x20,
	[ECC_ENCPAR05] =	0x24,
	[ECC_ENCPAR06] =	0x28,
	[ECC_ENCIRQ_EN] =	0x30,
	[ECC_ENCIRQ_STA] =	0x34,
	[ECC_PIO_DIRDY] =	0x80,
	[ECC_PIO_DI] =		0x84,
	[ECC_DECCON] =		0x100,
	[ECC_DECCNFG] =		0x104,
	[ECC_DECIDLE] =		0x10c,
	[ECC_DECENUM0] =	0x114,
	[ECC_DECENUM1] =	0x118,
	[ECC_DECDONE] =		0x11c,
	[ECC_DECIRQ_EN] =	0x140,
	[ECC_DECIRQ_STA] =	0x144,
};

static int mt2712_ecc_regs[] = {
	[ECC_ENCCON] =          0x0,
	[ECC_ENCCNFG] =         0x4,
	[ECC_ENCDIADDR] =       0x8,
	[ECC_ENCIDLE] =         0xc,
	[ECC_ENCSTA] =		0x7c,
	[ECC_ENCPAR00] =        0x300,
	[ECC_ENCPAR01] =        0x304,
	[ECC_ENCPAR02] =        0x308,
	[ECC_ENCPAR03] =        0x30c,
	[ECC_ENCPAR04] =        0x310,
	[ECC_ENCPAR05] =        0x314,
	[ECC_ENCPAR06] =        0x318,
	[ECC_ENCPAR07] =        0x31c,
	[ECC_ENCPAR08] =        0x320,
	[ECC_ENCPAR09] =        0x324,
	[ECC_ENCPAR10] =        0x328,
	[ECC_ENCPAR11] =        0x32c,
	[ECC_ENCPAR12] =        0x330,
	[ECC_ENCPAR13] =        0x334,
	[ECC_ENCPAR14] =        0x338,
	[ECC_ENCPAR15] =        0x33c,
	[ECC_ENCPAR16] =        0x340,
	[ECC_ENCPAR17] =        0x344,
	[ECC_ENCPAR18] =        0x348,
	[ECC_ENCPAR19] =        0x34c,
	[ECC_ENCPAR20] =        0x350,
	[ECC_ENCPAR21] =        0x354,
	[ECC_ENCPAR22] =        0x358,
	[ECC_ENCPAR23] =        0x35c,
	[ECC_ENCPAR24] =        0x360,
	[ECC_ENCPAR25] =        0x364,
	[ECC_ENCPAR26] =        0x368,
	[ECC_ENCPAR27] =        0x36c,
	[ECC_ENCPAR28] =        0x370,
	[ECC_ENCPAR29] =        0x374,
	[ECC_ENCPAR30] =        0x378,
	[ECC_ENCPAR31] =        0x37c,
	[ECC_ENCPAR32] =        0x380,
	[ECC_ENCPAR33] =        0x384,
	[ECC_ENCPAR34] =        0x388,
	[ECC_ENCIRQ_EN] =       0x80,
	[ECC_ENCIRQ_STA] =      0x84,
	[ECC_PIO_DIRDY] =       0x90,
	[ECC_PIO_DI] =          0x94,
	[ECC_DECCON] =          0x100,
	[ECC_DECCNFG] =         0x104,
	[ECC_DECIDLE] =         0x10c,
	[ECC_DECENUM0] =        0x114,
	[ECC_DECENUM1] =        0x118,
	[ECC_DECENUM2] =        0x11c,
	[ECC_DECENUM3] =        0x120,
	[ECC_DECDONE] =         0x124,
	[ECC_DECIRQ_EN] =       0x200,
	[ECC_DECIRQ_STA] =      0x204,
};
enum mtk_ecc_type {
	MTK_ECC_MT2701,
	MTK_ECC_MT7622,
	MTK_ECC_MT2712,
};

struct mtk_ecc_comp {
	int *ecc_regs;
	enum mtk_ecc_type type;
	int mode_shift;
	int max_ecc_str;
	int parity_bit;
	/* ECC_DECENUMx register bit definition */
	int ERR_MASK;

	int pg_irq_enable;
};

struct mtk_ecc {
	struct device *dev;
	void __iomem *regs;
	struct clk *clk;

	struct mtk_ecc_comp *ecc_data;

	struct completion done;
	struct mutex lock;
	u32 sectors;
};

static inline void mtk_ecc_wait_idle(struct mtk_ecc *ecc,
				     enum mtk_ecc_operation op)
{
	struct device *dev = ecc->dev;
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(ECC_IDLE_REG(ecc, op), val,
					val & ECC_IDLE_MASK,
					10, ECC_TIMEOUT);

	if (ret)
		dev_warn(dev, "%s NOT idle\n",
			 op == ECC_ENCODE ? "encoder" : "decoder");
}

static irqreturn_t mtk_ecc_irq(int irq, void *id)
{
	struct mtk_ecc *ecc = id;
	enum mtk_ecc_operation op;
	u32 dec, enc;

	dec = readw(ECC_REG(ecc, ECC_DECIRQ_STA)) & ECC_IRQ_EN;
	if (dec) {
		op = ECC_DECODE;
		dec = readw(ECC_REG(ecc, ECC_DECDONE));
		if (dec & ecc->sectors) {
			/*
			 * Clear decode IRQ status once again to ensure that
			 * there will be no extra IRQ.
			 */
			readw(ecc->regs + ECC_DECIRQ_STA);
			ecc->sectors = 0;
			complete(&ecc->done);
		} else {
			return IRQ_HANDLED;
		}
	} else {
		enc = readl(ECC_REG(ecc, ECC_ENCIRQ_STA)) & ECC_IRQ_EN;
		if (enc) {
			op = ECC_ENCODE;
			complete(&ecc->done);
		} else {
			return IRQ_NONE;
		}
	}

	return IRQ_HANDLED;
}

static void mtk_ecc_config(struct mtk_ecc *ecc, struct mtk_ecc_config *config)
{
	u32 ecc_bit = ECC_CNFG_4BIT, dec_sz, enc_sz;
	u32 reg;

	switch (config->strength) {
	case 4:
		ecc_bit = ECC_CNFG_4BIT;
		break;
	case 6:
		ecc_bit = ECC_CNFG_6BIT;
		break;
	case 8:
		ecc_bit = ECC_CNFG_8BIT;
		break;
	case 10:
		ecc_bit = ECC_CNFG_10BIT;
		break;
	case 12:
		ecc_bit = ECC_CNFG_12BIT;
		break;
	case 14:
		ecc_bit = ECC_CNFG_14BIT;
		break;
	case 16:
		ecc_bit = ECC_CNFG_16BIT;
		break;
	case 18:
		ecc_bit = ECC_CNFG_18BIT;
		break;
	case 20:
		ecc_bit = ECC_CNFG_20BIT;
		break;
	case 22:
		ecc_bit = ECC_CNFG_22BIT;
		break;
	case 24:
		ecc_bit = ECC_CNFG_24BIT;
		break;
	case 28:
		ecc_bit = ECC_CNFG_28BIT;
		break;
	case 32:
		ecc_bit = ECC_CNFG_32BIT;
		break;
	case 36:
		ecc_bit = ECC_CNFG_36BIT;
		break;
	case 40:
		ecc_bit = ECC_CNFG_40BIT;
		break;
	case 44:
		ecc_bit = ECC_CNFG_44BIT;
		break;
	case 48:
		ecc_bit = ECC_CNFG_48BIT;
		break;
	case 52:
		ecc_bit = ECC_CNFG_52BIT;
		break;
	case 56:
		ecc_bit = ECC_CNFG_56BIT;
		break;
	case 60:
		ecc_bit = ECC_CNFG_60BIT;
		break;
	case 68:
		ecc_bit = ECC_CNFG_68BIT;
		break;
	case 72:
		ecc_bit = ECC_CNFG_72BIT;
		break;
	case 80:
		ecc_bit = ECC_CNFG_80BIT;
		break;
	default:
		dev_err(ecc->dev, "invalid strength %d, default to 4 bits\n",
			config->strength);
	}

	if (ecc_bit > ecc->ecc_data->max_ecc_str)
		ecc_bit = ecc->ecc_data->max_ecc_str;

	if (config->op == ECC_ENCODE) {
		/* configure ECC encoder (in bits) */
		enc_sz = config->len << 3;

		if (ecc->ecc_data->mode_shift)
			reg = ecc_bit | (config->mode << ecc->ecc_data->mode_shift);
		else
			reg = ecc_bit | (config->mode << ECC_MODE_SHIFT);
		reg |= (enc_sz << ECC_MS_SHIFT);
		writel(reg, ECC_REG(ecc, ECC_ENCCNFG));

		if (config->mode != ECC_NFI_MODE)
			writel(lower_32_bits(config->addr),
			       ECC_REG(ecc, ECC_ENCDIADDR));

	} else {
		/* configure ECC decoder (in bits) */
		dec_sz = (config->len << 3) +
					config->strength * ecc->ecc_data->parity_bit;

		if (ecc->ecc_data->mode_shift)
			reg = ecc_bit | (config->mode << ecc->ecc_data->mode_shift);
		else
			reg = ecc_bit | (config->mode << ECC_MODE_SHIFT);
		reg |= (dec_sz << ECC_MS_SHIFT) | DEC_CNFG_CORRECT;
		reg |= DEC_EMPTY_EN;
		writel(reg, ECC_REG(ecc, ECC_DECCNFG));

		if (config->sectors)
			ecc->sectors = 1 << (config->sectors - 1);
	}
}

void mtk_ecc_get_stats(struct mtk_ecc *ecc, struct mtk_ecc_stats *stats,
		       int sectors)
{
	u32 offset, i, err;
	u32 bitflips = 0;

	stats->corrected = 0;
	stats->failed = 0;
	stats->failed_bitmap = 0;

	for (i = 0; i < sectors; i++) {
		offset = i >> 2;
		err = readl(ECC_REG(ecc, ECC_DECENUM0 + offset));
		err = err >> ((i % 4) * 8);
		err &= ecc->ecc_data->ERR_MASK;
		if (err == ecc->ecc_data->ERR_MASK) {
			/* uncorrectable errors */
			stats->failed++;
			stats->failed_bitmap |= (1 << i);
			continue;
		}

		stats->corrected += err;
		bitflips = max_t(u32, bitflips, err);
	}

	stats->bitflips = bitflips;
}
EXPORT_SYMBOL(mtk_ecc_get_stats);

void mtk_ecc_release(struct mtk_ecc *ecc)
{
	clk_disable_unprepare(ecc->clk);
	put_device(ecc->dev);
}
EXPORT_SYMBOL(mtk_ecc_release);

static void mtk_ecc_hw_init(struct mtk_ecc *ecc)
{
	mtk_ecc_wait_idle(ecc, ECC_ENCODE);
	writew(ECC_OP_DISABLE, ECC_REG(ecc, ECC_ENCCON));

	mtk_ecc_wait_idle(ecc, ECC_DECODE);
	writel(ECC_OP_DISABLE, ECC_REG(ecc, ECC_DECCON));
}

static struct mtk_ecc *mtk_ecc_get(struct device_node *np)
{
	struct platform_device *pdev;
	struct mtk_ecc *ecc;

	pdev = of_find_device_by_node(np);
	if (!pdev || !platform_get_drvdata(pdev))
		return ERR_PTR(-EPROBE_DEFER);

	get_device(&pdev->dev);
	ecc = platform_get_drvdata(pdev);
	clk_prepare_enable(ecc->clk);
	mtk_ecc_hw_init(ecc);

	return ecc;
}

struct mtk_ecc *of_mtk_ecc_get(struct device_node *of_node)
{
	struct mtk_ecc *ecc = NULL;
	struct device_node *np;

	np = of_parse_phandle(of_node, "ecc-engine", 0);
	if (np) {
		ecc = mtk_ecc_get(np);
		of_node_put(np);
	}

	return ecc;
}
EXPORT_SYMBOL(of_mtk_ecc_get);

int mtk_ecc_enable(struct mtk_ecc *ecc, struct mtk_ecc_config *config)
{
	enum mtk_ecc_operation op = config->op;
	int ret;
	u32 reg;

	ret = mutex_lock_interruptible(&ecc->lock);
	if (ret) {
		dev_err(ecc->dev, "interrupted when attempting to lock\n");
		return ret;
	}

	mtk_ecc_wait_idle(ecc, op);
	mtk_ecc_config(ecc, config);

	if (config->mode != ECC_NFI_MODE || op != ECC_ENCODE) {
		init_completion(&ecc->done);
		reg = ECC_IRQ_EN;
		if (ecc->ecc_data->pg_irq_enable && config->mode == ECC_NFI_MODE)
			reg |= ECC_PG_IRQ_SEL;
		writew(reg, ECC_IRQ_REG(ecc, op));
	}

	writew(ECC_OP_ENABLE, ECC_CTL_REG(ecc, op));

	return 0;
}
EXPORT_SYMBOL(mtk_ecc_enable);

void mtk_ecc_disable(struct mtk_ecc *ecc)
{
	enum mtk_ecc_operation op = ECC_ENCODE;

	/* find out the running operation */
	if (readw(ECC_CTL_REG(ecc, op)) != ECC_OP_ENABLE)
		op = ECC_DECODE;

	/* disable it */
	mtk_ecc_wait_idle(ecc, op);
	if (op == ECC_DECODE)
		/*
		 * Clear decode IRQ status in case there is a timeout to wait
		 * decode IRQ.
		 */
		readw(ecc->regs + ECC_DECIRQ_STA);
	writew(0, ecc->regs + ECC_IRQ_REG(op));
	writew(ECC_OP_DISABLE, ecc->regs + ECC_CTL_REG(op));

	mutex_unlock(&ecc->lock);
}
EXPORT_SYMBOL(mtk_ecc_disable);

int mtk_ecc_wait_done(struct mtk_ecc *ecc, enum mtk_ecc_operation op)
{
	int ret;

	ret = wait_for_completion_timeout(&ecc->done, msecs_to_jiffies(500));
	if (!ret) {
		dev_err(ecc->dev, "%s timeout - interrupt did not arrive)\n",
			(op == ECC_ENCODE) ? "encoder" : "decoder");
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_ecc_wait_done);

int mtk_ecc_encode(struct mtk_ecc *ecc, struct mtk_ecc_config *config,
		   u8 *data, u32 bytes)
{
	dma_addr_t addr;
	u8 *p;
	u32 len, i, val = 0;
	int ret = 0;

	addr = dma_map_single(ecc->dev, data, bytes, DMA_TO_DEVICE);
	ret = dma_mapping_error(ecc->dev, addr);
	if (ret) {
		dev_err(ecc->dev, "dma mapping error\n");
		return -EINVAL;
	}

	config->op = ECC_ENCODE;
	config->addr = addr;
	ret = mtk_ecc_enable(ecc, config);
	if (ret) {
		dma_unmap_single(ecc->dev, addr, bytes, DMA_TO_DEVICE);
		return ret;
	}

	ret = mtk_ecc_wait_done(ecc, ECC_ENCODE);
	if (ret)
		goto timeout;

	mtk_ecc_wait_idle(ecc, ECC_ENCODE);

	/* Program ECC bytes to OOB: per sector oob = FDM + ECC + SPARE */
	len = (config->strength * ecc->ecc_data->parity_bit + 7) >> 3;
	p = data + bytes;

	/* write the parity bytes generated by the ECC back to the OOB region */
	for (i = 0; i < len; i++) {
		if ((i % 4) == 0)
			val = readl(ECC_REG(ecc, ECC_ENCPAR00 + (i / 4)));
		p[i] = (val >> ((i % 4) * 8)) & 0xff;
	}
timeout:

	dma_unmap_single(ecc->dev, addr, bytes, DMA_TO_DEVICE);
	mtk_ecc_disable(ecc);

	return ret;
}
EXPORT_SYMBOL(mtk_ecc_encode);

void mtk_ecc_adjust_strength(u32 *p)
{
	u32 ecc[] = {4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36,
			40, 44, 48, 52, 56, 60};
	int i;

	for (i = 0; i < ARRAY_SIZE(ecc); i++) {
		if (*p <= ecc[i]) {
			if (!i)
				*p = ecc[i];
			else if (*p != ecc[i])
				*p = ecc[i - 1];
			return;
		}
	}

	*p = ecc[ARRAY_SIZE(ecc) - 1];
}
EXPORT_SYMBOL(mtk_ecc_adjust_strength);

/* Compatible to MT2701 */
static const struct mtk_ecc_comp ecc_mt2701 = {
	.ecc_regs = mt2701_ecc_regs,
	.type = MTK_ECC_MT2701,
	.mode_shift = 5,
	.max_ecc_str = 19,
	.parity_bit = 14,
	.ERR_MASK = 0x3f,
	.pg_irq_enable = 0,
};

/* Compatible to MT7622 */
static const struct mtk_ecc_comp ecc_mt7622 = {
	.ecc_regs = mt7622_ecc_regs,
	.type = MTK_ECC_MT7622,
	.mode_shift = 4,
	.max_ecc_str = 6,
	.parity_bit = 13,
	.ERR_MASK = 0x3f,
	.pg_irq_enable = 0,
};

/* Compatible to MT2712 */
static const struct mtk_ecc_comp ecc_mt2712 = {
	.ecc_regs = mt2712_ecc_regs,
	.type = MTK_ECC_MT2712,
	.mode_shift = 5,
	.max_ecc_str = 19,
	.parity_bit = 14,
	.ERR_MASK = 0x7f,
	.pg_irq_enable = 1,
};

static const struct of_device_id mtk_ecc_dt_match[] = {
	{
		.compatible = "mediatek,mt2701-ecc",
		.data = &ecc_mt2701,
	}, {
		.compatible = "mediatek,mt7622-ecc",
		.data = &ecc_mt7622,
	}, {
		.compatible = "mediatek,mt2712-ecc",
		.data = &ecc_mt2712,
	}, {
	},
};

static int mtk_ecc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ecc *ecc;
	struct resource *res;
	int irq, ret;
	const struct of_device_id *of_ecc_id = NULL;

	of_ecc_id = of_match_device(mtk_ecc_dt_match, &pdev->dev);
	if (!of_ecc_id)
		return -EINVAL;

	ecc = devm_kzalloc(dev, sizeof(*ecc), GFP_KERNEL);
	if (!ecc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ecc->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(ecc->regs)) {
		dev_err(dev, "failed to map regs: %ld\n", PTR_ERR(ecc->regs));
		return PTR_ERR(ecc->regs);
	}
	ecc->ecc_data = (struct mtk_ecc_comp *)of_ecc_id->data;

	ecc->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ecc->clk)) {
		dev_err(dev, "failed to get clock: %ld\n", PTR_ERR(ecc->clk));
		return PTR_ERR(ecc->clk);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -EINVAL;
	}

	ret = dma_set_mask(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "failed to set DMA mask\n");
		return ret;
	}

	ret = devm_request_irq(dev, irq, mtk_ecc_irq, 0x0, "mtk-ecc", ecc);
	if (ret) {
		dev_err(dev, "failed to request irq\n");
		return -EINVAL;
	}

	ecc->dev = dev;
	mutex_init(&ecc->lock);
	platform_set_drvdata(pdev, ecc);
	dev_info(dev, "probed\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_ecc_suspend(struct device *dev)
{
	struct mtk_ecc *ecc = dev_get_drvdata(dev);

	clk_disable_unprepare(ecc->clk);

	return 0;
}

static int mtk_ecc_resume(struct device *dev)
{
	struct mtk_ecc *ecc = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(ecc->clk);
	if (ret) {
		dev_err(dev, "failed to enable clk\n");
		return ret;
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_ecc_pm_ops, mtk_ecc_suspend, mtk_ecc_resume);
#endif

MODULE_DEVICE_TABLE(of, mtk_ecc_dt_match);

static struct platform_driver mtk_ecc_driver = {
	.probe  = mtk_ecc_probe,
	.driver = {
		.name  = "mtk-ecc",
		.of_match_table = of_match_ptr(mtk_ecc_dt_match),
#ifdef CONFIG_PM_SLEEP
		.pm = &mtk_ecc_pm_ops,
#endif
	},
};

module_platform_driver(mtk_ecc_driver);

MODULE_AUTHOR("Xiaolei Li <xiaolei.li@mediatek.com>");
MODULE_DESCRIPTION("MTK Nand ECC Driver");
MODULE_LICENSE("GPL");
