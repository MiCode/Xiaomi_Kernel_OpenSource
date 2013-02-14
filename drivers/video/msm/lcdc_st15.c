/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include "msm_fb.h"

#define DEVICE_NAME "sii9022"
#define SII9022_DEVICE_ID   0xB0
#define SII9022_ISR                   0x3D
#define SII9022_ISR_RXS_STATUS        0x08

static int lcdc_sii9022_panel_on(struct platform_device *pdev);
static int lcdc_sii9022_panel_off(struct platform_device *pdev);

static struct i2c_client *sii9022_i2c_client;

struct sii9022_data {
	struct msm_hdmi_platform_data *pd;
	struct platform_device *pdev;
	struct work_struct work;
	int x_res;
	int y_res;
	int sysfs_entry_created;
	int hdmi_attached;
};
static struct sii9022_data *dd;

struct sii9022_i2c_addr_data{
	u8 addr;
	u8 data;
};

/* video mode data */
static u8 video_mode_data[] = {
	0x00,
	0xF9, 0x1C, 0x70, 0x17, 0x72, 0x06, 0xEE, 0x02,
};

static u8 avi_io_format[] = {
	0x09,
	0x00, 0x00,
};

/* power state */
static struct sii9022_i2c_addr_data regset0[] = {
	{ 0x60, 0x04 },
	{ 0x63, 0x00 },
	{ 0x1E, 0x00 },
};

static u8 video_infoframe[] = {
	0x0C,
	0xF0, 0x00, 0x68, 0x00, 0x04, 0x00, 0x19, 0x00,
	0xE9, 0x02, 0x04, 0x01, 0x04, 0x06,
};

/* configure audio */
static struct sii9022_i2c_addr_data regset1[] = {
	{ 0x26, 0x90 },
	{ 0x20, 0x90 },
	{ 0x1F, 0x80 },
	{ 0x26, 0x80 },
	{ 0x24, 0x02 },
	{ 0x25, 0x0B },
	{ 0xBC, 0x02 },
	{ 0xBD, 0x24 },
	{ 0xBE, 0x02 },
};

/* enable audio */
static u8 misc_infoframe[] = {
	0xBF,
	0xC2, 0x84, 0x01, 0x0A, 0x6F, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* set HDMI, active */
static struct sii9022_i2c_addr_data regset2[] = {
	{ 0x1A, 0x01 },
	{ 0x3D, 0x00 },
	{ 0x3C, 0x02 },
};

static struct msm_fb_panel_data sii9022_panel_data = {
	.on = lcdc_sii9022_panel_on,
	.off = lcdc_sii9022_panel_off,
};

static struct platform_device sii9022_device = {
	.name   = DEVICE_NAME,
	.id	= 1,
	.dev	= {
		.platform_data = &sii9022_panel_data,
	}
};

static int send_i2c_data(struct i2c_client *client,
			 struct sii9022_i2c_addr_data *regset,
			 int size)
{
	int i;
	int rc = 0;

	for (i = 0; i < size; i++) {
		rc = i2c_smbus_write_byte_data(
			client,
			regset[i].addr, regset[i].data);
		if (rc)
			break;
	}
	return rc;
}

static void sii9022_work_f(struct work_struct *work)
{
	int isr;

	isr = i2c_smbus_read_byte_data(sii9022_i2c_client, SII9022_ISR);
	if (isr < 0) {
		dev_err(&sii9022_i2c_client->dev,
			"i2c read of isr failed rc = 0x%x\n", isr);
		return;
	}
	if (isr == 0)
		return;

	/* reset any set bits */
	i2c_smbus_write_byte_data(sii9022_i2c_client, SII9022_ISR, isr);
	dd->hdmi_attached = isr & SII9022_ISR_RXS_STATUS;
	if (dd->pd->cable_detect)
		dd->pd->cable_detect(dd->hdmi_attached);
	if (dd->hdmi_attached) {
		dd->x_res = 1280;
		dd->y_res = 720;
	} else {
		dd->x_res = sii9022_panel_data.panel_info.xres;
		dd->y_res = sii9022_panel_data.panel_info.yres;
	}
}

static irqreturn_t sii9022_interrupt(int irq, void *dev_id)
{
	struct sii9022_data *dd = dev_id;

	schedule_work(&dd->work);
	return IRQ_HANDLED;
}

static int hdmi_sii_enable(struct i2c_client *client)
{
	int rc;
	int retries = 10;
	int count;

	rc = i2c_smbus_write_byte_data(client, 0xC7, 0x00);
	if (rc)
		goto enable_exit;

	do {
		msleep(1);
		rc = i2c_smbus_read_byte_data(client, 0x1B);
	} while ((rc != SII9022_DEVICE_ID) && retries--);

	if (rc != SII9022_DEVICE_ID)
		return -ENODEV;

	rc = i2c_smbus_write_byte_data(client, 0x1A, 0x11);
	if (rc)
		goto enable_exit;

	count = ARRAY_SIZE(video_mode_data);
	rc = i2c_master_send(client, video_mode_data, count);
	if (rc != count) {
		rc = -EIO;
		goto enable_exit;
	}

	rc = i2c_smbus_write_byte_data(client, 0x08, 0x20);
	if (rc)
		goto enable_exit;
	count = ARRAY_SIZE(avi_io_format);
	rc = i2c_master_send(client, avi_io_format, count);
	if (rc != count) {
		rc = -EIO;
		goto enable_exit;
	}

	rc = send_i2c_data(client, regset0, ARRAY_SIZE(regset0));
	if (rc)
		goto enable_exit;

	count = ARRAY_SIZE(video_infoframe);
	rc = i2c_master_send(client, video_infoframe, count);
	if (rc != count) {
		rc = -EIO;
		goto enable_exit;
	}

	rc = send_i2c_data(client, regset1, ARRAY_SIZE(regset1));
	if (rc)
		goto enable_exit;

	count = ARRAY_SIZE(misc_infoframe);
	rc = i2c_master_send(client, misc_infoframe, count);
	if (rc != count) {
		rc = -EIO;
		goto enable_exit;
	}

	rc = send_i2c_data(client, regset2, ARRAY_SIZE(regset2));
	if (rc)
		goto enable_exit;

	return 0;
enable_exit:
	printk(KERN_ERR "%s: exited rc=%d\n", __func__, rc);
	return rc;
}

static ssize_t show_res(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%dx%d\n", dd->x_res, dd->y_res);
}

static struct device_attribute device_attrs[] = {
	__ATTR(screen_resolution, S_IRUGO|S_IWUSR, show_res, NULL),
};

static int lcdc_sii9022_panel_on(struct platform_device *pdev)
{
	int rc;
	if (!dd->sysfs_entry_created) {
		dd->pdev = pdev;
		rc = device_create_file(&pdev->dev, &device_attrs[0]);
		if (!rc)
			dd->sysfs_entry_created = 1;
	}

	rc = hdmi_sii_enable(sii9022_i2c_client);
	if (rc) {
		dd->hdmi_attached = 0;
		dd->x_res = sii9022_panel_data.panel_info.xres;
		dd->y_res = sii9022_panel_data.panel_info.yres;
	}
	if (dd->pd->irq)
		enable_irq(dd->pd->irq);
	/* Don't return the value from hdmi_sii_enable().
	 * It may fail on some ST1.5s, but we must return 0 from this
	 * function in order for the on-board display to turn on.
	 */
	return 0;
}

static int lcdc_sii9022_panel_off(struct platform_device *pdev)
{
	if (dd->pd->irq)
		disable_irq(dd->pd->irq);
	return 0;
}

static const struct i2c_device_id hmdi_sii_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};

