/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
   BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_i2c.h> 
	#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_pm_ldo.h>
    #include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>
#ifndef FPGA_EARLY_PORTING
#include <cust_i2c.h>
#endif
#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif


static const unsigned int BL_MIN_LEVEL =20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    


#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>  
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
//#include <linux/jiffies.h>
#include <linux/uaccess.h>
//#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
/***************************************************************************** 
 * Define
 *****************************************************************************/
#ifndef FPGA_EARLY_PORTING
#define TPS_I2C_BUSNUM  I2C_I2C_LCD_BIAS_CHANNEL//for I2C channel 0
#define I2C_ID_NAME "tps65132"
#define TPS_ADDR 0x3E
/***************************************************************************** 
 * GLobal Variable
 *****************************************************************************/
static struct i2c_board_info __initdata tps65132_board_info = {I2C_BOARD_INFO(I2C_ID_NAME, TPS_ADDR)};
static struct i2c_client *tps65132_i2c_client = NULL;


/***************************************************************************** 
 * Function Prototype
 *****************************************************************************/ 
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);
/***************************************************************************** 
 * Data Structure
 *****************************************************************************/

 struct tps65132_dev	{	
	struct i2c_client	*client;
	
};

static const struct i2c_device_id tps65132_id[] = {
	{ I2C_ID_NAME, 0 },
	{ }
};

//#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
//static struct i2c_client_address_data addr_data = { .forces = forces,};
//#endif
static struct i2c_driver tps65132_iic_driver = {
	.id_table	= tps65132_id,
	.probe		= tps65132_probe,
	.remove		= tps65132_remove,
	//.detect		= mt6605_detect,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tps65132",
	},
 
};
/***************************************************************************** 
 * Extern Area
 *****************************************************************************/ 
 
 

/***************************************************************************** 
 * Function
 *****************************************************************************/ 
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{  
	printk( "tps65132_iic_probe\n");
	printk("TPS: info==>name=%s addr=0x%x\n",client->name,client->addr);
	tps65132_i2c_client  = client;		
	return 0;      
}


static int tps65132_remove(struct i2c_client *client)
{  	
  printk( "tps65132_remove\n");
  tps65132_i2c_client = NULL;
   i2c_unregister_device(client);
  return 0;
}


 int tps65132_write_bytes(unsigned char addr, unsigned char value)
{	
	int ret = 0;
	struct i2c_client *client = tps65132_i2c_client;
	char write_data[2]={0};	
	write_data[0]= addr;
	write_data[1] = value;
    ret=i2c_master_send(client, write_data, 2);
	if(ret<0)
	printk("tps65132 write data fail !!\n");	
	return ret ;
}
EXPORT_SYMBOL_GPL(tps65132_write_bytes);



/*
 * module load/unload record keeping
 */

static int __init tps65132_iic_init(void)
{

   printk( "tps65132_iic_init\n");
   i2c_register_board_info(TPS_I2C_BUSNUM, &tps65132_board_info, 1);
   printk( "tps65132_iic_init2\n");
   i2c_add_driver(&tps65132_iic_driver);
   printk( "tps65132_iic_init success\n");	
   return 0;
}

static void __exit tps65132_iic_exit(void)
{
  printk( "tps65132_iic_exit\n");
  i2c_del_driver(&tps65132_iic_driver);  
}


module_init(tps65132_iic_init);
module_exit(tps65132_iic_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL"); 
#endif
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define HX8395 1

#if HX8395
#define FRAME_WIDTH  										(1080)//(1080)
#define FRAME_HEIGHT 										(1920)//(1920)
#else
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)
#endif
#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#ifndef GPIO_LCD_BIAS_ENP_PIN
#define GPIO_LCD_BIAS_ENP_PIN GPIO122
#endif
#ifndef GPIO_LCD_BIAS_ENN_PIN
#define GPIO_LCD_BIAS_ENN_PIN GPIO95
#endif
#ifndef GPIO_LCM_BL_EN
#define GPIO_LCM_BL_EN GPIO113
#endif
#ifndef GPIO_LCM_LED_EN
#define GPIO_LCM_LED_EN GPIO94
#endif

#define LCM_DSI_CMD_MODE									0
#ifndef FPGA_EARLY_PORTING
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#endif
#define LCM_ID_OTM1284 0x40

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
       

static struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

#if 0
static struct LCM_setting_table lcm_initialization_setting[] = {
	
