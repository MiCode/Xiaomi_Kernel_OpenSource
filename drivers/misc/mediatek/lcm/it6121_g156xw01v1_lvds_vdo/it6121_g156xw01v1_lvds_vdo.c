#ifndef BUILD_LK
#include <linux/string.h>
#else
#include <string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_pmic.h>
	#include <platform/mt_i2c.h>
#elif defined(BUILD_UBOOT)
#else
	#include <mach/mt_gpio.h>
	#include <mach/mt_pm_ldo.h>
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  (1368)
#define FRAME_HEIGHT (768)

#ifdef GPIO_LCM_RST
#define GPIO_LCD_RST_EN      GPIO_LCM_RST
#else
#define GPIO_LCD_RST_EN      0xFFFFFFFF
#endif

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static LCM_UTIL_FUNCS lcm_util = 
{
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

typedef struct
{
  kal_uint8 dev_addr;	
  kal_uint8 addr;
  kal_uint8 data;
}it6121_setting_table;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)																		lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)																			lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   

#define LCM_DSI_CMD_MODE	0

#define MIPI_I2C_ADDR 	(0x6C << 0)
#define REGFLAG_DELAY 	(0xAB)
//#define it6121_DEBUG
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
  memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	if(GPIO == 0xFFFFFFFF)
	{
	#ifdef BUILD_LK
		printf("[LK/LCM] GPIO_LCD_RST_EN =  0x%x \n",GPIO_LCD_RST_EN);	
	#elif (defined BUILD_UBOOT)
	#else	
	#endif
		return;
	}

	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}

#ifdef BUILD_LK
#define IT6121_BUSNUM	I2C0

static kal_uint32 it6121_i2c_write_byte(kal_uint8 dev_addr,kal_uint8 addr, kal_uint8 data)
{
  	kal_uint32 ret_code = I2C_OK;
  	kal_uint8 write_data[I2C_FIFO_SIZE], len;
	struct mt_i2c_t i2c;
	
	i2c.id = IT6121_BUSNUM;
	i2c.addr = dev_addr;
	i2c.mode = ST_MODE;
	i2c.speed = 100;

	write_data[0]= addr;
  	write_data[1] = data;
	len = 2;

	#ifdef it6121_DEBUG
  	/* dump write_data for check */
	printf("[it6121_i2c_write] dev_addr = 0x%x, write_data[0x%x] = 0x%x \n", dev_addr, write_data[0], write_data[1]);
	#endif
	
	ret_code = i2c_write(&i2c, write_data, len);

  	return ret_code;
}

static kal_uint32 it6121_i2c_read_byte(kal_uint8 dev_addr,kal_uint8 addr, kal_uint8 *dataBuffer)
{
  	kal_uint32 ret_code = I2C_OK;
	kal_uint8 len;
	struct mt_i2c_t i2c;
	
	*dataBuffer = addr;

	i2c.id = IT6121_BUSNUM;
	i2c.addr = dev_addr;
	i2c.mode = ST_MODE;
	i2c.speed = 100;
	len = 1;

	ret_code = i2c_write_read(&i2c, dataBuffer, len, len);

	#ifdef it6121_DEBUG
	/* dump write_data for check */
  	printf("[it6121_read_byte] dev_addr = 0x%x, read_data[0x%x] = 0x%x \n", dev_addr, addr, *dataBuffer);
	#endif
	
  	return ret_code;
}
 
 /******************************************************************************
 *IIC drvier,:protocol type 2 add by chenguangjian end
 ******************************************************************************/
#else
extern int it6121_i2c_read_byte(kal_uint8 dev_addr, kal_uint8 addr, kal_uint8 *returnData);
extern int it6121_i2c_write_byte(kal_uint8 dev_addr, kal_uint8 addr, kal_uint8 writeData);
#endif

