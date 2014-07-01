/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.

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
#include <linux/of.h>
#include <mach/qdsp6v2/apr.h>

#define DEVICE_NAME "avtimer"


#define ADSP_CMD_SET_POWER_COLLAPSE_STATE 0x0001115C

static int major;	/* Major number assigned to our device driver */
struct avtimer_t {
	struct apr_svc *core_handle_q;
	struct cdev myc;
	struct class *avtimer_class;
	struct mutex avtimer_lock;
	int avtimer_open_cnt;
	struct dev_avtimer_data avtimer_pdata;
	wait_queue_head_t adsp_resp_wait;
	int enable_timer_resp_recieved;
	int timer_handle;
	void __iomem *p_avtimer_msw;
	void __iomem *p_avtimer_lsw;
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
	switch (ioctl_num) {
	case IOCTL_GET_AVTIMER_TICK:
	{
		uint32_t avtimer_msw_1st = 0, avtimer_lsw = 0;
		uint32_t avtimer_msw_2nd = 0;
		uint64_t avtimer_tick;
		do {
			avtimer_msw_1st = ioread32(avtimer.p_avtimer_msw);
			avtimer_lsw = ioread32(avtimer.p_avtimer_lsw);
			avtimer_msw_2nd = ioread32(avtimer.p_avtimer_msw);
		} while (avtimer_msw_1st != avtimer_msw_2nd);

		avtimer_tick =
		((uint64_t) avtimer_msw_1st << 32) | avtimer_lsw;

		pr_debug("%s: AV Timer tick: msw: %x, lsw: %x time %llx\n",
		__func__, avtimer_msw_1st, avtimer_lsw, avtimer_tick);
		if (copy_to_user((void *) ioctl_param, &avtimer_tick,
				sizeof(avtimer_tick))) {
					pr_err("copy_to_user failed\n");
					return -EFAULT;
			}
		}
		break;

	default:
		pr_err("%s: invalid cmd\n", __func__);
		return -EINVAL;
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
	struct resource *reg_lsb = NULL, *reg_msb = NULL;

	if (!pdev) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	reg_lsb = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "avtimer_lsb_addr");
	if (!reg_lsb) {
		dev_err(&pdev->dev, "%s: Looking up %s property",
			"avtimer_lsb_addr", __func__);
		return -EINVAL;
	}
	reg_msb = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "avtimer_msb_addr");
	if (!reg_msb) {
		dev_err(&pdev->dev, "%s: Looking up %s property",
			"avtimer_msb_addr", __func__);
		return -EINVAL;
	}
	avtimer.p_avtimer_lsw = devm_ioremap_nocache(&pdev->dev,
				reg_lsb->start, resource_size(reg_lsb));
	if (!avtimer.p_avtimer_lsw) {
		dev_err(&pdev->dev, "%s: ioremap failed for lsb avtimer register",
			__func__);
		return -ENOMEM;
	}

	avtimer.p_avtimer_msw = devm_ioremap_nocache(&pdev->dev,
				reg_msb->start, resource_size(reg_msb));
	if (!avtimer.p_avtimer_msw) {
		dev_err(&pdev->dev, "%s: ioremap failed for msb avtimer register",
			__func__);
		goto unmap;
	}
	/* get the device number */
	if (major)
		result = register_chrdev_region(dev, 1, DEVICE_NAME);
	else {
		result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
		major = MAJOR(dev);
	}

	if (result < 0) {
		pr_err("%s: Registering avtimer device failed\n", __func__);
		goto unmap;
	}

	avtimer.avtimer_class = class_create(THIS_MODULE, "avtimer");
	if (IS_ERR(avtimer.avtimer_class)) {
		result = PTR_ERR(avtimer.avtimer_class);
		pr_err("%s: Error creating avtimer class: %d\n",
			__func__, result);
		goto unregister_chrdev_region;
	}

	cdev_init(&avtimer.myc, &avtimer_fops);
	result = cdev_add(&avtimer.myc, dev, 1);

	if (result < 0) {
		pr_err("%s: Registering file operations failed\n", __func__);
		goto class_destroy;
	}

	device_handle = device_create(avtimer.avtimer_class,
			NULL, avtimer.myc.dev, NULL, "avtimer");
	if (IS_ERR(device_handle)) {
		result = PTR_ERR(device_handle);
		pr_err("%s: device_create failed: %d\n", __func__, result);
		goto class_destroy;
	}
	init_waitqueue_head(&avtimer.adsp_resp_wait);
	mutex_init(&avtimer.avtimer_lock);
	avtimer.avtimer_open_cnt = 0;

	pr_debug("%s: Device create done for avtimer major=%d\n",
			__func__, major);

	return 0;

class_destroy:
	class_destroy(avtimer.avtimer_class);
unregister_chrdev_region:
	unregister_chrdev_region(MKDEV(major, 0), 1);
unmap:
	if (avtimer.p_avtimer_lsw)
		devm_iounmap(&pdev->dev, avtimer.p_avtimer_lsw);
	if (avtimer.p_avtimer_msw)
		devm_iounmap(&pdev->dev, avtimer.p_avtimer_msw);
	avtimer.p_avtimer_lsw = NULL;
	avtimer.p_avtimer_msw = NULL;
	return result;

}

static int __devexit dev_avtimer_remove(struct platform_device *pdev)
{
	pr_debug("%s: dev_avtimer_remove\n", __func__);

	if (avtimer.p_avtimer_lsw)
		devm_iounmap(&pdev->dev, avtimer.p_avtimer_lsw);
	if (avtimer.p_avtimer_msw)
		devm_iounmap(&pdev->dev, avtimer.p_avtimer_msw);
	device_destroy(avtimer.avtimer_class, avtimer.myc.dev);
	cdev_del(&avtimer.myc);
	class_destroy(avtimer.avtimer_class);
	unregister_chrdev_region(MKDEV(major, 0), 1);

	return 0;
}

static const struct of_device_id avtimer_machine_of_match[]  = {
	{ .compatible = "qcom,avtimer", },
	{},
};
static struct platform_driver dev_avtimer_driver = {
	.probe = dev_avtimer_probe,
	.remove = dev_avtimer_remove,
	.driver = {
		.name = "dev_avtimer",
		.of_match_table = avtimer_machine_of_match,
	},
};

static int  __init avtimer_init(void)
{
	s32 rc;
	rc = platform_driver_register(&dev_avtimer_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register failed\n", __func__);
		goto error_platform_driver;
	}
	pr_debug("%s: dev_avtimer_init : done\n", __func__);

	return 0;
error_platform_driver:

	pr_err("%s: encounterd error\n", __func__);
	return rc;
}

static void __exit avtimer_exit(void)
{
	pr_debug("%s: avtimer_exit\n", __func__);
	platform_driver_unregister(&dev_avtimer_driver);
}

module_init(avtimer_init);
module_exit(avtimer_exit);

MODULE_DESCRIPTION("avtimer driver");
MODULE_LICENSE("GPL v2");
