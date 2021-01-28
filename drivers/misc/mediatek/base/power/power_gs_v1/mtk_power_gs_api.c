// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <mt-plat/aee.h>

#if defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif

#include "mtk_power_gs.h"

#define HEX_FMT "0x%08x"
#define SIZEOF_SNAPSHOT(g) \
	(sizeof(struct snapshot) + sizeof(unsigned int) * (g->nr_gs - 1))
#define DEBUG_BUF_SIZE 2000

static char buf[DEBUG_BUF_SIZE] = { 0 };
static struct base_remap br;
static struct pmic_manual_dump pmd;

static bool _is_pmic_addr(unsigned int addr)
{
	return (addr >> 16) ? 0 : 1;
}

static u16 gs_pmic_read(u16 reg)
{
	u32 reg_val = 0;
#if defined(CONFIG_MTK_PMIC_NEW_ARCH)
	u32 ret = 0;

	ret = pmic_read_interface_nolock(reg, &reg_val, 0xFFFF, 0x0);
#endif
	return (u16)reg_val;
}


static void _golden_setting_disable(struct golden *g)
{
	if (g) {
		g->is_golden_log = 0;

		g->func[0] = '\0';

		g->buf_gs = (struct golden_setting *)g->buf;
		g->nr_gs = 0;
		g->max_nr_gs = g->buf_size / 3 / sizeof(struct golden_setting);
	}
}


static void _golden_setting_init(struct golden *g, unsigned int *buf,
		unsigned int buf_size)
{
	if (g && buf) {
		g->mode = MODE_NORMAL;

		g->buf = buf;
		g->buf_size = buf_size;

		_golden_setting_disable(g);
	}
}

static void __iomem *_golden_io_phys_to_virt(unsigned int addr)
{
	unsigned int base = addr & (~(unsigned long)REMAP_SIZE_MASK);
	unsigned int offset = addr & (unsigned long)REMAP_SIZE_MASK;

	if (!_g.phy_base || _g.phy_base != base) {
		if (_g.io_base)
			iounmap(_g.io_base);

		_g.phy_base = base;
		_g.io_base =
			ioremap_nocache(_g.phy_base, REMAP_SIZE_MASK+1);

		if (!_g.io_base)
			pr_err("warning: ioremap_nocache(0x%x, 0x%x)\n",
					base, REMAP_SIZE_MASK+1);
	}

	return (_g.io_base + offset);
}

static int _is_snapshot_full(struct golden *g)
{
	if (g->snapshot_head + 1 == g->snapshot_tail ||
			g->snapshot_head + 1 ==
			g->snapshot_tail + g->max_nr_snapshot)
		return 1;
	else
		return 0;
}


static struct snapshot *_snapshot_produce(struct golden *g)
{
	if (g && !_is_snapshot_full(g)) {
		int idx = g->snapshot_head++;

		if (g->snapshot_head == g->max_nr_snapshot)
			g->snapshot_head = 0;

		return (struct snapshot *)
			((size_t)(g->buf_snapshot) + SIZEOF_SNAPSHOT(g) * idx);
	} else
		return NULL;
}

int _snapshot_golden_setting(struct golden *g, const char *func,
		const unsigned int line)
{
	struct snapshot *snapshot;
	int i;

	snapshot = _snapshot_produce(g);

	if (g && 1 == g->is_golden_log && snapshot &&
	    (g->func[0] == '\0' || (!strcmp(g->func, func) &&
				    ((g->line == line) || (g->line == 0))))) {
		snapshot->func = func;
		snapshot->line = line;

		for (i = 0; i < g->nr_gs; i++) {
			if (_g.mode == MODE_APPLY) {
				_golden_write_reg(g->buf_gs[i].addr,
						  g->buf_gs[i].mask,
						  g->buf_gs[i].golden_val);
			}

			snapshot->reg_val[i] =
				_golden_read_reg(g->buf_gs[i].addr);
		}

		is_already_snap_shot = true;

		return 0;
	} else
		return -1;
}

