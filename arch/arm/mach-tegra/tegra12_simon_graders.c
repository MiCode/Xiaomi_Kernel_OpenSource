/*
 * arch/arm/mach-tegra/tegra_simon_graders.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/tegra-fuse.h>

#include "iomap.h"
#include "tegra_simon.h"
#include "tegra_ism.h"
#include "tegra_simon_graders.h"

#define FUSE_CTRL			0x0
#define FUSE_CTRL_STATE_OFFSET		16
#define FUSE_CTRL_STATE_MASK		0x1F
#define FUSE_CTRL_STATE_IDLE		0x4
#define FUSE_CTRL_CMD_OFFSET		0
#define FUSE_CTRL_CMD_MASK		0x3
#define FUSE_CTRL_CMD_READ		0x1
#define FUSE_ADDR			0x4
#define FUSE_DATA			0x8

#define FIXED_SCALE			14
#define TEMP_COEFF_A			-45   /* -0.0027 * (1 << FIXED_SCALE) */
#define TEMP_COEFF_B			17360 /* 1.0596 * (1 << FIXED_SCALE) */
#define FUSE_SIMON_STATE		113
#define INITIAL_SHIFT_MASK		0x1F
#define CPU_INITIAL_SHIFT_OFFSET	5
#define GPU_INITIAL_SHIFT_OFFSET	0
#define TEGRA_SIMON_THRESHOLD		57344 /* 3.5% */

DEFINE_MUTEX(tegra_simon_fuse_lock);

struct volt_scale_entry {
	int mv;
	int scale;
};

static struct volt_scale_entry volt_scale_table[] = {
	[0] = {
		.mv = 800,
		.scale = 16384, /* 1 << FIXED_SCALE */
	},
	[1] = {
		.mv = 900,
		.scale = 11633, /* 0.71 * (1 << FIXED_SCALE) */
	},
	[2] = {
		.mv = 1000,
		.scale = 8684, /* 0.53 * (1 << FIXED_SCALE) */
	},
};

static s64 scale_voltage(int mv, s64 num)
{
	int i;
	s64 scale;

	for (i = 1; i < ARRAY_SIZE(volt_scale_table); i++) {
		if (volt_scale_table[i].mv >= mv)
			break;
	}

	/* Invalid voltage */
	WARN_ON(i == ARRAY_SIZE(volt_scale_table));
	if (i == ARRAY_SIZE(volt_scale_table))
		return num * volt_scale_table[i - 1].scale;


	/* Interpolate/Extrapolate for exacte scale value */
	scale = (volt_scale_table[i].scale - volt_scale_table[i - 1].scale) /
		(volt_scale_table[i].mv - volt_scale_table[i - 1].mv);
	scale = scale * (mv - volt_scale_table[i - 1].mv) +
		volt_scale_table[i - 1].scale;

	do_div(num, scale);

	return num;
}

static s64 scale_temp(int temperature_mc, s64 num)
{
	/* num / (a * T + b) */
	int sign = 1;
	s64 scale = TEMP_COEFF_A;
	scale = scale * temperature_mc;
	if (scale < 0) {
		sign = -1;
		scale = scale * sign;
	}
	do_div(scale, 1000);
	scale = scale * sign;
	scale += TEMP_COEFF_B;
	do_div(num, scale);

	return num;
}

#define FUSE_TIMEOUT		20

static u32 get_tegra_simon_fuse(void)
{
	u32 ctrl_state = ~FUSE_CTRL_STATE_IDLE;
	int timeout = 0;
	u32 reg;

	mutex_lock(&tegra_simon_fuse_lock);

	/* Wait for fuse controller to go idle */
	while (ctrl_state != FUSE_CTRL_STATE_IDLE && timeout < FUSE_TIMEOUT) {
		ctrl_state = tegra_fuse_readl(FUSE_CTRL);
		ctrl_state >>= FUSE_CTRL_STATE_OFFSET;
		ctrl_state &= FUSE_CTRL_STATE_MASK;
		msleep(50);
		timeout++;
	}

	if (timeout == FUSE_TIMEOUT)
		return 0;

	/* Setup fuse to read */
	tegra_fuse_writel(FUSE_SIMON_STATE, FUSE_ADDR);
	reg = tegra_fuse_readl(FUSE_CTRL);
	reg = reg & ~(FUSE_CTRL_CMD_MASK << FUSE_CTRL_CMD_OFFSET);
	reg = reg | (FUSE_CTRL_CMD_READ << FUSE_CTRL_CMD_OFFSET);
	tegra_fuse_writel(reg, FUSE_CTRL);

	/* Wait for read to complete */
	ctrl_state = ~FUSE_CTRL_STATE_IDLE;
	timeout = 0;
	while (ctrl_state != FUSE_CTRL_STATE_IDLE && timeout < FUSE_TIMEOUT) {
		ctrl_state = tegra_fuse_readl(FUSE_CTRL);
		ctrl_state >>= FUSE_CTRL_STATE_OFFSET;
		ctrl_state &= FUSE_CTRL_STATE_MASK;
		msleep(50);
		timeout++;
	}

	if (timeout == FUSE_TIMEOUT)
		return 0;

	reg = tegra_fuse_readl(FUSE_DATA);

	mutex_unlock(&tegra_simon_fuse_lock);

	return reg;
}

