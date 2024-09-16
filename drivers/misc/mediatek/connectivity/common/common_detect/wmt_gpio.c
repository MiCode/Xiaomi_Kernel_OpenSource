/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include "wmt_gpio.h"
#if (LINUX_VERSION_CODE >> 8) == 0x40E
#include <wmt_build_in_adapter.h>
#endif

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
const PUINT8 gpio_state_name[GPIO_PIN_ID_MAX][GPIO_STATE_MAX] = {{"gpio_ldo_en_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_ldo_en_in_pulldown",
		""},
	{"gpio_pmuv28_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_pmuv28_in_pulldown",
		""},
	{"gpio_pmu_en_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_pmu_en_in_pulldown",
		""},
	{"gpio_rst_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_rst_in_pulldown",
		""},
	{"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_bgf_eint_in_pull_dis",
		"gpio_bgf_eint_in_pulldown",
		"gpio_bgf_eint_in_pullup"},
	{"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_wifi_eint_in_pull_dis",
		"",
		"gpio_wifi_eint_in_pullup"},
	{"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"gpio_all_eint_in_pulldown",
		"gpio_all_eint_in_pullup"},
	{"gpio_urxd_uart_pull_dis",
		"",
		"",
		"gpio_urxd_uart_out_low",
		"",
		"",
		"",
		"gpio_urxd_gpio_in_pull_dis",
		"",
		"gpio_urxd_gpio_in_pullup"},
	{"gpio_utxd_uart_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_pcm_daiclk_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_pcm_daipcmin_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_pcm_daipcmout_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_pcm_daisync_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_i2s_ck_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_i2s_ws_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_i2s_dat_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_gps_sync_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"gpio_gps_lna_pull_dis",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		""},
	{"",
		"",
		"",
		"",
		"",
		"gpio_chip_deep_sleep_in_pull_dis",
		"",
		"",
		"",
		""},
	{"",
		"",
		"gpio_chip_wake_up_pullup",
		"",
		"",
		"",
		"",
		"",
		"",
		""}
};

const PUINT8 gpio_pin_name[GPIO_PIN_ID_MAX] = {"gpio_combo_ldo_en_pin",
					"gpio_combo_pmuv28_en_pin",
					"gpio_combo_pmu_en_pin",
					"gpio_combo_rst_pin",
					"gpio_combo_bgf_eint_pin",
					"gpio_wifi_eint_pin",
					"gpio_all_eint_pin",
					"gpio_combo_urxd_pin",
					"gpio_combo_utxd_pin",
					"gpio_pcm_daiclk_pin",
					"gpio_pcm_daipcmin_pin",
					"gpio_pcm_daipcmout_pin",
					"gpio_pcm_daisync_pin",
					"gpio_combo_i2s_ck_pin",
					"gpio_combo_i2s_ws_pin",
					"gpio_combo_i2s_dat_pin",
					"gpio_gps_sync_pin",
					"gpio_gps_lna_pin",
					"gpio_chip_deep_sleep_pin",
					"gpio_chip_wake_up_pin"
					};

GPIO_CTRL_INFO gpio_ctrl_info;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
int __weak mt_get_gpio_mode_base(unsigned long pin)
{
	return 0;
}

int __weak mt_get_gpio_pull_select_base(unsigned long pin)
{
	return 0;
}

int __weak mt_get_gpio_in_base(unsigned long pin)
{
	return 0;
}

int __weak mt_get_gpio_out_base(unsigned long pin)
{
	return 0;
}

int __weak mt_get_gpio_pull_enable_base(unsigned long pin)
{
	return 0;
}

int __weak mt_get_gpio_dir_base(unsigned long pin)
{
	return 0;
}

int __weak mt_get_gpio_ies_base(unsigned long pin)
{
	return 0;
}

INT32 wmt_gpio_init(struct platform_device *pdev)
{
	INT32 iret = 0;
	UINT32 i, j;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,connectivity-combo");
	if (!node) {
		for (i = 0; i < GPIO_PIN_ID_MAX; i++)
			gpio_ctrl_info.gpio_ctrl_state[i].gpio_num = DEFAULT_PIN_ID;
		pr_err("wmt_gpio:can't find device tree node!\n");
		iret = -1;
		goto err;
	}

	gpio_ctrl_info.pinctrl_info = devm_pinctrl_get(&pdev->dev);
	if (gpio_ctrl_info.pinctrl_info) {
		for (i = 0; i < GPIO_PIN_ID_MAX; i++) {
			gpio_ctrl_info.gpio_ctrl_state[i].gpio_num = of_get_named_gpio(node,
					gpio_pin_name[i], 0);
			if (gpio_ctrl_info.gpio_ctrl_state[i].gpio_num < 0)
				gpio_ctrl_info.gpio_ctrl_state[i].gpio_num = DEFAULT_PIN_ID;
			if (gpio_ctrl_info.gpio_ctrl_state[i].gpio_num != DEFAULT_PIN_ID) {
				for (j = 0; j < GPIO_STATE_MAX; j++) {
					if (strlen(gpio_state_name[i][j]) != 0) {
						gpio_ctrl_info.gpio_ctrl_state[i].gpio_state[j] =
							pinctrl_lookup_state(gpio_ctrl_info.pinctrl_info,
									gpio_state_name[i][j]);
					} else
						gpio_ctrl_info.gpio_ctrl_state[i].gpio_state[j] = NULL;
				}
			} else {
				for (j = 0; j < GPIO_STATE_MAX; j++)
					gpio_ctrl_info.gpio_ctrl_state[i].gpio_state[j] = NULL;
			}
		}

		pr_info("wmt_gpio: gpio init start!\n");
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
								 gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_URXD_PIN].
								 gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_COMBO_URXD_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
								 gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_UTXD_PIN].
								 gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_COMBO_UTXD_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].
					gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_COMBO_PMU_EN_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num != DEFAULT_PIN_ID) {
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num,
					0);
			pr_err("wmt_gpio:set GPIO_COMBO_PMU_EN_PIN out to 0: %d!\n",
					gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num));
		}

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_COMBO_RST_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num != DEFAULT_PIN_ID) {
			gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num,
					0);
			pr_err("wmt_gpio:set GPIO_COMBO_RST_PIN out to 0: %d!\n",
					gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num));
		}

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_state[GPIO_IN_PULLUP]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_state[GPIO_IN_PULLUP]);
		} else
			pr_err("wmt_gpio:set GPIO_WIFI_EINT_PIN to GPIO_IN_PULLUP fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAICLK_PIN].gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_PCM_DAICLK_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMIN_PIN].
					gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_PCM_DAIPCMIN_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAIPCMOUT_PIN].
					gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_PCM_DAIPCMOUT_PIN to GPIO_PULL_DIS fail, is NULL!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_state[GPIO_PULL_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].
					gpio_state[GPIO_PULL_DIS]);
		} else
			pr_err("wmt_gpio:set GPIO_PCM_DAISYNC_PIN to GPIO_PULL_DIS fail, is NULL!\n");
		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_CHIP_DEEP_SLEEP_PIN].gpio_state[GPIO_IN_DIS]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_CHIP_DEEP_SLEEP_PIN].
					gpio_state[GPIO_IN_DIS]);
		} else
			pr_warn("wmt_gpio:it may not be 6632 project, GPIO_CHIP_DEEP_SLEEP_PIN no need config!\n");

		if (gpio_ctrl_info.gpio_ctrl_state[GPIO_CHIP_WAKE_UP_PIN].gpio_state[GPIO_PULL_UP]) {
			pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
					gpio_ctrl_info.gpio_ctrl_state[GPIO_CHIP_WAKE_UP_PIN].
					gpio_state[GPIO_PULL_UP]);
		} else
			pr_warn("wmt_gpio:it may not be 6632 project, GPIO_CHIP_WAKE_UP_PIN no need config!\n");

		pr_info("wmt_gpio: gpio init done!\n");
	} else {
		pr_err("wmt_gpio:can't find pinctrl dev!\n");
		iret = -1;
	}

