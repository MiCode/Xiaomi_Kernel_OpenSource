/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/mt-gic.h>

/*#include <mach/irqs.h>*/
#include "mt_sys_cirq.h"
#include <mt-plat/sync_write.h>
#include <mt-plat/mt_io.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

void __iomem *SYS_CIRQ_BASE;
static unsigned int CIRQ_IRQ_NUM;
static unsigned int CIRQ_SPI_START;
/*
 *Define Data Structure
 */
struct mt_cirq_driver {
	struct platform_driver driver;
	const struct platform_device_id *id_table;
};

/*
 * Define Global Variable
 */
static struct mt_cirq_driver mt_cirq_drv = {
	.driver = {
		.driver = {
		   .name = "cirq",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
	},
	.id_table = NULL,
};

static unsigned long cirq_clone_flush_check_val;
static unsigned long cirq_pattern_clone_flush_check_val;
static unsigned long cirq_pattern_list;



static int mt_cirq_ack(unsigned int cirq_num)
{
	void __iomem *base;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}
	base = (cirq_num / 32) * 4 + CIRQ_ACK_BASE;

	mt_reg_sync_writel(bit, base);
	return 0;
}

/*
 * mt_cirq_get_mask: Get the specified SYS_CIRQ mask
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is masked
 *    0: this cirq is umasked
 *    -1: cirq num is out of range
 */
static int mt_cirq_get_mask(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_MASK_BASE));
	return !!(st & bit);
}

/*
 * mt_cirq_mask_all: Mask all interrupts on SYS_CIRQ.
 */


/*
 * mt_cirq_ack_all: Ack all the interrupt on SYS_CIRQ
 */
void mt_cirq_ack_all(void)
{
	unsigned int i;
	unsigned int gic_pen;
	unsigned int cirq_mask;
/*
	for (i = 0; i < CIRQ_CTRL_REG_NUM; i++)
	{
		writel_relaxed(0xFFFFFFFF, CIRQ_ACK_BASE + (i * 4));
	}
*/

	for (i = 0; i < CIRQ_IRQ_NUM; i++) {
		gic_pen = mt_irq_get_pending(CIRQ_TO_IRQ_NUM(i));
		cirq_mask = mt_cirq_get_mask(i);
		if (gic_pen == 1 && cirq_mask == 0)
			continue;
		else
			mt_cirq_ack(i);
	}
	/* make sure all cirq setting take effect before doing other things */
	mb();

}
void mt_cirq_mask_all(void)
{
	unsigned int i;

	for (i = 0; i < CIRQ_CTRL_REG_NUM; i++)
		writel_relaxed(0xFFFFFFFF, CIRQ_MASK_SET_BASE + (i * 4));

	/* make sure all cirq setting take effect before doing other things */
	mb();
}

/*
 * mt_cirq_unmask_all: Unmask all interrupts on SYS_CIRQ.
 */
void mt_cirq_unmask_all(void)
{
	unsigned int i;

	for (i = 0; i < CIRQ_CTRL_REG_NUM; i++)
		writel_relaxed(0xFFFFFFFF, CIRQ_MASK_CLR_BASE + (i * 4));
	/* make sure all cirq setting take effect before doing other things */
	mb();
}

/*
 * mt_cirq_mask: Mask the specified SYS_CIRQ.
 * @cirq_num: the SYS_CIRQ number to mask
 * @return:
 *    0: mask success
 *   -1: cirq num is out of range
 */
static int mt_cirq_mask(unsigned int cirq_num)
{
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	mt_reg_sync_writel(bit, (cirq_num / 32) * 4 + CIRQ_MASK_SET_BASE);
	return 0;
}

/*
 * mt_cirq_unmask: Unmask the specified SYS_CIRQ.
 * @cirq_num: the SYS_CIRQ number to unmask
 * @return:
 *    0: umask success
 *   -1: cirq num is out of range
 */
static int mt_cirq_unmask(unsigned int cirq_num)
{
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	mt_reg_sync_writel(bit, (cirq_num / 32) * 4 + CIRQ_MASK_CLR_BASE);
	return 0;
}

/*
 * mt_cirq_get_sens: Get the specified SYS_CIRQ sensitivity
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is MT_LEVEL_SENSITIVE
 *    0: this cirq is MT_EDGE_SENSITIVE
 *   -1: cirq num is out of range
 */
