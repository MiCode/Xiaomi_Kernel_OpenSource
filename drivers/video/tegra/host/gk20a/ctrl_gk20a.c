/*
 * GK20A Ctrl
 *
 * Copyright (c) 2011-2014, NVIDIA Corporation.  All rights reserved.
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

#include <linux/highmem.h>
#include <linux/cdev.h>
#include <linux/nvhost_gpu_ioctl.h>

#include "dev.h"
#include "class_ids.h"
#include "bus_client.h"
#include "nvhost_acm.h"

#include "gk20a.h"

int gk20a_ctrl_dev_open(struct inode *inode, struct file *filp)
{
	struct nvhost_device_data *pdata;
	struct platform_device *dev;
	struct nvhost_channel *ch;

	nvhost_dbg_fn("");

	pdata = container_of(inode->i_cdev,
			     struct nvhost_device_data, ctrl_cdev);
	dev = pdata->pdev;

	BUG_ON(dev == NULL);

	filp->private_data = dev;

	ch = nvhost_getchannel(pdata->channel, false);
	if (!ch) {
		nvhost_dbg_fn("fail to get channel!");
		return -ENOMEM;
	}

	return 0;
}

int gk20a_ctrl_dev_release(struct inode *inode, struct file *filp)
{
	struct platform_device *dev = filp->private_data;
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);

	nvhost_dbg_fn("");

	nvhost_putchannel(pdata->channel);
	return 0;
}

static long
gk20a_ctrl_ioctl_gpu_characteristics(
	struct gk20a *g,
	struct nvhost_gpu_get_characteristics *request)
{
	struct nvhost_gpu_characteristics *pgpu = &g->gpu_characteristics;
	long err = 0;

	if (request->gpu_characteristics_buf_size > 0) {
		size_t write_size = sizeof(*pgpu);

		if (write_size > request->gpu_characteristics_buf_size)
			write_size = request->gpu_characteristics_buf_size;

		err = copy_to_user((void __user *)(uintptr_t)
				   request->gpu_characteristics_buf_addr,
				   pgpu, write_size);
	}

	if (err == 0)
		request->gpu_characteristics_buf_size = sizeof(*pgpu);

	return err;
}

long gk20a_ctrl_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct platform_device *dev = filp->private_data;
	struct gk20a *g = get_gk20a(dev);
	struct nvhost_gpu_zcull_get_ctx_size_args *get_ctx_size_args;
	struct nvhost_gpu_zcull_get_info_args *get_info_args;
	struct nvhost_gpu_zbc_set_table_args *set_table_args;
	struct nvhost_gpu_zbc_query_table_args *query_table_args;
	u8 buf[NVHOST_GPU_IOCTL_MAX_ARG_SIZE];
	struct gr_zcull_info *zcull_info;
	struct zbc_entry *zbc_val;
	struct zbc_query_params *zbc_tbl;
	int i, err = 0;

	nvhost_dbg_fn("");

	if ((_IOC_TYPE(cmd) != NVHOST_GPU_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVHOST_GPU_IOCTL_LAST))
		return -EFAULT;

	BUG_ON(_IOC_SIZE(cmd) > NVHOST_GPU_IOCTL_MAX_ARG_SIZE);

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	if (!g->gr.sw_ready) {
		gk20a_busy(g->dev);
		gk20a_idle(g->dev);
	}

	switch (cmd) {
	case NVHOST_GPU_IOCTL_ZCULL_GET_CTX_SIZE:
		get_ctx_size_args = (struct nvhost_gpu_zcull_get_ctx_size_args *)buf;

		get_ctx_size_args->size = gr_gk20a_get_ctxsw_zcull_size(g, &g->gr);

		break;
	case NVHOST_GPU_IOCTL_ZCULL_GET_INFO:
		get_info_args = (struct nvhost_gpu_zcull_get_info_args *)buf;

		memset(get_info_args, 0, sizeof(struct nvhost_gpu_zcull_get_info_args));

		zcull_info = kzalloc(sizeof(struct gr_zcull_info), GFP_KERNEL);
		if (zcull_info == NULL)
			return -ENOMEM;

		err = gr_gk20a_get_zcull_info(g, &g->gr, zcull_info);
		if (err)
			break;

		get_info_args->width_align_pixels = zcull_info->width_align_pixels;
		get_info_args->height_align_pixels = zcull_info->height_align_pixels;
		get_info_args->pixel_squares_by_aliquots = zcull_info->pixel_squares_by_aliquots;
		get_info_args->aliquot_total = zcull_info->aliquot_total;
		get_info_args->region_byte_multiplier = zcull_info->region_byte_multiplier;
		get_info_args->region_header_size = zcull_info->region_header_size;
		get_info_args->subregion_header_size = zcull_info->subregion_header_size;
		get_info_args->subregion_width_align_pixels = zcull_info->subregion_width_align_pixels;
		get_info_args->subregion_height_align_pixels = zcull_info->subregion_height_align_pixels;
		get_info_args->subregion_count = zcull_info->subregion_count;

		if (zcull_info)
			kfree(zcull_info);
		break;
	case NVHOST_GPU_IOCTL_ZBC_SET_TABLE:
		set_table_args = (struct nvhost_gpu_zbc_set_table_args *)buf;

		zbc_val = kzalloc(sizeof(struct zbc_entry), GFP_KERNEL);
		if (zbc_val == NULL)
			return -ENOMEM;

		zbc_val->format = set_table_args->format;
		zbc_val->type = set_table_args->type;

		switch (zbc_val->type) {
		case GK20A_ZBC_TYPE_COLOR:
			for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
				zbc_val->color_ds[i] = set_table_args->color_ds[i];
				zbc_val->color_l2[i] = set_table_args->color_l2[i];
			}
			break;
		case GK20A_ZBC_TYPE_DEPTH:
			zbc_val->depth = set_table_args->depth;
			break;
		default:
			err = -EINVAL;
		}

		if (!err) {
			gk20a_busy(dev);
			err = gk20a_gr_zbc_set_table(g, &g->gr, zbc_val);
			gk20a_idle(dev);
		}

		if (zbc_val)
			kfree(zbc_val);
		break;
	case NVHOST_GPU_IOCTL_ZBC_QUERY_TABLE:
		query_table_args = (struct nvhost_gpu_zbc_query_table_args *)buf;

		zbc_tbl = kzalloc(sizeof(struct zbc_query_params), GFP_KERNEL);
		if (zbc_tbl == NULL)
			return -ENOMEM;

		zbc_tbl->type = query_table_args->type;
		zbc_tbl->index_size = query_table_args->index_size;

		err = gr_gk20a_query_zbc(g, &g->gr, zbc_tbl);

		if (!err) {
			switch (zbc_tbl->type) {
			case GK20A_ZBC_TYPE_COLOR:
				for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
					query_table_args->color_ds[i] = zbc_tbl->color_ds[i];
					query_table_args->color_l2[i] = zbc_tbl->color_l2[i];
				}
				break;
			case GK20A_ZBC_TYPE_DEPTH:
				query_table_args->depth = zbc_tbl->depth;
				break;
			case GK20A_ZBC_TYPE_INVALID:
				query_table_args->index_size = zbc_tbl->index_size;
				break;
			default:
				err = -EINVAL;
			}
			if (!err) {
				query_table_args->format = zbc_tbl->format;
				query_table_args->ref_cnt = zbc_tbl->ref_cnt;
			}
		}

		if (zbc_tbl)
			kfree(zbc_tbl);
		break;

	case NVHOST_GPU_IOCTL_GET_CHARACTERISTICS:
		err = gk20a_ctrl_ioctl_gpu_characteristics(
			g, (struct nvhost_gpu_get_characteristics *)buf);
		break;

	default:
		nvhost_err(dev_from_gk20a(g), "unrecognized gpu ioctl cmd: 0x%x", cmd);
		err = -ENOTTY;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	return err;
}

