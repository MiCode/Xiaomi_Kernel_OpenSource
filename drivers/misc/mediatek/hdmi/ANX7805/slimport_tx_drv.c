/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
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
 
#define _SP_TX_DRV_C_
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include "hdmi_drv.h"
#include "slimport.h"
#include "slimport_tx_drv.h"
#include "slimport_tx_reg.h"

#include "slimport_edid.h"
#include "slimport_edid_3d_api.h"

BYTE bEDID_extblock[128];
BYTE bEDID_firstblock[128];
BYTE bEDID_fourblock[256];
static BYTE mute_count = 0;

struct MIPI_Video_Format mipi_video_timing_table[] = {
//////////////////////pixel_clk--Htotal---H active--Vtotal--V active-HFP---HSW--HBP--VFP--VSW--VBP-----////////

//{0, "1400x1050@60",		 108,			 1688,	 1400,	   1066,	 1050,		  88,	  50,		 150,	  9,  4,   3},
	{0, "720x480@60",		 27,		 858,	 720,	  525,	   480, 	  16,	  60,		 62,  10,  6,   29},
	{1, "1280X720P@60",	   74,		   1650,	  1280,    750, 	  720,		 110,	 40,	   220, 	5,	5,	20},
	{2, "1680X1050@60",	   146,	  2240, 	1680,	  1089, 	1050,	   104,   176,	   280, 	 3,  6 , 30},
	{3, "1920x1080p@60",	  148,	  2200, 	 1920,	   1125,	 1080,		88, 	44, 	 148,	   4, 2, 39},
	{4, "1920x1080p@24",	  59, 	 2200,		1920,	  1125, 	1080,	   88,	   44,		148,	  4, 2, 39},
	{5, "1920x1080p@30",	  74,	  2200, 	 1920,	   1125,	 1080,		88, 	44, 	 148,	   4, 2, 39},
	{6, "2560x1600@60",	 268,	  2720,  2560,	   1646,	 1600,		48, 	32, 	 80,  2,  6 , 38},
//{7, "640x480@60",			25.17,			 816,		640,	   525, 	  480,		  32,	  96,	   48,	  10,  2,  33},
	{7, "640x480@60",			25,			 800,		640,	   525, 	  480,		  16,	  96,	   48,	  10,  2,  33},
//{5, "640x480@60",		10.06,			 334,		320,	   502, 	  480,		  3,	 3, 	 8,   7,  8,  7},
	{8, "640x480@75",			 31,	  840,	 640,		   500, 	  480,		  16,	  64,		120,  1,  3,  16},
	{9, "1920*1200@60",	 154,			 2080,	 1920,	   1235,	 1200,		  40,	  80,		  40,	  3,  6,  26},
	{10, "800x600@60", 	 38,  976,	 800,		645,	   600, 	  32,	  96,		  48,	  10,  2,  33},
//{11, "320x480@60",		 10.06, 		 334,		320,	   502, 	  480,		  3,	 3, 	 8,   7,  8,  7},
	{11, "320x480@60", 	 10, 		 352,		320,	   493, 	  480,		  8,	 8, 	 16,  6,  2,  5},
	{12, "1024x768@60",	 65,		 1344,		 1024,		   806, 	  768,		  24,	  136,		160,  3,  2,  33},
	{13, "1920x1200@60",	 154,		 2080,		 1920,		   1235,	  1200, 	  48,	  32,	   80,	  3,  2,  30},
	{14, "1280X1024@60",	 108,		 1688,		 1280,		   1066,	  1024, 	  48,	  112,	   248,   1,  2,  39}


};
struct Bist_Video_Format video_timing_table[] = {
//number,video_type[32],pclk,h_total,h_active, v_total,v_active,h_front,h_sync,h_BP, v_FP, v_sync,v_BP, h_polarity, v_polarity, interlaced, repeat_times, frame_rate, bpp, video_mode;

	{0, "1400x1050@60",	  	108,	        1688,	1400,     1066,     1050,	     88,     50,	    150,	 9,  4,  3,   1,  1,  0, 1,	60, 1, 0},
	{1, "1280X720P@60",       75,          1650,      1280,    750,       720,       110,    40,       220,     5,  5,  20,   0,0,  0, 1,    60, 1, 1},
	{2, "1680X1050@60",       146,    2240,     1680,     1089,     1050,      104,   176,     280,      3,  6 , 30,  0,  1, 0, 1,   60, 1, 0},
	{3, "1920x1080p@60",     148,      2200,      1920,     1125,     1080,      88,     44,      148,      4, 5, 36, 0,0,  0, 1,    60, 1, 1},
	{4, "2560x1600@60",		268,  	 2720,	2560,     1646,     1600,      48,     32,      80,	 2,  6 , 38,  1,  1, 0, 1, 	60, 1, 0},
	{5, "640x480@60",	       25,	        840,       640,	      500,       480,	     16,     64,      120,	 1,  3,  16,  1,  1,  0, 1,	75, 1, 0},
	{6, "640x480@75",	      	31,	 840,	640,	      500,       480,	     16,     64,       120,	 1,  3,  16,  1,  1,	0, 1,	75, 1, 0},
	{7, "1920*1200@60",		154,	        2080,	1920,     1235,     1200,	     40,     80,	     40,	 3,  6,  26,  1,  1, 0, 1,	60, 1, 0},
	{8, "800x600@75",	  	49,	 1056,	800,       628,       600,	     40,     128,	     88,	 3,  6,  29,  1,  1, 0, 1,	75, 1, 0},
	{9, "800x480@60",           25,          848,	800,      493,        480,       14,     8,         26,       5,  1,   1,	  1,	1, 0, 1,	60, 1,0}
};
#if(REDUCE_REPEAT_PRINT_INFO)
void loop_print_msg(BYTE msg_id)
{
	BYTE i = 0, no_msg = 0;
	if(maybe_repeat_print_info_flag == REPEAT_PRINT_INFO_CLEAR) {
		pr_info("Repeat print info clear");
		for(i = 0; i < LOOP_PRINT_MSG_MAX; i++)
			repeat_printf_info_count[i] = 0;
	}
	switch(msg_id) {
	case 0x00:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("Stream clock not found!");
		break;
	case 0x01:
		break;
	case 0x02:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("video stream not valid!");
		break;
	case 0x03:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("Stream clock not stable!");
		break;
	case 0x04:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("colorspace: %.2x, Embedded_Sync: %.2x, DE_reDenerate: %.2x,YC_MUX: %.2x ",
			        (unsigned int)SP_TX_Video_Input.bColorSpace,
			        (unsigned int)SP_TX_Video_Input.bLVTTL_HW_Interface.sEmbedded_Sync.Embedded_Sync,
			        (unsigned int)SP_TX_Video_Input.bLVTTL_HW_Interface.DE_reDenerate,
			        (unsigned int)SP_TX_Video_Input.bLVTTL_HW_Interface.sYC_MUX.YC_MUX
			       );
		break;
	case 0x05:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("****Over bandwidth**** \n");
		break;
	case 0x06:
		break;
	case 0x07:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("PLL not lock!");
		break;
	case 0x08:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("LINK_TRAINING_ERROR! \n");
		break;
	case 0x09:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("loop HDCP_END \n");
		break;
	case 0x0A:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("Video enable \n");
		break;
	case 0x0B:
		if(repeat_printf_info_count[msg_id] == 0)
			pr_info("Video  disable \n");
		break;
	default:
		no_msg = 1;
		break;
	}
	if(no_msg == 0)
		repeat_printf_info_count[msg_id]++;

	maybe_repeat_print_info_flag = REPEAT_PRINT_INFO_START;
}
void confirm_loop_print_msg(void)
{
	if(maybe_repeat_print_info_flag > REPEAT_PRINT_INFO_INIT
	   && maybe_repeat_print_info_flag < REPEAT_PRINT_INFO_NUM)
		maybe_repeat_print_info_flag++;
}
#endif

void SP_TX_Initialization(struct VideoFormat* pInputFormat)
{
	BYTE c;
	/*
	 //software reset
	 sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	 sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c | SP_TX_RST_SW_RST);
	 sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c & ~SP_TX_RST_SW_RST);
	*/
	sp_write_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG,0x14);//setting the bandwith to default value 5.4G
	sp_write_reg(SP_TX_PORT0_ADDR,SP_TX_TRAINING_LANE0_SET_REG,0x00);//setting the bandwith to default value 5.4G
	sp_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS4, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS4, c);
	
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_EXTRA_ADDR_REG, 0x50);//EDID address for AUX access
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL, 0x02);	//disable HDCP polling mode.
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_DEBUG_REG, 0x30);//enable M value read out

	
	//sp_read_reg(SP_TX_PORT0_ADDR, 0x85, &c);//,modify I2c glitch setting
	//sp_write_reg(SP_TX_PORT0_ADDR, 0x85, c | 0x80);//modify I2c glitch setting


	/*added for B0 to enable enable c-wire polling-ANX.Fei-20110831*/
	#ifdef Standard_DP
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1,0x00);
	#else
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c);

	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, (c|0x82));//disable polling HPD, force hotplug for HDCP, enable polling
	#endif
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, (c|0x80));//disable polling HPD, force hotplug for HDCP, disable polling, for HDCP blocked bug
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DP_POLLING_CTRL_REG, 0x01);

	/*added for B0 to change the c-wire termination from 100ohm to 50 ohm for polling error iisue-ANX.Fei-20110916*/
	sp_read_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, (c|0x30));//change the c-wire termination from 100ohm to 50 ohm

	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x37, 0x26);//set 400mv3.5db value according to mehran CTS report-ANX.Fei-20111009
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x47, 0x10);//set 400mv3.5db value according to mehran CTS report-ANX.Fei-20111009

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c | 0x03);//set KSV valid

	/*
	sp_write_reg(SP_TX_PORT2_ADDR, ANALOG_DEBUG_REG2, 0x06);

	//sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c& (~0x08));//set signle AUX output
	*/
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_GNS_CTRL_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_GNS_CTRL_REG, c | 0x40);//set interation counter to 5 times for link CTS
	
	sp_write_reg(SP_TX_PORT2_ADDR, ANALOG_DEBUG_REG2, 0x06);
	
	
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c); 
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c|0x08);//set double AUX output 

	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0x00);//mask all int
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK2, 0x00);//mask all int
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK3, 0x00);//mask all int
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK4, 0x00);//mask all int

	//PHY parameter for cts
	//change for 200--400mv, 300-600mv, 400-800nv
	//400mv (actual 200mv)
	//Swing
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x30, 0x16);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x36, 0x1b);//3.5db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x39, 0x22);//6db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x3b, 0x23);//9db

	//Pre-emphasis
	//sp_write_reg(MIPI_RX_PORT1_ADDR, 0x40, 0x00);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x46, 0x09);//3.5db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x49, 0x16);//6db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x4b, 0x1F);//9db


	//600mv (actual 300mv)
	//Swing
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x31, 0x26);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x37, 0x28);//3.5db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x3A, 0x2F);//6db

	//Pre-emphasis
	//sp_write_reg(MIPI_RX_PORT1_ADDR, 0x41, 0x00);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x47, 0x10);//3.5db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x4A, 0x1F);//6db


	//800mv (actual 400mv)
	//Swing
	//sp_write_reg(MIPI_RX_PORT1_ADDR, 0x34, 0x36);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x34, 0x32);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x38, 0x3c);//3.5db
	//emp
	//sp_write_reg(MIPI_RX_PORT1_ADDR, 0x44, 0x00);//0db
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x48, 0x10);//3.5db

	//1200mv (actual 600mv)
	//Swing
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x35, 0x3F);//0db
	//Pre-emphasis
	//sp_write_reg(MIPI_RX_PORT1_ADDR, 0x45, 0x00);//0db

	/*added for B0 version-ANX.Fei-20110831-Begin*/
	sp_write_reg(SP_TX_PORT2_ADDR, SP_INT_MASK, 0xb4);//0xb0 unmask IRQ request Int & c-wire polling error int
	/*added for B0 version-ANX.Fei-20110831-Begin*/

	//force termination open for clock lane
	sp_read_reg(MIPI_RX_PORT1_ADDR, 0x02, &c);
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x02, (c&0x3f));

	sp_read_reg(MIPI_RX_PORT1_ADDR, 0x2f, &c);
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x2f, (c|0xc0));

	//M value select, select clock with downspreading
	SP_TX_API_M_GEN_CLK_Select(1);

	if(pInputFormat->Interface == LVTTL_RGB) {
		//set clock edge
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xfc) | 0x03);

		//set Input BPC mode & color space
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c &= 0x8c;
		c = c |(pInputFormat->bColordepth << 4);
		c |= pInputFormat->bColorSpace;
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
	}


	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DP_POLLING_PERIOD, 0x01);

	sp_write_reg(SP_TX_PORT2_ADDR, 0x0F, 0x10);//0db
	pr_info("SP_TX_Initialization\n");

}
void SP_TX_Power_Enable(SP_TX_POWER_BLOCK sp_tx_pd_block, BYTE power)
{
	BYTE need_return = 0, c, power_type;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG , &c);
	switch(sp_tx_pd_block) {
	case SP_TX_PWR_REG://power down register
		power_type = SP_POWERD_REGISTER_REG;
		break;
	case SP_TX_PWR_HDCP://power down IO
		power_type = SP_POWERD_HDCP_REG;
		break;
	case SP_TX_PWR_AUDIO://power down audio
		power_type = SP_POWERD_AUDIO_REG;
		break;
	case SP_TX_PWR_VIDEO://power down video
		power_type = SP_POWERD_VIDEO_REG;
		break;
	case SP_TX_PWR_LINK://power down link
		power_type = SP_POWERD_LINK_REG;
		break;
	case SP_TX_PWR_TOTAL://power down total.
		power_type = SP_POWERD_TOTAL_REG;
		break;
	default:
		need_return = 1;
		break;;
	}
	if(need_return == 0) {
		if( (power == SP_TX_POWER_ON) && (c & power_type) == power_type)
			sp_write_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, (c & (~power_type)));
		else if((power == SP_TX_POWER_DOWN) && ((c & power_type) == 0))
			sp_write_reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, (c |power_type));
	}
	pr_info("SP_TX_Power_Enable\n");

}

void system_power_ctrl(BYTE ON)
{
	BYTE c1,c2,c3;

	if(ON == 0) {
		SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);
		//vbus_power_ctrl(0);
		SP_TX_Power_Enable(SP_TX_PWR_REG, SP_TX_POWER_DOWN);
		SP_TX_Power_Enable(SP_TX_PWR_TOTAL,SP_TX_POWER_DOWN);
		SP_TX_Hardware_PowerDown();
		sp_tx_pd_mode = 1;
		sp_tx_rx_type = RX_NULL;
		sp_tx_test_lt = 0;

		pr_err("Reset EDID Buffer\n");
		memset(bEDID_extblock, 0, 128);
		memset(bEDID_firstblock, 0, 128);
		memset(bEDID_fourblock, 0, 256);
	} else {
		sp_tx_pd_mode = 0;
		SP_TX_Hardware_PowerOn();
		SP_TX_Power_Enable(SP_TX_PWR_REG, SP_TX_POWER_ON);
		SP_TX_Power_Enable(SP_TX_PWR_TOTAL, SP_TX_POWER_ON);
		SP_TX_Initialization(&SP_TX_Video_Input);
		//vbus_power_ctrl(1);
#ifdef Standard_DP

			sp_tx_rx_type = RX_DP;
#endif
		c1 = 0;
		c2 = 0;
		c3 = 0;
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c1);
		pr_info("0x70  = %.2x\n",(unsigned int)c1);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c1| 0x01);
		pr_info("write 0x70 ok \n");

		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDL_REG , &c1);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDH_REG , &c2);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_REV_REG , &c3);
		if ((c1==0x05) && (c2==0x78)&&(c3==0xca)) {
			pr_info("ANX7805 Reversion CA");
		} else {
			pr_info("dev IDL = %.2x, deb IDH = %.2x, REV= %.2x\n",(unsigned int)c1,(unsigned int)c2,(unsigned int)c3);
		}
	}
}

void sp_tx_enable_video_input(BYTE enable)
{
	BYTE c;
	if (enable) {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c = (c & 0xf7) | 0x80;
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0xf5);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1, 0x0a);
#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x0A);
#else
		pr_info("Video Enabled!\n");
#endif

	} else {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c &= ~0x80;
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x0B);
#else
		pr_info("Video disable! \n");
#endif
	}
}
/*#ifdef __KEIL51_ENV__*/

void SP_TX_BIST_Format_Config(unsigned int sp_tx_bist_select_number)
{
	unsigned int sp_tx_bist_data;
	BYTE c,c1;
	unsigned int wTemp,wTemp1,wTemp2;
	BYTE bInterlace;


	pr_info("config bist vid timing");
	if(!Force_Video_Resolution) {
		//use prefered timing if EDID read success, otherwise output failsafe mode.
		if((sp_tx_edid_err_code==0) && (!edid_pclk_out_of_range)) {
			//Interlace or Progressive mode
			//temp = (SP_TX_EDID_PREFERRED[17]&0x80)>>7;
			c = SP_TX_EDID_PREFERRED[17] & 0x80;
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c1);
			if(c == 0) { //progress
				sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c1 &(~ SP_TX_VID_CTRL10_I_SCAN)));
				bInterlace = 0;
			} else { //interlace
				sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c1 | SP_TX_VID_CTRL10_I_SCAN));
				bInterlace = 1;
			}

			//Vsync Polarity set
			//temp = (SP_TX_EDID_PREFERRED[17]&0x04)>>2;
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
			if(SP_TX_EDID_PREFERRED[17]&0x04) {
				sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_VSYNC_POL));
			} else {
				sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_VSYNC_POL)));
			}

			//Hsync Polarity set
			//temp = (SP_TX_EDID_PREFERRED[17]&0x20)>>1;
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
			if(SP_TX_EDID_PREFERRED[17]&0x20) { //h sync polarity should be bit 1 2010/07/06
				sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_HSYNC_POL));
			} else {
				sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_HSYNC_POL)));
			}

			//H active length set
			wTemp = SP_TX_EDID_PREFERRED[4];
			wTemp = (wTemp << 4) & 0x0f00;
			sp_tx_bist_data = wTemp + SP_TX_EDID_PREFERRED[2];
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, (sp_tx_bist_data&0x00FF));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, (sp_tx_bist_data>>8));

			//H total length = hactive+hblank
			wTemp = SP_TX_EDID_PREFERRED[4];
			wTemp = (wTemp<< 8) & 0x0f00;
			wTemp= wTemp + SP_TX_EDID_PREFERRED[3];
			sp_tx_bist_data = sp_tx_bist_data + wTemp;
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, (sp_tx_bist_data&0x00FF));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, (sp_tx_bist_data >> 8));


			//H front porch width set
			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 2) & 0x0300;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[8];
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, (wTemp&0xF00FF));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, (wTemp>>8));

			//H sync width set
			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 4) & 0x0300;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[9];
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, (wTemp&0xF00FF));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, (wTemp>>8));

			//H back porch = H blank - H Front porch - H sync width
			//Hblank
			wTemp = SP_TX_EDID_PREFERRED[4];
			wTemp = (wTemp<< 8) & 0x0f00;
			wTemp= wTemp + SP_TX_EDID_PREFERRED[3];

			//H Front porch
			wTemp1 = SP_TX_EDID_PREFERRED[11];
			wTemp1 = (wTemp1 << 2) & 0x0300;
			wTemp1 = wTemp1 + SP_TX_EDID_PREFERRED[8];

			//Hsync width
			sp_tx_bist_data = SP_TX_EDID_PREFERRED[11];
			sp_tx_bist_data = (sp_tx_bist_data << 4) & 0x0300;
			sp_tx_bist_data = sp_tx_bist_data + SP_TX_EDID_PREFERRED[9];

			//H Back porch
			wTemp2 = wTemp - wTemp1 - sp_tx_bist_data;
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, (wTemp2&0x00ff));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, (wTemp2 >> 8));

			//V active length set
			wTemp = SP_TX_EDID_PREFERRED[7];
			wTemp = (wTemp << 4) & 0x0f00;
			sp_tx_bist_data = wTemp + SP_TX_EDID_PREFERRED[5];
			//for interlaced signal
			if(bInterlace ==1)
				sp_tx_bist_data = sp_tx_bist_data*2;
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, (sp_tx_bist_data&0x00ff));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, (sp_tx_bist_data >> 8));

			//V total length set
			wTemp = SP_TX_EDID_PREFERRED[7];
			wTemp = (wTemp << 8) & 0x0f00;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[6];
			//vactive+vblank
			sp_tx_bist_data = sp_tx_bist_data + wTemp;
			//for interlaced signal
			if(bInterlace ==1)
				sp_tx_bist_data = sp_tx_bist_data*2+1;
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEL_REG, (sp_tx_bist_data&0x00ff));
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEH_REG, (sp_tx_bist_data >> 8));

			//V front porch width set
			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 2) & 0x0030;
			wTemp = wTemp + (SP_TX_EDID_PREFERRED[10] >> 4);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, wTemp);

			//V sync width set

			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 4) & 0x0030;
			wTemp = wTemp + (SP_TX_EDID_PREFERRED[10] & 0x0f);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, wTemp);


			//V back porch = V blank - V Front porch - V sync width
			//V blank
			wTemp = SP_TX_EDID_PREFERRED[7];
			wTemp = (wTemp << 8) & 0x0f00;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[6];

			//V front porch
			wTemp1 = SP_TX_EDID_PREFERRED[11];
			wTemp1 = (wTemp1 << 2) & 0x0030;
			wTemp1 = wTemp1 + (SP_TX_EDID_PREFERRED[10] >> 4);

			//V sync width
			wTemp2 = SP_TX_EDID_PREFERRED[11];
			wTemp2 = (wTemp2 << 4) & 0x0030;
			wTemp2 = wTemp2 + (SP_TX_EDID_PREFERRED[10] & 0x0f);
			sp_tx_bist_data = wTemp - wTemp1 - wTemp2;
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, sp_tx_bist_data);

		} else {

			SP_TX_BIST_Format_Resolution(5);

			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, 0x00);//18bpp for fail safe mode
			pr_info("safe mode  = 640*480p@60hz_18bpp");
		}
	} else
		SP_TX_BIST_Format_Resolution(sp_tx_bist_select_number);


	//BIST color bar width set--set to each bar is 32 pixel width
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, (c &(~SP_TX_VID_CTRL4_BIST_WIDTH)));

	if(sp_tx_lane_count == 1) {
		//set to gray bar
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
		c&= 0xfc;
		c|= 0x01;
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, c);
	}

	//Enable video BIST
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, (c | SP_TX_VID_CTRL4_BIST));

}
void SP_TX_BIST_Format_Resolution(unsigned int video_id)
{
	unsigned int sp_tx_bist_data;
	BYTE c;

	sp_tx_bist_data = video_timing_table[video_id].is_interlaced;
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	if(sp_tx_bist_data == 0) {
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_I_SCAN)));
		pr_info("Bist video is progressive.");
	} else {
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_I_SCAN));
		pr_info("Bist video is interlace.");
	}

	//Vsync Polarity set
	sp_tx_bist_data = video_timing_table[video_id].v_sync_polarity;
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	if(sp_tx_bist_data == 1) {
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_VSYNC_POL));
		pr_info("Bist video VSYNC polarity: low is active.");
	} else {
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_VSYNC_POL)));
		pr_info("Bist video VSYNC polarity: high is active.");
	}

	//Hsync Polarity set
	sp_tx_bist_data = video_timing_table[video_id].h_sync_polarity;
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	if(sp_tx_bist_data == 1) {
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_HSYNC_POL));
		pr_info("Bist video HSYNC polarity: low is active.");
	} else {
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_HSYNC_POL)));
		pr_info("Bist video HSYNC polarity: high is active.");
	}

	//H total length set
	sp_tx_bist_data = video_timing_table[video_id].h_total_length;

	//sp_tx_bist_data = video_timing_table[video_id].h_total_length;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, (sp_tx_bist_data >> 8));

	//H active length set
	sp_tx_bist_data = video_timing_table[video_id].h_active_length;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, (sp_tx_bist_data >> 8));

	//H front porth width set
	sp_tx_bist_data = video_timing_table[video_id].h_front_porch;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, (sp_tx_bist_data >> 8));

	//H sync width set
	sp_tx_bist_data = video_timing_table[video_id].h_sync_width;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, (sp_tx_bist_data >> 8));

	//H back porth width set
	sp_tx_bist_data = (video_timing_table[video_id].h_back_porch);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, (sp_tx_bist_data >> 8));

	//V total length set
	sp_tx_bist_data = video_timing_table[video_id].v_total_length;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEH_REG, (sp_tx_bist_data >> 8));

	//V active length set
	sp_tx_bist_data = video_timing_table[video_id].v_active_length;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, sp_tx_bist_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, (sp_tx_bist_data >> 8));


	//V front porth width set
	sp_tx_bist_data = video_timing_table[video_id].v_front_porch;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, sp_tx_bist_data);

	//V sync width set
	sp_tx_bist_data = video_timing_table[video_id].v_sync_width;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, sp_tx_bist_data);

	//V back porth width set
	sp_tx_bist_data = video_timing_table[video_id].v_back_porch;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, sp_tx_bist_data);
}

