/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/edac.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include "edac_mc.h"
#include "edac_device.h"

#ifdef CONFIG_EDAC_QCOM_LLCC_PANIC_ON_CE
#define LLCC_ERP_PANIC_ON_CE 1
#else
#define LLCC_ERP_PANIC_ON_CE 0
#endif

#ifdef CONFIG_EDAC_QCOM_LLCC_PANIC_ON_UE
#define LLCC_ERP_PANIC_ON_UE 1
#else
#define LLCC_ERP_PANIC_ON_UE 0
#endif

#define EDAC_LLCC	"qcom_llcc"

#define TRP_SYN_REG_CNT	6

#define DRP_SYN_REG_CNT	8

#define LLCC_COMMON_STATUS0		0x0003000C
#define LLCC_LB_CNT_MASK		0xf0000000
#define LLCC_LB_CNT_SHIFT		28

/* single & Double Bit syndrome register offsets */
#define TRP_ECC_SB_ERR_SYN0		0x0002304C
#define TRP_ECC_DB_ERR_SYN0		0x00020370
#define DRP_ECC_SB_ERR_SYN0		0x0004204C
#define DRP_ECC_DB_ERR_SYN0		0x00042070

/* Error register offsets */
#define TRP_ECC_ERROR_STATUS1		0x00020348
#define TRP_ECC_ERROR_STATUS0		0x00020344
#define DRP_ECC_ERROR_STATUS1		0x00042048
#define DRP_ECC_ERROR_STATUS0		0x00042044

/* TRP, DRP interrupt register offsets */
#define DRP_INTERRUPT_STATUS		0x00041000
#define TRP_INTERRUPT_0_STATUS		0x00020480
#define DRP_INTERRUPT_CLEAR		0x00041008
#define DRP_ECC_ERROR_CNTR_CLEAR	0x00040004
#define TRP_INTERRUPT_0_CLEAR		0x00020484
#define TRP_ECC_ERROR_CNTR_CLEAR	0x00020440

/* Mask and shift macros */
#define ECC_DB_ERR_COUNT_MASK	0x0000001f
#define ECC_DB_ERR_WAYS_MASK	0xffff0000
#define ECC_DB_ERR_WAYS_SHIFT	16

#define ECC_SB_ERR_COUNT_MASK	0x00ff0000
#define ECC_SB_ERR_COUNT_SHIFT	16
#define ECC_SB_ERR_WAYS_MASK	0x0000ffff

#define SB_ECC_ERROR	0x1
#define DB_ECC_ERROR	0x2

#define DRP_TRP_INT_CLEAR	0x3
#define DRP_TRP_CNT_CLEAR	0x3

#ifdef CONFIG_EDAC_LLCC_POLL
static int poll_msec = 5000;
module_param(poll_msec, int, 0444);
#endif

static int interrupt_mode = 1;
module_param(interrupt_mode, int, 0444);
MODULE_PARM_DESC(interrupt_mode,
		 "Controls whether to use interrupt or poll mode");

enum {
	LLCC_DRAM_CE = 0,
	LLCC_DRAM_UE,
	LLCC_TRAM_CE,
	LLCC_TRAM_UE,
};

struct errors_edac {
	const char *msg;
	void (*func)(struct edac_device_ctl_info *edev_ctl,
				int inst_nr, int block_nr, const char *msg);
};

struct erp_drvdata {
	struct regmap *llcc_map;
	u32 *llcc_banks;
	u32 ecc_irq;
	u32 num_banks;
	u32 b_off;
};

static const struct errors_edac errors[] = {
	{"LLCC Data RAM correctable Error", edac_device_handle_ce},
	{"LLCC Data RAM uncorrectable Error", edac_device_handle_ue},
	{"LLCC Tag RAM correctable Error", edac_device_handle_ce},
	{"LLCC Tag RAM uncorrectable Error", edac_device_handle_ue},
};

/* Clear the error interrupt and counter registers */
static void qcom_llcc_clear_errors(int err_type, struct erp_drvdata *drv)
{
	switch (err_type) {
	case LLCC_DRAM_CE:
	case LLCC_DRAM_UE:
		/* Clear the interrupt */
		regmap_write(drv->llcc_map, drv->b_off + DRP_INTERRUPT_CLEAR,
			DRP_TRP_INT_CLEAR);
		/* Clear the counters */
		regmap_write(drv->llcc_map,
			drv->b_off + DRP_ECC_ERROR_CNTR_CLEAR,
			DRP_TRP_CNT_CLEAR);
		break;
	case LLCC_TRAM_CE:
	case LLCC_TRAM_UE:
		regmap_write(drv->llcc_map, drv->b_off + TRP_INTERRUPT_0_CLEAR,
			     DRP_TRP_INT_CLEAR);
		regmap_write(drv->llcc_map,
			drv->b_off + TRP_ECC_ERROR_CNTR_CLEAR,
			DRP_TRP_CNT_CLEAR);
		break;
	}
}

