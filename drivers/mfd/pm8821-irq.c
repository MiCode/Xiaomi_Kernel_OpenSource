/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/export.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pm8821-irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/mpm.h>

#define PM8821_TOTAL_IRQ_MASTERS	2
#define PM8821_BLOCKS_PER_MASTER	7
#define PM8821_IRQ_MASTER1_SET		0x01
#define PM8821_IRQ_CLEAR_OFFSET		0x01
#define PM8821_IRQ_RT_STATUS_OFFSET	0x0F
#define PM8821_IRQ_MASK_REG_OFFSET	0x08
#define SSBI_REG_ADDR_IRQ_MASTER0	0x30
#define SSBI_REG_ADDR_IRQ_MASTER1	0xB0
#define MPM_PIN_FOR_8821_IRQ		7
#define SSBI_REG_ADDR_IRQ_IT_STATUS(master_base, block) (master_base + block)

/*
 * Block 0 does not exist in PM8821 IRQ SSBI address space,
 * IRQ0 is assigned to bit0 of block1.
 */
#define SSBI_REG_ADDR_IRQ_IT_CLEAR(master_base, block) \
	(master_base + PM8821_IRQ_CLEAR_OFFSET + block)

#define SSBI_REG_ADDR_IRQ_RT_STATUS(master_base, block) \
	(master_base + PM8821_IRQ_RT_STATUS_OFFSET + block)

#define SSBI_REG_ADDR_IRQ_MASK(master_base, block) \
	(master_base + PM8821_IRQ_MASK_REG_OFFSET + block)

struct pm_irq_chip {
	struct device		*dev;
	spinlock_t		pm_irq_lock;
	unsigned int		base_addr;
	unsigned int		devirq;
	unsigned int		irq_base;
	unsigned int		num_irqs;
	int			masters[PM8821_TOTAL_IRQ_MASTERS];
};