void SP_TX_Config_BIST_Video (BYTE cBistIndex,struct VideoFormat* pInputFormat)
{

	BYTE c;
	//power down MIPI,enable lvttl input
	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
	c |= 0x10;
	sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);

	//SP_TX_Clean_HDCP();
	SP_CTRL_Clean_HDCP();
	SP_TX_Power_Enable(SP_TX_PWR_VIDEO, SP_TX_POWER_ON);

	pr_info("Configure video format in BIST mode");

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	if(!(c & SP_TX_SYS_CTRL1_DET_STA)) {
		pr_info("Stream clock not found!");
		return;
	}

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	if(c & SP_TX_SYS_CTRL2_CHA_STA) {
		pr_info("Stream clock not stable!");
		return;
	}

	//set Input BPC mode & color space
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
	c &= 0x8c;
	if(cBistIndex == 4)//bist fail safe mode, set to 18 bpp for CTS test
		c&=0x8f;
	else
		c = c |(pInputFormat->bColordepth << 4);

	c |= pInputFormat->bColorSpace;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);


	SP_TX_BIST_Format_Config(cBistIndex);

	//enable video input
	sp_tx_enable_video_input(1);
	msleep(50);

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	if(!(c & SP_TX_SYS_CTRL3_STRM_VALID)) {
		pr_info("video stream not valid!");
		return;
	}
	SP_TX_Config_Packets(AVI_PACKETS);

	sp_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, c|0x0e);//Unmask video clock&format change &PLL int

	//if(Force_AUD)//force to configure audio regadless of EDID info.
	//sp_tx_ds_edid_hdmi = 1;

	SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
	/*
	if(sp_tx_ds_edid_hdmi) {
		SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
	} else {
		SP_TX_Power_Enable(SP_TX_PWR_AUDIO, SP_TX_POWER_DOWN);//power down audio when DVI
		SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
	}
	*/

}
void SP_CTRL_BIST_Clk_MN_Gen(unsigned int sp_tx_bist_select_number)
{

	switch (sp_tx_bist_select_number) {
	case 0:    //Freq = 108MHz, M=216, N=2
		SP_CTRL_nbc12429_setting(108);
		pclk = 54;
		break;

	case 1: //Freq = 75, M=146, N=1
		SP_CTRL_nbc12429_setting(75);
		pclk = 37;
		break;
	case 2:    //Freq = 146MHz, M=308, N=2
		SP_CTRL_nbc12429_setting(146);
		pclk = 73;
		break;

	case 3:    //Freq = 154.128MHz, M=308, N=2
		SP_CTRL_nbc12429_setting(148);
		pclk = 74;
		break;

	case 4: //Freq = 268, M=268, N=0
		SP_CTRL_nbc12429_setting(268);
		pclk = 134;
		break;

	case 5: //Freq = 25, M=100, N=2
		SP_CTRL_nbc12429_setting(25);
		pclk = 12;
		break;

	case 6: //Freq = 31.5, M=268, N=0
		SP_CTRL_nbc12429_setting(31);
		pclk = 16;
		break;

	case 7: //Freq = 154, M=268, N=0
		SP_CTRL_nbc12429_setting(154);
		pclk = 77;
		break;

	case 8: //Freq = 49.5, M=268, N=0
		SP_CTRL_nbc12429_setting(49);
		pclk = 25;
		break;
	case 9: //Freq = 25, M=268, N=0
		SP_CTRL_nbc12429_setting(25);
		pclk = 12;
		break;

	default:
		break;

	}

}

void SP_CTRL_BIST_CLK_Genarator(unsigned int sp_tx_bist_select_number)
{

	unsigned int wTemp=0;
	int M=0,N=0;
	int wPixelClk=0;//,wPixelClk1;

	if(!Force_Video_Resolution) {
		if(sp_tx_edid_err_code == 0) { //commented for QDI test
			//select the correct clock according to EDID
			//Get pixel clock
			wTemp = SP_TX_EDID_PREFERRED[1];
			wTemp = wTemp << 8;
			wPixelClk = wTemp + SP_TX_EDID_PREFERRED[0];
			pr_info("Pixel clock is 10000 * %u\n",	wPixelClk);
			//pr_info("config clk\n");

			if((wPixelClk > 27000)||(wPixelClk < 2500)) { //25M-256M clk

				edid_pclk_out_of_range = 1;
				SP_CTRL_nbc12429_setting(25);
				pclk = 12;
				pr_info("clk out of range, set to safe clock 25MHz SDR\n");

			} else {
				M = wPixelClk/100;
				SP_CTRL_nbc12429_setting(M);
				pclk = M/2;
				pr_info("clock M =0x%.2x, N = 0x%.2x\n",(unsigned int)M,(unsigned int)N);

			}
		} else {
			SP_CTRL_nbc12429_setting(25);
			pclk = 12;
			pr_info("EDID read error, set to safe clk 25Mhz SDR\n");
		}
	} else
		SP_CTRL_BIST_Clk_MN_Gen(sp_tx_bist_select_number);


}


void SP_CTRL_nbc12429_setting(int frequency)
{
#if 0
	int m_setting;
	//BYTE x,y;
	pr_info("set pclk: %d\n",frequency);

	if(/* frequency>=25 &&*/ frequency<=50) {
		// N = 8
		MC12429_N0 = 1;
		MC12429_N1 = 1;
		m_setting = frequency << 3;
		/*
		       if((sp_tx_lane_count!=0x01)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
		       {
		           m_setting = frequency << 2;
		       }*/

		MC12429_M0 = (m_setting & 0x001);
		MC12429_M1 = (m_setting & 0x002);
		MC12429_M2 = (m_setting & 0x004);
		MC12429_M3 = (m_setting & 0x008);
		MC12429_M4 = (m_setting & 0x010);
		MC12429_M5 = (m_setting & 0x020);
		MC12429_M6 = (m_setting & 0x040);
		MC12429_M7 = (m_setting & 0x080);
		MC12429_M8 = (m_setting & 0x100);

	} else if(frequency>50 && frequency<=110) {
		// N = 4
		MC12429_N0 = 0;
		MC12429_N1 = 1;
		/*
		if((sp_tx_lane_count!=1)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
			{
			   MC12429_N0 = 1;
			   MC12429_N1 = 1;
			}*/

		m_setting = frequency << 2;
		MC12429_M0 = (m_setting & 0x001);
		MC12429_M1 = (m_setting & 0x002);
		MC12429_M2 = (m_setting & 0x004);
		MC12429_M3 = (m_setting & 0x008);
		MC12429_M4 = (m_setting & 0x010);
		MC12429_M5 = (m_setting & 0x020);
		MC12429_M6 = (m_setting & 0x040);
		MC12429_M7 = (m_setting & 0x080);
		MC12429_M8 = (m_setting & 0x100);
	} else if(frequency>110 && frequency<=200) {
		// N = 2
		MC12429_N0 = 1;
		MC12429_N1 = 0;
		/*
		if((sp_tx_lane_count!=1)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
		{
		   MC12429_N0 = 0;
		   MC12429_N1 = 1;
		}*/

		m_setting = frequency << 1;

		MC12429_M0 = (m_setting & 0x001);
		MC12429_M1 = (m_setting & 0x002);
		MC12429_M2 = (m_setting & 0x004);
		MC12429_M3 = (m_setting & 0x008);
		MC12429_M4 = (m_setting & 0x010);
		MC12429_M5 = (m_setting & 0x020);
		MC12429_M6 = (m_setting & 0x040);
		MC12429_M7 = (m_setting & 0x080);
		MC12429_M8 = (m_setting & 0x100);
	} else if(frequency>200 && frequency<=400) {
		// N = 1
		MC12429_N0 = 0;
		MC12429_N1 = 0;
		/*
		if((sp_tx_lane_count!=1)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
		{
		   MC12429_N0 = 1;
		   MC12429_N1 = 0;
		}  */
		m_setting = frequency ;

		MC12429_M0 = (m_setting & 0x001);
		MC12429_M1 = (m_setting & 0x002);
		MC12429_M2 = (m_setting & 0x004);
		MC12429_M3 = (m_setting & 0x008);
		MC12429_M4 = (m_setting & 0x010);
		MC12429_M5 = (m_setting & 0x020);
		MC12429_M6 = (m_setting & 0x040);
		MC12429_M7 = (m_setting & 0x080);
		MC12429_M8 = (m_setting & 0x100);
	} else
		pr_info("Wrong value given!");
#endif
}
/*#endif*/
BYTE get_bandwidth_and_pclk(void)
{
	BYTE c,c1;
	unsigned int wPacketLenth;

	SP_TX_Power_Enable(SP_TX_PWR_VIDEO, SP_TX_POWER_ON);
	if(SP_TX_Video_Input.Interface == LVTTL_RGB) {
		if(SP_TX_Config_Video_LVTTL(&SP_TX_Video_Input))
			return 1;
#if(REDUCE_REPEAT_PRINT_INFO)
		else
			loop_print_msg(0x04);
#endif
	} else if(SP_TX_Video_Input.Interface == MIPI_DSI) {

		pclk = mipi_video_timing_table[bMIPIFormatIndex].MIPI_pixel_frequency;
		if(SP_TX_BW_LC_Sel(&SP_TX_Video_Input)) {
			pr_info("****Over bandwidth****");
			return 1;
		}
		//set bandwidth
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, sp_tx_bw);
		if(SP_TX_Config_Video_MIPI())
			return 1;
	}

	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0xf5);//unmask video clock change&format change int
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1, 0x0a);//Clear video format and clock change-20111206-ANX.Fei

	SP_TX_Video_Mute(1);

	//enable video input
	sp_tx_enable_video_input(1);

	msleep(50);
	if(SP_TX_Video_Input.Interface == LVTTL_RGB) {
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c);
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
		if(!(c & SP_TX_SYS_CTRL3_STRM_VALID)) {
#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x02);
#else
			pr_info("video stream not valid!");
#endif
			return 1;
		}
		//pr_info("video stream valid!");
		//Get  transmit lane count&link bw
		SP_TX_Get_Link_BW(&c);
		sp_tx_bw = (SP_LINK_BW)c;
		//Calculate the pixel clock
		SP_TX_PCLK_Calc(sp_tx_bw);
	} else { //MIPI DSI
		//check video packet to determin if video stream valid
		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_LONG_PACKET_LENTH_LOW, &c);
		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_LONG_PACKET_LENTH_HIGH, &c1);

		wPacketLenth = (mipi_video_timing_table[bMIPIFormatIndex].MIPI_HActive)*3;

		if(((wPacketLenth&0x00ff)!=c)||(((wPacketLenth&0xff00)>>8)!=c1)) {
#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x02);
#else
			pr_info("video stream not valid!");
#endif

			return 1;
		} else
			//pr_info("mipi video stream valid!");
			pclk = mipi_video_timing_table[bMIPIFormatIndex].MIPI_pixel_frequency;
	}
	//Optimize the LT to get minimum power consumption
	if(SP_TX_BW_LC_Sel(&SP_TX_Video_Input)) {
#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x05);
#else
		pr_info("****Over bandwidth****");
#endif
		return 1;
	}
	pr_info("get->pclk: %.2x, The optimized BW =%.2x, Lane cnt=%.2x \n",
	        (unsigned int)pclk, (unsigned int)sp_tx_bw, (unsigned int)sp_tx_lane_count);
	return 0;

}

void SP_TX_DE_reGenerate (unsigned int video_id)
{
	BYTE c;

	//interlace scan mode configuration
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG,
	             (c & 0xfb) | (video_timing_table[video_id].is_interlaced<< 2));

	//V sync polarity
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG,
	             (c & 0xfd) | (video_timing_table[video_id].v_sync_polarity<< 1) );

	//H sync polarity
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG,
	             (c & 0xfe) | (video_timing_table[video_id].h_sync_polarity) );

	//active line
	c = video_timing_table[video_id].v_active_length& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, c);
	c = video_timing_table[video_id].v_active_length >> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, c);

	//V sync width
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG,
	             video_timing_table[video_id].v_sync_width);

	//V sync back porch.
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG,
	             video_timing_table[video_id].v_back_porch);

	//total pixel in each frame
	c = video_timing_table[video_id].h_total_length& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, c);
	c = video_timing_table[video_id].h_total_length>> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, c);

	//active pixel in each frame.
	c = video_timing_table[video_id].h_active_length& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, c);
	c = video_timing_table[video_id].h_active_length >> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, c);

	//pixel number in H period
	c = video_timing_table[video_id].h_sync_width& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, c);
	c = video_timing_table[video_id].h_sync_width >> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, c);

	//pixel number in frame horizontal back porch
	c = video_timing_table[video_id].h_back_porch& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, c);
	c = video_timing_table[video_id].h_back_porch >> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, c);

	//enable DE mode.
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c | SP_TX_VID_CTRL1_DE_GEN);
}

void SP_TX_Embedded_Sync(struct VideoFormat* pInputFormat, unsigned int video_id)
{
	BYTE c;

	//set embeded sync flag check
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG,
	             (c & ~SP_TX_VID_CTRL4_EX_E_SYNC) |
	             pInputFormat->bLVTTL_HW_Interface.sEmbedded_Sync.Extend_Embedded_Sync_flag << 6);

	// sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL3_REG, &c);
	// sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL3_REG,(c & 0xfb | 0x02));

	//set Embedded sync repeat mode.
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG,
	             (c & 0xcf) | (video_timing_table[video_id].pix_repeat_times<< 4) );

	//V sync polarity
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG,
	             (c & 0xfd) | (video_timing_table[video_id].v_sync_polarity<< 1) );

	//H sync polarity
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG,
	             (c & 0xfe) | (video_timing_table[video_id].h_sync_polarity) );

	//V  front porch.
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG,
	             video_timing_table[video_id].v_front_porch);

	//V sync width
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG,
	             video_timing_table[video_id].v_sync_width);

	//H front porch
	c = video_timing_table[video_id].h_front_porch& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, c);
	c = video_timing_table[video_id].h_front_porch >> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, c);

	//H sync width
	c = video_timing_table[video_id].h_sync_width& 0xff;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, c);
	c = video_timing_table[video_id].h_sync_width >> 8;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, c);

	//Enable Embedded sync
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, c | SP_TX_VID_CTRL4_E_SYNC_EN);

}

BYTE SP_TX_Config_Video_LVTTL (struct VideoFormat* pInputFormat)
{
	BYTE c;//,i;
	//power down MIPI,enable lvttl input
	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
	c |= 0x10;
	sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	if(!(c & SP_TX_SYS_CTRL1_DET_STA)) {
/*
#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x00);
#else
		pr_err("Stream clock not found!");
#endif
*/
		return 1;
	}

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	if(c & SP_TX_SYS_CTRL2_CHA_STA) {
#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x03);
#else
		pr_info("Stream clock not stable!");
#endif
		return 1;
	}

	if(pInputFormat->bColorSpace==COLOR_YCBCR_444) {
		//pr_info("ColorSpace YCbCr444");
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0xfc)|0x02));
		//sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL5_REG, 0x90);//enable Y2R conversion based on BT709
		//sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL6_REG, 0x40);//enable video porcess
	} else if(pInputFormat->bColorSpace==COLOR_YCBCR_422) {
		//pr_info("ColorSpace YCbCr422");
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0xfc)|0x01));


		// sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL5_REG, 0x90);//enable Y2R conversion based on BT709
		//sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL6_REG, 0x42);//enable video porcess and upsample
	} else {
		// pr_info("ColorSpace RGB");
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, (c&0xfc));
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL5_REG, 0x00);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL6_REG, 0x00);
	}


	if(pInputFormat->bLVTTL_HW_Interface.sEmbedded_Sync.Embedded_Sync) {
		//pr_info("Embedded_Sync");
		SP_TX_Embedded_Sync(pInputFormat,1);//set 720p as the default
	} else {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, c & ~SP_TX_VID_CTRL4_E_SYNC_EN);
	}

	if(pInputFormat->bLVTTL_HW_Interface.DE_reDenerate) {
		//pr_info("DE_reDenerate\n");
		SP_TX_DE_reGenerate(1);//set 720p as the default
	} else {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c & ~SP_TX_VID_CTRL1_DE_GEN);
	}

	if(pInputFormat->bLVTTL_HW_Interface.sYC_MUX.YC_MUX) {
		//pr_info("YC_MUX\n");
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xef) | SP_TX_VID_CTRL1_DEMUX);

		if(pInputFormat->bLVTTL_HW_Interface.sYC_MUX.YC_BIT_SEL) {
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, (c & 0xfb) |SP_TX_VID_CTRL1_YCBIT_SEL );
		}

	} else {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c & ~SP_TX_VID_CTRL1_DEMUX);
	}

	if(pInputFormat->bLVTTL_HW_Interface.DDR_Mode) {
		//pr_info("DDR_mode\n");
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xfb) | SP_TX_VID_CTRL1_IN_BIT);
	} else {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c & ~SP_TX_VID_CTRL1_IN_BIT);
	}

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xfc)|SP_TX_VID_CTRL1_DDRCTRL);

	//Force output to CEA range(16-235)
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL9_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL9_REG, c|0x80);

	SP_TX_LVTTL_Bit_Mapping(pInputFormat);
	return 0;

}
void SP_TX_LVTTL_Bit_Mapping(struct VideoFormat* pInputFormat)//the default mode is 12bit ddr
{

	BYTE c;
	/*if(bEDIDBreak)
	 {
	 	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c & 0x8f);
            	debug_puts("6bits!\n");
              for(c=0; c<18; c++)
              {	 
                  sp_write_reg(SP_TX_PORT2_ADDR, 0x40+c, 0x00+c);
              }
				
	 }//jgliu add 1026
	else */if(pInputFormat->bLVTTL_HW_Interface.DDR_Mode) {
		switch(pInputFormat->bColordepth) {
		case COLOR_8_BIT://correspond with ANX8770 24bit DDR mode
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x10));//set input video 8-bit
			for(c=0; c<12; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x00 + c);
			}
			for(c=0; c<12; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x4c + c, 0x18 + c);
			}
			break;
		case COLOR_10_BIT://correspond with ANX8770 30bit DDR mode
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x20));//set input video 10-bit
			for(c=0; c<15; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x03 + c);
			}
			for(c=0; c<15; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x4f + c, 0x1b + c);
			}
			break;
		case COLOR_12_BIT://correspond with ANX8770 36bit DDR mode
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x30));//set input video 12-bit
			for(c=0; c<18; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x00 + c);
			}
			for(c=0; c<18; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x52 + c, 0x18 + c);
			}
			break;
		default:
			break;
		}
	} else {
		switch(pInputFormat->bColordepth) {
		case COLOR_8_BIT://8bit SDR mode
			sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x10));//set input video 8-bit
			for(c=0; c<24; c++) {
				sp_write_reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x00 + c);
			}
			break;
		default:
			break;
		}
	}

