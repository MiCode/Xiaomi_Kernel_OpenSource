/*
 * arch/arm/mach-tegra/tegra_simon.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "clock.h"
#include "tegra_ism.h"
#include "tegra_apb2jtag.h"

#define NV_ISM_ROSC_BIN_IDDQ_LSB		8
#define NV_ISM_ROSC_BIN_IDDQ_MSB		8
#define NV_ISM_ROSC_BIN_MODE_LSB		9
#define NV_ISM_ROSC_BIN_MODE_MSB		10
#define NV_ISM_ROSC_BIN_OUT_DIV_LSB		5
#define NV_ISM_ROSC_BIN_OUT_DIV_MSB		7
#define NV_ISM_ROSC_BIN_LOCAL_DURATION_LSB	11
#define NV_ISM_ROSC_BIN_LOCAL_DRUATION_MSB	26
#define NV_ISM_ROSC_BIN_SRC_SEL_LSB		0
#define NV_ISM_ROSC_BIN_SRC_SEL_MSB		4
#define NV_ISM_ROSC_BIN_COUNT_LSB		27
#define NV_ISM_ROSC_BIN_COUNT_MSB		42

#define A_FCCPLEX0_CLUSTER_BIN_0_T124_ID	0x2C
#define A_FCCPLEX0_CLUSTER_BIN_0_T124_WIDTH	46
#define A_FCCPLEX0_CLUSTER_BIN_0_T124_CHIPLET_SEL 3
#define A_FCCPLEX0_CLUSTER_BIN_0_T124_ROSC_BIN_CPU_FCPU0 2

#define E_TPC0_CLUSTER_BIN_T124_ID	0xC8
#define E_TPC0_CLUSTER_BIN_T124_WIDTH	46
#define E_TPC0_CLUSTER_BIN_T124_CHIPLET_SEL	2
#define E_TPC0_CLUSTER_BIN_T124_ROSC_BIN_GPU_ET0TX0A_GPCCLK_TEX_P00 2

static u32 delay_us = 1000;

static u32 get_bits(u32 value, u32 lsb, u32 msb)
{
	u32 mask = 0xFFFFFFFF;
	u32 num_bits = msb - lsb + 1;
	if (num_bits + lsb > 32)
		return 0;
	if (num_bits < 32)
		mask = (1 << num_bits) - 1;

	return (value >> lsb) & mask;
}

static u32 get_buf_bits(u32 *buf, u32 lsb, u32 msb)
{
	u32 lower_addr = lsb / 32;
	u32 upper_addr = msb / 32;
	u32 ret = 0;

	if (msb < lsb)
		return 0;
	if (msb - lsb + 1 > 32)
		return 0;
	lsb = lsb % 32;
	msb = msb % 32;
	if (lower_addr == upper_addr) {
		ret = get_bits(buf[lower_addr], lsb, msb);
	} else {
		ret = get_bits(buf[lower_addr], lsb, 31);
		ret = ret | (get_bits(buf[upper_addr], 0, msb) << (32 - lsb));
	}
	return ret;
}

static inline u32 set_bits(u32 cur_val, u32 new_val, u32 lsb, u32 msb)
{
	u32 mask = 0xFFFFFFFF;
	u32 num_bits = msb - lsb + 1;
	if (num_bits + lsb > 32)
		return 0;
	if (num_bits < 32)
		mask = (1 << num_bits) - 1;
	return (cur_val & ~(mask << lsb)) | ((new_val & mask) << lsb);
}

static void set_buf_bits(u32 *buf, u32 val, u32 lsb, u32 msb)
{
	u32 lower_addr = lsb / 32;
	u32 upper_addr = msb / 32;
	u32 num_bits;

	if (msb < lsb)
		return;
	if (msb - lsb + 1 > 32)
		return;
	num_bits = msb - lsb + 1;
	lsb = lsb % 32;
	msb = msb % 32;
	if (lower_addr == upper_addr) {
		buf[lower_addr] = set_bits(buf[lower_addr], val, lsb, msb);
	} else {
		buf[lower_addr] = set_bits(buf[lower_addr], val, lsb, 31);
		buf[upper_addr] = set_bits(buf[upper_addr], val >> (32 - lsb),
					   0, msb);
	}
}

static u32 set_disable_ism(u32 mode, u32 offset,
			u8 chiplet, u16 len, u8 instr_id, u32 disable)
{
	u32 buf[2];
	int ret;

	memset(buf, 0, sizeof(buf));
	set_buf_bits(buf, mode, NV_ISM_ROSC_BIN_MODE_LSB + offset,
			NV_ISM_ROSC_BIN_MODE_MSB + offset);
	set_buf_bits(buf, disable, NV_ISM_ROSC_BIN_IDDQ_LSB + offset,
			NV_ISM_ROSC_BIN_IDDQ_MSB + offset);
	ret = apb2jtag_write(instr_id, len, chiplet, buf);
	if (ret < 0)
		pr_err("set_disable_ism: APB2JTAG write failed\n");
	udelay(delay_us);
	return ret;
}

u32 read_ism(u32 mode, u32 duration, u32 div, u32 sel, u32 offset,
	     u8 chiplet, u16 len, u8 instr_id)
{
	u32 buf[2];
	int ret;
	u32 count;

	/* Toggle IDDQ to reset ISM */
	set_disable_ism(mode, offset, chiplet, len, instr_id, 1);
	memset(buf, 0, sizeof(buf));

	set_buf_bits(buf, mode, NV_ISM_ROSC_BIN_MODE_LSB + offset,
			NV_ISM_ROSC_BIN_MODE_MSB + offset);
	set_buf_bits(buf, sel, NV_ISM_ROSC_BIN_SRC_SEL_LSB + offset,
			NV_ISM_ROSC_BIN_SRC_SEL_MSB + offset);
	set_buf_bits(buf, div, NV_ISM_ROSC_BIN_OUT_DIV_LSB + offset,
			NV_ISM_ROSC_BIN_OUT_DIV_MSB + offset);
	set_buf_bits(buf, duration, NV_ISM_ROSC_BIN_LOCAL_DURATION_LSB + offset,
			NV_ISM_ROSC_BIN_LOCAL_DRUATION_MSB + offset);
	set_buf_bits(buf, 0, NV_ISM_ROSC_BIN_IDDQ_LSB + offset,
			NV_ISM_ROSC_BIN_IDDQ_MSB + offset);

	apb2jtag_get();
	ret = apb2jtag_write_locked(instr_id, len, chiplet, buf);
	if (ret < 0) {
		pr_err("read_ism: APB2JTAG write failed.\n");
		apb2jtag_put();
		return 0;
	}

	/* Delay ensures that the apb2jtag interface has time to respond */
	udelay(delay_us);

	memset(buf, 0, sizeof(buf));
	ret = apb2jtag_read_locked(instr_id, len, chiplet, buf);
	apb2jtag_put();

	if (ret < 0) {
		pr_err("read_ism: APB2JTAG read failed.\n");
		return 0;
	}
	count = get_buf_bits(buf, NV_ISM_ROSC_BIN_COUNT_LSB + offset,
				NV_ISM_ROSC_BIN_COUNT_MSB + offset);
	return (count * (tegra_clk_measure_input_freq() / 1000000) *
		(1 << div)) / duration;
}

