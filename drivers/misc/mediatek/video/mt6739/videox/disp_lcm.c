/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>

#include <linux/types.h>
#include "disp_drv_log.h"
#include "lcm_drv.h"
#include "lcm_define.h"
#include "disp_drv_platform.h"
#include "ddp_manager.h"
#include "disp_lcm.h"

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
#include <linux/of.h>
#endif

/* This macro and array is designed for multiple LCM support */
/* for multiple LCM, we should assign I/F Port id in lcm driver,
 * such as DPI0, DSI0/1
 */
/* static disp_lcm_handle _disp_lcm_driver[MAX_LCM_NUMBER]; */

int _lcm_count(void)
{
	return lcm_count;
}

int _is_lcm_inited(struct disp_lcm_handle *plcm)
{
	if (plcm) {
		if (plcm->params && plcm->drv)
			return 1;
	}

	DISPERR("WARNING, invalid lcm handle: %p\n", plcm);
	return 0;
}

struct LCM_PARAMS *_get_lcm_params_by_handle(struct disp_lcm_handle *plcm)
{
	if (plcm)
		return plcm->params;

	DISPERR("WARNING, invalid lcm handle:%p\n", plcm);
	return NULL;
}

struct LCM_DRIVER *_get_lcm_driver_by_handle(struct disp_lcm_handle *plcm)
{
	if (plcm)
		return plcm->drv;

	DISPERR("WARNING, invalid lcm handle:%p\n", plcm);
	return NULL;
}

void _dump_lcm_info(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *l = NULL;
	struct LCM_PARAMS *p = NULL;

	if (plcm == NULL) {
		DISPERR("plcm is null\n");
		return;
	}

	l = plcm->drv;
	p = plcm->params;

	if (!l || !p)
		return;

	DISPCHECK("[LCM] name: %s\n", l->name);
	DISPCHECK("[LCM] resolution: %d x %d\n", p->width, p->height);
	DISPCHECK("[LCM] physical size: %d x %d\n",
		  p->physical_width, p->physical_height);
	DISPCHECK("[LCM] physical size: %d x %d\n",
		  p->physical_width, p->physical_height);

	switch (p->lcm_if) {
	case LCM_INTERFACE_DSI0:
		DISPCHECK("[LCM] interface: DSI0\n");
		break;
	case LCM_INTERFACE_DSI1:
		DISPCHECK("[LCM] interface: DSI1\n");
		break;
	case LCM_INTERFACE_DSI_DUAL:
		DISPCHECK("[LCM] interface: DSI_DUAL\n");
		break;
	case LCM_INTERFACE_DPI0:
		DISPCHECK("[LCM] interface: DPI0\n");
		break;
	case LCM_INTERFACE_DPI1:
		DISPCHECK("[LCM] interface: DPI1\n");
		break;
	case LCM_INTERFACE_DBI0:
		DISPCHECK("[LCM] interface: DBI0\n");
		break;
	default:
		DISPCHECK("[LCM] interface: unknown\n");
		break;
	}

	switch (p->type) {
	case LCM_TYPE_DBI:
		DISPCHECK("[LCM] Type: DBI\n");
		break;
	case LCM_TYPE_DSI:
		DISPCHECK("[LCM] Type: DSI\n");

		break;
	case LCM_TYPE_DPI:
		DISPCHECK("[LCM] Type: DPI\n");
		break;
	default:
		DISPCHECK("[LCM] TYPE: unknown\n");
		break;
	}

	if (p->type == LCM_TYPE_DSI) {
		switch (p->dsi.mode) {
		case CMD_MODE:
			DISPCHECK("[LCM] DSI Mode: CMD_MODE\n");
			break;
		case SYNC_PULSE_VDO_MODE:
			DISPCHECK("[LCM] DSI Mode: SYNC_PULSE_VDO_MODE\n");
			break;
		case SYNC_EVENT_VDO_MODE:
			DISPCHECK("[LCM] DSI Mode: SYNC_EVENT_VDO_MODE\n");
			break;
		case BURST_VDO_MODE:
			DISPCHECK("[LCM] DSI Mode: BURST_VDO_MODE\n");
			break;
		default:
			DISPCHECK("[LCM] DSI Mode: Unknown\n");
			break;
		}
	}

	if (p->type == LCM_TYPE_DSI) {
		DISPCHECK("[LCM] LANE_NUM: %d\n", (int)p->dsi.LANE_NUM);
		DISPCHECK("[LCM] vact: %u, vbp: %u, vfp: %u, vact_line: %u, hact: %u, hbp: %u, hfp: %u, hblank: %u\n",
			p->dsi.vertical_sync_active, p->dsi.vertical_backporch,
			p->dsi.vertical_frontporch, p->dsi.vertical_active_line,
			p->dsi.horizontal_sync_active, p->dsi.horizontal_backporch,
			p->dsi.horizontal_frontporch, p->dsi.horizontal_blanking_pixel);
		DISPCHECK("[LCM] pll_select: %d, pll_div1: %d, pll_div2: %d, fbk_div: %d,fbk_sel: %d, rg_bir: %d\n",
			p->dsi.pll_select, p->dsi.pll_div1, p->dsi.pll_div2, p->dsi.fbk_div,
			p->dsi.fbk_sel, p->dsi.rg_bir);
		DISPCHECK("[LCM] rg_bic: %d, rg_bp: %d,PLL_CLOCK: %d, dsi_clock: %d, ssc_range: %d,ssc_disable: %d",
			p->dsi.rg_bic, p->dsi.rg_bp, p->dsi.PLL_CLOCK, p->dsi.dsi_clock,
			p->dsi.ssc_range, p->dsi.ssc_disable);
		DISPCHECK("[LCM]compatibility_for_nvk: %d, cont_clock: %d\n",
			p->dsi.compatibility_for_nvk,
			p->dsi.cont_clock);
		DISPCHECK("[LCM] lcm_ext_te_enable: %d, noncont_clock: %d, noncont_clock_period: %d\n",
			p->dsi.lcm_ext_te_enable, p->dsi.noncont_clock,
			p->dsi.noncont_clock_period);
	}
}

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
static unsigned char dts[sizeof(LCM_DATA) * MAX_SIZE];
static LCM_DTS lcm_dts;

int disp_of_getprop_u32(const struct device_node *np, const char *propname,
			u32 *out_value)
{
	unsigned int i;
	int ret = 0;
	unsigned int len = 0;
	const unsigned int *prop = NULL;

	/* Get the interrupts property */
	prop = of_get_property(np, propname, &len);
	if (prop == NULL)
		ret = -1;

	if (ret)
		*out_value = 0;
	else {
		len /= sizeof(*prop);
		for (i = 0; i < len; i++)
			*(out_value + i) = be32_to_cpup(prop++);
	}

	return len;
}