/* Dump syndrome registers for tag Ram Double bit errors */
static void dump_trp_db_syn_reg(struct erp_drvdata *drv, u32 bank)
{
	int i;
	int db_err_cnt;
	int db_err_ways;
	u32 synd_reg;
	u32 synd_val;

	for (i = 0; i < TRP_SYN_REG_CNT; i++) {
		synd_reg = TRP_ECC_DB_ERR_SYN0 + (i * 4);
		regmap_read(drv->llcc_map, drv->llcc_banks[bank] + synd_reg,
			&synd_val);
		edac_printk(KERN_CRIT, EDAC_LLCC, "TRP_ECC_SYN%d: 0x%8x\n",
			i, synd_val);
	}

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + TRP_ECC_ERROR_STATUS1, &db_err_cnt);
	db_err_cnt = (db_err_cnt & ECC_DB_ERR_COUNT_MASK);
	edac_printk(KERN_CRIT, EDAC_LLCC, "Double-Bit error count: 0x%4x\n",
		db_err_cnt);

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + TRP_ECC_ERROR_STATUS0, &db_err_ways);
	db_err_ways = (db_err_ways & ECC_DB_ERR_WAYS_MASK);
	db_err_ways >>= ECC_DB_ERR_WAYS_SHIFT;

	edac_printk(KERN_CRIT, EDAC_LLCC, "Double-Bit error ways: 0x%4x\n",
		db_err_ways);
}

/* Dump syndrome register for tag Ram Single Bit Errors */
static void dump_trp_sb_syn_reg(struct erp_drvdata *drv, u32 bank)
{
	int i;
	int sb_err_cnt;
	int sb_err_ways;
	u32 synd_reg;
	u32 synd_val;

	for (i = 0; i < TRP_SYN_REG_CNT; i++) {
		synd_reg = TRP_ECC_SB_ERR_SYN0 + (i * 4);
		regmap_read(drv->llcc_map, drv->llcc_banks[bank] + synd_reg,
			&synd_val);
		edac_printk(KERN_CRIT, EDAC_LLCC, "TRP_ECC_SYN%d: 0x%8x\n",
			i, synd_val);
	}

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + TRP_ECC_ERROR_STATUS1, &sb_err_cnt);
	sb_err_cnt = (sb_err_cnt & ECC_SB_ERR_COUNT_MASK);
	sb_err_cnt >>= ECC_SB_ERR_COUNT_SHIFT;
	edac_printk(KERN_CRIT, EDAC_LLCC, "Single-Bit error count: 0x%4x\n",
		sb_err_cnt);

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + TRP_ECC_ERROR_STATUS0, &sb_err_ways);
	sb_err_ways = sb_err_ways & ECC_SB_ERR_WAYS_MASK;

	edac_printk(KERN_CRIT, EDAC_LLCC, "Single-Bit error ways: 0x%4x\n",
		sb_err_ways);
}

/* Dump syndrome registers for Data Ram Double bit errors */
static void dump_drp_db_syn_reg(struct erp_drvdata *drv, u32 bank)
{
	int i;
	int db_err_cnt;
	int db_err_ways;
	u32 synd_reg;
	u32 synd_val;

	for (i = 0; i < DRP_SYN_REG_CNT; i++) {
		synd_reg = DRP_ECC_DB_ERR_SYN0 + (i * 4);
		regmap_read(drv->llcc_map, drv->llcc_banks[bank] + synd_reg,
			&synd_val);
		edac_printk(KERN_CRIT, EDAC_LLCC, "DRP_ECC_SYN%d: 0x%8x\n",
			i, synd_val);
	}

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + DRP_ECC_ERROR_STATUS1, &db_err_cnt);
	db_err_cnt = (db_err_cnt & ECC_DB_ERR_COUNT_MASK);
	edac_printk(KERN_CRIT, EDAC_LLCC, "Double-Bit error count: 0x%4x\n",
		db_err_cnt);

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + DRP_ECC_ERROR_STATUS0, &db_err_ways);
	db_err_ways &= ECC_DB_ERR_WAYS_MASK;
	db_err_ways >>= ECC_DB_ERR_WAYS_SHIFT;
	edac_printk(KERN_CRIT, EDAC_LLCC, "Double-Bit error ways: 0x%4x\n",
		db_err_ways);
}

