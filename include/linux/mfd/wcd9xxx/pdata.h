/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
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

#ifndef __MFD_TABLA_PDATA_H__

#define __MFD_TABLA_PDATA_H__

#include <linux/slimbus/slimbus.h>

#define MICBIAS_EXT_BYP_CAP 0x00
#define MICBIAS_NO_EXT_BYP_CAP 0x01

#define SITAR_LDOH_1P95_V 0x0
#define SITAR_LDOH_2P35_V 0x1
#define SITAR_LDOH_2P75_V 0x2
#define SITAR_LDOH_2P85_V 0x3

#define SITAR_CFILT1_SEL 0x0
#define SITAR_CFILT2_SEL 0x1
#define SITAR_CFILT3_SEL 0x2

#define WCD9XXX_LDOH_1P95_V 0x0
#define WCD9XXX_LDOH_2P35_V 0x1
#define WCD9XXX_LDOH_2P75_V 0x2
#define WCD9XXX_LDOH_2P85_V 0x3
#define WCD9XXX_LDOH_3P0_V 0x3

#define TABLA_LDOH_1P95_V 0x0
#define TABLA_LDOH_2P35_V 0x1
#define TABLA_LDOH_2P75_V 0x2
#define TABLA_LDOH_2P85_V 0x3

#define TABLA_CFILT1_SEL 0x0
#define TABLA_CFILT2_SEL 0x1
#define TABLA_CFILT3_SEL 0x2

#define MAX_AMIC_CHANNEL 7

#define TABLA_OCP_300_MA 0x0
#define TABLA_OCP_350_MA 0x2
#define TABLA_OCP_365_MA 0x3
#define TABLA_OCP_150_MA 0x4
#define TABLA_OCP_190_MA 0x6
#define TABLA_OCP_220_MA 0x7

#define TABLA_DCYCLE_255  0x0
#define TABLA_DCYCLE_511  0x1
#define TABLA_DCYCLE_767  0x2
#define TABLA_DCYCLE_1023 0x3
#define TABLA_DCYCLE_1279 0x4
#define TABLA_DCYCLE_1535 0x5
#define TABLA_DCYCLE_1791 0x6
#define TABLA_DCYCLE_2047 0x7
#define TABLA_DCYCLE_2303 0x8
#define TABLA_DCYCLE_2559 0x9
#define TABLA_DCYCLE_2815 0xA
#define TABLA_DCYCLE_3071 0xB
#define TABLA_DCYCLE_3327 0xC
#define TABLA_DCYCLE_3583 0xD
#define TABLA_DCYCLE_3839 0xE
#define TABLA_DCYCLE_4095 0xF

#define WCD9XXX_MCLK_CLK_12P288MHZ 12288000
#define WCD9XXX_MCLK_CLK_9P6HZ 9600000

/* Only valid for 9.6 MHz mclk */
#define WCD9XXX_DMIC_SAMPLE_RATE_600KHZ 600000
#define WCD9XXX_DMIC_SAMPLE_RATE_2P4MHZ 2400000
#define WCD9XXX_DMIC_SAMPLE_RATE_3P2MHZ 3200000
#define WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ 4800000

/* Only valid for 12.288 MHz mclk */
#define WCD9XXX_DMIC_SAMPLE_RATE_768KHZ 768000
#define WCD9XXX_DMIC_SAMPLE_RATE_2P048MHZ 2048000
#define WCD9XXX_DMIC_SAMPLE_RATE_3P072MHZ 3072000
#define WCD9XXX_DMIC_SAMPLE_RATE_4P096MHZ 4096000
#define WCD9XXX_DMIC_SAMPLE_RATE_6P144MHZ 6144000

#define WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED 0

#define WCD9XXX_DMIC_CLK_DRIVE_UNDEFINED 0