/////////////////////////////////////////////////////////////////////
///       for it6121 defines start    ///////////////////////////////
/////////////////////////////////////////////////////////////////////
//#define PANEL_RESOLUTION_1280x800_NOUFO
//#define PANEL_RESOLUTION_2048x1536_NOUFO_18B
//#define PANEL_RESOLUTION_2048x1536
//#define PANEL_RESOLUTION_2048x1536_NOUFO
//#define PANEL_RESOLUTION_1920x1200p60RB
//#define PANEL_RESOLUTION_1920x1080p60
//#define PANEL_RESOLUTION_1366x768_2LANE_24B
//#define PANEL_RESOLUTION_1366x768_4LANE_24B
#define PANEL_RESOLUTION_1368x768_4LANE_24B

#define MIPI_4_LANE 	(3)
#define MIPI_3_LANE 	(2)
#define MIPI_2_LANE 	(1)
#define MIPI_1_LANE		(0)

// MIPI Packed Pixel Stream
#define RGB_24b         (0x3E)
#define RGB_30b         (0x0D)
#define RGB_36b         (0x1D)
#define RGB_18b			(0x1E)
#define RGB_18b_L       (0x2E)
#define YCbCr_16b       (0x2C)
#define YCbCr_20b       (0x0C)
#define YCbCr_24b       (0x1C)

// DPTX reg62[3:0]
#define B_DPTXIN_6Bpp   (0)
#define B_DPTXIN_8Bpp   (1)
#define B_DPTXIN_10Bpp  (2)
#define B_DPTXIN_12Bpp  (3)

#define B_LBR    		(1)
#define B_HBR    		(0)

#define DP_4_LANE 		(3)
#define DP_2_LANE 		(1)
#define DP_1_LANE 		(0)

#define B_SSC_ENABLE   	(1)
#define B_SSC_DISABLE   (0)

#define H_Neg			(0)
#define H_Pos			(1)

#define V_Neg			(0)
#define V_Pos			(1)

#define En_UFO			(1)
#define H_ReSync		(1)
///////////////////////////////////////////////////////////////////////////
//CONFIGURE
///////////////////////////////////////////////////////////////////////////
#define TRAINING_BITRATE	(B_HBR)
#define DPTX_SSC_SETTING	(B_SSC_ENABLE)
#define HIGH_PCLK			(1)
#define MP_MCLK_INV			(1)
#define MP_CONTINUOUS_CLK	(1)
#define MP_LANE_DESKEW		(1)
#define MP_LANE_SWAP		(0)
#define MP_PN_SWAP			(0)

#define DP_PN_SWAP			(0)
#define DP_AUX_PN_SWAP		(0)
#define DP_LANE_SWAP		(1)	//(0) our convert board need to LANE SWAP for data lane
#define FRAME_RESYNC		(1)
#define LVDS_LANE_SWAP		(0)
#define LVDS_PN_SWAP		(0)
#define LVDS_DC_BALANCE		(0)

#define LVDS_6BIT			(1) // '0' for 8 bit, '1' for 6 bit
#define VESA_MAP		    (1) // '0' for JEIDA , '1' for VESA MAP

#define INT_MASK			(3)
#define MIPI_RECOVER		(1)

#define MIPI_EVENT_MODE		(1)
#define	MIPI_HSYNC_W		(8)
#define MIPI_VSYNC_W		(2)
#define TIMER_CNT			(0x0A)
///////////////////////////////////////////////////////////////////////
// Global Setting
///////////////////////////////////////////////////////////////////////
struct PanelInfoStr{
	unsigned char	ucVic;// non-Zero value for CEA setting, check the given input format.
	unsigned short	usPWidth;
	unsigned char	ucDpLanes;
	unsigned char	ucMpLanes;
	unsigned char	ucMpHPol;
	unsigned char	ucMpVPol;
	unsigned char	ucUFO;	
	unsigned char	ucMpFmt;
	unsigned char	ucMpHReSync;
	unsigned char	ucMpVReSync;
	unsigned char	ucMpClkDiv;
	unsigned char	ucIntMask;
};

#if defined PANEL_RESOLUTION_1280x800_4LANE_24B
	struct PanelInfoStr sPInfo = {  0, 1280, DP_2_LANE, MIPI_4_LANE, H_Neg, V_Pos, 0, RGB_24b, H_ReSync, 0, 2, 0};