	/*
	Note :

	Data ID will depends on the following rule.
	
		count of parameters > 1	=> Data ID = 0x39
		count of parameters = 1	=> Data ID = 0x15
		count of parameters = 0	=> Data ID = 0x05

	Structure Format :

	{DCS command, count of parameters, {parameter list}}
	{REGFLAG_DELAY, milliseconds of time, {}},

	...

	Setting ending by predefined flag
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/


	//must use 0x39 for init setting for all register.

	{0XB9, 3, {0XFF,0X83,0X89}},
	{REGFLAG_DELAY, 10, {}},

#if 0//CLOSE IC ESD Protect enhance
	{0XBA, 17, {0X41,0X83,0X00,0X16,
				0XA4,0X00,0X18,0XFF,
				0X0F,0X21,0X03,0X21,
				0X23,0X25,0X20,0X02, 
		  		0X31}}, 
	{REGFLAG_DELAY, 10, {}},
#else //open IC ESD  Protect enhance 
{0XBA, 17, {0X41,0X83,0X00,0X16,
			0XA4,0X00,0X18,0XFF,
			0X0F,0X21,0X03,0X21,
			0X23,0X25,0X20,0X02, 
			0X35}}, 
{REGFLAG_DELAY, 10, {}},

#endif

	{0XDE, 2, {0X05,0X58}},
	{REGFLAG_DELAY, 10, {}},

	{0XB1, 19, {0X00,0X00,0X04,0XD9,
				0XCF,0X10,0X11,0XAC,
				0X0C,0X1D,0X25,0X1D,
				0X1D,0X42,0X01,0X58,
		  		0XF7,0X20,0X80}},
	{REGFLAG_DELAY, 10, {}},


	{0XB2, 5,   {0X00,0X00,0X78,0X03,
				 0X02}},
	{REGFLAG_DELAY, 10, {}},
	
	{0XB4, 31, {0X82,0X04,0X00,0X32,
				0X10,0X00,0X32,0X10,
				0X00,0X00,0X00,0X00,
				0X17,0X0A,0X40,0X01,
				0X13,0X0A,0X40,0X14,
				0X46,0X50,0X0A,0X0A,
				0X3C,0X0A,0X3C,0X14,
		  		0X46,0X50,0X0A}}, 
	{REGFLAG_DELAY, 10, {}},
	
	{0XD5, 48, {0X00,0X00,0X00,0X00,
				0X01,0X00,0X00,0X00,
				0X20,0X00,0X99,0X88,
				0X88,0X88,0X88,0X88,
				0X88,0X88,0X88,0X01,
				0X88,0X23,0X01,0X88,
				0X88,0X88,0X88,0X88,
				0X88,0X88,0X99,0X88,
				0X88,0X88,0X88,0X88,
				0X88,0X88,0X32,0X88,
				0X10,0X10,0X88,0X88,
				0X88,0X88,0X88,0X88}}, 
	{REGFLAG_DELAY, 10, {}},
	

	{0XB6, 4,   {0X00,0X8A,0X00,0X8A}},
	{REGFLAG_DELAY, 10, {}},

	{0XCC, 1,   {0X02}},
	{REGFLAG_DELAY, 10, {}},


	{0X35, 1,   {0X00}},//TE on
	{REGFLAG_DELAY, 10, {}},

	// Note
	// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.

	// Setting ending by predefined flag
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},

    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_backlight_level_setting[] = {
{0x51, 1, {0xFF}},
{REGFLAG_END_OF_TABLE, 0x00, {}}
};

#ifdef BUILD_LK
#ifndef FPGA_EARLY_PORTING
#define TPS65132_SLAVE_ADDR_WRITE  0x7C  
static struct mt_i2c_t TPS65132_i2c;

int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value)
{
    kal_uint32 ret_code = I2C_OK;
    kal_uint8 write_data[2];
    kal_uint16 len;

    write_data[0]= addr;
    write_data[1] = value;

    TPS65132_i2c.id = I2C_I2C_LCD_BIAS_CHANNEL;//I2C2;
    /* Since i2c will left shift 1 bit, we need to set FAN5405 I2C address to >>1 */
    TPS65132_i2c.addr = (TPS65132_SLAVE_ADDR_WRITE >> 1);
    TPS65132_i2c.mode = ST_MODE;
    TPS65132_i2c.speed = 100;
    len = 2;

    ret_code = i2c_write(&TPS65132_i2c, write_data, len);
    //printf("%s: i2c_write: ret_code: %d\n", __func__, ret_code);

    return ret_code;
}

#else
  
//	extern int mt8193_i2c_write(u16 addr, u32 data);
//	extern int mt8193_i2c_read(u16 addr, u32 *data);
	
//	#define TPS65132_write_byte(add, data)  mt8193_i2c_write(add, data)
	//#define TPS65132_read_byte(add)  mt8193_i2c_read(add)
  