static int mt_cirq_get_sens(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_SENS_BASE));
	return !!(st & bit);
}

/*
 * mt_cirq_set_sens: Set the sensitivity for the specified SYS_CIRQ number.
 * @cirq_num: the SYS_CIRQ number to set
 * @sens: sensitivity to set
 * @return:
 *    0: set sens success
 *   -1: cirq num is out of range
 */
static int mt_cirq_set_sens(unsigned int cirq_num, unsigned int sens)
{
	void __iomem *base;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	if (sens == MT_EDGE_SENSITIVE) {
		base = (cirq_num / 32) * 4 + CIRQ_SENS_CLR_BASE;
	} else if (sens == MT_LEVEL_SENSITIVE) {
		base = (cirq_num / 32) * 4 + CIRQ_SENS_SET_BASE;
	} else {
		pr_err("[CIRQ] set_sens invalid sensitivity value %d\n", sens);
		return -1;
	}

	mt_reg_sync_writel(bit, base);
	return 0;
}

/*
 * mt_cirq_get_pol: Get the specified SYS_CIRQ polarity
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is MT_CIRQ_POL_POS
 *    0: this cirq is MT_CIRQ_POL_NEG
 *   -1: cirq num is out of range
 */
static int mt_cirq_get_pol(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_POL_BASE));
	return !!(st & bit);
}

/*
 * mt_cirq_set_pol: Set the polarity for the specified SYS_CIRQ number.
 * @cirq_num: the SYS_CIRQ number to set
 * @pol: polarity to set
 * @return:
 *    0: set pol success
 *   -1: cirq num is out of range
 */
static int mt_cirq_set_pol(unsigned int cirq_num, unsigned int pol)
{
	void __iomem *base;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	if (pol == MT_CIRQ_POL_NEG) {
		base = (cirq_num / 32) * 4 + CIRQ_POL_CLR_BASE;
	} else if (pol == MT_CIRQ_POL_POS) {
		base = (cirq_num / 32) * 4 + CIRQ_POL_SET_BASE;
	} else {
		pr_err("[CIRQ] set_pol invalid polarity value %d\n", pol);
		return -1;
	}

	mt_reg_sync_writel(bit, base);
	return 0;
}

/*
 * mt_cirq_enable: Enable SYS_CIRQ
 */
void mt_cirq_enable(void)
{
	unsigned int st;


	mt_cirq_ack_all();

	st = readl(IOMEM(CIRQ_CON));
	st |=
	    (CIRQ_CON_EN << CIRQ_CON_EN_BITS) | (CIRQ_CON_EDGE_ONLY <<
						 CIRQ_CON_EDGE_ONLY_BITS);
	mt_reg_sync_writel((st & CIRQ_CON_BITS_MASK), CIRQ_CON);
}
EXPORT_SYMBOL(mt_cirq_enable);

/*
 * mt_cirq_get_pending: Get the specified SYS_CIRQ pending
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is pending
 *    0: this cirq is not pending
 */
static bool mt_cirq_get_pending(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_err("[CIRQ] %s: invalid cirq num %d\n", __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_STA_BASE));
	st = st & bit;
	return st;
}

/*
 * mt_cirq_disable: Disable SYS_CIRQ
 */
void mt_cirq_disable(void)
{
	unsigned int st;


	st = readl(IOMEM(CIRQ_CON));
	st &= ~(CIRQ_CON_EN << CIRQ_CON_EN_BITS);
	mt_reg_sync_writel((st & CIRQ_CON_BITS_MASK), CIRQ_CON);
}
EXPORT_SYMBOL(mt_cirq_disable);


/*
 * mt_cirq_disable: Flush interrupt from SYS_CIRQ to GIC
 */