int disp_of_getprop_u8(const struct device_node *np, const char *propname,
		       u8 *out_value)
{
	unsigned int i;
	int ret = 0;
	unsigned int len = 0;
	const unsigned int *prop = NULL;

	/* Get the interrupts property */
	prop = of_get_property(np, propname, &len);
	if (prop == NULL)
		ret = -1;

	if (ret)
		*out_value = 0;
	else {
		len /= sizeof(*prop);
		for (i = 0; i < len; i++)
			*(out_value + i) =
				(unsigned char)((be32_to_cpup(prop++)) & 0xFF);
	}

	return len;
}

void parse_lcm_params_dt_node(struct device_node *np,
			      struct LCM_PARAMS *lcm_params)
{
	if (!lcm_params) {
		DISPERR("%s:%d, ERROR: Error access to LCM_PARAMS(NULL)\n",
			__FILE__, __LINE__);
		return;
	}

	memset(lcm_params, 0x0, sizeof(struct LCM_PARAMS));

	disp_of_getprop_u32(np, "lcm_params-types", &lcm_params->type);
	disp_of_getprop_u32(np, "lcm_params-resolution", &lcm_params->width);
	disp_of_getprop_u32(np, "lcm_params-io_select_mode",
			    &lcm_params->io_select_mode);

	disp_of_getprop_u32(np, "lcm_params-dbi-port", &lcm_params->dbi.port);
	disp_of_getprop_u32(np, "lcm_params-dbi-clock_freq",
			    &lcm_params->dbi.clock_freq);
	disp_of_getprop_u32(np, "lcm_params-dbi-data_width",
			    &lcm_params->dbi.data_width);
	disp_of_getprop_u32(np, "lcm_params-dbi-data_format",
			    (u32 *)(&lcm_params->dbi.data_format));
	disp_of_getprop_u32(np, "lcm_params-dbi-cpu_write_bits",
			    &lcm_params->dbi.cpu_write_bits);
	disp_of_getprop_u32(np, "lcm_params-dbi-io_driving_current",
			    &lcm_params->dbi.io_driving_current);
	disp_of_getprop_u32(np, "lcm_params-dbi-msb_io_driving_current",
			    &lcm_params->dbi.msb_io_driving_current);
	disp_of_getprop_u32(np, "lcm_params-dbi-ctrl_io_driving_current",
			    &lcm_params->dbi.ctrl_io_driving_current);

	disp_of_getprop_u32(np, "lcm_params-dbi-te_mode",
			    &lcm_params->dbi.te_mode);
	disp_of_getprop_u32(np, "lcm_params-dbi-te_edge_polarity",
			    &lcm_params->dbi.te_edge_polarity);
	disp_of_getprop_u32(np, "lcm_params-dbi-te_hs_delay_cnt",
			    &lcm_params->dbi.te_hs_delay_cnt);
	disp_of_getprop_u32(np, "lcm_params-dbi-te_vs_width_cnt",
			    &lcm_params->dbi.te_vs_width_cnt);
	disp_of_getprop_u32(np, "lcm_params-dbi-te_vs_width_cnt_div",
			    &lcm_params->dbi.te_vs_width_cnt_div);

	disp_of_getprop_u32(np, "lcm_params-dbi-serial-params0",
			    &lcm_params->dbi.serial.cs_polarity);
	disp_of_getprop_u32(np, "lcm_params-dbi-serial-params1",
			    &lcm_params->dbi.serial.css);
	disp_of_getprop_u32(np, "lcm_params-dbi-serial-params2",
			    &lcm_params->dbi.serial.sif_3wire);
	disp_of_getprop_u32(np, "lcm_params-dbi-parallel-params0",
			    &lcm_params->dbi.parallel.write_setup);
	disp_of_getprop_u32(np, "lcm_params-dbi-parallel-params1",
			    &lcm_params->dbi.parallel.read_hold);

	disp_of_getprop_u32(np, "lcm_params-dpi-mipi_pll_clk_ref",
			    &lcm_params->dpi.mipi_pll_clk_ref);
	disp_of_getprop_u32(np, "lcm_params-dpi-mipi_pll_clk_div1",
			    &lcm_params->dpi.mipi_pll_clk_div1);
	disp_of_getprop_u32(np, "lcm_params-dpi-mipi_pll_clk_div2",
			    &lcm_params->dpi.mipi_pll_clk_div2);
	disp_of_getprop_u32(np, "lcm_params-dpi-mipi_pll_clk_fbk_div",
			    &lcm_params->dpi.mipi_pll_clk_fbk_div);

	disp_of_getprop_u32(np, "lcm_params-dpi-dpi_clk_div",
			    &lcm_params->dpi.dpi_clk_div);
	disp_of_getprop_u32(np, "lcm_params-dpi-dpi_clk_duty",
			    &lcm_params->dpi.dpi_clk_duty);
	disp_of_getprop_u32(np, "lcm_params-dpi-PLL_CLOCK",
			    &lcm_params->dpi.PLL_CLOCK);
	disp_of_getprop_u32(np, "lcm_params-dpi-dpi_clock",
			    &lcm_params->dpi.dpi_clock);
	disp_of_getprop_u32(np, "lcm_params-dpi-ssc_disable",
			    &lcm_params->dpi.ssc_disable);
	disp_of_getprop_u32(np, "lcm_params-dpi-ssc_range",
			    &lcm_params->dpi.ssc_range);

	disp_of_getprop_u32(np, "lcm_params-dpi-width",
			    &lcm_params->dpi.width);
	disp_of_getprop_u32(np, "lcm_params-dpi-height",
			    &lcm_params->dpi.height);
	disp_of_getprop_u32(np, "lcm_params-dpi-bg_width",
			    &lcm_params->dpi.bg_width);
	disp_of_getprop_u32(np, "lcm_params-dpi-bg_height",
			    &lcm_params->dpi.bg_height);

	disp_of_getprop_u32(np, "lcm_params-dpi-clk_pol",
			    &lcm_params->dpi.clk_pol);
	disp_of_getprop_u32(np, "lcm_params-dpi-de_pol",
			    &lcm_params->dpi.de_pol);
	disp_of_getprop_u32(np, "lcm_params-dpi-vsync_pol",
			    &lcm_params->dpi.vsync_pol);
	disp_of_getprop_u32(np, "lcm_params-dpi-hsync_pol",
			    &lcm_params->dpi.hsync_pol);
	disp_of_getprop_u32(np, "lcm_params-dpi-hsync_pulse_width",
			    &lcm_params->dpi.hsync_pulse_width);
	disp_of_getprop_u32(np, "lcm_params-dpi-hsync_back_porch",
			    &lcm_params->dpi.hsync_back_porch);
	disp_of_getprop_u32(np, "lcm_params-dpi-hsync_front_porch",
			    &lcm_params->dpi.hsync_front_porch);
	disp_of_getprop_u32(np, "lcm_params-dpi-vsync_pulse_width",
			    &lcm_params->dpi.vsync_pulse_width);
	disp_of_getprop_u32(np, "lcm_params-dpi-vsync_back_porch",
			    &lcm_params->dpi.vsync_back_porch);
	disp_of_getprop_u32(np, "lcm_params-dpi-vsync_front_porch",
			    &lcm_params->dpi.vsync_front_porch);