#endif
#endif
static void lcm_init_power(void)
{
#ifndef FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6325_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_suspend_power(void)
{
#ifndef FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(0);
#else
	printk("%s, begin\n", __func__);
	hwPowerDown(MT6325_POWER_LDO_VGP1, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_resume_power(void)
{
#ifndef FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6325_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_init_registers()
{
#if 1
	unsigned int data_array[16];
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);//EXTC = 1
	
	data_array[0] = 0x00042902;
	data_array[1] = 0x018312FF;
	dsi_set_cmdq(&data_array, 2, 1);//EXTC = 1
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Orise mode enable
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x008312FF;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*===================panel setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//TCON Setting
	
	data_array[0] = 0x000A2902;
	data_array[1] = 0x006400C0;
	data_array[2] = 0x6400120e;
	data_array[3] = 0x0000120e;
	dsi_set_cmdq(&data_array, 4, 1);

	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x55C02300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x81002300;
	dsi_set_cmdq(&data_array, 1, 1);	//frame rate: 60Hz
	
	data_array[0] = 0x55C12300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x81002300;				//Source bias 0.75uA
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x82C42300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x90002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x49C42300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x82002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x02C42300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0xc6002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x03b02300;
	dsi_set_cmdq(&data_array, 1, 1);

	//////////////////////////////////////////
#if 0
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Panel Timing Setting
	
	data_array[0] = 0x00072902;
	data_array[1] = 0x005C00C0;
	data_array[2] = 0x00040001;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0xA4002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Source pre.
	
	data_array[0] = 0x1CC02300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xB3002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Interval Scan Frame: 0 frame, column inversion
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x005000C0;
	dsi_set_cmdq(&data_array, 2, 1);

	#endif
	/*================Power setting===============*/
	data_array[0] = 0xA0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//DCDC setting
	data_array[1] = 0x061005C4;
	data_array[2] = 0x10150502;
	data_array[3] = 0x02071005;
	data_array[4] = 0x00101505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//Clamp coltage setting
	data_array[1] = 0x000000C4;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xBB002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x80c52300;	//LVD voltage level setting
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x91002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGH=12v, VGL=-12v, pump ratio: VGH=6x, VGL=-5x
	data_array[1] = 0x005016c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//GVDD=4.87v, NGVDD = -4.87V
	data_array[1] = 0x00aeaed8;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VDD_18v=1.6v, LVDSVDD=1.55v
	data_array[1] = 0x00b804c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*=========================Panel Timming State Control================*/
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00052902;	//mode-3
	data_array[1] = 0x021102f5;
	data_array[2] = 0x00000011;
	dsi_set_cmdq(&data_array, 3, 1);
	
		data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x50c52300;	//2xVPNL, 1.5*=00, 2*=50, 3*=A0
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x94002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x66c52300;	//Frequency
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*===============VGL01/02 disable================*/
	data_array[0] = 0xb2002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb6002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb8002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0] = 0x94002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VCL ON
	data_array[1] = 0x000002f5;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xBA002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VSP ON
	data_array[1] = 0x000003f5;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xB2002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VCL ON
	data_array[1] = 0x000040C5;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0xB4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VCL ON
	data_array[1] = 0x0000C0C5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000C2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x050505cb;
	data_array[2] = 0x00050505;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x05050500;
	data_array[3] = 0x05050505;
	data_array[4] = 0x00000505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xe0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00050505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xf0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//Panel timing state control
	data_array[1] = 0xffffffcb;
	data_array[2] = 0xffffffff;
	data_array[3] = 0xfffffffC;
	dsi_set_cmdq(&data_array, 4, 1);
	
	/*===============Panel pad mapping control===============*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x100A0Ccc;
	data_array[2] = 0x0004020e;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2d2e0600;
	data_array[3] = 0x0d0f090b;
	data_array[4] = 0x00000301;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002d2e05;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x090f0dcc;
	data_array[2] = 0x0001030b;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2e2d0600;
	data_array[3] = 0x0c0a100e;
	data_array[4] = 0x00000204;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002e2d05;
	dsi_set_cmdq(&data_array, 5, 1);
	
	/*===============Panel Timing Setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000d2902;	//Panel VST Setting
	data_array[1] = 0x18038bce;
	data_array[2] = 0x8918038a;
	data_array[3] = 0x03881803;
	data_array[4] = 0x00000018;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel vend setting
	data_array[1] = 0x180f38ce;
	data_array[2] = 0x00180e38;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka1/2 setting
	data_array[1] = 0x050738ce;
	data_array[2] = 0x00180000;
	data_array[3] = 0x01050638;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka3/4 setting
	data_array[1] = 0x050538ce;
	data_array[2] = 0x00180002;
	data_array[3] = 0x03050438;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb1/2 setting
	data_array[1] = 0x050338ce;
	data_array[2] = 0x00180004;
	data_array[3] = 0x05050238;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb3/4 setting
	data_array[1] = 0x050138ce;
	data_array[2] = 0x00180006;
	data_array[3] = 0x07050038;
	data_array[4] = 0x00001800;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//gate pre. ena
	data_array[1] = 0x200101cf;
	data_array[2] = 0x02000020;
	data_array[3] = 0x08030081;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0xb5002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00072902;	//normal output with VGH/VGL
	data_array[1] = 0x7f1138c5;
	data_array[2] = 0x007f1138;
	dsi_set_cmdq(&data_array, 3, 1);
	
	/*===================Gamma====================*/
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x1c1605e1;
	data_array[2] = 0x0c11060d;
	data_array[3] = 0x0807020b;
	data_array[4] = 0x0b100e07;
	data_array[5] = 0x00000002;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x1C1605e2;
	data_array[2] = 0x0c12060d;
	data_array[3] = 0x0907020b;
	data_array[4] = 0x0b110e07;
	data_array[5] = 0x00000002;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x92002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00032902;
	data_array[1] = 0x000230ff;
	dsi_set_cmdq(&data_array, 2 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00022902;
	data_array[1] = 0x000070d9;
	dsi_set_cmdq(&data_array, 2 ,1);
#else
	unsigned int data_array[16];
	data_array[0] = 0x00042902;
	data_array[1] = 0x018312FF;
	dsi_set_cmdq(&data_array, 2, 1);//EXTC = 1
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Orise mode enable
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x008312FF;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*===================panel setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);	//TCON Setting
	
	data_array[0] = 0x000A2902;
	data_array[1] = 0x006400C0;
	data_array[2] = 0x6400110F;
	data_array[3] = 0x0000110F;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Panel Timing Setting
	
	data_array[0] = 0x00072902;
	data_array[1] = 0x005C00C0;
	data_array[2] = 0x00040001;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0xA4002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Source pre.
	
	data_array[0] = 0x1CC02300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xB3002300;
	dsi_set_cmdq(&data_array, 1, 1);	//Interval Scan Frame: 0 frame, column inversion
	
	data_array[0] = 0x00032902;
	data_array[1] = 0x005000C0;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x81002300;
	dsi_set_cmdq(&data_array, 1, 1);	//frame rate: 60Hz
	
	data_array[0] = 0x55C12300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x81002300;				//Source bias 0.75uA
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x82C42300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x90002300;				//clock delay for data latch
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x49C42300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*================Power setting===============*/
	data_array[0] = 0xA0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//DCDC setting
	data_array[1] = 0x061005C4;
	data_array[2] = 0x10150502;
	data_array[3] = 0x02071005;
	data_array[4] = 0x00101505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//Clamp coltage setting
	data_array[1] = 0x000000C4;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x91002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGH=12v, VGL=-12v, pump ratio: VGH=6x, VGL=-5x
	data_array[1] = 0x005019c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//GVDD=4.87v, NGVDD = -4.87V
	data_array[1] = 0x00bcbcd8;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xd9652300;		//VCOMDC=-1.1
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xB0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VDD_18v=1.6v, LVDSVDD=1.55v
	data_array[1] = 0x00b804c5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xBB002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x80c52300;	//LVD voltage level setting
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*=========================Control setting======================*/
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x40d02300;		//ID1
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//ID2, ID3
	data_array[1] = 0x000000d1;
	dsi_set_cmdq(&data_array, 2, 1);
	
	/*=========================Panel Timming State Control================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000C2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x050505cb;
	data_array[2] = 0x00050505;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x05050000;
	data_array[3] = 0x05050505;
	data_array[4] = 0x00000505;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xe0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000F2902;	//Panel timing state control
	data_array[1] = 0x000000cb;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00050500;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xf0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//Panel timing state control
	data_array[1] = 0xffffffcb;
	data_array[2] = 0xffffffff;
	data_array[3] = 0xffffffff;
	dsi_set_cmdq(&data_array, 4, 1);
	
	/*===============Panel pad mapping control===============*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x0e0c0acc;
	data_array[2] = 0x00040210;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2d2e0000;
	data_array[3] = 0x0f0d0b09;
	data_array[4] = 0x00000301;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00040210;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002d2e00;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x0b0d0fcc;
	data_array[2] = 0x00010309;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00102902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x2e2d0000;
	data_array[3] = 0x0a0c0e10;
	data_array[4] = 0x00000204;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel pad mapping control
	data_array[1] = 0x000000cc;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x002e2d00;
	dsi_set_cmdq(&data_array, 5, 1);
	
	/*===============Panel Timing Setting====================*/
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000d2902;	//Panel VST Setting
	data_array[1] = 0x18038fce;
	data_array[2] = 0x8d18038e;
	data_array[3] = 0x038c1803;
	data_array[4] = 0x00000018;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel vend setting
	data_array[1] = 0x000000ce;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka1/2 setting
	data_array[1] = 0x050b38ce;
	data_array[2] = 0x00000000;
	data_array[3] = 0x01050a38;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clka3/4 setting
	data_array[1] = 0x050938ce;
	data_array[2] = 0x00000002;
	data_array[3] = 0x03050838;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb1/2 setting
	data_array[1] = 0x050738ce;
	data_array[2] = 0x00000004;
	data_array[3] = 0x05050638;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xd0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkb3/4 setting
	data_array[1] = 0x050538ce;
	data_array[2] = 0x00000006;
	data_array[3] = 0x07050438;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x80002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkc3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xa0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd1/2 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xb0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000f2902;	//Panel clkd3/4 setting
	data_array[1] = 0x000000cf;
	data_array[2] = 0x00000000;
	data_array[3] = 0x00000000;
	data_array[4] = 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);
	
	data_array[0] = 0xc0002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x000c2902;	//gate pre. ena
	data_array[1] = 0x200101cf;
	data_array[2] = 0x01000020;
	data_array[3] = 0x08030081;
	dsi_set_cmdq(&data_array, 4, 1);
	
	data_array[0] = 0xb5002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00072902;	//normal output with VGH/VGL
	data_array[1] = 0xfff133c5;
	data_array[2] = 0x00fff133;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00052902;	//mode-3
	data_array[1] = 0x021102f5;
	data_array[2] = 0x00000011;
	dsi_set_cmdq(&data_array, 3, 1);
	
	data_array[0] = 0x90002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x50c52300;	//2xVPNL, 1.5*=00, 2*=50, 3*=A0
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x94002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x66c52300;	//Frequency
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*===============VGL01/02 disable================*/
	data_array[0] = 0xb2002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL01_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb6002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb8002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0x00032902;	//VGL02_S
	data_array[1] = 0x000000f5;
	dsi_set_cmdq(&data_array, 2, 1);
	
	data_array[0] = 0xb4002300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	data_array[0] = 0xC0C52300;
	dsi_set_cmdq(&data_array, 1, 1);
	
	/*===================Gamma====================*/
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x231d0ae1;
	data_array[2] = 0x0a0f040d;
	data_array[3] = 0x0a060309;
	data_array[4] = 0x0d110e05;
	data_array[5] = 0x00000001;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00112902;
	data_array[1] = 0x221d0ae2;
	data_array[2] = 0x0b0e040e;
	data_array[3] = 0x0906020a;
	data_array[4] = 0x0d110e05;
	data_array[5] = 0x00000001;
	dsi_set_cmdq(&data_array, 6 ,1);
	
	data_array[0] = 0x00002300;
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00042902;
	data_array[1] = 0x000000ff;
	dsi_set_cmdq(&data_array, 2 ,1);
#endif
}
static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
       	}
    }
	
}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = BURST_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
            //The following defined the fomat for data coming from LCD engine.
            params->dsi.data_format.color_order     = LCM_COLOR_ORDER_RGB;
            params->dsi.data_format.trans_seq       = LCM_DSI_TRANS_SEQ_MSB_FIRST;
            params->dsi.data_format.padding         = LCM_DSI_PADDING_ON_LSB;
            params->dsi.data_format.format              = LCM_DSI_FORMAT_RGB888;

            // Highly depends on LCD driver capability.
                 params->dsi.packet_size=256;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 2;
		params->dsi.vertical_backporch					= 12;
		params->dsi.vertical_frontporch					= 18;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