void mt_cirq_flush(void)
{
	unsigned int i;
	unsigned char cirq_p_val = 0;
	unsigned char irq_p_val = 0;
	unsigned int irq_p = 0;
	unsigned char pass = 1;
	unsigned int first_cirq_found = 0;
	unsigned int first_flushed_cirq;
	unsigned int first_irq_flushedto;
	unsigned int last_fluashed_cirq;
	unsigned int last_irq_flushedto;

	if (cirq_pattern_clone_flush_check_val == 1) {
		if (cirq_pattern_list < CIRQ_IRQ_NUM) {
			mt_cirq_unmask(cirq_pattern_list);
			mt_cirq_set_sens(cirq_pattern_list, MT_EDGE_SENSITIVE);
			mt_cirq_set_pol(cirq_pattern_list, MT_CIRQ_POL_NEG);
			mt_cirq_set_pol(cirq_pattern_list, MT_CIRQ_POL_POS);
			mt_cirq_set_pol(cirq_pattern_list, MT_CIRQ_POL_NEG);
		} else {
			pr_debug
			("[CIRQ] no pattern to test, input pattern first\n");
		}
		pr_debug("[CIRQ] cirq_pattern %ld, cirq_p %d,",
		cirq_pattern_list, mt_cirq_get_pending(cirq_pattern_list));
		pr_debug("cirq_s %d, cirq_con 0x%x\n",
		mt_cirq_get_sens(cirq_pattern_list), readl(IOMEM(CIRQ_CON)));
	}

	mt_cirq_unmask_all();

	for (i = 0; i < CIRQ_IRQ_NUM; i++) {
		cirq_p_val = mt_cirq_get_pending(i);
		if (cirq_p_val)
			mt_irq_set_pending(CIRQ_TO_IRQ_NUM(i));

		if (cirq_clone_flush_check_val == 1) {
			if (cirq_p_val == 0)
				continue;
			irq_p = CIRQ_TO_IRQ_NUM(i);
			irq_p_val = mt_irq_get_pending(irq_p);
			if (cirq_p_val != irq_p_val) {
				pr_err
			("[CIRQ] CIRQ Flush Failed %d(cirq %d) != %d(gic %d)\n",
				     cirq_p_val, i, irq_p_val,
				     CIRQ_TO_IRQ_NUM(i));
				pass = 0;
			} else {
				pr_debug
			("[CIRQ] CIRQ Flush Pass %d(cirq %d) = %d(gic %d)\n",
				     cirq_p_val, i, irq_p_val,
				     CIRQ_TO_IRQ_NUM(i));
			}
			if (!first_cirq_found) {
				first_flushed_cirq = i;
				first_irq_flushedto = irq_p;
				first_cirq_found = 1;
			}
			last_fluashed_cirq = i;
			last_irq_flushedto = irq_p;
		}
	}

	if (cirq_clone_flush_check_val == 1) {
		if (first_cirq_found) {
			pr_debug("[CIRQ] The first flush : CIRQ%d to IRQ%d\n",
				  first_flushed_cirq, first_irq_flushedto);
			pr_debug("[CIRQ] The last flush : CIRQ%d to IRQ%d\n",
				  last_fluashed_cirq, last_irq_flushedto);
		} else {
			pr_debug
			    ("[CIRQ] There are no pending interrupt in CIRQ");
			pr_debug("so no flush operation happend\n");
		}
		pr_debug
	("[CIRQ] The Flush Max Range : CIRQ%d to IRQ%d ~ CIRQ%d to IRQ%d\n",
		     0, CIRQ_TO_IRQ_NUM(0), CIRQ_IRQ_NUM - 1,
		     CIRQ_TO_IRQ_NUM(CIRQ_IRQ_NUM - 1));
		pr_debug
		    ("[CIRQ] Flush Check %s, Confirm:SPI_START_OFFSET:%d\n",
		     pass == 1 ? "Pass" : "Failed", CIRQ_SPI_START);
	}
	mt_cirq_mask_all();
	mt_cirq_ack_all();
	return;

}
EXPORT_SYMBOL(mt_cirq_flush);

__attribute__((weak)) u32 mt_irq_get_pol(u32 irq);