#if(AUTO_TEST_CTS)
	if (sp_tx_test_edid ) {
		/*set color depth to 18-bit for link cts*/
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
		sp_tx_test_edid = 0;
		pr_info("***color space is set to 18bit***");
	}
#endif

	msleep(10);
}

BYTE  SP_TX_Config_Video_MIPI (void)
{
	BYTE c;//,i;

	if(!bMIPI_Configured) {
		//config video format
		SP_TX_Config_MIPI_Video_Format();

		//force blanking vsync
		sp_read_reg(SP_TX_PORT0_ADDR, 0xB4, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, 0xb4, (c|0x02));

		//power down MIPI,
		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
		c |= 0x10;
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);

		//set power up counter, never power down high speed data path during blanking
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, 0x40);
		//sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG3, 0xC4);

		//set LP high reference voltage to 800mv
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x2A, 0x0b);

		//sp_write_reg(MIPI_RX_PORT1_ADDR, 0x1c, 0x10);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x1c, 0x31);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x1b, 0xbb);

		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x19, 0x3e);

		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x08, 0x08);



		//set mipi data lane count

		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, &c);
		if((MIPI_LANE_SEL_0)&&(MIPI_LANE_SEL_1)) {
			pr_info("####1 lane");
			//set lane count
			c&= 0xF9;
		} else if((MIPI_LANE_SEL_0)&&(!MIPI_LANE_SEL_1)) {
			pr_info("####2 lanes");
			//set lane count
			c&= 0xF9;
			c|= 0x02;// two lanes
		} else if((!MIPI_LANE_SEL_0)&&(MIPI_LANE_SEL_1)) {
			pr_info("####3 lanes");
			//set lane count
			c&= 0xF9;
			c|= 0x04;// three lanes
		} else {
			pr_info("####4 lanes");
			//set lane count
			c&= 0xF9;
			c|= 0x06;// four lanes
		}
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, c);//Set 4 lanes, link clock 270M


		//power on MIPI, //enable MIPI input
		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
		c &= 0xEF;
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);

		//control reset_n_ls_clk_comb to reset mipi
		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, &c);
		c&=0xF7;
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, c);
		msleep(1);
		c|=0x08;
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, c);

		//reset low power mode
		sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, &c);
		c|=0x01;
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, c);
		msleep(1);
		c&=0xfe;
		sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, c);

		pr_info("MIPI configured!");

		SP_TX_MIPI_CONFIG_Flag_Set(1);


	} else
		pr_info("MIPI interface enabled");


	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, &c);
	sp_write_reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, c);
	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, &c);
	if(!(c & 0X40)) {
		pr_info("Stream clock not found!");
		pr_info("0x70:0x80=%.2x\n",(unsigned int)c);
		msleep(100);
		return 1;
	}
#ifdef MIPI_DEBUG
	else {
		pr_info("#######Stream clock found!");
		pr_info("0x70:0x80=%.2x\n",(unsigned int)c);
	}
#endif


	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	if(c & SP_TX_SYS_CTRL2_CHA_STA) {
		pr_info("Stream clock not stable!");
		pr_info("0x70:0x04=%.2x\n",(unsigned int)c);
		msleep(100);

		return 1;
	}
#ifdef MIPI_DEBUG
	else {
		pr_info("#######Stream clock stable!");
		pr_info("0x70:0x04=%.2x\n",(unsigned int)c);
	}
#endif




	return 0;

}

void SP_TX_EnhaceMode_Set(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,DPCD_MAX_LANE_COUNT,1,&c);
	if(c & 0x80) {

		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, c | SP_TX_SYS_CTRL4_ENHANCED);

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x01,DPCD_LANE_COUNT_SET,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x01,DPCD_LANE_COUNT_SET, c | 0x80);

		pr_info("Enhance mode enabled");
	} else {

		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, c & (~SP_TX_SYS_CTRL4_ENHANCED));

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x01,DPCD_LANE_COUNT_SET,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x01,DPCD_LANE_COUNT_SET, c & (~0x80));

		pr_info("Enhance mode disabled");
	}
}


void SP_TX_Clean_HDCP(void)
{
	BYTE c;
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, 0x00);//disable HW HDCP

	//reset HDCP logic
   	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
    sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c | SP_TX_RST_HDCP_REG);
    sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c& ~SP_TX_RST_HDCP_REG);
		
	//set re-auth
	SP_TX_HDCP_ReAuth();
}

void hdcp_encryption_enable(BYTE enable)
{
	BYTE c;
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	if(enable == 0)
		c &= ~SP_TX_HDCP_CONTROL_0_HDCP_ENC_EN;
	else
		c |= SP_TX_HDCP_CONTROL_0_HDCP_ENC_EN;
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
}

void SP_TX_HW_HDCP_Enable(void)
{
	BYTE c;
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	c&=0xf3;
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	c|=0x0f;//enable HDCP and encryption
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	pr_info("SP_TX_HDCP_CTRL0_REG = %.2x\n", (unsigned int)c);

	/*SP_TX_WAIT_R0_TIME*/
	sp_write_reg(SP_TX_PORT0_ADDR, 0x40, 0xba);//HDCP CTS 20150925 jgliu
	/*SP_TX_WAIT_KSVR_TIME*/
	sp_write_reg(SP_TX_PORT0_ADDR, 0x42, 0xc8);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK2, 0xfc);//unmask auth change&done int

	pr_info("Hardware HDCP is enabled.\n");
}
/*
void SP_TX_HW_HDCP_Disable(void)
{
    BYTE c;
    sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
    sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c & ~SP_TX_HDCP_CONTROL_0_HARD_AUTH_EN);
}
*/
void SP_TX_PCLK_Calc(SP_LINK_BW hbr_rbr)
{
	long int str_clk;
	BYTE c;
	switch(hbr_rbr) {
	case BW_54G:
		str_clk = 540;
		break;
	/*case BW_45G:
		str_clk = 450;
	break;*/
	case BW_27G:
		str_clk = 270;
		break;
	case BW_162G:
		str_clk = 162;
		break;
	default:
		str_clk = 540;
		break;

	}


	sp_read_reg(SP_TX_PORT0_ADDR,M_VID_2, &c);
	M_val = c * 0x10000;
	sp_read_reg(SP_TX_PORT0_ADDR,M_VID_1, &c);
	M_val = M_val + c * 0x100;
	sp_read_reg(SP_TX_PORT0_ADDR,M_VID_0, &c);
	M_val = M_val + c;

	sp_read_reg(SP_TX_PORT0_ADDR,N_VID_2, &c);
	N_val = c * 0x10000;
	sp_read_reg(SP_TX_PORT0_ADDR,N_VID_1, &c);
	N_val = N_val + c * 0x100;
	sp_read_reg(SP_TX_PORT0_ADDR,N_VID_0, &c);
	N_val = N_val + c;

	str_clk = str_clk * M_val;
	pclk = str_clk ;
	pclk = pclk / N_val;
}


void SP_TX_Show_Infomation(void)
{
	BYTE c,c1;
	unsigned int h_res,h_act,v_res,v_act;
	unsigned int h_fp,h_sw,h_bp,v_fp,v_sw,v_bp;
	unsigned long fresh_rate;

	pr_info("\n*************************SP Video Information*************************\n");
	if(BIST_EN)
		pr_info("   SP TX mode = BIST mode\n");
	else
		pr_info("   SP TX mode = Normal mode");



	sp_read_reg(SP_TX_PORT0_ADDR,SP_TX_LANE_COUNT_SET_REG, &c);
	if(c==0x00)
		pr_info("   LC = 1");


	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
	if(c&0x10) {
		sp_read_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG, &c);
		if(c==0x06) {
			pr_info("   BW = 1.62G");
			SP_TX_PCLK_Calc(BW_162G);//str_clk = 162;
		} else if(c==0x0a) {
			pr_info("   BW = 2.7G");
			SP_TX_PCLK_Calc(BW_27G);//str_clk = 270;
		} else if(c==0x14) {
			pr_info("   BW = 5.4G");
			SP_TX_PCLK_Calc(BW_54G);//str_clk = 540;
		}

	} else {
		sp_read_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG, &c);
		if(c==0x06) {
			pr_info("   BW = 1.62G");
		} else if(c==0x0a) {
			pr_info("   BW = 2.7G");
		} else if(c==0x14) {
			pr_info("   BW = 5.4G");
		}

	}



	if(SSC_EN)
		pr_info("   SSC On");
	else
		pr_info("   SSC Off");

	pr_info("   M = %lu, N = %lu, PCLK = %.2x MHz\n",M_val,N_val,(unsigned int)pclk);

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_H,&c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;


	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H,&c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;


	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_H,&c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;


	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H,&c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_F_PORCH_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_F_PORCH_STA_H,&c1);

	h_fp = c1;
	h_fp = h_fp << 8;
	h_fp = h_fp + c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_SYNC_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_SYNC_STA_H,&c1);

	h_sw = c1;
	h_sw = h_sw << 8;
	h_sw = h_sw + c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_B_PORCH_STA_L,&c);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_H_B_PORCH_STA_H,&c1);

	h_bp = c1;
	h_bp = h_bp << 8;
	h_bp = h_bp + c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_V_F_PORCH_STA,&c);
	v_fp = c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_V_SYNC_STA,&c);
	v_sw = c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_V_B_PORCH_STA,&c);
	v_bp = c;

	pr_info("   Total resolution is %d * %d \n", h_res, v_res);

	pr_info("   HF=%d, HSW=%d, HBP=%d\n", h_fp, h_sw, h_bp);
	pr_info("   VF=%d, VSW=%d, VBP=%d\n", v_fp, v_sw, v_bp);
	pr_info("   Active resolution is %d * %d ", h_act, v_act);


	/*
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_L,&c);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_H,&c1);

		v_res = c1;
		v_res = v_res << 8;
		v_res = v_res + c;


		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L,&c);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H,&c1);

		v_act = c1;
		v_act = v_act << 8;
		v_act = v_act + c;


		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_L,&c);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_H,&c1);

		h_res = c1;
		h_res = h_res << 8;
		h_res = h_res + c;


		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L,&c);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H,&c1);

		h_act = c1;
		h_act = h_act << 8;
		h_act = h_act + c;

		pr_info("   Total resolution is %d * %d \n", h_res, v_res);
		pr_info("   Active resolution is %d * %d ", h_act, v_act);
	*/
	{
		fresh_rate = pclk * 1000;
		fresh_rate = fresh_rate / h_res;
		fresh_rate = fresh_rate * 1000;
		fresh_rate = fresh_rate / v_res;
		//pr_info(" @ %.2fHz\n", fresh_rate);
	}

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL,&c);
	if((c & 0x06) == 0x00)
		pr_info("   ColorSpace: RGB,");
	else if((c & 0x06) == 0x02)
		pr_info("   ColorSpace: YCbCr422,");
	else if((c & 0x06) == 0x04)
		pr_info("   ColorSpace: YCbCr444,");


	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL,&c);
	if((c & 0xe0) == 0x00)
		pr_info("  6 BPC");
	else if((c & 0xe0) == 0x20)
		pr_info("  8 BPC");
	else if((c & 0xe0) == 0x40)
		pr_info("  10 BPC");
	else if((c & 0xe0) == 0x60)
		pr_info("  12 BPC");

#ifdef ANX7730
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x05, 0x23, 1, ByteBuf);
	if((ByteBuf[0]&0x0f)!=0x02) {
		pr_info("   ANX7730 BB current FW Ver %.2x \n", (unsigned int)(ByteBuf[0]&0x0f));
		pr_info("   It can be updated to the latest version 02 with the command:\update7730 ");
	} else
		pr_info("   ANX7730 BB current FW is the latest version 02.");

#endif

	pr_info("\n********************************************************************\n");

}

void SP_TX_AUX_WR (BYTE offset)
{
	BYTE c,cnt;
	cnt = 0;
	//load offset to fifo
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, offset);
	//set I2C write com 0x04 mot = 1
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	//enable aux
	#ifdef Standard_DP
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x09);
	#else
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	#endif
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c&0x01) {
		msleep(10);
		cnt ++;
		//pr_info("cntwr = %.2x\n",(unsigned int)cnt);
		if(cnt == 10) {
			pr_info("write break");
			//SP_TX_RST_AUX();
			cnt = 0;
			bEDIDBreak=1;
			break;
		}
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	}

}

void SP_TX_AUX_RD (BYTE len_cmd)
{
	BYTE c,cnt;
	cnt = 0;

	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, len_cmd);
	//enable aux
	#ifdef Standard_DP
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x09);
	#else
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	#endif
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01) {
		msleep(10);
		cnt ++;
		//pr_info("cntrd = %.2x\n",(unsigned int)cnt);
		if(cnt == 10) {
			pr_info("read break");
			SP_TX_RST_AUX();
			bEDIDBreak=1;
			break;
		}
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	}

}



void SP_TX_Insert_Err(void)
{
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_DEBUG_REG, 0x02);
	pr_info("Insert err\n");
}

BYTE SP_TX_Chip_Located(void)
{
	BYTE c1,c2,c3;
	SP_TX_Hardware_PowerOn();
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDL_REG , &c1);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDH_REG , &c2);
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_DEV_REV_REG , &c3);
	if ((c1==0x05) && (c2==0x78)&&(c3==0xca)) {
		pr_info("ANX7805 Reversion CA");
		return 1;
	} else {
		pr_info("dev IDL = %.2x, deb IDH = %.2x, REV= %.2x\n",(unsigned int)c1,(unsigned int)c2,(unsigned int)c3);
		return 0;
	}
}
void SP_TX_Hardware_PowerOn(void)
{
	sp_tx_hardware_poweron();
	pr_info("Chip is power on\n");
}

void SP_TX_Hardware_PowerDown(void)
{
	sp_tx_hardware_powerdown();
	pr_info("Chip is power down\n");

}
void vbus_power_ctrl(BYTE ON)
{
	BYTE c, i;
	if(ON == 0) {
		sp_read_reg (SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, (c& (~V33_SWITCH_ON)));
		//Power down  5V detect and short portect circuit
		sp_read_reg (SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, c|0x30);
		pr_info("3.3V output disabled");
	} else {
		for (i = 0; i < 5; i++) {
			//Power up  5V detect and short portect circuit
			sp_write_reg_mask(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, ~0x30, 0x00);
			// Enable power 3.3v out
			sp_read_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, &c);
			c &= ~V33_SWITCH_ON;
			sp_write_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, c);
			c |= V33_SWITCH_ON;
			sp_write_reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, c);

			msleep(100);
			sp_read_reg (SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, &c);
			if (!(c & 0xc0)) {
				pr_info("3.3V output enabled\n");
				break;
			} else {
				pr_info("VBUS power can not be supplied\n");
			}
		}

	}


}


/*
void SP_TX_Hardware_Reset(void)
{
    SP_TX_HW_RESET= 0;
    msleep(20);
    SP_TX_HW_RESET = 1;
    msleep(10);
}
*/

void SP_TX_CONFIG_SSC(SP_LINK_BW linkbw)
{
	BYTE c;

	sp_write_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, 0x00); 			// disable SSC first
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, 0x00);		//disable speed first


	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00,DPCD_MAX_DOWNSPREAD,1,&c);


#ifndef SSC_1
	//pr_info("############### Config SSC 0.4% ####################");
	if(linkbw == BW_54G) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xc0);	              // set value according to mehran CTS report -ANX.Fei-20111009
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x75);			// ctrl_th

	} else if(linkbw == BW_27G) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x5f);			//  set value according to mehran CTS report -ANX.Fei-20111009
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x75);			// ctrl_th
	} else {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x9e);         	//  set value according to mehran CTS report -ANX.Fei-20111009
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);			// ctrl_th
	}
#else
	//pr_info("############### Config SSC 1% ####################");
	if(linkbw == BW_54G) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xdd);	              // ssc d  1%, f0/8 mode
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x01);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x76);			// ctrl_th
	} else if(linkbw == BW_27G) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xef);			// ssc d  1%, f0/4 mode
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x76);			// ctrl_th
	} else {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x8e);			// ssc d 0.4%, f0/4 mode
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x01);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);			// ctrl_th
	}
#endif

	//Enable SSC
	SP_TX_SPREAD_Enable(1);


}


void SP_TX_Config_Audio_BIST(struct AudioFormat *bAudioFormat)
{
	BYTE c;

	pr_info("############## Config BIST audio #####################\n");
	// config audio bist status
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status1, 0x00 ); // configure channel status1
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status2, 0x00); // configure channel status2
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status3, 0x00); // configure channel status3
	c = bAudioFormat->bAudio_Fs;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status4, c ); // configure channel status4
	c = bAudioFormat->bAudio_word_len;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status5, c ); // configure channel status5

	// Disable SPDIF input
	sp_read_reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, (c & 0x7e )|0x01);

	// disable  I2S input
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL, 0x00 );

	// enable AUDIO bist
	sp_read_reg (SP_TX_PORT2_ADDR, AUDIO_BIST_CTRL, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, AUDIO_BIST_CTRL, (c | 0xf1) );   // max sin amp
}

void SP_TX_Config_Audio_SPDIF(void)
{
	BYTE c;

	pr_info("############## Config SPDIF audio #####################\n");

	// disable  I2S input
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL, 0x00 );

	sp_read_reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, (c | SPDIF_AUDIO_CTRL0_SPDIF_IN) ); // enable SPDIF input

	msleep(2);

	sp_read_reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_STATUS0, &c);

	if ( ( c & SPDIF_AUDIO_STATUS0_CLK_DET ) != 0 )
		pr_info("SPDIF Clock is detected!\n");
	else
		pr_info("ERR:SPDIF Clock is Not detected!\n");

	if ( ( c & SPDIF_AUDIO_STATUS0_AUD_DET ) != 0 )
		pr_info("SPDIF Audio is detected!\n");
	else
		pr_info("ERR:SPDIF Audio is Not detected!\n");
}

void SP_TX_Config_Audio_I2S(struct AudioFormat *bAudioFormat)
{
	BYTE c;

	// Disable SPDIF input
	sp_read_reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, ((c & 0x7e )|0x01));

	pr_info("############## Config I2S audio #####################\n");
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL,&c); // enable I2S input
	c = (c&~0xff) | (bAudioFormat->bI2S_FORMAT.SHIFT_CTRL<<3) |bAudioFormat->bI2S_FORMAT.DIR_CTRL <<2
	    | (bAudioFormat->bI2S_FORMAT.WS_POL <<1) | (bAudioFormat->bI2S_FORMAT.JUST_CTRL);
	switch(bAudioFormat->bI2S_FORMAT.Channel_Num) {
	case I2S_CH_2:
		c = c|0x10;
		break;
	case I2S_CH_4:
		c = c|0x30;
		break;
	case I2S_CH_6:
		c = c|0x70;
		break;
	case I2S_CH_8:
		c = c|0xf0;
		break;
	default:
		c = c|0x10;
		break;
	}
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL,c); // enable I2S input

	sp_read_reg (SP_TX_PORT2_ADDR, SP_TX_I2S_SWAP_WORD_LENGTH, &c);
	c = c & 0xF0;
	c = c | bAudioFormat->bAudio_word_len;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_SWAP_WORD_LENGTH, c); // enable I2S input

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL,&c); // select I2S clock as audio reference clock-2011.9.9-ANX.Fei
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL,c|0x06);

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_FMT,&c); // configure I2S format
	c =  (c&~0xe5)| (bAudioFormat->bI2S_FORMAT.EXT_VUCP <<2) | (bAudioFormat->bI2S_FORMAT.AUDIO_LAYOUT)
	     | (bAudioFormat->bI2S_FORMAT.Channel_Num << 5);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_FMT,c); // configure I2S format


	c = bAudioFormat->bI2S_FORMAT.Channel_status1;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status1, c ); // configure I2S channel status1
	c = bAudioFormat->bI2S_FORMAT.Channel_status2;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status2, c ); // configure I2S channel status2
	c = bAudioFormat->bI2S_FORMAT.Channel_status3;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status3, c ); // configure I2S channel status3
	c = bAudioFormat->bI2S_FORMAT.Channel_status4;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status4, c ); // configure I2S channel status4
	c = bAudioFormat->bI2S_FORMAT.Channel_status5;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status5, c ); // configure I2S channel status5

}
void SP_TX_Config_Audio_Slimbus(struct AudioFormat *bAudioFormat)
{
//TO DO
	bAudioFormat = bAudioFormat;
}


