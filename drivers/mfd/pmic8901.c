/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/pmic8901.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

/* PMIC8901 Revision */
#define SSBI_REG_REV			0x002  /* PMIC4 revision */

/* PMIC8901 IRQ */
#define	SSBI_REG_ADDR_IRQ_BASE		0xD5

#define	SSBI_REG_ADDR_IRQ_ROOT		(SSBI_REG_ADDR_IRQ_BASE + 0)
#define	SSBI_REG_ADDR_IRQ_M_STATUS1	(SSBI_REG_ADDR_IRQ_BASE + 1)
#define	SSBI_REG_ADDR_IRQ_M_STATUS2	(SSBI_REG_ADDR_IRQ_BASE + 2)
#define	SSBI_REG_ADDR_IRQ_M_STATUS3	(SSBI_REG_ADDR_IRQ_BASE + 3)
#define	SSBI_REG_ADDR_IRQ_M_STATUS4	(SSBI_REG_ADDR_IRQ_BASE + 4)
#define	SSBI_REG_ADDR_IRQ_BLK_SEL	(SSBI_REG_ADDR_IRQ_BASE + 5)
#define	SSBI_REG_ADDR_IRQ_IT_STATUS	(SSBI_REG_ADDR_IRQ_BASE + 6)
#define	SSBI_REG_ADDR_IRQ_CONFIG	(SSBI_REG_ADDR_IRQ_BASE + 7)
#define	SSBI_REG_ADDR_IRQ_RT_STATUS	(SSBI_REG_ADDR_IRQ_BASE + 8)

#define	PM8901_IRQF_LVL_SEL		0x01	/* level select */
#define	PM8901_IRQF_MASK_FE		0x02	/* mask falling edge */
#define	PM8901_IRQF_MASK_RE		0x04	/* mask rising edge */
#define	PM8901_IRQF_CLR			0x08	/* clear interrupt */
#define	PM8901_IRQF_BITS_MASK		0x70
#define	PM8901_IRQF_BITS_SHIFT		4
#define	PM8901_IRQF_WRITE		0x80

#define	PM8901_IRQF_MASK_ALL		(PM8901_IRQF_MASK_FE | \
					PM8901_IRQF_MASK_RE)
#define PM8901_IRQF_W_C_M		(PM8901_IRQF_WRITE |	\
					PM8901_IRQF_CLR |	\
					PM8901_IRQF_MASK_ALL)

#define	MAX_PM_IRQ			72
#define	MAX_PM_BLOCKS			(MAX_PM_IRQ / 8 + 1)
#define	MAX_PM_MASTERS			(MAX_PM_BLOCKS / 8 + 1)

#define MPP_IRQ_BLOCK			1

/* FTS regulator PMR registers */
#define SSBI_REG_ADDR_S1_PMR		(0xA7)
#define SSBI_REG_ADDR_S2_PMR		(0xA8)
#define SSBI_REG_ADDR_S3_PMR		(0xA9)
#define SSBI_REG_ADDR_S4_PMR		(0xAA)

#define REGULATOR_PMR_STATE_MASK	0x60
#define REGULATOR_PMR_STATE_OFF		0x20

/* Shutdown/restart delays to allow for LDO 7/dVdd regulator load settling. */
#define DELAY_AFTER_REG_DISABLE_MS	4
#define DELAY_BEFORE_SHUTDOWN_MS	8

struct pm8901_chip {
	struct pm8901_platform_data	pdata;
	struct device			*dev;

	u8	irqs_allowed[MAX_PM_BLOCKS];
	u8	blocks_allowed[MAX_PM_MASTERS];
	u8	masters_allowed;
	int	pm_max_irq;
	int	pm_max_blocks;
	int	pm_max_masters;

	u8	config[MAX_PM_IRQ];
	u8	wake_enable[MAX_PM_IRQ];
	u16	count_wakeable;

	u8	revision;

	spinlock_t	pm_lock;
};

