// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "PROFILER: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/types.h>
#include <soc/qcom/profiler.h>

#include <linux/qtee_shmbridge.h>
#include <linux/qcom_scm.h>

#define PROFILER_DEV			"profiler"

static struct class *driver_class;
static dev_t profiler_device_no;

struct profiler_control {
	struct device *pdev;
	struct cdev cdev;
};

static struct profiler_control profiler;

struct profiler_dev_handle {
	bool released;
	int abort;
};

static int bw_profiling_command(const void *req)
{
	int      ret = 0;
	uint32_t qseos_cmd_id = 0;
	struct tz_bw_svc_resp *rsp = NULL;
	size_t req_size = 0, rsp_size = 0;
	struct qtee_shm bw_shm = {0};

	rsp = &((struct tz_bw_svc_buf *)req)->bwresp;
	rsp_size = sizeof(struct tz_bw_svc_resp);
	req_size = ((struct tz_bw_svc_buf *)req)->req_size;

	if (!req || !rsp) {
		pr_err("Invalid buffer pointer\n");
		return -EINVAL;
	}
	qseos_cmd_id = *(uint32_t *)req;

	ret = qtee_shmbridge_allocate_shm(PAGE_ALIGN(req_size + rsp_size), &bw_shm);
	if (ret) {
		ret = -ENOMEM;
		pr_err("shmbridge alloc failed for in msg in release\n");
		goto out;
	}

	memcpy(bw_shm.vaddr, req, req_size);
	qtee_shmbridge_flush_shm_buf(&bw_shm);

	switch (qseos_cmd_id) {
	case TZ_BW_SVC_START_ID:
	case TZ_BW_SVC_GET_ID:
	case TZ_BW_SVC_STOP_ID:
		/* Send the command to TZ */
		ret = qcom_scm_ddrbw_profiler(bw_shm.paddr, req_size,
				bw_shm.paddr + req_size, rsp_size);
		break;
	default:
		pr_err("cmd_id %d is not supported.\n",
			   qseos_cmd_id);
		ret = -EINVAL;
	} /*end of switch (qsee_cmd_id)  */

	qtee_shmbridge_inv_shm_buf(&bw_shm);
	memcpy(rsp, (char *)bw_shm.vaddr + req_size, rsp_size);
out:
	qtee_shmbridge_free_shm(&bw_shm);
	/* Verify cmd id and Check that request succeeded.*/
	if ((rsp->status != E_BW_SUCCESS) ||
		(qseos_cmd_id != rsp->cmd_id)) {
		ret = -1;
		pr_err("Status: %d,Cmd: %d\n",
			rsp->status,
			rsp->cmd_id);
	}
	return ret;
}

static int bw_profiling_start(struct tz_bw_svc_buf *bwbuf)
{
	bwbuf->bwreq.start_req.cmd_id = TZ_BW_SVC_START_ID;
	bwbuf->bwreq.start_req.version = TZ_BW_SVC_VERSION;
	bwbuf->req_size = sizeof(struct tz_bw_svc_start_req);
	return bw_profiling_command(bwbuf);
}

static int bw_profiling_get(void __user *argp, struct tz_bw_svc_buf *bwbuf)
{
	int ret;
	const int bufsize = sizeof(struct profiler_bw_cntrs_req)
							- sizeof(uint32_t);
	struct profiler_bw_cntrs_req cnt_buf;
	struct qtee_shm buf_shm = {0};

	memset(&cnt_buf, 0, sizeof(cnt_buf));
	/* Allocate memory for get buffer */
	ret = qtee_shmbridge_allocate_shm(PAGE_ALIGN(bufsize), &buf_shm);
	if (ret) {
		ret = -ENOMEM;
		pr_err("shmbridge alloc buf failed\n");
		goto out;
	}
	/* Populate request data */
	bwbuf->bwreq.get_req.cmd_id = TZ_BW_SVC_GET_ID;
	bwbuf->bwreq.get_req.buf_ptr = buf_shm.paddr;
	bwbuf->bwreq.get_req.buf_size = bufsize;
	bwbuf->req_size = sizeof(struct tz_bw_svc_get_req);
	qtee_shmbridge_flush_shm_buf(&buf_shm);
	ret = bw_profiling_command(bwbuf);
	if (ret) {
		pr_err("bw_profiling_command failed\n");
		goto out;
	}
	qtee_shmbridge_inv_shm_buf(&buf_shm);
	memcpy(&cnt_buf, buf_shm.vaddr, bufsize);
	if (copy_to_user(argp, &cnt_buf, sizeof(struct profiler_bw_cntrs_req)))
		pr_err("copy_to_user failed\n");
out:
	/* Free memory for response */
	qtee_shmbridge_free_shm(&buf_shm);
	return ret;
}

static int bw_profiling_stop(struct tz_bw_svc_buf *bwbuf)
{
	bwbuf->bwreq.stop_req.cmd_id = TZ_BW_SVC_STOP_ID;
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
	if (ret)
		return ret;
	/* Allocate memory for request */
	bwbuf = kzalloc(sizeof(struct tz_bw_svc_buf), GFP_KERNEL);
	if (bwbuf == NULL)
		return -ENOMEM;

	switch (cnt_buf.cmd) {
	case TZ_BW_SVC_START_ID:
		ret = bw_profiling_start(bwbuf);
		if (ret)
			pr_err("bw_profiling_start Failed with ret: %d\n", ret);
		break;
	case TZ_BW_SVC_GET_ID:
		ret = bw_profiling_get(argp, bwbuf);
		if (ret)
			pr_err("bw_profiling_get Failed with ret: %d\n", ret);
		break;
	case TZ_BW_SVC_STOP_ID:
		ret = bw_profiling_stop(bwbuf);
		if (ret)
			pr_err("bw_profiling_stop Failed with ret: %d\n", ret);
		break;
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
	return ret;
}

static int compat_get_profiler_bw_info(
		struct compat_profiler_bw_cntrs_req __user *data32,
		struct profiler_bw_cntrs_req __user *data)
{
	compat_uint_t val = 0;
	int err = 0;
	int i = 0;

	for (i = 0; i < (sizeof(struct profiler_bw_cntrs_req))
						/sizeof(uint32_t) - 1; ++i) {
		err |= get_user(val, (compat_uint_t *)data32 + i);
		err |= put_user(val, (uint32_t *)data + i);
	}

	return err;
}

static int compat_put_profiler_bw_info(
		struct compat_profiler_bw_cntrs_req __user *data32,
		struct profiler_bw_cntrs_req __user *data)
{
	compat_uint_t val = 0;
	int err = 0;
	int i = 0;

	for (i = 0; i < (sizeof(struct profiler_bw_cntrs_req))
						/sizeof(uint32_t) - 1; ++i) {
		err |= get_user(val, (uint32_t *)data + i);
		err |= put_user(val, (compat_uint_t *)data32 + i);
	}

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


static long profiler_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
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
	case PROFILER_IOCTL_GET_BW_INFO:
		ret = profiler_get_bw_info(argp);
		if (ret)
			pr_err("failed get system bandwidth info: %d\n", ret);
		break;
	default:
		pr_err("Invalid IOCTL: 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int profiler_release(struct inode *inode, struct file *file)
{
	pr_info("profiler release\n");
	return 0;
}

static const struct file_operations profiler_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = profiler_ioctl,
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
		pr_err("%s: cdev_add failed %d\n", __func__, rc);
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