	disp_of_getprop_u32(np, "lcm_params-dpi-format",
			    &lcm_params->dpi.format);
	disp_of_getprop_u32(np, "lcm_params-dpi-rgb_order",
			    &lcm_params->dpi.rgb_order);
	disp_of_getprop_u32(np, "lcm_params-dpi-is_serial_output",
			    &lcm_params->dpi.is_serial_output);
	disp_of_getprop_u32(np, "lcm_params-dpi-i2x_en",
			    &lcm_params->dpi.i2x_en);
	disp_of_getprop_u32(np, "lcm_params-dpi-i2x_edge",
			    &lcm_params->dpi.i2x_edge);
	disp_of_getprop_u32(np, "lcm_params-dpi-embsync",
			    &lcm_params->dpi.embsync);
	disp_of_getprop_u32(np, "lcm_params-dpi-lvds_tx_en",
			    &lcm_params->dpi.lvds_tx_en);
	disp_of_getprop_u32(np, "lcm_params-dpi-bit_swap",
			    &lcm_params->dpi.bit_swap);
	disp_of_getprop_u32(np, "lcm_params-dpi-intermediat_buffer_num",
			    &lcm_params->dpi.intermediat_buffer_num);
	disp_of_getprop_u32(np, "lcm_params-dpi-io_driving_current",
			    &lcm_params->dpi.io_driving_current);
	disp_of_getprop_u32(np, "lcm_params-dpi-lsb_io_driving_current",
			    &lcm_params->dpi.lsb_io_driving_current);

	disp_of_getprop_u32(np, "lcm_params-dsi-mode",
			    &lcm_params->dsi.mode);
	disp_of_getprop_u32(np, "lcm_params-dsi-switch_mode",
			    &lcm_params->dsi.switch_mode);
	disp_of_getprop_u32(np, "lcm_params-dsi-DSI_WMEM_CONTI",
			    &lcm_params->dsi.DSI_WMEM_CONTI);
	disp_of_getprop_u32(np, "lcm_params-dsi-DSI_RMEM_CONTI",
			    &lcm_params->dsi.DSI_RMEM_CONTI);
	disp_of_getprop_u32(np, "lcm_params-dsi-VC_NUM",
			    &lcm_params->dsi.VC_NUM);
	disp_of_getprop_u32(np, "lcm_params-dsi-lane_num",
			    &lcm_params->dsi.LANE_NUM);
	disp_of_getprop_u32(np, "lcm_params-dsi-data_format",
			    (u32 *)(&lcm_params->dsi.data_format));
	disp_of_getprop_u32(np, "lcm_params-dsi-intermediat_buffer_num",
			    &lcm_params->dsi.intermediat_buffer_num);
	disp_of_getprop_u32(np, "lcm_params-dsi-ps",
			    &lcm_params->dsi.PS);
	disp_of_getprop_u32(np, "lcm_params-dsi-word_count",
			    &lcm_params->dsi.word_count);
	disp_of_getprop_u32(np, "lcm_params-dsi-packet_size",
			    &lcm_params->dsi.packet_size);

