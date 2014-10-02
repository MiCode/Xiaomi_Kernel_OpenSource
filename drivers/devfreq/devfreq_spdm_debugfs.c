/*
*Copyright (c) 2014, The Linux Foundation. All rights reserved.
*
*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License version 2 and
*only version 2 as published by the Free Software Foundation.
*
*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.
*/
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <soc/qcom/hvc.h>
#include "devfreq_spdm.h"

static int spdm_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t pl_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;
	int i;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;

	sscanf(buf, "%u %u\n", &spdm_data->config_data.pl_freqs[0],
		&spdm_data->config_data.pl_freqs[1]);


	desc.arg[0] = SPDM_CMD_CFG_PL;
	desc.arg[1] = spdm_data->spdm_client;
	for (i = 0; i < SPDM_PL_COUNT - 1; i++)
		desc.arg[i+2] = spdm_data->config_data.pl_freqs[i];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations pl_fops = {
	.open = spdm_open,
	.write = pl_write,
};

static ssize_t rejrate_low_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;

	sscanf(buf, "%u %u\n", &spdm_data->config_data.reject_rate[0],
		&spdm_data->config_data.reject_rate[1]);

	desc.arg[0] = SPDM_CMD_CFG_REJRATE_LOW;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.reject_rate[0];
	desc.arg[3] = spdm_data->config_data.reject_rate[1];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations rrl_fops = {
	.open = spdm_open,
	.write = rejrate_low_write,
};

static ssize_t rejrate_med_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.reject_rate[2],
		&spdm_data->config_data.reject_rate[3]);

	desc.arg[0] = SPDM_CMD_CFG_REJRATE_MED;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.reject_rate[2];
	desc.arg[3] = spdm_data->config_data.reject_rate[3];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations rrm_fops = {
	.open = spdm_open,
	.write = rejrate_med_write,
};

static ssize_t rejrate_high_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.reject_rate[4],
		&spdm_data->config_data.reject_rate[5]);

	desc.arg[0] = SPDM_CMD_CFG_REJRATE_HIGH;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.reject_rate[4];
	desc.arg[3] = spdm_data->config_data.reject_rate[5];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations rrh_fops = {
	.open = spdm_open,
	.write = rejrate_high_write,
};

static ssize_t resptime_low_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.response_time_us[0],
		&spdm_data->config_data.response_time_us[1]);

	desc.arg[0] = SPDM_CMD_CFG_RESPTIME_LOW;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.response_time_us[0];
	desc.arg[3] = spdm_data->config_data.response_time_us[1];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations rtl_fops = {
	.open = spdm_open,
	.write = resptime_low_write,
};

static ssize_t resptime_med_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.response_time_us[2],
		&spdm_data->config_data.response_time_us[3]);

	desc.arg[0] = SPDM_CMD_CFG_RESPTIME_MED;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.response_time_us[2];
	desc.arg[3] = spdm_data->config_data.response_time_us[3];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations rtm_fops = {
	.open = spdm_open,
	.write = resptime_med_write,
};

static ssize_t resptime_high_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.response_time_us[4],
		&spdm_data->config_data.response_time_us[5]);

	desc.arg[0] = SPDM_CMD_CFG_RESPTIME_HIGH;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.response_time_us[4];
	desc.arg[3] = spdm_data->config_data.response_time_us[5];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations rth_fops = {
	.open = spdm_open,
	.write = resptime_high_write,
};

static ssize_t cciresptime_low_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.cci_response_time_us[0],
		&spdm_data->config_data.cci_response_time_us[1]);

	desc.arg[0] = SPDM_CMD_CFG_CCIRESPTIME_LOW;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.cci_response_time_us[0];
	desc.arg[3] = spdm_data->config_data.cci_response_time_us[1];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations ccil_fops = {
	.open = spdm_open,
	.write = cciresptime_low_write,
};