void SP_TX_Enable_Audio_Output(BYTE bEnable)
{
	BYTE c;

	sp_read_reg (SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, &c);
	if(bEnable) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, ( c| SP_TX_AUD_CTRL_AUD_EN ) ); // enable SP audio

		SP_TX_InfoFrameUpdate(&SP_TX_AudioInfoFrmae);
	} else {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, (c &(~SP_TX_AUD_CTRL_AUD_EN))); // Disable SP audio


		sp_read_reg (SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ( c&(~SP_TX_PKT_AUD_EN )) ); // Disable the audio info-frame
	}


}

void SP_TX_Disable_Audio_Input(void)
{
	BYTE c;

	sp_read_reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, (c &(~SPDIF_AUDIO_CTRL0_SPDIF_IN))); // Disable SPDIF

	// disable  I2S input
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL, 0x00 );

}


void SP_TX_AudioInfoFrameSetup(struct AudioFormat *bAudioFormat)
{
	BYTE freq = 0, bitWidth = 0, temp = 0;

	if (bAudioFormat->bAudio_word_len == 0x02)
		bitWidth = 1;
	else if ((bAudioFormat->bAudio_word_len == 0x0a) || (bAudioFormat->bAudio_word_len == 0x03))
		bitWidth = 2;
	else if (bAudioFormat->bAudio_word_len == 0x0b)
		bitWidth = 3;

	if (bAudioFormat->bAudio_Fs == AUDIO_FS_441K)
		freq = 2;
	else if (bAudioFormat->bAudio_Fs == AUDIO_FS_48K)
		freq = 3;
	else if (bAudioFormat->bAudio_Fs == AUDIO_FS_32K)
		freq = 1;
	else if (bAudioFormat->bAudio_Fs == AUDIO_FS_882K)
		freq = 4;
	else if (bAudioFormat->bAudio_Fs == AUDIO_FS_96K)
		freq = 5;
	else if (bAudioFormat->bAudio_Fs == AUDIO_FS_1764K)
		freq = 6;
	else if (bAudioFormat->bAudio_Fs == AUDIO_FS_192K)
		freq = 7;

	temp = freq << 2 | bitWidth;
	SP_TX_AudioInfoFrmae.type = 0x84;
	SP_TX_AudioInfoFrmae.version = 0x01;
	SP_TX_AudioInfoFrmae.length = 0x0A;

	if(bAudioFormat ->bAudioType ==AUDIO_I2S) {
		switch(bAudioFormat->bI2S_FORMAT.Channel_Num) {
		case I2S_CH_2:
			SP_TX_AudioInfoFrmae.pb_byte[0]=0x01;//coding type ,refer to stream header, audio channel count,two channel
			SP_TX_AudioInfoFrmae.pb_byte[3]=0x00;//for multi channel LPCM
			break;
		case I2S_CH_4:
			SP_TX_AudioInfoFrmae.pb_byte[0]=0x03;
			SP_TX_AudioInfoFrmae.pb_byte[3]=0x08;
			break;
		case I2S_CH_6:
			SP_TX_AudioInfoFrmae.pb_byte[0]=0x05;
			SP_TX_AudioInfoFrmae.pb_byte[3]=0x0b;
			break;
		case I2S_CH_8:
			SP_TX_AudioInfoFrmae.pb_byte[0]=0x07;
			SP_TX_AudioInfoFrmae.pb_byte[3]=0x13;
			break;
		default:
			break;
		}

	} else {

		SP_TX_AudioInfoFrmae.pb_byte[0]=0x00;//coding type ,refer to stream header, audio channel count,two channel
		SP_TX_AudioInfoFrmae.pb_byte[3]=0x00;//for multi channel LPCM
	}


	//SP_TX_AudioInfoFrmae.pb_byte[0]=0x00;//coding type ,refer to stream header, audio channel count,two channel
	SP_TX_AudioInfoFrmae.pb_byte[1]=temp;//refer to stream header
	SP_TX_AudioInfoFrmae.pb_byte[2]=0x00;
	//SP_TX_AudioInfoFrmae.pb_byte[3]=0x00;//for multi channel LPCM
	SP_TX_AudioInfoFrmae.pb_byte[4]=0x00;//for multi channel LPCM
	SP_TX_AudioInfoFrmae.pb_byte[5]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[6]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[7]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[8]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[9]=0x00;//reserved to 0
}

void SP_TX_InfoFrameUpdate(struct AudiInfoframe* pAudioInfoFrame)
{
	BYTE c;

	c = pAudioInfoFrame->type;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_TYPE, c); // Audio infoframe


	c = pAudioInfoFrame->version;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_VER,	c);

	c = pAudioInfoFrame->length;
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_LEN,	c);

	c = pAudioInfoFrame->pb_byte[0];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB0,c);

	c = pAudioInfoFrame->pb_byte[1];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB1,c);

	c = pAudioInfoFrame->pb_byte[2];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB2,c);

	c = pAudioInfoFrame->pb_byte[3];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB3,c);

	c = pAudioInfoFrame->pb_byte[4];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB4,c);

	c = pAudioInfoFrame->pb_byte[5];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB5,c);

	c = pAudioInfoFrame->pb_byte[6];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB6,c);

	c = pAudioInfoFrame->pb_byte[7];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB7,c);

	c = pAudioInfoFrame->pb_byte[8];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB8,c);

	c = pAudioInfoFrame->pb_byte[9];
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB9,c);


	sp_read_reg (SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ( c | SP_TX_PKT_AUD_UP ) ); // update the audio info-frame


	sp_read_reg (SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ( c | SP_TX_PKT_AUD_EN ) ); // enable the audio info-frame
}


void SP_TX_Config_Audio(struct AudioFormat *bAudio)
{
	BYTE c;
	pr_info("############## Config audio #####################");

	SP_TX_Power_Enable(SP_TX_PWR_AUDIO, SP_TX_POWER_ON);

	switch(bAudio->bAudioType) {
	case AUDIO_I2S:
		SP_TX_Config_Audio_I2S(bAudio);
		break;
	case AUDIO_SPDIF:
		SP_TX_Config_Audio_SPDIF();
		break;
	case AUDIO_BIST:
		SP_TX_Config_Audio_BIST(bAudio);
		break;
	case AUDIO_SLIMBUS:
		SP_TX_Config_Audio_Slimbus(bAudio);
		break;
	default:
		pr_info("ERR:Illegal audio format.\n");
		break;
	}

	// write audio info-frame
	SP_TX_AudioInfoFrameSetup(bAudio);
	SP_TX_Enable_Audio_Output(1);//enable audio

	sp_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, c|0x04);//Unmask audio clock change int
	/*SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);*/
	SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);

}

void SP_TX_Update_Audio(struct AudioFormat *bAudio)
{
	BYTE c;
	pr_err("############## Config audio #####################");

	SP_TX_Power_Enable(SP_TX_PWR_AUDIO, SP_TX_POWER_ON);

	switch(bAudio->bAudioType) {
	case AUDIO_I2S:
		SP_TX_Config_Audio_I2S(bAudio);
		break;
	case AUDIO_SPDIF:
		SP_TX_Config_Audio_SPDIF();
		break;
	case AUDIO_BIST:
		SP_TX_Config_Audio_BIST(bAudio);
		break;
	case AUDIO_SLIMBUS:
		SP_TX_Config_Audio_Slimbus(bAudio);
		break;
	default:
		pr_err("ERR:Illegal audio format.\n");
		break;
	}

	// write audio info-frame
	SP_TX_AudioInfoFrameSetup(bAudio);
	SP_TX_Enable_Audio_Output(1);//enable audio

	sp_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, c|0x04);//Unmask audio clock change int
	//SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
	SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);

}



void SP_TX_RST_AUX(void)
{
	BYTE c,c1;

	//pr_info("reset aux");

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c1);
	c = c1;
	c1&=0xdd;//clear HPD polling and Transmitter polling
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c1); //disable  polling  before reset AUX-ANX.Fei-2011.9.19

	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, &c1);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c1|SP_TX_AUX_RST);
	msleep(1);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c1& (~SP_TX_AUX_RST));

	//set original polling enable
	//sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c1);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c); //enable  polling  after reset AUX-ANX.Fei-2011.9.19
}


BYTE SP_TX_AUX_DPCDRead_Bytes(BYTE addrh, BYTE addrm, BYTE addrl,BYTE cCount,BYTE * pBuf)
{
	BYTE c,i;
	BYTE bOK;
	//BYTE c1;

	//clr buffer
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);

	//set read cmd and count
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, ((cCount-1) <<4)|0x09);


	//set aux address15:0
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	//set address19:16 and enable aux
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, (c & 0xf0) | addrh);


	//Enable Aux
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c | 0x01);


	msleep(2);

	bOK = SP_TX_Wait_AUX_Finished();

	if(!bOK) {
#ifdef AUX_DBG
		pr_info("aux read failed");
#endif
		if(SP_TX_HDCP_AUTHENTICATION != get_system_state())
			SP_TX_RST_AUX();
		return AUX_ERR;
	}

	for(i =0; i<cCount; i++) {
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG+i, &c);

		*(pBuf+i) = c;

		if(i >= MAX_BUF_CNT)
			break;
	}

	return AUX_OK;

}


BYTE SP_TX_AUX_DPCDWrite_Bytes(BYTE addrh, BYTE addrm, BYTE addrl,BYTE cCount,BYTE * pBuf)
{
	BYTE c,i;
	BYTE bOK;

	//clr buffer
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);

	//set write cmd and count;
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, ((cCount-1) <<4)|0x08);

	//set aux address15:0
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	//set address19:16
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, (c & 0xf0) | addrh);


	//write data to buffer
	for(i =0; i<cCount; i++) {
		c = *pBuf;
		pBuf++;
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG+i, c);

		if(i >= MAX_BUF_CNT)
			break;
	}

	//Enable Aux
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c | 0x01);

	bOK = SP_TX_Wait_AUX_Finished();

	if(bOK)
		return AUX_OK;
	else {
#ifdef AUX_DBG
		pr_info("aux write failed");
#endif
		//SP_TX_RST_AUX();
		return AUX_ERR;
	}

}

BYTE SP_TX_AUX_DPCDWrite_Byte(BYTE addrh, BYTE addrm, BYTE addrl, BYTE data1)
{
	return SP_TX_AUX_DPCDWrite_Bytes(addrh, addrm, addrl, 1, &data1);
}



BYTE SP_TX_Wait_AUX_Finished(void)
{
	BYTE c;
	BYTE cCnt;
	cCnt = 0;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01) {
		cCnt++;
		 delay_ms(1);
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

		if(cCnt>100) {
#ifdef AUX_DBG
			pr_info("AUX Operaton does not finished, and tome out.");
#endif
			return 0;
		}

	}

    sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);
	if(c&0x0F) {
#ifdef AUX_DBG
		pr_info("aux operation failed %.2x\n",(unsigned int)c);
#endif
		return 0;
	} else
		return 1; //succeed

	//sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);
	//if(c&0x0f !=0)
	//pr_info("**AUX Access error code = %.2x***\n",(unsigned int)c);

}


/*
void SP_TX_SW_Reset(void)
{
	BYTE c;

	//software reset
	sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c | SP_TX_RST_SW_RST);
	msleep(10);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c & ~SP_TX_RST_SW_RST);
}

*/


void SP_TX_SPREAD_Enable(BYTE bEnable)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, &c);

	if(bEnable) {
		sp_write_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, c | SPREAD_AMP);// enable SSC
		//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, (c &(~0x04)));// powerdown SSC

		//reset SSC
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, &c);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c | SP_TX_RST_SSC);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c & (~SP_TX_RST_SSC));

		//enable the DPCD SSC
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01, DPCD_DOWNSPREAD_CTRL,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, (c | 0x10));

	} else {
		sp_write_reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, (c & (~SPREAD_AMP)));// disable SSC
		//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, c|0x04);// powerdown SSC
		//disable the DPCD SSC
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01, DPCD_DOWNSPREAD_CTRL,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, (c & 0xef));
	}

}



void SP_TX_Get_Int_status(INTStatus IntIndex, BYTE *cStatus)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1 + IntIndex, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1 + IntIndex, c);

	*cStatus = c;
}

/*
void SP_TX_Get_HPD_status( BYTE *cStatus)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);

	*cStatus = c;
}
*/

BYTE SP_TX_Get_PLL_Lock_Status(void)
{
	BYTE c;
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c);
	return (c & SP_TX_DEBUG_PLL_LOCK) == SP_TX_DEBUG_PLL_LOCK ? 1 : 0;
}




void SP_TX_HDCP_ReAuth(void)
{
	BYTE c;
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	c |= 0x20;
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
	c &= ~(0x20);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
}


void SP_TX_Lanes_PWR_Ctrl(ANALOG_PWD_BLOCK eBlock, BYTE powerdown)
{
	BYTE c;

	switch(eBlock) {

	case CH0_BLOCK:
		if(powerdown) {
			sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
			c|=SP_TX_ANALOG_POWER_DOWN_CH0_PD;
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
		} else {
			sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
			c&=~SP_TX_ANALOG_POWER_DOWN_CH0_PD;
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
		}

		break;

	case CH1_BLOCK:
		if(powerdown) {
			sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
			c|=SP_TX_ANALOG_POWER_DOWN_CH1_PD;
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
		} else {
			sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
			c&=~SP_TX_ANALOG_POWER_DOWN_CH1_PD;
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
		}

		break;

	default:
		break;
	}
}

/*

void SP_TX_Get_Rx_LaneCount(BYTE bMax,BYTE *cLaneCnt)
{
	if(bMax)
	    SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00,DPCD_MAX_LANE_COUNT,1,cLaneCnt);
	else
	    SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01,DPCD_LANE_COUNT_SET,1,cLaneCnt);
}


void SP_TX_Set_Rx_laneCount(BYTE cLaneCnt)
{
	SP_TX_AUX_DPCDWrite_Byte(0x00, 0x00, DPCD_LANE_COUNT_SET, cLaneCnt);
}
*/

void SP_TX_Get_Rx_BW(BYTE bMax,BYTE *cBw)
{
	if(bMax)
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00,DPCD_MAX_LINK_RATE,1,cBw);
	else
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01,DPCD_LINK_BW_SET,1,cBw);
}

/*
void SP_TX_Set_Rx_BW(BYTE cBw)
{
	SP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_LINK_BW_SET, cBw);
}
*/



void SP_TX_Get_Link_BW(BYTE *bwtype)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);

	*bwtype = c;
}

/*
void SP_TX_Get_Lane_Count(BYTE *count)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_LANE_COUNT_SET_REG, &c);

	*count = c;

}
*/

void SP_TX_EDID_Read_Initial(void)
{
	BYTE c;

	//Set I2C address
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x50);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, 0);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, c & 0xf0);
}

BYTE SP_TX_AUX_EDIDRead_Byte(BYTE offset)
{
	BYTE c,i,edid[16],data_cnt,cnt,vsdbdata[4],VSDBaddr;
	BYTE bReturn=0;
	//pr_info("***************************offset = %.2x\n", (unsigned int)offset);
	VSDBaddr= 0;
	vsdbdata[0] = 0;
	cnt = 0;

	SP_TX_AUX_WR(offset);//offset

	if((offset == 0x00) || (offset == 0x80))
		checksum = 0;

	SP_TX_AUX_RD(0xf5);//set I2C read com 0x05 mot = 1 and read 16 bytes

	data_cnt = 0;
	while(data_cnt < 16) {
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
		c = c & 0x1f;
		//pr_info("cnt_d = %.2x\n",(unsigned int)c);
		if(c != 0) {
			for( i = 0; i < c; i ++) {
				sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG + i, &edid[i + data_cnt]);
				//pr_info("edid[%.2x] = %.2x\n",(unsigned int)(i + offset),(unsigned int)edid[i + data_cnt]);
				checksum = checksum + edid[i + data_cnt];
			}
		} else {
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
			//enable aux
			//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only
			#ifdef Standard_DP
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x0b);
			#else
			sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
			#endif
			sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
			while(c & 0x01) {
				msleep(2);
				cnt ++;
				if(cnt == 10) {
					pr_info("read break");
					SP_TX_RST_AUX();
					bEDIDBreak=1;
					bReturn = 0x01;
				}
				sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
			}
			//pr_info("cnt_d = 0, break");
			sp_tx_edid_err_code = 0xff;
			bReturn = 0x02;// for fixing bug leading to dead lock in loop "while(data_cnt < 16)"
			return bReturn;
		}
		data_cnt = data_cnt + c;
		if(data_cnt < 16) { // 080610. solution for handle case ACK + M byte

			//SP_TX_AUX_WR(offset);
			SP_TX_RST_AUX();
			msleep(10);

			c = 0x05 | ((0x0f - data_cnt) << 4);//Read MOT = 1
			SP_TX_AUX_RD(c);
			//pr_info("M < 16");
		}
	}

	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
	//enable aux
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only to stop EDID reading
	#ifdef Standard_DP
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x0b);
	#else
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	#endif
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01)
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	//pr_info("***************************offset %.2x reading completed\n", (unsigned int)offset);


	if(EDID_Print_Enable) {
		for(i=0; i<16; i++) {
			if((i&0x0f)==0)
				pr_info("\n edid: [%.2x]  %.2x  ", (unsigned int)offset, (unsigned int)edid[i]);
			else
				pr_info("%.2x  ", (unsigned int)edid[i]);

			if((i&0x0f)==0x0f)
				pr_info("\n");
		}

	}

//#if 0
	if(offset < 0x80) {
		for(i=0; i<16; i++) //record all 128 data in extsion block.
			bEDID_firstblock[offset+i]=edid[i];
	} else if(offset >= 0x80) {
		for(i=0; i<16; i++) //record all 128 data in extsion block.
			bEDID_extblock[offset-0x80+i]=edid[i];
	}


//#else

	if(offset == 0x00) {
		if((edid[0] == 0) && (edid[7] == 0) && (edid[1] == 0xff) && (edid[2] == 0xff) && (edid[3] == 0xff)
		   && (edid[4] == 0xff) && (edid[5] == 0xff) && (edid[6] == 0xff))
			pr_info("Good EDID header!");
		else {
			pr_info("Bad EDID header!");
			sp_tx_edid_err_code = 0x01;
		}

	}

	else if(offset == 0x30) {
		for(i = 0; i < 10; i ++ )
			SP_TX_EDID_PREFERRED[i] = edid[i + 6];//edid[0x36]~edid[0x3f]
	}

	else if(offset == 0x40) {
		for(i = 0; i < 8; i ++ )
			SP_TX_EDID_PREFERRED[10 + i] = edid[i];//edid[0x40]~edid[0x47]
	}

	else if(offset == 0x70) {
		checksum = checksum&0xff;
		checksum = checksum - edid[15];
		checksum = ~checksum + 1;
		if(checksum != edid[15]) {
			pr_info("Bad EDID check sum1!");
			sp_tx_edid_err_code = 0x02;
			checksum = edid[15];
			bEDIDBreak=1;
		} else
			pr_info("Good EDID check sum1!");
	}
	/*
	    else if(offset == 0xf0)
	    {
	        checksum = checksum - edid[15];
	        checksum = ~checksum + 1;
	        if(checksum != edid[15])
	        {
	            pr_info("Bad EDID check sum2!");
	            sp_tx_edid_err_code = 0x02;
	        }
		 else
		 	pr_info("Good EDID check sum2!");
	    }*/
	else if( (offset >= 0x80)&&(sp_tx_ds_edid_hdmi==0)) {
		if(offset ==0x80) {
			if(edid[0] !=0x02)
				return 0x03;
		}
		for(i=0; i<16; i++) //record all 128 data in extsion block.
			EDIDExtBlock[offset-0x80+i]=edid[i];
		/*
				for(i=0;i<16;i++)
				{
					if((i&0x0f)==0)
						pr_info("\n edid: [%.2x]  %.2x  ", (unsigned int)offset, (unsigned int)EDIDExtBlock[offset-0x80+i]);
					else
						pr_info("%.2x  ", (unsigned int)edid[i]);

					if((i&0x0f)==0x0f)
						pr_info("\n");
				}*/

		if(offset ==0x80)
			DTDbeginAddr = edid[2];

		if(offset == 0xf0) {
			checksum = checksum - edid[15];
			checksum = ~checksum + 1;
			if(checksum != edid[15]) {
				pr_info("Bad EDID check sum2!");
				sp_tx_edid_err_code = 0x02;
			} else
				pr_info("Good EDID check sum2!");

			for(VSDBaddr = 0x04; VSDBaddr <DTDbeginAddr;) {
				//pr_info("####VSDBaddr = %.2x\n",(unsigned int)(VSDBaddr));

				vsdbdata[0] = EDIDExtBlock[VSDBaddr];
				vsdbdata[1] = EDIDExtBlock[VSDBaddr+1];
				vsdbdata[2] = EDIDExtBlock[VSDBaddr+2];
				vsdbdata[3] = EDIDExtBlock[VSDBaddr+3];

				// pr_info("vsdbdata= %.2x,%.2x,%.2x,%.2x\n",(unsigned int)vsdbdata[0],(unsigned int)vsdbdata[1],(unsigned int)vsdbdata[2],(unsigned int)vsdbdata[3]);
				if((vsdbdata[0]&0xe0)==0x60) {


					if((vsdbdata[0]&0x1f) > 0x08) {
						if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0x80) {
							if((EDIDExtBlock[VSDBaddr+11]&0x80)	== 0x80)
								sp_tx_ds_edid_3d_present = 1;
							pr_info("Downstream monitor supports 3D");

						} else if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0x40) {
							if((EDIDExtBlock[VSDBaddr+11]&0x80)	== 0x80)
								sp_tx_ds_edid_3d_present = 1;
							pr_info("Downstream monitor supports 3D");

						} else if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0xc0) {
							if((EDIDExtBlock[VSDBaddr+13]&0x80)	== 0x80)
								sp_tx_ds_edid_3d_present = 1;
							pr_info("Downstream monitor supports 3D");

						} else if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0x00) {
							if((EDIDExtBlock[VSDBaddr+9]&0x80)	== 0x80)
								sp_tx_ds_edid_3d_present = 1;
							pr_info("Downstream monitor supports 3D");

						} else {
							sp_tx_ds_edid_3d_present = 0;
							pr_info("Downstream monitor does not support 3D");
						}


					}

					if((vsdbdata[1]==0x03)&&(vsdbdata[2]==0x0c)&&(vsdbdata[3]==0x00)) {
						sp_tx_ds_edid_hdmi = 1;
						return 0;
					} else {
						sp_tx_ds_edid_hdmi = 0;
						return 0x03;
					}

				} else {
					sp_tx_ds_edid_hdmi = 0;
					VSDBaddr = VSDBaddr+(vsdbdata[0]&0x1f);
					VSDBaddr = VSDBaddr + 0x01;
				}

				if(VSDBaddr > DTDbeginAddr)
					return 0x03;

			}
		}

	}
