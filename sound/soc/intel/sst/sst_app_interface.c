/*
 *  sst_app_interface.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corp
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *  Harsha Priya <priya.harsha@intel.com>
 *  Dharageswari R <dharageswari.r@intel.com>
 *  Jeeja KP <jeeja.kp@intel.com>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This driver exposes the audio engine functionalities to the ALSA
 *	and middleware.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/aio.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/ioctl.h>
#include <sound/intel_sst_ioctl.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

#define AM_MODULE 1

/**
 * intel_sst_open_cntrl - opens a handle to driver
 *
 * @i_node:	inode structure
 * @file_ptr:pointer to file
 *
 * This function is called by OS when a user space component
 * tries to get a driver handle to /dev/intel_sst_control.
 * Only one handle at a time will be allowed
 * This is for control operations only
 */
int intel_sst_open_cntrl(struct inode *i_node, struct file *file_ptr)
{
	unsigned int retval;

	/* audio manager open */
	mutex_lock(&sst_drv_ctx->stream_lock);
	retval = intel_sst_check_device();
	if (retval) {
		mutex_unlock(&sst_drv_ctx->stream_lock);
		return retval;
	}
	pr_debug("AM handle opened\n");

	mutex_unlock(&sst_drv_ctx->stream_lock);
	return retval;
}


int intel_sst_release_cntrl(struct inode *i_node, struct file *file_ptr)
{
	/* audio manager close */
	mutex_lock(&sst_drv_ctx->stream_lock);
	sst_pm_runtime_put(sst_drv_ctx);
	mutex_unlock(&sst_drv_ctx->stream_lock);
	pr_debug("AM handle closed\n");
	return 0;
}

/**
 * sst_get_max_streams - Function to populate the drv info structure
 *				with the max streams
 * @info: the out params that holds the drv info
 *
 * This function is called when max streams count is required
**/
void sst_get_max_streams(struct snd_sst_driver_info *info)
{
	pr_debug("info.max_streams %d num_probes %d\n", sst_drv_ctx->info.max_streams,
					sst_drv_ctx->info.num_probes);
	info->max_streams = sst_drv_ctx->info.max_streams - sst_drv_ctx->info.num_probes;
}

/**
 * sst_create_algo_ipc - create ipc msg for algorithm parameters
 *
 * @algo_params: Algorithm parameters
 * @msg: post msg pointer
 * @pvt_id: Checked by wake_up_block
 *
 * This function is called to create ipc msg
 * For copying the mailbox data the function returns offset in bytes to mailbox
 * memory where the mailbox data should be copied after msg header
 */
static int sst_create_algo_ipc(struct snd_ppp_params *algo_params,
					struct ipc_post **msg, int pvt_id)
{
	u32 header_size = 0;
	u32 ipc_msg_size = sizeof(u32) + sizeof(*algo_params)
			 - sizeof(algo_params->params) + algo_params->size;
	u32 offset = 0;

	if (ipc_msg_size > sst_drv_ctx->mailbox_size)
		return -ENOMEM;
	if (sst_create_ipc_msg(msg, true))
		return -ENOMEM;
	sst_fill_header(&(*msg)->header,
			IPC_IA_ALG_PARAMS, 1, pvt_id);
	(*msg)->header.part.data = ipc_msg_size;
	memcpy((*msg)->mailbox_data, &(*msg)->header, sizeof(u32));
	offset = sizeof(u32);
	header_size = sizeof(*algo_params) - sizeof(algo_params->params);
	memcpy((*msg)->mailbox_data + offset, algo_params, header_size);
	offset += header_size;
	return offset;
}

static long sst_send_algo(struct snd_ppp_params *algo_params,
		struct sst_block *block, enum sst_algo_ops algo)
{
	struct ipc_post *msg;
	int retval;
	int offset;

	pr_debug("Algo ID %d Str id %d Enable %d Size %d\n",
		algo_params->algo_id, algo_params->str_id,
		algo_params->enable, algo_params->size);

	algo_params->operation = algo;

	offset = sst_create_algo_ipc(algo_params, &msg, block->drv_id);
	if (offset < 0)
		return offset;

	if (copy_from_user(msg->mailbox_data + offset,
			algo_params->params, algo_params->size)) {
		kfree(msg);
		return -EFAULT;
	}

	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	retval = sst_wait_timeout(sst_drv_ctx, block);
	if (retval) {
		pr_debug("%s: failed for algo ops %s with retval %d\n",
				__func__, algo ? "SST_GET_ALGO" : "SST_SET_ALGO", retval);
		return -EIO;
	}
	return 0;
}

/**
 * intel_sst_ioctl_dsp - receives the device ioctl's
 *
 * @cmd:Ioctl cmd
 * @arg:data
 *
 * This function is called when a user space component
 * sends a DSP Ioctl to SST driver
 */
long intel_sst_ioctl_dsp(unsigned int cmd,
		struct snd_ppp_params *algo_params, unsigned long arg)
{
	int retval = 0;
	struct snd_ppp_params *algo_params_copied;
	struct sst_block *block;
	int pvt_id;

