/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/err.h>

#include <linux/leds-msm-tricolor.h>
#include <mach/msm_rpcrouter.h>

#define LED_RPC_PROG	0x30000091
#define LED_RPC_VER	0x00030001

#define LED_SUBSCRIBE_PROC	0x03
#define LED_SUBS_RCV_EVNT	0x01
#define LED_SUBS_REGISTER	0x00
#define LED_EVNT_CLASS_ALL	0x00
#define LINUX_HOST		0x04
#define LED_CMD_PROC		0x02
#define TRICOLOR_LED_ID		0x0A

enum tricolor_led_status {
	ALL_OFF,
	ALL_ON,
	BLUE_ON,
	BLUE_OFF,
	RED_ON,
	RED_OFF,
	GREEN_ON,
	GREEN_OFF,
	BLUE_BLINK,
	RED_BLINK,
	GREEN_BLINK,
	BLUE_BLINK_OFF,
	RED_BLINK_OFF,
	GREEN_BLINK_OFF,
	LED_MAX,
};

struct led_cmd_data_type {
	u32 cmd_data_type_ptr; /* cmd_data_type ptr */
	u32 ver; /* version */
	u32 id; /* command id */
	u32 handle; /* handle returned from subscribe proc */
	u32 disc_id1; /* discriminator id */
	u32 input_ptr; /* input ptr length */
	u32 input_val; /* command specific data */
	u32 input_len; /* length of command input */
	u32 disc_id2; /* discriminator id */
	u32 output_len; /* length of output data */
	u32 delayed; /* execution context for modem */
};

struct led_subscribe_req {
	u32 subs_ptr; /* subscribe ptr */
	u32 ver; /* version */
	u32 srvc; /* command or event */
	u32 req; /* subscribe or unsubscribe */
	u32 host_os; /* host operating system */
	u32 disc_id; /* discriminator id */
	u32 event; /* event */
	u32 cb_id; /* callback id */
	u32 handle_ptr; /* handle ptr */
	u32 handle_data; /* handle data */
};

struct tricolor_led_data {
	struct led_classdev	cdev;
	struct msm_rpc_client	*rpc_client;
	bool			blink_status;
	struct mutex		lock;
	u8			color;
};

static struct led_subscribe_req *led_subs_req;

static int led_send_cmd_arg(struct msm_rpc_client *client,
				    void *buffer, void *data)
{
	struct led_cmd_data_type *led_cmd = buffer;
	enum tricolor_led_status status = *(enum tricolor_led_status *) data;

	led_cmd->cmd_data_type_ptr = cpu_to_be32(0x01);
	led_cmd->ver = cpu_to_be32(0x03);
	led_cmd->id = cpu_to_be32(TRICOLOR_LED_ID);
	led_cmd->handle = cpu_to_be32(led_subs_req->handle_data);
	led_cmd->disc_id1 = cpu_to_be32(TRICOLOR_LED_ID);
	led_cmd->input_ptr = cpu_to_be32(0x01);
	led_cmd->input_val = cpu_to_be32(status);
	led_cmd->input_len = cpu_to_be32(0x01);
	led_cmd->disc_id2 = cpu_to_be32(TRICOLOR_LED_ID);
	led_cmd->output_len = cpu_to_be32(0x00);
	led_cmd->delayed = cpu_to_be32(0x00);

	return sizeof(*led_cmd);
}

static int led_rpc_res(struct msm_rpc_client *client,
				    void *buffer, void *data)
{
	uint32_t result;

	result = be32_to_cpu(*((uint32_t *)buffer));
	pr_debug("%s: request completed: 0x%x\n", __func__, result);

	return 0;
}

static void led_rpc_set_status(struct msm_rpc_client *client,
			enum tricolor_led_status status)
{
	int rc;

	rc = msm_rpc_client_req(client, LED_CMD_PROC,
			led_send_cmd_arg, &status, led_rpc_res, NULL, -1);
	if (rc)
		pr_err("%s: RPC client request for led failed", __func__);

}

static ssize_t led_blink_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct tricolor_led_data *led = dev_get_drvdata(dev);

	return snprintf(buf, 2, "%d\n", led->blink_status);
}