	disp_of_getprop_u32(np, "lcm_params-dsi-vertical_sync_active",
			    &lcm_params->dsi.vertical_sync_active);
	disp_of_getprop_u32(np, "lcm_params-dsi-vertical_backporch",
			    &lcm_params->dsi.vertical_backporch);
	disp_of_getprop_u32(np, "lcm_params-dsi-vertical_frontporch",
			    &lcm_params->dsi.vertical_frontporch);
	disp_of_getprop_u32(np,
			    "lcm_params-dsi-vertical_frontporch_for_low_power",
			    &lcm_params->dsi.vertical_frontporch_for_low_power);
	disp_of_getprop_u32(np, "lcm_params-dsi-vertical_active_line",
			    &lcm_params->dsi.vertical_active_line);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_sync_active",
			    &lcm_params->dsi.horizontal_sync_active);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_backporch",
			    &lcm_params->dsi.horizontal_backporch);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_frontporch",
			    &lcm_params->dsi.horizontal_frontporch);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_blanking_pixel",
			    &lcm_params->dsi.horizontal_blanking_pixel);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_active_pixel",
			    &lcm_params->dsi.horizontal_active_pixel);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_bllp",
			    &lcm_params->dsi.horizontal_bllp);
	disp_of_getprop_u32(np, "lcm_params-dsi-line_byte",
			    &lcm_params->dsi.line_byte);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_sync_active_byte",
			    &lcm_params->dsi.horizontal_sync_active_byte);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_backportch_byte",
			    &lcm_params->dsi.horizontal_backporch_byte);
	disp_of_getprop_u32(np, "lcm_params-dsi-horizontal_frontporch_byte",
			    &lcm_params->dsi.horizontal_frontporch_byte);
	disp_of_getprop_u32(np, "lcm_params-dsi-rgb_byte",
			    &lcm_params->dsi.rgb_byte);
	disp_of_getprop_u32(np,
			    "lcm_params-dsi-horizontal_sync_active_word_count",
			    &lcm_params->dsi.horizontal_sync_active_word_count);
	disp_of_getprop_u32(np,
			    "lcm_params-dsi-horizontal_backporch_word_count",
			    &lcm_params->dsi.horizontal_backporch_word_count);
	disp_of_getprop_u32(np,
			    "lcm_params-dsi-horizontal_frontporch_word_count",
			    &lcm_params->dsi.horizontal_frontporch_word_count);

	disp_of_getprop_u8(np, "lcm_params-dsi-HS_TRAIL",
			   &lcm_params->dsi.HS_TRAIL);
	disp_of_getprop_u8(np, "lcm_params-dsi-ZERO",
			   &lcm_params->dsi.HS_ZERO);
	disp_of_getprop_u8(np, "lcm_params-dsi-HS_PRPR",
			   &lcm_params->dsi.HS_PRPR);
	disp_of_getprop_u8(np, "lcm_params-dsi-LPX",
			   &lcm_params->dsi.LPX);
	disp_of_getprop_u8(np, "lcm_params-dsi-TA_SACK",
			   &lcm_params->dsi.TA_SACK);
	disp_of_getprop_u8(np, "lcm_params-dsi-TA_GET",
			   &lcm_params->dsi.TA_GET);
	disp_of_getprop_u8(np, "lcm_params-dsi-TA_SURE",
			   &lcm_params->dsi.TA_SURE);
	disp_of_getprop_u8(np, "lcm_params-dsi-TA_GO",
			   &lcm_params->dsi.TA_GO);
	disp_of_getprop_u8(np, "lcm_params-dsi-CLK_TRAIL",
			   &lcm_params->dsi.CLK_TRAIL);
	disp_of_getprop_u8(np, "lcm_params-dsi-CLK_ZERO",
			   &lcm_params->dsi.CLK_ZERO);
	disp_of_getprop_u8(np, "lcm_params-dsi-LPX_WAIT",
			   &lcm_params->dsi.LPX_WAIT);
	disp_of_getprop_u8(np, "lcm_params-dsi-CONT_DET",
			   &lcm_params->dsi.CONT_DET);
	disp_of_getprop_u8(np, "lcm_params-dsi-CLK_HS_PRPR",
			   &lcm_params->dsi.CLK_HS_PRPR);
	disp_of_getprop_u8(np, "lcm_params-dsi-CLK_HS_POST",
			   &lcm_params->dsi.CLK_HS_POST);
	disp_of_getprop_u8(np, "lcm_params-dsi-DA_HS_EXIT",
			   &lcm_params->dsi.DA_HS_EXIT);
	disp_of_getprop_u8(np, "lcm_params-dsi-CLK_HS_EXIT",
			   &lcm_params->dsi.CLK_HS_EXIT);

	disp_of_getprop_u32(np, "lcm_params-dsi-pll_select",
			    &lcm_params->dsi.pll_select);
	disp_of_getprop_u32(np, "lcm_params-dsi-pll_div1",
			    &lcm_params->dsi.pll_div1);
	disp_of_getprop_u32(np, "lcm_params-dsi-pll_div2",
			    &lcm_params->dsi.pll_div2);
	disp_of_getprop_u32(np, "lcm_params-dsi-fbk_div",
			    &lcm_params->dsi.fbk_div);
	disp_of_getprop_u32(np, "lcm_params-dsi-fbk_sel",
			    &lcm_params->dsi.fbk_sel);
	disp_of_getprop_u32(np, "lcm_params-dsi-rg_bir",
			    &lcm_params->dsi.rg_bir);
	disp_of_getprop_u32(np, "lcm_params-dsi-rg_bic",
			    &lcm_params->dsi.rg_bic);
	disp_of_getprop_u32(np, "lcm_params-dsi-rg_bp",
			    &lcm_params->dsi.rg_bp);
	disp_of_getprop_u32(np, "lcm_params-dsi-pll_clock",
			    &lcm_params->dsi.PLL_CLOCK);
	disp_of_getprop_u32(np, "lcm_params-dsi-dsi_clock",
			    &lcm_params->dsi.dsi_clock);
	disp_of_getprop_u32(np, "lcm_params-dsi-ssc_disable",
			    &lcm_params->dsi.ssc_disable);
	disp_of_getprop_u32(np, "lcm_params-dsi-ssc_range",
			    &lcm_params->dsi.ssc_range);
	disp_of_getprop_u32(np, "lcm_params-dsi-compatibility_for_nvk",
			    &lcm_params->dsi.compatibility_for_nvk);
	disp_of_getprop_u32(np, "lcm_params-dsi-cont_clock",
			    &lcm_params->dsi.cont_clock);

	disp_of_getprop_u32(np, "lcm_params-dsi-ufoe_enable",
			    &lcm_params->dsi.ufoe_enable);
	disp_of_getprop_u32(np, "lcm_params-dsi-ufoe_params",
			    (u32 *)(&lcm_params->dsi.ufoe_params));
	disp_of_getprop_u32(np, "lcm_params-dsi-edp_panel",
			    &lcm_params->dsi.edp_panel);

	disp_of_getprop_u32(np, "lcm_params-dsi-customization_esd_check_enable",
			    &lcm_params->dsi.customization_esd_check_enable);
	disp_of_getprop_u32(np, "lcm_params-dsi-esd_check_enable",
			    &lcm_params->dsi.esd_check_enable);

	disp_of_getprop_u32(np, "lcm_params-dsi-lcm_int_te_monitor",
			    &lcm_params->dsi.lcm_int_te_monitor);
	disp_of_getprop_u32(np, "lcm_params-dsi-lcm_int_te_period",
			    &lcm_params->dsi.lcm_int_te_period);
	disp_of_getprop_u32(np, "lcm_params-dsi-lcm_ext_te_monitor",
			    &lcm_params->dsi.lcm_ext_te_monitor);
	disp_of_getprop_u32(np, "lcm_params-dsi-lcm_ext_te_enable",
			    &lcm_params->dsi.lcm_ext_te_enable);

	disp_of_getprop_u32(np, "lcm_params-dsi-noncont_clock",
			    &lcm_params->dsi.noncont_clock);
	disp_of_getprop_u32(np, "lcm_params-dsi-noncont_clock_period",
			    &lcm_params->dsi.noncont_clock_period);
	disp_of_getprop_u32(np, "lcm_params-dsi-clk_lp_per_line_enable",
			    &lcm_params->dsi.clk_lp_per_line_enable);

	disp_of_getprop_u8(np, "lcm_params-dsi-lcm_esd_check_table0",
			   (u8 *)(&(lcm_params->dsi.lcm_esd_check_table[0])));
	disp_of_getprop_u8(np, "lcm_params-dsi-lcm_esd_check_table1",
			   (u8 *)(&(lcm_params->dsi.lcm_esd_check_table[1])));
	disp_of_getprop_u8(np, "lcm_params-dsi-lcm_esd_check_table2",
			   (u8 *)(&(lcm_params->dsi.lcm_esd_check_table[2])));

	disp_of_getprop_u32(np, "lcm_params-dsi-switch_mode_enable",
			    &lcm_params->dsi.switch_mode_enable);
	disp_of_getprop_u32(np, "lcm_params-dsi-dual_dsi_type",
			    &lcm_params->dsi.dual_dsi_type);
	disp_of_getprop_u32(np, "lcm_params-dsi-lane_swap_en",
			    &lcm_params->dsi.lane_swap_en);
	disp_of_getprop_u32(np, "lcm_params-dsi-lane_swap0",
			    (u32 *)(&(lcm_params->dsi.lane_swap[0][0])));
	disp_of_getprop_u32(np, "lcm_params-dsi-lane_swap1",
			    (u32 *)(&(lcm_params->dsi.lane_swap[1][0])));
	disp_of_getprop_u32(np, "lcm_params-dsi-vertical_vfp_lp",
			    &lcm_params->dsi.vertical_vfp_lp);
	disp_of_getprop_u32(np, "lcm_params-physical_width",
			    &lcm_params->physical_width);
	disp_of_getprop_u32(np, "lcm_params-physical_height",
			    &lcm_params->physical_height);
	disp_of_getprop_u32(np, "lcm_params-physical_width_um",
			    &lcm_params->physical_width_um);
	disp_of_getprop_u32(np, "lcm_params-physical_height_um",
			    &lcm_params->physical_height_um);
	disp_of_getprop_u32(np, "lcm_params-od_table_size",
			    &lcm_params->od_table_size);
	disp_of_getprop_u32(np, "lcm_params-od_table",
			    (u32 *)(&lcm_params->od_table));
}