static ssize_t cciresptime_med_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.cci_response_time_us[2],
		&spdm_data->config_data.cci_response_time_us[3]);

	desc.arg[0] = SPDM_CMD_CFG_CCIRESPTIME_MED;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.cci_response_time_us[2];
	desc.arg[3] = spdm_data->config_data.cci_response_time_us[3];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations ccim_fops = {
	.open = spdm_open,
	.write = cciresptime_med_write,
};

static ssize_t cciresptime_high_write(struct file *file,
				      const char __user *data,
				      size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u\n", &spdm_data->config_data.cci_response_time_us[4],
		&spdm_data->config_data.cci_response_time_us[5]);

	desc.arg[0] = SPDM_CMD_CFG_CCIRESPTIME_HIGH;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.cci_response_time_us[4];
	desc.arg[3] = spdm_data->config_data.cci_response_time_us[5];
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations ccih_fops = {
	.open = spdm_open,
	.write = cciresptime_high_write,
};

static ssize_t cci_max_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u\n", &spdm_data->config_data.max_cci_freq);

	desc.arg[0] = SPDM_CMD_CFG_MAXCCI;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.max_cci_freq;
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations ccimax_fops = {
	.open = spdm_open,
	.write = cci_max_write,
};

static ssize_t vote_cfg_write(struct file *file, const char __user *data,
		 size_t size, loff_t *offset)
{
	struct spdm_data *spdm_data = file->private_data;
	char *buf;
	struct hvc_desc desc;

	buf = kzalloc(size, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;
	if (copy_from_user(buf, data, size))
		return -EINVAL;
	sscanf(buf, "%u %u %u %u\n", &spdm_data->config_data.upstep,
		&spdm_data->config_data.downstep,
		&spdm_data->config_data.max_vote,
		&spdm_data->config_data.up_step_multp);

	desc.arg[0] = SPDM_CMD_CFG_VOTES;
	desc.arg[1] = spdm_data->spdm_client;
	desc.arg[2] = spdm_data->config_data.upstep;
	desc.arg[3] = spdm_data->config_data.downstep;
	desc.arg[4] = spdm_data->config_data.max_vote;
	desc.arg[5] = spdm_data->config_data.up_step_multp;
	if (hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc))
		pr_debug("check hvc logs");
	*offset += size;
	kfree(buf);
	return size;
}

static const struct file_operations vote_fops = {
	.open = spdm_open,
	.write = vote_cfg_write,
};

void spdm_init_debugfs(struct device *dev)
{
	struct spdm_data *data = 0;

	data = dev_get_drvdata(dev);
	data->debugfs_dir = debugfs_create_dir(dev_name(dev), NULL);

	debugfs_create_file("pl_freqs", 0x700, data->debugfs_dir, data,
			    &pl_fops);
	debugfs_create_file("rej_rate_low", 0x700, data->debugfs_dir, data,
			    &rrl_fops);
	debugfs_create_file("rej_rate_med", 0x700, data->debugfs_dir, data,
			    &rrm_fops);
	debugfs_create_file("rej_rate_high", 0x700, data->debugfs_dir, data,
			    &rrh_fops);
	debugfs_create_file("resp_time_low", 0x700, data->debugfs_dir, data,
			    &rtl_fops);
	debugfs_create_file("resp_time_med", 0x700, data->debugfs_dir, data,
			    &rtm_fops);
	debugfs_create_file("resp_time_high", 0x700, data->debugfs_dir, data,
			    &rth_fops);
	debugfs_create_file("cci_resp_time_low", 0x700, data->debugfs_dir, data,
			    &ccil_fops);
	debugfs_create_file("cci_resp_time_med", 0x700, data->debugfs_dir, data,
			    &ccim_fops);
	debugfs_create_file("cci_resp_time_high", 0x700, data->debugfs_dir,
			data, &ccih_fops);
	debugfs_create_file("cci_max", 0x700, data->debugfs_dir, data,
			&ccimax_fops);
	debugfs_create_file("vote_cfg", 0x700, data->debugfs_dir, data,
			&vote_fops);
}

void spdm_remove_debugfs(struct spdm_data *data)
{
	debugfs_remove_recursive(data->debugfs_dir);
}

MODULE_LICENSE("GPL v2");