static int pm8821_irq_masked_write(struct pm_irq_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = pm8xxx_readb(chip->dev, addr, &reg);
	if (rc) {
		pr_err("read failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}

	reg &= ~mask;
	reg |= val & mask;

	rc = pm8xxx_writeb(chip->dev, addr, reg);
	if (rc) {
		pr_err("write failed addr = %03X, rc = %d\n", addr, rc);
		return rc;
	}

	return 0;
}

static int pm8821_read_master_irq(const struct pm_irq_chip *chip,
						int m, u8 *master)
{
	return pm8xxx_readb(chip->dev, chip->masters[m], master);
}

static int pm8821_read_block_irq(struct pm_irq_chip *chip, int master,
						u8 block, u8 *bits)
{
	int rc;

	spin_lock(&chip->pm_irq_lock);

	rc = pm8xxx_readb(chip->dev,
	    SSBI_REG_ADDR_IRQ_IT_STATUS(chip->masters[master], block), bits);
	if (rc)
		pr_err("Failed Reading Status rc=%d\n", rc);

	spin_unlock(&chip->pm_irq_lock);
	return rc;
}


static int pm8821_irq_block_handler(struct pm_irq_chip *chip,
					int master_number, int block)
{
	int pmirq, irq, i, ret;
	u8 bits;

	ret = pm8821_read_block_irq(chip, master_number, block, &bits);
	if (ret) {
		pr_err("Failed reading %d block ret=%d", block, ret);
		return ret;
	}
	if (!bits) {
		pr_err("block bit set in master but no irqs: %d", block);
		return 0;
	}

	/* Convert block offset to global block number */
	block += (master_number * PM8821_BLOCKS_PER_MASTER) - 1;

	/* Check IRQ bits */
	for (i = 0; i < 8; i++) {
		if (bits & BIT(i)) {
			pmirq = (block << 3) + i;
			irq = pmirq + chip->irq_base;
			generic_handle_irq(irq);
		}
	}
	return 0;
}

static int pm8821_irq_read_master(struct pm_irq_chip *chip,
				int master_number, u8 master_val)
{
	int ret = 0;
	int block;

	for (block = 1; block < 8; block++) {
		if (master_val & BIT(block)) {
			ret |= pm8821_irq_block_handler(chip,
					master_number, block);
		}
	}

	return ret;
}

static irqreturn_t pm8821_irq_handler(int irq, void *data)
{
	struct pm_irq_chip *chip = data;
	int ret;
	u8 master;

	ret = pm8821_read_master_irq(chip, 0, &master);
	if (ret) {
		pr_err("Failed to read master 0 ret=%d\n", ret);
		return ret;
	}

	if (master & ~PM8821_IRQ_MASTER1_SET)
		pm8821_irq_read_master(chip, 0, master);

	if (!(master & PM8821_IRQ_MASTER1_SET))
		goto done;

	ret = pm8821_read_master_irq(chip, 1, &master);
	if (ret) {
		pr_err("Failed to read master 1 ret=%d\n", ret);
		return ret;
	}

	pm8821_irq_read_master(chip, 1, master);

done:
	return IRQ_HANDLED;
}

static void pm8821_irq_mask(struct irq_data *d)
{
	struct pm_irq_chip *chip = irq_data_get_irq_chip_data(d);
	unsigned int pmirq = d->irq - chip->irq_base;
	int irq_bit, rc;
	u8 block, master;

	block = pmirq >> 3;
	master = block / PM8821_BLOCKS_PER_MASTER;
	irq_bit = pmirq % 8;
	block %= PM8821_BLOCKS_PER_MASTER;

	spin_lock(&chip->pm_irq_lock);

	rc = pm8821_irq_masked_write(chip,
		SSBI_REG_ADDR_IRQ_MASK(chip->masters[master], block),
		BIT(irq_bit), BIT(irq_bit));

	if (rc)
		pr_err("Failed to read/write mask IRQ:%d rc=%d\n", pmirq, rc);

	spin_unlock(&chip->pm_irq_lock);
}

static void pm8821_irq_mask_ack(struct irq_data *d)
{
	struct pm_irq_chip *chip = irq_data_get_irq_chip_data(d);
	unsigned int pmirq = d->irq - chip->irq_base;
	int irq_bit, rc;
	u8 block, master;

	block = pmirq >> 3;
	master = block / PM8821_BLOCKS_PER_MASTER;
	irq_bit = pmirq % 8;
	block %= PM8821_BLOCKS_PER_MASTER;

	spin_lock(&chip->pm_irq_lock);

	rc = pm8821_irq_masked_write(chip,
		SSBI_REG_ADDR_IRQ_MASK(chip->masters[master], block),
		BIT(irq_bit), BIT(irq_bit));

	if (rc) {
		pr_err("Failed to read/write mask IRQ:%d rc=%d\n", pmirq, rc);
		goto fail;
	}

	rc = pm8821_irq_masked_write(chip,
		SSBI_REG_ADDR_IRQ_IT_CLEAR(chip->masters[master], block),
		BIT(irq_bit), BIT(irq_bit));

	if (rc) {
		pr_err("Failed to read/write IT_CLEAR IRQ:%d rc=%d\n",
								pmirq, rc);
	}

fail:
	spin_unlock(&chip->pm_irq_lock);
}

static void pm8821_irq_unmask(struct irq_data *d)
{
	struct pm_irq_chip *chip = irq_data_get_irq_chip_data(d);
	unsigned int pmirq = d->irq - chip->irq_base;
	int irq_bit, rc;
	u8 block, master;

	block = pmirq >> 3;
	master = block / PM8821_BLOCKS_PER_MASTER;
	irq_bit = pmirq % 8;
	block %= PM8821_BLOCKS_PER_MASTER;

	spin_lock(&chip->pm_irq_lock);

	rc = pm8821_irq_masked_write(chip,
		SSBI_REG_ADDR_IRQ_MASK(chip->masters[master], block),
		BIT(irq_bit), ~BIT(irq_bit));

	if (rc)
		pr_err("Failed to read/write unmask IRQ:%d rc=%d\n", pmirq, rc);

	spin_unlock(&chip->pm_irq_lock);
}

static int pm8821_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	/*
	 * PM8821 IRQ controller does not have explicit software support for
	 * IRQ flow type.
	 */
	return 0;
}

static int pm8821_irq_set_wake(struct irq_data *d, unsigned int on)
{
	return 0;
}

static int pm8821_irq_read_line(struct irq_data *d)
{
	struct pm_irq_chip *chip = irq_data_get_irq_chip_data(d);

	return pm8821_get_irq_stat(chip, d->irq);
}

static struct irq_chip pm_irq_chip = {
	.name		= "pm8821-irq",
	.irq_mask	= pm8821_irq_mask,
	.irq_mask_ack	= pm8821_irq_mask_ack,
	.irq_unmask	= pm8821_irq_unmask,
	.irq_set_type	= pm8821_irq_set_type,
	.irq_set_wake	= pm8821_irq_set_wake,
	.irq_read_line	= pm8821_irq_read_line,
	.flags		= IRQCHIP_MASK_ON_SUSPEND,
};

/**
 * pm8821_get_irq_stat - get the status of the irq line
 * @chip: pointer to identify a pmic irq controller
 * @irq: the irq number
 *
 * The pm8821 gpio and mpp rely on the interrupt block to read
 * the values on their pins. This function is to facilitate reading
 * the status of a gpio or an mpp line. The caller has to convert the
 * gpio number to irq number.
 *
 * RETURNS:
 * an int indicating the value read on that line
 */
int pm8821_get_irq_stat(struct pm_irq_chip *chip, int irq)
{
	int pmirq, rc;
	u8 block, bits, bit, master;
	unsigned long flags;

	if (chip == NULL || irq < chip->irq_base
	    || irq >= chip->irq_base + chip->num_irqs)
		return -EINVAL;

	pmirq = irq - chip->irq_base;

	block = pmirq >> 3;
	master = block / PM8821_BLOCKS_PER_MASTER;
	bit = pmirq % 8;
	block %= PM8821_BLOCKS_PER_MASTER;

	spin_lock_irqsave(&chip->pm_irq_lock, flags);

	rc = pm8xxx_readb(chip->dev,
		SSBI_REG_ADDR_IRQ_RT_STATUS(chip->masters[master], block),
		&bits);
	if (rc) {
		pr_err("Failed Configuring irq=%d pmirq=%d blk=%d rc=%d\n",
						irq, pmirq, block, rc);
		goto bail_out;
	}

	rc = (bits & BIT(bit)) ? 1 : 0;

bail_out:
	spin_unlock_irqrestore(&chip->pm_irq_lock, flags);

	return rc;
}
EXPORT_SYMBOL_GPL(pm8821_get_irq_stat);

struct pm_irq_chip *  __devinit pm8821_irq_init(struct device *dev,
				const struct pm8xxx_irq_platform_data *pdata)
{
	struct pm_irq_chip	*chip;
	int			devirq, rc, blocks, masters;
	unsigned int		pmirq;

	if (!pdata) {
		pr_err("No platform data\n");
		return ERR_PTR(-EINVAL);
	}

	devirq = pdata->devirq;
	if (devirq < 0) {
		pr_err("missing devirq\n");
		rc = devirq;
		return ERR_PTR(-EINVAL);
	}

	chip = kzalloc(sizeof(struct pm_irq_chip)
			+ sizeof(u8) * pdata->irq_cdata.nirqs, GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot alloc pm_irq_chip struct\n");
		return ERR_PTR(-EINVAL);
	}

	chip->dev	= dev;
	chip->devirq	= devirq;
	chip->irq_base	= pdata->irq_base;
	chip->num_irqs	= pdata->irq_cdata.nirqs;
	chip->base_addr = pdata->irq_cdata.base_addr;
	blocks		= DIV_ROUND_UP(pdata->irq_cdata.nirqs, 8);
	masters		= DIV_ROUND_UP(blocks, PM8821_BLOCKS_PER_MASTER);
	chip->masters[0] = chip->base_addr + SSBI_REG_ADDR_IRQ_MASTER0;
	chip->masters[1] = chip->base_addr + SSBI_REG_ADDR_IRQ_MASTER1;

	if (masters != PM8821_TOTAL_IRQ_MASTERS) {
		pr_err("Unequal number of masters, passed: %d, "
		"should have been: %d\n", masters, PM8821_TOTAL_IRQ_MASTERS);
		kfree(chip);
		return ERR_PTR(-EINVAL);
	}

	spin_lock_init(&chip->pm_irq_lock);

	for (pmirq = 0; pmirq < chip->num_irqs; pmirq++) {
		irq_set_chip_and_handler(chip->irq_base + pmirq,
				&pm_irq_chip, handle_level_irq);
		irq_set_chip_data(chip->irq_base + pmirq, chip);
#ifdef CONFIG_ARM
		set_irq_flags(chip->irq_base + pmirq, IRQF_VALID);
#else
		irq_set_noprobe(chip->irq_base + pmirq);
#endif
	}

	if (devirq != 0) {
		rc = request_irq(devirq, pm8821_irq_handler,
			pdata->irq_trigger_flag, "pm8821_sec_irq", chip);
		if (rc) {
			pr_err("failed to request_irq for %d rc=%d\n",
							devirq, rc);
			kfree(chip);
			return ERR_PTR(rc);
		} else{
			irq_set_irq_wake(devirq, 1);
			msm_mpm_set_pin_wake(MPM_PIN_FOR_8821_IRQ, 1);
			msm_mpm_set_pin_type(MPM_PIN_FOR_8821_IRQ,
				pdata->irq_trigger_flag);
		}
	}

	return chip;
}

int pm8821_irq_exit(struct pm_irq_chip *chip)
{
	irq_set_chained_handler(chip->devirq, NULL);
	kfree(chip);
	return 0;
}
