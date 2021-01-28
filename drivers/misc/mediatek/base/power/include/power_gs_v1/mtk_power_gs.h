/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef MTK_POWER_GS_H
#define MTK_POWER_GS_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "mtk_power_gs_internal.h"

#define REMAP_SIZE_MASK     0xFFF

extern bool is_already_snap_shot;

enum pr_mode {
	MODE_NORMAL,
	MODE_COMPARE,
	MODE_APPLY,
	MODE_COLOR,
	MODE_DIFF,
};

struct golden_setting {
	unsigned int addr;
	unsigned int mask;
	unsigned int golden_val;
};

struct snapshot {
	const char *func;
	unsigned int line;
	unsigned int reg_val[1];
};

struct golden {
	unsigned int is_golden_log;

	enum pr_mode mode;

	char func[64];
	unsigned int line;

	unsigned int *buf;
	unsigned int buf_size;

	struct golden_setting *buf_gs;
	unsigned int nr_gs;
	unsigned int max_nr_gs;

	struct snapshot *buf_snapshot;
	unsigned int max_nr_snapshot;
	unsigned int snapshot_head;
	unsigned int snapshot_tail;
#ifdef CONFIG_OF
	unsigned int phy_base;
	void __iomem *io_base;
#endif
};

struct phys_to_virt_table {
	void __iomem *va;
	unsigned int pa;
};

struct base_remap {
	unsigned int table_pos;
	unsigned int table_size;
	struct phys_to_virt_table *table;
};

struct pmic_manual_dump {
	unsigned int array_pos;
	unsigned int array_size;
	unsigned int *addr_array;
};

unsigned int golden_read_reg(unsigned int addr);
int snapshot_golden_setting(const char *func, const unsigned int line);
void mt_power_gs_pmic_manual_dump(void);
void mt_power_gs_compare(char *scenario,
			char *pmic_name,
			 const unsigned int *pmic_gs,
			 unsigned int pmic_gs_len);
unsigned int _golden_read_reg(unsigned int addr);
void _golden_write_reg(unsigned int addr, unsigned int mask,
				unsigned int reg_val);
int _snapshot_golden_setting(struct golden *g, const char *func,
				const unsigned int line);
void mt_power_gs_suspend_compare(unsigned int dump_flag);
void mt_power_gs_dpidle_compare(unsigned int dump_flag);
void mt_power_gs_sodi_compare(unsigned int dump_flag);
void mt_power_gs_sp_dump(void);

bool _is_exist_in_phys_to_virt_table(unsigned int phys_base);
void __iomem *_get_virt_base_from_table(unsigned int phys_base);
unsigned int mt_power_gs_base_remap_init(char *scenario, char *pmic_name,
			const unsigned int *pmic_gs, unsigned int pmic_gs_len);
void mt_power_gs_internal_init(void);
void mt_power_gs_table_init(void);

extern struct golden _g;
extern bool slp_chk_golden_diff_mode;

#endif