static ssize_t led_blink_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tricolor_led_data *led = dev_get_drvdata(dev);
	enum tricolor_led_status status;
	unsigned long value;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &value);
	if (rc)
		return rc;


	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(dev, "invalid brightness\n");
		return -EINVAL;
	}

	switch (led->color) {
	case LED_COLOR_RED:
		status = value ? RED_BLINK : RED_BLINK_OFF;
		break;
	case LED_COLOR_GREEN:
		status = value ? GREEN_BLINK : GREEN_BLINK_OFF;
		break;
	case LED_COLOR_BLUE:
		status = value ? BLUE_BLINK : BLUE_BLINK_OFF;
		break;
	default:
		dev_err(dev, "unknown led device\n");
		return -EINVAL;
	}

	mutex_lock(&led->lock);
	led->blink_status = !!value;
	led->cdev.brightness = 0;

	/* program the led blink */
	led_rpc_set_status(led->rpc_client, status);
	mutex_unlock(&led->lock);

	return size;
}

static DEVICE_ATTR(blink, 0644, led_blink_show, led_blink_store);

static void tricolor_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct tricolor_led_data *led;
	enum tricolor_led_status status;

	led = container_of(led_cdev, struct tricolor_led_data, cdev);

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "invalid brightness\n");
		return;
	}

	switch (led->color) {
	case LED_COLOR_RED:
		status = value ? RED_ON : RED_OFF;
		break;
	case LED_COLOR_GREEN:
		status = value ? GREEN_ON : GREEN_OFF;
		break;
	case LED_COLOR_BLUE:
		status = value ? BLUE_ON : BLUE_OFF;
		break;
	default:
		dev_err(led->cdev.dev, "unknown led device\n");
		return;
	}

	mutex_lock(&led->lock);
	led->blink_status = 0;
	led->cdev.brightness = value;

	/* program the led brightness */
	led_rpc_set_status(led->rpc_client, status);
	mutex_unlock(&led->lock);
}

static enum led_brightness tricolor_led_get(struct led_classdev *led_cdev)
{
	struct tricolor_led_data *led;

	led = container_of(led_cdev, struct tricolor_led_data, cdev);

	return led->cdev.brightness;
}

static int led_rpc_register_subs_arg(struct msm_rpc_client *client,
				    void *buffer, void *data)
{
	led_subs_req = buffer;

	led_subs_req->subs_ptr = cpu_to_be32(0x1);
	led_subs_req->ver = cpu_to_be32(0x1);
	led_subs_req->srvc = cpu_to_be32(LED_SUBS_RCV_EVNT);
	led_subs_req->req = cpu_to_be32(LED_SUBS_REGISTER);
	led_subs_req->host_os = cpu_to_be32(LINUX_HOST);
	led_subs_req->disc_id = cpu_to_be32(LED_SUBS_RCV_EVNT);
	led_subs_req->event = cpu_to_be32(LED_EVNT_CLASS_ALL);
	led_subs_req->cb_id = cpu_to_be32(0x1);
	led_subs_req->handle_ptr = cpu_to_be32(0x1);
	led_subs_req->handle_data = cpu_to_be32(0x0);

	return sizeof(*led_subs_req);
}

static int led_cb_func(struct msm_rpc_client *client, void *buffer, int in_size)
{
	struct rpc_request_hdr *hdr = buffer;
	int rc;

	hdr->type = be32_to_cpu(hdr->type);
	hdr->xid = be32_to_cpu(hdr->xid);
	hdr->rpc_vers = be32_to_cpu(hdr->rpc_vers);
	hdr->prog = be32_to_cpu(hdr->prog);
	hdr->vers = be32_to_cpu(hdr->vers);
	hdr->procedure = be32_to_cpu(hdr->procedure);

	msm_rpc_start_accepted_reply(client, hdr->xid,
				     RPC_ACCEPTSTAT_SUCCESS);
	rc = msm_rpc_send_accepted_reply(client, 0);
	if (rc)
		pr_err("%s: sending reply failed: %d\n", __func__, rc);

	return rc;
}

