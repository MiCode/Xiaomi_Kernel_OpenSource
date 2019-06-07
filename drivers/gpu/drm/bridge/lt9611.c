/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/component.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/hdmi.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_crtc_helper.h>

#define CFG_HPD_INTERRUPTS BIT(0)
#define CFG_EDID_INTERRUPTS BIT(1)
#define CFG_CEC_INTERRUPTS BIT(2)
#define CFG_VID_CHK_INTERRUPTS BIT(3)

#define EDID_SEG_SIZE 256
#define READ_BUF_MAX_SIZE 9
#define WRITE_BUF_MAX_SIZE 2
#define HPD_UEVENT_BUFFER_SIZE 30

struct lt9611_reg_cfg {
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct lt9611_vreg {
	struct regulator *vreg; /* vreg handle */
	char vreg_name[32];
	int min_voltage;
	int max_voltage;
	int enable_load;
	int disable_load;
	int pre_on_sleep;
	int post_on_sleep;
	int pre_off_sleep;
	int post_off_sleep;
};

struct lt9611_video_cfg {
	u32 h_active;
	u32 h_front_porch;
	u32 h_pulse_width;
	u32 h_back_porch;
	bool h_polarity;
	u32 v_active;
	u32 v_front_porch;
	u32 v_pulse_width;
	u32 v_back_porch;
	bool v_polarity;
	u32 pclk_khz;
	bool interlaced;
	u32 vic;
	enum hdmi_picture_aspect ar;
	u32 num_of_lanes;
	u32 num_of_intfs;
	u8 scaninfo;
};

struct lt9611 {
	struct device *dev;
	struct drm_bridge bridge;

	struct device_node *host_node;
	struct mipi_dsi_device *dsi;
	struct drm_connector connector;

	u8 i2c_addr;
	int irq;
	bool ac_mode;

	u32 irq_gpio;
	u32 reset_gpio;
	u32 hdmi_ps_gpio;
	u32 hdmi_en_gpio;

	unsigned int num_vreg;
	struct lt9611_vreg *vreg_config;

	struct i2c_client *i2c_client;

	enum drm_connector_status status;
	bool power_on;
	bool regulator_on;

	/* get display modes from device tree */
	bool non_pluggable;
	u32 num_of_modes;
	struct list_head mode_list;

	struct drm_display_mode curr_mode;
	struct lt9611_video_cfg video_cfg;

	struct workqueue_struct *wq;
	struct work_struct work;

	u8 edid_buf[EDID_SEG_SIZE];
	u8 i2c_wbuf[WRITE_BUF_MAX_SIZE];
	u8 i2c_rbuf[READ_BUF_MAX_SIZE];
	bool hdmi_mode;
};

static struct lt9611_reg_cfg lt9611_init_setup[] = {
	/* LT9611_System_Init */
	{0xFF, 0x81, 0},
	{0x01, 0x18, 0}, /* sel xtal clock */

	/* timer for frequency meter */
	{0xff, 0x82, 0},
	{0x1b, 0x69, 0}, /*timer 2*/
	{0x1c, 0x78, 0},
	{0xcb, 0x69, 0}, /*timer 1 */
	{0xcc, 0x78, 0},

	/* irq init */
	{0xff, 0x82, 0},
	{0x51, 0x01, 0},
	{0x58, 0x0a, 0}, /* hpd irq */
	{0x59, 0x80, 0}, /* hpd debounce width */
	{0x9e, 0xf7, 0}, /* video check irq */

	/* power consumption for work */
	{0xff, 0x80, 0},
	{0x04, 0xf0, 0},
	{0x06, 0xf0, 0},
	{0x0a, 0x80, 0},
	{0x0b, 0x40, 0},
	{0x0d, 0xef, 0},
	{0x11, 0xfa, 0},
};

struct lt9611_timing_info {
	u16 xres;
	u16 yres;
	u8 bpp;
	u8 fps;
	u8 lanes;
	u8 intfs;
};

static struct lt9611_timing_info lt9611_supp_timing_cfg[] = {
	{3840, 2160, 24, 30, 4, 2}, /* 3840x2160 24bit 30Hz 4Lane 2ports */
	{1920, 1080, 24, 60, 4, 1}, /* 1080P 24bit 60Hz 4lane 1port */
	{1920, 1080, 24, 30, 3, 1}, /* 1080P 24bit 30Hz 3lane 1port */
	{1920, 1080, 24, 24, 3, 1},
	{720, 480, 24, 60, 2, 1},
	{720, 576, 24, 50, 2, 1},
	{640, 480, 24, 60, 2, 1},
	{0xffff, 0xffff, 0xff, 0xff, 0xff},
};

static void lt9611_hpd_work(struct work_struct *work)
{
	struct drm_device *dev = NULL;
	char name[HPD_UEVENT_BUFFER_SIZE], status[HPD_UEVENT_BUFFER_SIZE];
	char *envp[5];
	struct lt9611 *pdata = container_of(work, struct lt9611, work);

	if (!pdata)
		return;

	dev = pdata->connector.dev;
	pdata->connector.status =
		pdata->connector.funcs->detect(&pdata->connector, true);

	scnprintf(name, HPD_UEVENT_BUFFER_SIZE, "name=%s",
		 pdata->connector.name);
	scnprintf(status, HPD_UEVENT_BUFFER_SIZE, "status=%s",
		drm_get_connector_status_name(pdata->connector.status));

	pr_debug("[%s]:[%s]\n", name, status);
	envp[0] = name;
	envp[1] = status;
	envp[2] = NULL;
	envp[3] = NULL;
	envp[4] = NULL;
	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
			envp);
}

static void lt9611_device_power_ctl(struct lt9611 *pdata, bool on_off);

static struct lt9611 *bridge_to_lt9611(struct drm_bridge *bridge)
{
	return container_of(bridge, struct lt9611, bridge);
}

static struct lt9611 *connector_to_lt9611(struct drm_connector *connector)
{
	return container_of(connector, struct lt9611, connector);
}

static int lt9611_write(struct lt9611 *pdata, u8 reg, u8 val)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = pdata->i2c_wbuf,
	};

	pdata->i2c_wbuf[0] = reg;
	pdata->i2c_wbuf[1] = val;

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		pr_err("i2c write failed\n");
		return -EIO;
	}

	return 0;
}

static int lt9611_read(struct lt9611 *pdata, u8 reg, char *buf, u32 size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = pdata->i2c_wbuf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = pdata->i2c_rbuf,
		}
	};

	pdata->i2c_wbuf[0] = reg;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		pr_err("i2c read failed\n");
		return -EIO;
	}

	memcpy(buf, pdata->i2c_rbuf, size);

	return 0;
}

static int lt9611_write_array(struct lt9611 *pdata,
	struct lt9611_reg_cfg *cfg, int size)
{
	int ret = 0;
	int i;

	size = size / sizeof(struct lt9611_reg_cfg);
	for (i = 0; i < size; i++) {
		ret = lt9611_write(pdata, cfg[i].reg, cfg[i].val);

		if (ret != 0) {
			pr_err("reg writes failed. Last write %02X to %02X\n",
				cfg[i].val, cfg[i].reg);
			goto w_regs_fail;
		}

		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}

w_regs_fail:
	if (ret != 0)
		pr_err("exiting with ret = %d after %d writes\n", ret, i);

	return ret;
}