/* Dump Syndrome registers for Data Ram Single bit errors*/
static void dump_drp_sb_syn_reg(struct erp_drvdata *drv, u32 bank)
{
	int i;
	int sb_err_cnt;
	int sb_err_ways;
	u32 synd_reg;
	u32 synd_val;

	for (i = 0; i < DRP_SYN_REG_CNT; i++) {
		synd_reg = DRP_ECC_SB_ERR_SYN0 + (i * 4);
		regmap_read(drv->llcc_map, drv->llcc_banks[bank] + synd_reg,
			&synd_val);
		edac_printk(KERN_CRIT, EDAC_LLCC, "DRP_ECC_SYN%d: 0x%8x\n",
			i, synd_val);
	}

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + DRP_ECC_ERROR_STATUS1, &sb_err_cnt);
	sb_err_cnt &= ECC_SB_ERR_COUNT_MASK;
	sb_err_cnt >>= ECC_SB_ERR_COUNT_SHIFT;
	edac_printk(KERN_CRIT, EDAC_LLCC, "Single-Bit error count: 0x%4x\n",
		sb_err_cnt);

	regmap_read(drv->llcc_map,
		drv->llcc_banks[bank] + DRP_ECC_ERROR_STATUS0, &sb_err_ways);
	sb_err_ways = sb_err_ways & ECC_SB_ERR_WAYS_MASK;

	edac_printk(KERN_CRIT, EDAC_LLCC, "Single-Bit error ways: 0x%4x\n",
		sb_err_ways);
}


static void dump_syn_reg(struct edac_device_ctl_info *edev_ctl,
			 int err_type, u32 bank)
{
	struct erp_drvdata *drv = edev_ctl->pvt_info;

	switch (err_type) {
	case LLCC_DRAM_CE:
		dump_drp_sb_syn_reg(drv, bank);
		break;
	case LLCC_DRAM_UE:
		dump_drp_db_syn_reg(drv, bank);
		break;
	case LLCC_TRAM_CE:
		dump_trp_sb_syn_reg(drv, bank);
		break;
	case LLCC_TRAM_UE:
		dump_trp_db_syn_reg(drv, bank);
		break;
	}

	qcom_llcc_clear_errors(err_type, drv);

	errors[err_type].func(edev_ctl, 0, bank, errors[err_type].msg);
}

static irqreturn_t qcom_llcc_check_cache_errors
		(struct edac_device_ctl_info *edev_ctl)
{
	u32 drp_error;
	u32 trp_error;
	struct erp_drvdata *drv = edev_ctl->pvt_info;
	u32 i;
	irqreturn_t irq_rc = IRQ_NONE;

	for (i = 0; i < drv->num_banks; i++) {
		/* Look for Data RAM errors */
		regmap_read(drv->llcc_map,
			drv->llcc_banks[i] + DRP_INTERRUPT_STATUS, &drp_error);

		if (drp_error & SB_ECC_ERROR) {
			edac_printk(KERN_CRIT, EDAC_LLCC,
				"Single Bit Error detected in Data Ram\n");
			dump_syn_reg(edev_ctl, LLCC_DRAM_CE, i);
			irq_rc = IRQ_HANDLED;
		} else if (drp_error & DB_ECC_ERROR) {
			edac_printk(KERN_CRIT, EDAC_LLCC,
				"Double Bit Error detected in Data Ram\n");
			dump_syn_reg(edev_ctl, LLCC_DRAM_UE, i);
			irq_rc = IRQ_HANDLED;
		}

		/* Look for Tag RAM errors */
		regmap_read(drv->llcc_map,
			drv->llcc_banks[i] + TRP_INTERRUPT_0_STATUS,
			&trp_error);
		if (trp_error & SB_ECC_ERROR) {
			edac_printk(KERN_CRIT, EDAC_LLCC,
				"Single Bit Error detected in Tag Ram\n");
			dump_syn_reg(edev_ctl, LLCC_TRAM_CE, i);
			irq_rc = IRQ_HANDLED;
		} else if (trp_error & DB_ECC_ERROR) {
			edac_printk(KERN_CRIT, EDAC_LLCC,
				"Double Bit Error detected in Tag Ram\n");
			dump_syn_reg(edev_ctl, LLCC_TRAM_UE, i);
			irq_rc = IRQ_HANDLED;
		}
	}

	return irq_rc;
}

#ifdef CONFIG_EDAC_LLCC_POLL
static void qcom_llcc_poll_cache_errors(struct edac_device_ctl_info *edev_ctl)
{
	qcom_llcc_check_cache_errors(edev_ctl);
}
#endif

static irqreturn_t llcc_ecc_irq_handler
			(int irq, void *edev_ctl)
{
	return qcom_llcc_check_cache_errors(edev_ctl);
}

