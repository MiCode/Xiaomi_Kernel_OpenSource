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

#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <helio-dvfsrc.h>
#include <helio-dvfsrc-opp.h>
#include <mtk_spm_vcore_dvfs.h>
#include <mt-plat/mtk_devinfo.h>

#if defined(CONFIG_MTK_DRAMC)
#include <mtk_dramc.h>
#endif

__weak int __spm_get_dram_type(void) { return 0; }

#define VCORE_OPP_EFUSE_NUM     (2)

__weak int spm_vcorefs_pwarp_cmd(void) { return 0; }

/* SOC v1 Voltage (10uv)*/
static unsigned int
vcore_opp_L4_2CH[VCORE_DVFS_OPP_NUM][VCORE_OPP_EFUSE_NUM] = {
	{ 800000, 800000 },
	{ 725000, 700000 },
	{ 725000, 700000 },
	{ 725000, 700000 },
};

static unsigned int
vcore_opp_L4_2CH_CASE2[VCORE_DVFS_OPP_NUM][VCORE_OPP_EFUSE_NUM] = {
	{ 800000, 800000 },
	{ 800000, 800000 },
	{ 725000, 700000 },
	{ 725000, 700000 },
};

static unsigned int
vcore_opp_L3_1CH[VCORE_DVFS_OPP_NUM][VCORE_OPP_EFUSE_NUM] = {
	{ 800000, 800000 },
	{ 800000, 800000 },
	{ 725000, 700000 },
	{ 725000, 700000 },
};

int vcore_dvfs_to_vcore_opp[] = {
	VCORE_OPP_0, VCORE_OPP_1, VCORE_OPP_1, VCORE_OPP_1, VCORE_OPP_NUM
};

int vcore_dvfs_to_ddr_opp[]   = {
	DDR_OPP_0, DDR_OPP_0, DDR_OPP_1, DDR_OPP_2, DDR_OPP_NUM
};

/* ptr that points to v1 or v2 opp table */
unsigned int (*vcore_opp)[VCORE_OPP_EFUSE_NUM];

/* final vcore opp table */
unsigned int vcore_opp_table[VCORE_DVFS_OPP_NUM];

/* record index for vcore opp table from efuse */
unsigned int vcore_opp_efuse_idx[VCORE_DVFS_OPP_NUM] = { 0 };

unsigned int get_cur_vcore_opp(void)
{
	return vcore_dvfs_to_vcore_opp[spm_vcorefs_get_dvfs_opp()];
}

unsigned int get_cur_ddr_opp(void)
{
	return vcore_dvfs_to_ddr_opp[spm_vcorefs_get_dvfs_opp()];
}

unsigned int get_min_opp_for_vcore(int vcore_opp)
{
	int i = VCORE_DVFS_OPP_NUM;

	for (i = VCORE_DVFS_OPP_NUM; i >= 0 ; i--) {
		if (vcore_dvfs_to_vcore_opp[i] == vcore_opp)
			break;
	}
	return i;
}

unsigned int get_min_opp_for_ddr(int ddr_opp)
{
	int i = VCORE_DVFS_OPP_NUM;

	for (i = VCORE_DVFS_OPP_NUM; i >= 0; i--) {
		if (vcore_dvfs_to_ddr_opp[i] == ddr_opp)
			break;
	}
	return i;
}

unsigned int get_vcore_opp_volt(unsigned int opp)
{
	if (opp >= VCORE_DVFS_OPP_NUM) {
		pr_info("WRONG OPP: %u\n", opp);
		return 0;
	}

	return vcore_opp_table[opp];
}

unsigned int update_vcore_opp_uv(unsigned int opp, unsigned int vcore_uv)
{
	unsigned int ret = 0;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int i;
#endif

	if ((opp < VCORE_DVFS_OPP_NUM) && (opp >= 0))
		vcore_opp_table[opp] = vcore_uv;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++)
		dvfsrc_update_sspm_vcore_opp_table(i, get_vcore_opp_volt(i));