static int lt9611_parse_dt_modes(struct device_node *np,
					struct list_head *head,
					u32 *num_of_modes)
{
	int rc = 0;
	struct drm_display_mode *mode;
	u32 mode_count = 0;
	struct device_node *node = NULL;
	struct device_node *root_node = NULL;
	u32 h_front_porch, h_pulse_width, h_back_porch;
	u32 v_front_porch, v_pulse_width, v_back_porch;
	bool h_active_high, v_active_high;
	u32 flags = 0;

	root_node = of_get_child_by_name(np, "lt,customize-modes");
	if (!root_node) {
		root_node = of_parse_phandle(np, "lt,customize-modes", 0);
		if (!root_node) {
			pr_info("No entry present for lt,customize-modes");
			goto end;
		}
	}

	for_each_child_of_node(root_node, node) {
		rc = 0;
		mode = kzalloc(sizeof(*mode), GFP_KERNEL);
		if (!mode) {
			pr_err("Out of memory\n");
			rc =  -ENOMEM;
			continue;
		}

		rc = of_property_read_u32(node, "lt,mode-h-active",
						&mode->hdisplay);
		if (rc) {
			pr_err("failed to read h-active, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-h-front-porch",
						&h_front_porch);
		if (rc) {
			pr_err("failed to read h-front-porch, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-h-pulse-width",
						&h_pulse_width);
		if (rc) {
			pr_err("failed to read h-pulse-width, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-h-back-porch",
						&h_back_porch);
		if (rc) {
			pr_err("failed to read h-back-porch, rc=%d\n", rc);
			goto fail;
		}

		h_active_high = of_property_read_bool(node,
						"lt,mode-h-active-high");

		rc = of_property_read_u32(node, "lt,mode-v-active",
						&mode->vdisplay);
		if (rc) {
			pr_err("failed to read v-active, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-v-front-porch",
						&v_front_porch);
		if (rc) {
			pr_err("failed to read v-front-porch, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-v-pulse-width",
						&v_pulse_width);
		if (rc) {
			pr_err("failed to read v-pulse-width, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-v-back-porch",
						&v_back_porch);
		if (rc) {
			pr_err("failed to read v-back-porch, rc=%d\n", rc);
			goto fail;
		}

		v_active_high = of_property_read_bool(node,
						"lt,mode-v-active-high");

		rc = of_property_read_u32(node, "lt,mode-refresh-rate",
						&mode->vrefresh);
		if (rc) {
			pr_err("failed to read refresh-rate, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-clock-in-khz",
						&mode->clock);
		if (rc) {
			pr_err("failed to read clock, rc=%d\n", rc);
			goto fail;
		}

		mode->hsync_start = mode->hdisplay + h_front_porch;
		mode->hsync_end = mode->hsync_start + h_pulse_width;
		mode->htotal = mode->hsync_end + h_back_porch;
		mode->vsync_start = mode->vdisplay + v_front_porch;
		mode->vsync_end = mode->vsync_start + v_pulse_width;
		mode->vtotal = mode->vsync_end + v_back_porch;
		if (h_active_high)
			flags |= DRM_MODE_FLAG_PHSYNC;
		else
			flags |= DRM_MODE_FLAG_NHSYNC;
		if (v_active_high)
			flags |= DRM_MODE_FLAG_PVSYNC;
		else
			flags |= DRM_MODE_FLAG_NVSYNC;
		mode->flags = flags;

		if (!rc) {
			mode_count++;
			list_add_tail(&mode->head, head);
		}

		drm_mode_set_name(mode);

		pr_debug("mode[%s] h[%d,%d,%d,%d] v[%d,%d,%d,%d] %d %x %dkHZ\n",
			mode->name, mode->hdisplay, mode->hsync_start,
			mode->hsync_end, mode->htotal, mode->vdisplay,
			mode->vsync_start, mode->vsync_end, mode->vtotal,
			mode->vrefresh, mode->flags, mode->clock);
fail:
		if (rc) {
			kfree(mode);
			continue;
		}
	}

	if (num_of_modes)
		*num_of_modes = mode_count;

end:
	return rc;
}


static int lt9611_parse_dt(struct device *dev,
	struct lt9611 *pdata)
{
	struct device_node *np = dev->of_node;
	struct device_node *end_node;
	int ret = 0;

	end_node = of_graph_get_endpoint_by_regs(dev->of_node, 0, 0);
	if (!end_node) {
		pr_err("remote endpoint not found\n");
		return -ENODEV;
	}

	pdata->host_node = of_graph_get_remote_port_parent(end_node);
	of_node_put(end_node);
	if (!pdata->host_node) {
		pr_err("remote node not found\n");
		return -ENODEV;
	}
	of_node_put(pdata->host_node);

	pdata->irq_gpio =
		of_get_named_gpio(np, "lt,irq-gpio", 0);
	if (!gpio_is_valid(pdata->irq_gpio)) {
		pr_err("irq gpio not specified\n");
		ret = -EINVAL;
	}
	pr_debug("irq_gpio=%d\n", pdata->irq_gpio);

	pdata->reset_gpio =
		of_get_named_gpio(np, "lt,reset-gpio", 0);
	if (!gpio_is_valid(pdata->reset_gpio)) {
		pr_err("reset gpio not specified\n");
		ret = -EINVAL;
	}
	pr_debug("reset_gpio=%d\n", pdata->reset_gpio);

	pdata->hdmi_ps_gpio =
		of_get_named_gpio(np, "lt,hdmi-ps-gpio", 0);
	if (!gpio_is_valid(pdata->hdmi_ps_gpio))
		pr_debug("hdmi ps gpio not specified\n");
	else
		pr_debug("hdmi_ps_gpio=%d\n", pdata->hdmi_ps_gpio);

	pdata->hdmi_en_gpio =
		of_get_named_gpio(np, "lt,hdmi-en-gpio", 0);
	if (!gpio_is_valid(pdata->hdmi_en_gpio))
		pr_debug("hdmi en gpio not specified\n");
	else
		pr_debug("hdmi_en_gpio=%d\n", pdata->hdmi_en_gpio);

	pdata->ac_mode = of_property_read_bool(np, "lt,ac-mode");
	pr_debug("ac_mode=%d\n", pdata->ac_mode);

	pdata->non_pluggable = of_property_read_bool(np, "lt,non-pluggable");
	pr_debug("non_pluggable = %d\n", pdata->non_pluggable);
	if (pdata->non_pluggable) {
		INIT_LIST_HEAD(&pdata->mode_list);
		ret = lt9611_parse_dt_modes(np,
			&pdata->mode_list, &pdata->num_of_modes);
	}

	return ret;
}

static int lt9611_gpio_configure(struct lt9611 *pdata, bool on)
{
	int ret = 0;

	if (on) {
		ret = gpio_request(pdata->reset_gpio,
			"lt9611-reset-gpio");
		if (ret) {
			pr_err("lt9611 reset gpio request failed\n");
			goto error;
		}

		ret = gpio_direction_output(pdata->reset_gpio, 0);
		if (ret) {
			pr_err("lt9611 reset gpio direction failed\n");
			goto reset_error;
		}

		if (gpio_is_valid(pdata->hdmi_en_gpio)) {
			ret = gpio_request(pdata->hdmi_en_gpio,
					"lt9611-hdmi-en-gpio");
			if (ret) {
				pr_err("lt9611 hdmi en gpio request failed\n");
				goto reset_error;
			}

			ret = gpio_direction_output(pdata->hdmi_en_gpio, 1);
			if (ret) {
				pr_err("lt9611 hdmi en gpio direction failed\n");
				goto hdmi_en_error;
			}
		}

		if (gpio_is_valid(pdata->hdmi_ps_gpio)) {
			ret = gpio_request(pdata->hdmi_ps_gpio,
				"lt9611-hdmi-ps-gpio");
			if (ret) {
				pr_err("lt9611 hdmi ps gpio request failed\n");
				goto hdmi_en_error;
			}

			ret = gpio_direction_input(pdata->hdmi_ps_gpio);
			if (ret) {
				pr_err("lt9611 hdmi ps gpio direction failed\n");
				goto hdmi_ps_error;
			}
		}

		ret = gpio_request(pdata->irq_gpio, "lt9611-irq-gpio");
		if (ret) {
			pr_err("lt9611 irq gpio request failed\n");
			goto hdmi_ps_error;
		}

		ret = gpio_direction_input(pdata->irq_gpio);
		if (ret) {
			pr_err("lt9611 irq gpio direction failed\n");
			goto irq_error;
		}
	} else {
		gpio_free(pdata->irq_gpio);
		if (gpio_is_valid(pdata->hdmi_ps_gpio))
			gpio_free(pdata->hdmi_ps_gpio);
		if (gpio_is_valid(pdata->hdmi_en_gpio))
			gpio_free(pdata->hdmi_en_gpio);
		gpio_free(pdata->reset_gpio);
	}

	return ret;


irq_error:
	gpio_free(pdata->irq_gpio);
hdmi_ps_error:
	if (gpio_is_valid(pdata->hdmi_ps_gpio))
		gpio_free(pdata->hdmi_ps_gpio);
hdmi_en_error:
	if (gpio_is_valid(pdata->hdmi_en_gpio))
		gpio_free(pdata->hdmi_en_gpio);
reset_error:
	gpio_free(pdata->reset_gpio);
error:
	return ret;
}

static int lt9611_read_device_rev(struct lt9611 *pdata)
{
	u8 rev = 0;
	int ret = 0;

	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0xee, 0x01);

	ret = lt9611_read(pdata, 0x02, &rev, 1);

	if (ret == 0)
		pr_info("LT9611 revision: 0x%x\n", rev);

	return ret;
}

static int lt9611_mipi_input_analog(struct lt9611 *pdata,
		struct lt9611_video_cfg *cfg)
{
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xff, 0x81, 0},
		{0x06, 0x40, 0}, /*port A rx current*/
		{0x0a, 0xfe, 0}, /*port A ldo voltage set*/
		{0x0b, 0xbf, 0}, /*enable port A lprx*/
		{0x11, 0x40, 0}, /*port B rx current*/
		{0x15, 0xfe, 0}, /*port B ldo voltage set*/
		{0x16, 0xbf, 0}, /*enable port B lprx*/

		{0x1c, 0x03, 0}, /*PortA clk lane no-LP mode*/
		{0x20, 0x03, 0}, /*PortB clk lane with-LP mode*/
	};

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	lt9611_write_array(pdata, reg_cfg, sizeof(reg_cfg));

	return 0;
}

static int lt9611_mipi_input_digital(struct lt9611 *pdata,
	struct lt9611_video_cfg *cfg)
{
	u8 lanes = 0;
	u8 ports = 0;
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xff, 0x82, 0},
		{0x4f, 0x80, 0},
		{0x50, 0x10, 0},
		{0xff, 0x83, 0},

		{0x02, 0x0a, 0},
		{0x06, 0x0a, 0},
	};

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	lanes = cfg->num_of_lanes;
	ports = cfg->num_of_intfs;

	lt9611_write(pdata, 0xff, 0x83);
	if (lanes == 4)
		lt9611_write(pdata, 0x00, 0x00);
	else if (lanes < 4)
		lt9611_write(pdata, 0x00, lanes);
	else {
		pr_err("invalid lane count\n");
		return -EINVAL;
	}

	if (ports == 1)
		lt9611_write(pdata, 0x0a, 0x00);
	else if (ports == 2)
		lt9611_write(pdata, 0x0a, 0x03);
	else {
		pr_err("invalid port count\n");
		return -EINVAL;
	}

	lt9611_write_array(pdata, reg_cfg, sizeof(reg_cfg));

	return 0;
}

static void lt9611_mipi_video_setup(struct lt9611 *pdata,
	struct lt9611_video_cfg *cfg)
{
	u32 h_total, h_act, hpw, hfp, hss;
	u32 v_total, v_act, vpw, vfp, vss;

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return;
	}

	h_total = cfg->h_active + cfg->h_front_porch +
	      cfg->h_pulse_width + cfg->h_back_porch;
	v_total = cfg->v_active + cfg->v_front_porch +
	      cfg->v_pulse_width + cfg->v_back_porch;

	h_act = cfg->h_active;
	hpw = cfg->h_pulse_width;
	hfp = cfg->h_front_porch;
	hss = cfg->h_pulse_width + cfg->h_back_porch;

	v_act = cfg->v_active;
	vpw = cfg->v_pulse_width;
	vfp = cfg->v_front_porch;
	vss = cfg->v_pulse_width + cfg->v_back_porch;

	pr_debug("h_total=%d, h_active=%d, hfp=%d, hpw=%d, hbp=%d\n",
		h_total, cfg->h_active, cfg->h_front_porch,
		cfg->h_pulse_width, cfg->h_back_porch);

	pr_debug("v_total=%d, v_active=%d, vfp=%d, vpw=%d, vbp=%d\n",
		v_total, cfg->v_active, cfg->v_front_porch,
		cfg->v_pulse_width, cfg->v_back_porch);

	lt9611_write(pdata, 0xff, 0x83);

	lt9611_write(pdata, 0x0d, (u8)(v_total / 256));
	lt9611_write(pdata, 0x0e, (u8)(v_total % 256));

	lt9611_write(pdata, 0x0f, (u8)(v_act / 256));
	lt9611_write(pdata, 0x10, (u8)(v_act % 256));

	lt9611_write(pdata, 0x11, (u8)(h_total / 256));
	lt9611_write(pdata, 0x12, (u8)(h_total % 256));

	lt9611_write(pdata, 0x13, (u8)(h_act / 256));
	lt9611_write(pdata, 0x14, (u8)(h_act % 256));

	lt9611_write(pdata, 0x15, (u8)(vpw % 256));
	lt9611_write(pdata, 0x16, (u8)(hpw % 256));

	lt9611_write(pdata, 0x17, (u8)(vfp % 256));

	lt9611_write(pdata, 0x18, (u8)(vss % 256));

	lt9611_write(pdata, 0x19, (u8)(hfp % 256));

	lt9611_write(pdata, 0x1a, (u8)(hss / 256));
	lt9611_write(pdata, 0x1b, (u8)(hss % 256));
}

static int lt9611_pcr_setup(struct lt9611 *pdata,
		struct lt9611_video_cfg *cfg)
{
	u32 h_act = 0;
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xff, 0x83, 0},
		{0x0b, 0x01, 0},
		{0x0c, 0x10, 0},
		{0x48, 0x00, 0},
		{0x49, 0x81, 0},

		/* stage 1 */
		{0x21, 0x4a, 0},
		{0x24, 0x71, 0},
		{0x25, 0x30, 0},
		{0x2a, 0x01, 0},

		/* stage 2 */
		{0x4a, 0x40, 0},
		{0x1d, 0x10, 0},

		/* MK limit */
		{0x2d, 0x38, 0},
		{0x31, 0x08, 0},
	};

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	lt9611_write_array(pdata, reg_cfg, sizeof(reg_cfg));

	h_act = cfg->h_active;

	if (h_act == 1920) {
		lt9611_write(pdata, 0x26, 0x37);
	} else if (h_act == 3840) {
		lt9611_write(pdata, 0x0b, 0x03);
		lt9611_write(pdata, 0x0c, 0xd0);
		lt9611_write(pdata, 0x48, 0x03);
		lt9611_write(pdata, 0x49, 0xe0);
		lt9611_write(pdata, 0x24, 0x72);
		lt9611_write(pdata, 0x25, 0x00);
		lt9611_write(pdata, 0x2a, 0x01);
		lt9611_write(pdata, 0x4a, 0x10);
		lt9611_write(pdata, 0x1d, 0x10);
		lt9611_write(pdata, 0x26, 0x37);
	} else if (h_act == 640) {
		lt9611_write(pdata, 0x26, 0x14);
	}

	/* pcr rst */
	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0x11, 0x5a);
	lt9611_write(pdata, 0x11, 0xfa);

	return 0;
}

