/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/sort.h>
#include <asm/uaccess.h>
#include <mach/msm_iomap.h>

#define RBCPR_BUF_LEN 8000
#define RBCPR_STATS_MAX_SIZE SZ_2K
#define RBCPR_MAX_RAILS 4
#define RBCPR_NUM_RECMNDS 3
#define RBCPR_NUM_CORNERS 3

#define FIELD(a)     ((strnstr(#a, "->", 80) + 2))
#define PRINT(buf, pos, format, ...) \
	((pos < RBCPR_BUF_LEN) ? snprintf((buf + pos), (RBCPR_BUF_LEN - pos),\
	format, ## __VA_ARGS__) : 0)

enum {
	CORNER_OFF,
	CORNER_SVS,
	CORNER_NOMINAL,
	CORNER_TURBO,
	CORNER_MAX,
};

struct rbcpr_recmnd_data_type {
	uint32_t microvolts;
	uint64_t timestamp;
};

struct rbcpr_corners_data_type {
	uint32_t efuse_adjustment;
	uint32_t programmed_voltage;
	uint32_t isr_count;
	uint32_t min_count;
	uint32_t max_count;
	struct rbcpr_recmnd_data_type rbcpr_recmnd[RBCPR_NUM_RECMNDS];
};

struct rbcpr_rail_stats_type {
	uint32_t num_corners;
	uint32_t num_latest_recommends;
	struct rbcpr_corners_data_type rbcpr_corners[RBCPR_NUM_CORNERS];
	uint32_t current_corner;
	uint32_t railway_voltage;
	uint32_t off_corner;
	uint32_t margin;
};

struct rbcpr_stats_type {
	uint32_t num_rails;
	uint32_t status;
	struct rbcpr_rail_stats_type *rbcpr_rail;
};

struct rbcpr_data_type {
	void __iomem *start;
	uint32_t len;
	char buf[RBCPR_BUF_LEN];
};

static char *rbcpr_rail_labels[] = {
	[0] = "VDD-CX",
	[1] = "VDD-GFX",
};

static char *rbcpr_corner_string[] = {
	[CORNER_OFF] = "CORNERS_OFF",
	[CORNER_SVS] = "SVS",
	[CORNER_NOMINAL] = "NOMINAL",
	[CORNER_TURBO] = "TURBO",
};
#define CORNER_STRING(a)	\
	((a >= CORNER_MAX) ? "INVALID Corner" : rbcpr_corner_string[a])

static struct rbcpr_data_type *rbcpr_data;
static struct rbcpr_stats_type *rbcpr_stats;

static void msm_rpmrbcpr_read_rpm_data(void)
{
	uint32_t start_offset;
	uint32_t stats_size;

	start_offset = offsetof(struct rbcpr_stats_type, rbcpr_rail);
	stats_size =
		rbcpr_stats->num_rails * sizeof(struct rbcpr_rail_stats_type);

	if (stats_size > RBCPR_STATS_MAX_SIZE) {
		pr_err("%s: Max copy size exceeded. stats size %d max %d",
			__func__, stats_size, RBCPR_STATS_MAX_SIZE);
		return;
	}

	memcpy_fromio(rbcpr_stats->rbcpr_rail,
		(rbcpr_data->start + start_offset), stats_size);
}

static uint32_t msm_rpmrbcpr_print_data(void)
{
	uint32_t pos = 0;
	uint32_t i, j, k;
	struct rbcpr_rail_stats_type *rail;
	struct rbcpr_corners_data_type *corner;
	struct rbcpr_recmnd_data_type *rbcpr_recmnd;
	char *buf = rbcpr_data->buf;

	pos += PRINT(buf, pos, ":RBCPR STATS\n");
	pos += PRINT(buf, pos, "(%s: %d)", FIELD(rbcpr_stats->num_rails),
				rbcpr_stats->num_rails);
	pos += PRINT(buf, pos, "(%s: %d)\n", FIELD(rbcpr_stats->status),
				rbcpr_stats->status);


	for (i = 0; i < rbcpr_stats->num_rails; i++) {
		rail = &rbcpr_stats->rbcpr_rail[i];
		pos += PRINT(buf, pos, ":%s Rail Data\n", rbcpr_rail_labels[i]);
		pos += PRINT(buf, pos, "(%s: %s)",
			FIELD(rail->current_corner),
			CORNER_STRING(rail->current_corner));
		pos += PRINT(buf, pos, "(%s: %d)",
			FIELD(rail->railway_voltage), rail->railway_voltage);
		pos += PRINT(buf, pos, "(%s: %d)",
			FIELD(rail->off_corner), rail->off_corner);
		pos += PRINT(buf, pos, "(%s: %d)\n",
			FIELD(rail->margin), rail->margin);

		for (j = 0; j < RBCPR_NUM_CORNERS; j++) {
			pos += PRINT(buf, pos, "\t\tCorner Data:%s ",
							CORNER_STRING(j + 1));
			corner = &rail->rbcpr_corners[j];
			pos += PRINT(buf, pos, "(%s: %d)",
				FIELD(corner->efuse_adjustment),
						corner->efuse_adjustment);
			pos += PRINT(buf, pos, "(%s: %d)",
				FIELD(corner->programmed_voltage),
						corner->programmed_voltage);
			pos += PRINT(buf, pos, "(%s: %d)",
				FIELD(corner->isr_count), corner->isr_count);
			pos += PRINT(buf, pos, "(%s: %d)",
				FIELD(corner->min_count), corner->min_count);
			pos += PRINT(buf, pos, "(%s: %d)\n",
				FIELD(corner->max_count), corner->max_count);


			for (k = 0; k < RBCPR_NUM_RECMNDS; k++) {
				rbcpr_recmnd = &corner->rbcpr_recmnd[k];
				pos += PRINT(buf, pos,
					"\t\t\t\tVoltage History[%d] ", k);
				pos += PRINT(buf, pos, " (%s: %d) ",
					FIELD(rbcpr_recmd->microvolts),
						rbcpr_recmnd->microvolts);
				pos += PRINT(buf, pos, " (%s: %lld)\n",
					FIELD(rbcpr_recmd->timestamp),
						rbcpr_recmnd->timestamp);
			}
		}
	}

	return pos;
}


static int msm_rpmrbcpr_file_read(struct file *file, char __user *bufu,
					size_t count, loff_t *ppos)
{
	struct rbcpr_data_type *pdata = file->private_data;
	int ret = 0;
	int status_counter;
	static DEFINE_MUTEX(rbcpr_lock);

	mutex_lock(&rbcpr_lock);
	if (!pdata) {
		pr_err("%s pdata is null", __func__);
		ret = -EINVAL;
		goto exit_rpmrbcpr_file_read;
	}

	if (!bufu || count < 0) {
		pr_err("%s count %d ", __func__, count);
		ret = -EINVAL;
		goto exit_rpmrbcpr_file_read;
	}

	if (*ppos > pdata->len || !*ppos) {
		/* Read RPM stats */
		status_counter = readl_relaxed(pdata->start +
			offsetof(struct rbcpr_stats_type, status));
		if (status_counter != rbcpr_stats->status) {
			msm_rpmrbcpr_read_rpm_data();
			rbcpr_stats->status = status_counter;
		}
		pdata->len = msm_rpmrbcpr_print_data();
		*ppos = 0;
	}

	/* copy to user data*/
	ret = simple_read_from_buffer(bufu, count, ppos, pdata->buf,
					pdata->len);
exit_rpmrbcpr_file_read:
	mutex_unlock(&rbcpr_lock);
	return ret;
}

static int msm_rpmrbcpr_file_open(struct inode *inode, struct file *file)
{
	file->private_data = rbcpr_data;

	if (!rbcpr_data->start)
		return -ENODEV;

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

static int msm_rpmrbcpr_memalloc(struct platform_device *pdev)
{
	void *addr = NULL;
	int ret = 0;
	uint32_t num_latest_recommends = 0;
	uint32_t num_corners = 0;

	rbcpr_stats->num_rails = readl_relaxed(rbcpr_data->start);

	if (rbcpr_stats->num_rails > RBCPR_MAX_RAILS) {
		pr_err("%s: Invalid number of RPM RBCPR rails %d",
				__func__, rbcpr_stats->num_rails);
		rbcpr_stats->num_rails = 0;
		ret = -EFAULT;
		goto rbcpr_memalloc_fail;
	}

	rbcpr_stats->rbcpr_rail =
		devm_kzalloc(&pdev->dev,
			sizeof(struct rbcpr_rail_stats_type) *
			rbcpr_stats->num_rails, GFP_KERNEL);

	if (!rbcpr_stats->rbcpr_rail) {
		ret = -ENOMEM;
		goto rbcpr_memalloc_fail;
	}

	addr = rbcpr_data->start + offsetof(struct rbcpr_stats_type,
								rbcpr_rail);

	/* Each rail has the same number of corners and number of latest
	   recommended values. Read these from the first rail and check them
	   to make sure the values are valid. (RPM doesn't 0 initialize this
	   memory region, so its possible we end up with bogus values if the
	   rbcpr driver is not initialized.).
	*/
	num_corners = readl_relaxed(addr);
	num_latest_recommends = readl_relaxed(addr +
				offsetof(struct rbcpr_rail_stats_type,
						num_latest_recommends));

	if ((num_latest_recommends != RBCPR_NUM_RECMNDS)
		|| (num_corners != RBCPR_NUM_CORNERS)) {
		pr_err("%s: Invalid num corners %d, num recmnds %d",
			__func__, num_corners, num_latest_recommends);
		ret = -EFAULT;
		goto rbcpr_memalloc_fail;
	}

rbcpr_memalloc_fail:
	return ret;
}

static  int __devinit msm_rpmrbcpr_probe(struct platform_device *pdev)
{
	struct dentry *dent;
	int ret = 0;
	struct resource *res = NULL;
	void __iomem *start_ptr = NULL;
	uint32_t rbcpr_start_addr = 0;
	char *key = NULL;
	uint32_t start_addr;

	rbcpr_data = devm_kzalloc(&pdev->dev,
				sizeof(struct rbcpr_data_type), GFP_KERNEL);

	if (!rbcpr_data)
		return -ENOMEM;

	rbcpr_stats = devm_kzalloc(&pdev->dev,
				sizeof(struct rbcpr_stats_type), GFP_KERNEL);

	if (!rbcpr_stats) {
		pr_err("%s: Failed to allocate memory for RBCPR stats",
						__func__);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		pr_err("%s: Failed to get IO resource from platform device",
				__func__);
		ret = -ENXIO;
		goto rbcpr_probe_fail;
	}

	key = "qcom,start-offset";
	ret = of_property_read_u32(pdev->dev.of_node, key, &start_addr);

	if (ret) {
		pr_err("%s: Failed to get start offset", __func__);
		goto rbcpr_probe_fail;
	}

	start_addr += res->start;
	start_ptr = ioremap_nocache(start_addr, 4);

	if (!start_ptr) {
		pr_err("%s: Failed to remap RBCPR start pointer",
					__func__);
		goto rbcpr_probe_fail;
	}

	rbcpr_start_addr = res->start + readl_relaxed(start_ptr);

	if ((rbcpr_start_addr > (res->end - RBCPR_STATS_MAX_SIZE)) ||
			(rbcpr_start_addr < start_addr)) {
		pr_err("%s: Invalid start address for rbcpr stats 0x%x",
			__func__, rbcpr_start_addr);
		goto rbcpr_probe_fail;
	}

	rbcpr_data->start = devm_ioremap_nocache(&pdev->dev, rbcpr_start_addr,
							RBCPR_STATS_MAX_SIZE);

	if (!rbcpr_data->start) {
		pr_err("%s: Failed to remap RBCPR start address",
				__func__);
		goto rbcpr_probe_fail;
	}

	ret = msm_rpmrbcpr_memalloc(pdev);

	if (ret)
		goto rbcpr_probe_fail;

	dent = debugfs_create_file("rpm_rbcpr", S_IRUGO, NULL,
			pdev->dev.platform_data, &msm_rpmrbcpr_fops);

	if (!dent) {
		pr_err("%s: error debugfs_create_file failed\n", __func__);
		ret = -ENOMEM;
		goto rbcpr_probe_fail;
	}

	platform_set_drvdata(pdev, dent);
rbcpr_probe_fail:
	iounmap(start_ptr);
	return ret;
}

static int __devexit msm_rpmrbcpr_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id rpmrbcpr_stats_table[] = {
	{.compatible = "qcom,rpmrbcpr-stats"},
	{},
};

static struct platform_driver msm_rpmrbcpr_driver = {
	.probe  = msm_rpmrbcpr_probe,
	.remove = __devexit_p(msm_rpmrbcpr_remove),
	.driver = {
		.name = "msm_rpmrbcpr_stats",
		.owner = THIS_MODULE,
		.of_match_table = rpmrbcpr_stats_table,
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