#if HX8395
		params->dsi.horizontal_sync_active				= 42;//2;
		params->dsi.horizontal_backporch				= 64;
		params->dsi.horizontal_frontporch				= 13;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

#else
		params->dsi.horizontal_sync_active				= 2;
		params->dsi.horizontal_backporch				= 42;
		params->dsi.horizontal_frontporch				= 44;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
#endif
#ifndef FPGA_EARLY_PORTING
#if HX8395
                params->dsi.PLL_CLOCK = 400;//220; //this value must be in MTK suggested table

#else
                params->dsi.PLL_CLOCK = 220; //this value must be in MTK suggested table
#endif
#else
                params->dsi.pll_div1 = 0;
                params->dsi.pll_div2 = 0;
                params->dsi.fbk_div = 0x1;
#endif
params->dsi.LPX = 10;
}

static unsigned int lcm_compare_id(void)
{
		unsigned int id = 0;
		unsigned char buffer[2];
		unsigned int array[16];
			SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
			SET_RESET_PIN(0);
			MDELAY(1);
			SET_RESET_PIN(1);
			MDELAY(150);
	
	//	push_table(lcm_compare_id_setting, sizeof(lcm_compare_id_setting) / sizeof(struct LCM_setting_table), 1);
	
		array[0] = 0x00023700;// read id return two byte,version and id
		dsi_set_cmdq(array, 1, 1);
		read_reg_v2(0xDA, buffer, 1);
	
		id = buffer[0]; //we only need ID
      #ifdef BUILD_UBOOT
		printf("%s,  id otm1284A= 0x%08x\n", __func__, id);
	  #endif
		return (LCM_ID_OTM1284 == id)?1:0;


}