static int lt9611_pll_setup(struct lt9611 *pdata,
		struct lt9611_video_cfg *cfg)
{
	u32 pclk = 0;
	struct lt9611_reg_cfg reg_cfg[] = {
		/* txpll init */
		{0xff, 0x81, 0},
		{0x23, 0x40, 0},
		{0x24, 0x64, 0},
		{0x25, 0x80, 0},
		{0x26, 0x55, 0},
		{0x2c, 0x37, 0},
		{0x2f, 0x01, 0},
		{0x26, 0x55, 0},
		{0x27, 0x66, 0},
		{0x28, 0x88, 0},
	};

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	pclk = cfg->pclk_khz;

	lt9611_write_array(pdata, reg_cfg, sizeof(reg_cfg));

	if (pclk > 150000)
		lt9611_write(pdata, 0x2d, 0x88);
	else if (pclk > 70000)
		lt9611_write(pdata, 0x2d, 0x99);
	else
		lt9611_write(pdata, 0x2d, 0xaa);

	lt9611_write(pdata, 0xff, 0x82);
	pclk = pclk / 2;
	lt9611_write(pdata, 0xe3, pclk/65536); /* pclk[19:16] */
	pclk = pclk % 65536;
	lt9611_write(pdata, 0xe4, pclk/256);   /* pclk[15:8]  */
	lt9611_write(pdata, 0xe5, pclk%256);   /* pclk[7:0]   */

	lt9611_write(pdata, 0xde, 0x20);
	lt9611_write(pdata, 0xde, 0xe0);

	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0x16, 0xf1);
	lt9611_write(pdata, 0x16, 0xf3);

	return 0;
}