struct wcd9xxx_amic {
	/*legacy mode, txfe_enable and txfe_buff take 7 input
	 * each bit represent the channel / TXFE number
	 * and numbered as below
	 * bit 0 = channel 1 / TXFE1_ENABLE / TXFE1_BUFF
	 * bit 1 = channel 2 / TXFE2_ENABLE / TXFE2_BUFF
	 * ...
	 * bit 7 = channel 7 / TXFE7_ENABLE / TXFE7_BUFF
	 */
	u8 legacy_mode:MAX_AMIC_CHANNEL;
	u8 txfe_enable:MAX_AMIC_CHANNEL;
	u8 txfe_buff:MAX_AMIC_CHANNEL;
	u8 use_pdata:MAX_AMIC_CHANNEL;
};

/* Each micbias can be assigned to one of three cfilters
 * Vbatt_min >= .15V + ldoh_v
 * ldoh_v >= .15v + cfiltx_mv
 * If ldoh_v = 1.95 160 mv < cfiltx_mv < 1800 mv
 * If ldoh_v = 2.35 200 mv < cfiltx_mv < 2200 mv
 * If ldoh_v = 2.75 240 mv < cfiltx_mv < 2600 mv
 * If ldoh_v = 2.85 250 mv < cfiltx_mv < 2700 mv
 */

struct wcd9xxx_micbias_setting {
	u8 ldoh_v;
	u32 cfilt1_mv; /* in mv */
	u32 cfilt2_mv; /* in mv */
	u32 cfilt3_mv; /* in mv */
	u32 micb1_mv;
	u32 micb2_mv;
	u32 micb3_mv;
	u32 micb4_mv;
	/* Different WCD9xxx series codecs may not
	 * have 4 mic biases. If a codec has fewer
	 * mic biases, some of these properties will
	 * not be used.
	 */
	u8 bias1_cfilt_sel;
	u8 bias2_cfilt_sel;
	u8 bias3_cfilt_sel;
	u8 bias4_cfilt_sel;
	u8 bias1_cap_mode;
	u8 bias2_cap_mode;
	u8 bias3_cap_mode;
	u8 bias4_cap_mode;
	bool bias2_is_headset_only;
};

struct wcd9xxx_ocp_setting {
	unsigned int	use_pdata:1; /* 0 - use sys default as recommended */
	unsigned int	num_attempts:4; /* up to 15 attempts */
	unsigned int	run_time:4; /* in duty cycle */
	unsigned int	wait_time:4; /* in duty cycle */
	unsigned int	hph_ocp_limit:3; /* Headphone OCP current limit */
};

#define WCD9XXX_MAX_REGULATOR	9
/*
 *      format : TABLA_<POWER_SUPPLY_PIN_NAME>_CUR_MAX
 *
 *      <POWER_SUPPLY_PIN_NAME> from Tabla objective spec
*/

#define  WCD9XXX_CDC_VDDA_CP_CUR_MAX      500000
#define  WCD9XXX_CDC_VDDA_RX_CUR_MAX      20000
#define  WCD9XXX_CDC_VDDA_TX_CUR_MAX      20000
#define  WCD9XXX_VDDIO_CDC_CUR_MAX        5000

#define  WCD9XXX_VDDD_CDC_D_CUR_MAX       5000
#define  WCD9XXX_VDDD_CDC_A_CUR_MAX       5000

#define WCD9XXX_VDD_SPKDRV_NAME "cdc-vdd-spkdrv"
#define WCD9XXX_VDD_SPKDRV2_NAME "cdc-vdd-spkdrv-2"

struct wcd9xxx_regulator {
	const char *name;
	int min_uV;
	int max_uV;
	int optimum_uA;
	bool ondemand;
	struct regulator *regulator;
};

struct wcd9xxx_pdata {
	int irq;
	int irq_base;
	int num_irqs;
	int reset_gpio;
	struct device_node *wcd_rst_np;
	struct wcd9xxx_amic amic_settings;
	struct slim_device slimbus_slave_device;
	struct wcd9xxx_micbias_setting micbias;
	struct wcd9xxx_ocp_setting ocp;
	struct wcd9xxx_regulator regulator[WCD9XXX_MAX_REGULATOR];
	u32 mclk_rate;
	u32 dmic_sample_rate;
	u32 mad_dmic_sample_rate;
	u32 ecpp_dmic_sample_rate;
	u32 dmic_clk_drv;
	enum codec_variant cdc_variant;
	u16 use_pinctrl;
};

#endif