static void lcm_init(void)
{
	unsigned int data_array[16];

	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0E;
#ifndef FPGA_EARLY_PORTING
MDELAY(5);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
MDELAY(5);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
MDELAY(10);
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
    if(ret)    	
    dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
	dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write success----\n",cmd);    		
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
	
	cmd=0x01;
	data=0x0E;
#ifdef BUILD_LK
	ret=TPS65132_write_byte(cmd,data);
    if(ret)    	
	    dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
		dprintf(0, "[LK]nt35595----tps6132----cmd=%0x--i2c write success----\n",cmd);   
#else
	ret=tps65132_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]nt35595----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
#endif	
#if HX8395
	SET_RESET_PIN(1);
	MDELAY(10);//5
    SET_RESET_PIN(0);
	MDELAY(10);//50
    SET_RESET_PIN(1);
	MDELAY(10);//100
#else	
	SET_RESET_PIN(1);
	MDELAY(10);//5
    SET_RESET_PIN(0);
	MDELAY(120);//50
    SET_RESET_PIN(1);
	MDELAY(10);//100
#endif
#if 0

        data_array[0]= 0x00043902;
	data_array[1]= 0x9483FFB9;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0]= 0x00033902;
	data_array[1]= 0x008333BA;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00103902;
	data_array[1]= 0x0E0E6CB1;
	data_array[2]= 0xF1110437;
	data_array[3]= 0x2395E880;
	data_array[4]= 0x18D2C080;
	dsi_set_cmdq(&data_array, 5, 1);



	data_array[0]= 0x000C3902;
	data_array[1]= 0x0E6400B2;
	data_array[2]= 0x0823320D;
	data_array[3]= 0x004D1C08;
	dsi_set_cmdq(&data_array, 4, 1);


	data_array[0]= 0x000D3902;
	data_array[1]= 0x03FF00B4;
	data_array[2]= 0x03500350;
	data_array[3]= 0x016A0150;
	data_array[4]= 0x0000006A;
	dsi_set_cmdq(&data_array, 5, 1);

        data_array[0]= 0x00043902;
	data_array[1]= 0x010E41BF;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00263902;
	data_array[1]= 0x000700D3; //GIP
	data_array[2]= 0x00100000;
	data_array[3]= 0x00051032;
	data_array[4]= 0x00103200;
	data_array[5]= 0x10320000;
	data_array[6]= 0x36000000;
	data_array[7]= 0x37090903;
	data_array[8]= 0x00370000;
	data_array[9]= 0x0A000000;
	data_array[10]= 0x00000100;
	dsi_set_cmdq(&data_array, 11, 1);

	data_array[0]= 0x002D3902;
	data_array[1]= 0x000302D5;
	data_array[2]= 0x04070601;
	data_array[3]= 0x22212005;
	data_array[4]= 0x18181823;
	data_array[5]= 0x18181818;
	data_array[6]= 0x18181818;
	data_array[7]= 0x18181818;
	data_array[8]= 0x18181818;
	data_array[9]= 0x18181818;
	data_array[10]= 0x24181818;
	data_array[11]= 0x19181825;
	data_array[12]= 0x00000019;
	dsi_set_cmdq(&data_array, 13, 1);


	data_array[0]= 0x002D3902;
	data_array[1]= 0x070405D6;
	data_array[2]= 0x03000106;
	data_array[3]= 0x21222302;
	data_array[4]= 0x18181820;
	data_array[5]= 0x58181818;
	data_array[6]= 0x18181858;
	data_array[7]= 0x18181818;
	data_array[8]= 0x18181818;
	data_array[9]= 0x18181818;
	data_array[10]= 0x25181818;
	data_array[11]= 0x18191924;
	data_array[12]= 0x00000018;
	dsi_set_cmdq(&data_array, 13, 1);
	

	data_array[0]= 0x002B3902;
	data_array[1]= 0x231F07E0;
	data_array[2]= 0x2E3F3B38;
	data_array[3]= 0x0D0B0846;
	data_array[4]= 0x15120F17;
	data_array[5]= 0x12081413;
	data_array[6]= 0x1F071916;
	data_array[7]= 0x3F3B3823;
	data_array[8]= 0x0B08462E;
	data_array[9]= 0x120F170D;
	data_array[10]= 0x08141315;
	data_array[11]= 0x00191612;
	dsi_set_cmdq(&data_array, 12, 1);

        data_array[0]= 0x00023902;
	data_array[1]= 0x000009CC;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0]= 0x00053902;
	data_array[1]= 0x40C000C7;
	data_array[2]= 0x000000C0;
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(10);

        data_array[0]= 0x00033902;
	data_array[1]= 0x007575B6;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00033902;
	data_array[1]= 0x001430C0;
	dsi_set_cmdq(&data_array, 2, 1);

        data_array[0]= 0x00023902;
	data_array[1]= 0x000007BC;
	dsi_set_cmdq(&data_array, 2, 1);
 
        data_array[0]= 0x00023902;
	data_array[1]= 0x00000135;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x0000ff51;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0]= 0x00043902;
	data_array[1]= 0x1e001fC9;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0]= 0x00023902;
	data_array[1]= 0x00000055;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= 0x00002453;
	dsi_set_cmdq(&data_array, 2, 1);


	data_array[0] = 0x00110500;	
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(200);

	data_array[0] = 0x00290500;	
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(10);
#else//8395a
  	data_array[0]= 0x00043902;
	data_array[1]= 0x9583FFB9;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x00053902;
	data_array[1]= 0x7D0000B0;
	data_array[2]= 0x0000000C;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0]= 0x000E3902;
	data_array[1]= 0xA08333BA;
	data_array[2]= 0x0080B265;
	data_array[3]= 0x0FFF1000;
	data_array[4]= 0x00000800;
	dsi_set_cmdq(&data_array, 5, 1);

	data_array[0]= 0x000C3902;//9.22 Update
	data_array[1]= 0x18186CB1;
	data_array[2]= 0xF1110423;
	data_array[3]= 0x23E9A380;
	dsi_set_cmdq(&data_array, 4, 1);


  	data_array[0]= 0x00023902;//9.22 Update
	data_array[1]= 0x000088D2;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00063902;
	data_array[1]= 0x0CB400B2;
	data_array[2]= 0x00007A10;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0]= 0x00383902;	//GIP//9.22 Update
		data_array[1]= 0x000000D3; 
		data_array[2]= 0x00100000;
		data_array[3]= 0x00011032;
		data_array[4]= 0xC0133201;
		data_array[5]= 0x10320000;
		data_array[6]= 0x37000008;
		data_array[7]= 0x37030304;
		data_array[8]= 0x00470004;
		data_array[9]= 0x0A000000;
		data_array[10]= 0x15010100;
		data_array[11]= 0x00000000;
		data_array[12]= 0xC0030000;
		data_array[13]= 0x04020800;
		data_array[14]= 0x15010000;
		dsi_set_cmdq(&data_array, 15, 1);


	data_array[0]= 0x00173902;//9.22 Update
		data_array[1]= 0x0EFF00B4;
		data_array[2]= 0x0E4A0E4A;
		data_array[3]= 0x0150014A;
		data_array[4]= 0x013A0150;
		data_array[5]= 0x013A033A;
		data_array[6]= 0x004A014A;
		dsi_set_cmdq(&data_array, 7, 1);


	data_array[0]= 0x002D3902;  // Forward
	data_array[1]= 0x183939D5;
	data_array[2]= 0x03000118;
	data_array[3]= 0x07040502;
	data_array[4]= 0x18181806;
	data_array[5]= 0x38181818;
	data_array[6]= 0x21191938;
	data_array[7]= 0x18222320;
	data_array[8]= 0x18181818;
	data_array[9]= 0x18181818;
	data_array[10]= 0x18181818;
	data_array[11]= 0x18181818;
	data_array[12]= 0x00000018;
	dsi_set_cmdq(&data_array, 13, 1);

	data_array[0]= 0x002D3902;  // Backward
	data_array[1]= 0x193939D6;
	data_array[2]= 0x04070619;
	data_array[3]= 0x00030205;
	data_array[4]= 0x18181801;
	data_array[5]= 0x38181818;
	data_array[6]= 0x22181838;
	data_array[7]= 0x58212023;
	data_array[8]= 0x58585858;
	data_array[9]= 0x58585858;
	data_array[10]= 0x58585858;
	data_array[11]= 0x58585858;
	data_array[12]= 0x00000058;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00023902;
	data_array[1]= 0x00000ACB;
	dsi_set_cmdq(&data_array, 2, 1);
	
  	data_array[0]= 0x00023902;
	data_array[1]= 0x000008CC;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x00033902;
	data_array[1]= 0x001530C0;
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0]= 0x00063902;
	data_array[1]= 0x0C0800C7;
	data_array[2]= 0x0000D000;
	dsi_set_cmdq(&data_array, 3, 1);

	data_array[0]= 0x002B3902;//9.22 Update
		data_array[1]= 0x0D0800E0;
		data_array[2]= 0x1C3F322E;
		data_array[3]= 0x0C0A063C;
		data_array[4]= 0x15130E17;
		data_array[5]= 0x11071514;
		data_array[6]= 0x08001812;
		data_array[7]= 0x3F312E0C;
		data_array[8]= 0x0A073D1B;
		data_array[9]= 0x130F170D;
		data_array[10]= 0x06131315;
		data_array[11]= 0x00171210;
		dsi_set_cmdq(&data_array, 12, 1);