void parse_lcm_ops_dt_node(struct device_node *np, LCM_DTS *lcm_dts,
			   unsigned char *dts)
{
	unsigned int i;
	unsigned char *tmp;
	int len = 0;
	int tmp_len;
	struct LCM_DATA *lcm_data;

	if (!lcm_dts) {
		DISPERR("%s:%d, ERROR: Error access to LCM_PARAMS(NULL)\n",
			__FILE__, __LINE__);
		return;
	}
	/* parse LCM init table */
	len = disp_of_getprop_u8(np, "init", dts);
	if (len <= 0) {
		DISPERR("%s:%d: Cannot find LCM init table, cannot skip it!\n",
			__FILE__, __LINE__);
		return;
	}
	if (len > (sizeof(LCM_DATA) * INIT_SIZE)) {
		DISPERR("%s:%d: LCM init table overflow: %d\n", __FILE__,
			__LINE__, len);
		return;
	}
	DISPMSG("%s:%d: len: %d\n", __FILE__, __LINE__, len);

	tmp = dts;
	lcm_data = lcm_dts->init;
	for (i = 0; i < INIT_SIZE; i++) {
		lcm_data[i].func = (*tmp) & 0xFF;
		lcm_data[i].type = (*(tmp + 1)) & 0xFF;
		lcm_data[i].size = (*(tmp + 2)) & 0xFF;
		tmp_len = 3;

		DISPMSG("%s:%d: dts: %d, %d, %d\n",
			__FILE__, __LINE__, *tmp, *(tmp + 1), i);
		switch (lcm_data[i].func) {
		case LCM_FUNC_GPIO:
			memcpy(&(lcm_data[i].data_t1), tmp + 3,
			       lcm_data[i].size);
			break;

		case LCM_FUNC_I2C:
			memcpy(&(lcm_data[i].data_t2), tmp + 3,
			       lcm_data[i].size);
			break;

		case LCM_FUNC_UTIL:
			memcpy(&(lcm_data[i].data_t1), tmp + 3,
			       lcm_data[i].size);
			break;

		case LCM_FUNC_CMD:
			switch (lcm_data[i].type) {
			case LCM_UTIL_WRITE_CMD_V1:
				memcpy(&(lcm_data[i].data_t5), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_UTIL_WRITE_CMD_V2:
				memcpy(&(lcm_data[i].data_t3), tmp + 3,
				       lcm_data[i].size);
				break;

			default:
				DISPERR("%s/%d: %d\n", __FILE__, __LINE__,
					lcm_data[i].type);
				return;
			}
			break;

		default:
			DISPERR("%s/%d: %d\n",
				__FILE__, __LINE__, lcm_data[i].func);
			return;
		}
		tmp_len = tmp_len + lcm_data[i].size;

		if (tmp_len < len) {
			tmp = tmp + tmp_len;
			len = len - tmp_len;
		} else {
			break;
		}
	}
	lcm_dts->init_size = i + 1;
	if (lcm_dts->init_size > INIT_SIZE) {
		DISPERR("%s:%d: LCM init table overflow: %d\n",
			__FILE__, __LINE__, len);
		return;
	}

	/* parse LCM compare_id table */
	len = disp_of_getprop_u8(np, "compare_id", dts);
	if (len <= 0) {
		pr_debug("%s:%d: Cannot find LCM compare_id table, skip it!\n",
			 __FILE__, __LINE__);
	} else {
		if (len > (sizeof(LCM_DATA) * COMPARE_ID_SIZE)) {
			DISPERR("%s:%d: LCM compare_id table overflow: %d\n",
				__FILE__, __LINE__, len);
			return;
		}

		tmp = dts;
		lcm_data = lcm_dts->compare_id;
		for (i = 0; i < COMPARE_ID_SIZE; i++) {
			lcm_data[i].func = (*tmp) & 0xFF;
			lcm_data[i].type = (*(tmp + 1)) & 0xFF;
			lcm_data[i].size = (*(tmp + 2)) & 0xFF;
			tmp_len = 3;

			switch (lcm_data[i].func) {
			case LCM_FUNC_GPIO:
				memcpy(&(lcm_data[i].data_t1), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_I2C:
				memcpy(&(lcm_data[i].data_t2), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_UTIL:
				memcpy(&(lcm_data[i].data_t1), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_CMD:
				switch (lcm_data[i].type) {
				case LCM_UTIL_WRITE_CMD_V1:
					memcpy(&(lcm_data[i].data_t5), tmp + 3,
					       lcm_data[i].size);
					break;

				case LCM_UTIL_WRITE_CMD_V2:
					memcpy(&(lcm_data[i].data_t3), tmp + 3,
					       lcm_data[i].size);
					break;

				case LCM_UTIL_READ_CMD_V2:
					memcpy(&(lcm_data[i].data_t4), tmp + 3,
					       lcm_data[i].size);
					break;

				default:
					DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
						(unsigned int)lcm_data[i].type);
					return;
				}
				break;

			default:
				DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
					(unsigned int)lcm_data[i].func);
				return;
			}
			tmp_len = tmp_len + lcm_data[i].size;

			if (tmp_len < len) {
				tmp = tmp + tmp_len;
				len = len - tmp_len;
			} else {
				break;
			}
		}
		lcm_dts->compare_id_size = i + 1;
		if (lcm_dts->compare_id_size > COMPARE_ID_SIZE) {
			DISPERR("%s:%d: LCM compare_id table overflow: %d\n",
				__FILE__, __LINE__, len);
			return;
		}
	}

	/* parse LCM suspend table */
	len = disp_of_getprop_u8(np, "suspend", dts);
	if (len <= 0) {
		DISPERR("%s:%d: Cannot find LCM suspend table, cannot skip it!\n",
			__FILE__, __LINE__);
		return;
	}
	if (len > (sizeof(LCM_DATA) * SUSPEND_SIZE)) {
		DISPERR("%s:%d: LCM suspend table overflow: %d\n",
			__FILE__, __LINE__, len);
		return;
	}

	tmp = dts;
	lcm_data = lcm_dts->suspend;
	for (i = 0; i < SUSPEND_SIZE; i++) {
		lcm_data[i].func = (*tmp) & 0xFF;
		lcm_data[i].type = (*(tmp + 1)) & 0xFF;
		lcm_data[i].size = (*(tmp + 2)) & 0xFF;
		tmp_len = 3;

		switch (lcm_data[i].func) {
		case LCM_FUNC_GPIO:
			memcpy(&(lcm_data[i].data_t1), tmp + 3,
			       lcm_data[i].size);
			break;

		case LCM_FUNC_I2C:
			memcpy(&(lcm_data[i].data_t2), tmp + 3,
			       lcm_data[i].size);
			break;

		case LCM_FUNC_UTIL:
			memcpy(&(lcm_data[i].data_t1), tmp + 3,
			       lcm_data[i].size);
			break;

		case LCM_FUNC_CMD:
			switch (lcm_data[i].type) {
			case LCM_UTIL_WRITE_CMD_V1:
				memcpy(&(lcm_data[i].data_t5), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_UTIL_WRITE_CMD_V2:
				memcpy(&(lcm_data[i].data_t3), tmp + 3,
				       lcm_data[i].size);
				break;

			default:
				DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
					lcm_data[i].type);
				return;
			}
			break;

		default:
			DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
				(unsigned int)lcm_data[i].func);
			return;
		}
		tmp_len = tmp_len + lcm_data[i].size;

		if (tmp_len < len) {
			tmp = tmp + tmp_len;
			len = len - tmp_len;
		} else {
			break;
		}
	}
	lcm_dts->suspend_size = i + 1;
	if (lcm_dts->suspend_size > SUSPEND_SIZE) {
		DISPERR("%s:%d: LCM suspend table overflow: %d\n",
			__FILE__, __LINE__, len);
		return;
	}

	/* parse LCM backlight table */
	len = disp_of_getprop_u8(np, "backlight", dts);
	if (len <= 0) {
		DISPERR("%s:%d: Cannot find LCM backlight table, skip it!\n",
			__FILE__, __LINE__);
	} else {
		if (len > (sizeof(LCM_DATA) * BACKLIGHT_SIZE)) {
			DISPERR("%s:%d: LCM backlight table overflow: %d\n",
				__FILE__, __LINE__, len);
			return;
		}

		tmp = dts;
		lcm_data = lcm_dts->backlight;
		for (i = 0; i < BACKLIGHT_SIZE; i++) {
			lcm_data[i].func = (*tmp) & 0xFF;
			lcm_data[i].type = (*(tmp + 1)) & 0xFF;
			lcm_data[i].size = (*(tmp + 2)) & 0xFF;
			tmp_len = 3;

			switch (lcm_data[i].func) {
			case LCM_FUNC_GPIO:
				memcpy(&(lcm_data[i].data_t1), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_I2C:
				memcpy(&(lcm_data[i].data_t2), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_UTIL:
				memcpy(&(lcm_data[i].data_t1), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_CMD:
				switch (lcm_data[i].type) {
				case LCM_UTIL_WRITE_CMD_V2:
					memcpy(&(lcm_data[i].data_t3), tmp + 3,
					       lcm_data[i].size);
					break;

				default:
					DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
						lcm_data[i].type);
					return;
				}
				break;

			default:
				DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
					(unsigned int)lcm_data[i].func);
				return;
			}
			tmp_len = tmp_len + lcm_data[i].size;

			if (tmp_len < len) {
				tmp = tmp + tmp_len;
				len = len - tmp_len;
			} else {
				break;
			}
		}
		lcm_dts->backlight_size = i + 1;
		if (lcm_dts->backlight_size > BACKLIGHT_SIZE) {
			DISPERR("%s:%d: LCM backlight table overflow: %d\n",
				__FILE__, __LINE__, len);
			return;
		}
	}

	/* parse LCM backlight cmdq table */
	len = disp_of_getprop_u8(np, "backlight_cmdq", dts);
	if (len <= 0) {
		DISPERR("%s:%d: Cannot find LCM backlight cmdq table, skip it!\n",
			__FILE__, __LINE__);
	} else {
		if (len > (sizeof(LCM_DATA) * BACKLIGHT_CMDQ_SIZE)) {
			DISPERR("%s:%d: LCM backlight cmdq table overflow: %d\n",
				__FILE__, __LINE__, len);
			return;
		}

		tmp = dts;
		lcm_data = lcm_dts->backlight_cmdq;
		for (i = 0; i < BACKLIGHT_CMDQ_SIZE; i++) {
			lcm_data[i].func = (*tmp) & 0xFF;
			lcm_data[i].type = (*(tmp + 1)) & 0xFF;
			lcm_data[i].size = (*(tmp + 2)) & 0xFF;
			tmp_len = 3;

			switch (lcm_data[i].func) {
			case LCM_FUNC_GPIO:
				memcpy(&(lcm_data[i].data_t1), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_I2C:
				memcpy(&(lcm_data[i].data_t2), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_UTIL:
				memcpy(&(lcm_data[i].data_t1), tmp + 3,
				       lcm_data[i].size);
				break;

			case LCM_FUNC_CMD:
				switch (lcm_data[i].type) {
				case LCM_UTIL_WRITE_CMD_V23:
					memcpy(&(lcm_data[i].data_t3),
					       tmp + 3, lcm_data[i].size);
					break;

				default:
					DISPERR("%s:%d: %d\n",
						__FILE__, __LINE__,
						lcm_data[i].type);
					return;
				}
				break;

			default:
				DISPERR("%s:%d: %d\n", __FILE__, __LINE__,
					(unsigned int)lcm_data[i].func);
				return;
			}
			tmp_len = tmp_len + lcm_data[i].size;

			if (tmp_len < len) {
				tmp = tmp + tmp_len;
				len = len - tmp_len;
			} else {
				break;
			}
		}
		lcm_dts->backlight_cmdq_size = i + 1;
		if (lcm_dts->backlight_cmdq_size > BACKLIGHT_CMDQ_SIZE) {
			DISPERR("%s:%d: LCM backlight cmdq table overflow: %d\n",
				__FILE__, __LINE__, len);
			return;
		}
	}
}

int check_lcm_node_from_DT(void)
{
	char lcm_node[128] = {0};
	struct device_node *np = NULL;

	sprintf(lcm_node, "mediatek,lcm_params-%s", lcm_name_list[0]);
	DISPMSG("LCM PARAMS DT compatible: %s\n", lcm_node);

	/* Load LCM parameters from DT */
	np = of_find_compatible_node(NULL, NULL, lcm_node);
	if (!np) {
		DISPERR("LCM PARAMS DT node: Not found\n");
		return -1;
	}

	sprintf(lcm_node, "mediatek,lcm_ops-%s", lcm_name_list[0]);
	DISPMSG("LCM OPS DT compatible: %s\n", lcm_node);

	/* Load LCM parameters from DT */
	np = of_find_compatible_node(NULL, NULL, lcm_node);
	if (!np) {
		DISPERR("LCM OPS DT node: Not found\n");
		return -1;
	}

	return 0;
}

void load_lcm_resources_from_DT(struct LCM_DRIVER *lcm_drv)
{
	char lcm_node[128] = {0};
	struct device_node *np = NULL;
	unsigned char *tmp_dts = dts;
	LCM_DTS *parse_dts = &lcm_dts;

	if (!lcm_drv) {
		DISPERR("%s:%d: Error access to LCM_DRIVER(NULL)\n",
			__FILE__, __LINE__);
		return;
	}

	memset((unsigned char *)parse_dts, 0x0, sizeof(LCM_DTS));

	sprintf(lcm_node, "mediatek,lcm_params-%s", lcm_name_list[0]);
	DISPMSG("LCM PARAMS DT compatible: %s\n", lcm_node);

	/* Load LCM parameters from DT */
	np = of_find_compatible_node(NULL, NULL, lcm_node);
	if (!np)
		DISPERR("LCM PARAMS DT node: Not found\n");
	else
		parse_lcm_params_dt_node(np, &(parse_dts->params));

	sprintf(lcm_node, "mediatek,lcm_ops-%s", lcm_name_list[0]);
	DISPMSG("LCM OPS DT compatible: %s\n", lcm_node);

	/* Load LCM parameters from DT */
	np = of_find_compatible_node(NULL, NULL, lcm_node);
	if (!np)
		DISPERR("LCM OPS DT node: Not found\n");
	else
		parse_lcm_ops_dt_node(np, parse_dts, tmp_dts);

	if (lcm_drv->parse_dts)
		lcm_drv->parse_dts(parse_dts, 1);
	else
		DISPERR("LCM set_params not implemented!!!\n");
}
#endif

struct disp_lcm_handle *disp_lcm_probe(char *plcm_name,
		enum LCM_INTERFACE_ID lcm_id, int is_lcm_inited)
{
	int lcmindex = 0;
	bool isLCMFound = false;
	bool isLCMInited = false;

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
	bool isLCMDtFound = false;
#endif

	struct LCM_DRIVER *lcm_drv = NULL;
	struct LCM_PARAMS *lcm_param = NULL;
	struct disp_lcm_handle *plcm = NULL;

	DISPFUNC();
	DISPCHECK("plcm_name=%s is_lcm_inited %d\n", plcm_name, is_lcm_inited);

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
	if (check_lcm_node_from_DT() == 0) {
		lcm_drv = &lcm_common_drv;
		lcm_drv->name = lcm_name_list[0];
		if (strcmp(lcm_drv->name, plcm_name)) {
			DISPERR("FATAL ERROR!!!LCM Driver defined in kernel(%s) is different with LK(%s)\n",
				lcm_drv->name, plcm_name);
			return NULL;
		}

		isLCMInited = true;
		isLCMFound = true;
		isLCMDtFound = true;

		if (!is_lcm_inited) {
			isLCMFound = true;
			isLCMInited = false;
		}

		lcmindex = 0;
	}
#endif
	if (_lcm_count() == 0) {
		DISPERR("no lcm driver defined in linux kernel driver\n");
		return NULL;
	} else if (_lcm_count() == 1) {
		if (plcm_name == NULL) {
			lcm_drv = lcm_driver_list[0];

			isLCMFound = true;
			isLCMInited = false;
			DISPCHECK("LCM Name NULL\n");
		} else {
			lcm_drv = lcm_driver_list[0];
			if (strcmp(lcm_drv->name, plcm_name)) {
				DISPERR("FATAL ERROR!!!LCM Driver defined in kernel(%s) is different with LK(%s)\n",
					lcm_drv->name, plcm_name);
				return NULL;
			}

			isLCMInited = true;
			isLCMFound = true;
		}

		if (!is_lcm_inited) {
			isLCMFound = true;
			isLCMInited = false;
		}

		lcmindex = 0;
	} else {
		if (plcm_name == NULL) {
			/* TODO: we need to detect all the lcm driver */
		} else {
			int i = 0;

			for (i = 0; i < _lcm_count(); i++) {
				lcm_drv = lcm_driver_list[i];
				if (!strcmp(lcm_drv->name, plcm_name)) {
					isLCMFound = true;
					isLCMInited = true;
					lcmindex = i;
					break;
				}
			}
			if (!isLCMFound) {
				DISPERR("FATAL ERROR: can't found lcm driver:%s in linux kernel driver\n",
					plcm_name);
			} else if (!is_lcm_inited) {
				isLCMInited = false;
			}
		}
		/* TODO: */
	}

	if (isLCMFound == false) {
		DISPERR("FATAL ERROR!!!No LCM Driver defined\n");
		return NULL;
	}

	plcm = kzalloc(sizeof(uint8_t *) * sizeof(struct disp_lcm_handle),
		       GFP_KERNEL);
	lcm_param = kzalloc(sizeof(uint8_t *) * sizeof(struct LCM_PARAMS),
			    GFP_KERNEL);
	if (plcm && lcm_param) {
		plcm->params = lcm_param;
		plcm->drv = lcm_drv;
		plcm->is_inited = isLCMInited;
		plcm->index = lcmindex;
	} else {
		DISPERR("FATAL ERROR!!!kzalloc plcm and plcm->params failed\n");
		goto FAIL;
	}

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
	if (isLCMDtFound == true)
		load_lcm_resources_from_DT(plcm->drv);
#endif

	{
		plcm->drv->get_params(plcm->params);
		plcm->lcm_if_id = plcm->params->lcm_if;

		/* below code is for lcm driver forward compatible */
		if (plcm->params->type == LCM_TYPE_DSI &&
		    plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED)
			plcm->lcm_if_id = LCM_INTERFACE_DSI0;
		if (plcm->params->type == LCM_TYPE_DPI &&
		    plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED)
			plcm->lcm_if_id = LCM_INTERFACE_DPI0;
		if (plcm->params->type == LCM_TYPE_DBI &&
		    plcm->params->lcm_if == LCM_INTERFACE_NOTDEFINED)
			plcm->lcm_if_id = LCM_INTERFACE_DBI0;

		if ((lcm_id == LCM_INTERFACE_NOTDEFINED) || lcm_id == plcm->lcm_if_id) {
			plcm->lcm_original_width = plcm->params->width;
			plcm->lcm_original_height = plcm->params->height;
			_dump_lcm_info(plcm);
			return plcm;
		}

		DISPERR("the specific LCM Interface [%d] didn't define any lcm driver\n",
			lcm_id);
		goto FAIL;
	}

FAIL:

	kfree(plcm);
	kfree(lcm_param);
	return NULL;
}

int disp_lcm_init(struct disp_lcm_handle *plcm, int force)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->init_power) {
			if (!disp_lcm_is_inited(plcm) || force) {
				DISPMSG("lcm init power()\n");
				lcm_drv->init_power();
			}
		}

		if (lcm_drv->init) {
			if (!disp_lcm_is_inited(plcm) || force) {
				DISPMSG("lcm init()\n");
				lcm_drv->init();
			}
		} else {
			DISPERR("FATAL ERROR, lcm_drv->init is null\n");
			return -1;
		}
#if 0
		if (plcm->params->type == LCM_TYPE_DSI) {
			int ret = 0;
			char buffer = 0;

			ret = DSI_dcs_read_lcm_reg_v2(DISP_MODULE_DSI0, NULL, 0x0A, &buffer, 1);
			if (ret == 0)
				DISPMSG("lcm is not connected\n");
			else
				DISPMSG("lcm is connected\n");

		}
#endif
		/* ddp_dsi_start(DISP_MODULE_DSI0, NULL); */
		/* DSI_BIST_Pattern_Test(DISP_MODULE_DSI0,NULL,true, 0x00ffff00); */
		return 0;
	}
	{
		DISPERR("plcm is null\n");
		return -1;
	}
}

struct LCM_PARAMS *disp_lcm_get_params(struct disp_lcm_handle *plcm)
{
	/* DISPFUNC(); */

	if (_is_lcm_inited(plcm))
		return plcm->params;
	else
		return NULL;
}

enum LCM_INTERFACE_ID disp_lcm_get_interface_id(struct disp_lcm_handle *plcm)
{
	DISPFUNC();

	if (_is_lcm_inited(plcm))
		return plcm->lcm_if_id;
	else
		return LCM_INTERFACE_NOTDEFINED;
}

int disp_lcm_update(struct disp_lcm_handle *plcm, int x, int y, int w, int h,
		    int force)
{
	struct LCM_DRIVER *lcm_drv = NULL;
	/* enum LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED; */
	/* struct LCM_PARAMS *plcm_param = NULL; */

	DISPDBGFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->update) {
			lcm_drv->update(x, y, w, h);
		} else {
			if (!disp_lcm_is_video_mode(plcm))
				DISPERR("FATAL ERROR, lcm is cmd mode lcm_drv->update is null\n");
			return -1;
		}

		return 0;
	}
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