static int lt9611_video_check(struct lt9611 *pdata)
{
	int ret = 0;
	u32 v_total, v_act, h_act_a, h_act_b, h_total_sysclk;
	u8 temp = 0;

	if (!pdata) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	/* top module video check */
	lt9611_write(pdata, 0xff, 0x82);

	/* v_act */
	ret = lt9611_read(pdata, 0x82, &temp, 1);
	if (ret)
		goto end;

	v_act = temp << 8;
	ret = lt9611_read(pdata, 0x83, &temp, 1);
	if (ret)
		goto end;
	v_act = v_act + temp;

	/* v_total */
	ret = lt9611_read(pdata, 0x6c, &temp, 1);
	if (ret)
		goto end;
	v_total = temp << 8;
	ret = lt9611_read(pdata, 0x6d, &temp, 1);
	if (ret)
		goto end;
	v_total = v_total + temp;

	/* h_total_sysclk */
	ret = lt9611_read(pdata, 0x86, &temp, 1);
	if (ret)
		goto end;
	h_total_sysclk = temp << 8;
	ret = lt9611_read(pdata, 0x87, &temp, 1);
	if (ret)
		goto end;
	h_total_sysclk = h_total_sysclk + temp;

	/* h_act_a */
	lt9611_write(pdata, 0xff, 0x83);
	ret = lt9611_read(pdata, 0x82, &temp, 1);
	if (ret)
		goto end;
	h_act_a = temp << 8;
	ret = lt9611_read(pdata, 0x83, &temp, 1);
	if (ret)
		goto end;
	h_act_a = (h_act_a + temp)/3;

	/* h_act_b */
	lt9611_write(pdata, 0xff, 0x83);
	ret = lt9611_read(pdata, 0x86, &temp, 1);
	if (ret)
		goto end;
	h_act_b = temp << 8;
	ret = lt9611_read(pdata, 0x87, &temp, 1);
	if (ret)
		goto end;
	h_act_b = (h_act_b + temp)/3;

	pr_info("video check: h_act_a=%d, h_act_b=%d, v_act=%d, v_total=%d, h_total_sysclk=%d\n",
		h_act_a, h_act_b, v_act, v_total, h_total_sysclk);

	return 0;

end:
	pr_err("read video check error\n");
	return ret;
}

static int lt9611_hdmi_tx_digital(struct lt9611 *pdata,
		struct lt9611_video_cfg *cfg)
{
	int ret = -EINVAL;
	u32 checksum, vic;

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return ret;
	}

	vic = cfg->vic;
	checksum = 0x46 - vic;

	lt9611_write(pdata, 0xff, 0x84);
	lt9611_write(pdata, 0x43, checksum);
	lt9611_write(pdata, 0x44, 0x84);
	lt9611_write(pdata, 0x47, vic);

	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0xd6, 0x8c);
	lt9611_write(pdata, 0xd7, 0x04);

	return ret;
}

static int lt9611_hdmi_tx_phy(struct lt9611 *pdata,
		struct lt9611_video_cfg *cfg)
{
	int ret = -EINVAL;
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xff, 0x81, 0},
		{0x30, 0x6a, 0},
		{0x31, 0x44, 0}, /* HDMI DC mode */
		{0x32, 0x4a, 0},
		{0x33, 0x0b, 0},
		{0x34, 0x00, 0},
		{0x35, 0x00, 0},
		{0x36, 0x00, 0},
		{0x37, 0x44, 0},
		{0x3f, 0x0f, 0},
		{0x40, 0xa0, 0},
		{0x41, 0xa0, 0},
		{0x42, 0xa0, 0},
		{0x43, 0xa0, 0},
		{0x44, 0x0a, 0},
	};

	if (!pdata || !cfg) {
		pr_err("invalid input\n");
		return ret;
	}

	/* HDMI AC mode */
	if (pdata->ac_mode)
		reg_cfg[2].val = 0x73;

	lt9611_write_array(pdata, reg_cfg, sizeof(reg_cfg));

	return ret;
}

static void lt9611_hdmi_output_enable(struct lt9611 *pdata)
{
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x30, 0xea);
}

static void lt9611_hdmi_output_disable(struct lt9611 *pdata)
{
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x30, 0x6a);
}

static irqreturn_t lt9611_irq_thread_handler(int irq, void *dev_id)
{
	struct lt9611 *pdata = dev_id;
	u8 irq_flag0 = 0;
	u8 irq_flag3 = 0;

	lt9611_write(pdata, 0xff, 0x82);
	lt9611_read(pdata, 0x0f, &irq_flag3, 1);
	lt9611_read(pdata, 0x0c, &irq_flag0, 1);

	 /* hpd changed low */
	if (irq_flag3 & BIT(7)) {
		pr_info("hdmi cable disconnected\n");

		lt9611_write(pdata, 0xff, 0x82); /* irq 3 clear flag */
		lt9611_write(pdata, 0x07, 0xbf);
		lt9611_write(pdata, 0x07, 0x3f);
	}
	 /* hpd changed high */
	if (irq_flag3 & BIT(6)) {
		pr_info("hdmi cable connected\n");

		lt9611_write(pdata, 0xff, 0x82); /* irq 3 clear flag */
		lt9611_write(pdata, 0x07, 0x7f);
		lt9611_write(pdata, 0x07, 0x3f);
	}

	/* video input changed */
	if (irq_flag0 & BIT(0)) {
		pr_info("video input changed\n");
		lt9611_write(pdata, 0xff, 0x82); /* irq 0 clear flag */
		lt9611_write(pdata, 0x9e, 0xff);
		lt9611_write(pdata, 0x9e, 0xf7);
		lt9611_write(pdata, 0x04, 0xff);
		lt9611_write(pdata, 0x04, 0xfe);
	}

	if (irq_flag3 & (BIT(6) | BIT(7)))
		queue_work(pdata->wq, &pdata->work);

	return IRQ_HANDLED;
}

static int lt9611_enable_interrupts(struct lt9611 *pdata, int interrupts)
{
	int ret = 0;
	u8 reg_val = 0;
	u8 init_reg_val;

	if (!pdata) {
		pr_err("invalid input\n");
		goto end;
	}

	if (interrupts & CFG_VID_CHK_INTERRUPTS) {
		lt9611_write(pdata, 0xff, 0x82);
		lt9611_read(pdata, 0x00, &reg_val, 1);

		if (reg_val & 0x01) {
			init_reg_val = reg_val & 0xfe;
			pr_debug("enabling video check interrupts\n");
			lt9611_write(pdata, 0x00, init_reg_val);
		}
		lt9611_write(pdata, 0x04, 0xff); /* clear */
		lt9611_write(pdata, 0x04, 0xfe);
	}

	if (interrupts & CFG_HPD_INTERRUPTS) {
		lt9611_write(pdata, 0xff, 0x82);
		lt9611_read(pdata, 0x03, &reg_val, 1);

		if (reg_val & 0xc0) { //reg_val | 0xc0???
			init_reg_val = reg_val & 0x3f;
			pr_debug("enabling hpd interrupts\n");
			lt9611_write(pdata, 0x03, init_reg_val);
		}

		lt9611_write(pdata, 0x07, 0xff); //clear
		lt9611_write(pdata, 0x07, 0x3f);
	}

end:
	return ret;
}

static void lt9611_pcr_mk_debug(struct lt9611 *pdata)
{
	u8 m = 0, k1 = 0, k2 = 0, k3 = 0;

	lt9611_write(pdata, 0xff, 0x83);
	lt9611_read(pdata, 0xb4, &m, 1);
	lt9611_read(pdata, 0xb5, &k1, 1);
	lt9611_read(pdata, 0xb6, &k2, 1);
	lt9611_read(pdata, 0xb7, &k3, 1);

	pr_info("pcr mk:0x%x 0x%x 0x%x 0x%x\n",
			m, k1, k2, k3);
}

