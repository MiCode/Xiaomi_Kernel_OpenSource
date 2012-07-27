/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/sort.h>
#include <asm/uaccess.h>
#include <mach/msm_iomap.h>
#include "timer.h"
#include "rpm_rbcpr_stats.h"

#define RBCPR_USER_BUF		(2000)
#define STR(a)  (#a)
#define GETFIELD(a)     ((strnstr(STR(a), "->", 80) + 2))
#define PRINTFIELD(buf, buf_size, pos, format, ...) \
	((pos < buf_size) ? snprintf((buf + pos), (buf_size - pos), format,\
		## __VA_ARGS__) : 0)

enum {
	RBCPR_CORNER_SVS = 0,
	RBCPR_CORNER_NOMINAL,
	RBCPR_CORNER_TURBO,
	RBCPR_CORNERS_COUNT,
	RBCPR_CORNER_INVALID = 0x7FFFFFFF,
};

struct msm_rpmrbcpr_recmnd {
	uint32_t voltage;
	uint32_t timestamp;
};

struct msm_rpmrbcpr_corners {
	int efuse_adjustment;
	struct msm_rpmrbcpr_recmnd *rpm_rcmnd;
	uint32_t programmed_voltage;
	uint32_t isr_counter;
	uint32_t min_counter;
	uint32_t max_counter;
};

struct msm_rpmrbcpr_stats {
	uint32_t status_count;
	uint32_t num_corners;
	uint32_t num_latest_recommends;
	struct msm_rpmrbcpr_corners *rbcpr_corners;
	uint32_t current_corner;
	uint32_t railway_voltage;
	uint32_t enable;
};

struct msm_rpmrbcpr_stats_internal {
	void __iomem *regbase;
	uint32_t len;
	char buf[RBCPR_USER_BUF];
};

static DEFINE_SPINLOCK(rpm_rbcpr_lock);
static struct msm_rpmrbcpr_design_data rbcpr_design_data;
static struct msm_rpmrbcpr_stats rbcpr_stats;
static struct msm_rpmrbcpr_stats_internal pvtdata;

static inline unsigned long msm_rpmrbcpr_read_data(void __iomem *regbase,
						int offset)
{
	return readl_relaxed(regbase + (offset * 4));
}

static int msm_rpmrbcpr_cmp_func(const void *a, const void *b)
{
	struct msm_rpmrbcpr_recmnd *pa = (struct msm_rpmrbcpr_recmnd *)(a);
	struct msm_rpmrbcpr_recmnd *pb = (struct msm_rpmrbcpr_recmnd *)(b);
	return pa->timestamp - pb->timestamp;
}

static char *msm_rpmrbcpr_corner_string(uint32_t corner)
{
	switch (corner) {
	case RBCPR_CORNER_SVS:
		return STR(RBCPR_CORNER_SVS);
		break;
	case RBCPR_CORNER_NOMINAL:
		return STR(RBCPR_CORNER_NOMINAL);
		break;
	case RBCPR_CORNER_TURBO:
		return STR(RBCPR_CORNER_TURBO);
		break;
	case RBCPR_CORNERS_COUNT:
	case RBCPR_CORNER_INVALID:
	default:
		return STR(RBCPR_CORNER_INVALID);
		break;
	}
}

static int msm_rpmrbcpr_print_buf(struct msm_rpmrbcpr_stats *pdata,
				struct msm_rpmrbcpr_design_data *pdesdata,
				char *buf)
{
	int pos = 0;
	struct msm_rpmrbcpr_corners *corners;
	struct msm_rpmrbcpr_recmnd *rcmnd;
	int i, j;
	int current_timestamp = msm_timer_get_sclk_ticks();

	if (!pdata->enable) {
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
				"RBCPR Stats not enabled at RPM");
		return pos;
	}

	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
				":RBCPR Platform Data");
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %u)", GETFIELD(pdesdata->upside_steps),
				pdesdata->upside_steps);
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s:%u)", GETFIELD(pdesdata->downside_steps),
				pdesdata->downside_steps);
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %d)", GETFIELD(pdesdata->svs_voltage),
				pdesdata->svs_voltage);
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %d)", GETFIELD(pdesdata->nominal_voltage),
				pdesdata->nominal_voltage);
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %d)\n", GETFIELD(pdesdata->turbo_voltage),
				pdesdata->turbo_voltage);

	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
				":RBCPR Stats");
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %u)", GETFIELD(pdata->status_counter),
				pdata->status_count);
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %s)", GETFIELD(pdata->current_corner),
			msm_rpmrbcpr_corner_string(pdata->current_corner));
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (current_timestamp: 0x%x)",
				current_timestamp);
	pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
		" (%s: %u)\n", GETFIELD(pdata->railway_voltage),
			pdata->railway_voltage);

	for (i = 0; i < pdata->num_corners; i++) {
		corners = &pdata->rbcpr_corners[i];
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
				":\tRBCPR Corner Data");
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			" (name: %s)", msm_rpmrbcpr_corner_string(i));
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			" (%s: %d)", GETFIELD(corners->efuse_adjustment),
			corners->efuse_adjustment);
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			" (%s: %u)", GETFIELD(corners->programmed_voltage),
						corners->programmed_voltage);
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			" (%s: %u)", GETFIELD(corners->isr_counter),
						corners->isr_counter);
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			"(%s: %u)", GETFIELD(corners->min_counter),
						corners->min_counter);
		pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			"(%s:%u)\n", GETFIELD(corners->max_counter),
						corners->max_counter);
		for (j = 0; j < pdata->num_latest_recommends; j++) {
			pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
				":\t\tVoltage History[%d]", j);
			rcmnd = &corners->rpm_rcmnd[j];
			pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			" (%s: %u)", GETFIELD(rcmnd->voltage),
							rcmnd->voltage);
			pos += PRINTFIELD(buf, RBCPR_USER_BUF, pos,
			" (%s: 0x%x)\n", GETFIELD(rcmnd->timestamp),
							rcmnd->timestamp);
		}
	}
	return pos;
}


