/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>

#include "dsi_v2.h"
#include "dsi_io_v2.h"
#include "dsi_host_v2.h"
#include "mdss_debug.h"

extern void mdss_dsi_set_tx_power_mode(int mode, struct mdss_panel_data *pdata);
extern int mdss_dsi_cmd_dma_tx(struct mdss_dsi_ctrl_pdata *ctrl,
					struct dsi_buf *tp);

extern int mdss_dsi_cmd_dma_rx(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_buf *rp, int rlen);

/*--------------All LCM Panels----------------*/
static unsigned int dsi_panel_lcm_id = 0;
static int __init board_lcd_name_setup(char *lcdname)
{
	get_option(&lcdname, &dsi_panel_lcm_id);
	return 1;
}
early_param("lcm.id", board_lcd_name_setup);

static unsigned int mdss_fb_detect_panel(void)
{
	return dsi_panel_lcm_id;
}

/*---------------------------HX8394D START---------------------------*/
#define TRULYHX8394D_ID 		0x8394
static char hx8394d_panel_status_cmd[] = {0x09, 0x00, 0x06, 0xA0};
static char hx8394d_panel_status_cmd1[] = {0x45, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc hx8394d_status_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(hx8394d_panel_status_cmd)},
		hx8394d_panel_status_cmd};
static struct dsi_cmd_desc hx8394d_status_read_cmd1 = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(hx8394d_panel_status_cmd1)},
		hx8394d_panel_status_cmd1};

static int dsi_hx8394d_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;
	u32 panel_status = 0;
	u32 panel_status1 = 0;
	int rx_ret = 0;
	int rx_ret1 = 0;
	int ret = 1;
	unsigned int panel_id = mdss_fb_detect_panel();

	memset(rp->data, 0, 4);
	mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);
	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &hx8394d_status_read_cmd, 3);
	if (rx_ret)
		panel_status = ((u32)(rp->data[0])<<24) | ((u32)(rp->data[1])<<16) |
					((u32)(rp->data[2])<<8) | ((u32)(rp->data[3]));

	rx_ret1 = mdss_dsi_cmds_rx(ctrl_pdata, &hx8394d_status_read_cmd1, 3);
	if (rx_ret1)
		panel_status1 = ((u32)(rp->data[0])<<8) | ((u32)(rp->data[1]));

	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);
	if (panel_status != 0x80730400 || !(panel_status1 == 0x0510 || panel_status1 == 0x050b)) {
		printk("%s hx8394d panel_id=0x%x, panel_status = 0x%08x, panel_status1 = 0x%08x\n", __func__, panel_id, panel_status, panel_status1);
		ret = 0;
	}

	return ret;
}
/*---------------------------HX8394D END---------------------------*/



/*----ili9806c----*/
#define ILI9806C_ID 0x9816
static char ili9806c_panel_id_cmd[] = {0xD3, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc ili9806c_id_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(ili9806c_panel_id_cmd)},
		ili9806c_panel_id_cmd};

static char ili9806c_panel_status_cmd[] = {0x09, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc ili9806c_status_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(ili9806c_panel_status_cmd)},
		ili9806c_panel_status_cmd};

static int dsi_ili9806c_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;
	u32 panel_id = 0;
	u32 panel_status = 0;
	int rx_ret = 0;
	int ret = 1;

	memset(rp->data, 0, 4);
	mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);
	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &ili9806c_id_read_cmd, 4);
	if (!rx_ret && rp->len > 0) {
		panel_id = ((u32)(rp->data[0])<<24) | ((u32)(rp->data[1])<<16) |
					((u32)(rp->data[2])<<8) | rp->data[3];
	}
	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &ili9806c_status_read_cmd, 3);
	if (!rx_ret && rp->len > 0) {
		panel_status = ((u32)(rp->data[0])<<16) | ((u32)(rp->data[1])<<8) |
					((u32)(rp->data[2]));
	}
	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);
	if (panel_status != 0x807304 || panel_id != 0x00981600) {
		pr_info("%s ili9806c Panel ID =0x%08x, panel_status = 0x%08x\n", __func__, panel_id, panel_status);
		ret = 0;
	}

	return ret;
}


/*----ili9806e----*/
#define ILI9806E_ID	0x980604
static char ili9806e_panel_page0_cmd[] = {0xFF, 0xFF, 0x98, 0x06, 0x04, 0x00};
static struct dsi_cmd_desc ili9806e_page0_cmd = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ili9806e_panel_page0_cmd)},
		ili9806e_panel_page0_cmd};
static char ili9806e_panel_page0_0x0A_cmd[] = {0x0A, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc ili9806e_page0_read_0x0A_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(ili9806e_panel_page0_0x0A_cmd)},
		ili9806e_panel_page0_0x0A_cmd};

static char ili9806e_panel_page0_0x0B_cmd[] = {0x0B, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc ili9806e_page0_read_0x0B_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(ili9806e_panel_page0_0x0B_cmd)},
		ili9806e_panel_page0_0x0B_cmd};