#if defined(CONFIG_DEBUG_FS)
struct pm8901_dbg_device {
	struct mutex		dbg_mutex;
	struct pm8901_chip	*pm_chip;
	struct dentry		*dent;
	int			addr;
};

static struct pm8901_dbg_device *pmic_dbg_device;
#endif

static struct pm8901_chip *pmic_chip;

/* Helper Functions */
DEFINE_RATELIMIT_STATE(pm8901_msg_ratelimit, 60 * HZ, 10);

static inline int pm8901_can_print(void)
{
	return __ratelimit(&pm8901_msg_ratelimit);
}

static inline int
ssbi_read(struct device *dev, u16 addr, u8 *buf, size_t len)
{
	return msm_ssbi_read(dev->parent, addr, buf, len);
}

static inline int
ssbi_write(struct device *dev, u16 addr, u8 *buf, size_t len)
{
	return msm_ssbi_write(dev->parent, addr, buf, len);
}

/* External APIs */
int pm8901_rev(struct pm8901_chip *chip)
{
	if (chip == NULL) {
		if (pmic_chip != NULL)
			return pmic_chip->revision;
		else
			return -EINVAL;
	}

	return chip->revision;
}
EXPORT_SYMBOL(pm8901_rev);

int pm8901_read(struct pm8901_chip *chip, u16 addr, u8 *values,
		unsigned int len)
{
	if (chip == NULL)
		return -EINVAL;

	return ssbi_read(chip->dev, addr, values, len);
}
EXPORT_SYMBOL(pm8901_read);

int pm8901_write(struct pm8901_chip *chip, u16 addr, u8 *values,
		 unsigned int len)
{
	if (chip == NULL)
		return -EINVAL;

	return ssbi_write(chip->dev, addr, values, len);
}
EXPORT_SYMBOL(pm8901_write);

int pm8901_irq_get_rt_status(struct pm8901_chip *chip, int irq)
{
	int     rc;
	u8      block, bits, bit;
	unsigned long   irqsave;

	if (chip == NULL || irq < chip->pdata.irq_base ||
			irq >= chip->pdata.irq_base + MAX_PM_IRQ)
		return -EINVAL;

	irq -= chip->pdata.irq_base;

	block = irq / 8;
	bit = irq % 8;

	spin_lock_irqsave(&chip->pm_lock, irqsave);

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_BLK_SEL, &block, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(): rc=%d (Select Block)\n",
				__func__, rc);
		goto bail_out;
	}

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_RT_STATUS, &bits, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read RT Status)\n",
				__func__, rc);
		goto bail_out;
	}

	rc = (bits & (1 << bit)) ? 1 : 0;

bail_out:
	spin_unlock_irqrestore(&chip->pm_lock, irqsave);

	return rc;
}
EXPORT_SYMBOL(pm8901_irq_get_rt_status);

int pm8901_reset_pwr_off(int reset)
{
	int rc = 0, i;
	u8 pmr;
	u8 pmr_addr[4] = {
		SSBI_REG_ADDR_S2_PMR,
		SSBI_REG_ADDR_S3_PMR,
		SSBI_REG_ADDR_S4_PMR,
		SSBI_REG_ADDR_S1_PMR,
	};

	if (pmic_chip == NULL)
		return -ENODEV;

	/* Turn off regulators S1, S2, S3, S4 when shutting down. */
	if (!reset) {
		for (i = 0; i < 4; i++) {
			rc = ssbi_read(pmic_chip->dev, pmr_addr[i], &pmr, 1);
			if (rc) {
				pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n",
				       __func__, pmr_addr[i], rc);
				goto get_out;
			}

			pmr &= ~REGULATOR_PMR_STATE_MASK;
			pmr |= REGULATOR_PMR_STATE_OFF;

			rc = ssbi_write(pmic_chip->dev, pmr_addr[i], &pmr, 1);
			if (rc) {
				pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d"
				       "\n", __func__, pmr_addr[i], pmr, rc);
				goto get_out;
			}
			mdelay(DELAY_AFTER_REG_DISABLE_MS);
		}
	}