//#endif
	return bReturn;
}



void SP_TX_Parse_Segments_EDID(BYTE segment, BYTE offset)
{
	BYTE c,cnt;
	int i;
	BYTE edid[16];
	
	//set I2C write com 0x04 mot = 1
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);

	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x30);

	// adress_only
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only
	#ifdef Standard_DP
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x0b);
	#else
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);
	#endif
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	//while(c & 0x01)
	//	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	SP_TX_Wait_AUX_Finished();
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, &c);

	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, segment);

	//set I2C write com 0x04 mot = 1
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	//enable aux
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, (c&(~0x02))|(0x01));
	

	cnt = 0;
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	while(c&0x01) {
		msleep(10);
		cnt ++;
		//pr_info("cntwr = %.2x\n",(unsigned int)cnt);
		if(cnt == 10) {
			pr_info("write break");
			SP_TX_RST_AUX();
			cnt = 0;
			bEDIDBreak=1;
			return;// bReturn;
		}
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	}

	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x50);//set EDID addr 0xa0
	// adress_only
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only

	SP_TX_AUX_WR(offset);//offset
	//adress_only
	//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only

	SP_TX_AUX_RD(0xf5);//set I2C read com 0x05 mot = 1 and read 16 bytes
	cnt = 0;
	for(i=0; i<16; i++) {
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
		while((c & 0x1f) == 0) {
			msleep(2);
			cnt ++;
			sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
			if(cnt == 10) {
				pr_info("read break");
				SP_TX_RST_AUX();
				bEDIDBreak=1;
				return;
			}
		}


		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG+i, &edid[i]);
		//pr_info("edid[0x%.2x] = 0x%.2x\n",(unsigned int)(offset+i),(unsigned int)c);
	}
	
	
	for(i=0; i<16; i++) //record all 256data in extsion block.
		bEDID_fourblock[offset+i]=edid[i];


	///*
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
	//enable aux
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c|0x03);//set address only to stop EDID reading
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c&(~0x02));//set address only to stop EDID reading
	
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01)
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

}





BYTE SP_TX_Get_EDID_Block(void)
{
	BYTE c;
	SP_TX_AUX_WR(0x00);
	SP_TX_AUX_RD(0x01);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, &c);
	//pr_info("[a0:00] = %.2x\n", (unsigned int)c);

	SP_TX_AUX_WR(0x7e);
	SP_TX_AUX_RD(0x01);
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, &c);

	if((c >= 0)&&(c<=3))
	{
		pr_info("EDID Block = %d\n",(int)(c+1));
	}
	else
	{
		c=1;// default 2 block
		bEDIDBreak=1;
		debug_puts("bad EDID block");
	}
	return c;
}




void SP_TX_AddrOnly_Set(BYTE bSet)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	if(bSet) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, (c|SP_TX_ADDR_ONLY_BIT));
	} else {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, (c&~SP_TX_ADDR_ONLY_BIT));
	}
}

/*
void SP_TX_Scramble_Enable(BYTE bEnabled)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, &c);
	if(bEnabled)//enable scramble
	{
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, (c&~SP_TX_SCRAMBLE_DISABLE));
	}
	else
	{
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, (c|SP_TX_SCRAMBLE_DISABLE));
	}

}
*/


void SP_TX_API_M_GEN_CLK_Select(BYTE bSpreading)
{
	BYTE c;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, &c);
	if(bSpreading) {
		//M value select, select clock with downspreading
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, (c | M_GEN_CLK_SEL));
	} else {
		//M value select, initialed as clock without downspreading
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, c&(~M_GEN_CLK_SEL));
	}
}
#if(ENABLE_3D)
/*
	Function: sp_tx_send_3d_vsi_packet_to_7730
	Parameter:
		video_fromat
			0x00 framepacking
			0x01 field alternative
			0x02 line alternative
			0x06 top-and-bottom
			0x08 side by side(half)
*/
void sp_tx_send_3d_vsi_packet_to_7730(BYTE video_format)
{

	BYTE i;
	for(i = 0; i < MPEG_PACKET_SIZE; i++) {
		SP_TX_Packet_MPEG.MPEG_data[i] = 0;
	}
	if (video_format != 0xFF) {
		SP_TX_Packet_MPEG.MPEG_data[0] = 0x03;
		SP_TX_Packet_MPEG.MPEG_data[1] = 0x0C;
		SP_TX_Packet_MPEG.MPEG_data[2] = 0x00;
		SP_TX_Packet_MPEG.MPEG_data[3] = 0x40;
		SP_TX_Packet_MPEG.MPEG_data[4] =(BYTE) (video_format << 4 );
	}

	SP_TX_Config_Packets(VSI_PACKETS);

	if (video_format != 0xFF) {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_VSC_DB1, 0x04);
		sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_3D_VSC_CTRL, 0xFF, INFO_FRAME_VSC_EN);
	} else {
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_VSC_DB1, 0x0);
		sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_3D_VSC_CTRL, 0xFE, 0x0);
	}
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ~0x01, 0x00);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, 0x10);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, 0x01);


}
#endif

void SP_TX_Config_Packets(PACKETS_TYPE bType)
{
	BYTE c,c1;
	unsigned int h_act,v_act;

	switch(bType) {
	case AVI_PACKETS:
		//clear packet enable
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c&(~SP_TX_PKT_AVI_EN));

		//get input color space
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL, &c);

		SP_TX_Packet_AVI.AVI_data[0] = SP_TX_Packet_AVI.AVI_data[0] & 0x9f;
		SP_TX_Packet_AVI.AVI_data[0] = SP_TX_Packet_AVI.AVI_data[0] | (c <<4);

		//pr_info("AVI 0 =%x\n", (unsigned int)SP_TX_Packet_AVI.AVI_data[0]);


		//set timing ID

		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L,&c);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H,&c1);

		v_act = c1;
		v_act = v_act << 8;
		v_act = v_act + c;


		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L,&c);
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H,&c1);

		h_act = c1;
		h_act = h_act << 8;
		h_act = h_act + c;

		if(((v_act > 470)&&(v_act < 490))&&((h_act > 710)&&(h_act < 730)))
			SP_TX_Packet_AVI.AVI_data[3] = 0x03;
		else if (((v_act > 710)&&(v_act < 730))&&((h_act > 1270)&&(h_act < 1290)))
			SP_TX_Packet_AVI.AVI_data[3] = 0x04;
		else if (((v_act > 1070)&&(v_act < 1090))&&((h_act > 1910)&&(h_act < 1930)))
			SP_TX_Packet_AVI.AVI_data[3] = 0x10;
		else
			SP_TX_Packet_AVI.AVI_data[3] = 0x0;

		//Set aspect ratio
		SP_TX_Packet_AVI.AVI_data[1] = SP_TX_Packet_AVI.AVI_data[1] & 0xcf;
		SP_TX_Packet_AVI.AVI_data[1] = SP_TX_Packet_AVI.AVI_data[1] |0x20;

		//Set  limited range
		//SP_TX_Packet_AVI.AVI_data[2] = SP_TX_Packet_AVI.AVI_data[2] & 0xf3;
		//SP_TX_Packet_AVI.AVI_data[2] = SP_TX_Packet_AVI.AVI_data[2] |0x04;


		SP_TX_Load_Packet(AVI_PACKETS);

		//send packet update
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_AVI_UD);

		//enable packet
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_AVI_EN);

		break;

	case SPD_PACKETS:
		//clear packet enable
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c&(~SP_TX_PKT_SPD_EN));

		SP_TX_Load_Packet(SPD_PACKETS);

		//send packet update
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_SPD_UD);

		//enable packet
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_SPD_EN);

		break;
#if(ENABLE_3D)
	case VSI_PACKETS:
		//clear packet enable
		sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, (~SP_TX_PKT_MPEG_EN), 0x00);
		SP_TX_Load_Packet(VSI_PACKETS);
		//send packet update
		sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, SP_TX_PKT_MPEG_UD);
		//enable packet
		sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, SP_TX_PKT_MPEG_EN);

		break;
#endif
	case MPEG_PACKETS:
		//clear packet enable
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c&(~SP_TX_PKT_MPEG_EN));

		SP_TX_Load_Packet(MPEG_PACKETS);

		//send packet update
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_MPEG_UD);

		//enable packet
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_MPEG_EN);

		break;

	default:
		break;
	}

}

void SP_TX_Load_Packet (PACKETS_TYPE type)
{
	BYTE i;

	switch(type) {
	case AVI_PACKETS:
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_TYPE , 0x82);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_VER , 0x02);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_LEN , 0x0d);

		for(i=0; i<13; i++) {
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_AVI_DB0 + i, SP_TX_Packet_AVI.AVI_data[i]);
		}
		break;

	case SPD_PACKETS:
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_TYPE , 0x83);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_VER , 0x01);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_LEN , 0x19);
		for(i=0; i<25; i++) {
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_SPD_DATA1 + i, SP_TX_Packet_SPD.SPD_data[i]);
		}
		break;
#if(ENABLE_3D)
	case VSI_PACKETS:
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_TYPE, 0x81);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_VER, 0x01);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_LEN, 0x05);

		for (i = 0; i < 8; i++) {
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_DATA0 + i,
			             SP_TX_Packet_MPEG.MPEG_data[i]);
		}
		break;
#endif

	case MPEG_PACKETS:
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_TYPE , 0x85);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_VER , 0x01);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_LEN , 0x0a);
		for(i=0; i<10; i++) {
			sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_DATA1 + i, SP_TX_Packet_MPEG.MPEG_data[i]);
		}
		break;

	default:
		break;
	}
}
void SP_TX_AVI_Setup(void)
{
	SP_TX_Packet_AVI.AVI_data[0]=0x10;// Active video, color space RGB
	SP_TX_Packet_AVI.AVI_data[1]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[2]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[3]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[4]=0x00;//repeat 0
	SP_TX_Packet_AVI.AVI_data[5]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[6]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[7]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[8]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[9]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[10]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[11]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[12]=0x00;//reserved to 0
}

BYTE SP_TX_BW_LC_Sel(struct VideoFormat* pInputFormat)
{
	BYTE over_bw;
	int pixel_clk;

	over_bw = 0;
	pixel_clk = pclk;

	if(pInputFormat->bColordepth != COLOR_8_BIT)
		return 1;


	//pr_info("pclk = %d\n",(unsigned int)pixel_clk);
	SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,0x01,1,ByteBuf);
	//sp_tx_bw = ByteBuf[0];
	switch (ByteBuf[0]) {
	case 0x06:
		sp_tx_bw = BW_162G;
		break;
	case 0x0a:
		sp_tx_bw = BW_27G;
		break;
	case 0x14:
		sp_tx_bw = BW_54G;
		break;
	default:
		sp_tx_bw = BW_54G;
		break;
	}
	//sp_tx_lane_count = ByteBuf[1] & 0x0f;


	if(pixel_clk <= 54) {
		sp_tx_bw = BW_162G;
		sp_tx_lane_count = 0x01;
	} else if((54 < pixel_clk) && (pixel_clk <= 90)) {
		if(sp_tx_bw >= BW_27G) {
			sp_tx_bw = BW_27G;
			sp_tx_lane_count = 0x01;
		} else {
			over_bw = 1;
		}
	} else if((90 < pixel_clk) && (pixel_clk <= 180)) {
		if(sp_tx_bw >= BW_54G) {
			sp_tx_bw = BW_54G;
			sp_tx_lane_count = 0x01;
		} else {
			over_bw = 1;
		}
	} else {
		over_bw = 1;
	}
	
	if(over_bw)
	    pr_err("over bw!\n");
	 else
	 
	pr_err("The optimized BW =%.2x, Lane cnt=%.2x\n",(unsigned int)sp_tx_bw,(unsigned int)sp_tx_lane_count);
	
	return over_bw;

}

#if(AUTO_TEST_CTS)
unsigned int sp_tx_link_err_check(void)
{
	unsigned int errl = 0, errh = 0;
	BYTE bytebuf[2];
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x10, 2, bytebuf);
	msleep(5);
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x10, 2, bytebuf);
	errh = bytebuf[1];

	if (errh & 0x80) {
		errl = bytebuf[0];
		errh = (errh & 0x7f) << 8;
		errl = errh + errl;
	}

	pr_info(" Err of Lane = %d\n", errl);
	return errl;
}
#endif
BYTE sp_tx_lt_pre_config(void)
{
	BYTE legel_bw,legel_lc,c;
	legel_bw = legel_lc = 1;

	SP_TX_Get_Rx_BW(1,&c);
	switch(c) {
	case 0x06:
		sp_tx_bw=BW_162G;
		break;
	case 0x0a:
		sp_tx_bw=BW_27G;
		break;
	case 0x14:
		sp_tx_bw=BW_54G;
		break;
	default:
		sp_tx_bw=BW_54G;
		break;
	}

	// if(sp_tx_lane_count>2)//ANX7805 supports 2 lanes max.
	SP_TX_Lanes_PWR_Ctrl(CH1_BLOCK, 1);

#if(AUTO_TEST_CTS)
	if(sp_tx_test_lt) {
		//sp_tx_test_lt = 0;
		//sp_tx_bw = sp_tx_test_bw;
		switch (sp_tx_test_bw) {
		case 0x06:
			sp_tx_bw = BW_162G;
			break;
		case 0x0a:
			sp_tx_bw = BW_27G;
			break;
		case 0x14:
			sp_tx_bw = BW_54G;
			break;
		default:
			sp_tx_bw = BW_54G;
			break;
		}
		
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f);
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
	} else {
		struct VideoFormat InputFormattemp;
		InputFormattemp.bColordepth = COLOR_8_BIT;
		if (SP_TX_BW_LC_Sel(&InputFormattemp)) {
#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x05);
#else
			pr_info("****Over bandwidth****");
#endif
			return 1;
		} else
			pr_info("pclk: %.2x, The optimized BW =%.2x, Lane cnt=%.2x \n",
			        (unsigned int)pclk, (unsigned int)sp_tx_bw, (unsigned int)sp_tx_lane_count);
	}
#endif

	/*Diable video before link training to enable idle pattern*/
	SP_TX_Enable_Audio_Output(0);	//Disable audio  output
	SP_TX_Disable_Audio_Input();  //Disable audio input
	sp_tx_enable_video_input(0);//Disable video input
	
	if (!sp_tx_test_lt) {
		if(SSC_EN)
			SP_TX_CONFIG_SSC(sp_tx_bw);
		else
			SP_TX_SPREAD_Enable(0);
	}
	
	//set bandwidth
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, sp_tx_bw);
	//set lane conut
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LANE_COUNT_SET_REG, sp_tx_lane_count);

	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_ANALOG_PD_REG, 0xFF, CH0_PD);
	msleep(2);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_ANALOG_PD_REG, ~CH0_PD, 0x00);

	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PLL_CTRL_REG, 0xFF, PLL_RST);
	msleep(2);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PLL_CTRL_REG, ~PLL_RST, 0x00);
	return 0;
}

BYTE SP_TX_HW_Link_Training (void)
{

	BYTE c, return_value = 1;
	switch(sp_tx_link_training_state) {

	case LINK_TRAINING_INIT:
		sp_tx_link_training_state = LINK_TRAINING_PRE_CONFIG;
		break;
	case LINK_TRAINING_PRE_CONFIG:
		if(sp_tx_lt_pre_config() == 0)
			sp_tx_link_training_state = LINK_TRAINING_START;
		break;

	case LINK_TRAINING_START:
		if(!SP_TX_Get_PLL_Lock_Status()) {
#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x07);
#else
			pr_info("PLL not lock!");
#endif
			break;
		}
		pr_info("Hardware link training");

		for(c = 0; c <= 0x0b; c ++)
       		SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,c,1,ByteBuf);
				
		SP_TX_EnhaceMode_Set();
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x06, 0x00, 0x01, &c);
		c |= 0x01;
		SP_TX_AUX_DPCDWrite_Bytes(0x00,0x06,0x00, 0x01, &c); //Set sink to D0 mode

		//sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, 0x09);//link training from 400mv3.5db for ANX7730 B0-ANX.Fei-20111011 // by wsl

		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_TRAINING_CTRL_REG, SP_TX_LINK_TRAINING_CTRL_EN);
		sp_tx_link_training_state = LINK_TRAINING_WAITTING_FINISH;

		break;

	case LINK_TRAINING_WAITTING_FINISH:
		/*here : waitting interrupt to change training state.*/
		break;
	case LINK_TRAINING_ERROR:
#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x08);
#else
		pr_info("LINK_TRAINING_ERROR! \r\n");
#endif
		sp_tx_link_training_state = LINK_TRAINING_INIT;
		break;
	case LINK_TRAINING_FINISH:
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x02, 1, ByteBuf);
		//pr_info(" ##DPCD 0x00202 = 0x%.2x##\n",(unsigned int)ByteBuf[0]);
		if(ByteBuf[0] != 0x07)
			sp_tx_link_training_state = LINK_TRAINING_ERROR;
		else {
#if(AUTO_TEST_CTS)
			/* if there is link error, adjust pre-emphsis to check error again.
				If there is no error,keep the setting, otherwise use 400mv0db */
			if(!sp_tx_test_lt) {
				c = 0x01;//DRVIE_CURRENT_LEVEL1;
				sp_write_reg(SP_TX_PORT0_ADDR, 0xA3, c);

				if (sp_tx_link_err_check()) {
					c = 0x08 | 0x01;
					sp_write_reg(SP_TX_PORT0_ADDR, 0xA3, c);

					if (sp_tx_link_err_check())
						sp_write_reg(SP_TX_PORT0_ADDR, 0xA3, 0x01);
				}

				sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);
				if (c != sp_tx_bw) {
					/*here can not replace using
					sp_tx_link_training_state = LINK_TRAINING_ERROR; */
					SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
					break;
				}
			}
#endif
			return_value = 0;
			SP_TX_MIPI_CONFIG_Flag_Set(0);
			sp_tx_link_training_state = LINK_TRAINING_END;

			SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO_OUTPUT);

		}
		break;
	default:
	case LINK_TRAINING_END:
		//when link training end, the state machine is keep here.
		break;
	}
	return return_value;

}
void SP_TX_Video_Mute(BYTE enable)
{
	BYTE c;

	pr_info("SP_TX_Video_Mute, enable:%d\n", enable);
	if(enable) {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c |=SP_TX_VID_CTRL1_VID_MUTE;
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG , c);
	} else {
		sp_read_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c &=~SP_TX_VID_CTRL1_VID_MUTE;
		sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG , c);
	}


}