static char ili9806e_panel_page0_0x0C_cmd[] = {0x0C, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc ili9806e_page0_read_0x0C_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(ili9806e_panel_page0_0x0C_cmd)},
		ili9806e_panel_page0_0x0C_cmd};

static char ili9806e_panel_page0_0x0D_cmd[] = {0x0D, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc ili9806e_page0_read_0x0D_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(ili9806e_panel_page0_0x0D_cmd)},
		ili9806e_panel_page0_0x0D_cmd};

static int dsi_ili9806e_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;

	u32 panel_status_page0 = 0;
	int rx_ret = 0;
	int ret = 1;

	memset(rp->data, 0, 4);

	mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);
	rx_ret = mdss_dsi_cmds_tx(ctrl_pdata, &ili9806e_page0_cmd, 1);
	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &ili9806e_page0_read_0x0A_cmd, 1);
	if (rx_ret > 0)
		panel_status_page0 = (u32)(rp->data[0])<<24;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &ili9806e_page0_read_0x0B_cmd, 1);
	if (rx_ret > 0)
		panel_status_page0 |= (u32)(rp->data[0])<<16;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &ili9806e_page0_read_0x0C_cmd, 1);
	if (rx_ret > 0)
		panel_status_page0 |= (u32)(rp->data[0])<<8;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &ili9806e_page0_read_0x0D_cmd, 1);
	if (rx_ret > 0)
		panel_status_page0 |= (u32)(rp->data[0]);

	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);


	if ((panel_status_page0&0xFFFFFFFF) != 0x9C007020) {
		pr_info("%s panel_status_page0 = 0x%08x\n", __func__, panel_status_page0);
		ret = 0;
	}

	return ret;
}


/*----otm8018b----*/
#define OTM8018B_ID		0x28009
static char otm8018b_panel_status_1_cmd[] = {0x0A, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8018b_status_1_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8018b_panel_status_1_cmd)},
		otm8018b_panel_status_1_cmd};

static char otm8018b_panel_status_2_cmd[] = {0x0B, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8018b_status_2_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8018b_panel_status_2_cmd)},
		otm8018b_panel_status_2_cmd};

static char otm8018b_panel_status_3_cmd[] = {0x0D, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8018b_status_3_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8018b_panel_status_3_cmd)},
		otm8018b_panel_status_3_cmd};

static char otm8018b_panel_offset00_cmd[] = {0x00, 0x00};
static struct dsi_cmd_desc ili9806e_offset00_cmd = {
       {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(otm8018b_panel_offset00_cmd)},
		otm8018b_panel_offset00_cmd};
static char otm8018b_panel_offset01_cmd[] = {0x00, 0x01};
static struct dsi_cmd_desc ili9806e_offset01_cmd = {
       {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(otm8018b_panel_offset01_cmd)},
		otm8018b_panel_offset01_cmd};
static char otm8018b_panel_status_4_cmd[] = {0xD9, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8018b_status_4_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8018b_panel_status_4_cmd)},
		otm8018b_panel_status_4_cmd};
static char otm8018b_panel_status_5_cmd[] = {0xD8, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8018b_status_5_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8018b_panel_status_5_cmd)},
		otm8018b_panel_status_5_cmd};

static int dsi_otm8018b_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;
	u32 panel_status_1 = 0;
	u32 panel_status_2 = 0;
	int rx_ret = 0;
	int ret = 1;

	memset(rp->data, 0, 4);
	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8018b_status_1_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_1 = (u32)(rp->data[0])<<24;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8018b_status_2_read_cmd, 1);
	if (rx_ret > 0)
		panel_status_1 |= (u32)(rp->data[0])<<16;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8018b_status_3_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_1 |= (u32)(rp->data[0])<<8;

	rx_ret = mdss_dsi_cmds_tx(ctrl_pdata, &ili9806e_offset00_cmd, 1);

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8018b_status_4_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_1 |= (u32)(rp->data[0]);

	rx_ret = mdss_dsi_cmds_tx(ctrl_pdata, &ili9806e_offset00_cmd, 1);

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8018b_status_5_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_2 = (u32)(rp->data[0])<<8;

	rx_ret = mdss_dsi_cmds_tx(ctrl_pdata, &ili9806e_offset01_cmd, 1);

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8018b_status_5_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_2 |= (u32)(rp->data[0]);

	if (((panel_status_1&0xFFFFFFFF) != 0x9C00003E) || ((panel_status_2&0x0000FFFF) != 0x00005F5F)) {
		pr_info("%s panel_status_1 = 0x%08x, panel_status_2 = 0x%08x, rp->len=%d, rx_ret=%d\n", __func__, panel_status_1, panel_status_2, rp->len, rx_ret);
		ret = 0;
	}

	return ret;
}


/*----otm8018b----*/
#define OTM8019A_ID		0x8019
static char otm8019a_panel_status_1_cmd[] = {0x0A, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8019a_status_1_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8019a_panel_status_1_cmd)},
		otm8019a_panel_status_1_cmd};