get_out:
	mdelay(DELAY_BEFORE_SHUTDOWN_MS);
	return rc;
}
EXPORT_SYMBOL(pm8901_reset_pwr_off);

/* Internal functions */
static inline int
pm8901_config_irq(struct pm8901_chip *chip, u8 *bp, u8 *cp)
{
	int	rc;

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_BLK_SEL, bp, 1);
	if (rc) {
		pr_err("%s: ssbi_write: rc=%d (Select block)\n",
			__func__, rc);
		goto bail_out;
	}

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_CONFIG, cp, 1);
	if (rc)
		pr_err("%s: ssbi_write: rc=%d (Configure IRQ)\n",
			__func__, rc);

bail_out:
	return rc;
}

static void pm8901_irq_mask(struct irq_data *d)
{
	int	master, irq_bit;
	struct	pm8901_chip *chip = irq_data_get_irq_handler_data(d);
	u8	block, config;
	unsigned int irq = d->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;
	master = block / 8;
	irq_bit = irq % 8;

	chip->irqs_allowed[block] &= ~(1 << irq_bit);
	if (!chip->irqs_allowed[block]) {
		chip->blocks_allowed[master] &= ~(1 << (block % 8));

		if (!chip->blocks_allowed[master])
			chip->masters_allowed &= ~(1 << master);
	}

	config = PM8901_IRQF_WRITE | chip->config[irq] |
		PM8901_IRQF_MASK_FE | PM8901_IRQF_MASK_RE;
	pm8901_config_irq(chip, &block, &config);
}

static void pm8901_irq_unmask(struct irq_data *d)
{
	int	master, irq_bit;
	struct	pm8901_chip *chip = irq_data_get_irq_handler_data(d);
	u8	block, config, old_irqs_allowed, old_blocks_allowed;
	unsigned int irq = d->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;
	master = block / 8;
	irq_bit = irq % 8;

	old_irqs_allowed = chip->irqs_allowed[block];
	chip->irqs_allowed[block] |= 1 << irq_bit;
	if (!old_irqs_allowed) {
		master = block / 8;

		old_blocks_allowed = chip->blocks_allowed[master];
		chip->blocks_allowed[master] |= 1 << (block % 8);

		if (!old_blocks_allowed)
			chip->masters_allowed |= 1 << master;
	}

	config = PM8901_IRQF_WRITE | chip->config[irq];
	pm8901_config_irq(chip, &block, &config);
}

static void pm8901_irq_ack(struct irq_data *d)
{
	struct	pm8901_chip *chip = irq_data_get_irq_handler_data(d);
	u8	block, config;
	unsigned int irq = d->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;

	config = PM8901_IRQF_WRITE | chip->config[irq] | PM8901_IRQF_CLR;
	pm8901_config_irq(chip, &block, &config);
}

static int pm8901_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	int	master, irq_bit;
	struct	pm8901_chip *chip = irq_data_get_irq_handler_data(d);
	u8	block, config;
	unsigned int irq = d->irq;

	irq -= chip->pdata.irq_base;
	if (irq > chip->pm_max_irq) {
		chip->pm_max_irq = irq;
		chip->pm_max_blocks =
			chip->pm_max_irq / 8 + 1;
		chip->pm_max_masters =
			chip->pm_max_blocks / 8 + 1;
	}
	block = irq / 8;
	master = block / 8;
	irq_bit = irq % 8;

	chip->config[irq] = (irq_bit << PM8901_IRQF_BITS_SHIFT) |
			PM8901_IRQF_MASK_RE | PM8901_IRQF_MASK_FE;
	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		if (flow_type & IRQF_TRIGGER_RISING)
			chip->config[irq] &= ~PM8901_IRQF_MASK_RE;
		if (flow_type & IRQF_TRIGGER_FALLING)
			chip->config[irq] &= ~PM8901_IRQF_MASK_FE;
	} else {
		chip->config[irq] |= PM8901_IRQF_LVL_SEL;

		if (flow_type & IRQF_TRIGGER_HIGH)
			chip->config[irq] &= ~PM8901_IRQF_MASK_RE;
		else
			chip->config[irq] &= ~PM8901_IRQF_MASK_FE;
	}

	config = PM8901_IRQF_WRITE | chip->config[irq] | PM8901_IRQF_CLR;
	return pm8901_config_irq(chip, &block, &config);
}