void SP_TX_Config_MIPI_Video_Format()
{
	unsigned long  M_vid;
	//long float lTemp;
	unsigned long lBW;
	unsigned long  l_M_Vid;

	BYTE bIndex;
	unsigned int MIPI_Format_data;

	BYTE c,c1,c2;
	lBW = 0;
	//clear force stream valid flag
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c&0xfc);


	//Get BW
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);
	if(c==0x06) {
		pr_info("1.62G");
		lBW = 162;
	} else if(c==0x0a) {
		pr_info("2.7G");
		lBW = 270;
	} else if(c==0x14) {
		pr_info("5.4G");
		lBW = 540;
	} else {
		lBW = 540;
		pr_info("invalid BW");
	}
	bIndex = MIPI_Format_Index_Get();

	M_vid = (unsigned long)((mipi_video_timing_table[bIndex].MIPI_pixel_frequency)*100);

	c = (unsigned char)M_vid;
	c1 = (unsigned char)(M_vid>>8);
	c2 = (unsigned char)(M_vid>>16);

	//pr_info("m_vid h = %x,m_vid m = %x, mvid l= %x \n",(unsigned int)c2,(unsigned int)c1,(unsigned int)c);


	M_vid = M_vid*32768;

	c = (unsigned char)M_vid;
	c1 = (unsigned char)(M_vid>>8);
	c2 = (unsigned char)(M_vid>>16);

	//pr_info("m_vid h = %x,m_vid m = %x, mvid l= %x \n",(unsigned int)c2,(unsigned int)c1,(unsigned int)c);

	M_vid=M_vid/(lBW*100);

	//pr_info("m_vid = %x \n", M_vid);

	c = (unsigned char)M_vid;
	c1 = (unsigned char)(M_vid>>8);
	c2 = (unsigned char)(M_vid>>16);

	//pr_info("m_vid h = %x,m_vid m = %x, mvid l= %x \n",(unsigned int)c2,(unsigned int)c1,(unsigned int)c);

	//M_vid = ((mipi_video_timing_table[bIndex].MIPI_pixel_frequency)*32768)/lBW;

	//pr_info("m_vid = %x \n", M_vid);

	l_M_Vid =(unsigned long)M_vid;

	//pr_info("m_vid1 = %x \n", l_M_Vid);

	//Set M_vid
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x20, c);
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x21, c1);
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x22, c2);

	/*
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x20, 0x75);
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x21, 0x27);
	sp_write_reg(MIPI_RX_PORT1_ADDR, 0x22, 0x0);
	*/

	//Vtotal
	//MIPI_Format_data = (mipi_video_timing_table[bIndex].VTOTAL);
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_VTOTAL);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEH_REG, (MIPI_Format_data >> 8));

	//V active
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_VActive);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, (MIPI_Format_data >> 8));

	//V Front porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_V_Front_Porch);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, MIPI_Format_data);

	//V Sync width
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_V_Sync_Width);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, MIPI_Format_data);

	//V Back porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_V_Back_Porch);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, MIPI_Format_data);

	//H total
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_HTOTAL);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, (MIPI_Format_data >> 8));

	//H active
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_HActive);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, (MIPI_Format_data >> 8));

	//H Front porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_H_Front_Porch);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, (MIPI_Format_data >> 8));

	//H Sync width
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_H_Sync_Width);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, (MIPI_Format_data >> 8));

	//H Back porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_H_Back_Porch);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, MIPI_Format_data);
	sp_write_reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, (MIPI_Format_data >> 8));


	//force stream valid for MIPI
	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c|0x03);

	//force video format select from register
	sp_read_reg(SP_TX_PORT2_ADDR, 0x011, &c);
	sp_write_reg(SP_TX_PORT2_ADDR, 0x11, c|0x10);


}

void MIPI_Format_Index_Set(BYTE bFormatIndex)
{
	bMIPIFormatIndex = bFormatIndex;
	//pr_info("Set MIPI video format index to %d\n",(unsigned int)bMIPIFormatIndex);
}

BYTE MIPI_Format_Index_Get(void)
{
	pr_info("MIPI video format index is %d\n",(unsigned int)bMIPIFormatIndex);
	return bMIPIFormatIndex;
}

void SP_TX_MIPI_CONFIG_Flag_Set(BYTE bConfigured)
{
	bMIPI_Configured = bConfigured;
}
/*
BYTE MIPI_CheckSum_Status_OK(void)
{
	BYTE c;

	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, &c);
	//pr_info("protocol state = %.2x\n",(unsigned int)c);
	if(c&MIPI_CHECKSUM_ERR)
		return 0;
	else
		return 1;
}
*/




void SP_CTRL_Variable_Init(void)
{
#if(REDUCE_REPEAT_PRINT_INFO)
	BYTE i = 0;
#endif
	sp_tx_edid_err_code = 0;
	edid_pclk_out_of_range = 0;
	sp_tx_ds_edid_hdmi = 0;
	sp_tx_ds_edid_3d_present = 0;

	sp_tx_pd_mode = 1;//initial power state is power down.


	sp_tx_link_training_state = LINK_TRAINING_INIT;
	hdcp_process_state = HDCP_PROCESS_INIT;

	bEDIDBreak = 0;
	//VSDBaddr = 0x84;
	EDID_Print_Enable = 0;
	//Bist format index initial
	bBIST_FORMAT_INDEX = 1;//default 720P   BIST Clock
	Force_Video_Resolution = 1; // set bist resolution acoording to the timing table

	sp_tx_lane_count = 0x01;
	sp_tx_bw= BW_NULL;
	pclk = 0;

	sp_tx_rx_type = RX_NULL;
#if(AUTO_TEST_CTS)
	sp_tx_test_lt = 0;
	sp_tx_test_bw = 0;
	sp_tx_test_edid = 0;
#endif
#if(REDUCE_REPEAT_PRINT_INFO)
	maybe_repeat_print_info_flag = 0;
	for(i = 0; i < LOOP_PRINT_MSG_MAX; i++)
		repeat_printf_info_count[i] = 0;
#endif

	//CEC support index initial
	CEC_abort_message_received = 0;
	CEC_get_physical_adress_message_received = 0;
	CEC_logic_addr = 0x00;
	CEC_loop_number = 0;
	CEC_resent_flag = 0;

	MIPI_Format_Index_Set(1);

}


void SP_CTRL_Set_LVTTL_Interface(BYTE eBedSync, BYTE rDE, BYTE sYCMUX, BYTE sDDR,BYTE lEdge )
{
	/*
	eBedSync:    1_Embeded SYNC, 0_Separate SYNC
	rDE:             1_Regenerate DE, 0_Separate DE
	sYCMUX:       1_YCMUX, 0_Not YCMUX
	sDDR:           1_DDR,  0_SDR
	lEdge:           1_negedge, 0_posedge
	*/

	SP_TX_Video_Input.bLVTTL_HW_Interface.sEmbedded_Sync.Embedded_Sync = eBedSync;
	SP_TX_Video_Input.bLVTTL_HW_Interface.sEmbedded_Sync.Extend_Embedded_Sync_flag = 0;
	SP_TX_Video_Input.bLVTTL_HW_Interface.DE_reDenerate = rDE;
	SP_TX_Video_Input.bLVTTL_HW_Interface.sYC_MUX.YC_MUX = sYCMUX;
	SP_TX_Video_Input.bLVTTL_HW_Interface.sYC_MUX.YC_BIT_SEL = 1;
	SP_TX_Video_Input.bLVTTL_HW_Interface.DDR_Mode = sDDR;
	SP_TX_Video_Input.bLVTTL_HW_Interface.Clock_EDGE = lEdge;
}


void SP_CTRL_InputSet(VideoInterface Interface,ColorSpace bColorSpace, ColorDepth cBpc)
{
	SP_TX_Video_Input.Interface = Interface;

	SP_TX_Video_Input.bColordepth = cBpc;
	SP_TX_Video_Input.bColorSpace = bColorSpace;

}

void SP_CTRL_AUDIO_FORMAT_Set(AudioType cAudio_Type,AudioFs cAudio_Fs,AudioWdLen cAudio_Word_Len)
{
	SP_TX_Audio_Input.bAudioType = cAudio_Type;
	SP_TX_Audio_Input.bAudio_Fs = cAudio_Fs;
	SP_TX_Audio_Input.bAudio_word_len = cAudio_Word_Len;
}


void SP_CTRL_I2S_CONFIG_Set(I2SChNum cCh_Num, I2SLayOut cI2S_Layout)
{
	SP_TX_Audio_Input.bI2S_FORMAT.AUDIO_LAYOUT  = cI2S_Layout;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_Num     = cCh_Num;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status1 = 0x00;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status2 = 0x00;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status3 = 0x00;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status4 = SP_TX_Audio_Input.bAudio_Fs;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status5 = SP_TX_Audio_Input.bAudio_word_len;
	SP_TX_Audio_Input.bI2S_FORMAT.SHIFT_CTRL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.DIR_CTRL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.WS_POL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.JUST_CTRL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.EXT_VUCP = 0;
}

SP_TX_System_State get_system_state(void)
{
	return sp_tx_system_state;
}
/*
* FUNCTION: change_system_state_clean
* 		clear up pre system state machine register status.
*/
void change_system_state_clean(SP_TX_System_State cur_state)
{
	if(sp_tx_system_state >= SP_TX_HDCP_AUTHENTICATION
	   && cur_state <= SP_TX_HDCP_AUTHENTICATION) {
		SP_TX_Video_Mute(1);
		hdcp_encryption_enable(0);
		SP_CTRL_Clean_HDCP();
	} 
	if(sp_tx_system_state > SP_TX_CONFIG_VIDEO_INPUT
	          && cur_state <= SP_TX_CONFIG_VIDEO_INPUT) {
		sp_tx_enable_video_input(0);
		SP_TX_Disable_Audio_Input();
		SP_TX_Enable_Audio_Output(0);
	}
}
void SP_CTRL_Set_System_State(SP_TX_System_State ss)
{
	pr_info("SP_TX To System State: ");
	change_system_state_clean(ss);
	switch (ss) {
	case SP_TX_INITIAL:
		sp_tx_system_state = SP_TX_INITIAL;
		pr_info("SP_TX_INITIAL");
		break;
	case SP_TX_WAIT_SLIMPORT_PLUGIN:
		sp_tx_system_state = SP_TX_WAIT_SLIMPORT_PLUGIN;
		pr_info("SP_TX_WAIT_SLIMPORT_PLUGIN");
		break;
	case SP_TX_PARSE_EDID:
		sp_tx_system_state = SP_TX_PARSE_EDID;
		pr_info("SP_TX_READ_PARSE_EDID");
		break;
	case SP_TX_CONFIG_VIDEO_INPUT:
		sp_tx_system_state = SP_TX_CONFIG_VIDEO_INPUT;
		pr_info("SP_TX_CONFIG_VIDEO_INPUT");
		break;
	case SP_TX_CONFIG_AUDIO:
		sp_tx_system_state = SP_TX_CONFIG_AUDIO;
		pr_info("SP_TX_CONFIG_AUDIO");
		break;
	case SP_TX_LINK_TRAINING:
		sp_tx_system_state = SP_TX_LINK_TRAINING;
		sp_tx_link_training_state = LINK_TRAINING_INIT;
		pr_info("SP_TX_LINK_TRAINING");
		break;
	case SP_TX_CONFIG_VIDEO_OUTPUT:
		sp_tx_system_state = SP_TX_CONFIG_VIDEO_OUTPUT;
		break;
	case SP_TX_HDCP_AUTHENTICATION:
		hdcp_process_state = HDCP_PROCESS_INIT;
		sp_tx_system_state = SP_TX_HDCP_AUTHENTICATION;
		pr_info("SP_TX_HDCP_AUTHENTICATION");
		break;
	case SP_TX_PLAY_BACK:
		sp_tx_system_state = SP_TX_PLAY_BACK;
		pr_info("SP_TX_PLAY_BACK");
#if(UPDATE_ANX7730_SW)
		sp_write_reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0 , 0x85);
#endif
		break;
	default:
		pr_info("state error!\n");
		break;
	}

	//per_system_state = sp_tx_system_state;

}

//check downstream cable stauts ok-20110906-ANX.Fei
BYTE SP_CTRL_Check_Cable_Status(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c);
	if((c&0x28)==0x28)
		return 1; //RX OK

	return 0;

}

/*
static void sp_tx_send_message(enum SP_TX_SEND_MSG message)
{
	BYTE c;

	switch (message) {
	case MSG_OCM_EN:
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0x25, 0x5a);
		break;

	case MSG_INPUT_HDMI:
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0x26, 0x01);
		break;

	case MSG_INPUT_DVI:
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0x26, 0x00);
		break;

	case MSG_CLEAR_IRQ:
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x04, 0x10, 1, &c);
		c |= 0x01;
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x04, 0x10, c);
		break;
	}

}
*/
static BYTE sp_tx_get_cable_type(void)
{
	BYTE SINK_OUI[8] = { 0 };
	BYTE ds_port_preset = 0;
	BYTE ds_port_recoginze = 0;
	BYTE temp_value;
	int i,j;

	pr_err("sp_tx_get_cable_type ++.\n");

	for (i = 0; i < 5; i++) {
		if(AUX_ERR == SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x05, 1, &ds_port_preset)) {
			/*time delay for VGA dongle mcu startup*/
			msleep(50);
			pr_info(" AUX access error");
			SP_TX_RST_AUX();
			continue;
		}

		for( j =0; j < 0x0c; j++) {
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, j, 1, &temp_value);
			//pr_info(" DPCD 0x000%.2x = 0x%.2x\n", (uint)i, (uint)bytebuf[0]);
		}

		switch (ds_port_preset & 0x07) {
		case 0x00:
		case 0x01:
			sp_tx_rx_type = RX_DP;
			ds_port_recoginze = 1;
			pr_info("Downstream is DP dongle.\n");
			break;
		case 0x03:
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x04, 0x00, 8, SINK_OUI);
			if ((SINK_OUI[0] == 0x00) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0xb9) && (SINK_OUI[3] == 0x61)
			    && (SINK_OUI[4] == 0x39) && (SINK_OUI[5] == 0x38)
			    && (SINK_OUI[6] == 0x33)) {
				pr_info("Downstream is VGA dongle.\n");
				sp_tx_rx_type = RX_VGA;
			} else {
				sp_tx_rx_type = RX_DP;
				pr_info("Downstream is general DP2HDMI converter.\n");
			}
			ds_port_recoginze = 1;
			break;
		case 0x05:
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x04, 0x00, 8, SINK_OUI);
			if ((SINK_OUI[0] == 0xb9) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0x00) && (SINK_OUI[3] == 0x00)
			    && (SINK_OUI[4] == 0x00) && (SINK_OUI[5] == 0x00)
			    && (SINK_OUI[6] == 0x00)) {
				pr_info("Downstream is HDMI dongle.\n");
				//sp_tx_send_message(MSG_OCM_EN);
				sp_tx_rx_type = RX_HDMI;
			} else {
				sp_tx_rx_type = RX_DP;
				pr_info("Downstream is general DP2VGA converter.\n");
			}
			ds_port_recoginze = 1;
			break;
		default:
			msleep(100);
			pr_info("Downstream can not recognized.\n");
			sp_tx_rx_type = RX_NULL;
			ds_port_recoginze = 0;
			break;

		}

		if(ds_port_recoginze !=0)
		{
			pr_err("sp_tx_get_cable_type - done.\n");
			return ds_port_recoginze;
		}
	}

	return ds_port_recoginze;
}

BYTE sp_tx_get_hdmi_connection(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x05, 0x18, 1, &c);
	if ((c & 0x41) == 0x41) {
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0xf3, 0x70);
		return 1;
	} else
		return 0;
}
BYTE sp_tx_get_dp_connection(void)
{
	BYTE c;

	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, DPCD_SINK_COUNT, 1, &c);
	if (c & 0x1f) {
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x04, 1, &c);
		if (c & 0x20) {
			c = 0x20;
			SP_TX_AUX_DPCDWrite_Bytes(0x00, 0x06, 0x00, 1, &c);
		}
		return 1;
	} else
		return 0;
}
BYTE sp_tx_get_vga_connection(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, DPCD_SINK_COUNT, 1, &c);
	return ((c & 0x01) == 0x01) ? 1:0;
}
BYTE is_cable_detected(void)
{	
	return slimport_is_connected();
}
void SP_CTRL_Slimport_Plug_Process(void)
{
	/*BYTE c,i,j;*/
	if(is_cable_detected() == 1) {
		//pr_info("detected cable: %.2x \n",(unsigned int)is_cable_detected());
		if(sp_tx_pd_mode) {
			system_power_ctrl(1);
			msleep(5);
			if(sp_tx_get_cable_type() == 0) {
				pr_info("AUX ERR");
				system_power_ctrl(0);
				return;
			}
#if(UPDATE_ANX7730_SW)
			SP_TX_RST_AUX();
			if(update_anx7730_sw() != 0) {
				system_power_ctrl(0);
				return;
			}
#endif

		}
		switch (sp_tx_rx_type) {
		case RX_HDMI:
			if(sp_tx_get_hdmi_connection())
				SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
			break;
		case RX_DP:
			if (sp_tx_get_dp_connection())
				SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
			break;
		case RX_VGA:
			if(sp_tx_get_vga_connection()) {
				//sp_tx_send_message(MSG_CLEAR_IRQ);
				SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
			}
			break;
		case RX_NULL:
		default:
			break;
		}
		
	} else if(sp_tx_pd_mode == 0) {
		pr_info("cable not detected");
		system_power_ctrl(0);
	}

}

void SP_CTRL_Video_Changed_Int_Handler (BYTE changed_source)
{
	//pr_info("[INT]: SP_CTRL_Video_Changed_Int_Handler");
	if(sp_tx_system_state >= SP_TX_CONFIG_AUDIO) {
		switch(changed_source) {
		case 0:
			pr_info("Video:_______________Video clock changed!");
			
			//SP_TX_Power_Enable(SP_TX_PWR_LINK, SP_TX_POWER_DOWN);
			SP_TX_Lanes_PWR_Ctrl(CH0_BLOCK, 1);
			delay_ms(5);
			SP_TX_Lanes_PWR_Ctrl(CH0_BLOCK, 0);
			//SP_TX_Lanes_PWR_Ctrl(CH1_BLOCK, 1);
			SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO_INPUT);
			break;
		case 1:
			//pr_info("Video:_______________Video format changed!");
			//SP_TX_Disable_Video_Input();
			// SP_TX_Disable_Audio_Input();
			// SP_TX_Enable_Audio_Output(0);
			//SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
			break;
		default:
			break;
		}
	}
}

void SP_CTRL_PLL_Changed_Int_Handler(void)
{
	// pr_info("[INT]: SP_CTRL_PLL_Changed_Int_Handler");
	if (sp_tx_system_state > SP_TX_PARSE_EDID) {
		if(!SP_TX_Get_PLL_Lock_Status()) {
			pr_info("PLL:_______________PLL not lock!");
			//SP_CTRL_Clean_HDCP();
			SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
		}
	}
}
void SP_CTRL_AudioClk_Change_Int_Handler(void)
{
	// pr_info("[INT]: SP_CTRL_AudioClk_Change_Int_Handler");
	if (sp_tx_system_state >= SP_TX_CONFIG_AUDIO) {

		pr_info("Audio:_______________Audio clock changed!");
		SP_TX_Disable_Audio_Input();
		SP_TX_Enable_Audio_Output(0);
		//SP_CTRL_Clean_HDCP();
		SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
	}
}

void  SP_CTRL_Auth_Done_Int_Handler(void)
{
	BYTE temp_value[2] = {0};
	static BYTE auth_fail_counter = 0;
	if (sp_tx_system_state < SP_TX_HDCP_AUTHENTICATION)
		return;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_STATUS, temp_value);

	if (temp_value[0] & SP_TX_HDCP_AUTH_PASS) {
		SP_TX_AUX_DPCDRead_Bytes(0x06, 0x80, 0x2A, 2, temp_value);
		if (temp_value[1] & 0x08) {
			hdcp_process_state = HDCP_PROCESS_INIT;
			pr_info("Re-authentication 1!\n");
		} else {
			pr_info("Authentication pass in Auth_Done\n");
			hdcp_process_state = HDCP_FINISH;
			auth_fail_counter = 0;
		}
	} else {
		pr_info("Authentication failed in AUTH_done\n");
		auth_fail_counter++;
		if(auth_fail_counter >= SP_TX_HDCP_FAIL_THRESHOLD) {
			auth_fail_counter = 0;
			hdcp_process_state = HDCP_FAILE;
		} else {
			//hdcp_process_state = HDCP_PROCESS_INIT;
			hdcp_process_state = HDCP_FAILE;
			pr_info("Re-authentication 2!\n");
		}
	}
}

#if 0
void SP_CTRL_Auth_Change_Int_Handler(void)
{
	BYTE c;
	// pr_info("[INT]: SP_CTRL_Auth_Change_Int_Handler");
	SP_TX_Get_HDCP_status(&c);
	if(c & SP_TX_HDCP_AUTH_PASS) {
		sp_tx_hdcp_auth_pass = 1;
		pr_info("Authentication pass in Auth_Change");
	} else {
		pr_info("Authentication failed in Auth_change");
		sp_tx_hdcp_auth_pass = 0;
		SP_TX_Video_Mute(1);
		SP_TX_HDCP_Encryption_Disable();
		if(sp_tx_system_state > SP_TX_CONFIG_VIDEO) {
			SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
			SP_CTRL_Clean_HDCP();
		}
	}
}
#endif

/*added for B0 version-ANX.Fei-20110901-Start*/

// hardware linktraining finish interrupt handle process
void SP_CTRL_LT_DONE_Int_Handler(void)
{
	BYTE c;
	//pr_info("[INT]: SP_CTRL_LT_DONE_Int_Handler");
	if((sp_tx_link_training_state >= LINK_TRAINING_FINISH))
		return;

	sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_TRAINING_CTRL_REG, &c);
	if(c & 0x70) {
		c = (c & 0x70) >> 4;
		pr_info("HW LT failed in interrupt, ERR code = %.2x\n",(unsigned int)c);
		SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
		msleep(50);

	} else {
		sp_tx_link_training_state = LINK_TRAINING_FINISH;
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, &c);
		pr_info("HW LT succeed in interrupt ,LANE0_SET = %.2x\n",(unsigned int)c);
	}
}

//for mipi interrupt process
void SP_CTRL_MIPI_Htotal_Chg_Int_Handler(void)
{
#if 1
	if(SP_TX_Video_Input.Interface   == MIPI_DSI) {
		pr_info("mipi htotal changed");
		//SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
	}
#endif
}

void SP_CTRL_MIPI_Packet_Len_Chg_Int_Handler(void)
{
#if 1
	if(SP_TX_Video_Input.Interface   == MIPI_DSI) {
		pr_info("mipi packet length changed");

		//SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
	}
#endif
}