// YYG Code Correction: 20140830
//  	data_array[0]= 0x00033902;
//		data_array[1]= 0x006E6EB6;
//		dsi_set_cmdq(&data_array, 2, 1);
	
  //********************YYG*******************//
  	data_array[0]= 0x00023902;
	data_array[1]= 0x000001BD;
	dsi_set_cmdq(&data_array, 2, 1);
	
	// Himax EF
  	data_array[0]= 0x00043902;
	data_array[1]= 0x010002EF;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x00023902;
	data_array[1]= 0x000000BD;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x00023902;
	data_array[1]= 0x000000BD;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x00073902;
	data_array[1]= 0xF700B1EF;
	data_array[2]= 0x00022F37;
	dsi_set_cmdq(&data_array, 3, 1);

  	data_array[0]= 0x00033902;
	data_array[1]= 0x0060B2EF;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x000E3902;
	data_array[1]= 0x0000B5EF;
  	data_array[2]= 0xFF000000;
	data_array[3]= 0x00000003;
	data_array[4]= 0x00000000;
	dsi_set_cmdq(&data_array, 5, 1);

  	data_array[0]= 0x00163902;
	data_array[1]= 0x0040B6EF;
  	data_array[2]= 0x01000080;
	data_array[3]= 0x38040080;
	data_array[4]= 0x20200004;
	data_array[5]= 0x00043804;
	data_array[6]= 0x00002020;
	dsi_set_cmdq(&data_array, 7, 1);

 	data_array[0]= 0x00093902;
	data_array[1]= 0xA000B7EF;
 	data_array[2]= 0x00000400;
	data_array[3]= 0x00000800;
	dsi_set_cmdq(&data_array, 4, 1);

 	data_array[0]= 0x00303902;
	data_array[1]= 0x0000C0EF;
 	data_array[2]= 0x60026002;
	data_array[3]= 0xC005C004;
	data_array[4]= 0x800B8008;
	data_array[5]= 0x00160010;
	data_array[6]= 0x002D0020;
 	data_array[7]= 0x005A0040;
	data_array[8]= 0x00B40080;
 	data_array[9]= 0x01680100;
	data_array[10]= 0x02D10201;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0000C1EF;
  	data_array[2]= 0x60026002;
	data_array[3]= 0xC005C004;
	data_array[4]= 0x800B8008;
	data_array[5]= 0x00160010;
	data_array[6]= 0x002D0020;
  	data_array[7]= 0x005A0040;
	data_array[8]= 0x00B40080;
  	data_array[9]= 0x01680100;
	data_array[10]= 0x02D10201;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0001C2EF;
  	data_array[2]= 0x60996098;
	data_array[3]= 0xC132C031;
	data_array[4]= 0x82648062;
	data_array[5]= 0x04C807C5;
	data_array[6]= 0x09910D8B;
  	data_array[7]= 0x13231A16;
	data_array[8]= 0x2647282C;
  	data_array[9]= 0x4C8E5F58;
	data_array[10]= 0x991C80B1;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00243902;
	data_array[1]= 0x0002C4EF;
  	data_array[2]= 0x8E888E89;
	data_array[3]= 0x1D101D12;
	data_array[4]= 0x3A213A25;
	data_array[5]= 0x7443744B;
	data_array[6]= 0xE886E897;
  	data_array[7]= 0xD10DD12E;
	data_array[8]= 0x88888888;
  	data_array[9]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 10, 1);

  	data_array[0]= 0x00283902;
	data_array[1]= 0x0002C5EF;
  	data_array[2]= 0x8E888E89;
	data_array[3]= 0x1D101D12;
	data_array[4]= 0x3A213A25;
	data_array[5]= 0x7443744B;
	data_array[6]= 0xE886E897;
  	data_array[7]= 0xD10DD12E;
	data_array[8]= 0xA21BA25C;
  	data_array[9]= 0x88888888;
	data_array[10]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 11, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0005C7EF;
  	data_array[2]= 0x5C315C31;
	data_array[3]= 0xB863B862;
	data_array[4]= 0x70C670C5;
	data_array[5]= 0xE18CE18B;
	data_array[6]= 0xC319C317;
  	data_array[7]= 0x8633862F;
	data_array[8]= 0x4C764C5F;
  	data_array[9]= 0x58AD588E;
	data_array[10]= 0x7266703C;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0000C8EF;
  	data_array[2]= 0x00000000;
	data_array[3]= 0x00000000;
	data_array[4]= 0x00000000;
	data_array[5]= 0x00000000;
	data_array[6]= 0x00000000;
  	data_array[7]= 0x00000000;
	data_array[8]= 0x00000000;
  	data_array[9]= 0x00000000;
	data_array[10]= 0x00000000;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0000C9EF;
  	data_array[2]= 0x00000000;
	data_array[3]= 0x00000000;
	data_array[4]= 0x00000000;
	data_array[5]= 0x00000000;
	data_array[6]= 0x00000000;
  	data_array[7]= 0x00000000;
	data_array[8]= 0x00000000;
  	data_array[9]= 0x00000000;
	data_array[10]= 0x00000000;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0000CAEF;
  	data_array[2]= 0x00000000;
	data_array[3]= 0x00000000;
	data_array[4]= 0x00000000;
	data_array[5]= 0x00000000;
	data_array[6]= 0x00000000;
  	data_array[7]= 0x00000000;
	data_array[8]= 0x00000000;
  	data_array[9]= 0x00000000;
	data_array[10]= 0x00000000;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0000CBEF;
  	data_array[2]= 0x00000000;
	data_array[3]= 0x00000000;
	data_array[4]= 0x00000000;
	data_array[5]= 0x00000000;
	data_array[6]= 0x00000000;
  	data_array[7]= 0x00000000;
	data_array[8]= 0x00000000;
  	data_array[9]= 0x00000000;
	data_array[10]= 0x00000000;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00183902;
	data_array[1]= 0x0004CCEF;
  	data_array[2]= 0x300D300C;
	data_array[3]= 0x601A6018;
	data_array[4]= 0xC035C031;
	data_array[5]= 0x88888888;
	data_array[6]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 7, 1);

  	data_array[0]= 0x00073902;
	data_array[1]= 0x3210D3EF;
  	data_array[2]= 0x00107654;
	dsi_set_cmdq(&data_array, 3, 1);

  	data_array[0]= 0x00063902;
	data_array[1]= 0x3210D4EF;
  	data_array[2]= 0x00007654;
	dsi_set_cmdq(&data_array, 3, 1);

  	data_array[0]= 0x00053902;
	data_array[1]= 0x0101D5EF;
  	data_array[2]= 0x00000001;
	dsi_set_cmdq(&data_array, 3, 1);

  	data_array[0]= 0x00033902;
	data_array[1]= 0x0012D6EF;
	dsi_set_cmdq(&data_array, 2, 1);

  	data_array[0]= 0x00063902;
	data_array[1]= 0xC14CDEEF;
  	data_array[2]= 0x00008D52;
	dsi_set_cmdq(&data_array, 3, 1);

  	data_array[0]= 0x00143902;
	data_array[1]= 0xAB00DFEF;
  	data_array[2]= 0x0A0B0C0D;
	data_array[3]= 0x0A0B0C0D;
	data_array[4]= 0x00FF0000;
	data_array[5]= 0x0A0B0C0D;
	dsi_set_cmdq(&data_array, 6, 1);

  	data_array[0]= 0x00303902;
	data_array[1]= 0x0001C3EF;
  	data_array[2]= 0x60996098;
	data_array[3]= 0xC132C331;
	data_array[4]= 0x82648662;
	data_array[5]= 0x04C802C5;
	data_array[6]= 0x0991018B;
  	data_array[7]= 0x13231F16;
	data_array[8]= 0x2647362C;
  	data_array[9]= 0x4C8E5858;
	data_array[10]= 0x991C80B1;
	data_array[11]= 0x88888888;
	data_array[12]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 13, 1);

  	data_array[0]= 0x00343902;
	data_array[1]= 0x0003C6EF;
  	data_array[2]= 0x62DB62DB;
	data_array[3]= 0xC5B6C5B7;
	data_array[4]= 0x8B6C8B6F;
	data_array[5]= 0x16D916DE;
	data_array[6]= 0x2DB32DBC;
  	data_array[7]= 0x5B665B79;
	data_array[8]= 0xB6CDB6F2;
  	data_array[9]= 0x6D9A6DE5;
	data_array[10]= 0xDB34DBCA;
	data_array[11]= 0xB668B795;
	data_array[12]= 0x88888888;
	data_array[13]= 0xFFFFFFFF;
	dsi_set_cmdq(&data_array, 14, 1);


  //********************YYG*******************//
 
 	data_array[0]= 0x00043902;
	data_array[1]= 0x14001fC9;// PWM 20K
	dsi_set_cmdq(&data_array, 2, 1);

	data_array[0] = 0x00352300;//te on
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0xFF512300;//bl mode
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x24532300;//bl mode
	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00552300;//cabc 03
	dsi_set_cmdq(&data_array, 1, 1);
	