static void msm_rpmrbcpr_copy_data(struct msm_rpmrbcpr_stats_internal *pdata,
					struct msm_rpmrbcpr_stats *prbcpr_stats)
{
	struct msm_rpmrbcpr_corners *corners;
	struct msm_rpmrbcpr_recmnd *rcmnd;
	int i, j;
	int offset = (offsetof(struct msm_rpmrbcpr_stats, rbcpr_corners) / 4);

	if (!prbcpr_stats)
		return;

	for (i = 0; i < prbcpr_stats->num_corners; i++) {
		corners = &prbcpr_stats->rbcpr_corners[i];
		corners->efuse_adjustment = msm_rpmrbcpr_read_data(
					pdata->regbase, offset++);
		for (j = 0; j < prbcpr_stats->num_latest_recommends; j++) {
			rcmnd = &corners->rpm_rcmnd[j];
			rcmnd->voltage = msm_rpmrbcpr_read_data(
					pdata->regbase, offset++);
			rcmnd->timestamp = msm_rpmrbcpr_read_data(
					pdata->regbase, offset++);
		}
		sort(&corners->rpm_rcmnd[0],
			prbcpr_stats->num_latest_recommends,
			sizeof(struct msm_rpmrbcpr_recmnd),
			msm_rpmrbcpr_cmp_func, NULL);
		corners->programmed_voltage = msm_rpmrbcpr_read_data(
					pdata->regbase, offset++);
		corners->isr_counter = msm_rpmrbcpr_read_data(pdata->regbase,
					offset++);
		corners->min_counter = msm_rpmrbcpr_read_data(pdata->regbase,
					offset++);
		corners->max_counter = msm_rpmrbcpr_read_data(pdata->regbase,
					offset++);
	}
	prbcpr_stats->current_corner = msm_rpmrbcpr_read_data(pdata->regbase,
					offset++);
	prbcpr_stats->railway_voltage = msm_rpmrbcpr_read_data
				(pdata->regbase, offset++);
	prbcpr_stats->enable = msm_rpmrbcpr_read_data(pdata->regbase, offset++);
}

static int msm_rpmrbcpr_file_read(struct file *file, char __user *bufu,
				size_t count, loff_t *ppos)
{
	struct msm_rpmrbcpr_stats_internal *pdata = file->private_data;
	int ret;
	int status_counter;

	if (!pdata) {
		pr_info("%s pdata is null", __func__);
		return -EINVAL;
	}

	if (!bufu || count < 0) {
		pr_info("%s count %d ", __func__, count);
		return -EINVAL;
	}

	if (*ppos > pdata->len || !pdata->len) {
		/* Read RPM stats */
		status_counter = readl_relaxed(pdata->regbase +
			offsetof(struct msm_rpmrbcpr_stats, status_count));
		if (status_counter != rbcpr_stats.status_count) {
			spin_lock(&rpm_rbcpr_lock);
			msm_rpmrbcpr_copy_data(pdata, &rbcpr_stats);
			rbcpr_stats.status_count = status_counter;
			spin_unlock(&rpm_rbcpr_lock);
		}
		pdata->len = msm_rpmrbcpr_print_buf(&rbcpr_stats,
				&rbcpr_design_data, pdata->buf);
		*ppos = 0;
	}
	/* copy to user data */
	ret = simple_read_from_buffer(bufu, count, ppos, pdata->buf,
					pdata->len);
	return ret;
}