static int hdmi_sii_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE | I2C_FUNC_I2C))
		return -ENODEV;

	dd = kzalloc(sizeof *dd, GFP_KERNEL);
	if (!dd) {
		rc = -ENOMEM;
		goto probe_exit;
	}
	sii9022_i2c_client = client;
	i2c_set_clientdata(client, dd);
	dd->pd = client->dev.platform_data;
	if (!dd->pd) {
		rc = -ENODEV;
		goto probe_free;
	}
	if (dd->pd->irq) {
		INIT_WORK(&dd->work, sii9022_work_f);
		rc = request_irq(dd->pd->irq,
				 &sii9022_interrupt,
				 IRQF_TRIGGER_FALLING,
				 "sii9022_cable", dd);
		if (rc)
			goto probe_free;
		disable_irq(dd->pd->irq);
	}
	msm_fb_add_device(&sii9022_device);
	dd->x_res = sii9022_panel_data.panel_info.xres;
	dd->y_res = sii9022_panel_data.panel_info.yres;

	return 0;

probe_free:
	i2c_set_clientdata(client, NULL);
	kfree(dd);
probe_exit:
	return rc;
}

static int __devexit hdmi_sii_remove(struct i2c_client *client)
{
	int err = 0 ;
	struct msm_hdmi_platform_data *pd;

	if (dd->sysfs_entry_created)
		device_remove_file(&dd->pdev->dev, &device_attrs[0]);
	pd = client->dev.platform_data;
	if (pd && pd->irq)
		free_irq(pd->irq, dd);
	i2c_set_clientdata(client, NULL);
	kfree(dd);

	return err ;
}

#ifdef CONFIG_PM
static int sii9022_suspend(struct device *dev)
{
	if (dd && dd->pd && dd->pd->irq)
		disable_irq(dd->pd->irq);
	return 0;
}

static int sii9022_resume(struct device *dev)
{
	if (dd && dd->pd && dd->pd->irq)
		enable_irq(dd->pd->irq);
	return 0;
}

static struct dev_pm_ops sii9022_pm_ops = {
	.suspend = sii9022_suspend,
	.resume = sii9022_resume,
};
#endif

static struct i2c_driver hdmi_sii_i2c_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm     = &sii9022_pm_ops,
#endif
	},
	.probe = hdmi_sii_probe,
	.remove =  __exit_p(hdmi_sii_remove),
	.id_table = hmdi_sii_id,
};

static int __init lcdc_st15_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	if (msm_fb_detect_client("lcdc_st15"))
		return 0;

	pinfo = &sii9022_panel_data.panel_info;
	pinfo->xres = 1366;
	pinfo->yres = 768;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 74250000;

	pinfo->lcdc.h_back_porch = 120;
	pinfo->lcdc.h_front_porch = 20;
	pinfo->lcdc.h_pulse_width = 40;
	pinfo->lcdc.v_back_porch = 25;
	pinfo->lcdc.v_front_porch = 1;
	pinfo->lcdc.v_pulse_width = 7;
	pinfo->lcdc.border_clr = 0;      /* blk */
	pinfo->lcdc.underflow_clr = 0xff;        /* blue */
	pinfo->lcdc.hsync_skew = 0;

	ret = i2c_add_driver(&hdmi_sii_i2c_driver);
	if (ret)
		printk(KERN_ERR "%s: failed to add i2c driver\n", __func__);

	return ret;
}

static void __exit hdmi_sii_exit(void)
{
	i2c_del_driver(&hdmi_sii_i2c_driver);
}

module_init(lcdc_st15_init);
module_exit(hdmi_sii_exit);
