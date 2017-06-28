/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "PROFILER: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/types.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/socinfo.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <soc/qcom/profiler.h>

#include <linux/compat.h>

#define PROFILER_DEV			"profiler"

static struct class *driver_class;
static dev_t profiler_device_no;
static int stop_done = 1;

struct profiler_control {

	struct device *pdev;
	struct cdev cdev;
};

static struct profiler_control profiler;

struct profiler_dev_handle {

	bool released;
	int abort;
	atomic_t ioctl_count;
};


static int profiler_scm_call2(uint32_t svc_id, uint32_t tz_cmd_id,
			const void *req_buf, void *resp_buf)
{
	int      ret = 0;
	uint32_t qseos_cmd_id = 0;
	struct scm_desc desc = {0};

	if (!req_buf || !resp_buf) {
		pr_err("Invalid buffer pointer\n");
		return -EINVAL;
	}
	qseos_cmd_id = *(uint32_t *)req_buf;

	switch (svc_id) {

	case SCM_SVC_BW: {
		switch (qseos_cmd_id) {
		case TZ_BW_SVC_START_ID:
		case TZ_BW_SVC_GET_ID:
		case TZ_BW_SVC_STOP_ID: {
			/* Send the command to TZ */
			desc.arginfo = SCM_ARGS(4, SCM_RW, SCM_VAL,
							SCM_RW, SCM_VAL);
			desc.args[0] = virt_to_phys(&
						(((struct tz_bw_svc_buf *)
						req_buf)->bwreq));
			desc.args[1] = ((struct tz_bw_svc_buf *)
						req_buf)->req_size;
			desc.args[2] = virt_to_phys(&
						((struct tz_bw_svc_buf *)
						req_buf)->bwresp);
			desc.args[3] = sizeof(struct tz_bw_svc_resp);

			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO,
					TZ_SVC_BW_PROF_ID), &desc);
		break;
		}
		default: {
			pr_err("cmd_id 0x%d is not supported by scm_call2.\n",
						qseos_cmd_id);
			ret = -EINVAL;
		}
		} /*end of switch (qsee_cmd_id)  */
	break;
	}
	default: {
		pr_err("svc_id 0x%x is not supported by armv8 scm_call2.\n",
					svc_id);
		ret = -EINVAL;
		break;
	}
	} /*end of switch svc_id */
	return ret;
}


static int profiler_scm_call(u32 svc_id, u32 tz_cmd_id, const void *cmd_buf,
		size_t cmd_len, void *resp_buf, size_t resp_len)
{
	if (!is_scm_armv8())
		return scm_call(svc_id, tz_cmd_id, cmd_buf, cmd_len,
				resp_buf, resp_len);
	else
		return profiler_scm_call2(svc_id, tz_cmd_id, cmd_buf, resp_buf);
}

static int bw_profiling_command(void *req)
{
	struct tz_bw_svc_resp *bw_resp = NULL;
	uint32_t cmd_id = 0;
	int ret;

	cmd_id = *(uint32_t *)req;
	bw_resp = &((struct tz_bw_svc_buf *)req)->bwresp;
	/* Flush buffers from cache to memory. */
	dmac_flush_range(req, req +
			PAGE_ALIGN(sizeof(union tz_bw_svc_req)));
	dmac_flush_range((void *)bw_resp, ((void *)bw_resp) +
			sizeof(struct tz_bw_svc_resp));
	ret = profiler_scm_call(SCM_SVC_BW, TZ_SVC_BW_PROF_ID, req,
				sizeof(struct tz_bw_svc_buf),
				bw_resp, sizeof(struct tz_bw_svc_resp));
	if (ret) {
		pr_err("profiler_scm_call failed with err: %d\n", ret);
		return -EINVAL;
	}
	/* Invalidate cache. */
	dmac_inv_range((void *)bw_resp, ((void *)bw_resp) +
			sizeof(struct tz_bw_svc_resp));
	/* Verify cmd id and Check that request succeeded.*/
	if ((bw_resp->status != E_BW_SUCCESS) ||
		(cmd_id != bw_resp->cmd_id)) {
		ret = -1;
		pr_err("Status: %d,Cmd: %d\n",
			bw_resp->status,
			bw_resp->cmd_id);
	}
	return ret;
}

static int bw_profiling_start(struct tz_bw_svc_buf *bwbuf)
{
	struct tz_bw_svc_start_req *bwstartreq = NULL;

	bwstartreq = (struct tz_bw_svc_start_req *) &bwbuf->bwreq;
	/* Populate request data */
	bwstartreq->cmd_id = TZ_BW_SVC_START_ID;
	bwstartreq->version = TZ_BW_SVC_VERSION;
	bwbuf->req_size = sizeof(struct tz_bw_svc_start_req);
	return bw_profiling_command(bwbuf);
}