static void lt9611_sleep_setup(struct lt9611 *pdata)
{
	struct lt9611_reg_cfg sleep_setup[] = {
		{0xff, 0x80, 0}, //register I2C addr
		{0x24, 0x76, 0},
		{0x23, 0x01, 0},
		{0xff, 0x81, 0}, //set addr pin as output
		{0x57, 0x03, 0},
		{0x49, 0x0b, 0},
		{0xff, 0x81, 0}, //anlog power down
		{0x51, 0x30, 0}, //disable IRQ
		{0x02, 0x48, 0}, //MIPI Rx power down
		{0x23, 0x80, 0},
		{0x30, 0x00, 0},
		{0x00, 0x01, 0}, //bandgap power down
		{0x01, 0x00, 0}, //system clk power down
	};

	pr_err("sleep\n");

	lt9611_write_array(pdata, sleep_setup, sizeof(sleep_setup));
}

static int lt9611_power_on(struct lt9611 *pdata, bool on)
{
	int ret = 0;

	pr_debug("power_on: on=%d\n", on);

	if (on && !pdata->power_on) {
		lt9611_write_array(pdata, lt9611_init_setup,
			sizeof(lt9611_init_setup));

		ret = lt9611_enable_interrupts(pdata, CFG_HPD_INTERRUPTS);
		if (ret) {
			pr_err("Failed to enable HPD intr %d\n", ret);
			return ret;
		}
		pdata->power_on = true;
	} else if (!on) {
		lt9611_write(pdata, 0xff, 0x81);
		lt9611_write(pdata, 0x30, 0x6a);

		pdata->power_on = false;
	}

	return ret;
}

static int lt9611_video_on(struct lt9611 *pdata, bool on)
{
	int ret = 0;
	struct lt9611_video_cfg *cfg = &pdata->video_cfg;

	pr_debug("on=%d\n", on);

	if (on) {
		lt9611_mipi_input_analog(pdata, cfg);
		lt9611_mipi_input_digital(pdata, cfg);
		lt9611_pll_setup(pdata, cfg);
		lt9611_mipi_video_setup(pdata, cfg);
		lt9611_pcr_setup(pdata, cfg);
		lt9611_hdmi_tx_digital(pdata, cfg);
		lt9611_hdmi_tx_phy(pdata, cfg);

		msleep(500);

		lt9611_video_check(pdata);
		lt9611_hdmi_output_enable(pdata);
	} else {
		lt9611_hdmi_output_disable(pdata);
	}

	return ret;
}

static void lt9611_mipi_byte_clk_debug(struct lt9611 *pdata)
{
	u8 reg_val = 0;
	u32 byte_clk;

	/* port A byte clk meter */
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0xc7, 0x03); /* port A */
	msleep(50);
	lt9611_read(pdata, 0xcd, &reg_val, 1);

	if ((reg_val & 0x60) == 0x60) {
		byte_clk =  (reg_val & 0x0f) * 65536;
		lt9611_read(pdata, 0xce, &reg_val, 1);
		byte_clk = byte_clk + reg_val * 256;
		lt9611_read(pdata, 0xcf, &reg_val, 1);
		byte_clk = byte_clk + reg_val;

		pr_info("port A byte clk = %d khz,\n", byte_clk);
	} else
		pr_info("port A byte clk unstable\n");

	/* port B byte clk meter */
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0xc7, 0x04); /* port B */
	msleep(50);
	lt9611_read(pdata, 0xcd, &reg_val, 1);

	if ((reg_val & 0x60) == 0x60) {
		byte_clk =  (reg_val & 0x0f) * 65536;
		lt9611_read(pdata, 0xce, &reg_val, 1);
		byte_clk = byte_clk + reg_val * 256;
		lt9611_read(pdata, 0xcf, &reg_val, 1);
		byte_clk = byte_clk + reg_val;

		pr_info("port B byte clk = %d khz,\n", byte_clk);
	} else
		pr_info("port B byte clk unstable\n");
}

static void lt9611_reset(struct lt9611 *pdata, bool on_off)
{
	if (on_off) {
		gpio_set_value(pdata->reset_gpio, 1);
		msleep(20);
		gpio_set_value(pdata->reset_gpio, 0);
		msleep(20);
		gpio_set_value(pdata->reset_gpio, 1);
		msleep(20);
	} else
		gpio_set_value(pdata->reset_gpio, 0);
}

static void lt9611_assert_5v(struct lt9611 *pdata, bool on_off)
{
	if (!gpio_is_valid(pdata->hdmi_en_gpio))
		return;

	if (on_off)
		gpio_set_value(pdata->hdmi_en_gpio, 1);
	else
		gpio_set_value(pdata->hdmi_en_gpio, 0);

	msleep(20);
}

static int lt9611_config_vreg(struct device *dev,
	struct lt9611_vreg *in_vreg, int num_vreg, bool config)
{
	int i = 0, rc = 0;
	struct lt9611_vreg *curr_vreg = NULL;

	if (!in_vreg || !num_vreg)
		return rc;

