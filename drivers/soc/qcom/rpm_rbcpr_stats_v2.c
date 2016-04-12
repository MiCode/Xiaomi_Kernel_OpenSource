/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>

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
	CORNER_RETENTION,
	CORNER_SVS_KRAIT,
	CORNER_SVS_SOC,
	CORNER_NOMINAL,
	CORNER_TURBO,
	CORNER_SUPER_TURBO,
	CORNER_MAX,
};

struct rbcpr_recmnd_data_type {
	uint32_t microvolts;
	uint64_t timestamp;
};

struct rbcpr_corners_data_type {
	int32_t efuse_adjustment;
	uint32_t programmed_voltage;
	uint32_t isr_count;
	uint32_t min_count;
	uint32_t max_count;
	struct rbcpr_recmnd_data_type rbcpr_recmnd[RBCPR_NUM_RECMNDS];
};

struct rbcpr_rail_stats_header_type {
	uint32_t num_corners;
	uint32_t num_latest_recommends;
};

struct rbcpr_rail_stats_footer_type {
	uint32_t current_corner;
	uint32_t railway_voltage;
	uint32_t off_corner;
	uint32_t margin;
};

struct rbcpr_stats_type {
	uint32_t num_rails;
	uint32_t status;
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
	[CORNER_RETENTION] = "RETENTION",
	[CORNER_SVS_KRAIT] = "SVS",
	[CORNER_SVS_SOC] = "SVS_SOC",
	[CORNER_NOMINAL] = "NOMINAL",
	[CORNER_TURBO] = "TURBO",
	[CORNER_SUPER_TURBO] = "SUPER_TURBO",
};

#define CORNER_STRING(a)	\
	((a >= CORNER_MAX) ? "INVALID Corner" : rbcpr_corner_string[a])

static struct rbcpr_data_type *rbcpr_data;

static void msm_rpmrbcpr_print_stats_header(
		struct rbcpr_stats_type *rbcpr_stats, char *buf,
						uint32_t *pos)
{
	*pos += PRINT(buf, *pos, "\n:RBCPR STATS  ");
	*pos += PRINT(buf, *pos, "(%s: %d)", FIELD(rbcpr_stats->num_rails),
				rbcpr_stats->num_rails);
	*pos += PRINT(buf, *pos, "(%s: %d)", FIELD(rbcpr_stats->status),
				rbcpr_stats->status);
}

static void msm_rpmrbcpr_print_rail_header(
		struct rbcpr_rail_stats_header_type *rail_header, char *buf,
							uint32_t *pos)
{
	*pos += PRINT(buf, *pos, "(%s: %d)", FIELD(rail_header->num_corners),
				rail_header->num_corners);
	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(rail_header->num_latest_recommends),
			rail_header->num_latest_recommends);
}

static void msm_rpmrbcpr_print_corner_recmnds(
		struct rbcpr_recmnd_data_type *rbcpr_recmnd, char *buf,
							uint32_t *pos)
{
	*pos += PRINT(buf, *pos, "\n\t\t\t :(%s: %d) ",
						FIELD(rbcpr_recmd->microvolts),
						rbcpr_recmnd->microvolts);
	*pos += PRINT(buf, *pos, " (%s: %lld)", FIELD(rbcpr_recmd->timestamp),
						rbcpr_recmnd->timestamp);
}

static void msm_rpmrbcpr_print_corner_data(
		struct rbcpr_corners_data_type *corner, char *buf,
			uint32_t num_corners, uint32_t *pos)
{
	int i;

	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(corner->efuse_adjustment),
					corner->efuse_adjustment);
	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(corner->programmed_voltage),
					corner->programmed_voltage);
	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(corner->isr_count), corner->isr_count);
	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(corner->min_count), corner->min_count);
	*pos += PRINT(buf, *pos, "(%s: %d)\n",
			FIELD(corner->max_count), corner->max_count);
	*pos += PRINT(buf, *pos, "\t\t\t:Latest Recommends");
	for (i = 0; i < num_corners; i++)
		msm_rpmrbcpr_print_corner_recmnds(&corner->rbcpr_recmnd[i], buf,
						pos);
}

static void msm_rpmrbcpr_print_rail_footer(
		struct rbcpr_rail_stats_footer_type *rail, char *buf,
							uint32_t *pos)
{
	*pos += PRINT(buf, *pos, "(%s: %s)", FIELD(rail->current_corner),
			CORNER_STRING(rail->current_corner));
	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(rail->railway_voltage), rail->railway_voltage);
	*pos += PRINT(buf, *pos, "(%s: %d)",
			FIELD(rail->off_corner), rail->off_corner);
	*pos += PRINT(buf, *pos, "(%s: %d)\n",
			FIELD(rail->margin), rail->margin);
}