void SP_CTRL_MIPI_Lane_clk_Chg_Int_Handler(void)
{
#if 1
	if(SP_TX_Video_Input.Interface   == MIPI_DSI)
		pr_info("mipi lane clk changed");
#endif
}
void SP_CTRL_LINK_CHANGE_Int_Handler(void)
{

	BYTE lane0_1_status,sl_cr,al;

	//pr_info("[INT]: SP_CTRL_LINK_CHANGE_Int_Handler");
	if(sp_tx_system_state < SP_TX_CONFIG_VIDEO_OUTPUT)//(sp_tx_system_state < SP_TX_LINK_TRAINING )
		return;


	SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
	al = ByteBuf[0];


	SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
	lane0_1_status = ByteBuf[0];


	// pr_info("al = %x, lane0_1 = %x\n",(unsigned int)al,(unsigned int)lane0_1_status);

	if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
		sl_cr = 0;
	else
		sl_cr = 1;


	if(((al & 0x01) == 0) || (sl_cr == 0) ) { //align not done, CR not done
		if((al & 0x01)==0)
			pr_info("Lane align not done\n");
		if(sl_cr == 0)
			pr_info("Lane clock recovery not done\n");

		//re-link training only happen when link traing have done
		if((sp_tx_system_state > SP_TX_LINK_TRAINING )
		   &&sp_tx_link_training_state > LINK_TRAINING_FINISH) {
			SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
			pr_info("IRQ:____________re-LT request!");
		}
	}


}


// c-wire polling error interrupt handle process
void SP_CTRL_POLLING_ERR_Int_Handler(void)
{
	BYTE c;
	int i;

	//pr_info("[INT]: SP_CTRL_POLLING_ERR_Int_Handler\n");
	if((sp_tx_system_state < SP_TX_WAIT_SLIMPORT_PLUGIN)||sp_tx_pd_mode)
		return;

	for(i=0; i<5; i++) {
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,0x00,1,&c);
		if(c==0x11)
			return;
		msleep(100);//delay 1s for ANX9832 dongle
	}

	if(sp_tx_pd_mode ==0) {
		//pr_info("read dpcd 0x00000=%.2x\n",(unsigned int)c);
		pr_info("Cwire polling is corrupted,power down ANX7805.\n");
		system_power_ctrl(0);
	}
}

#if(AUTO_TEST_CTS)
void sp_tx_phy_auto_test(void)
{

	BYTE bSwing,bEmp;//for automated phy test
	BYTE c1;
	BYTE bytebuf[10];

	SP_TX_AUX_DPCDRead_Bytes(0x0, 0x02, 0x19, 1, bytebuf);
	switch(bytebuf[0]) { //DPCD 0x219 TEST_LINK_RATE
	case 0x06:

		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, 0x06);
		sp_tx_test_bw=0x06;
		pr_err("test BW= 1.62Gbps\n");
		break;
	case 0x0a:
		sp_write_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG,0x0a);
		sp_tx_test_bw=0x0a;
		pr_err("test BW= 2.7Gbps\n");
		break;
	case 0x14:
		sp_write_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG,0x014);
		sp_tx_test_bw=0x14;
		pr_err("test BW= 5.4Gbps\n");
		break;
	}

	SP_TX_AUX_DPCDRead_Bytes(0x0,0x02, 0x48, 1, bytebuf);
	switch(bytebuf[0]) { //DPCD 0x248 PHY_TEST_PATTERN
	case 0:
		pr_info("No test pattern selected\n");
		break;
	case 1:
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x04);
		pr_info("D10.2 Pattern\n");
		break;
	case 2:
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x08);
		pr_info("Symbol Error Measurement Count\n");
		break;
	case 3:
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x0c);
		pr_info("PRBS7 Pattern\n");
		break;
	case 4:
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x50, 0xa, bytebuf);

		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x80, bytebuf[0]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x81, bytebuf[1]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x82, bytebuf[2]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x83, bytebuf[3]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x84, bytebuf[4]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x85, bytebuf[5]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x86, bytebuf[6]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x87, bytebuf[7]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x88, bytebuf[8]);
		sp_write_reg(MIPI_RX_PORT1_ADDR, 0x89, bytebuf[9]);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x30);//disable scramble?
		pr_info("80bit custom pattern transmitted\n");
		break;
	case 5:
		sp_write_reg(SP_TX_PORT0_ADDR, 0xA9, 0xFC);
		sp_write_reg(SP_TX_PORT0_ADDR, 0xAA, 0x00);
		sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x14);
		pr_info("HBR2 Compliance Eye Pattern\n");
		break;
	}
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x03, 1, bytebuf);
	switch(bytebuf[0]&0x01) {
	case 0:
		SP_TX_SPREAD_Enable(0);
		pr_info("SSC OFF\n");
		break;
	case 1:
		sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c1);
		//sp_tx_bw = c1;
		switch (c1) {
		case 0x06:
			sp_tx_bw = BW_162G;
			break;
		case 0x0a:
			sp_tx_bw = BW_27G;
			break;
		case 0x14:
			sp_tx_bw = BW_54G;
			break;
		default:
			sp_tx_bw = BW_54G;
			break;
		}
				
		SP_TX_CONFIG_SSC(sp_tx_bw);
		pr_info("SSC ON\n");
		break;
	}

	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x06,1,bytebuf);
	c1 = bytebuf[0]&0x03;//get swing adjust request
	/*sp_read_reg(SP_TX_PORT0_ADDR, 0xA2, &c);
	if((c == 0x0c)&&((bytebuf[0]&0x0f)==0x02)) { //PRBS7 pattern, eye diagram, swing2/emp 0
		sp_write_reg(SP_TX_PORT0_ADDR, 0xA3, 0x0a);
		pr_info("eye request,lane0,400/6db\n");
	} else if (c == 0x14) {//cep pattern

		sp_write_reg(SP_TX_PORT0_ADDR, 0xA3, 0x0a);
		pr_info("cep pattern,lane0,600/3.5db\n");
	} else */{

		switch(c1) {
		case 0x00:
			sp_read_reg(0x70, 0xA3, &bSwing);
			sp_write_reg(0x70, 0xA3, (bSwing&~0x03)|0x00);
			pr_info("lane0,Swing200mv\n");
			break;
		case 0x01:
			sp_read_reg(0x70, 0xA3, &bSwing);
			sp_write_reg(0x70, 0xA3, (bSwing&~0x03)|0x01);
			pr_info("lane0,Swing400mv\n");
			break;
		case 0x02:
			sp_read_reg(0x70, 0xA3, &bSwing);
			sp_write_reg(0x70, 0xA3, (bSwing&~0x03)|0x02);
			pr_info("lane0,Swing600mv\n");
			break;
		case 0x03:
			sp_read_reg(0x70, 0xA3, &bSwing);
			sp_write_reg(0x70, 0xA3, (bSwing&~0x03)|0x03);
			pr_info("lane0,Swing800mv\n");
			break;
		default:
			break;
		}

		c1 = ( bytebuf[0]&0x0c);//get emphasis adjust request
		c1=c1>>2;
		switch(c1) {
		case 0x00:
			sp_read_reg(0x70, 0xA3, &bEmp);
			sp_write_reg(0x70, 0xA3, (bEmp&~0x18)|0x00);
			pr_info("lane0,emp 0db\n");
			break;
		case 0x01:
			sp_read_reg(0x70, 0xA3, &bEmp);
			sp_write_reg(0x70, 0xA3, (bEmp&~0x18)|0x08);
			pr_info("lane0,emp 3.5db\n");
			break;
		case 0x02:
			sp_read_reg(0x70, 0xA3, &bEmp);
			sp_write_reg(0x70, 0xA3, (bEmp&~0x18)|0x10);
			pr_info("lane0,emp 6db\n");
			break;

		default:
			break;
		}
	}

}
#endif
/*added for B0 version-ANX.Fei-20110901-End*/
void SP_CTRL_IRQ_ISP(void)
{
	BYTE c,lane0_1_status,sl_cr,al,need_return = 0, temp_value;
	BYTE IRQ_Vector,Int_vector1,Int_vector2,Int_vector3;
  	BYTE cRetryCount;

	//SP_TX_RST_AUX();

	SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_DEVICE_SERVICE_IRQ_VECTOR,4,ByteBuf);
	IRQ_Vector = ByteBuf[0];
	debug_printf("IRQ_VECTOR = %.2x\n", (unsigned int)IRQ_Vector);
	Int_vector3 = ByteBuf[3];
	SP_TX_AUX_DPCDWrite_Bytes(0x00, 0x02, DPCD_DEVICE_SERVICE_IRQ_VECTOR,1, ByteBuf);//write clear IRQ


	if(IRQ_Vector & 0x04) { //HDCP IRQ
		if(hdcp_process_state == HDCP_FINISH) {
			SP_TX_AUX_DPCDRead_Bytes(0x06,0x80,0x29,1,&c);
			//pr_info("Bstatus = %.2x\n", (unsigned int)c1);
			if(c & 0x04) {
				if(sp_tx_system_state > SP_TX_HDCP_AUTHENTICATION) {
					SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
					pr_info("IRQ:____________HDCP Sync lost!");
				}
			}
		}
	}

	if((IRQ_Vector & 0x40)&&(sp_tx_rx_type == RX_HDMI)) { //specific int

		// pr_info("Rx specific interrupt IRQ!\n");

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_1,1,&Int_vector1);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_1,Int_vector1);
		//pr_info("DPCD00510 = 0x%.2x!\n",(unsigned int)Int_vector1);

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_2,1,&Int_vector2);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_2,Int_vector2);
		//pr_info("DPCD00511 = 0x%.2x!\n",(unsigned int)Int_vector2);

		temp_value = 0x01;
		do {
			switch( Int_vector1 & temp_value) {
			default:
				break;
			case 0x01:
				//check downstream HDMI hotplug status plugin
				SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c) ;
				if((c&0x01)==0x01)
					pr_info("Downstream HDMI is pluged!\n");
				break;
			case 0x02:
				//check downstream HDMI hotplug status unplug
				SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c);
				if((c&0x01)!=0x01) {
					pr_info("Downstream HDMI is unpluged!\n");
					if((sp_tx_system_state > SP_TX_WAIT_SLIMPORT_PLUGIN)
					   && (!sp_tx_pd_mode)) {
						system_power_ctrl(0);
						need_return = 1;
					}
				}
				break;
			case 0x04:
				if(sp_tx_system_state < SP_TX_CONFIG_AUDIO)
					break;

				pr_info("Rx specific  IRQ: Link is down!\n");
				SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
				al = ByteBuf[0];

				SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
				lane0_1_status = ByteBuf[0];

				if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
					sl_cr = 0;
				else
					sl_cr = 1;

				if(((al & 0x01) == 0) || (sl_cr == 0) ) { //align not done, CR not done
					if((al & 0x01)==0)
						pr_info("Lane align not done\n");
					if(sl_cr == 0)
						pr_info("Lane clock recovery not done\n");

					//re-link training only happen when link traing have done
					if((sp_tx_system_state > SP_TX_LINK_TRAINING )
					   && (sp_tx_link_training_state > LINK_TRAINING_PRE_CONFIG) ) {
						SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
						pr_info("IRQ:____________re-LT request!");
					}
				}
				break;
			case 0x08:
				pr_info("Downstream HDCP is done!\n");
				if((Int_vector1&0x10) !=0x10)
					pr_info("Downstream HDCP is passed!\n");
				else {
					if(sp_tx_system_state > SP_TX_CONFIG_VIDEO_OUTPUT ) {
						SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
						pr_info("Re-authentication due to downstream HDCP failure!");
					}
				}
				break;
			case 0x10:
				pr_info("Downstream HDCP is fail! \n");
				break;
			case 0x20:
				pr_info(" Downstream HDCP link integrity check fail!");
				//add for hdcp fail
				if(sp_tx_system_state > SP_TX_HDCP_AUTHENTICATION) {
					SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
					pr_info("IRQ:____________HDCP Sync lost!\n");
				}
				break;
			case 0x40:
				pr_info("Receive CEC command from upstream done!\n");
				break;
			case 0x80:
				pr_info("CEC command transfer to downstream done!\n");
				break;
			}
			if(need_return == 1)
				return;
			temp_value = (temp_value << 1);

		} while(temp_value != 0);

		/*check downstream HDMI Rx sense status -20110906-ANX.Fei*/
		if((Int_vector2&0x04)==0x04) {
			SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c);
			if((c&0x40)==0x40) {
				pr_info("Downstream HDMI termination is detected!\n");
			}
		}

	}
	else  if(((IRQ_Vector & 0x40) && (sp_tx_rx_type == RX_DP))||((Int_vector3&0x40)&& (sp_tx_rx_type == RX_DP))) { //specific int

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
		al = ByteBuf[0];
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
		lane0_1_status = ByteBuf[0];

		if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
			sl_cr = 0;
		else
			sl_cr = 1;

     		if((al&0x40)==0x40)// added for link cts item 4.2.2.8,sink status change
	    	{
		    //re-link training only happen when link traing have done		  
	            if(sp_tx_system_state >= SP_TX_PARSE_EDID )
	            {
	                SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);

	            }
	    
	   	}
			
		if(((al & 0x01) == 0) || (sl_cr == 0) ) { //align not done, CR not done
			if((al & 0x01)==0)
				pr_info("Lane align not done\n");
			if(sl_cr == 0)
				pr_info("Lane clock recovery not done\n");

			//re-link training only happen when link traing have done
			if((sp_tx_system_state > SP_TX_LINK_TRAINING )
			   &&sp_tx_link_training_state > LINK_TRAINING_PRE_CONFIG) {
				SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
				pr_info("IRQ:____________re-LT request!");
			}
		}

	}
	else  if((IRQ_Vector & 0x40)&&(sp_tx_rx_type == RX_VGA)) { //specific int
		//check sink count for general monitor
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x00,1,&c);
		if( (c & 0x01) == 0x00) {
			if((sp_tx_system_state > SP_TX_WAIT_SLIMPORT_PLUGIN )
			   && (sp_tx_pd_mode == 0)) {
				SP_TX_Power_Enable(SP_TX_PWR_TOTAL, SP_TX_POWER_DOWN);
				SP_TX_Power_Enable(SP_TX_PWR_REG, SP_TX_POWER_DOWN);
				SP_TX_Hardware_PowerDown();
				SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);
				sp_tx_pd_mode = 1;
			}
		}

		//Indicate the Rx to clear the specific irq
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x04,0x10,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x04,0x10,(c|0x01));

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
		al = ByteBuf[0];
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
		lane0_1_status = ByteBuf[0];

		if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
			sl_cr = 0;
		else
			sl_cr = 1;

		if(((al & 0x01) == 0) || (sl_cr == 0) ) { //align not done, CR not done
			if((al & 0x01)==0)
				pr_info("Lane align not done\n");
			if(sl_cr == 0)
				pr_info("Lane clock recovery not done\n");

			//re-link training only happen when link traing have done
			if((sp_tx_system_state > SP_TX_LINK_TRAINING )
			   &&sp_tx_link_training_state > LINK_TRAINING_PRE_CONFIG) {
				SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
				pr_info("IRQ:____________re-LT request!");
			}
		}

	}
#if(AUTO_TEST_CTS)
	/* AUTOMATED TEST IRQ */
	if (IRQ_Vector & 0x02) {
		BYTE bytebuf[1]= {0};
		BYTE test_vector = 0;

		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x18, 1, bytebuf);
		test_vector = bytebuf[0];
		if(test_vector & 0x01) { //test link training
			sp_tx_test_lt = 1;

			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x19,1,bytebuf);
			sp_tx_test_bw = bytebuf[0];
			pr_info(" test_bw = %.2x\n", (unsigned int)sp_tx_test_bw);

			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x60,1,bytebuf);
			bytebuf[0] = bytebuf[0] | TEST_ACK;
			SP_TX_AUX_DPCDWrite_Bytes(0x00, 0x02, 0x60,1, bytebuf);

			pr_info("Set TEST_ACK!\n");
			if (sp_tx_system_state >= SP_TX_LINK_TRAINING)
				SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
			pr_info("IRQ:test-LT request!\n");


		}


		if(test_vector & 0x04) { //test edid
			if (sp_tx_system_state > SP_TX_PARSE_EDID)
				SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
			sp_tx_test_edid = 1;
			pr_info("Test EDID Requested!\n");

		}

		if(test_vector & 0x02)//test pattern
		        {
		            SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x60, 1,bytebuf);
		            //debug_printf("respone = %.2x\n", (unsigned int)c);
		            bytebuf[0]=bytebuf[0] | 0x01;
		            SP_TX_AUX_DPCDWrite_Bytes(0x00,0x02,0x60,1, bytebuf);
		            SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x60, 1,bytebuf);
		 

		            cRetryCount = 0;
		            while((bytebuf[0] & 0x03) == 0)
		            {
		                cRetryCount++;
				  bytebuf[0]=bytebuf[0] | 0x01;
		            	  SP_TX_AUX_DPCDWrite_Bytes(0x00,0x02,0x60,1, bytebuf);
		            	  SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x60, 1,bytebuf);
		                if(cRetryCount>10)
		                {
		                    debug_puts("Fail to write ack!");
		                    break;
		                }
		            }

		            debug_printf("respone = %.2x\n", (unsigned int)c);
		        }

  
		if(test_vector & 0x08) { //phy test pattern
			sp_tx_test_lt = 1;
			sp_tx_phy_auto_test();

			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x60,1,bytebuf);
			bytebuf[0] = bytebuf[0] | 0x01;
			SP_TX_AUX_DPCDWrite_Bytes(0x00, 0x02, 0x60, 1,bytebuf);
			/*
						sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60,1,bytebuf);
						while((bytebuf[0] & 0x03) == 0){
							bytebuf[0] = bytebuf[0] | 0x01;
							sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, bytebuf);
							sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60,1,bytebuf);
						}*/
		}

	}
#endif
}

// downstream DPCD IRQ request interrupt handle process
void SP_CTRL_SINK_IRQ_Int_Handler(void)
{
	//pr_info("[INT]: SP_CTRL_SINK_IRQ_Int_Handler\n");
	SP_CTRL_IRQ_ISP();
}

void SP_CTRL_Clean_HDCP(void)
{
	// pr_info("HDCP Clean!");
	SP_TX_Clean_HDCP();
}

void SP_CTRL_EDID_Read(void)//add adress only cmd before every I2C over AUX access.-fei
{
	BYTE i,j,test_vector,edid_block = 0,segment = 0,offset = 0;

	SP_TX_EDID_Read_Initial();

	checksum = 0;
	sp_tx_ds_edid_hdmi =0;

	//VSDBaddr = 0x84;
	bEDIDBreak = 0;
	//Set the address only bit
	//SP_TX_AddrOnly_Set(1);
	//set I2C write com 0x04 mot = 1
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	//enable aux
	sp_write_reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x0b);
	SP_TX_Wait_AUX_Finished();
	SP_TX_AddrOnly_Set(0);
	
	edid_block = SP_TX_Get_EDID_Block();
	if(edid_block < 2) {

		edid_block = 8 * (edid_block + 1);

		for(i = 0; i < edid_block; i ++) {
			if(!bEDIDBreak)
				SP_TX_AUX_EDIDRead_Byte(i * 16);
			msleep(10);
		}

		//clear the address only bit
		SP_TX_AddrOnly_Set(0);

	} else {

		for(i = 0; i < 16; i ++) {
			if(!bEDIDBreak)
				SP_TX_AUX_EDIDRead_Byte(i * 16);
		}

		//clear the address only bit
		SP_TX_AddrOnly_Set(0);
		if(!bEDIDBreak) {
			edid_block = (edid_block + 1);
			for(i=0; i<((edid_block-1)/2); i++) { //for the extern 256bytes EDID block
				//pr_info("EXT 256 EDID block");
				segment = i + 1;
				for(j = 0; j<8 * (edid_block - 2); j++) {
					if(!bEDIDBreak)
						SP_TX_Parse_Segments_EDID(segment, offset);
					//msleep(1);
					offset = offset + 0x10;
				}
			}

		}


	}


	//clear the address only bit
	SP_TX_AddrOnly_Set(0);


	SP_TX_RST_AUX();
#if 0
	if(sp_tx_ds_edid_hdmi)
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,0x26, 0x01);//inform ANX7730 the downstream is HDMI
	else
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,0x26, 0x00);//inform ANX7730 the downstream is not HDMI
#endif

	SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x18,1,ByteBuf);


	test_vector = ByteBuf[0];

	if(test_vector & 0x04) { //test edid
		// pr_info("check sum = %.2x\n", (unsigned int)checksum);

		{
			SP_TX_AUX_DPCDWrite_Byte(0x00,0x02,0x61,checksum);
			SP_TX_AUX_DPCDWrite_Byte(0x00,0x02,0x60,0x04);
		}
		pr_info("Test read EDID finished");
	}
}

void SP_CTRL_EDID_Process(void)
{
	BYTE i,c;
	bEDIDBreak = 0;

	//read DPCD 00000-0000b
	for(i = 0; i <= 0x0b; i ++)
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,i,1,&c);
	
        SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,00,1,&c);//read  sink_count for link_cts 4.2.2.7

	SP_CTRL_EDID_Read();
	SP_TX_RST_AUX();
	if(bEDIDBreak)
		pr_info("ERR:EDID corruption!\n");
	SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO_INPUT);
}

#if(ANX7730_VIDEO_STB)
BYTE sp_tx_get_ds_video_status(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x05, 0x27, 1, &c);
	//pr_info("0x00527 = %.2x.\n", (unsigned int)c);
	return ((c & 0x01) != 0 ? 1 : 0);
}
#endif

