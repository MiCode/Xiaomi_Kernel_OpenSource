
/* Copyright (c) 2012, The Linux Foundation. All rights reserved.

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/avtimer.h>
#include <mach/qdsp6v2/apr.h>

#define DEVICE_NAME "avtimer"


#define ADSP_CMD_SET_POWER_COLLAPSE_STATE 0x0001115C

static int major;	/* Major number assigned to our device driver */
struct avtimer_t {
	struct cdev myc;
	struct class *avtimer_class;
	struct mutex avtimer_lock;
	int avtimer_open_cnt;
	struct dev_avtimer_data *avtimer_pdata;
};
static struct avtimer_t avtimer;

static struct apr_svc *core_handle;

struct adsp_power_collapse {
	struct apr_hdr hdr;
	uint32_t power_collapse;
};

static int32_t avcs_core_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *payload;

	pr_debug("core msg: payload len = %u, apr resp opcode = 0x%X\n",
		data->payload_size, data->opcode);

	switch (data->opcode) {

	case APR_BASIC_RSP_RESULT:{

		if (data->payload_size == 0) {
			pr_err("%s: APR_BASIC_RSP_RESULT No Payload ",
					__func__);
			return 0;
		}

		payload = data->payload;

		switch (payload[0]) {

		case ADSP_CMD_SET_POWER_COLLAPSE_STATE:
			pr_debug("CMD_SET_POWER_COLLAPSE_STATE status[0x%x]\n",
					payload[1]);
			break;
		default:
			pr_err("Invalid cmd rsp[0x%x][0x%x]\n",
					payload[0], payload[1]);
			break;
		}
		break;
	}
	case RESET_EVENTS:{
		pr_debug("Reset event received in Core service");
		apr_reset(core_handle);
		core_handle = NULL;
		break;
	}

	default:
		pr_err("Message id from adsp core svc: %d\n", data->opcode);
		break;
	}

	return 0;
}

int avcs_core_open(void)
{
	if (core_handle == NULL)
		core_handle = apr_register("ADSP", "CORE",
					avcs_core_callback, 0xFFFFFFFF, NULL);

	pr_debug("Open_q %p\n", core_handle);
	if (core_handle == NULL) {
		pr_err("%s: Unable to register CORE\n", __func__);
		return -ENODEV;
	}
	return 0;
}

int avcs_core_disable_power_collapse(int disable)
{
	struct adsp_power_collapse pc;
	int rc = 0;

	if (core_handle) {
		pc.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		pc.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
					sizeof(uint32_t));
		pc.hdr.src_port = 0;
		pc.hdr.dest_port = 0;
		pc.hdr.token = 0;
		pc.hdr.opcode = ADSP_CMD_SET_POWER_COLLAPSE_STATE;
		/*
		* When power_collapse set to 1 -- If the aDSP is in the power
		* collapsed state when this command is received, it is awakened
		* from this state. The aDSP does not power collapse again until
		* the client revokes this	command
		* When power_collapse set to 0 -- This indicates to the aDSP
		* that the remote client does not need it to be out of power
		* collapse any longer. This may not always put the aDSP into
		* power collapse; the aDSP must honor an internal client's
		* power requirements as well.
		*/
		pc.power_collapse = disable;
		rc = apr_send_pkt(core_handle, (uint32_t *)&pc);
		if (rc < 0) {
			pr_debug("disable power collapse = %d failed\n",
				disable);
			return rc;
		}
		pr_debug("disable power collapse = %d\n", disable);
	}
	return 0;
}

static int avtimer_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct avtimer_t *pavtimer = &avtimer;

	pr_debug("avtimer_open\n");
	mutex_lock(&pavtimer->avtimer_lock);

	if (pavtimer->avtimer_open_cnt != 0) {
		pavtimer->avtimer_open_cnt++;
		pr_debug("%s: opened avtimer open count=%d\n",
			__func__, pavtimer->avtimer_open_cnt);
		mutex_unlock(&pavtimer->avtimer_lock);
		return 0;
	}
	try_module_get(THIS_MODULE);

	rc = avcs_core_open();
	if (core_handle)
		rc = avcs_core_disable_power_collapse(1);

	pavtimer->avtimer_open_cnt++;
	pr_debug("%s: opened avtimer open count=%d\n",
		__func__, pavtimer->avtimer_open_cnt);
	mutex_unlock(&pavtimer->avtimer_lock);
	pr_debug("avtimer_open leave rc=%d\n", rc);

	return rc;
}

static int avtimer_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct avtimer_t *pavtimer = &avtimer;

	mutex_lock(&pavtimer->avtimer_lock);
	pavtimer->avtimer_open_cnt--;

	if (core_handle && pavtimer->avtimer_open_cnt == 0)
		rc = avcs_core_disable_power_collapse(0);

	pr_debug("device_release(%p,%p) open count=%d\n",
		inode, file, pavtimer->avtimer_open_cnt);

	module_put(THIS_MODULE);

	mutex_unlock(&pavtimer->avtimer_lock);

	return rc;
}

/*
 * ioctl call provides GET_AVTIMER
 */