#if (LINUX_VERSION_CODE >> 8) == 0x40E
	KERNEL_mtk_wcn_cmb_sdio_request_eirq();
#endif
err:
	return iret;
}

INT32 wmt_gpio_deinit(VOID)
{
	INT32 iret = 0;
	UINT32 i;
	UINT32 j;

	for (i = 0; i < GPIO_PIN_ID_MAX; i++) {
		gpio_ctrl_info.gpio_ctrl_state[i].gpio_num = DEFAULT_PIN_ID;
		if (gpio_ctrl_info.gpio_ctrl_state[i].gpio_num != DEFAULT_PIN_ID) {
			for (j = 0; j < GPIO_STATE_MAX; j++) {
				if (strlen(gpio_state_name[i][j]) != 0)
					gpio_ctrl_info.gpio_ctrl_state[i].gpio_state[j] = NULL;
			}
		}
	}
	if (gpio_ctrl_info.pinctrl_info) {
		devm_pinctrl_put(gpio_ctrl_info.pinctrl_info);
		gpio_ctrl_info.pinctrl_info = NULL;
	}

	return iret;
}

VOID _wmt_dump_gpio_regs(INT32 idx)
{
	ULONG idxl = (ULONG)idx;

	pr_info("PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [IES]\n");
	pr_info("idx = %3d: %d %d %d %d %d %d %d\n",
	       idx, mt_get_gpio_mode_base(idxl),
	       mt_get_gpio_pull_select_base(idxl),
	       mt_get_gpio_in_base(idxl),
	       mt_get_gpio_out_base(idxl),
	       mt_get_gpio_pull_enable_base(idxl),
	       mt_get_gpio_dir_base(idxl),
	       mt_get_gpio_ies_base(idxl));
}

VOID _wmt_gpio_pre_regs(INT32 num, WMT_GPIO_STATE_INFO *gpio_state)
{
	gpio_state->gpio_num = num;
	gpio_state->mode = mt_get_gpio_mode_base(num);
	gpio_state->pull_sel = mt_get_gpio_pull_select_base(num);
	gpio_state->in = mt_get_gpio_in_base(num);
	gpio_state->out = mt_get_gpio_out_base(num);
	gpio_state->pull_en = mt_get_gpio_pull_enable_base(num);
	gpio_state->dir = mt_get_gpio_dir_base(num);
	gpio_state->ies = mt_get_gpio_ies_base(num);

}

VOID _wmt_dump_gpio_pre_regs(WMT_GPIO_STATE_INFO gpio_state)
{
	pr_info("PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [IES]\n");
	pr_info("idx = %3d: %d %d %d %d %d %d %d\n",
		gpio_state.gpio_num, gpio_state.mode,
		gpio_state.pull_sel, gpio_state.in,
		gpio_state.out, gpio_state.pull_en,
		gpio_state.dir, gpio_state.ies);
}
