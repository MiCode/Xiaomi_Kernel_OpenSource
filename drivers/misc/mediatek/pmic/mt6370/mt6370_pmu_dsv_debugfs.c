/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include "inc/mt6370_pmu.h"
#include "inc/mt6370_pmu_dsv_debugfs.h"
#include <mt-plat/aee.h>


int irq_count[DSV_MODE_MAX];

#define IRQ_COUNT_MAX 20
#define MT6370_DB_VBST_MAX_V 0x2B	/*6.15v*/
#define MT6370_PMU_REG_DB_VBST_MASK 0x3F
#define DB_MASK_DEFAULT_SHIFT 0x3

int g_db_vbst;
int g_vbst_adjustment;
int g_irq_count_max;
int g_irq_mask;
int g_irq_mask_warning;
int g_irq_disable;

int mt6370_pmu_dsv_scp_ocp_irq_debug(struct mt6370_pmu_chip *chip,
					enum dsv_dbg_mode_t mode)
{
	int ret = 0;
	int dbvbst, dbvpos, dbvneg, dbmask;
	char str[50] = "";

	if (!g_irq_mask)
		return ret;

	if (!(g_irq_disable & (1 << mode)))
		return ret;

	irq_count[mode] = irq_count[mode] + 1;

	if (irq_count[mode] > g_irq_count_max) {
		irq_count[mode] = 0;

		mt6370_pmu_reg_update_bits(chip, MT6370_PMU_DBMASK,
				1 << (DB_MASK_DEFAULT_SHIFT + mode),
				1 << (DB_MASK_DEFAULT_SHIFT + mode));

		dbvbst = mt6370_pmu_reg_read(chip, MT6370_PMU_REG_DBVBST);
		dbvpos = mt6370_pmu_reg_read(chip, MT6370_PMU_REG_DBVPOS);
		dbvneg = mt6370_pmu_reg_read(chip, MT6370_PMU_REG_DBVNEG);
		dbmask = mt6370_pmu_reg_read(chip, MT6370_PMU_DBMASK);
		pr_info("%s: DB_VBST = 0x%x, DB_VPOS = 0x%x, DB_VNEG = 0x%x, DBMASK = 0x%x\n",
			__func__, dbvbst, dbvpos, dbvneg, dbmask);

		snprintf(str, 50, "Vbst=0x%x,Vpos=0x%x,Vneg=0x%x,mask=0x%x",
			dbvbst, dbvpos, dbvneg, dbmask);
		if (g_irq_mask_warning)
			aee_kernel_warning("mt6370 dsv irq",
				"db irq type = %x %s\n", mode, str);
		ret = 1;
	}

	return ret;
}


void mt6370_pmu_dsv_auto_vbst_adjustment(struct mt6370_pmu_chip *chip,
					enum dsv_dbg_mode_t mode)
{
	int db_vbst;

	if (!g_vbst_adjustment)
		return;

	irq_count[mode] = irq_count[mode] + 1;

	if (irq_count[mode] > g_irq_count_max) {
		irq_count[mode] = 0;

		g_db_vbst = mt6370_pmu_reg_read(chip, MT6370_PMU_REG_DBVBST);
		db_vbst = MT6370_PMU_REG_DB_VBST_MASK & g_db_vbst;

		if (db_vbst < MT6370_DB_VBST_MAX_V) {
			/*0.05V per step*/
			mt6370_pmu_reg_update_bits(chip, MT6370_PMU_REG_DBVBST,
				MT6370_PMU_REG_DB_VBST_MASK, db_vbst + 1);

			db_vbst = mt6370_pmu_reg_read(chip,
							MT6370_PMU_REG_DBVBST);

			pr_info("%s: set DB_VBST from 0x%x to 0x%x\n",
				__func__, g_db_vbst, db_vbst);

			aee_kernel_warning("mt6370 dsv auto vbst ",
					"set DB_VBST= 0x%x\n", db_vbst);

		} else
			pr_info_ratelimited("%s: fixed DB_VBST = 0x%x\n",
				__func__, g_db_vbst);
	}
}


