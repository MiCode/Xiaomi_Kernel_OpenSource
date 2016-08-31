/*
 * nvc_debugfs.c - nvc camera debugfs
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <media/nvc_debugfs.h>
#include <linux/module.h>

static int i2ca_get(void *data, u64 *val)
{
	struct nvc_debugfs_info *info = (struct nvc_debugfs_info *)(data);
	*val = (u64)info->i2c_reg;
	return 0;
}

static int i2ca_set(void *data, u64 val)
{
	struct nvc_debugfs_info *info = (struct nvc_debugfs_info *)(data);

	if (val > info->i2c_addr_limit) {
		dev_err(&info->i2c_client->dev, "ERR:%s out of range\n",
				__func__);
		return -EIO;
	}

	info->i2c_reg = (u16) val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2ca_fops, i2ca_get, i2ca_set, "0x%02llx\n");

static int i2cr_get(void *data, u64 *val)
{
	u8 temp = 0;
	struct nvc_debugfs_info *info = (struct nvc_debugfs_info *)(data);

	if (info->i2c_rd8(info->i2c_client, info->i2c_reg, &temp)) {
		dev_err(&info->i2c_client->dev, "ERR:%s failed\n", __func__);
		return -EIO;
	}
	*val = (u64)temp;
	return 0;
}

static int i2cr_set(void *data, u64 val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2cr_fops, i2cr_get, i2cr_set, "0x%02llx\n");

static int i2cw_get(void *data, u64 *val)
{
	return 0;
}

static int i2cw_set(void *data, u64 val)
{
	struct nvc_debugfs_info *info = (struct nvc_debugfs_info *)(data);

	val &= 0xFF;
	if (info->i2c_wr8(info->i2c_client, info->i2c_reg, val)) {
		dev_err(&info->i2c_client->dev, "ERR:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2cw_fops, i2cw_get, i2cw_set, "0x%02llx\n");

int nvc_debugfs_init(struct nvc_debugfs_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s", __func__);

	info->i2c_reg = 0;
	info->debugfs_root = debugfs_create_dir(info->name, NULL);

	if (!info->debugfs_root)
		goto err_out;

	if (!debugfs_create_file("i2ca", S_IRUGO | S_IWUSR,
				info->debugfs_root, info, &i2ca_fops))
		goto err_out;

	if (!debugfs_create_file("i2cr", S_IRUGO,
				info->debugfs_root, info, &i2cr_fops))
		goto err_out;

	if (!debugfs_create_file("i2cw", S_IWUSR,
				info->debugfs_root, info, &i2cw_fops))
		goto err_out;

	return 0;

err_out:
	dev_err(&info->i2c_client->dev, "ERROR:%s failed", __func__);
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
	return -ENOMEM;
}


int nvc_debugfs_remove(struct nvc_debugfs_info *info)
{
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
	return 0;
}

