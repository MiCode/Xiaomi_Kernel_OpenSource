/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <mach/dc.h>

#include "board-panel.h"
#include "board.h"
#include "iomap.h"

atomic_t sd_brightness = ATOMIC_INIT(255);
EXPORT_SYMBOL(sd_brightness);

void tegra_dsi_resources_init(u8 dsi_instance,
			struct resource *resources, int n_resources)
{
	int i;
	for (i = 0; i < n_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "dsi_regs")) {
			switch (dsi_instance) {
			case DSI_INSTANCE_0:
				r->start = TEGRA_DSI_BASE;
				r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
				break;
			case DSI_INSTANCE_1:
			default:
				r->start = TEGRA_DSIB_BASE;
				r->end = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1;
				break;
			}
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "ganged_dsia_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "ganged_dsib_regs")) {
			r->start = TEGRA_DSIB_BASE;
			r->end = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1;
		}
	}
}

void tegra_dsi_update_init_cmd_gpio_rst(
	struct tegra_dc_out *dsi_disp1_out)
{
	int i;
	for (i = 0; i < dsi_disp1_out->dsi->n_init_cmd; i++) {
		if (dsi_disp1_out->dsi->dsi_init_cmd[i].cmd_type ==
					TEGRA_DSI_GPIO_SET)
			dsi_disp1_out->dsi->dsi_init_cmd[i].sp_len_dly.gpio
				= dsi_disp1_out->dsi->dsi_panel_rst_gpio;
	}
}

int tegra_panel_reset(struct tegra_panel_of *panel, unsigned int delay_ms)
{
	int gpio = panel->panel_gpio[TEGRA_GPIO_RESET];

	if (!gpio_is_valid(gpio))
		return -ENOENT;

	gpio_direction_output(gpio, 1);
	usleep_range(1000, 5000);
	gpio_set_value(gpio, 0);
	usleep_range(1000, 5000);
	gpio_set_value(gpio, 1);
	msleep(delay_ms);

	return 0;
}

int tegra_panel_gpio_get_dt(const char *comp_str,
				struct tegra_panel_of *panel)
{
	int cnt = 0;
	char *label;
	const char *node_status;
	int err = 0;
	struct device_node *node =
		of_find_compatible_node(NULL, NULL, comp_str);

	/*
	 * If gpios are already populated, just return.
	 */
	if (panel->panel_gpio_populated)
		return 0;

	if (!node) {
		pr_info("%s panel dt support not available\n", comp_str);
		err = -ENOENT;
		goto fail;
	}

	of_property_read_string(node, "status", &node_status);
	if (strcmp(node_status, "okay")) {
		pr_info("%s panel dt support disabled\n", comp_str);
		err = -ENOENT;
		goto fail;
	}

	panel->panel_gpio[TEGRA_GPIO_RESET] =
		of_get_named_gpio(node, "nvidia,dsi-panel-rst-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_BL_ENABLE] =
		of_get_named_gpio(node, "nvidia,dsi-panel-bl-en-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_PWM] =
		of_get_named_gpio(node, "nvidia,dsi-panel-bl-pwm-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_TE] =
		of_get_named_gpio(node, "nvidia,te-gpio", 0);

	for (cnt = 0; cnt < TEGRA_N_GPIO_PANEL; cnt++) {
		if (gpio_is_valid(panel->panel_gpio[cnt])) {
			switch (cnt) {
			case TEGRA_GPIO_RESET:
				label = "tegra-panel-reset";
				break;
			case TEGRA_GPIO_BL_ENABLE:
				label = "tegra-panel-bl-enable";
				break;
			case TEGRA_GPIO_PWM:
				label = "tegra-panel-pwm";
				break;
			case TEGRA_GPIO_TE:
				label = "tegra-panel-te";
				break;
			default:
				pr_err("tegra panel no gpio entry\n");
			}
			gpio_request(panel->panel_gpio[cnt], label);
		}
	}
	panel->panel_gpio_populated = true;
fail:
	of_node_put(node);
	return err;
}

static void tegra_panel_register_ops(struct tegra_dc_out *dc_out,
				struct tegra_panel_ops *p_ops)
{
	BUG_ON(!dc_out);

	if (!p_ops) {
		/* TODO: register default ops */
	}

	dc_out->enable = p_ops->enable;
	dc_out->postpoweron = p_ops->postpoweron;
	dc_out->prepoweroff = p_ops->prepoweroff;
	dc_out->disable = p_ops->disable;
	dc_out->hotplug_init = p_ops->hotplug_init;
	dc_out->postsuspend = p_ops->postsuspend;
	dc_out->hotplug_report = p_ops->hotplug_report;
}

struct device_node *tegra_panel_get_dt_node(
			struct tegra_dc_platform_data *pdata)
{
	struct tegra_dc_out *dc_out = pdata->default_out;
	struct device_node *np_panel = NULL;
	struct board_info display_board;

	tegra_get_display_board_info(&display_board);

	switch (display_board.board_id) {
	case BOARD_E1627:
		tegra_panel_register_ops(dc_out, &dsi_p_wuxga_10_1_ops);
		np_panel = of_find_compatible_node(NULL, NULL, "p,wuxga-10-1");
		break;
	case BOARD_E1549:
		tegra_panel_register_ops(dc_out, &dsi_lgd_wxga_7_0_ops);
		np_panel = of_find_compatible_node(NULL, NULL, "lg,wxga-7");
		break;
	default:
		WARN(1, "Display panel not supported\n");
	};

	return of_device_is_available(np_panel) ? np_panel : NULL;
}

#ifdef CONFIG_TEGRA_DC
/**
 * tegra_init_hdmi - initialize and add HDMI device if not disabled by DT
 */
int tegra_init_hdmi(struct platform_device *pdev,
		     struct platform_device *phost1x)
{
	struct resource __maybe_unused *res;
	bool enabled = true;
	int err;
#ifdef CONFIG_OF
	struct device_node *hdmi_node = NULL;

	hdmi_node = of_find_node_by_path("/host1x/hdmi");
	/* disable HDMI if explicitly set that way in the device tree */
	enabled = !hdmi_node || of_device_is_available(hdmi_node);
#endif

	if (enabled) {
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "fbmem");
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif
		pdev->dev.parent = &phost1x->dev;
		err = platform_device_register(pdev);
		if (err) {
			dev_err(&pdev->dev, "device registration failed\n");
			return err;
		}
	}

	return 0;
}
#else
int tegra_init_hdmi(struct platform_device *pdev,
		     struct platform_device *phost1x)
{
	return 0;
}
#endif