#elif defined PANEL_RESOLUTION_1920x1080p60_4LANE_24B
	struct PanelInfoStr sPInfo = { 16, 1920, DP_2_LANE, MIPI_4_LANE, H_Pos, V_Pos, 0, RGB_24b, H_ReSync, 0, 2, 0};
#elif defined PANEL_RESOLUTION_1920x1200_4LANE_24B
	struct PanelInfoStr sPInfo = {  0, 1920, DP_2_LANE, MIPI_4_LANE, H_Pos, V_Neg, 0, RGB_24b, H_ReSync, 0, 2, 0};
#elif defined PANEL_RESOLUTION_2048x1536_4LANE_24B_UFO
	struct PanelInfoStr sPInfo = {  0, 2048, DP_4_LANE, MIPI_4_LANE, H_Neg, V_Pos, En_UFO, RGB_24b, 0, 0, 2, 0};
#elif defined PANEL_RESOLUTION_2048x1536_4LANE_24B
	struct PanelInfoStr sPInfo = {  0, 2048, DP_4_LANE, MIPI_4_LANE, H_Neg, V_Pos, 0, RGB_24b, H_ReSync, 0, 2, 0};
#elif defined PANEL_RESOLUTION_2048x1536_4LANE_18B
	struct PanelInfoStr sPInfo = {  0, 2048, DP_4_LANE, MIPI_4_LANE, H_Neg, V_Pos, 0, RGB_18b, H_ReSync, 0, 2, 0};
#elif defined PANEL_RESULUTION_1536x2048_4LANE_24B
	struct PanelInfoStr sPInfo = {  0, 1536, DP_4_LANE, MIPI_4_LANE, H_Neg, V_Pos, 0, RGB_24b, H_ReSync, 0, 2, 0};
#elif defined PANEL_RESULUTION_1536x2048_4LANE_24B_UFO
	struct PanelInfoStr sPInfo = {  0, 1536, DP_4_LANE, MIPI_4_LANE, H_Neg, V_Pos, En_UFO, RGB_24b, 0, 0, 2, 0};
#elif defined PANEL_RESOLUTION_1366x768_4LANE_24B
	struct PanelInfoStr sPInfo = {  0, 1366, DP_2_LANE, MIPI_4_LANE, H_Neg, V_Neg, 0, RGB_18b, H_ReSync, 0, 3, 0x11};
#elif defined PANEL_RESOLUTION_1366x768_2LANE_24B
	struct PanelInfoStr sPInfo = {  0, 1366, DP_2_LANE, MIPI_2_LANE, H_Neg, V_Neg, 0, RGB_24b, H_ReSync, 0, 5, 0x11};
#elif defined PANEL_RESOLUTION_1368x768_4LANE_24B
	struct PanelInfoStr sPInfo = {  0, 1368, DP_2_LANE, MIPI_4_LANE, H_Neg, V_Neg, 0, RGB_24b, H_ReSync, 0, 2, 0};
#endif

/////////////////////////////////////////////////////////////////////
///       for it6121 defines end    /////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// Function
/////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

void it6121_MIPI_Init(void)
{
	#ifndef BUILD_LK
		printk("[KE/LCM] it6121_MIPI_Init\n");
	#else
		printf("[LK/LCM] it6121_MIPI_Init\n");
	#endif
	
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x33);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x40);
	if(sPInfo.ucIntMask){
		it6121_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x20);
	}else{
		it6121_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x00);
	}
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x0c,(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(sPInfo.ucMpLanes<<4));
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x11,MP_MCLK_INV);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x19,(MP_CONTINUOUS_CLK<<1) | MP_LANE_DESKEW);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x4B,(FRAME_RESYNC<<4));
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x4E,(sPInfo.ucMpVPol<<1)|(sPInfo.ucMpHPol));
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x72,0x01);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x73,0x03);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x80,sPInfo.ucMpClkDiv);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xC0,(HIGH_PCLK<< 4) | 0x0F);
#if (LVDS_6BIT == 1)
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xC1,0x71);
#else
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xC1,0x01);
#endif
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xC2,0x25);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xC3,0x37);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xC4,0x03);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0xCB,(LVDS_PN_SWAP<<5)|(LVDS_LANE_SWAP<<4)|(LVDS_6BIT<<2)|(LVDS_DC_BALANCE<<1)| VESA_MAP);
#if (MIPI_EVENT_MODE == 1)
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x33,0x80 | MIPI_HSYNC_W >> 8);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x32,MIPI_HSYNC_W);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x3D,0x80 | MIPI_VSYNC_W >> 8);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x3C,MIPI_VSYNC_W);
#endif
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x06,0xFF);
	it6121_i2c_write_byte(MIPI_I2C_ADDR,0x09,sPInfo.ucIntMask);
}