static ssize_t mt6370_pmu_dsv_debug_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	char lbuf[128];
	char *b = &lbuf[0];
	int flag = 0;
	ssize_t res;
	char *token;
	unsigned int val;

	if (*ppos != 0 || size >= sizeof(lbuf) || size == 0)
		return -EINVAL;

	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, buf, size);
	if (res <= 0)
		return -EFAULT;

	lbuf[size] = '\0';

	if (!strncmp(b, "vbst_adjustment ", strlen("vbst_adjustment "))) {
		b += strlen("vbst_adjustment ");
		flag = DSV_VAR_VBST_ADJUSTMENT;
	} else if (!strncmp(b, "irq_count_max ", strlen("irq_count_max "))) {
		b += strlen("irq_count_max ");
		flag = DSV_VAR_IRQ_COUNT;
	} else if (!strncmp(b, "irq_mask ", strlen("irq_mask "))) {
		b += strlen("irq_mask ");
		flag = DSV_VAR_IRQ_MASK;
	} else if (!strncmp(b, "irq_mask_warning ",
				strlen("irq_mask_warning "))) {
		b += strlen("irq_mask_warning ");
		flag = DSV_VAR_IRQ_MASK_WARNING;
	} else if (!strncmp(b, "irq_disable ",
				strlen("irq_disable "))) {
		b += strlen("irq_disable ");
		flag = DSV_VAR_IRQ_DISABLE;
	} else
		return -EINVAL;

	token = strsep(&b, " ");
	if (token == NULL)
		return -EINVAL;

	if (kstrtouint(token, 0, &val))
		return -EINVAL;

	switch (flag) {
	case DSV_VAR_VBST_ADJUSTMENT:
		g_vbst_adjustment = val;
		pr_info("[%s] set vbst_adjustment = 0x%x\n",
					__func__, g_vbst_adjustment);
		break;
	case DSV_VAR_IRQ_COUNT:
		g_irq_count_max = val;
		pr_info("[%s] set irq_count_max = 0x%x\n",
					__func__, g_irq_count_max);
		break;
	case DSV_VAR_IRQ_MASK:
		g_irq_mask = val;
		pr_info("[%s] set irq_mask = 0x%x\n",
					__func__, g_irq_mask);
		break;
	case DSV_VAR_IRQ_MASK_WARNING:
		g_irq_mask_warning = val;
		pr_info("[%s] set irq_mask_warning = 0x%x\n",
					__func__, g_irq_mask_warning);
		break;
	case DSV_VAR_IRQ_DISABLE:
		g_irq_disable = val;
		pr_info("[%s] set irq_disable = 0x%x\n",
					__func__, g_irq_disable);
		break;
	default:
		pr_info("[%s] do nothing\n", __func__);
		break;
	}

	return size;
}

static int mt6370_pmu_dsv_debug_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "vbst_adjustment = %d\n", g_vbst_adjustment);
	seq_printf(s, "irq_count_max = %d\n", g_irq_count_max);
	seq_printf(s, "irq_mask = 0x%x\n", g_irq_mask);
	seq_printf(s, "irq_mask_warning = 0x%x\n", g_irq_mask_warning);
	seq_printf(s, "irq_disable = 0x%x\n", g_irq_disable);

	return 0;
}

static int mt6370_pmu_dsv_debug_open(struct inode *inode,
						struct file *file)
{
	return single_open(file, mt6370_pmu_dsv_debug_show, NULL);
}

static const struct file_operations mt6370_pmu_dsv_debug_ops = {
	.open    = mt6370_pmu_dsv_debug_open,
	.read    = seq_read,
	.write   = mt6370_pmu_dsv_debug_write,
	.llseek  = seq_lseek,
	.release = single_release,
};


int mt6370_pmu_dsv_debug_init(struct mt6370_pmu_chip *chip)
{
	struct dentry *mt6370_pmu_dir;

	g_db_vbst = mt6370_pmu_reg_read(chip, MT6370_PMU_REG_DBVBST);
	g_vbst_adjustment = 0;
	g_irq_count_max = IRQ_COUNT_MAX;
	g_irq_mask = 1;
	g_irq_mask_warning = 0;
	g_irq_disable |= (1 << DSV_VPOS_OCP);

	mt6370_pmu_dir = debugfs_create_dir("mt6370_pmu", NULL);
	if (!mt6370_pmu_dir) {
		pr_info("create /sys/kernel/debug/mt6370_pmu failed\n");
		return -ENOMEM;
	}

	debugfs_create_file("mt6370_pmu_dsv", 0644,
				mt6370_pmu_dir, NULL,
				&mt6370_pmu_dsv_debug_ops);

	return 0;
}

MODULE_AUTHOR("Wilma Wu");
MODULE_DESCRIPTION("MT6370 Display Bias Debugfs Driver");
MODULE_LICENSE("GPL");

