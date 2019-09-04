/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

static void _golden_setting_enable(struct golden *g)
{
	if (g) {
		g->buf_snapshot = (struct snapshot *) &(g->buf_gs[g->nr_gs]);
		g->max_nr_snapshot = (g->buf_size -
				sizeof(struct golden_setting) * g->nr_gs) /
				SIZEOF_SNAPSHOT(g);
		g->snapshot_head = 0;
		g->snapshot_tail = 0;

		g->is_golden_log = 1;
	}
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

static void _golden_setting_set_mode(struct golden *g, unsigned int mode)
{
	g->mode = mode;
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

static void _golden_setting_add(struct golden *g, unsigned int addr,
		unsigned int mask, unsigned int golden_val)
{
	if (g && 0 == g->is_golden_log &&
			g->nr_gs < g->max_nr_gs && mask != 0) {
		g->buf_gs[g->nr_gs].addr = addr;
		g->buf_gs[g->nr_gs].mask = mask;
		g->buf_gs[g->nr_gs].golden_val = golden_val;

		g->nr_gs++;
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

static int _is_snapshot_empty(struct golden *g)
{
	if (g->snapshot_head == g->snapshot_tail)
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

static struct snapshot *_snapshot_consume(struct golden *g)
{
	if (g && !_is_snapshot_empty(g)) {
		int idx = g->snapshot_tail++;

		if (g->snapshot_tail == g->max_nr_snapshot)
			g->snapshot_tail = 0;

		is_already_snap_shot = false;

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

static int _parse_mask_val(char *buf, unsigned int *mask,
		unsigned int *golden_val)
{
	unsigned int i, bit_shift;
	unsigned int mask_result;
	unsigned int golden_val_result;

	for (i = 0, bit_shift = 1 << 31, mask_result = 0, golden_val_result = 0;
			bit_shift > 0;) {
		switch (buf[i]) {
		case '1':
			golden_val_result += bit_shift;

		case '0':
			mask_result += bit_shift;

		case 'x':
		case 'X':
			bit_shift >>= 1;

		case '_':
			break;

		default:
			return -1;
		}

		i++;
	}

	*mask = mask_result;
	*golden_val = golden_val_result;

	return 0;
}

static char *_gen_mask_str(const unsigned int mask, const unsigned int reg_val)
{
	static char _mask_str[42];
	unsigned int i, bit_shift;

	strncpy(_mask_str, "0bxxxx_xxxx_xxxx_xxxx_xxxx_xxxx_xxxx_xxxx", 42);
	for (i = 2, bit_shift = 1 << 31; bit_shift > 0;) {
		switch (_mask_str[i]) {
		case '_':
			break;

		default:
			if (0 == (mask & bit_shift))
				_mask_str[i] = 'x';
			else if (0 == (reg_val & bit_shift))
				_mask_str[i] = '0';
			else
				_mask_str[i] = '1';

		case '\0':
			bit_shift >>= 1;
			break;
		}

		i++;
	}

	return _mask_str;
}

static char *_gen_diff_str(const unsigned int mask,
		const unsigned int golden_val, const unsigned int reg_val)
{
	static char _diff_str[42];
	unsigned int i, bit_shift;

	strncpy(_diff_str, "0b    _    _    _    _    _    _    _    ", 42);
	for (i = 2, bit_shift = 1 << 31; bit_shift > 0;) {
		switch (_diff_str[i]) {
		case '_':
			break;

		default:
			if (0 != ((golden_val ^ reg_val) & mask & bit_shift))
				_diff_str[i] = '^';
			else
				_diff_str[i] = ' ';

		case '\0':
			bit_shift >>= 1;
			break;
		}

		i++;
	}

	return _diff_str;
}

static char *_gen_color_str(const unsigned int mask,
		const unsigned int golden_val, const unsigned int reg_val)
{
#define FC "\e[41m"
#define EC "\e[m"
#define XXXX FC "x" EC FC "x" EC FC "x" EC FC "x" EC
	static char _clr_str[300];
	unsigned int i, bit_shift;

	strncpy(_clr_str, "0b"XXXX"_"XXXX"_"XXXX"_"XXXX"_"
			XXXX"_"XXXX"_"XXXX"_"XXXX, 300);
	for (i = 2, bit_shift = 1 << 31; bit_shift > 0;) {
		switch (_clr_str[i]) {
		case '_':
			break;

		default:
			if (0 != ((golden_val ^ reg_val) & mask & bit_shift))
				_clr_str[i + 3] = '1';
			else
				_clr_str[i + 3] = '0';

			if (0 == (mask & bit_shift))
				_clr_str[i + 5] = 'x';
			else if (0 == (reg_val & bit_shift))
				_clr_str[i + 5] = '0';
			else
				_clr_str[i + 5] = '1';

			i += strlen(EC) + strlen(FC);

		case '\0':
			bit_shift >>= 1;
			break;
		}

		i++;
	}

	return _clr_str;

#undef FC
#undef EC
#undef XXXX
}

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

 out:
	free_page((unsigned long)buf);

	return NULL;
}

static int golden_test_proc_show(struct seq_file *m, void *v)
{
	static int idx;
	int i = 0;

	idx = 0;

	if (_g.is_golden_log == 0) {
		for (i = 0; i < _g.nr_gs; i++) {
			seq_printf(m, ""HEX_FMT" "HEX_FMT" "HEX_FMT"\n",
				   _g.buf_gs[i].addr,
				   _g.buf_gs[i].mask,
				   _g.buf_gs[i].golden_val
				   );
		}
	}

	if (_g.nr_gs == 0) {
		seq_puts(m, "\n********** golden_test help *********\n");
		seq_puts(m, "1.   disable snapshot:                  echo disable > /proc/clkmgr/golden_test\n");
		seq_puts(m, "2.   insert golden setting (tool mode): echo 0x10000000 (addr) 0bxxxx_xxxx_xxxx_xxxx_0001_0100_1001_0100 (mask & golden value) > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(2.) insert golden setting (hex mode):  echo 0x10000000 (addr) 0xFFFF (mask) 0x1494 (golden value) > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(2.) insert golden setting (dec mode):  echo 268435456 (addr) 65535 (mask) 5268 (golden value) > /proc/clkmgr/golden_test\n");
		seq_puts(m, "3.   set filter:                        echo filter func_name [line_num] > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(3.) disable filter:                    echo filter > /proc/clkmgr/golden_test\n");
		seq_puts(m, "4.   enable snapshot:                   echo enable > /proc/clkmgr/golden_test\n");
		seq_puts(m, "5.   set compare mode:                  echo compare > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(5.) set apply mode:                    echo apply > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(5.) set color mode:                    echo color > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(5.) set diff mode:                     echo color > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(5.) disable compare/apply/color mode:  echo normal > /proc/clkmgr/golden_test\n");
		seq_puts(m, "6.   set register value (normal mode):  echo set 0x10000000 (addr) 0x13201494 (reg val) > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(6.) set register value (mask mode):    echo set 0x10000000 (addr) 0xffff (mask) 0x13201494 (reg val) > /proc/clkmgr/golden_test\n");
		seq_puts(m, "(6.) set register value (bit mode):     echo set 0x10000000 (addr) 0 (bit num) 1 (reg val) > /proc/clkmgr/golden_test\n");
		seq_puts(m, "7.   dump suspend comapare:             echo dump_suspend > /proc/clkmgr/golden_test\n");
		seq_puts(m, "8.   dump dpidle comapare:              echo dump_dpidle > /proc/clkmgr/golden_test\n");
		seq_puts(m, "9.   dump sodi comapare:                echo dump_sodi > /proc/clkmgr/golden_test\n");
	} else {
		static struct snapshot *ss;

		if (!strcmp(_g.func, __func__) && (_g.line == 0))
			snapshot_golden_setting(__func__, 0);

		while ((idx != 0) || ((ss = _snapshot_consume(&_g)) != NULL)) {
			if (idx == 0)
				seq_printf(m, "// @ %s():%d\n",
						ss->func, ss->line);

			for (i = idx, idx = 0; i < _g.nr_gs; i++) {
				if (!(_g.mode == MODE_NORMAL
				    || ((_g.buf_gs[i].mask &
						    _g.buf_gs[i].golden_val)
					!= (_g.buf_gs[i].mask & ss->reg_val[i])
					)
				    ))
					continue;
				if (_g.mode == MODE_COLOR) {
					seq_printf(m,
						HEX_FMT"\t"HEX_FMT"\t"
						HEX_FMT"\t%s\n",
						_g.buf_gs[i].addr,
						_g.buf_gs[i].mask,
						ss->reg_val[i],
						_gen_color_str(
							_g.buf_gs[i].mask,
							_g.buf_gs[i].golden_val,
							ss->reg_val[i]));
				} else if (_g.mode == MODE_DIFF) {
					seq_printf(m,
						HEX_FMT"\t"HEX_FMT"\t"
						HEX_FMT"\t%s\n",
						_g.buf_gs[i].addr,
						_g.buf_gs[i].mask,
						ss->reg_val[i],
						_gen_mask_str(
							_g.buf_gs[i].mask,
							ss->reg_val[i]));

					seq_printf(m,
						HEX_FMT"\t"HEX_FMT"\t"
						HEX_FMT"\t%s\n",
						_g.buf_gs[i].addr,
						_g.buf_gs[i].mask,
						_g.buf_gs[i].golden_val,
						_gen_diff_str(
							_g.buf_gs[i].mask,
							_g.buf_gs[i].golden_val,
							ss->reg_val[i]));
				} else
					seq_printf(m,
						HEX_FMT"\t"HEX_FMT"\t"
						HEX_FMT"\n",
						_g.buf_gs[i].addr,
						_g.buf_gs[i].mask,
						ss->reg_val[i]);
			}
		}
	}

	mt_power_gs_sp_dump();

	return 0;
}

static ssize_t golden_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	char cmd[64];
	struct phys_to_virt_table *table;
	unsigned int base;
	unsigned int addr;
	unsigned int mask;
	unsigned int golden_val;

	if (!buf)
		return -EINVAL;

	/* set golden setting (hex mode) */
	if (sscanf(buf, "0x%x 0x%x 0x%x", &addr, &mask, &golden_val) == 3)
		_golden_setting_add(&_g, addr, mask, golden_val);
	/* set golden setting (dec mode) */
	else if (sscanf(buf, "%d %d %d", &addr, &mask, &golden_val) == 3)
		_golden_setting_add(&_g, addr, mask, golden_val);
	/* set filter (func + line) */
	/* XXX: 63 = sizeof(_g.func) - 1 */
	else if (sscanf(buf, "filter %63s %d", _g.func, &_g.line) == 2)
		;
	/* set filter (func) */
	/* XXX: 63 = sizeof(_g.func) - 1 */
	else if (sscanf(buf, "filter %63s", _g.func) == 1)
		_g.line = 0;
	/* set golden setting (mixed mode) */
	/* XXX: 63 = sizeof(cmd) - 1 */
	else if (sscanf(buf, "0x%x 0b%63s", &addr, cmd) == 2) {
		if (!_parse_mask_val(cmd, &mask, &golden_val))
			_golden_setting_add(&_g, addr, mask, golden_val);
	}
	/* set reg value (mask mode) */
	else if (sscanf(buf,
			"set 0x%x 0x%x 0x%x", &addr, &mask, &golden_val) == 3)
		_golden_write_reg(addr, mask, golden_val);
	/* set reg value (bit mode) */
	/* XXX: mask is bit number (alias) */
	else if (sscanf(buf,
			"set 0x%x %d %d", &addr, &mask, &golden_val) == 3) {
		if (mask >= 0 && mask <= 31) {
			golden_val = (golden_val & BIT(0)) << mask;
			mask = BIT(0) << mask;
			_golden_write_reg(addr, mask, golden_val);
		}
	}
	/* set reg value (normal mode) */
	else if (sscanf(buf, "set 0x%x 0x%x", &addr, &golden_val) == 2)
		_golden_write_reg(addr, 0xFFFFFFFF, golden_val);
	/* set to dump pmic reg value */
	else if (sscanf(buf, "set_pmic_manual_dump 0x%x", &addr) == 1) {
		if (pmd.addr_array && pmd.array_pos < pmd.array_size) {
			pmd.addr_array[pmd.array_pos++] = addr;
			base  = (addr &
				(~(unsigned long)REMAP_SIZE_MASK));

			if (!_is_pmic_addr(addr) &&
			    !_is_exist_in_phys_to_virt_table(base) &&
			    br.table) {

				table = br.table;
				table[br.table_pos].pa
					= base;
				table[br.table_pos].va
					= ioremap_nocache(base,
						REMAP_SIZE_MASK + 1);

				if (!table[br.table_pos].va)
					pr_info("Power_gs: va(0x%x, 0x%x)\n"
						, base, REMAP_SIZE_MASK + 1);

				if (br.table_pos < br.table_size)
					br.table_pos++;
				else
					pr_info("Power_gs: base_remap full\n");
			}
		} else {
			if (!pmd.addr_array)
				pr_info("Power_gs: pmd init fail\n");
			else
				pr_info("Power_gs: pmd array is full\n");
		}
	}
	/* XXX: 63 = sizeof(cmd) - 1 */
	else if (sscanf(buf, "%63s", cmd) == 1) {
		if (!strcmp(cmd, "enable"))
			_golden_setting_enable(&_g);
		else if (!strcmp(cmd, "disable"))
			_golden_setting_disable(&_g);
		else if (!strcmp(cmd, "normal"))
			_golden_setting_set_mode(&_g, MODE_NORMAL);
		else if (!strcmp(cmd, "compare"))
			_golden_setting_set_mode(&_g, MODE_COMPARE);
		else if (!strcmp(cmd, "apply"))
			_golden_setting_set_mode(&_g, MODE_APPLY);
		else if (!strcmp(cmd, "color"))
			_golden_setting_set_mode(&_g, MODE_COLOR);
		else if (!strcmp(cmd, "diff"))
			_golden_setting_set_mode(&_g, MODE_DIFF);
		else if (!strcmp(cmd, "filter"))
			_g.func[0] = '\0';
		else if (!strcmp(cmd, "dump_suspend"))
			mt_power_gs_suspend_compare(GS_ALL);
		else if (!strcmp(cmd, "dump_dpidle"))
			mt_power_gs_dpidle_compare(GS_ALL);
		else if (!strcmp(cmd, "dump_sodi"))
			mt_power_gs_sodi_compare(GS_ALL);
		else if (!strcmp(cmd, "free_pmic_manual_dump")) {
			if (pmd.addr_array)
				pmd.array_pos = 0;
		}
	}

	free_page((size_t)buf);

	return count;
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

PROC_FOPS_RW(golden_test);

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
			struct proc_dir_entry *dir = NULL;
			int i;

			const struct {
				const char *name;
				const struct file_operations *fops;
			} entries[] = {
				PROC_ENTRY(golden_test),
			};

			dir = proc_mkdir("golden", NULL);

			if (!dir) {
				pr_err("[%s]: fail to mkdir /proc/golden\n",
						__func__);
				return -ENOMEM;
			}

			for (i = 0; i < ARRAY_SIZE(entries); i++) {
				if (!proc_create(entries[i].name, 0664,
						dir, entries[i].fops))
					pr_err("[%s]: fail to mkdir /proc/golden/%s\n",
							__func__,
							entries[i].name);
			}

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
#if 0
		io_base = ioremap_nocache(base, REMAP_SIZE_MASK+1);
		reg_val = ioread32(io_base + offset);
		iounmap(io_base);
#else
		io_base = _get_virt_base_from_table(base);
		if (io_base)
			reg_val = ioread32(io_base + offset);
		else
			reg_val = 0;
#endif
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