static int bw_profiling_get(void __user *argp, struct tz_bw_svc_buf *bwbuf)
{
	struct tz_bw_svc_get_req *bwgetreq = NULL;
	int ret;
	char *buf = NULL;
	const int numberofregs = 3;
	struct profiler_bw_cntrs_req cnt_buf;

	bwgetreq = (struct tz_bw_svc_get_req *) &bwbuf->bwreq;
	/* Allocate memory for get buffer */
	buf = kzalloc(PAGE_ALIGN(numberofregs * sizeof(uint32_t)), GFP_KERNEL);
	if (buf == NULL) {
		ret = -ENOMEM;
		pr_err(" Failed to allocate memory\n");
		return ret;
	}
	/* Populate request data */
	bwgetreq->cmd_id = TZ_BW_SVC_GET_ID;
	bwgetreq->buf_ptr = (uint64_t) virt_to_phys(buf);
	bwgetreq->buf_size = numberofregs * sizeof(uint32_t);
	bwbuf->req_size = sizeof(struct tz_bw_svc_get_req);
	dmac_flush_range(buf, ((void *)buf) + PAGE_ALIGN(bwgetreq->buf_size));
	ret = bw_profiling_command(bwbuf);
	if (ret) {
		pr_err("bw_profiling_command failed\n");
		return ret;
	}
	dmac_inv_range(buf, ((void *)buf) + PAGE_ALIGN(bwgetreq->buf_size));
	cnt_buf.total = *(uint32_t *) (buf + 0 * sizeof(uint32_t));
	cnt_buf.cpu = *(uint32_t *) (buf + 1 * sizeof(uint32_t));
	cnt_buf.gpu = *(uint32_t *) (buf + 2 * sizeof(uint32_t));
	if (copy_to_user(argp, &cnt_buf, sizeof(struct profiler_bw_cntrs_req)))
		pr_err("copy_to_user failed\n");
	/* Free memory for response */
	if (buf != NULL) {
		kfree(buf);
		buf = NULL;
	}
	return ret;
}

static int bw_profiling_stop(struct tz_bw_svc_buf *bwbuf)
{
	struct tz_bw_svc_stop_req *bwstopreq = NULL;

	if (stop_done) {
		stop_done = 0;
		return 0;
	}
	bwstopreq = (struct tz_bw_svc_stop_req *) &bwbuf->bwreq;
	/* Populate request data */
	bwstopreq->cmd_id = TZ_BW_SVC_STOP_ID;
	bwbuf->req_size = sizeof(struct tz_bw_svc_stop_req);
	return bw_profiling_command(bwbuf);
}


