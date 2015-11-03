/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <video/msm_dba.h>
#include "msm_dba_internal.h"

static void msm_dba_helper_hdcp_handler(struct work_struct *work)
{
	struct msm_dba_device_info *dev;
	int rc = 0;

	if (!work) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	dev = container_of(work, struct msm_dba_device_info, hdcp_work);

	mutex_lock(&dev->dev_mutex);
	if (dev->hdcp_status) {
		pr_debug("%s[%s:%d] HDCP is authenticated\n", __func__,
			 dev->chip_name, dev->instance_id);
		mutex_unlock(&dev->dev_mutex);
		return;
	}

	if (dev->dev_ops.hdcp_reset) {
		rc = dev->dev_ops.hdcp_reset(dev);
		if (rc)
			pr_err("%s[%s:%d] HDCP reset failed\n", __func__,
			       dev->chip_name, dev->instance_id);
	}

	if (dev->dev_ops.hdcp_retry) {
		rc = dev->dev_ops.hdcp_retry(dev, MSM_DBA_ASYNC_FLAG);
		if (rc)
			pr_err("%s[%s:%d] HDCP retry failed\n", __func__,
			       dev->chip_name, dev->instance_id);
	}
	mutex_unlock(&dev->dev_mutex);
}

static void msm_dba_helper_issue_cb(struct msm_dba_device_info *dev,
				    struct msm_dba_client_info *client,
				    enum msm_dba_callback_event event)
{
	struct msm_dba_client_info *c;
	struct list_head *pos = NULL;
	u32 user_mask = 0;

	list_for_each(pos, &dev->client_list) {
		c = list_entry(pos, struct msm_dba_client_info, list);
		if (client && client == c)
			continue;

		user_mask = c->event_mask & event;
		if (c->cb && user_mask)
			c->cb(c->cb_data, user_mask);
	}
}

static irqreturn_t msm_dba_helper_irq_handler(int irq, void *dev)
{
	struct msm_dba_device_info *device = dev;
	u32 mask = 0;
	int rc = 0;
	bool ret;

	mutex_lock(&device->dev_mutex);
	if (device->dev_ops.handle_interrupts) {
		rc = device->dev_ops.handle_interrupts(device, &mask);
		if (rc)
			pr_err("%s: interrupt handler failed\n", __func__);
	}

	pr_debug("%s(%s:%d): Eventmask  = 0x%x\n", __func__, device->chip_name,
		 device->instance_id, mask);
	if (mask)
		msm_dba_helper_issue_cb(device, NULL, mask);

	if ((mask &  MSM_DBA_CB_HDCP_LINK_UNAUTHENTICATED) &&
	     device->hdcp_monitor_on) {
		ret = queue_work(device->hdcp_wq, &device->hdcp_work);
		if (!ret)
			pr_err("%s: queue_work failed %d\n", __func__, rc);
	}

	if (device->dev_ops.unmask_interrupts)
		rc = device->dev_ops.unmask_interrupts(device, mask);

	mutex_unlock(&device->dev_mutex);
	return IRQ_HANDLED;
}

int msm_dba_helper_i2c_write_byte(struct i2c_client *client,
				  u8 addr,
				  u8 reg,
				  u8 val)
{
	int rc = 0;
	struct i2c_msg msg;
	u8 buf[2] = {reg, val};

	if (!client) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: [%s:0x02%x] : W[0x%02x, 0x%02x]\n", __func__,
		 client->name, addr, reg, val);
	client->addr = addr;

	msg.addr = addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		pr_err("%s: i2c write failed\n", __func__);
		rc = -EIO;
	}

	return rc;
}

int msm_dba_helper_i2c_write_buffer(struct i2c_client *client,
				    u8 addr,
				    u8 *buf,
				    u32 size)
{
	int rc = 0;
	struct i2c_msg msg;

	if (!client) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: [%s:0x02%x] : W %d bytes\n", __func__,
		 client->name, addr, size);

	client->addr = addr;

	msg.addr = addr;
	msg.flags = 0;
	msg.len = size;
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) != 1) {
		pr_err("%s: i2c write failed\n", __func__);
		rc = -EIO;
	}

	return rc;
}
int msm_dba_helper_i2c_read(struct i2c_client *client,
			    u8 addr,
			    u8 reg,
			    char *buf,
			    u32 size)
{
	int rc = 0;
	struct i2c_msg msg[2];

	if (!client || !buf) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	client->addr = addr;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = buf;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		pr_err("%s: i2c read failed\n", __func__);
		rc = -EIO;
	}

	pr_debug("%s: [%s:0x02%x] : R[0x%02x, 0x%02x]\n", __func__,
		 client->name, addr, reg, *buf);
	return rc;
}