	if (config) {
		for (i = 0; i < num_vreg; i++) {
			curr_vreg = &in_vreg[i];
			curr_vreg->vreg = regulator_get(dev,
					curr_vreg->vreg_name);
			rc = PTR_RET(curr_vreg->vreg);
			if (rc) {
				pr_err("%s get failed. rc=%d\n",
						curr_vreg->vreg_name, rc);
				curr_vreg->vreg = NULL;
				goto vreg_get_fail;
			}

			rc = regulator_set_voltage(
					curr_vreg->vreg,
					curr_vreg->min_voltage,
					curr_vreg->max_voltage);
			if (rc < 0) {
				pr_err("%s set vltg fail\n",
						curr_vreg->vreg_name);
				goto vreg_set_voltage_fail;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			curr_vreg = &in_vreg[i];
			if (curr_vreg->vreg) {
				regulator_set_voltage(curr_vreg->vreg,
						0, curr_vreg->max_voltage);

				regulator_put(curr_vreg->vreg);
				curr_vreg->vreg = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
	regulator_set_load(curr_vreg->vreg, 0);

vreg_set_voltage_fail:
	regulator_put(curr_vreg->vreg);
	curr_vreg->vreg = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		curr_vreg = &in_vreg[i];
		goto vreg_unconfig;
	}
	return rc;
}

static int lt9611_get_dt_supply(struct device *dev,
		struct lt9611 *pdata)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_root_node = NULL;
	struct device_node *supply_node = NULL;

	if (!dev || !pdata) {
		pr_err("invalid input param dev:%pK pdata:%pK\n", dev, pdata);
		return -EINVAL;
	}

	of_node = dev->of_node;

	pdata->num_vreg = 0;
	supply_root_node = of_get_child_by_name(of_node,
			"lt,supply-entries");
	if (!supply_root_node) {
		pr_info("no supply entry present\n");
		return 0;
	}

	pdata->num_vreg = of_get_available_child_count(supply_root_node);
	if (pdata->num_vreg == 0) {
		pr_info("no vreg present\n");
		return 0;
	}

	pr_debug("vreg found. count=%d\n", pdata->num_vreg);
	pdata->vreg_config = devm_kzalloc(dev, sizeof(struct lt9611_vreg) *
			pdata->num_vreg, GFP_KERNEL);
	if (!pdata->vreg_config)
		return -ENOMEM;

	for_each_available_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;

		rc = of_property_read_string(supply_node,
				"lt,supply-name", &st);
		if (rc) {
			pr_err("error reading name. rc=%d\n", rc);
			goto error;
		}

		strlcpy(pdata->vreg_config[i].vreg_name, st,
				sizeof(pdata->vreg_config[i].vreg_name));

		rc = of_property_read_u32(supply_node,
				"lt,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("error reading min volt. rc=%d\n", rc);
			goto error;
		}
		pdata->vreg_config[i].min_voltage = tmp;

		rc = of_property_read_u32(supply_node,
				"lt,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("error reading max volt. rc=%d\n", rc);
			goto error;
		}
		pdata->vreg_config[i].max_voltage = tmp;

		rc = of_property_read_u32(supply_node,
				"lt,supply-enable-load", &tmp);
		if (rc)
			pr_debug("no supply enable load value. rc=%d\n", rc);

		pdata->vreg_config[i].enable_load = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-disable-load", &tmp);
		if (rc)
			pr_debug("no supply disable load value. rc=%d\n", rc);

		pdata->vreg_config[i].disable_load = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-pre-on-sleep", &tmp);
		if (rc)
			pr_debug("no supply pre on sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].pre_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-pre-off-sleep", &tmp);
		if (rc)
			pr_debug("no supply pre off sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].pre_off_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-post-on-sleep", &tmp);
		if (rc)
			pr_debug("no supply post on sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].post_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-post-off-sleep", &tmp);
		if (rc)
			pr_debug("no supply post off sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].post_off_sleep = (!rc ? tmp : 0);

		pr_debug("%s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
				pdata->vreg_config[i].vreg_name,
				pdata->vreg_config[i].min_voltage,
				pdata->vreg_config[i].max_voltage,
				pdata->vreg_config[i].enable_load,
				pdata->vreg_config[i].disable_load,
				pdata->vreg_config[i].pre_on_sleep,
				pdata->vreg_config[i].post_on_sleep,
				pdata->vreg_config[i].pre_off_sleep,
				pdata->vreg_config[i].post_off_sleep);
		++i;

		rc = 0;
	}

	rc = lt9611_config_vreg(dev,
			pdata->vreg_config, pdata->num_vreg, true);
	if (rc)
		goto error;

	return rc;

error:
	if (pdata->vreg_config) {
		devm_kfree(dev, pdata->vreg_config);
		pdata->vreg_config = NULL;
		pdata->num_vreg = 0;
	}

	return rc;
}

static void lt9611_put_dt_supply(struct device *dev,
		struct lt9611 *pdata)
{
	if (!dev || !pdata) {
		pr_err("invalid input param dev:%pK pdata:%pK\n", dev, pdata);
		return;
	}

	lt9611_config_vreg(dev,
			pdata->vreg_config, pdata->num_vreg, false);

	if (pdata->vreg_config) {
		devm_kfree(dev, pdata->vreg_config);
		pdata->vreg_config = NULL;
	}
	pdata->num_vreg = 0;
}

static int lt9611_enable_vreg(struct lt9611 *pdata, int enable)
{
	int i = 0, rc = 0;
	bool need_sleep;
	struct lt9611_vreg *in_vreg = pdata->vreg_config;
	int num_vreg = pdata->num_vreg;

	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			rc = PTR_RET(in_vreg[i].vreg);
			if (rc) {
				pr_err("%s regulator error. rc=%d\n",
						in_vreg[i].vreg_name, rc);
				goto vreg_set_opt_mode_fail;
			}

			need_sleep = !regulator_is_enabled(in_vreg[i].vreg);
			if (in_vreg[i].pre_on_sleep && need_sleep)
				usleep_range(in_vreg[i].pre_on_sleep * 1000,
						in_vreg[i].pre_on_sleep * 1000);

			rc = regulator_set_load(in_vreg[i].vreg,
					in_vreg[i].enable_load);
			if (rc < 0) {
				pr_err("%s set opt m fail\n",
						in_vreg[i].vreg_name);
				goto vreg_set_opt_mode_fail;
			}

			rc = regulator_enable(in_vreg[i].vreg);
			if (in_vreg[i].post_on_sleep && need_sleep)
				usleep_range(in_vreg[i].post_on_sleep * 1000,
					in_vreg[i].post_on_sleep * 1000);
			if (rc < 0) {
				pr_err("%s enable failed\n",
						in_vreg[i].vreg_name);
				goto disable_vreg;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			if (in_vreg[i].pre_off_sleep)
				usleep_range(in_vreg[i].pre_off_sleep * 1000,
					in_vreg[i].pre_off_sleep * 1000);

			regulator_set_load(in_vreg[i].vreg,
					in_vreg[i].disable_load);
			regulator_disable(in_vreg[i].vreg);

			if (in_vreg[i].post_off_sleep)
				usleep_range(in_vreg[i].post_off_sleep * 1000,
					in_vreg[i].post_off_sleep * 1000);
		}
	}
	return rc;

disable_vreg:
	regulator_set_load(in_vreg[i].vreg, in_vreg[i].disable_load);

vreg_set_opt_mode_fail:
	for (i--; i >= 0; i--) {
		if (in_vreg[i].pre_off_sleep)
			usleep_range(in_vreg[i].pre_off_sleep * 1000,
					in_vreg[i].pre_off_sleep * 1000);

		regulator_set_load(in_vreg[i].vreg,
				in_vreg[i].disable_load);
		regulator_disable(in_vreg[i].vreg);

		if (in_vreg[i].post_off_sleep)
			usleep_range(in_vreg[i].post_off_sleep * 1000,
					in_vreg[i].post_off_sleep * 1000);
	}

	return rc;
}

static struct lt9611_timing_info *lt9611_get_supported_timing(
		struct drm_display_mode *mode)
{
	int i = 0;

	while (lt9611_supp_timing_cfg[i].xres != 0xffff) {
		if (lt9611_supp_timing_cfg[i].xres == mode->hdisplay &&
			lt9611_supp_timing_cfg[i].yres == mode->vdisplay &&
			lt9611_supp_timing_cfg[i].fps ==
					drm_mode_vrefresh(mode)) {
			return &lt9611_supp_timing_cfg[i];
		}
		i++;
	}

	return NULL;
}

/* TODO: intf/lane number needs info from both DSI host and client */
static int lt9611_get_intf_num(struct lt9611 *pdata,
	struct drm_display_mode *mode)
{
	int num_of_intfs = 0;
	struct lt9611_timing_info *timing =
			lt9611_get_supported_timing(mode);

	if (timing)
		num_of_intfs = timing->intfs;
	else {
		pr_err("interface number not defined by bridge chip\n");
		num_of_intfs = 0;
	}

	return num_of_intfs;
}

static int lt9611_get_lane_num(struct lt9611 *pdata,
	struct drm_display_mode *mode)
{
	int num_of_lanes = 0;
	struct lt9611_timing_info *timing =
				lt9611_get_supported_timing(mode);

	if (timing)
		num_of_lanes = timing->lanes;
	else {
		pr_err("lane number not defined by bridge chip\n");
		num_of_lanes = 0;
	}

	return num_of_lanes;
}

static void lt9611_get_video_cfg(struct lt9611 *pdata,
	struct drm_display_mode *mode,
	struct lt9611_video_cfg *video_cfg)
{
	int rc = 0;
	struct hdmi_avi_infoframe avi_frame;

	memset(&avi_frame, 0, sizeof(avi_frame));

	video_cfg->h_active = mode->hdisplay;
	video_cfg->v_active = mode->vdisplay;
	video_cfg->h_front_porch = mode->hsync_start - mode->hdisplay;
	video_cfg->v_front_porch = mode->vsync_start - mode->vdisplay;
	video_cfg->h_back_porch = mode->htotal - mode->hsync_end;
	video_cfg->v_back_porch = mode->vtotal - mode->vsync_end;
	video_cfg->h_pulse_width = mode->hsync_end - mode->hsync_start;
	video_cfg->v_pulse_width = mode->vsync_end - mode->vsync_start;
	video_cfg->pclk_khz = mode->clock;

	video_cfg->h_polarity = !!(mode->flags & DRM_MODE_FLAG_PHSYNC);
	video_cfg->v_polarity = !!(mode->flags & DRM_MODE_FLAG_PVSYNC);

	video_cfg->num_of_lanes = lt9611_get_lane_num(pdata, mode);
	video_cfg->num_of_intfs = lt9611_get_intf_num(pdata, mode);

	pr_debug("video=h[%d,%d,%d,%d] v[%d,%d,%d,%d] pclk=%d lane=%d intf=%d\n",
		video_cfg->h_active, video_cfg->h_front_porch,
		video_cfg->h_pulse_width, video_cfg->h_back_porch,
		video_cfg->v_active, video_cfg->v_front_porch,
		video_cfg->v_pulse_width, video_cfg->v_back_porch,
		video_cfg->pclk_khz, video_cfg->num_of_lanes,
		video_cfg->num_of_intfs);