static int pm8901_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct	pm8901_chip *chip = irq_data_get_irq_handler_data(d);
	unsigned int irq = d->irq;

	irq -= chip->pdata.irq_base;
	if (on) {
		if (!chip->wake_enable[irq]) {
			chip->wake_enable[irq] = 1;
			chip->count_wakeable++;
		}
	} else {
		if (chip->wake_enable[irq]) {
			chip->wake_enable[irq] = 0;
			chip->count_wakeable--;
		}
	}

	return 0;
}

static inline int
pm8901_read_root(struct pm8901_chip *chip, u8 *rp)
{
	int	rc;

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_ROOT, rp, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read Root)\n",
			__func__, rc);
		*rp = 0;
	}

	return rc;
}

static inline int
pm8901_read_master(struct pm8901_chip *chip, u8 m, u8 *bp)
{
	int	rc;

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_M_STATUS1 + m, bp, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read Master)\n",
			__func__, rc);
		*bp = 0;
	}

	return rc;
}

static inline int
pm8901_read_block(struct pm8901_chip *chip, u8 *bp, u8 *ip)
{
	int	rc;

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_BLK_SEL, bp, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(): rc=%d (Select Block)\n",
		       __func__, rc);
		*bp = 0;
		goto bail_out;
	}

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_IT_STATUS, ip, 1);
	if (rc)
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read Status)\n",
		       __func__, rc);

bail_out:
	return rc;
}

static irqreturn_t pm8901_isr_thread(int irq_requested, void *data)
{
	struct pm8901_chip *chip = data;
	int	i, j, k;
	u8	root, block, config, bits;
	u8	blocks[MAX_PM_MASTERS];
	int	masters = 0, irq, handled = 0, spurious = 0;
	u16     irqs_to_handle[MAX_PM_IRQ];
	unsigned long	irqsave;

	spin_lock_irqsave(&chip->pm_lock, irqsave);

	/* Read root for masters */
	if (pm8901_read_root(chip, &root))
		goto bail_out;

	masters = root >> 1;

	if (!(masters & chip->masters_allowed) ||
	    (masters & ~chip->masters_allowed)) {
		spurious = 1000000;
	}

	/* Read allowed masters for blocks. */
	for (i = 0; i < chip->pm_max_masters; i++) {
		if (masters & (1 << i)) {
			if (pm8901_read_master(chip, i, &blocks[i]))
				goto bail_out;

			if (!blocks[i]) {
				if (pm8901_can_print())
					pr_err("%s: Spurious master: %d "
					       "(blocks=0)", __func__, i);
				spurious += 10000;
			}
		} else
			blocks[i] = 0;
	}

	/* Select block, read status and call isr */
	for (i = 0; i < chip->pm_max_masters; i++) {
		if (!blocks[i])
			continue;

		for (j = 0; j < 8; j++) {
			if (!(blocks[i] & (1 << j)))
				continue;

			block = i * 8 + j;	/* block # */
			if (pm8901_read_block(chip, &block, &bits))
				goto bail_out;

			if (!bits) {
				if (pm8901_can_print())
					pr_err("%s: Spurious block: "
					       "[master, block]=[%d, %d] "
					       "(bits=0)\n", __func__, i, j);
				spurious += 100;
				continue;
			}

			/* Check IRQ bits */
			for (k = 0; k < 8; k++) {
				if (!(bits & (1 << k)))
					continue;

				/* Check spurious interrupts */
				if (((1 << i) & chip->masters_allowed) &&
				    (blocks[i] & chip->blocks_allowed[i]) &&
				    (bits & chip->irqs_allowed[block])) {

					/* Found one */
					irq = block * 8 + k;
					irqs_to_handle[handled] = irq +
						chip->pdata.irq_base;
					handled++;
				} else {
					/* Clear and mask wrong one */
					config = PM8901_IRQF_W_C_M |
						(k < PM8901_IRQF_BITS_SHIFT);

					pm8901_config_irq(chip,
							  &block, &config);

					if (pm8901_can_print())
						pr_err("%s: Spurious IRQ: "
						       "[master, block, bit]="
						       "[%d, %d (%d), %d]\n",
							__func__,
						       i, j, block, k);
					spurious++;
				}
			}
		}

	}

bail_out:

	spin_unlock_irqrestore(&chip->pm_lock, irqsave);

	for (i = 0; i < handled; i++)
		generic_handle_irq(irqs_to_handle[i]);

	if (spurious) {
		if (!pm8901_can_print())
			return IRQ_HANDLED;

		pr_err("%s: spurious = %d (handled = %d)\n",
		       __func__, spurious, handled);
		pr_err("   root = 0x%x (masters_allowed<<1 = 0x%x)\n",
		       root, chip->masters_allowed << 1);
		for (i = 0; i < chip->pm_max_masters; i++) {
			if (masters & (1 << i))
				pr_err("   blocks[%d]=0x%x, "
				       "allowed[%d]=0x%x\n",
				       i, blocks[i],
				       i, chip->blocks_allowed[i]);
		}
	}

	return IRQ_HANDLED;
}