static int __devinit tricolor_led_probe(struct platform_device *pdev)
{
	const struct led_platform_data *pdata = pdev->dev.platform_data;
	struct msm_rpc_client *rpc_client;
	struct led_info *curr_led;
	struct tricolor_led_data *led, *tmp_led;
	int rc, i, j;

	if (!pdata) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	/* initialize rpc client */
	rpc_client = msm_rpc_register_client("led", LED_RPC_PROG,
					LED_RPC_VER, 0, led_cb_func);
	rc = IS_ERR(rpc_client);
	if (rc) {
		dev_err(&pdev->dev, "failed to initialize rpc_client\n");
		return -EINVAL;
	}

	/* subscribe */
	rc = msm_rpc_client_req(rpc_client, LED_SUBSCRIBE_PROC,
				led_rpc_register_subs_arg, NULL,
				led_rpc_res, NULL, -1);
	if (rc) {
		pr_err("%s: RPC client request failed for subscribe services\n",
						__func__);
		goto fail_mem_alloc;
	}

	led = devm_kzalloc(&pdev->dev, pdata->num_leds * sizeof(*led),
							GFP_KERNEL);
	if (!led) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		rc = -ENOMEM;
		goto fail_mem_alloc;
	}

	for (i = 0; i < pdata->num_leds; i++) {
		curr_led	= &pdata->leds[i];
		tmp_led		= &led[i];

		tmp_led->cdev.name		= curr_led->name;
		tmp_led->cdev.default_trigger   = curr_led->default_trigger;
		tmp_led->cdev.brightness_set    = tricolor_led_set;
		tmp_led->cdev.brightness_get    = tricolor_led_get;
		tmp_led->cdev.brightness	= LED_OFF;
		tmp_led->cdev.max_brightness	= LED_FULL;
		tmp_led->color			= curr_led->flags;
		tmp_led->rpc_client		= rpc_client;
		tmp_led->blink_status		= false;

		mutex_init(&tmp_led->lock);

		rc = led_classdev_register(&pdev->dev, &tmp_led->cdev);
		if (rc) {
			dev_err(&pdev->dev, "failed to register led %s(%d)\n",
						 tmp_led->cdev.name, rc);
			goto fail_led_reg;
		}

		/* Add blink attributes */
		rc = device_create_file(tmp_led->cdev.dev, &dev_attr_blink);
		if (rc) {
			dev_err(&pdev->dev, "failed to create blink attr\n");
			goto fail_blink_attr;
		}
		dev_set_drvdata(tmp_led->cdev.dev, tmp_led);
	}

	platform_set_drvdata(pdev, led);

	return 0;

fail_blink_attr:
	j = i;
	while (j)
		device_remove_file(led[--j].cdev.dev, &dev_attr_blink);
	i++;
fail_led_reg:
	while (i) {
		led_classdev_unregister(&led[--i].cdev);
		mutex_destroy(&led[i].lock);
	}
fail_mem_alloc:
	msm_rpc_unregister_client(rpc_client);
	return rc;
}

static int __devexit tricolor_led_remove(struct platform_device *pdev)
{
	const struct led_platform_data *pdata = pdev->dev.platform_data;
	struct tricolor_led_data *led = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pdata->num_leds; i++) {
		led_classdev_unregister(&led[i].cdev);
		device_remove_file(led[i].cdev.dev, &dev_attr_blink);
		mutex_destroy(&led[i].lock);
	}

	msm_rpc_unregister_client(led->rpc_client);

	return 0;
}

static struct platform_driver tricolor_led_driver = {
	.probe		= tricolor_led_probe,
	.remove		= __devexit_p(tricolor_led_remove),
	.driver		= {
		.name	= "msm-tricolor-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init tricolor_led_init(void)
{
	return platform_driver_register(&tricolor_led_driver);
}
late_initcall(tricolor_led_init);

static void __exit tricolor_led_exit(void)
{
	platform_driver_unregister(&tricolor_led_driver);
}
module_exit(tricolor_led_exit);

MODULE_DESCRIPTION("MSM Tri-color LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:tricolor-led");