	rc = drm_hdmi_avi_infoframe_from_display_mode(&avi_frame, mode, false);
	if (rc) {
		pr_err("get avi frame failed ret=%d\n", rc);
	} else {
		video_cfg->scaninfo = avi_frame.scan_mode;
		video_cfg->ar = avi_frame.picture_aspect;
		video_cfg->vic = avi_frame.video_code;
		pr_debug("scaninfo=%d ar=%d vic=%d\n",
			video_cfg->scaninfo, video_cfg->ar, video_cfg->vic);
	}
}

/* connector funcs */
static enum drm_connector_status
lt9611_connector_detect(struct drm_connector *connector, bool force)
{
	struct lt9611 *pdata = connector_to_lt9611(connector);

	if (!pdata->non_pluggable || force) {
		u8 reg_val = 0;
		int connected = 0;

		lt9611_write(pdata, 0xff, 0x82);
		lt9611_read(pdata, 0x5e, &reg_val, 1);
		connected  = (reg_val & BIT(2));
		pr_debug("connected = %x\n", connected);

		pdata->status = connected ?  connector_status_connected :
					connector_status_disconnected;
	} else
		pdata->status = connector_status_connected;

	return pdata->status;
}

static int lt9611_read_edid(struct lt9611 *pdata)
{
	int ret = 0;
	u8 i, j;
	u8 temp = 0;

	if (!pdata) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	memset(pdata->edid_buf, 0, EDID_SEG_SIZE);

	lt9611_write(pdata, 0xff, 0x85);
	lt9611_write(pdata, 0x03, 0xc9);
	lt9611_write(pdata, 0x04, 0xa0); /* 0xA0 is EDID device address */
	lt9611_write(pdata, 0x05, 0x00); /* 0x00 is EDID offset address */
	lt9611_write(pdata, 0x06, 0x20); /* length for read */
	lt9611_write(pdata, 0x14, 0x7f);

	for (i = 0 ; i < 8 ; i++) {
		lt9611_write(pdata, 0x05, i * 32); /* offset address */
		lt9611_write(pdata, 0x07, 0x36);
		lt9611_write(pdata, 0x07, 0x31);
		lt9611_write(pdata, 0x07, 0x37);
		usleep_range(5000, 10000);

		lt9611_read(pdata, 0x40, &temp, 1);

		if (temp & 0x02) {  /*KEY_DDC_ACCS_DONE=1*/
			for (j = 0; j < 32; j++) {
				lt9611_read(pdata, 0x83,
					&(pdata->edid_buf[i*32+j]), 1);
			}
		} else if (temp & 0x50) { /* DDC No Ack or Abitration lost */
			pr_err("read edid failed: no ack\n");
			ret = -EIO;
			goto end;
		} else {
			pr_err("read edid failed: access not done\n");
			ret = -EIO;
			goto end;
		}
	}

	pr_debug("read edid succeeded, checksum = 0x%x\n",
		pdata->edid_buf[255]);

end:
	lt9611_write(pdata, 0x07, 0x1f);
	return ret;
}

/* TODO: add support for more extenstion blocks */
static int lt9611_get_edid_block(void *data, u8 *buf, unsigned int block,
				  size_t len)
{
	struct lt9611 *pdata = data;
	int ret = 0;

	pr_debug("get edid block: block=%d, len=%d\n", block, (int)len);

	if (len > 128)
		return -EINVAL;

	/* support up to 1 extension block */
	if (block > 1)
		return -EINVAL;

	if (block == 0) {
		/* always read 2 edid blocks once */
		ret = lt9611_read_edid(pdata);
		if (ret) {
			pr_err("edid read failed\n");
			return ret;
		}
	}

	if (block % 2 == 0)
		memcpy(buf, pdata->edid_buf, len);
	else
		memcpy(buf, pdata->edid_buf + 128, len);

	return 0;
}

static void lt9611_set_preferred_mode(struct drm_connector *connector)
{
	struct lt9611 *pdata = connector_to_lt9611(connector);
	struct drm_display_mode *mode;
	const char *string;

	/* use specified mode as preferred */
	if (!of_property_read_string(pdata->dev->of_node,
			"lt,preferred-mode", &string)) {
		list_for_each_entry(mode, &connector->probed_modes, head) {
			if (!strcmp(mode->name, string))
				mode->type |= DRM_MODE_TYPE_PREFERRED;
		}
	}
}

static int lt9611_connector_get_modes(struct drm_connector *connector)
{
	struct lt9611 *pdata = connector_to_lt9611(connector);
	struct drm_display_mode *mode, *m;
	unsigned int count = 0;

	pr_debug("get modes\n");

	if (pdata->non_pluggable) {
		list_for_each_entry(mode, &pdata->mode_list, head) {
			m = drm_mode_duplicate(connector->dev, mode);
			if (!m) {
				pr_err("failed to add hdmi mode %dx%d\n",
					mode->hdisplay, mode->vdisplay);
				break;
			}
			drm_mode_probed_add(connector, m);
		}
		count = pdata->num_of_modes;
	} else {
		struct edid *edid;

		if (!pdata->power_on)
			lt9611_power_on(pdata, true);
		edid = drm_do_get_edid(connector, lt9611_get_edid_block, pdata);

		drm_mode_connector_update_edid_property(connector, edid);
		count = drm_add_edid_modes(connector, edid);

		pdata->hdmi_mode = drm_detect_hdmi_monitor(edid);
		pr_debug("hdmi_mode = %d\n", pdata->hdmi_mode);

		kfree(edid);
	}

	lt9611_set_preferred_mode(connector);

	return count;
}

static enum drm_mode_status lt9611_connector_mode_valid(
	struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct lt9611_timing_info *timing =
			lt9611_get_supported_timing(mode);

	return timing ? MODE_OK : MODE_BAD;
}

/* bridge funcs */
static void lt9611_bridge_enable(struct drm_bridge *bridge)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);

	pr_debug("bridge enable\n");

	if (lt9611_power_on(pdata, true)) {
		pr_err("power on failed\n");
		return;
	}

	if (lt9611_video_on(pdata, true)) {
		pr_err("video on failed\n");
		return;
	}
}

static void lt9611_bridge_disable(struct drm_bridge *bridge)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);

	pr_debug("bridge disable\n");

	if (lt9611_video_on(pdata, false)) {
		pr_err("video on failed\n");
		return;
	}

	if (lt9611_power_on(pdata, false)) {
		pr_err("power on failed\n");
		return;
	}
}

static void lt9611_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adj_mode)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);
	struct lt9611_video_cfg *video_cfg = &pdata->video_cfg;
	int ret = 0;

	pr_debug("bridge mode_set: hdisplay=%d, vdisplay=%d, vrefresh=%d, clock=%d\n",
		adj_mode->hdisplay, adj_mode->vdisplay,
		adj_mode->vrefresh, adj_mode->clock);

	drm_mode_copy(&pdata->curr_mode, adj_mode);

	memset(video_cfg, 0, sizeof(struct lt9611_video_cfg));
	lt9611_get_video_cfg(pdata, adj_mode, video_cfg);

	/* TODO: update intf number of host */
	if (video_cfg->num_of_lanes != pdata->dsi->lanes) {
		mipi_dsi_detach(pdata->dsi);
		pdata->dsi->lanes = video_cfg->num_of_lanes;
		ret = mipi_dsi_attach(pdata->dsi);
		if (ret)
			pr_err("failed to change host lanes\n");
	}
}

static const struct drm_connector_helper_funcs lt9611_connector_helper_funcs = {
	.get_modes = lt9611_connector_get_modes,
	.mode_valid = lt9611_connector_mode_valid,
};


static const struct drm_connector_funcs lt9611_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = lt9611_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};


static int lt9611_bridge_attach(struct drm_bridge *bridge)
{
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	struct lt9611 *pdata = bridge_to_lt9611(bridge);
	int ret;
	const struct mipi_dsi_device_info info = { .type = "lt9611",
						   .channel = 0,
						   .node = NULL,
						 };

	pr_debug("bridge attach\n");

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	ret = drm_connector_init(bridge->dev, &pdata->connector,
				 &lt9611_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		DRM_ERROR("Failed to initialize connector: %d\n", ret);
		return ret;
	}

	drm_connector_helper_add(&pdata->connector,
				 &lt9611_connector_helper_funcs);

	ret = drm_connector_register(&pdata->connector);
	if (ret) {
		DRM_ERROR("Failed to register connector: %d\n", ret);
		return ret;
	}

	pdata->connector.polled = DRM_CONNECTOR_POLL_CONNECT;

	ret = drm_mode_connector_attach_encoder(&pdata->connector,
						bridge->encoder);
	if (ret) {
		DRM_ERROR("Failed to link up connector to encoder: %d\n", ret);
		return ret;
	}

	host = of_find_mipi_dsi_host_by_node(pdata->host_node);
	if (!host) {
		pr_err("failed to find dsi host\n");
		return -EPROBE_DEFER;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		pr_err("failed to create dsi device\n");
		ret = PTR_ERR(dsi);
		goto err_dsi_device;
	}

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO_BLLP |
			  MIPI_DSI_MODE_VIDEO_EOF_BLLP;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		pr_err("failed to attach dsi to host\n");
		goto err_dsi_attach;
	}

	pdata->dsi = dsi;

	return 0;