int	it6121_init(void)
{
	unsigned char VenID[2], DevID[2], RevID;

	it6121_i2c_read_byte(MIPI_I2C_ADDR, 0x00, &VenID[0]);
	it6121_i2c_read_byte(MIPI_I2C_ADDR, 0x01, &VenID[1]);
	it6121_i2c_read_byte(MIPI_I2C_ADDR, 0x02, &DevID[0]);
	it6121_i2c_read_byte(MIPI_I2C_ADDR, 0x03, &DevID[1]);
	it6121_i2c_read_byte(MIPI_I2C_ADDR, 0x04, &RevID);

#ifndef BUILD_LK
	printk("Current MPDevID=%02X%02X\n", DevID[1], DevID[0]);
	printk("Current MPVenID=%02X%02X\n", VenID[1], VenID[0]);
	printk("Current MPRevID=%02X\n\n", RevID);
	printk(" Test 2 MIPI_I2C_ADDR=0x%x\n", MIPI_I2C_ADDR);
#endif

	if(VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x21 && DevID[1]==0x61 ){
		it6121_MIPI_Init();
			return 1;
  	}
  	
	return -1;
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
	params->dsi.mode   = SYNC_EVENT_VDO_MODE;	//SYNC_PULSE_VDO_MODE;
  	#endif

	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB666;

	// Highly depends on LCD driver capability.
	params->dsi.packet_size=256;

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count=FRAME_WIDTH*3;
	
	params->dsi.vertical_sync_active				= 8;	// total 38
	params->dsi.vertical_backporch					= 10;
	params->dsi.vertical_frontporch					= 20;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 
	
	params->dsi.horizontal_sync_active				= 5;	// total 200
	params->dsi.horizontal_backporch				= 85;
	params->dsi.horizontal_frontporch				= 110;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

 	params->dsi.PLL_CLOCK = 240;	// 76MHz * 3 *1.05
	params->dsi.cont_clock = 1;
}

static void lcm_power(void)
{
#ifdef BUILD_LK	
	
	printf("[LK/LCM] lcm_power() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ONE);
	MDELAY(20);
#else

	printk("[Kernel/LCM] lcm_power() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(20);
#endif
}

static void lcm_init(void)
{	
#ifdef BUILD_LK			
	printf("[LK/LCM] lcm_init() enter\n");
#else	
	printk("[Kernel/LCM] lcm_init() enter\n");	
#endif

	it6121_init();
}

static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ZERO);
	MDELAY(20);
#else
	printk("[Kernel/LCM] lcm_suspend() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ZERO);
	MDELAY(20);
#endif
}

static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ONE);
	MDELAY(20);
#else
	printk("[Kernel/LCM] lcm_resume() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST_EN,GPIO_OUT_ONE);
	MDELAY(20);
#endif

	lcm_init();
}

LCM_DRIVER it6121_g156xw01v1_lvds_vdo_lcm_drv = 
{
  	.name				= "it6121_g156xw01v1_lvds_vdo",
	.set_util_funcs 	= lcm_set_util_funcs,
	.get_params     	= lcm_get_params,
	.init           	= lcm_init,
	.suspend        	= lcm_suspend,
	.resume         	= lcm_resume,
	.init_power			= lcm_power,
};