#endif

	ret = spm_vcorefs_pwarp_cmd();

	return ret;
}

static int get_soc_efuse(void)
{
	pr_info("[VcoreFS]efuse=0x%x soc_efuse=0x%x\n",
	get_devinfo_with_index(65), ((get_devinfo_with_index(65) >> 12) & 0x3));
	return ((get_devinfo_with_index(65) >> 12) & 0x3);
}

static void build_vcore_opp_table(unsigned int ddr_type, unsigned int soc_efuse)
{
	int i, mask = 0x3;

	if (soc_efuse > 1) {
		pr_info("WRONG VCORE EFUSE(%d)\n", soc_efuse);
		/* set to default table */
		for (i = 0; i < VCORE_DVFS_OPP_NUM; i++)
			vcore_opp_table[i] = *(vcore_opp[i]);
		return;
	}

	if (ddr_type == SPMFW_LP4X_2CH_3200) {
		vcore_opp = &vcore_opp_L4_2CH[0];
		vcore_opp_efuse_idx[0] = 0; /* 0.8V, no corner tightening*/
		vcore_opp_efuse_idx[1] = soc_efuse & mask; /* 0.7V */
		vcore_opp_efuse_idx[2] = soc_efuse & mask; /* 0.7V */
		vcore_opp_efuse_idx[3] = soc_efuse & mask; /* 0.7V */
		vcore_dvfs_to_vcore_opp[0] = VCORE_OPP_0;
		vcore_dvfs_to_vcore_opp[1] = VCORE_OPP_1;
		vcore_dvfs_to_vcore_opp[2] = VCORE_OPP_1;
		vcore_dvfs_to_vcore_opp[3] = VCORE_OPP_1;
		vcore_dvfs_to_ddr_opp[0] = DDR_OPP_0;
		vcore_dvfs_to_ddr_opp[1] = DDR_OPP_0;
		vcore_dvfs_to_ddr_opp[2] = DDR_OPP_1;
		vcore_dvfs_to_ddr_opp[3] = DDR_OPP_2;
	} else if (ddr_type == SPMFW_LP4X_2CH_3733 ||
		   ddr_type == SPMFW_LP4_2CH_2400) {
		vcore_opp = &vcore_opp_L4_2CH_CASE2[0];
		vcore_opp_efuse_idx[0] = 0; /* 0.8V, no corner tightening*/
		vcore_opp_efuse_idx[1] = 0; /* 0.8V, no corner tightening*/
		vcore_opp_efuse_idx[2] = soc_efuse & mask; /* 0.7V */
		vcore_opp_efuse_idx[3] = soc_efuse & mask; /* 0.7V */
		vcore_dvfs_to_vcore_opp[0] = VCORE_OPP_0;
		vcore_dvfs_to_vcore_opp[1] = VCORE_OPP_0;
		vcore_dvfs_to_vcore_opp[2] = VCORE_OPP_1;
		vcore_dvfs_to_vcore_opp[3] = VCORE_OPP_1;
		vcore_dvfs_to_ddr_opp[0] = DDR_OPP_0;
		vcore_dvfs_to_ddr_opp[1] = DDR_OPP_1;
		vcore_dvfs_to_ddr_opp[2] = DDR_OPP_1;
		vcore_dvfs_to_ddr_opp[3] = DDR_OPP_2;
	} else if (ddr_type == SPMFW_LP3_1CH_1866) {
		vcore_opp = &vcore_opp_L3_1CH[0];
		vcore_opp_efuse_idx[0] = 0; /* 0.8V, no corner tightening*/
		vcore_opp_efuse_idx[1] = 0; /* 0.8V, no corner tightening*/
		vcore_opp_efuse_idx[2] = soc_efuse & mask; /* 0.7V */
		vcore_opp_efuse_idx[3] = soc_efuse & mask; /* 0.7V */
		vcore_dvfs_to_vcore_opp[0] = VCORE_OPP_0;
		vcore_dvfs_to_vcore_opp[1] = VCORE_OPP_0;
		vcore_dvfs_to_vcore_opp[2] = VCORE_OPP_1;
		vcore_dvfs_to_vcore_opp[3] = VCORE_OPP_1;
		vcore_dvfs_to_ddr_opp[0] = DDR_OPP_0;
		vcore_dvfs_to_ddr_opp[1] = DDR_OPP_1;
		vcore_dvfs_to_ddr_opp[2] = DDR_OPP_1;
		vcore_dvfs_to_ddr_opp[3] = DDR_OPP_2;
	} else {
		pr_info("WRONG SPM DRAM TYPE: %d\n", ddr_type);
		return;
	}

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++)
		vcore_opp_table[i] = *(vcore_opp[i] + vcore_opp_efuse_idx[i]);

	for (i = VCORE_DVFS_OPP_NUM - 2; i >= 0; i--)
		vcore_opp_table[i] =
			max(vcore_opp_table[i], vcore_opp_table[i + 1]);

	pr_info("[VcoreFS]table(d=%d, ef=%d): %d, %d, %d, %d\n",
		ddr_type, soc_efuse, vcore_opp_table[0], vcore_opp_table[1],
		vcore_opp_table[2], vcore_opp_table[3]);
	pr_info("[VcoreFS]vcore opp tbl: %d, %d, %d, %d\n",
		vcore_dvfs_to_vcore_opp[0], vcore_dvfs_to_vcore_opp[1],
		vcore_dvfs_to_vcore_opp[2], vcore_dvfs_to_vcore_opp[3]);
	pr_info("[VcoreFS]ddr opp tbl: %d, %d, %d, %d\n",
		vcore_dvfs_to_ddr_opp[0], vcore_dvfs_to_ddr_opp[1],
		vcore_dvfs_to_ddr_opp[2], vcore_dvfs_to_ddr_opp[3]);
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