err_dsi_attach:
	mipi_dsi_device_unregister(dsi);
err_dsi_device:
	return ret;
}

static void lt9611_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);

	pr_debug("bridge pre_enable\n");

	lt9611_reset(pdata, true);

	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0xee, 0x01);
}

static bool lt9611_bridge_mode_fixup(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	pr_debug("bridge mode_fixup\n");

	return true;
}

static void lt9611_bridge_post_disable(struct drm_bridge *bridge)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);

	pr_debug("bridge post_disable\n");

	lt9611_sleep_setup(pdata);
}

static const struct drm_bridge_funcs lt9611_bridge_funcs = {
	.attach = lt9611_bridge_attach,
	.mode_fixup   = lt9611_bridge_mode_fixup,
	.pre_enable   = lt9611_bridge_pre_enable,
	.enable = lt9611_bridge_enable,
	.disable = lt9611_bridge_disable,
	.post_disable = lt9611_bridge_post_disable,
	.mode_set = lt9611_bridge_mode_set,
};

/* sysfs */
static int lt9611_dump_debug_info(struct lt9611 *pdata)
{
	if (!pdata->power_on) {
		pr_err("device is not power on\n");
		return -EINVAL;
	}

	lt9611_video_check(pdata);

	lt9611_pcr_mk_debug(pdata);

	lt9611_mipi_byte_clk_debug(pdata);

	lt9611_read_edid(pdata);

	return 0;
}

static ssize_t lt9611_dump_info_wta_attr(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	struct lt9611 *pdata = dev_get_drvdata(dev);

	if (!pdata) {
		pr_err("pdata is NULL\n");
		return -EINVAL;
	}

	lt9611_dump_debug_info(pdata);

	return count;
}

static DEVICE_ATTR(dump_info, 0200, NULL, lt9611_dump_info_wta_attr);

static struct attribute *lt9611_sysfs_attrs[] = {
	&dev_attr_dump_info.attr,
	NULL,
};

static struct attribute_group lt9611_sysfs_attr_grp = {
	.attrs = lt9611_sysfs_attrs,
};

static int lt9611_sysfs_init(struct device *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	rc = sysfs_create_group(&dev->kobj, &lt9611_sysfs_attr_grp);
	if (rc)
		pr_err("%s: sysfs group creation failed %d\n", __func__, rc);

	return rc;
}

static void lt9611_sysfs_remove(struct device *dev)
{
	if (!dev) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	sysfs_remove_group(&dev->kobj, &lt9611_sysfs_attr_grp);
}

static int lt9611_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	struct lt9611 *pdata;
	int ret = 0;

	if (!client || !client->dev.of_node) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("device doesn't support I2C\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct lt9611), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = lt9611_parse_dt(&client->dev, pdata);
	if (ret) {
		pr_err("failed to parse device tree\n");
		goto err_dt_parse;
	}

	ret = lt9611_get_dt_supply(&client->dev, pdata);
	if (ret) {
		pr_err("failed to get dt supply\n");
		goto err_dt_parse;
	}

	pdata->dev = &client->dev;
	pdata->i2c_client = client;
	pr_debug("I2C address is %x\n", client->addr);

	ret = lt9611_gpio_configure(pdata, true);
	if (ret) {
		pr_err("failed to configure GPIOs\n");
		goto err_dt_supply;
	}

	lt9611_assert_5v(pdata, true);

	ret = lt9611_enable_vreg(pdata, true);
	if (ret) {
		pr_err("failed to enable vreg\n");
		goto err_dt_supply;
	}

	lt9611_reset(pdata, true);

	pdata->regulator_on = true;

	ret = lt9611_read_device_rev(pdata);
	if (ret) {
		pr_err("failed to read chip rev\n");
		goto err_i2c_prog;
	}

	pdata->irq = gpio_to_irq(pdata->irq_gpio);
	ret = request_threaded_irq(pdata->irq, NULL, lt9611_irq_thread_handler,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lt9611", pdata);
	if (ret) {
		pr_err("failed to request irq\n");
		goto err_i2c_prog;
	}

	i2c_set_clientdata(client, pdata);
	dev_set_drvdata(&client->dev, pdata);

	ret = lt9611_sysfs_init(&client->dev);
	if (ret) {
		pr_err("sysfs init failed\n");
		goto err_sysfs_init;
	}

#if IS_ENABLED(CONFIG_OF)
	pdata->bridge.of_node = client->dev.of_node;
#endif

	pdata->bridge.funcs = &lt9611_bridge_funcs;
	/*pdata->bridge.of_node = client->dev.of_node;*/

	drm_bridge_add(&pdata->bridge);

	pdata->wq = create_singlethread_workqueue("lt9611_wk");
	if (!pdata->wq) {
		pr_err("Error creating lt9611 wq\n");
		return -ENOMEM;
	}

	INIT_WORK(&pdata->work, lt9611_hpd_work);

	return 0;

err_sysfs_init:
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);
err_i2c_prog:
	lt9611_gpio_configure(pdata, false);
err_dt_supply:
	lt9611_put_dt_supply(&client->dev, pdata);
err_dt_parse:
	devm_kfree(&client->dev, pdata);
	return ret;
}

static int lt9611_remove(struct i2c_client *client)
{
	int ret = -EINVAL;
	struct lt9611 *pdata = i2c_get_clientdata(client);
	struct drm_display_mode *mode, *n;

	if (!pdata)
		goto end;

	mipi_dsi_detach(pdata->dsi);
	mipi_dsi_device_unregister(pdata->dsi);

	drm_bridge_remove(&pdata->bridge);

	lt9611_sysfs_remove(&client->dev);

	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	ret = lt9611_gpio_configure(pdata, false);

	lt9611_put_dt_supply(&client->dev, pdata);

	if (pdata->non_pluggable) {
		list_for_each_entry_safe(mode, n, &pdata->mode_list, head) {
			list_del(&mode->head);
			kfree(mode);
		}
	}

	if (pdata->wq)
		destroy_workqueue(pdata->wq);

	devm_kfree(&client->dev, pdata);

end:
	return ret;
}

static void lt9611_device_power_ctl(struct lt9611 *pdata, bool on_off)
{
	int ret = 0;

	lt9611_assert_5v(pdata, on_off);

	ret = lt9611_enable_vreg(pdata, on_off);

	if (ret)
		pr_err("failed to set vreg state %d\n", on_off);

	lt9611_reset(pdata, on_off);
}

#ifdef CONFIG_PM_SLEEP
static int lt9611_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lt9611 *pdata = i2c_get_clientdata(client);

	if (pdata->regulator_on) {
		disable_irq(pdata->irq);
		lt9611_device_power_ctl(pdata, false);
		pdata->regulator_on = false;
	}

	return 0;
}

static int lt9611_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lt9611 *pdata = i2c_get_clientdata(client);

	if (!pdata->regulator_on) {
		lt9611_device_power_ctl(pdata, true);
		enable_irq(pdata->irq);
		pdata->regulator_on = true;
	}

	return 0;
}

static const struct dev_pm_ops lt9611_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(lt9611_suspend, lt9611_resume)
};
#endif

static struct i2c_device_id lt9611_id[] = {
	{ "lt,lt9611", 0},
	{}
};

static const struct of_device_id lt9611_match_table[] = {
	{.compatible = "lt,lt9611"},
	{}
};
MODULE_DEVICE_TABLE(of, lt9611_match_table);

static struct i2c_driver lt9611_driver = {
	.driver = {
		.name = "lt9611",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lt9611_match_table,
#endif
#ifdef CONFIG_PM_SLEEP
		.pm = &lt9611_pm,
#endif
	},
	.probe = lt9611_probe,
	.remove = lt9611_remove,
	.id_table = lt9611_id,
};

static int __init lt9611_init(void)
{
	return i2c_add_driver(&lt9611_driver);
}

static void __exit lt9611_exit(void)
{
	i2c_del_driver(&lt9611_driver);
}

module_init(lt9611_init);
module_exit(lt9611_exit);