/* return 1: esd check fail */
/* return 0: esd check pass */
int disp_lcm_esd_check(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->esd_check)
			return lcm_drv->esd_check();

		DISPERR("FATAL ERROR, lcm_drv->esd_check is null\n");
		return 0;

	} else {
		DISPERR("lcm_drv is null\n");
		return 0;
	}
}

int disp_lcm_esd_recover(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->esd_recover) {
			lcm_drv->esd_recover();
		} else {
			DISPERR("FATAL ERROR, lcm_drv->esd_check is null\n");
			return -1;
		}

		return 0;
	}
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

int disp_lcm_suspend(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->suspend) {
			lcm_drv->suspend();
		} else {
			DISPERR("FATAL ERROR, lcm_drv->suspend is null\n");
			return -1;
		}

		if (lcm_drv->suspend_power)
			lcm_drv->suspend_power();

		return 0;
	}
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

int disp_lcm_resume(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;

		if (lcm_drv->resume_power)
			lcm_drv->resume_power();

		if (lcm_drv->resume) {
			lcm_drv->resume();
		} else {
			DISPERR("FATAL ERROR, lcm_drv->resume is null\n");
			return -1;
		}

		return 0;
	}
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

int disp_lcm_aod(struct disp_lcm_handle *plcm, int enter)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPMSG("%s, enter:%d\n", __func__, enter);
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->aod) {
			lcm_drv->aod(enter);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->aod is null\n");
			return -1;
		}
		return 0;
	}

	DISPERR("lcm_drv is null\n");
	return -1;
}