#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode, struct file *file) \
	{								\
		return single_open_size(file, name ## _proc_show, NULL, \
				2 * PAGE_SIZE);	\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
		.write          = name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode, struct file *file) \
	{								\
		return single_open(file, name ## _proc_show, NULL);	\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}


void __attribute__((weak)) mt_power_gs_internal_init(void)
{
}

static int mt_golden_setting_init(void)
{
#define GOLDEN_SETTING_BUF_SIZE (2 * PAGE_SIZE)

	unsigned int *buf;

	buf = kmalloc(GOLDEN_SETTING_BUF_SIZE, GFP_KERNEL);

	if (buf) {
		_golden_setting_init(&_g, buf, GOLDEN_SETTING_BUF_SIZE);

#ifdef CONFIG_OF
		_g.phy_base = 0;
		_g.io_base = 0;
#endif
		{
			pmd.array_size = REMAP_SIZE_MASK;
			if (!pmd.addr_array)
				pmd.addr_array =
					kmalloc(sizeof(unsigned int) *
							REMAP_SIZE_MASK + 1,
							GFP_KERNEL);

			br.table_size = REMAP_SIZE_MASK;
			br.table_pos = 0;
			mt_power_gs_internal_init();
			mt_power_gs_table_init();
		}
	}

	return 0;
}
module_init(mt_golden_setting_init);


unsigned int _golden_read_reg(unsigned int addr)
{
	unsigned int reg_val;
	unsigned int base = addr & (~(unsigned long)REMAP_SIZE_MASK);
	unsigned int offset = addr & (unsigned long)REMAP_SIZE_MASK;
	void __iomem *io_base;

	if (_is_pmic_addr(addr))
		reg_val = gs_pmic_read(addr);
	else {
		io_base = _get_virt_base_from_table(base);
		if (io_base)
			reg_val = ioread32(io_base + offset);
		else
			reg_val = 0;
	}

	return reg_val;
}

void _golden_write_reg(unsigned int addr, unsigned int mask,
		unsigned int reg_val)
{
	void __iomem *io_addr;

	if (_is_pmic_addr(addr)) {
#if defined(CONFIG_MTK_PMIC_NEW_ARCH)
		pmic_config_interface(addr, reg_val, mask, 0x0);
#endif
	} else {
		io_addr = _golden_io_phys_to_virt(addr);
		writel((ioread32(io_addr) & ~mask) | (reg_val & mask), io_addr);
	}
}

/* Check phys addr is existed in table or not */
bool _is_exist_in_phys_to_virt_table(unsigned int pa)
{
	unsigned int k;

	if (br.table)
		for (k = 0; k < br.table_pos; k++)
			if (br.table[k].pa == pa)
				return true;

	return false;
}

void __iomem *_get_virt_base_from_table(unsigned int pa)
{
	unsigned int k;
	void __iomem *io_base = 0;

	if (br.table) {
		for (k = 0; k < br.table_pos; k++)
			if (br.table[k].pa == pa)
				return (io_base = br.table[k].va);
	} else
		pr_err("Power_gs: cannot find virtual address\n");

	return io_base;
}

unsigned int mt_power_gs_base_remap_init(char *scenario, char *pmic_name,
			 const unsigned int *pmic_gs, unsigned int pmic_gs_len)
{
	unsigned int i, base;
	struct phys_to_virt_table *table;

	if (!br.table)
		br.table = kmalloc(sizeof(struct phys_to_virt_table) *
				REMAP_SIZE_MASK + 1, GFP_KERNEL);

	if (br.table) {
		table = br.table;

		for (i = 0; i < pmic_gs_len; i += 3) {
			base = (pmic_gs[i] & (~(unsigned long)REMAP_SIZE_MASK));

			if (!_is_exist_in_phys_to_virt_table(base)) {
				table[br.table_pos].pa = base;
				table[br.table_pos].va =
					ioremap_nocache(base,
							REMAP_SIZE_MASK + 1);

				if (!table[br.table_pos].va)
					pr_err("ioremap_nocache(0x%x, 0x%x)\n",
						base, REMAP_SIZE_MASK + 1);

				if (br.table_pos < br.table_size)
					br.table_pos++;
				else {
					pr_err("base_remap in maximum size\n");
					return 0;
				}
			}
		}
	}

	return 0;
}

#define PER_LINE_TO_PRINT 8

void mt_power_gs_pmic_manual_dump(void)
{
	unsigned int i, dump_cnt = 0;
	char *p;

	if (pmd.addr_array && pmd.array_pos) {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
				"Scenario - PMIC - Addr       - Value\n");

		for (i = 0; i < pmd.array_pos; i++) {
			dump_cnt++;
			p += snprintf(p, sizeof(buf) - (p - buf),
				"Manual   - PMIC - 0x%08x - 0x%08x\n",
				pmd.addr_array[i],
				_golden_read_reg(pmd.addr_array[i]));

			if (dump_cnt && ((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
				pr_notice("%s", buf);
				p = buf;
				p += snprintf(p, sizeof(buf), "\n");
			}
		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_notice("%s", buf);
	}
}

void mt_power_gs_compare(char *scenario, char *pmic_name,
			 const unsigned int *pmic_gs, unsigned int pmic_gs_len)
{
	unsigned int i, k, val0, val1, val2, diff, dump_cnt = 0;
	char *p;

	/* dump diff mode */
	if (slp_chk_golden_diff_mode) {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
				"Scenario - %s - Addr       - Value      - Mask       - Golden     - Wrong Bit\n",
				pmic_name);

		for (i = 0; i < pmic_gs_len; i += 3) {
			val0 = _golden_read_reg(pmic_gs[i]);
			val1 = val0 & pmic_gs[i + 1];
			val2 = pmic_gs[i + 2] & pmic_gs[i + 1];

			if (val1 != val2) {
				dump_cnt++;
				p += snprintf(p, sizeof(buf) - (p - buf),
					"%s - %s - 0x%08x - 0x%08x - 0x%08x - 0x%08x -",
					scenario, pmic_name, pmic_gs[i], val0,
					pmic_gs[i + 1], pmic_gs[i + 2]);

				for (k = 0, diff = val1 ^ val2; diff != 0; k++,
						diff >>= 1) {
					if ((diff % 2) != 0)
						p += snprintf(p,
							sizeof(buf) - (p - buf),
							" %d", k);
				}

				p += snprintf(p, sizeof(buf) - (p - buf), "\n");

				if (dump_cnt &&
					((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
					pr_notice("%s", buf);
					p = buf;
					p += snprintf(p, sizeof(buf), "\n");
				}
			}

		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_notice("%s", buf);

	/* dump raw data mode */
	} else {
		p = buf;
		p += snprintf(p, sizeof(buf), "\n");
		p += snprintf(p, sizeof(buf) - (p - buf),
		"Scenario - PMIC - Addr       - Value\n");

		for (i = 0; i < pmic_gs_len; i += 3) {
			val0 = _golden_read_reg(pmic_gs[i]);

			dump_cnt++;
			p += snprintf(p, sizeof(buf) - (p - buf),
				"%s - %s - 0x%08x - 0x%08x\n",
				scenario, pmic_name, pmic_gs[i], val0);

			if (dump_cnt && ((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
				pr_notice("%s", buf);
				p = buf;
				p += snprintf(p, sizeof(buf), "\n");
			}
		}
		if (dump_cnt % PER_LINE_TO_PRINT)
			pr_notice("%s", buf);
	}
}