/*
 * mt_cirq_clone_pol: Copy the polarity setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_pol(void)
{
	unsigned int cirq_num, irq_num;
	unsigned int st;
	unsigned int bit;


	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		irq_num = CIRQ_TO_IRQ_NUM(cirq_num);

		if (cirq_num == 0 || irq_num % 32 == 0) {
			if (mt_irq_get_pol) {
				st = mt_irq_get_pol(irq_num);
			} else {
				st = readl(IOMEM
				   (INT_POL_CTL0 +
				    ((irq_num -
				      GIC_PRIVATE_SIGNALS) / 32 * 4)));
			}
		}

		bit = 0x1 << ((irq_num - GIC_PRIVATE_SIGNALS) % 32);

		if (st & bit)
			mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_NEG);
		else
			mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_POS);
	}
}

/*
 * mt_cirq_clone_sens: Copy the sensitivity setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_sens(void)
{
	unsigned int cirq_num, irq_num;
	unsigned int st;
	unsigned int bit;

	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		irq_num = CIRQ_TO_IRQ_NUM(cirq_num);

		if (cirq_num == 0 || irq_num % 16 == 0) {
			st = readl(IOMEM
				   (GIC_DIST_BASE + GIC_DIST_CONFIG +
				    (irq_num / 16 * 4)));
		}

		bit = 0x2 << ((irq_num % 16) * 2);

		if (st & bit)
			mt_cirq_set_sens(cirq_num, MT_EDGE_SENSITIVE);
		else
			mt_cirq_set_sens(cirq_num, MT_LEVEL_SENSITIVE);
	}
}

/*
 * mt_cirq_clone_mask: Copy the mask setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_mask(void)
{
	unsigned int cirq_num, irq_num;
	unsigned int st;
	unsigned int bit;


	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		irq_num = CIRQ_TO_IRQ_NUM(cirq_num);

		if (cirq_num == 0 || irq_num % 32 == 0) {
			st = readl(IOMEM
				   (GIC_DIST_BASE + GIC_DIST_ENABLE_SET +
				    (irq_num / 32 * 4)));
		}

		bit = 0x1 << (irq_num % 32);

		if (st & bit)
			mt_cirq_unmask(cirq_num);
		else
			mt_cirq_mask(cirq_num);
	}
}

/*
 * mt_cirq_clone_gic: Copy the setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_gic(void)
{
	mt_cirq_clone_pol();
	mt_cirq_clone_sens();
	mt_cirq_clone_mask();
	if (cirq_clone_flush_check_val)
		mt_cirq_dump_reg();
}
EXPORT_SYMBOL(mt_cirq_clone_gic);


#if defined(LDVT)
/*
 * cirq_dvt_show: To show usage.
 */
static ssize_t cirq_dvt_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "==CIRQ dvt test==\n"
			"1.CIRQ dump regs\n"
			"2.CIRQ tests\n" "3.CIRQ disable\n");
}

/*
 * mci_dvt_store: To select mci test case.
 */
static ssize_t cirq_dvt_store(struct device_driver *driver, const char *buf,
			      size_t count)
{
	char *p = (char *)buf;
	unsigned long num;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&num);

	switch (num) {
	case 1:
		mt_cirq_clone_gic();
		mt_cirq_dump_reg();
		break;
	case 2:
		mt_cirq_test();
		break;
	case 3:
		mt_cirq_disable();
		break;
	default:
		break;
	}

	return count;
}

DRIVER_ATTR(cirq_dvt, 0664, cirq_dvt_show, cirq_dvt_store);
#endif

/*
 * cirq_clone_flush_check_show:
 * To show if we do cirq clone/flush value's check.
 */
static ssize_t cirq_clone_flush_check_show(struct device_driver *driver,
					   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n",
			cirq_clone_flush_check_val);
}

/*
 * cirq_clone_flush_check_store:
 * set 1 if we need to enable clone/flush value's check
 */
static ssize_t cirq_clone_flush_check_store(struct device_driver *driver,
					    const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned long value;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&value);
	cirq_clone_flush_check_val = value;
	return count;
}

DRIVER_ATTR(cirq_clone_flush_check, 0664, cirq_clone_flush_check_show,
	    cirq_clone_flush_check_store);

/*
 * cirq_pattern_clone_flush_check_show:
 * To show if we do need to do pattern test.
 */
static ssize_t cirq_pattern_clone_flush_check_show(struct device_driver *driver,
						   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n",
			cirq_pattern_clone_flush_check_val);
}

/*
 * cirq_pattern_clone_flush_check_show:  set 1 if we need to do pattern test.
 */
static ssize_t cirq_pattern_clone_flush_check_store(struct device_driver
						    *driver, const char *buf,
						    size_t count)
{
	char *p = (char *)buf;
	unsigned long value;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&value);
	cirq_pattern_clone_flush_check_val = value;
	return count;
}

DRIVER_ATTR(cirq_pattern_clone_flush_check, 0664,
	    cirq_pattern_clone_flush_check_show,
	    cirq_pattern_clone_flush_check_store);

/*
 * cirq_pattern_clone_flush_check_show:
 * To show if we do need to do pattern test.
 */
static ssize_t cirq_pattern_list_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n", cirq_pattern_list);
}

/*
 * cirq_pattern_clone_flush_check_show:  set 1 if we need to do pattern test.
 */