static uint32_t msm_rpmrbcpr_read_rpm_data(void)
{
	uint32_t read_offset = 0;
	static struct rbcpr_stats_type rbcpr_stats_header;
	uint32_t buffer_offset = 0;
	char *buf = rbcpr_data->buf;
	int i, j;

	memcpy_fromio(&rbcpr_stats_header, rbcpr_data->start,
					sizeof(rbcpr_stats_header));
	read_offset += sizeof(rbcpr_stats_header);
	msm_rpmrbcpr_print_stats_header(&rbcpr_stats_header, buf,
							&buffer_offset);

	for (i = 0; i < rbcpr_stats_header.num_rails; i++) {
		static struct rbcpr_rail_stats_header_type rail_header;
		static struct rbcpr_rail_stats_footer_type rail_footer;

		memcpy_fromio(&rail_header, (rbcpr_data->start + read_offset),
					sizeof(rail_header));
		read_offset += sizeof(rail_header);
		buffer_offset += PRINT(buf, buffer_offset, "\n:%s Rail Data ",
							rbcpr_rail_labels[i]);
		msm_rpmrbcpr_print_rail_header(&rail_header, buf,
							&buffer_offset);

		for (j = 0; j < rail_header.num_corners; j++) {
			static struct rbcpr_corners_data_type corner;
			uint32_t corner_index;

			memcpy_fromio(&corner,
					(rbcpr_data->start + read_offset),
					sizeof(corner));
			read_offset += sizeof(corner);

			/*
			 * RPM doesn't include corner type in the data for the
			 * corner. For now add this hack to know which corners
			 * are used based on number of corners for the rail.
			 */
			corner_index = j + 3;
			if (rail_header.num_corners == 3 && j == 2)
				corner_index++;

			buffer_offset += PRINT(buf, buffer_offset,
				"\n\t\t:Corner Data: %s ",
					CORNER_STRING(corner_index));
			msm_rpmrbcpr_print_corner_data(&corner, buf,
				rail_header.num_latest_recommends,
				&buffer_offset);
		}
		buffer_offset += PRINT(buf, buffer_offset,
				"\n\t\t");
		memcpy_fromio(&rail_footer, (rbcpr_data->start + read_offset),
					sizeof(rail_footer));
		read_offset += sizeof(rail_footer);
		msm_rpmrbcpr_print_rail_footer(&rail_footer, buf,
							&buffer_offset);
	}
	return buffer_offset;
}

static int msm_rpmrbcpr_file_read(struct seq_file *m, void *data)
{
	struct rbcpr_data_type *pdata = m->private;
	int ret = 0;
	int curr_status_counter;
	static int prev_status_counter;
	static DEFINE_MUTEX(rbcpr_lock);

	mutex_lock(&rbcpr_lock);
	if (!pdata) {
		pr_err("%s pdata is null", __func__);
		ret = -EINVAL;
		goto exit_rpmrbcpr_file_read;
	}

	/* Read RPM stats */
	curr_status_counter = readl_relaxed(pdata->start +
		offsetof(struct rbcpr_stats_type, status));
	if (curr_status_counter != prev_status_counter) {
		pdata->len = msm_rpmrbcpr_read_rpm_data();
		pdata->len = 0;
		prev_status_counter = curr_status_counter;
	}

	seq_printf(m, "%s", pdata->buf);

exit_rpmrbcpr_file_read:
	mutex_unlock(&rbcpr_lock);
	return ret;
}

static int msm_rpmrbcpr_file_open(struct inode *inode, struct file *file)
{
	if (!rbcpr_data->start)
		return -ENODEV;
	return single_open(file, msm_rpmrbcpr_file_read, inode->i_private);
}

static const struct file_operations msm_rpmrbcpr_fops = {
	.open		= msm_rpmrbcpr_file_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int msm_rpmrbcpr_validate(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t num_rails;

	num_rails = readl_relaxed(rbcpr_data->start);

	if (num_rails > RBCPR_MAX_RAILS) {
		pr_err("%s: Invalid number of RPM RBCPR rails %d",
				__func__, num_rails);
		ret = -EFAULT;
	}

	return ret;
}

static  int msm_rpmrbcpr_probe(struct platform_device *pdev)
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
	res->end = rbcpr_start_addr + RBCPR_STATS_MAX_SIZE;

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

	ret = msm_rpmrbcpr_validate(pdev);

	if (ret)
		goto rbcpr_probe_fail;

	dent = debugfs_create_file("rpm_rbcpr", S_IRUGO, NULL,
			rbcpr_data, &msm_rpmrbcpr_fops);

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

static int msm_rpmrbcpr_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id rpmrbcpr_stats_table[] = {
	{.compatible = "qcom,rpmrbcpr-stats"},
	{},
};

static struct platform_driver msm_rpmrbcpr_driver = {
	.probe  = msm_rpmrbcpr_probe,
	.remove = msm_rpmrbcpr_remove,
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