static void initialize_cpu_isms(void)
{
	u32 buf[3];

	memset(buf, 0, sizeof(buf));

	/* FCCPLEX jtag_reset_clamp_en */
	apb2jtag_read(0x0C, 84, 3, buf);
	set_buf_bits(buf, 1, 82, 82);
	apb2jtag_write(0x0C, 84, 3, buf);
}

static void reset_cpu_isms(void)
{
	u32 buf[3];

	memset(buf, 0, sizeof(buf));

	/* FCCPLEX jtag_reset_clamp_en */
	apb2jtag_read(0x0C, 84, 3, buf);
	set_buf_bits(buf, 0, 82, 82);
	apb2jtag_write(0x0C, 84, 3, buf);
}

/*
 * Reads the CPU0 ROSC BIN ISM frequency.
 * Assumes VDD_CPU is ON.
 */
u32 read_cpu0_ism(u32 mode, u32 duration, u32 div, u32 sel)
{
	u32 ret;
	initialize_cpu_isms();
	ret = read_ism(mode, duration, div, sel,
			A_FCCPLEX0_CLUSTER_BIN_0_T124_ROSC_BIN_CPU_FCPU0,
			A_FCCPLEX0_CLUSTER_BIN_0_T124_CHIPLET_SEL,
			A_FCCPLEX0_CLUSTER_BIN_0_T124_WIDTH,
			A_FCCPLEX0_CLUSTER_BIN_0_T124_ID);
	reset_cpu_isms();
	return ret;
}