static ssize_t cirq_pattern_list_store(struct device_driver *driver,
				       const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned long value;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&value);
	cirq_pattern_list = value;
	return count;
}

DRIVER_ATTR(cirq_pattern_list, 0664, cirq_pattern_list_show,
	    cirq_pattern_list_store);

#if defined(__CHECK_IRQ_TYPE)
#define X_DEFINE_IRQ(__name, __num, __polarity, __sensitivity) \
	{ .num = __num, .polarity = __polarity, .sensitivity = __sensitivity, },
#define L 0
#define H 1
#define EDGE MT_EDGE_SENSITIVE
#define LEVEL MT_LEVEL_SENSITIVE
struct __check_irq_type {
	int num;
	int polarity;
	int sensitivity;
};
#undef __X_DEFINE_IRQ
struct __check_irq_type __check_irq_type[] = {
#include <mach/x_define_irq.h>
	{.num = -1,},
};

#undef X_DEFINE_IRQ
#undef L
#undef H
#undef EDGE
#undef LEVEL
#endif

void mt_cirq_dump_reg(void)
{
	int cirq_num;
	int pol, sens, mask;
	int pen;
#if defined(__CHECK_IRQ_TYPE)
	int irq_iter;
	unsigned char pass = 1;
#endif

	pr_debug("[CIRQ] IRQ:\tPOL\tSENS\tMASK\tGIC_PENDING\n");
	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		pol = mt_cirq_get_pol(cirq_num);
		sens = mt_cirq_get_sens(cirq_num);
		mask = mt_cirq_get_mask(cirq_num);
		pen =  mt_irq_get_pending(CIRQ_TO_IRQ_NUM(cirq_num));
#if defined(__CHECK_IRQ_TYPE)
		if (mask == 0) {
			pr_debug("[CIRQ] IRQ:%d\t%d\t%d\t%d\t%d\n",
				 CIRQ_TO_IRQ_NUM(cirq_num),
				 pol,
				 sens,
				 mask,
				 pen);
			irq_iter = cirq_num + 32;
			if (__check_irq_type[irq_iter].num ==
			    CIRQ_TO_IRQ_NUM(cirq_num)) {
				if (__check_irq_type[irq_iter].sensitivity !=
				    sens) {
					pr_debug
					    ("[CIRQ] Error sens in irq:%d\n",
					     __check_irq_type[irq_iter].num);
					pass = 0;
				}
				if (__check_irq_type[irq_iter].polarity
					!= pol) {
					pr_debug
					    ("[CIRQ]Err polarity in irq:%d\n",
					     __check_irq_type[irq_iter].num);
					pass = 0;
				}
			} else {
				pr_err
				    ("[CIRQ] Error CIRQ num %d",
					__check_irq_type[irq_iter].num);
				pr_err("Mapping to wrong GIC num %d\n",
					CIRQ_TO_IRQ_NUM(cirq_num));
				pass = 0;
			}
		}
#endif
	}
#if defined __CHECK_IRQ_TYPE
	pr_debug("[CIRQ] CIRQ Clone To GIC Verfication %s !\n",
		  pass == 1 ? "Pass" : "Failed");
#else
	pr_debug
	    ("[CIRQ] Plz enable __CHECK_IRQ_TYE");
	pr_debug("and update x_define.h for enable CIRQ Clone checking\n");
#endif
}

#ifdef LDVT
int mt_cirq_test(void)
{
	int cirq_num = 126;


	/*test polarity */
	mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_NEG);
	if (mt_cirq_get_pol(cirq_num) != MT_CIRQ_POL_NEG)
		pr_debug("[CIRQ] mt_cirq_set_pol clear test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_pol clear test passed!!\n");
	mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_POS);
	if (mt_cirq_get_pol(cirq_num) != MT_CIRQ_POL_POS)
		pr_debug("[CIRQ] mt_cirq_set_pol set test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_pol set test passed!!\n");

	/*test sensitivity */
	mt_cirq_set_sens(cirq_num, MT_EDGE_SENSITIVE);
	if (mt_cirq_get_sens(cirq_num) != MT_EDGE_SENSITIVE)
		pr_debug("[CIRQ] mt_cirq_set_sens clear test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_sens clear test passed!!\n");
	mt_cirq_set_sens(cirq_num, MT_LEVEL_SENSITIVE);
	if (mt_cirq_get_sens(cirq_num) != MT_LEVEL_SENSITIVE)
		pr_debug("[CIRQ] mt_cirq_set_sens set test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_sens set test passed!!\n");

	/*test mask */
	mt_cirq_mask(cirq_num);
	if (mt_cirq_get_mask(cirq_num) != 1)
		pr_debug("[CIRQ] mt_cirq_mask test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_mask test passed!!\n");
	mt_cirq_unmask(cirq_num);
	if (mt_cirq_get_mask(cirq_num) != 0)
		pr_debug("[CIRQ] mt_cirq_unmask test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_unmask test passed!!\n");

	mt_cirq_clone_gic();
	mt_cirq_dump_reg();

	return 0;
}
#endif