MDELAY(180);

	data_array[0] = 0x00110500;	
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(200);

	data_array[0] = 0x00290500;	
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(20);

#endif
#ifdef GPIO_LCM_BL_EN
mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ONE);
#endif
#ifdef GPIO_LCM_LED_EN
mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ONE);
#endif
} 


       
 

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       



static void lcm_suspend(void)
{
#ifdef GPIO_LCM_BL_EN
	mt_set_gpio_mode(GPIO_LCM_BL_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_BL_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_BL_EN, GPIO_OUT_ZERO);
#endif
#ifdef GPIO_LCM_LED_EN
	mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ZERO);
#endif
	SET_RESET_PIN(1);	
	MDELAY(10);	
	SET_RESET_PIN(0);
MDELAY(10);	
//	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
SET_RESET_PIN(1);

MDELAY(120);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
MDELAY(5);
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
MDELAY(250);

}



static void lcm_resume(void)
{

	lcm_init();
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);	
}

static void lcm_setbacklight(unsigned int level)
{
#ifdef BUILD_LK
	dprintf(0,"%s,lk nt35595 backlight: level = %d\n", __func__, level);
#else
	printk("%s, kernel nt35595 backlight: level = %d\n", __func__, level);
#endif
	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = level;
	
	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);

}

LCM_DRIVER otm1285a_hd720_dsi_vdo_tm_lcm_drv = 
{
    .name			= "otm1285a_dsi_vdo_tm",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.init_power		= lcm_init_power,
//  .resume_power = lcm_resume_power,
//  .suspend_power = lcm_suspend_power,
	.set_backlight	= lcm_setbacklight,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};