static void initialize_gpu_isms(void)
{
	u32 buf[2];

	memset(buf, 0, sizeof(buf));

	/* A_GPU0 power_reset_n, get the reg out of reset */
	apb2jtag_read(0x25, 33, 2, buf);
	set_buf_bits(buf, 1, 1, 1);
	apb2jtag_write(0x25, 33, 2, buf);
}

static void reset_gpu_isms(void)
{
	u32 buf[2];

	memset(buf, 0, sizeof(buf));

	/*
	 * A_GPU0 power_reset_n, put back the reg in reset otherwise
	 * it will be in a bad state after the domain unpowergates
	 */
	apb2jtag_read(0x25, 33, 2, buf);
	set_buf_bits(buf, 0, 1, 1);
	apb2jtag_write(0x25, 33, 2, buf);
}

/*
 * Reads the GPU ROSC BIN ISM frequency.
 * Assumes VDD_GPU is ON.
 */
u32 read_gpu_ism(u32 mode, u32 duration, u32 div, u32 sel)
{
	u32 ret;
	initialize_gpu_isms();
	ret = read_ism(mode, duration, div, sel,
		E_TPC0_CLUSTER_BIN_T124_ROSC_BIN_GPU_ET0TX0A_GPCCLK_TEX_P00,
		E_TPC0_CLUSTER_BIN_T124_CHIPLET_SEL,
		E_TPC0_CLUSTER_BIN_T124_WIDTH,
		E_TPC0_CLUSTER_BIN_T124_ID);
	reset_gpu_isms();
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int cpu_ism_show(struct seq_file *s, void *data)
{
	char buf[150];
	u32 ro29, ro30, ro30_2;
	int ret;

	ro29 = read_cpu0_ism(0, 600, 3, 29);
	ro30 = read_cpu0_ism(0, 600, 3, 30);
	ro30_2 = read_cpu0_ism(2, 3000, 0, 30);
	ret = snprintf(buf, sizeof(buf),
			"RO29: %u RO30: %u RO30_2: %u diff: %d\n",
			ro29, ro30, ro30_2, ro29 - ro30);
	if (ret < 0)
		return ret;

	seq_write(s, buf, ret);
	return 0;
}

static int cpu_ism_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpu_ism_show, inode->i_private);
}

static const struct file_operations cpu_ism_fops = {
	.open		= cpu_ism_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int gpu_ism_show(struct seq_file *s, void *data)
{
	char buf[150];
	u32 ro29, ro30, ro30_2;
	int ret;

	ro29 = read_gpu_ism(0, 600, 3, 29);
	ro30 = read_gpu_ism(0, 600, 3, 30);
	ro30_2 = read_gpu_ism(2, 3000, 0, 30);
	ret = snprintf(buf, sizeof(buf),
			"RO29: %u RO30: %u RO30_2: %u diff: %d\n",
			ro29, ro30, ro30_2, ro29 - ro30);
	if (ret < 0)
		return ret;

	seq_write(s, buf, ret);
	return 0;
}

static int gpu_ism_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpu_ism_show, inode->i_private);
}

static const struct file_operations gpu_ism_fops = {
	.open		= gpu_ism_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init debugfs_init(void)
{
	struct dentry *dfs_file, *dfs_dir;

	dfs_dir = debugfs_create_dir("tegra12_ism", NULL);
	if (!dfs_dir)
		return -ENOMEM;

	dfs_file = debugfs_create_u32("delay_us", 0644, dfs_dir, &delay_us);
	if (!dfs_file)
		goto err;
	dfs_file = debugfs_create_file("cpu_ism", 0644, dfs_dir, NULL,
					&cpu_ism_fops);
	if (!dfs_file)
		goto err;
	dfs_file = debugfs_create_file("gpu_ism", 0644, dfs_dir, NULL,
					&gpu_ism_fops);
	if (!dfs_file)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dfs_dir);
	return -ENOMEM;
}
late_initcall_sync(debugfs_init);
#endif