static long avtimer_ioctl(struct file *file, unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	struct avtimer_t *pavtimer = &avtimer;
	pr_debug("avtimer_ioctl: ioctlnum=%d,param=%lx\n",
				ioctl_num, ioctl_param);

	switch (ioctl_num) {
	case IOCTL_GET_AVTIMER_TICK:
	{
		void __iomem *p_avtimer_msw = NULL, *p_avtimer_lsw = NULL;
		uint32_t avtimer_msw_1st = 0, avtimer_lsw = 0;
		uint32_t avtimer_msw_2nd = 0;
		uint64_t avtimer_tick;

		if (pavtimer->avtimer_pdata) {
			p_avtimer_lsw = ioremap(
			pavtimer->avtimer_pdata->avtimer_lsw_phy_addr, 4);
			p_avtimer_msw = ioremap(
			pavtimer->avtimer_pdata->avtimer_msw_phy_addr, 4);
		}
		if (!p_avtimer_lsw || !p_avtimer_msw) {
			pr_err("ioremap failed\n");
			return -EIO;
		}
		do {
			avtimer_msw_1st = ioread32(p_avtimer_msw);
			avtimer_lsw = ioread32(p_avtimer_lsw);
			avtimer_msw_2nd = ioread32(p_avtimer_msw);
		} while (avtimer_msw_1st != avtimer_msw_2nd);

		avtimer_tick =
		((uint64_t) avtimer_msw_1st << 32) | avtimer_lsw;

		pr_debug("AV Timer tick: msw: %d, lsw: %d\n", avtimer_msw_1st,
				avtimer_lsw);
		if (copy_to_user((void *) ioctl_param, &avtimer_tick,
				sizeof(avtimer_tick))) {
					pr_err("copy_to_user failed\n");
					iounmap(p_avtimer_lsw);
					iounmap(p_avtimer_msw);
					return -EFAULT;
			}
		iounmap(p_avtimer_lsw);
		iounmap(p_avtimer_msw);
		}
		break;

	default:
		pr_err("invalid cmd\n");
		break;
	}

	return 0;
}

static const struct file_operations avtimer_fops = {
	.unlocked_ioctl = avtimer_ioctl,
	.open = avtimer_open,
	.release = avtimer_release
};

static int dev_avtimer_probe(struct platform_device *pdev)
{
	int result;
	dev_t dev = MKDEV(major, 0);
	struct device *device_handle;
	struct avtimer_t *pavtimer = &avtimer;

	/* get the device number */
	if (major)
		result = register_chrdev_region(dev, 1, DEVICE_NAME);
	else {
		result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
		major = MAJOR(dev);
	}

	if (result < 0) {
		pr_err("Registering avtimer device failed\n");
		return result;
	}

	pavtimer->avtimer_class = class_create(THIS_MODULE, "avtimer");
	if (IS_ERR(pavtimer->avtimer_class)) {
		result = PTR_ERR(pavtimer->avtimer_class);
		pr_err("Error creating avtimer class: %d\n", result);
		goto unregister_chrdev_region;
	}
	pavtimer->avtimer_pdata = pdev->dev.platform_data;

	cdev_init(&pavtimer->myc, &avtimer_fops);
	result = cdev_add(&pavtimer->myc, dev, 1);

	if (result < 0) {
		pr_err("Registering file operations failed\n");
		goto class_destroy;
	}

	device_handle = device_create(pavtimer->avtimer_class,
			NULL, pavtimer->myc.dev, NULL, "avtimer");
	if (IS_ERR(device_handle)) {
		result = PTR_ERR(device_handle);
		pr_err("device_create failed: %d\n", result);
		goto class_destroy;
	}

	mutex_init(&pavtimer->avtimer_lock);
	core_handle = NULL;
	pavtimer->avtimer_open_cnt = 0;

	pr_debug("Device create done for avtimer major=%d\n", major);

	return 0;

class_destroy:
	class_destroy(pavtimer->avtimer_class);
unregister_chrdev_region:
	unregister_chrdev_region(MKDEV(major, 0), 1);
	return result;

}

static int __devexit dev_avtimer_remove(struct platform_device *pdev)
{
	struct avtimer_t *pavtimer = &avtimer;

	pr_debug("dev_avtimer_remove\n");

	device_destroy(pavtimer->avtimer_class, pavtimer->myc.dev);
	cdev_del(&pavtimer->myc);
	class_destroy(pavtimer->avtimer_class);
	unregister_chrdev_region(MKDEV(major, 0), 1);

	return 0;
}

static struct platform_driver dev_avtimer_driver = {
	.probe = dev_avtimer_probe,
	.remove = __exit_p(dev_avtimer_remove),
	.driver = {.name = "dev_avtimer"}
};

static int  __init avtimer_init(void)
{
	s32 rc;
	rc = platform_driver_register(&dev_avtimer_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("platform_driver_register failed.\n");
		goto error_platform_driver;
	}
	pr_debug("dev_avtimer_init : done\n");

	return 0;
error_platform_driver:

	pr_err("encounterd error\n");
	return -ENODEV;
}

static void __exit avtimer_exit(void)
{
	pr_debug("avtimer_exit\n");
	platform_driver_unregister(&dev_avtimer_driver);
}

module_init(avtimer_init);
module_exit(avtimer_exit);

MODULE_DESCRIPTION("avtimer driver");
MODULE_LICENSE("GPL v2");