int disp_lcm_is_support_adjust_fps(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->adjust_fps)
			return 1;
		else
			return 0;
	}
	DISPERR("lcm not initialied\n");
	return 0;
}

int disp_lcm_adjust_fps(void *cmdq, struct disp_lcm_handle *plcm, int fps)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->adjust_fps) {
			lcm_drv->adjust_fps(cmdq, fps, plcm->params);
			return 0;
		}
	}
	DISPERR("lcm not initialied\n");
	return -1;
}

int disp_lcm_set_backlight(struct disp_lcm_handle *plcm,
			   void *handle, int level)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->set_backlight_cmdq) {
			lcm_drv->set_backlight_cmdq(handle, level);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->set_backlight is null\n");
			return -1;
		}

		return 0;
	}
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

int disp_lcm_ioctl(struct disp_lcm_handle *plcm,
		   enum LCM_IOCTL ioctl, unsigned int arg)
{
	return 0;
}

int disp_lcm_is_inited(struct disp_lcm_handle *plcm)
{
	if (_is_lcm_inited(plcm))
		return plcm->is_inited;
	else
		return 0;
}

unsigned int disp_lcm_ATA(struct disp_lcm_handle *plcm)
{
	unsigned int ret = 0;
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->ata_check) {

			ret = lcm_drv->ata_check(NULL);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->ata_check is null\n");
			return 0;
		}

		return ret;
	}
	{
		DISPERR("lcm_drv is null\n");
		return 0;
	}
}

