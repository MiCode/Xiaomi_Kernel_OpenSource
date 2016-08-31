/*
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/sysedp.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/power_supply.h>
#include <mach/edp.h>
#include <linux/interrupt.h>
#include "board-ardbeg.h"
#include "board.h"
#include "board-panel.h"
#include "common.h"
#include "tegra11_soctherm.h"

/* --- EDP consumers data --- */
static unsigned int ov5693_states[] = { 0, 300 };
static unsigned int mt9m114_states[] = { 0, 150 };
static unsigned int sdhci_states[] = { 0, 966 };
static unsigned int speaker_states[] = { 0, 1080 };
static unsigned int wifi_states[] = { 0, 1020 };

/* default - 19x12 8" panel*/
static unsigned int pwm_backlight_default_states[] = {
	20, 241, 461, 681, 902, 1122, 1342, 1563, 1783, 2003, 2223
};

/* (optional) 12x8 8" panel */
static unsigned int pwm_backlight_8_12x8_states[] = {
	0, 125, 250, 375, 500, 625, 750, 875, 1000, 1125, 1250
};

/* (optional) 10" panel */
static unsigned int pwm_backlight_10_states[] = {
	0, 425, 851, 1276, 1702, 2127, 2553, 2978, 3404, 3829, 4255
};


static unsigned int as364x_states[] = {
	0, 350, 700, 1050, 1400, 1750, 2100, 2450, 2800, 3150, 3500
};

static struct sysedp_consumer_data tn8_sysedp_consumer_data[] = {
	SYSEDP_CONSUMER_DATA("ov5693", ov5693_states),
	SYSEDP_CONSUMER_DATA("mt9m114", mt9m114_states),
	SYSEDP_CONSUMER_DATA("speaker", speaker_states),
	SYSEDP_CONSUMER_DATA("wifi", wifi_states),
	SYSEDP_CONSUMER_DATA("pwm-backlight", pwm_backlight_default_states),
	SYSEDP_CONSUMER_DATA("sdhci-tegra.2", sdhci_states),
	SYSEDP_CONSUMER_DATA("sdhci-tegra.3", sdhci_states),
	SYSEDP_CONSUMER_DATA("as364x", as364x_states),
};

static struct sysedp_platform_data tn8_sysedp_platform_data = {
	.consumer_data = tn8_sysedp_consumer_data,
	.consumer_data_size = ARRAY_SIZE(tn8_sysedp_consumer_data),
	.margin = 0,
};

static struct platform_device tn8_sysedp_device = {
	.name = "sysedp",
	.id = -1,
	.dev = { .platform_data = &tn8_sysedp_platform_data }
};

void __init tn8_new_sysedp_init(void)
{
	int r;
	struct board_info board;

	tegra_get_display_board_info(&board);

	/* Some TN8 boards use non-default display */
	switch (board.board_id) {
	case BOARD_E1797:
		memcpy(pwm_backlight_default_states,
			   pwm_backlight_10_states,
			   sizeof(pwm_backlight_default_states));
		break;
	case BOARD_E1549:
	case BOARD_E1807:
		memcpy(pwm_backlight_default_states,
			   pwm_backlight_8_12x8_states,
			   sizeof(pwm_backlight_default_states));
		break;
	default:
		pr_warn("%s: Unknown tn8 display board id, 0x%x!  Assuming E1937.\n",
				__func__, board.board_id);
	case BOARD_E1937:
		break;
	}

	r = platform_device_register(&tn8_sysedp_device);
	WARN_ON(r);
}

static struct tegra_sysedp_platform_data tn8_sysedp_dynamic_capping_platdata = {
	.corecap = td575d_sysedp_corecap,
	.corecap_size = td575d_sysedp_corecap_sz,
	.core_gain = 100,
	.init_req_watts = 20000,
	.pthrot_ratio = 75,
	.cap_method = TEGRA_SYSEDP_CAP_METHOD_SIGNAL,
};

static struct platform_device tn8_sysedp_dynamic_capping = {
	.name = "sysedp_dynamic_capping",
	.id = -1,
	.dev = { .platform_data = &tn8_sysedp_dynamic_capping_platdata }
};

struct sysedp_reactive_capping_platform_data tn8_voltmon_oc1_platdata = {
	.max_capping_mw = 15000,
	.step_alarm_mw = 1000,
	.step_relax_mw = 500,
	.relax_ms = 250,
	.sysedpc = {
		.name = "voltmon_oc1"
	},
	.irq = TEGRA_SOC_OC_IRQ_BASE + TEGRA_SOC_OC_IRQ_1,
	.irq_flags = IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
};

static struct platform_device tn8_sysedp_reactive_capping_oc1 = {
	.name = "sysedp_reactive_capping",
	.id = -1,
	.dev = { .platform_data = &tn8_voltmon_oc1_platdata }
};


void __init tn8_sysedp_dynamic_capping_init(void)
{
	int r;
	int sku_id;
	struct board_info board;

	tn8_sysedp_dynamic_capping_platdata.cpufreq_lim = tegra_get_system_edp_entries(
		&tn8_sysedp_dynamic_capping_platdata.cpufreq_lim_size);
	if (!tn8_sysedp_dynamic_capping_platdata.cpufreq_lim) {
		WARN_ON(1);
		return;
	}

	sku_id = tegra_get_sku_id();
	switch (sku_id) {
	case 0x1F:
	case 0x27:
	case 0x87:
		break;
	default:
		pr_warn("%s: Unknown tn8 sku id, %x!  Assuming td570d.\n",
				__func__, sku_id);
	case 0xF:
		tn8_sysedp_dynamic_capping_platdata.corecap = td570d_sysedp_corecap;
		tn8_sysedp_dynamic_capping_platdata.corecap_size = td570d_sysedp_corecap_sz;
		break;
	}

	tegra_get_board_info(&board);

	if ((board.board_id == BOARD_P1761) &&
		(board.fab >= BOARD_FAB_A02)) {
		tn8_sysedp_dynamic_capping_platdata.cap_method =
			TEGRA_SYSEDP_CAP_METHOD_RELAX;
	}

	r = platform_device_register(&tn8_sysedp_dynamic_capping);
	WARN_ON(r);

	r = platform_device_register(&tn8_sysedp_reactive_capping_oc1);
	WARN_ON(r);
}
