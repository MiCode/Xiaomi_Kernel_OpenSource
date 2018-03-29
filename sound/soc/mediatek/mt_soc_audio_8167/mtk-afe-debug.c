/*
 * Mediatek audio debug function
 *
 * Copyright (c) 2016 MediaTek Inc.
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


#include "mtk-afe-debug.h"
#include "mtk-afe-regs.h"
#include "mtk-afe-util.h"
#include "mtk-afe-common.h"
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>


#ifdef CONFIG_DEBUG_FS

struct mtk_afe_debug_fs {
	char *fs_name;
	const struct file_operations *fops;
};

struct afe_dump_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct afe_dump_reg_attr afe_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON1),
	DUMP_REG_ENTRY(AUDIO_TOP_CON3),
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(AFE_DAC_CON1),
	DUMP_REG_ENTRY(AFE_I2S_CON),
	DUMP_REG_ENTRY(AFE_I2S_CON1),
	DUMP_REG_ENTRY(AFE_I2S_CON2),
	DUMP_REG_ENTRY(AFE_I2S_CON3),
	DUMP_REG_ENTRY(AFE_CONN0),
	DUMP_REG_ENTRY(AFE_CONN1),
	DUMP_REG_ENTRY(AFE_CONN2),
	DUMP_REG_ENTRY(AFE_CONN3),
	DUMP_REG_ENTRY(AFE_CONN4),
	DUMP_REG_ENTRY(AFE_CONN5),
	DUMP_REG_ENTRY(AFE_CONN_24BIT),
	DUMP_REG_ENTRY(AFE_DL1_BASE),
	DUMP_REG_ENTRY(AFE_DL1_CUR),
	DUMP_REG_ENTRY(AFE_DL1_END),
	DUMP_REG_ENTRY(AFE_DL2_BASE),
	DUMP_REG_ENTRY(AFE_DL2_CUR),
	DUMP_REG_ENTRY(AFE_DL2_END),
	DUMP_REG_ENTRY(AFE_AWB_BASE),
	DUMP_REG_ENTRY(AFE_AWB_CUR),
	DUMP_REG_ENTRY(AFE_AWB_END),
	DUMP_REG_ENTRY(AFE_VUL_BASE),
	DUMP_REG_ENTRY(AFE_VUL_CUR),
	DUMP_REG_ENTRY(AFE_VUL_END),
	DUMP_REG_ENTRY(AFE_DAI_BASE),
	DUMP_REG_ENTRY(AFE_DAI_CUR),
	DUMP_REG_ENTRY(AFE_DAI_END),
	DUMP_REG_ENTRY(AFE_MEMIF_MSB),
	DUMP_REG_ENTRY(AFE_MEMIF_MON0),
	DUMP_REG_ENTRY(AFE_MEMIF_MON1),
	DUMP_REG_ENTRY(AFE_MEMIF_MON2),
	DUMP_REG_ENTRY(AFE_MEMIF_MON3),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_TOP_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_DL_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_NEWIF_CFG0),
	DUMP_REG_ENTRY(AFE_ADDA_NEWIF_CFG1),
	DUMP_REG_ENTRY(AFE_ADDA_PREDIS_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_PREDIS_CON1),
	DUMP_REG_ENTRY(AFE_MRGIF_CON),
	DUMP_REG_ENTRY(AFE_DAIBT_CON0),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CON),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_EN),
	DUMP_REG_ENTRY(AFE_IRQ_CNT1),
	DUMP_REG_ENTRY(AFE_IRQ_CNT2),
	DUMP_REG_ENTRY(AFE_MEMIF_PBUF_SIZE),
	DUMP_REG_ENTRY(AFE_SGEN_CON0),
};

static const struct afe_dump_reg_attr hdmi_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_CON0),
	DUMP_REG_ENTRY(AFE_HDMI_CONN0),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_BASE),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_CUR),
	DUMP_REG_ENTRY(AFE_HDMI_OUT_END),
	DUMP_REG_ENTRY(AFE_TDM_CON1),
	DUMP_REG_ENTRY(AFE_TDM_CON2),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CON2),
	DUMP_REG_ENTRY(AFE_IRQ_CNT5),
};

static ssize_t mtk_afe_read_file(struct file *file, char __user *user_buf,
	size_t count, loff_t *pos)
{
	struct mtk_afe *afe = file->private_data;
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value;
	int n = 0;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	pm_runtime_get_sync(afe->dev);

	mtk_afe_enable_main_clk(afe);

	for (i = 0; i < ARRAY_SIZE(afe_dump_regs); i++) {
		if (regmap_read(afe->regmap, afe_dump_regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s = N/A\n",
				       afe_dump_regs[i].name);
		else
			n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
				       afe_dump_regs[i].name, reg_value);
	}

	n += scnprintf(buf + n, count - n, "adda_afe_on_ref_cnt = %d\n",
		       afe->adda_afe_on_ref_cnt);

	mtk_afe_disable_main_clk(afe);

	pm_runtime_put(afe->dev);

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}

static ssize_t mtk_afe_write_file(struct file *file, const char __user *user_buf,
	size_t count, loff_t *pos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	const char delim[] = " ,";
	unsigned long reg, value;
	struct mtk_afe *afe = file->private_data;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	pm_runtime_get_sync(afe->dev);

	mtk_afe_enable_main_clk(afe);

	regmap_write(afe->regmap, reg, value);

	mtk_afe_disable_main_clk(afe);

	pm_runtime_put(afe->dev);

	return buf_size;
}

static ssize_t mtk_afe_hdmi_read_file(struct file *file, char __user *user_buf,
				size_t count, loff_t *pos)
{
	struct mtk_afe *afe = file->private_data;
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value;
	int n = 0;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	pm_runtime_get_sync(afe->dev);

	mtk_afe_enable_main_clk(afe);


	for (i = 0; i < ARRAY_SIZE(hdmi_dump_regs); i++) {
		if (regmap_read(afe->regmap, hdmi_dump_regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s = N/A\n",
				       hdmi_dump_regs[i].name);
		else
			n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
				       hdmi_dump_regs[i].name, reg_value);
	}

	mtk_afe_disable_main_clk(afe);

	pm_runtime_put(afe->dev);

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}


static const struct file_operations mtk_afe_fops = {
	.open = simple_open,
	.read = mtk_afe_read_file,
	.write = mtk_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mtk_afe_hdmi_fops = {
	.open = simple_open,
	.read = mtk_afe_hdmi_read_file,
	.llseek = default_llseek,
};

static const struct mtk_afe_debug_fs afe_debug_fs[] = {
	{"mtksocaudio", &mtk_afe_fops},
	{"mtksochdmiaudio", &mtk_afe_hdmi_fops},
};

#endif

void mtk_afe_init_debugfs(struct mtk_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	int i;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++) {
		afe->debugfs_dentry[i] = debugfs_create_file(afe_debug_fs[i].fs_name,
							  0644, NULL, afe,
							  afe_debug_fs[i].fops);
		if (!afe->debugfs_dentry[i])
			dev_warn(afe->dev, "%s failed to create %s debugfs file\n",
				 __func__, afe_debug_fs[i].fs_name);
	}
#endif
}

void mtk_afe_cleanup_debugfs(struct mtk_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	int i;

	for (i = 0; i < MTK_AFE_DEBUGFS_NUM; i++)
		debugfs_remove(afe->debugfs_dentry[i]);
#endif
}