static char otm8019a_panel_status_2_cmd[] = {0x0B, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8019a_status_2_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8019a_panel_status_2_cmd)},
		otm8019a_panel_status_2_cmd};

static char otm8019a_panel_status_3_cmd[] = {0x0D, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm8019a_status_3_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm8019a_panel_status_3_cmd)},
		otm8019a_panel_status_3_cmd};

static int dsi_otm8019a_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;
	u32 panel_status_1 = 0;
	int rx_ret = 0;
	int ret = 1;

	memset(rp->data, 0, 4);

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8019a_status_1_read_cmd, 0);
	if (rp->len > 0)
		panel_status_1 = (u32)(rp->data[0])<<24;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8019a_status_2_read_cmd, 0);
	if (rp->len > 0)
		panel_status_1 |= (u32)(rp->data[0])<<16;

	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm8019a_status_3_read_cmd, 0);
	if (rp->len > 0)
		panel_status_1 |= (u32)(rp->data[0])<<8;

	if ((panel_status_1&0xFFFFFFFF) != 0x9C000000) {
		pr_info("%s panel_status_1 = 0x%08x, rp->len=%d, rx_ret=%d\n", __func__, panel_status_1, rp->len, rx_ret);
		ret = 0;
	}

	return ret;
}

#define OTM1283A_ID		0x1283
static char otm1283a_panel_status_1_cmd[] = {0x0A, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc otm1283a_status_1_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(otm1283a_panel_status_1_cmd)},
		otm1283a_panel_status_1_cmd};
static int dsi_otm1283a_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;
	u32 panel_status_1 = 0;
	int rx_ret = 0;
	int ret = 1;
	memset(rp->data, 0, 4);
	mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);
	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &otm1283a_status_1_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_1 = (u32)(rp->data[0]);

	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);
	if ((panel_status_1&0x00FF) != 0x9C) {
		pr_info("%s:%d otm1283a panel_status_0x0A =0x%x, rlen=%d, rx_ret=%d\n", __func__, __LINE__, panel_status_1, rp->len, rx_ret);
		ret = 0;
	}
		pr_info("%s:%d otm1283a panel_status_0x0A =0x%x, rlen=%d, rx_ret=%d\n", __func__, __LINE__, panel_status_1, rp->len, rx_ret);
	return ret;
}

#define NT35592_ID		0x9200
static char nt35592_panel_status_1_cmd[] = {0x0A, 0x00, 0x06, 0xA0};
static struct dsi_cmd_desc nt35592_status_1_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(nt35592_panel_status_1_cmd)},
		nt35592_panel_status_1_cmd};
static int dsi_nt35592_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dsi_buf *rp = &ctrl_pdata->rx_buf;
	u32 panel_status_1 = 0;
	int rx_ret = 0;
	int ret = 1;
	memset(rp->data, 0, 4);
	mdss_dsi_set_tx_power_mode(0, &ctrl_pdata->panel_data);
	rx_ret = mdss_dsi_cmds_rx(ctrl_pdata, &nt35592_status_1_read_cmd, 1);
	if (!rx_ret  && (rp->len > 0))
		panel_status_1 = (u32)(rp->data[0]);

	mdss_dsi_set_tx_power_mode(1, &ctrl_pdata->panel_data);
	if ((panel_status_1&0x00FF) != 0x9C) {
		pr_info("%s:%d nt35592 panel_status_0x0A =0x%x, rlen=%d, rx_ret=%d\n", __func__, __LINE__, panel_status_1, rp->len, rx_ret);
		ret = 0;
	}
		pr_info("%s:%d nt35592 panel_status_0x0A =0x%x, rlen=%d, rx_ret=%d\n", __func__, __LINE__, panel_status_1, rp->len, rx_ret);
	return ret;
}


/*---------dsi_oem_panel_status_check--------------
According to LCD ID to select peer lcd esd check functions.
-------------------------------------------------------------*/
int dsi_oem_panel_status_check(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	unsigned int panel_id = 0;
	int ret = 1;

	panel_id = mdss_fb_detect_panel();
	switch (panel_id) {
	case ILI9806C_ID:
		ret = dsi_ili9806c_status_check(ctrl_pdata);
		break;
	case ILI9806E_ID:
		ret = dsi_ili9806e_status_check(ctrl_pdata);
		break;
	case OTM8018B_ID:
		ret = dsi_otm8018b_status_check(ctrl_pdata);
		break;
	case OTM8019A_ID:
		ret = dsi_otm8019a_status_check(ctrl_pdata);
		break;
	case OTM1283A_ID:
		ret = dsi_otm1283a_status_check(ctrl_pdata);
		break;
		break;
	case NT35592_ID:
		ret = dsi_nt35592_status_check(ctrl_pdata);
		break;
	case TRULYHX8394D_ID:
		ret = dsi_hx8394d_status_check(ctrl_pdata);
		break;
	default:
		break;
	}

	return ret;
}