static int profiler_get_bw_info(void __user *argp)
{
	int ret = 0;
	struct tz_bw_svc_buf *bwbuf = NULL;
	struct profiler_bw_cntrs_req cnt_buf;

	ret = copy_from_user(&cnt_buf, argp,
				sizeof(struct profiler_bw_cntrs_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	/* Allocate memory for request */
	bwbuf = kzalloc(PAGE_ALIGN(sizeof(struct tz_bw_svc_buf)), GFP_KERNEL);
	if (bwbuf == NULL)
		return -ENOMEM;
	switch (cnt_buf.cmd) {
	case TZ_BW_SVC_START_ID: {
		ret = bw_profiling_start(bwbuf);
		if (ret)
			pr_err("bw_profiling_start Failed with ret: %d\n", ret);
		stop_done = 0;
		break;
	}
	case TZ_BW_SVC_GET_ID: {
		ret = bw_profiling_get(argp , bwbuf);
		if (ret)
			pr_err("bw_profiling_get Failed with ret: %d\n", ret);
		break;
	}
	case TZ_BW_SVC_STOP_ID: {
		ret = bw_profiling_stop(bwbuf);
		if (ret)
			pr_err("bw_profiling_stop Failed with ret: %d\n", ret);
		stop_done = 1;
		break;
	}
	default:
		pr_err("Invalid IOCTL: 0x%x\n", cnt_buf.cmd);
		ret = -EINVAL;
	}
	/* Free memory for command */
	if (bwbuf != NULL) {
		kfree(bwbuf);
		bwbuf = NULL;
	}
	return ret;
}

static int profiler_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct profiler_dev_handle *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	file->private_data = data;
	data->abort = 0;
	data->released = false;
	atomic_set(&data->ioctl_count, 0);
	return ret;
}

static int compat_get_profiler_bw_info(
		struct compat_profiler_bw_cntrs_req __user *data32,
		struct profiler_bw_cntrs_req __user *data)
{
	compat_uint_t total;
	compat_uint_t cpu;
	compat_uint_t gpu;
	compat_uint_t cmd;
	int err;

	err = get_user(total, &data32->total);
	err |= put_user(total, &data->total);
	err |= get_user(gpu, &data32->gpu);
	err |= put_user(gpu, &data->gpu);
	err |= get_user(cpu, &data32->cpu);
	err |= put_user(cpu, &data->cpu);
	err |= get_user(cmd, &data32->cmd);
	err |= put_user(cmd, &data->cmd);
	return err;
}

static int compat_put_profiler_bw_info(
		struct compat_profiler_bw_cntrs_req __user *data32,
		struct profiler_bw_cntrs_req __user *data)
{
	compat_uint_t total;
	compat_uint_t cpu;
	compat_uint_t gpu;
	compat_uint_t cmd;
	int err;

	err = get_user(total, &data->total);
	err |= put_user(total, &data32->total);
	err |= get_user(gpu, &data->gpu);
	err |= put_user(gpu, &data32->gpu);
	err |= get_user(cpu, &data->cpu);
	err |= put_user(cpu, &data32->cpu);
	err |= get_user(cmd, &data->cmd);
	err |= put_user(cmd, &data32->cmd);
	return err;
}

static unsigned int convert_cmd(unsigned int cmd)
{
	switch (cmd) {
	case COMPAT_PROFILER_IOCTL_GET_BW_INFO:
		return PROFILER_IOCTL_GET_BW_INFO;

	default:
		return cmd;
	}
}


long profiler_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;

	struct profiler_dev_handle *data = file->private_data;
	void __user *argp = (void __user *) arg;

	if (!data) {
		pr_err("Invalid/uninitialized device handle\n");
		return -EINVAL;
	}

	if (data->abort) {
		pr_err("Aborting profiler driver\n");
		return -ENODEV;
	}

	switch (cmd) {
	case PROFILER_IOCTL_GET_BW_INFO:{
		atomic_inc(&data->ioctl_count);
		ret = profiler_get_bw_info(argp);
		if (ret)
			pr_err("failed get system bandwidth info: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	default:
		pr_err("Invalid IOCTL: 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

long compat_profiler_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case COMPAT_PROFILER_IOCTL_GET_BW_INFO:{
		struct compat_profiler_bw_cntrs_req __user *data32;
		struct profiler_bw_cntrs_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
		err = compat_get_profiler_bw_info(data32, data);
		if (err)
			return err;
		ret = profiler_ioctl(file, convert_cmd(cmd),
					(unsigned long)data);
		err = compat_put_profiler_bw_info(data32, data);
		return ret ? ret : err;
	}
	break;
	default:
		return -ENOIOCTLCMD;
	break;
	}
	return 0;
}


static int profiler_release(struct inode *inode, struct file *file)
{
	pr_err("profiler release\n");
	return 0;
}

static const struct file_operations profiler_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = profiler_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_profiler_ioctl,
#endif
	.open = profiler_open,
	.release = profiler_release
};

static int profiler_init(void)
{
	int rc;
	struct device *class_dev;

	rc = alloc_chrdev_region(&profiler_device_no, 0, 1, PROFILER_DEV);
	if (rc < 0) {
		pr_err("alloc_chrdev_region failed %d\n", rc);
		return rc;
	}

	driver_class = class_create(THIS_MODULE, PROFILER_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		pr_err("class_create failed %d\n", rc);
		goto exit_unreg_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, profiler_device_no, NULL,
			PROFILER_DEV);
	if (IS_ERR(class_dev)) {
		pr_err("class_device_create failed %d\n", rc);
		rc = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&profiler.cdev, &profiler_fops);
	profiler.cdev.owner = THIS_MODULE;

	rc = cdev_add(&profiler.cdev, MKDEV(MAJOR(profiler_device_no), 0), 1);
	if (rc < 0) {
		pr_err("cdev_add failed %d\n", rc);
		goto exit_destroy_device;
	}

	profiler.pdev = class_dev;
	return 0;

exit_destroy_device:
	device_destroy(driver_class, profiler_device_no);
exit_destroy_class:
	class_destroy(driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(profiler_device_no, 1);
	return rc;
}

static void profiler_exit(void)
{
	pr_info("Exiting from profiler\n");
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. trustzone Communicator");

module_init(profiler_init);
module_exit(profiler_exit);