#if defined(CONFIG_DEBUG_FS)

static int check_addr(int addr, const char *func_name)
{
	if (addr < 0 || addr > 0x3FF) {
		pr_err("%s: PMIC 8901 register address is invalid: %d\n",
			func_name, addr);
		return -EINVAL;
	}
	return 0;
}

static int data_set(void *data, u64 val)
{
	struct pm8901_dbg_device *dbgdev = data;
	u8 reg = val;
	int rc;

	mutex_lock(&dbgdev->dbg_mutex);

	rc = check_addr(dbgdev->addr, __func__);
	if (rc)
		goto done;

	rc = pm8901_write(dbgdev->pm_chip, dbgdev->addr, &reg, 1);

	if (rc)
		pr_err("%s: FAIL pm8901_write(0x%03X)=0x%02X: rc=%d\n",
			__func__, dbgdev->addr, reg, rc);
done:
	mutex_unlock(&dbgdev->dbg_mutex);
	return rc;
}

static int data_get(void *data, u64 *val)
{
	struct pm8901_dbg_device *dbgdev = data;
	int rc;
	u8 reg;

	mutex_lock(&dbgdev->dbg_mutex);

	rc = check_addr(dbgdev->addr, __func__);
	if (rc)
		goto done;

	rc = pm8901_read(dbgdev->pm_chip, dbgdev->addr, &reg, 1);

	if (rc) {
		pr_err("%s: FAIL pm8901_read(0x%03X)=0x%02X: rc=%d\n",
			__func__, dbgdev->addr, reg, rc);
		goto done;
	}

	*val = reg;
done:
	mutex_unlock(&dbgdev->dbg_mutex);
	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_data_fops, data_get, data_set, "0x%02llX\n");

static int addr_set(void *data, u64 val)
{
	struct pm8901_dbg_device *dbgdev = data;
	int rc;

	rc = check_addr(val, __func__);
	if (rc)
		return rc;

	mutex_lock(&dbgdev->dbg_mutex);
	dbgdev->addr = val;
	mutex_unlock(&dbgdev->dbg_mutex);

	return 0;
}