static int vcore_opp_proc_show(struct seq_file *m, void *v)
{
	unsigned int i = 0;

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++)
		seq_printf(m, "%d ", get_vcore_opp_volt(i));
	seq_puts(m, "\n");

	return 0;
}

static ssize_t vcore_opp_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	s32 opp, vcore_uv;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &opp, &vcore_uv) != 2)
		goto err;

	if (opp < 0 || opp >= VCORE_DVFS_OPP_NUM || vcore_uv < 0)
		goto err;

	update_vcore_opp_uv(opp, vcore_uv);

	free_page((unsigned long)buf);

	return count;

err:
	free_page((unsigned long)buf);

	return -EINVAL;
}

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		= THIS_MODULE,				\
		.open		= name ## _proc_open,			\
		.read		= seq_read,				\
		.llseek		= seq_lseek,				\
		.release	= single_release,			\
		.write		= name ## _proc_write,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(vcore_opp);

static int vcore_opp_procfs_init(void)
{
	struct proc_dir_entry *dir = NULL;
	int ret = 0;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry det_entries_vcore[] = {
		PROC_ENTRY(vcore_opp),
	};

	dir = proc_mkdir("vcore_opp", NULL);
	if (!dir) {
		pr_info("%s: Failed to create /proc/vcore_opp dir\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(det_entries_vcore); i++) {
		if (!proc_create(det_entries_vcore[i].name,
					0644, dir,
					det_entries_vcore[i].fops)) {
			pr_info("%s: Failed to create /proc/vcore_opp/%s\n",
					__func__, det_entries_vcore[i].name);

			return -ENOMEM;
		}
	}

	return ret;
}

int vcore_opp_init(void)
{
	int ret = 0;

	ret = vcore_opp_procfs_init();
	if (ret)
		return ret;

	build_vcore_opp_table(__spm_get_dram_type(), get_soc_efuse());

	return 0;
}