int msm_dba_helper_power_on(void *client, bool on, u32 flags)
{
	int rc = 0;
	struct msm_dba_client_info *c = client;
	struct msm_dba_device_info *device;
	struct msm_dba_client_info *node;
	struct list_head *pos = NULL;
	bool power_on = false;

	if (!c) {
		pr_err("%s: Invalid Params\n", __func__);
		return -EINVAL;
	}

	device = c->dev;
	mutex_lock(&device->dev_mutex);

	/*
	 * Power on the device if atleast one client powers on the device. But
	 * power off will be done only after all the clients have called power
	 * off
	 */
	if (on == device->power_status) {
		c->power_on = on;
	} else if (on) {
		rc = device->dev_ops.dev_power_on(device, on);
		if (rc)
			pr_err("%s:%s: power on failed\n", device->chip_name,
			       __func__);
		else
			c->power_on = on;
	} else {
		c->power_on = false;

		list_for_each(pos, &device->client_list) {
			node = list_entry(pos, struct msm_dba_client_info,
					  list);
			if (c->power_on) {
				power_on = true;
				break;
			}
		}

		if (!power_on) {
			rc = device->dev_ops.dev_power_on(device, false);
			if (rc) {
				pr_err("%s:%s: power off failed\n",
				       device->chip_name, __func__);
				c->power_on = true;
			}
		}
	}

	mutex_unlock(&device->dev_mutex);
	return rc;
}

int msm_dba_helper_video_on(void *client, bool on,
			    struct msm_dba_video_cfg *cfg, u32 flags)
{
	int rc = 0;
	struct msm_dba_client_info *c = client;
	struct msm_dba_device_info *device;
	struct msm_dba_client_info *node;
	struct list_head *pos = NULL;
	bool video_on = false;

	if (!c) {
		pr_err("%s: Invalid Params\n", __func__);
		return -EINVAL;
	}

	device = c->dev;
	mutex_lock(&device->dev_mutex);

	/*
	 * Video will be turned on if at least one client turns on video. But
	 * video off will be done only after all the clients have called video
	 * off
	 */
	if (on == device->video_status) {
		c->video_on = on;
	} else if (on) {
		rc = device->dev_ops.dev_video_on(device, cfg, on);
		if (rc)
			pr_err("%s:%s: video on failed\n", device->chip_name,
			       __func__);
		else
			c->video_on = on;
	} else {
		c->video_on = false;

		list_for_each(pos, &device->client_list) {
			node = list_entry(pos, struct msm_dba_client_info,
					  list);
			if (c->video_on) {
				video_on = true;
				break;
			}
		}

		if (!video_on) {
			rc = device->dev_ops.dev_video_on(device, cfg, false);
			if (rc) {
				pr_err("%s:%s: video off failed\n",
				       device->chip_name, __func__);
				c->video_on = true;
			}
		}
	}

	mutex_unlock(&device->dev_mutex);
	return rc;
}

int msm_dba_helper_interrupts_enable(void *client, bool on,
				     u32 event_mask, u32 flags)
{
	struct msm_dba_client_info *c = client;
	struct msm_dba_device_info *device;

	if (!c) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	device = c->dev;
	mutex_lock(&device->dev_mutex);

	if (on)
		c->event_mask = event_mask;
	else
		c->event_mask = 0;

	mutex_unlock(&device->dev_mutex);
	return 0;
}

int msm_dba_helper_register_irq(struct msm_dba_device_info *dev,
				u32 irq, u32 irq_flags)
{
	int rc;

	if (!dev) {
		pr_err("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&dev->dev_mutex);

	rc = request_threaded_irq(irq, NULL, msm_dba_helper_irq_handler,
				  irq_flags, dev->chip_name, dev);

	if (rc)
		pr_err("%s:%s: Failed to register irq\n", dev->chip_name,
		       __func__);

	mutex_unlock(&dev->dev_mutex);
	return rc;
}

int msm_dba_helper_get_caps(void *client, struct msm_dba_capabilities *caps)
{
	struct msm_dba_client_info *c = client;
	struct msm_dba_device_info *device;

	if (!c || !caps) {
		pr_err("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	device = c->dev;
	mutex_lock(&device->dev_mutex);

	memcpy(caps, &device->caps, sizeof(*caps));

	mutex_unlock(&device->dev_mutex);
	return 0;
}

int msm_dba_register_hdcp_monitor(struct msm_dba_device_info *dev, bool enable)
{
	int rc = 0;

	if (!dev) {
		pr_err("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		dev->hdcp_wq = alloc_workqueue("hdcp_monitor(%s:%d)", 0, 0,
					       dev->chip_name,
					       dev->instance_id);
		if (!dev->hdcp_wq) {
			pr_err("%s: failed to allocate wq\n", __func__);
			rc = -ENOMEM;
			goto fail;
		}

		INIT_WORK(&dev->hdcp_work, msm_dba_helper_hdcp_handler);
		dev->hdcp_monitor_on = true;
	} else if (!enable && dev->hdcp_wq) {
		destroy_workqueue(dev->hdcp_wq);
		dev->hdcp_wq = NULL;
		dev->hdcp_monitor_on = false;
	}

fail:
	return rc;
}

int msm_dba_helper_force_reset(void *client, u32 flags)
{
	struct msm_dba_client_info *c = client;
	struct msm_dba_device_info *device;
	int rc = 0;

	if (!c) {
		pr_err("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	device = c->dev;
	mutex_lock(&device->dev_mutex);

	msm_dba_helper_issue_cb(device, c, MSM_DBA_CB_PRE_RESET);

	if (device->dev_ops.force_reset)
		rc = device->dev_ops.force_reset(device, flags);

	if (rc)
		pr_err("%s: Force reset failed\n", __func__);

	msm_dba_helper_issue_cb(device, c, MSM_DBA_CB_POST_RESET);

	mutex_unlock(&device->dev_mutex);
	return rc;
}