static void msm_rpmrbcpr_free_mem(struct msm_rpmrbcpr_stats_internal *pvtdata,
				struct msm_rpmrbcpr_stats *prbcpr_stats)
{
	int i;
	if (pvtdata->regbase)
		iounmap(pvtdata->regbase);


	if (prbcpr_stats) {
		for (i = 0; i < prbcpr_stats->num_corners; i++) {
			kfree(prbcpr_stats->rbcpr_corners[i].rpm_rcmnd);
			prbcpr_stats->rbcpr_corners[i].rpm_rcmnd = NULL;
		}

		kfree(prbcpr_stats->rbcpr_corners);
		prbcpr_stats->rbcpr_corners = NULL;
	}
}

static int msm_rpmrbcpr_allocate_mem(struct msm_rpmrbcpr_platform_data *pdata,
				struct resource *res)
{
	int i;

	pvtdata.regbase = ioremap(res->start, (res->end - res->start + 1));
	memcpy(&rbcpr_design_data, &pdata->rbcpr_data,
			sizeof(struct msm_rpmrbcpr_design_data));


	rbcpr_stats.num_corners = readl_relaxed(pvtdata.regbase +
			offsetof(struct msm_rpmrbcpr_stats, num_corners));
	rbcpr_stats.num_latest_recommends = readl_relaxed(pvtdata.regbase +
			offsetof(struct msm_rpmrbcpr_stats,
				num_latest_recommends));

	rbcpr_stats.rbcpr_corners = kzalloc(
				sizeof(struct msm_rpmrbcpr_corners)
				* rbcpr_stats.num_corners, GFP_KERNEL);

	if (!rbcpr_stats.rbcpr_corners) {
		msm_rpmrbcpr_free_mem(&pvtdata, &rbcpr_stats);
		return -ENOMEM;
	}

	for (i = 0; i < rbcpr_stats.num_corners; i++) {
		rbcpr_stats.rbcpr_corners[i].rpm_rcmnd =
			kzalloc(sizeof(struct msm_rpmrbcpr_corners)
				* rbcpr_stats.num_latest_recommends,
							GFP_KERNEL);

		if (!rbcpr_stats.rbcpr_corners[i].rpm_rcmnd) {
			msm_rpmrbcpr_free_mem(&pvtdata, &rbcpr_stats);
			return -ENOMEM;
		}
	}
	return 0;
}

static int msm_rpmrbcpr_file_open(struct inode *inode, struct file *file)
{
	file->private_data = &pvtdata;
	pvtdata.len = 0;

	if (!pvtdata.regbase)
		return -EBUSY;

	return 0;
}

static int msm_rpmrbcpr_file_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations msm_rpmrbcpr_fops = {
	.owner    = THIS_MODULE,
	.open     = msm_rpmrbcpr_file_open,
	.read     = msm_rpmrbcpr_file_read,
	.release  = msm_rpmrbcpr_file_close,
	.llseek   = no_llseek,
};

static  int __devinit msm_rpmrbcpr_probe(struct platform_device *pdev)
{
	struct dentry *dent;
	struct msm_rpmrbcpr_platform_data *pdata;
	int ret = 0;

	pdata = pdev->dev.platform_data;
	if (!pdata)
		return -EINVAL;
	dent = debugfs_create_file("rpm_rbcpr", S_IRUGO, NULL,
	pdev->dev.platform_data, &msm_rpmrbcpr_fops);

	if (!dent) {
		pr_err("%s: ERROR debugfs_create_file failed\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, dent);
	ret = msm_rpmrbcpr_allocate_mem(pdata, pdev->resource);
	return ret;
}

static int __devexit msm_rpmrbcpr_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	msm_rpmrbcpr_free_mem(&pvtdata, &rbcpr_stats);
	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver msm_rpmrbcpr_driver = {
	.probe  = msm_rpmrbcpr_probe,
	.remove = __devexit_p(msm_rpmrbcpr_remove),
	.driver = {
		.name = "msm_rpm_rbcpr",
		.owner = THIS_MODULE,
	},
};

static int __init msm_rpmrbcpr_init(void)
{
	return platform_driver_register(&msm_rpmrbcpr_driver);
}

static void __exit msm_rpmrbcpr_exit(void)
{
	platform_driver_unregister(&msm_rpmrbcpr_driver);
}

module_init(msm_rpmrbcpr_init);
module_exit(msm_rpmrbcpr_exit);