int grade_gpu_simon_domain(int domain, int mv, int temperature)
{
	u32 ro29, ro30;
	s64 shift, cur_shift, initial_shift;

	if (domain != TEGRA_SIMON_DOMAIN_GPU)
		return 0;

	ro29 = read_gpu_ism(0, 600, 3, 29);
	ro30 = read_gpu_ism(2, 3000, 0, 30);

	if (!ro29)
		return 0;

	shift = ro30;
	shift = (shift << FIXED_SCALE) * 100;
	do_div(shift, ro29);

	/* Normalize for voltage */
	shift = (shift << FIXED_SCALE);
	shift = scale_voltage(mv, shift);

	/* Normalize for temperature */
	shift = (shift << FIXED_SCALE);
	shift = scale_temp(temperature, shift);

	initial_shift = get_tegra_simon_fuse();
	initial_shift = (initial_shift >> GPU_INITIAL_SHIFT_OFFSET) &
				INITIAL_SHIFT_MASK;
	/* Invalid fuse */
	if (!initial_shift || initial_shift == INITIAL_SHIFT_MASK)
		return 0;

	initial_shift = (initial_shift << FIXED_SCALE) * 8;
	do_div(initial_shift, 31);
	initial_shift = initial_shift - (4 << FIXED_SCALE);
	cur_shift = shift - initial_shift;

	return cur_shift < TEGRA_SIMON_THRESHOLD;
}

int grade_cpu_simon_domain(int domain, int mv, int temperature)
{
	u32 ro29, ro30;
	s64 shift, cur_shift, initial_shift;

	if (domain != TEGRA_SIMON_DOMAIN_CPU)
		return 0;

	ro29 = read_cpu0_ism(0, 600, 3, 29);
	ro30 = read_cpu0_ism(2, 3000, 0, 30);

	if (!ro29)
		return 0;

	shift = ro30;
	shift = (shift << FIXED_SCALE) * 100;
	do_div(shift, ro29);

	/* Normalize for voltage */
	shift = (shift << FIXED_SCALE);
	shift = scale_voltage(mv, shift);

	/* Normalize for temperature */
	shift = (shift << FIXED_SCALE);
	shift = scale_temp(temperature, shift);

	initial_shift = get_tegra_simon_fuse();
	initial_shift = (initial_shift >> CPU_INITIAL_SHIFT_OFFSET) &
				INITIAL_SHIFT_MASK;
	/* Invalid fuse */
	if (!initial_shift || initial_shift == INITIAL_SHIFT_MASK)
		return 0;

	initial_shift = (initial_shift << FIXED_SCALE) * 8;
	do_div(initial_shift, 31);
	initial_shift = initial_shift - (4 << FIXED_SCALE);
	cur_shift = shift - initial_shift;

	return cur_shift < TEGRA_SIMON_THRESHOLD;
}

static struct tegra_simon_grader_desc gpu_grader_desc = {
	.domain = TEGRA_SIMON_DOMAIN_GPU,
	.grading_mv_max = 850,
	.grading_temperature_min = 20000,
	.settle_us = 3000,
	.grade_simon_domain = grade_gpu_simon_domain,
};

static struct tegra_simon_grader_desc cpu_grader_desc = {
	.domain = TEGRA_SIMON_DOMAIN_CPU,
	.grading_rate_max = 850000000,
	.grading_temperature_min = 20000,
	.settle_us = 3000,
	.grade_simon_domain = grade_cpu_simon_domain,
};

static int __init tegra12_simon_graders_init(void)
{
	tegra_simon_add_grader(&gpu_grader_desc);
	tegra_simon_add_grader(&cpu_grader_desc);
	return 0;
}
late_initcall_sync(tegra12_simon_graders_init);

#ifdef CONFIG_DEBUG_FS
static u32 fuse_offset = FUSE_SIMON_STATE;

static int fuse_show(struct seq_file *s, void *data)
{
	char buf[150];
	int ret;
	u32 fuse = get_tegra_simon_fuse();

	ret = snprintf(buf, sizeof(buf),
			"GPU Fuse: %u\nCPU Fuse: %u\nRaw: 0x%x\n",
			(fuse >> GPU_INITIAL_SHIFT_OFFSET) & INITIAL_SHIFT_MASK,
			(fuse >> CPU_INITIAL_SHIFT_OFFSET) &
			INITIAL_SHIFT_MASK,
			fuse);
	if (ret < 0)
		return ret;

	seq_write(s, buf, ret);
	return 0;
}

static int fuse_open(struct inode *inode, struct file *file)
{
	return single_open(file, fuse_show, inode->i_private);
}

static const struct file_operations fuse_fops = {
	.open		= fuse_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init debugfs_init(void)
{
	struct dentry *dfs_file, *dfs_dir;
	dfs_dir = debugfs_create_dir("tegra12_simon_graders", NULL);
	if (!dfs_dir)
		return -ENOMEM;

	dfs_file = debugfs_create_file("fuses", 0644, dfs_dir, NULL,
					&fuse_fops);
	if (!dfs_file)
		goto err;

	dfs_file = debugfs_create_u32("fuse_offset", 0644, dfs_dir,
			&fuse_offset);

	return 0;
err:
	debugfs_remove_recursive(dfs_dir);
	return -ENOMEM;
}
late_initcall_sync(debugfs_init);
#endif