	pvt_id = sst_assign_pvt_id(sst_drv_ctx);
	block = sst_create_block(sst_drv_ctx, IPC_IA_ALG_PARAMS, pvt_id);
	if (block == NULL)
		return -ENOMEM;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SNDRV_SST_SET_ALGO):
		retval = sst_send_algo(algo_params, block, SST_SET_ALGO);
		break;

	case _IOC_NR(SNDRV_SST_GET_ALGO):
		retval = sst_send_algo(algo_params, block, SST_GET_ALGO);
		if (retval)
			break;
		algo_params_copied = (struct snd_ppp_params *)block->data;

		if (algo_params_copied->size > algo_params->size) {
			pr_debug("mem insufficient to copy\n");
			retval = -EMSGSIZE;
			break;
		} else {
			char __user *tmp;
			struct snd_ppp_params *get_params;
			char *pp;

			tmp = (char __user *)arg + offsetof(
					struct snd_ppp_params, size);
			if (copy_to_user(tmp, &algo_params_copied->size,
						 sizeof(u32))) {
				retval = -EFAULT;
				break;
			}
			tmp = (char __user *)arg + offsetof(
					struct snd_ppp_params, enable);
			if (copy_to_user(tmp, &algo_params_copied->enable,
						 sizeof(u8))) {
				retval = -EFAULT;
				break;
			}
			if (algo_params_copied->size == 0)
				break;

			get_params = kmalloc(sizeof(*get_params), GFP_KERNEL);
			if (!get_params) {
				pr_err("sst: mem alloc failed\n");
				break;
			}
			memcpy(get_params, algo_params_copied,
							sizeof(*get_params));

			get_params->params = kmalloc(get_params->size, GFP_KERNEL);
			if (!get_params->params) {
				pr_err("sst: mem alloc failed\n");
				goto free_mem;
			}
			pp = (char *)algo_params_copied;
			pp = pp + sizeof(*get_params) -
						sizeof(get_params->params);
			memcpy(get_params->params, pp, get_params->size);
			if (copy_to_user(algo_params->params,
					get_params->params,
					get_params->size)) {
				retval = -EFAULT;
			}
			kfree(get_params->params);

free_mem:
			kfree(get_params);

		}
		break;
	}
	sst_free_block(sst_drv_ctx, block);
	pr_debug("ioctl dsp return = %d, for cmd = %x\n", retval, cmd);
	return retval;
}

static long sst_ioctl_tuning_params(unsigned int cmd, unsigned long arg)
{
	struct snd_sst_tuning_params params;
	struct ipc_post *msg;
	unsigned long address;

	if (copy_from_user(&params, (void __user *)arg, sizeof(params)))
		return -EFAULT;
	pr_debug("sst: Parameter %d, Stream %d, Size %d\n", params.type,
			params.str_id, params.size);
	if (sst_create_ipc_msg(&msg, true))
		return -ENOMEM;
	address = (unsigned long)params.addr;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SNDRV_SST_TUNING_PARAMS):
		sst_fill_header(&msg->header, IPC_IA_TUNING_PARAMS, 1,
				params.str_id);
		break;
	}
	msg->header.part.data = sizeof(u32) + sizeof(params) + params.size;
	memcpy(msg->mailbox_data, &msg->header.full, sizeof(u32));
	memcpy(msg->mailbox_data + sizeof(u32), &params, sizeof(params));
	/* driver doesn't need to send address, so overwrite addr with data */
	if (copy_from_user(msg->mailbox_data + sizeof(u32)
				+ sizeof(params) - sizeof(params.addr),
			(void __user *)address, params.size)) {
		kfree(msg->mailbox_data);
		kfree(msg);
		return -EFAULT;
	}

	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	return 0;
}
/**
 * intel_sst_ioctl - receives the device ioctl's
 * @file_ptr:pointer to file
 * @cmd:Ioctl cmd
 * @arg:data
 *
 * This function is called by OS when a user space component
 * sends an Ioctl to SST driver
 */
long intel_sst_ioctl(struct file *file_ptr, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct snd_ppp_params algo_params;

	if (sst_drv_ctx->sst_state != SST_FW_RUNNING)
		return -EBUSY;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SNDRV_SST_DRIVER_INFO): {
		struct snd_sst_driver_info info;

		pr_debug("SNDRV_SST_DRIVER_INFO received\n");
		sst_get_max_streams(&info);

		if (copy_to_user((void __user *)arg, &info,
				sizeof(info)))
			retval = -EFAULT;
		break;
	}
	case _IOC_NR(SNDRV_SST_GET_ALGO):
	case _IOC_NR(SNDRV_SST_SET_ALGO):
		if (copy_from_user(&algo_params, (void __user *)arg,
						sizeof(algo_params))) {
			return -EFAULT;
		}
		retval = intel_sst_ioctl_dsp(cmd, &algo_params, arg);
		break;

	case _IOC_NR(SNDRV_SST_TUNING_PARAMS):
		retval = sst_ioctl_tuning_params(cmd, arg);
		break;

	default:
		retval = -EINVAL;
	}
	pr_debug("intel_sst_ioctl:complete ret code = %d for command = %x\n", retval, cmd);
	return retval;
}