void *disp_lcm_switch_mode(struct disp_lcm_handle *plcm, int mode)
{
	struct LCM_DRIVER *lcm_drv = NULL;
	struct LCM_DSI_MODE_SWITCH_CMD *lcm_cmd = NULL;
	void *ret;

	/* DISPFUNC(); */
	if (_is_lcm_inited(plcm)) {
		if (plcm->params->dsi.switch_mode_enable == 0) {
			DISPERR(" ERROR, Not enable switch in lcm_get_params function\n");
			return NULL;
		}
		lcm_drv = plcm->drv;
		if (lcm_drv->switch_mode) {
			ret = lcm_drv->switch_mode(mode);
			lcm_cmd = (struct LCM_DSI_MODE_SWITCH_CMD *)ret;
			lcm_cmd->cmd_if =
				(unsigned int)(plcm->params->lcm_cmd_if);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->switch_mode is null\n");
			return NULL;
		}

		return (void *)(lcm_cmd);
	}
	{
		DISPERR("lcm_drv is null\n");
		return NULL;
	}
}

int disp_lcm_is_video_mode(struct disp_lcm_handle *plcm)
{
	struct LCM_PARAMS *lcm_param = NULL;
	/* enum LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED; */

	/* DISPFUNC(); */
	if (_is_lcm_inited(plcm))
		lcm_param = plcm->params;
	else
		return -1;

	switch (lcm_param->type) {
	case LCM_TYPE_DBI:
		return 0;
	case LCM_TYPE_DSI:
		break;
	case LCM_TYPE_DPI:
		return 1;
	default:
		DISPERR("[LCM] TYPE: unknown\n");
		break;
	}

	if (lcm_param->type == LCM_TYPE_DSI) {
		switch (lcm_param->dsi.mode) {
		case CMD_MODE:
			return 0;
		case SYNC_PULSE_VDO_MODE:
		case SYNC_EVENT_VDO_MODE:
		case BURST_VDO_MODE:
			return 1;
		default:
			DISPERR("[LCM] DSI Mode: Unknown\n");
			break;
		}
	}

	disp_aee_db_print("LCM parmas is error, type=%d\n", lcm_param->type);
	return -1;
}

int disp_lcm_set_lcm_cmd(struct disp_lcm_handle *plcm, void *cmdq_handle,
			 unsigned int *lcm_cmd, unsigned int *lcm_count,
			 unsigned int *lcm_value)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	DISPFUNC();
	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->set_lcm_cmd) {
			lcm_drv->set_lcm_cmd(cmdq_handle, lcm_cmd, lcm_count,
					     lcm_value);
		} else {
			DISPERR("FATAL ERROR, lcm_drv->set_lcm_cmd is null\n");
			return -1;
		}

		return 0;
	}
	{
		DISPERR("lcm_drv is null\n");
		return -1;
	}
}

int disp_lcm_is_partial_support(struct disp_lcm_handle *plcm)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->validate_roi)
			return 1;
	}
	return 0;
}

int disp_lcm_validate_roi(struct disp_lcm_handle *plcm, int *x, int *y,
			  int *w, int *h)
{
	struct LCM_DRIVER *lcm_drv = NULL;

	if (_is_lcm_inited(plcm)) {
		lcm_drv = plcm->drv;
		if (lcm_drv->validate_roi) {
			lcm_drv->validate_roi(x, y, w, h);
			return 0;
		}
		DISPERR("Not support partial roi\n");
		return -1;
	}
	DISPERR("validate roi lcm_drv is null\n");
	return -1;
}