static int qcom_llcc_erp_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct erp_drvdata *drv;
	struct edac_device_ctl_info *edev_ctl;
	struct device *dev = &pdev->dev;
	u32 num_banks;
	struct regmap *llcc_map = NULL;

	llcc_map = syscon_node_to_regmap(dev->parent->of_node);
	if (IS_ERR(llcc_map)) {
		dev_err(dev, "no regmap for syscon llcc parent\n");
		return -ENOMEM;
	}

	/* Find the number of LLC banks supported */
	regmap_read(llcc_map, LLCC_COMMON_STATUS0,
		    &num_banks);

	num_banks &= LLCC_LB_CNT_MASK;
	num_banks >>= LLCC_LB_CNT_SHIFT;

	/* Allocate edac control info */
	edev_ctl = edac_device_alloc_ctl_info(sizeof(*drv), "qcom-llcc", 1,
			"bank", num_banks, 1, NULL, 0,
			edac_device_alloc_index());

	if (!edev_ctl)
		return -ENOMEM;

	edev_ctl->dev = dev;
	edev_ctl->mod_name = dev_name(dev);
	edev_ctl->dev_name = dev_name(dev);
	edev_ctl->ctl_name = "llcc";
#ifdef CONFIG_EDAC_LLCC_POLL
	edev_ctl->poll_msec = poll_msec;
	edev_ctl->edac_check = qcom_llcc_poll_cache_errors;
	edev_ctl->defer_work = 1;
#endif
	edev_ctl->panic_on_ce = LLCC_ERP_PANIC_ON_CE;
	edev_ctl->panic_on_ue = LLCC_ERP_PANIC_ON_UE;

	drv = edev_ctl->pvt_info;
	drv->num_banks = num_banks;
	drv->llcc_map = llcc_map;

	rc = edac_device_add_device(edev_ctl);
	if (rc)
		goto out_mem;

	drv->llcc_banks = devm_kzalloc(&pdev->dev,
		sizeof(u32) * drv->num_banks, GFP_KERNEL);

	if (!drv->llcc_banks) {
		dev_err(dev, "Cannot allocate memory for llcc_banks\n");
		rc = -ENOMEM;
		goto out_dev;
	}

	rc = of_property_read_u32_array(dev->parent->of_node,
			"qcom,llcc-banks-off", drv->llcc_banks, drv->num_banks);
	if (rc) {
		dev_err(dev, "Cannot read llcc-banks-off property\n");
		goto out_dev;
	}

	rc = of_property_read_u32(dev->parent->of_node,
			"qcom,llcc-broadcast-off", &drv->b_off);
	if (rc) {
		dev_err(dev, "Cannot read llcc-broadcast-off property\n");
		goto out_dev;
	}

	platform_set_drvdata(pdev, edev_ctl);

	if (interrupt_mode) {
		drv->ecc_irq = platform_get_irq_byname(pdev, "ecc_irq");
		if (!drv->ecc_irq) {
			rc = -ENODEV;
			goto out_dev;
		}

		rc = devm_request_irq(dev, drv->ecc_irq, llcc_ecc_irq_handler,
				IRQF_SHARED | IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				"llcc_ecc", edev_ctl);
		if (rc) {
			dev_err(dev, "failed to request ecc irq\n");
			goto out_dev;
		}
	}

	return 0;

out_dev:
	edac_device_del_device(edev_ctl->dev);
out_mem:
	edac_device_free_ctl_info(edev_ctl);

	return rc;
}

static int qcom_llcc_erp_remove(struct platform_device *pdev)
{
	struct edac_device_ctl_info *edev_ctl = dev_get_drvdata(&pdev->dev);

	edac_device_del_device(edev_ctl->dev);
	edac_device_free_ctl_info(edev_ctl);

	return 0;
}

static const struct of_device_id qcom_llcc_erp_match_table[] = {
	{ .compatible = "qcom,llcc-erp" },
	{ },
};

static struct platform_driver qcom_llcc_erp_driver = {
	.probe = qcom_llcc_erp_probe,
	.remove = qcom_llcc_erp_remove,
	.driver = {
		.name = "qcom_llcc_erp",
		.owner = THIS_MODULE,
		.of_match_table = qcom_llcc_erp_match_table,
	},
};

static int __init qcom_llcc_erp_init(void)
{
	return platform_driver_register(&qcom_llcc_erp_driver);
}
module_init(qcom_llcc_erp_init);

static void __exit qcom_llcc_erp_exit(void)
{
	platform_driver_unregister(&qcom_llcc_erp_driver);
}
module_exit(qcom_llcc_erp_exit);

MODULE_DESCRIPTION("QCOM LLCC Error Reporting");
MODULE_LICENSE("GPL v2");