static unsigned int dump_count=0;
void SP_CTRL_HDCP_Process(void)
{
	BYTE c;
	static BYTE ds_vid_stb_cntr = 0;
	#ifdef Redo_HDCP
	static BYTE HDCP_fail_count = 0;
	#endif
	dump_count = 0;
	//pr_info("HDCP Process state: %x \r\n", (unsigned int)hdcp_process_state);
	switch(hdcp_process_state) {
	case HDCP_PROCESS_INIT:
		hdcp_process_state = HDCP_CAPABLE_CHECK;
		break;
	case HDCP_CAPABLE_CHECK:
		SP_TX_AUX_DPCDRead_Bytes(0x06, 0x80, 0x28,1,&c);
		if((c & 0x01) == 0)
			hdcp_process_state = HDCP_NOT_SUPPORT;
		else
			hdcp_process_state = HDCP_WAITTING_VID_STB;
		break;
	case HDCP_WAITTING_VID_STB:
#if(ANX7730_VIDEO_STB)
		/*In case ANX7730 can not get ready video*/
		if(sp_tx_rx_type == RX_HDMI) {
			//pr_info("video stb : count%.2x \n",(unsigned int)ds_vid_stb_cntr);
			if (!sp_tx_get_ds_video_status()) {
				if (ds_vid_stb_cntr >= SP_TX_DS_VID_STB_TH) {
					system_power_ctrl(0);
					ds_vid_stb_cntr = 0;
				} else {
					ds_vid_stb_cntr++;
					msleep(100);
				}
				break;
			} else {
				ds_vid_stb_cntr = 0;
				hdcp_process_state = HDCP_HW_ENABLE;
			}
		}
		else
		{
			hdcp_process_state = HDCP_HW_ENABLE;
		}
#endif
		break;
	case HDCP_HW_ENABLE:
		SP_TX_Power_Enable(SP_TX_PWR_HDCP, SP_TX_POWER_ON);// Poer on HDCP link clock domain logic for B0 version-20110913-ANX.Fei
		msleep(50);
		SP_TX_HW_HDCP_Enable();
		hdcp_process_state = HDCP_WAITTING_FINISH;
		break;
	case HDCP_WAITTING_FINISH:
		break;
	case HDCP_FINISH:
		if (mute_count <= 0) {
			hdcp_encryption_enable(1);
			SP_TX_Video_Mute(0);
			HDCP_fail_count = 0;
			pr_info("@@@@@@@@@@@@@@@@@@@@@@@hdcp_auth_pass@@@@@@@@@@@@@@@@@@@@\n");
			//SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
			SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
			SP_TX_Show_Infomation();
		} else {
			pr_info("authentication, mute_count > 0, mute_count:%d\n", mute_count);
			mute_count--;
		}
		break;
	case HDCP_FAILE:
		#ifdef Redo_HDCP
		SP_CTRL_Clean_HDCP();
		HDCP_fail_count++;
		hdcp_process_state = HDCP_WAITTING_VID_STB;
		pr_info("***************************hdcp_auth_failed*********************************\n");
		
		if(HDCP_fail_count >= 5) {
			system_power_ctrl(0);
			hdcp_process_state = HDCP_PROCESS_INIT;
			HDCP_fail_count = 0;
			pr_info("***************************hdcp_auth_failed more than 5 times*******************************\n");

		}
		#else
		hdcp_encryption_enable(0);
		SP_TX_Video_Mute(1);
		pr_info("***************************hdcp_auth_failed*********************************\n");
		//SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
		SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
		SP_TX_Show_Infomation();
		#endif
		break;
	default:
	case HDCP_NOT_SUPPORT:
		pr_info("Sink is not capable HDCP");

		#ifdef Display_NoHDCP
		if (mute_count <= 0) {
			SP_TX_Power_Enable(SP_TX_PWR_HDCP, SP_TX_POWER_DOWN);
			SP_TX_Video_Mute(0);
			//SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
			SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
		} else {
			pr_info("mute_count > 0, mute_count:%d\n", mute_count);
			mute_count--;
		}
		#else
		SP_TX_Video_Mute(1);//when Rx does not support HDCP, force to send blue screen
		//SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
		SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
		#endif
		break;
	}
}

void SP_CTRL_Dump_Reg(void)
{
	unsigned int BEGIN=0x70, END=0xFF, i=0;
	uint8_t temp=0, D3=0, D4=0, D5=0, D6=0, D7=0, D8=0, c=0;
	unsigned int Maud=0, Naud=0;

	//for ANX7418 Debug
	sp_read_reg(0x50, 0x36, &temp);
	pr_err("ANX7418 reg:0x50,offset:0x36, temp:0x%x\n", temp);
	if (temp & 0x10)
		pr_err("ANX7418 bit4 HPD is High\n");
	else
		pr_err("ANX7418 bit4 HPD is Low\n");

	for(i=BEGIN; i<=END; i++) {
		sp_read_reg(SP_TX_PORT0_ADDR, i, &temp);
		pr_err("reg:%x,offset:%x, temp:%d\n", SP_TX_PORT0_ADDR, i, temp);
	}
	for(i=BEGIN; i<=END; i++) {
		sp_read_reg(SP_TX_PORT2_ADDR, i, &temp);
		pr_err("reg:%x,offset:%x, temp:%d\n", SP_TX_PORT2_ADDR, i, temp);
	}

	sp_read_reg(SP_TX_PORT0_ADDR, 0xD3, &D3);
	sp_read_reg(SP_TX_PORT0_ADDR, 0xD4, &D4);
	sp_read_reg(SP_TX_PORT0_ADDR, 0xD5, &D5);
	Maud = D5 << 16 | D4 << 8 | D3;

	sp_read_reg(SP_TX_PORT0_ADDR, 0xD6, &D6);
	sp_read_reg(SP_TX_PORT0_ADDR, 0xD7, &D7);
	sp_read_reg(SP_TX_PORT0_ADDR, 0xD8, &D8);
	Naud = D8 << 16 | D7 << 8 | D6;

	sp_read_reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
	if(c&0x10) {
		sp_read_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG, &c);
		if(c==0x06) {
			pr_err("   BW = 1.62G");
			SP_TX_PCLK_Calc(BW_162G);//str_clk = 162;
		} else if(c==0x0a) {
			pr_err("   BW = 2.7G");
			SP_TX_PCLK_Calc(BW_27G);//str_clk = 270;
		} else if(c==0x14) {
			pr_err("   BW = 5.4G");
			SP_TX_PCLK_Calc(BW_54G);//str_clk = 540;
		}
	} else {
		sp_read_reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG, &c);
		if(c==0x06) {
			pr_err("   BW = 1.62G");
		} else if(c==0x0a) {
			pr_err("   BW = 2.7G");
		} else if(c==0x14) {
			pr_err("   BW = 5.4G");
		}
	}

	/*freqency = (Maud / Naud) * (540000 / 512);*/
	pr_err("Maud:%d, Naud:%d, (Maud / Naud * 540 000 / 512) KHz\n", Maud, Naud);
}

void SP_CTRL_PlayBack_Process(void)
{
	//BYTE c;

#if 0
	//for MIPI video change
	if(SP_TX_Video_Input.Interface  == MIPI_DSI) {
		//polling checksum error
		if(!MIPI_CheckSum_Status_OK()) {
			pr_info("mipi checksum error!");
			SP_TX_MIPI_CONFIG_Flag_Set(0);
			SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
		}
		//else
		//pr_info("mipi checksum ok!");
	}
#endif

	/*
	if (dump_count < 2) {
		dump_count++;
		SP_CTRL_Dump_Reg();
	}
	*/

if(audio_format_change)
{
	SP_TX_Enable_Audio_Output(0);
	SP_TX_Power_Enable(SP_TX_PWR_AUDIO, SP_TX_POWER_DOWN);
	/*	sp_write_reg(SP_TX_PORT2_ADDR, 0x06, 18);
		delay_ms(2);
		sp_write_reg(SP_TX_PORT2_ADDR, 0x06, 0);
		delay_ms(2);
	*/
	SP_TX_Update_Audio(&SP_TX_Audio_Input);
	audio_format_change=0;

}
if(video_format_change)
{
	mute_count = 45;
	slimport_config_video_output();
	video_format_change=0;
}
}


void sp_ctrl_hpd_int_handler(BYTE hpd_source)
{
	BYTE c;
    //BYTE c,c1,sl_cr,test_irq,test_lt,al,lane0_1_status,lane2_3_status;
    //BYTE IRQ_Vector, test_vector;

	
    if(sp_tx_system_state == SP_TX_INITIAL)
        return;
    else
    {
        switch(hpd_source)
        {
            case 0://IRQ
                SP_CTRL_IRQ_ISP();
                break;
            case 1://HPD change
                debug_puts("HPD:____________HPD changed!");
 
               sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);

                if(c & SP_TX_SYS_CTRL3_HPD_STATUS)
                {                    

				if (sp_tx_get_cable_type() == 0)
				{
					debug_puts("AUX ERR");
					system_power_ctrl(0);
					return;
				}

				switch (sp_tx_rx_type) {
				case RX_HDMI:
					if (sp_tx_get_hdmi_connection())
						SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
					break;
				case RX_DP:
					if (sp_tx_get_dp_connection())
						SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
					break;
				case RX_VGA:
					if (sp_tx_get_vga_connection()) {
						//sp_tx_send_message(MSG_CLEAR_IRQ);
						SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
					}
					break;
				case RX_NULL:
				default:
					break;
				}			
                }
                break;
            case 2:
                delay_ms(2);

               	SP_TX_Get_Int_status(SP_INT_STATUS,&c);
                //debug_printf("2c = %x\n",(unsigned int)c);

                if(c & SP_TX_INT_STATUS1_HPD)//HPD detected
                     return;
                else
                {

                     sp_read_reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);

                    if(c & SP_TX_SYS_CTRL3_HPD_STATUS)
                        return;
                    else
                    {
                        debug_puts("HPD:____________HPD_Lost issued!");
			  // system_power_ctrl(0);
                        SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void SP_CTRL_Int_Process(void)
{
	BYTE c1,c2,c3,c4,c5;//,b;
	#ifndef Standard_DP
	if(sp_tx_pd_mode )//When chip is power down, do not care the int.-ANX.Fei-20111020
		return;

	if(sp_tx_rx_type == RX_HDMI) {
		if(!SP_CTRL_Check_Cable_Status()) { //wait for downstream cable stauts ok-20110906-ANX.Fei
			//RX not ready, check DPCD polling is still available
			SP_CTRL_POLLING_ERR_Int_Handler();
			return;
		}
	}
	#endif
	
  if(sp_tx_system_state == SP_TX_WAIT_SLIMPORT_PLUGIN)
    	{
		if (is_cable_detected() == 1)
		{
       		/* debug_printf("detected cable: %.2x \n",(unsigned int)is_cable_detected()); */
       		if (sp_tx_pd_mode)
       		{
       			system_power_ctrl(1);
       			delay_ms(5);	
       		}	
				 sp_ctrl_hpd_int_handler(1);
       	}
       	else if (sp_tx_pd_mode == 0)
       	{
       		debug_puts("cable not detected");
       		system_power_ctrl(0);
       	}

	}
	else	
	{
		SP_TX_Get_Int_status(COMMON_INT_1,&c1);
		SP_TX_Get_Int_status(COMMON_INT_2,&c2);
		SP_TX_Get_Int_status(COMMON_INT_3,&c3);
		SP_TX_Get_Int_status(COMMON_INT_4,&c4);
		SP_TX_Get_Int_status(SP_INT_STATUS,&c5);


		 if(c4 & SP_COMMON_INT4_PLUG)//PLUG, hpd changed
	        {
	             sp_ctrl_hpd_int_handler(1);
	        }
	        else if((c4 & SP_COMMON_INT4_HPD_CHANGE)&&(!(c4 & SP_COMMON_INT4_PLUG))&&(!(c4 & SP_COMMON_INT4_HPDLOST)))//IRQ, HPD
	        {
	             debug_puts("IRQ!!!!!!!!!!!!!"); 
	            sp_ctrl_hpd_int_handler(0);
	        }
	       else if(c4 & SP_COMMON_INT4_HPDLOST)//cable lost
	        {
	            debug_puts("HPD lost\n");
	            sp_ctrl_hpd_int_handler(2);
		  return;
	        }

	if(c1 & SP_COMMON_INT1_VIDEO_CLOCK_CHG)//video clock change
		SP_CTRL_Video_Changed_Int_Handler(0);

	if(sp_tx_pd_mode )
		return;

	if(c1 & SP_COMMON_INT1_PLL_LOCK_CHG)//pll lock change
		SP_CTRL_PLL_Changed_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	if(c1 & SP_COMMON_INT1_PLL_LOCK_CHG)//audio clock change
		SP_CTRL_AudioClk_Change_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	if(c2 & SP_COMMON_INT2_AUTHCHG)//auth change
		;//SP_CTRL_Auth_Change_Int_Handler();

	if(c2 & SP_COMMON_INT2_AUTHDONE)//auth done
		SP_CTRL_Auth_Done_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	//for mipi interrupt
	if(c2 & SP_COMMON_INT2_MIPI_HTOTAL_CHG)//mipi htotal change
		SP_CTRL_MIPI_Htotal_Chg_Int_Handler();
	if(sp_tx_pd_mode )
		return;

	if(c3 & SP_COMMON_INT3_MIPI_PACKET_LEN_CHG)//
		SP_CTRL_MIPI_Packet_Len_Chg_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	if(c3 & SP_COMMON_INT3_MIPI_LANE_CLK_CHG)//
		SP_CTRL_MIPI_Lane_clk_Chg_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	/*added for B0 version-ANX.Fei-20110831-Begin*/
	if(c5 & SP_TX_INT_DPCD_IRQ_REQUEST)//IRQ int
		SP_CTRL_SINK_IRQ_Int_Handler();
	if(sp_tx_pd_mode )
		return;

		#ifndef Standard_DP
		if(c5 & SP_TX_INT_STATUS1_POLLING_ERR)//c-wire polling error
			SP_CTRL_POLLING_ERR_Int_Handler();
		if(sp_tx_pd_mode )
			return;
		#endif

	if(c5 & SP_TX_INT_STATUS1_TRAINING_Finish)//link training finish int
		SP_CTRL_LT_DONE_Int_Handler();
	if(sp_tx_pd_mode )
		return;

	if(c5 & SP_TX_INT_STATUS1_LINK_CHANGE)//link is lost  int
		SP_CTRL_LINK_CHANGE_Int_Handler();
	/*added for B0 version-ANX.Fei-20110831-End*/
	}
}

BYTE SP_CTRL_Chip_Detect(void)
{
	return SP_TX_Chip_Located();
}


void SP_CTRL_Chip_Initial(void)
{
	SP_CTRL_Variable_Init();

	//set video Input format
	if(MIPI_EN) {
		SP_CTRL_InputSet(MIPI_DSI,COLOR_RGB, COLOR_8_BIT);
		pr_info("MIPI interface selected!");
	} else {
		SP_CTRL_InputSet(LVTTL_RGB,COLOR_RGB, COLOR_8_BIT);
		SP_CTRL_Set_LVTTL_Interface(SeparateSync,SeparateDE,UnYCMUX,DDR,Negedge);//separate SYNC,DE, not YCMUX, SDR,negedge
		pr_info("LVTTL interface selected!");
	}
	//set audio input format
#if(UPDATE_ANX7730_SW)
	if(0)
#else
	if(!BIST_EN)
#endif
	{
		if((!AUD_IN_SEL_1)&&(!AUD_IN_SEL_2)) { //00:SPDIF input as default
			pr_info("Set audio input to SPDIF");
			SP_CTRL_AUDIO_FORMAT_Set(AUDIO_SPDIF,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
		} else if((!AUD_IN_SEL_1)&&(AUD_IN_SEL_2)) { //01: I2S input
			pr_info("Set audio input to I2S");
			SP_CTRL_AUDIO_FORMAT_Set(AUDIO_I2S,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
			SP_CTRL_I2S_CONFIG_Set(I2S_CH_2, I2S_LAYOUT_0);
		} else if((AUD_IN_SEL_1)&&(!AUD_IN_SEL_2)) {	//10:Slimbus input
			pr_info("Set audio input to Slimbus");
			SP_CTRL_AUDIO_FORMAT_Set(AUDIO_SLIMBUS,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
			SP_CTRL_I2S_CONFIG_Set(I2S_CH_2, I2S_LAYOUT_0);
		} else
			pr_info("invalid audio input");
	} else { //set bist audio Input format
		pr_info("bist audio input");
		SP_CTRL_AUDIO_FORMAT_Set(AUDIO_BIST,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
	}
	SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);

	//vbus_power_ctrl(0); // Disable the power supply for ANX7730 //liujg0729
	SP_TX_Hardware_PowerDown();//Power down ANX7805 totally

}

void slimport_config_video_input(void)
{
/*#ifdef __KEIL51_ENV__*/
	if(BIST_EN) {
		bBIST_FORMAT_INDEX_backup = bBIST_FORMAT_INDEX;
		SP_CTRL_BIST_CLK_Genarator(bBIST_FORMAT_INDEX);//Generate Bist format clock
		SP_TX_Config_BIST_Video(bBIST_FORMAT_INDEX, &SP_TX_Video_Input);
	}
/*#endif*/
	{
		if(get_bandwidth_and_pclk() == 0) {
			sp_tx_link_training_state = LINK_TRAINING_INIT;
			SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
		}
	}
}
void slimport_config_video_output(void)
{
	BYTE three_video_type = 0xFF;
	SP_TX_Video_Mute(1);
	//enable video input
	sp_tx_enable_video_input(1);
	msleep(50);
	SP_TX_AVI_Setup();//initial AVI infoframe packet
	SP_TX_Config_Packets(AVI_PACKETS);
	// 3d packed config
	/*if(EN_3D) {*/
	if (video_format_change !=0) {
#if(ENABLE_3D)
		if (three_3d_format == VIDEO_3D_TOP_AND_BOTTOM)
			three_video_type = 0x06;
		else if (three_3d_format == VIDEO_3D_SIDE_BY_SIDE)
			three_video_type = 0x08;

		pr_err("send 3D packet, format:%d, three_video_type:%x\r\n", three_3d_format, (unsigned int)three_video_type);
		sp_tx_send_3d_vsi_packet_to_7730(three_video_type);
#endif
	}
	//SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
	SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
}

void slimport_hdcp_authentication(BYTE enable)
{
	if(enable) {
		SP_CTRL_HDCP_Process();
	} else {
		if (mute_count <= 0) {
			SP_TX_Power_Enable(SP_TX_PWR_HDCP, SP_TX_POWER_DOWN);// Poer down HDCP link clock domain logic for B0 version-20110913-ANX.Fei
			SP_TX_Show_Infomation();
			SP_TX_Video_Mute(0);
			//SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
			SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
		} else {
			pr_info("authentication, mute_count > 0, mute_count:%d\n", mute_count);
			mute_count--;
		}
	}
}


void SP_CTRL_TimerProcess (void)
{
	switch(sp_tx_system_state) {
	default:
	case SP_TX_INITIAL:
		break;
	case SP_TX_WAIT_SLIMPORT_PLUGIN:
		#ifdef Standard_DP
		SP_CTRL_Slimport_Plug_Process();
		#endif
		break;
	case SP_TX_PARSE_EDID:
		SP_CTRL_EDID_Process();
		pr_err("slimport_edid_p: %p\n", slimport_edid_p);
		if (slimport_edid_p != NULL) {
			edid_3d_data_p p_edid = (edid_3d_data_p)slimport_edid_p;
			slimport_read_edid_All((uint8_t *)p_edid->EDID_block_data);
			si_mhl_tx_handle_atomic_hw_edid_read_complete((edid_3d_data_p)slimport_edid_p);

			if (p_edid->parse_data.HDMI_sink == true)
				Notify_AP_MHL_TX_Event(SLIMPORT_TX_EVENT_EDID_DONE, 0, NULL);
			else
				pr_err("p_edid->parse_data.HDMI_sink is false\n");
		}
		break;
	case SP_TX_CONFIG_VIDEO_INPUT:
		slimport_config_video_input();
		break;
	case SP_TX_LINK_TRAINING:
		if(SP_TX_HW_Link_Training())
			return;
		break;
	case SP_TX_CONFIG_VIDEO_OUTPUT:
		slimport_config_video_output();
		break;
	case SP_TX_CONFIG_AUDIO:
		SP_TX_Config_Audio(&SP_TX_Audio_Input);
		break;
	case SP_TX_HDCP_AUTHENTICATION:
		slimport_hdcp_authentication((HDCP_EN == 0 ? 0 : 1));
		break;
	case SP_TX_PLAY_BACK:
		SP_CTRL_PlayBack_Process();
		break;
	}
}

void SP_CTRL_Main_Procss(void)
{
#if(REDUCE_REPEAT_PRINT_INFO)
	confirm_loop_print_msg();
#endif
	SP_CTRL_TimerProcess();

#ifndef EXT_INT
	SP_CTRL_Int_Process();
#else

	if(ext_int_index) {
		ext_int_index = 0;
		SP_CTRL_Int_Process();
	}
#endif

}

int SP_Get_Connect_Status()
{
	return (sp_tx_system_state >= SP_TX_CONFIG_VIDEO_INPUT);
}

#undef _SP_TX_DRV_C_