/*
 * cirq_irq_handler: SYS_CIRQ interrupt service routine.
 */
static irqreturn_t cirq_irq_handler(int irq, void *dev_id)
{
	pr_debug("[CIRQ] CIRQ_Handler\n");

	mt_cirq_ack_all();

	return IRQ_HANDLED;
}

/*
 * mt_cirq_init: SYS_CIRQ init function
 * always return 0
 */
int __init mt_cirq_init(void)
{
	int ret;
#ifdef CONFIG_OF
	struct device_node *node;
	unsigned int sys_cirq_num = 0;
#endif

	pr_debug("[CIRQ] CIRQ init...\n");

#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6735-sys_cirq");
	if (!node) {
		pr_debug("[CIRQ] find SYS_CIRQ node failed!!!\n");
		return -1;
	}
	SYS_CIRQ_BASE = of_iomap(node, 0);
	pr_debug("[CIRQ] SYS_CIRQ_BASE = 0x%p\n", SYS_CIRQ_BASE);
	WARN(!SYS_CIRQ_BASE,
			"[CIRQ] unable to map SYS_CIRQ base registers!!!\n");

	if (of_property_read_u32(node, "mediatek,cirq_num", &CIRQ_IRQ_NUM))
		return -1;
	pr_debug("[CIRQ] cirq_num = %d\n", CIRQ_IRQ_NUM);

	if (of_property_read_u32(node, "mediatek,spi_start_offset",
				&CIRQ_SPI_START))
		return -1;
	pr_debug("[CIRQ] spi_start_offset = %d\n", CIRQ_SPI_START);

	sys_cirq_num = irq_of_parse_and_map(node, 0);
	pr_debug("[CIRQ] sys_cirq_num = %d\n", sys_cirq_num);
#endif

#ifdef CONFIG_OF
	ret =
		request_irq(sys_cirq_num, cirq_irq_handler, IRQF_TRIGGER_NONE,
				"CIRQ", NULL);
#else
	ret =
		request_irq(SYS_CIRQ_IRQ_BIT_ID, cirq_irq_handler,
				IRQF_TRIGGER_LOW, "CIRQ", NULL);
#endif

	if (ret > 0)
		pr_err("[CIRQ] CIRQ IRQ LINE NOT AVAILABLE!!\n");
	else
		pr_debug("[CIRQ] CIRQ handler init success.\n");

	ret = platform_driver_register(&mt_cirq_drv.driver);
	if (ret == 0)
		pr_debug("[CIRQ] CIRQ init done...\n");

#ifdef LDVT
	ret = driver_create_file(&mt_cirq_drv.driver.driver,
					&driver_attr_cirq_dvt);
	if (ret == 0)
		pr_debug("[CIRQ] CIRQ create sysfs file for dvt done...\n");
#endif

	ret = driver_create_file(&mt_cirq_drv.driver.driver,
			&driver_attr_cirq_clone_flush_check);
	if (ret == 0)
		pr_debug
		("[CIRQ] sysfs file for cirq clone flush check done...\n");

	ret = driver_create_file(&mt_cirq_drv.driver.driver,
			&driver_attr_cirq_pattern_clone_flush_check);
	if (ret == 0)
		pr_debug
		("[CIRQ] sysfs file for pattern clone flush check done...\n");
	cirq_pattern_list = CIRQ_IRQ_NUM;
	ret = driver_create_file(&mt_cirq_drv.driver.driver,
			&driver_attr_cirq_pattern_list);
	if (ret == 0)
		pr_debug
		("[CIRQ] CIRQ create sysfs file for pattern list setup...\n");

	pr_warn("### CIRQ init done. ###\n");

	return 0;
}

arch_initcall(mt_cirq_init);