static int addr_get(void *data, u64 *val)
{
	struct pm8901_dbg_device *dbgdev = data;
	int rc;

	mutex_lock(&dbgdev->dbg_mutex);

	rc = check_addr(dbgdev->addr, __func__);
	if (rc) {
		mutex_unlock(&dbgdev->dbg_mutex);
		return rc;
	}
	*val = dbgdev->addr;

	mutex_unlock(&dbgdev->dbg_mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_addr_fops, addr_get, addr_set, "0x%03llX\n");

static int __devinit pmic8901_dbg_probe(struct pm8901_chip *chip)
{
	struct pm8901_dbg_device *dbgdev;
	struct dentry *dent;
	struct dentry *temp;
	int rc;

	if (chip == NULL) {
		pr_err("%s: no parent data passed in.\n", __func__);
		return -EINVAL;
	}

	dbgdev = kzalloc(sizeof *dbgdev, GFP_KERNEL);
	if (dbgdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	dbgdev->pm_chip = chip;
	dbgdev->addr = -1;

	dent = debugfs_create_dir("pm8901-dbg", NULL);
	if (dent == NULL || IS_ERR(dent)) {
		pr_err("%s: ERR debugfs_create_dir: dent=0x%X\n",
					__func__, (unsigned)dent);
		rc = PTR_ERR(dent);
		goto dir_error;
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, dent,
					dbgdev, &dbg_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("%s: ERR debugfs_create_file: dent=0x%X\n",
					__func__, (unsigned)temp);
		rc = PTR_ERR(temp);
		goto debug_error;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, dent,
					dbgdev, &dbg_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("%s: ERR debugfs_create_file: dent=0x%X\n",
					__func__, (unsigned)temp);
		rc = PTR_ERR(temp);
		goto debug_error;
	}

	mutex_init(&dbgdev->dbg_mutex);

	dbgdev->dent = dent;

	pmic_dbg_device = dbgdev;

	return 0;

debug_error:
	debugfs_remove_recursive(dent);
dir_error:
	kfree(dbgdev);

	return rc;
}

static int __devexit pmic8901_dbg_remove(void)
{
	if (pmic_dbg_device) {
		debugfs_remove_recursive(pmic_dbg_device->dent);
		mutex_destroy(&pmic_dbg_device->dbg_mutex);
		kfree(pmic_dbg_device);
	}
	return 0;
}

#else

static int __devinit pmic8901_dbg_probe(struct pm8901_chip *chip)
{
	return 0;
}

static int __devexit pmic8901_dbg_remove(void)
{
	return 0;
}

#endif

static struct irq_chip pm8901_irq_chip = {
	.name      = "pm8901",
	.irq_ack       = pm8901_irq_ack,
	.irq_mask      = pm8901_irq_mask,
	.irq_unmask    = pm8901_irq_unmask,
	.irq_set_type  = pm8901_irq_set_type,
	.irq_set_wake  = pm8901_irq_set_wake,
};

static int pm8901_probe(struct platform_device *pdev)
{
	int i, rc;
	struct pm8901_platform_data *pdata = pdev->dev.platform_data;
	struct pm8901_chip *chip;

	if (pdata == NULL || pdata->irq <= 0) {
		pr_err("%s: No platform_data or IRQ.\n", __func__);
		return -ENODEV;
	}

	chip = kzalloc(sizeof *chip, GFP_KERNEL);
	if (chip == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	/* Read PMIC chip revision */
	rc = ssbi_read(chip->dev, SSBI_REG_REV, &chip->revision, 1);
	if (rc)
		pr_err("%s: Failed on ssbi_read for revision: rc=%d.\n",
			__func__, rc);
	pr_info("%s: PMIC revision: %X\n", __func__, chip->revision);

	(void) memcpy((void *)&chip->pdata, (const void *)pdata,
		      sizeof(chip->pdata));

	irq_set_handler_data(pdata->irq, (void *)chip);
	irq_set_irq_wake(pdata->irq, 1);

	chip->pm_max_irq = 0;
	chip->pm_max_blocks = 0;
	chip->pm_max_masters = 0;

	platform_set_drvdata(pdev, chip);

	pmic_chip = chip;
	spin_lock_init(&chip->pm_lock);

	/* Register for all reserved IRQs */
	for (i = pdata->irq_base; i < (pdata->irq_base + MAX_PM_IRQ); i++) {
		irq_set_chip(i, &pm8901_irq_chip);
		irq_set_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
		irq_set_handler_data(i, (void *)chip);
	}

	rc = mfd_add_devices(chip->dev, 0, pdata->sub_devices,
			     pdata->num_subdevs, NULL, 0);
	if (rc) {
		pr_err("%s: could not add devices %d\n", __func__, rc);
		return rc;
	}

	rc = request_threaded_irq(pdata->irq, NULL, pm8901_isr_thread,
			IRQF_ONESHOT | IRQF_DISABLED | pdata->irq_trigger_flags,
			"pm8901-irq", chip);
	if (rc)
		pr_err("%s: could not request irq %d: %d\n", __func__,
				pdata->irq, rc);

	rc = pmic8901_dbg_probe(chip);
	if (rc < 0)
		pr_err("%s: could not set up debugfs: %d\n", __func__, rc);

	return rc;
}

static int __devexit pm8901_remove(struct platform_device *pdev)
{
	struct	pm8901_chip *chip;

	chip = platform_get_drvdata(pdev);
	if (chip) {
		if (chip->pm_max_irq) {
			irq_set_irq_wake(chip->pdata.irq, 0);
			free_irq(chip->pdata.irq, chip);
		}

		mfd_remove_devices(chip->dev);

		chip->dev = NULL;

		kfree(chip);
	}

	pmic8901_dbg_remove();

	return 0;
}

#ifdef CONFIG_PM
static int pm8901_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct	pm8901_chip *chip;
	int	i;
	unsigned long	irqsave;

	chip = platform_get_drvdata(pdev);

	for (i = 0; i < MAX_PM_IRQ; i++) {
		spin_lock_irqsave(&chip->pm_lock, irqsave);
		if (chip->config[i] && !chip->wake_enable[i]) {
			if (!((chip->config[i] & PM8901_IRQF_MASK_ALL)
			      == PM8901_IRQF_MASK_ALL))
				pm8901_irq_mask(irq_get_irq_data(i +
							chip->pdata.irq_base));
		}
		spin_unlock_irqrestore(&chip->pm_lock, irqsave);
	}

	if (!chip->count_wakeable)
		disable_irq(chip->pdata.irq);

	return 0;
}

static int pm8901_resume(struct platform_device *pdev)
{
	struct	pm8901_chip *chip;
	int	i;
	unsigned long	irqsave;

	chip = platform_get_drvdata(pdev);

	for (i = 0; i < MAX_PM_IRQ; i++) {
		spin_lock_irqsave(&chip->pm_lock, irqsave);
		if (chip->config[i] && !chip->wake_enable[i]) {
			if (!((chip->config[i] & PM8901_IRQF_MASK_ALL)
			      == PM8901_IRQF_MASK_ALL))
				pm8901_irq_unmask(irq_get_irq_data(i +
							chip->pdata.irq_base));
		}
		spin_unlock_irqrestore(&chip->pm_lock, irqsave);
	}

	if (!chip->count_wakeable)
		enable_irq(chip->pdata.irq);

	return 0;
}
#else
#define	pm8901_suspend		NULL
#define	pm8901_resume		NULL
#endif

static struct platform_driver pm8901_driver = {
	.probe		= pm8901_probe,
	.remove		= __devexit_p(pm8901_remove),
	.driver		= {
		.name	= "pm8901-core",
		.owner	= THIS_MODULE,
	},
	.suspend	= pm8901_suspend,
	.resume		= pm8901_resume,
};

static int __init pm8901_init(void)
{
	return  platform_driver_register(&pm8901_driver);
}
arch_initcall(pm8901_init);

static void __exit pm8901_exit(void)
{
	platform_driver_unregister(&pm8901_driver);
}
module_exit(pm8901_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8901 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pmic8901-core");
