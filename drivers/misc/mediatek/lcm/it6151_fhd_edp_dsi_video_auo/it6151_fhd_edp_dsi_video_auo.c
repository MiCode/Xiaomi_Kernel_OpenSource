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

#define FRAME_WIDTH  (1920) //(800)
#define FRAME_HEIGHT (1080) //(1280)

#define LCM_ID_NT35590 (0x90)
// TODO. This LCM ID is NT51012 not 35590.

#define GPIO_LCD_RST_EN      GPIO_EDP_SYSRSTN_PIN	// GPIO64
#define GPIO_LCD_STB_EN      GPIO_EDP_STANDBY_PIN		// GPIO93 
#define GPIO_LCD_LED_EN      GPIO_LCM_LED_EN 		// GPIO17

#define GPIO_LCD_LED_PWM_EN      (GPIO116 | 0x80000000) //GPIO116


// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

//static LCM_UTIL_FUNCS lcm_util = {0};  //for fixed warning issue
static LCM_UTIL_FUNCS lcm_util = 
{
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};


#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   


#define   LCM_DSI_CMD_MODE							0

#if 0
static void init_lcm_registers(void)
{
	unsigned int data_array[16];
		
#if 1

	data_array[0] = 0x00011500;  //software reset					 
	dsi_set_cmdq(data_array, 1, 1);
	
	MDELAY(20);
	
	data_array[0]=0x0bae1500;
	data_array[1]=0xeaee1500;
	data_array[2]=0x5fef1500;
	data_array[3]=0x68f21500;	
	data_array[4]=0x00ee1500;
	data_array[5]=0x00ef1500;
	dsi_set_cmdq(&data_array, 6, 1);

#if 0

	data_array[0] = 0x7DB21500;  					 
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0BAE1500;  					 
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x18B61500;  					 
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0xEAEE1500;  					 
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x5FEF1500;  					 
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x68F21500;  					 
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x00EE1500;  					 
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00EF1500;  					 
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x64D21500;  					 
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00101500;  //sleep out                        
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
#endif

	//data_array[0] = 0xEFB11500;                       
	//dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(1);

	//data_array[0] = 0x00290500;  //display on                        
	//dsi_set_cmdq(data_array, 1, 1);
#endif

#if 0
	data_array[0] = 0x00010500;  //software reset					 
	dsi_set_cmdq(data_array, 1, 1);
	
	MDELAY(20);

	data_array[0] = 0x00023902; 
	data_array[1] = 0x00000BAE; 					 
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0] = 0x00023902; 
	data_array[1] = 0x0000EAEE; 					 
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0] = 0x00023902; 
	data_array[1] = 0x00005FEF; 					 
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0] = 0x00023902; 
	data_array[1] = 0x000068F2; 					 
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0] = 0x00023902; 
	data_array[1] = 0x000000EE; 					 
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0] = 0x00023902; 
	data_array[1] = 0x000000EF; 					 
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(1);
	
	data_array[0] = 0x00100500;  //sleep out                        
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);

	data_array[0] = 0x00290500;  //display on                        
	dsi_set_cmdq(data_array, 1, 1);
#endif
	//MDELAY(5);
}
#endif

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

#if 1
#ifdef BUILD_LK

#define IT6151_BUSNUM                I2C3
#define it6151_SLAVE_ADDR/*it6151_I2C_ADDR*/       0x58  //0x2d //write ;0x5b read


U32 it6151_reg_i2c_write_byte (U8 dev_addr,U8  cmd, U8 data)
{
    U8 cmdBufferLen = 1;
    U8 dataBufferLen = 1;	
    U32 ret_code = I2C_OK;
    U8 write_data[I2C_FIFO_SIZE];
    int transfer_len = 0;
    int i=0, cmdIndex=0, dataIndex=0;
	
    dev_addr = dev_addr<<1;  // for write
    
    transfer_len = cmdBufferLen + dataBufferLen;
    if(I2C_FIFO_SIZE < (cmdBufferLen + dataBufferLen))
    {
        dprintf(CRITICAL, "[it6151_i2c_write] exceed I2C FIFO length!! \n");
        return 0;
    }

    //write_data[0] = cmd;
    //write_data[1] = writeData;

    while(cmdIndex < cmdBufferLen)
    {
        write_data[i] = cmd;
        cmdIndex++;
        i++;
    }

    while(dataIndex < dataBufferLen)
    {
        write_data[i] = data;
        dataIndex++;
        i++;
    }


    /* dump write_data for check */
    for( i=0 ; i < transfer_len ; i++ )
    {
        dprintf(CRITICAL, "[it6151_i2c_write] write_data[%d]=%x\n", i, write_data[i]);
    }

    ret_code = mt_i2c_write(IT6151_BUSNUM, dev_addr, write_data, transfer_len,0);


    return ret_code;
}


static U32 it6151_reg_i2c_read_byte (U8 dev_addr,U8  *cmdBuffer, U8 *dataBuffer)
{
    U32 ret_code = I2C_OK;
    dev_addr = (dev_addr<<1) | 1;  // for read
    ret_code = mt_i2c_write(IT6151_BUSNUM, dev_addr, cmdBuffer, 1, 0);    // set register command
    if (ret_code != I2C_OK)
        return ret_code;

    ret_code = mt_i2c_read(IT6151_BUSNUM, dev_addr, dataBuffer, 1, 0);

    if (ret_code != I2C_OK)
        return ret_code;	

    //dbg_print("[it6151_read_byte] Done\n");

    return ret_code;
}

 //end
 
 /******************************************************************************
 *IIC drvier,:protocol type 2 add by chenguangjian end
 ******************************************************************************/
#else
extern UINT8 it6151_reg_i2c_read_byte(U8 dev_addr,U8  *cmdBuffer, U8 *dataBuffer);
extern void it6151_reg_i2c_write_byte(U8 dev_addr,U8  cmd, U8 data);
#endif
#endif

/////////////////////////////////////////////////////////////////////
///       for it6151 defines start                   ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////


//#define PANEL_RESOLUTION_1280x800_NOUFO
//#define PANEL_RESOLUTION_2048x1536_NOUFO_18B
//#define PANEL_RESOLUTION_2048x1536
// #define PANEL_RESOLUTION_2048x1536_NOUFO // FOR INTEL Platform
// #define PANEL_RESOLUTION_1920x1200p60RB
#define PANEL_RESOLUTION_1920x1080p60





///////////////////////////////////////////////////////////////

//#define DP_I2C_ADDR 	(0xB8)
//#define MIPI_I2C_ADDR 	(0xD8)


///////////////////////////////////////////////////////////////
#define MIPI_4_LANE 	(3)
#define MIPI_3_LANE 	(2)
#define MIPI_2_LANE 	(1)
#define MIPI_1_LANE		(0)

// MIPI Packed Pixel Stream
#define RGB_24b         (0x3E)
#define RGB_30b         (0x0D)
#define RGB_36b         (0x1D)
#define RGB_18b_P       (0x1E)
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

#define B_4_LANE 		(3)
#define B_2_LANE 		(1)
#define B_1_LANE 		(0)

#define B_SSC_ENABLE   	(1)
#define B_SSC_DISABLE   (0)

///////////////////////////////////////////////////////////////////////////
//CONFIGURE
///////////////////////////////////////////////////////////////////////////
#define TRAINING_BITRATE	(B_HBR)
#define DPTX_SSC_SETTING	(B_SSC_DISABLE)
#define MP_MCLK_INV			(0)
#define MP_CONTINUOUS_CLK	(1)
#define MP_LANE_DESKEW		(1)
#define MP_PCLK_DIV			(2)
#define MP_LANE_SWAP		(0)
#define MP_PN_SWAP			(0)

#define DP_PN_SWAP			(0)
#define DP_AUX_PN_SWAP		(0)
#define DP_LANE_SWAP		(0)

#define LVDS_LANE_SWAP		(0)
#define LVDS_PN_SWAP		(0)
#define LVDS_DC_BALANCE		(0)

#define LVDS_6BIT			(0) // '0' for 8 bit, '1' for 6 bit
#define VESA_MAP		    (1) // '0' for JEIDA , '1' for VESA MAP

#define INT_MASK			(7)
///////////////////////////////////////////////////////////////////////
// Global Setting
///////////////////////////////////////////////////////////////////////
#ifdef PANEL_RESOLUTION_1280x800_NOUFO
#define PANEL_WIDTH 1280
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_1920x1080p60
#define PANEL_WIDTH 1920
#define VIC 0x10
#define MP_HPOL 1
#define MP_VPOL 1
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_1920x1200p60RB
#define PANEL_WIDTH 1920
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 1
#define MP_VPOL 0
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_2048x1536
#define PANEL_WIDTH 2048
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define EN_UFO 1
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			0
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_2048x1536_NOUFO
#define PANEL_WIDTH 2048
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_24b
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif

#ifdef PANEL_RESOLUTION_2048x1536_NOUFO_18B
#define PANEL_WIDTH 2048
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define EN_UFO 0
#define MIPI_PACKED_FMT		RGB_18b_P
#define MP_H_RESYNC			1
#define MP_V_RESYNC			0
#endif
///////////////////////////////////////////////////////////////////////////
// MIPI CONFIGURE
///////////////////////////////////////////////////////////////////////////

#ifndef MIPI_LANE_COUNT
    #define MIPI_LANE_COUNT  MIPI_4_LANE
#endif
#ifndef PN_SWAP
    #define PN_SWAP 0
#endif

///////////////////////////////////////////////////////////////////////////

#ifndef DPTX_LANE_COUNT
    // option for lane count
    #define DPTX_LANE_COUNT  B_4_LANE
    // #define DPTX_LANE_COUNT  B_2_LANE
    // #define DPTX_LANE_COUNT  B_1_LANE
#endif


///////////////////////////////////////////////////////////////////////////

#define DP_I2C_ADDR 0x5C
#define MIPI_I2C_ADDR 0x6C

/////////////////////////////////////////////////////////////////////
///       for it6151 defines end                   /////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// Function
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void IT6151_DPTX_init(void)
{	
#ifndef BUILD_LK
		printk("\IT6151_DPTX_init !!!\n");
#else
		dprintf(INFO, "[LK/LCM] IT6151_DPTX_init\n");
#endif	
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x05,0x29);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x05,0x00);
	
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x09,INT_MASK);// Enable HPD_IRQ,HPD_CHG,VIDSTABLE
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x0A,0x00);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x0B,0x00);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xC5,0xC1);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xB5,0x00);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xB7,0x80);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xC4,0xF0);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x06,0xFF);// Clear all interrupt
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x07,0xFF);// Clear all interrupt
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x08,0xFF);// Clear all interrupt
	
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x05,0x00);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x0c,0x08);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x21,0x05);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x3a,0x04);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x5f,0x06);
	//    {DP_I2C_ADDR,0xb5,0xFF,0x80},
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xc9,0xf5);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xca,0x4c);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xcb,0x37);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xd3,0x03);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xd4,0x60);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xe8,0x11);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xec,VIC);

	MDELAY(5);			

	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x23,0x42);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x24,0x07);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x25,0x01);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x26,0x00);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x27,0x10);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x2B,0x05);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x23,0x40);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x22,(DP_AUX_PN_SWAP<<3)|(DP_PN_SWAP<<2)|0x03);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x16,(DPTX_SSC_SETTING<<4)|(DP_LANE_SWAP<<3)|(DPTX_LANE_COUNT<<1)|TRAINING_BITRATE);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x0f,0x01);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x76,0xa7);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x77,0xaf);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x7e,0x8f);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x7f,0x07);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x80,0xef);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x81,0x5f);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x82,0xef);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x83,0x07);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x88,0x38);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x89,0x1f);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x8a,0x48);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x0f,0x00);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x5c,0xf3);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x17,0x04);
	it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x17,0x01);
	MDELAY(5);	
}

int	vit6151_init(void)
{

	unsigned char VenID[2], DevID[2], RevID;
  	unsigned char cmdBuffer;

#ifdef BUILD_LK	
	dprintf(INFO, "[LK/LCM] vit6151_init\n");
#else
	printk("vit6151_init\n");
#endif

//++ DT12
// bug: the display will be disabled when kernel init 
#ifdef BUILD_LK
//-- DT12


#ifdef BUILD_LK
	  cmdBuffer = 0x00;
	  it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &VenID[0]);
	  cmdBuffer = 0x01;
	  it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &VenID[1]);
	  cmdBuffer = 0x02;
	  it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &DevID[0]);
	  cmdBuffer = 0x03;
	  it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &DevID[1]);

 	  cmdBuffer = 0x04;
 	  it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &RevID);	  
#else
	  cmdBuffer = 0x00;
	  VenID[0] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &VenID[0]);
	  cmdBuffer = 0x01;
	  VenID[1] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &VenID[1]);
	  cmdBuffer = 0x02;
	  DevID[0] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &DevID[0]);
	  cmdBuffer = 0x03;
	  DevID[1] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &DevID[1]);
 	  cmdBuffer = 0x04;
 	  RevID = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &RevID);		  	  
#endif

#ifndef BUILD_LK	
	printk("Current DPDevID=%02X%02X\n", DevID[1], DevID[0]);
	printk("Current DPVenID=%02X%02X\n", VenID[1], VenID[0]);
	printk("Current DPRevID=%02X\n\n", RevID);	
#endif
 		
	if( VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x51 && DevID[1]==0x61 ){

#ifndef BUILD_LK	
	printk(" Test 1 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#else
	dprintf(INFO, "[LK/LCM] Test 1 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#endif
		it6151_reg_i2c_write_byte(DP_I2C_ADDR,0x05,0x04);// DP SW Reset
		it6151_reg_i2c_write_byte(DP_I2C_ADDR,0xfd,(MIPI_I2C_ADDR<<1)|1);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x00);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x0c,(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(MIPI_LANE_COUNT<<4)|EN_UFO);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x11,MP_MCLK_INV);
		//{MIPI_I2C_ADDR,0x12,0xFF,0x03},
        if(RevID == 0xA1){			
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x19, MP_LANE_DESKEW); 
		}else{
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x19,(MP_CONTINUOUS_CLK<<1) | MP_LANE_DESKEW); 
		}
		
		//it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x19,0x01);
		
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x27, MIPI_PACKED_FMT);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x28,((PANEL_WIDTH/4-1)>>2)&0xC0);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x29,(PANEL_WIDTH/4-1)&0xFF);
		
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x2e,0x34);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x2f,0x01);
		
		
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x4e,(MP_V_RESYNC<<3)|(MP_H_RESYNC<<2)|(MP_VPOL<<1)|(MP_HPOL));
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x80,(EN_UFO<<5)|MP_PCLK_DIV);
		it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x84,0x8f);

		IT6151_DPTX_init();

		return 0;
	}

#ifndef BUILD_LK	
	printk(" Test 2 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#endif

#ifdef BUILD_LK
	  cmdBuffer = 0x00;
	  it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &VenID[0]);
	  cmdBuffer = 0x01;
	  it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &VenID[1]);
	  cmdBuffer = 0x02;
	  it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &DevID[0]);
	  cmdBuffer = 0x03;
	  it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &DevID[1]);      		

	  cmdBuffer = 0x04;
	  it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &RevID);
#else
	  cmdBuffer = 0x00;
	  VenID[0] = it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &VenID[0]);
	  cmdBuffer = 0x01;
	  VenID[1] = it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &VenID[1]);
	  cmdBuffer = 0x02;
	  DevID[0] = it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &DevID[0]);
	  cmdBuffer = 0x03;
	  DevID[1] = it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &DevID[1]);      		

	  cmdBuffer = 0x04;
	  RevID = it6151_reg_i2c_read_byte(MIPI_I2C_ADDR, &cmdBuffer, &RevID);

#endif

#ifndef BUILD_LK
	printk("Current MPDevID=%02X%02X\n", DevID[1], DevID[0]);
	printk("Current MPVenID=%02X%02X\n", VenID[1], VenID[0]);
	printk("Current MPRevID=%02X\n\n", RevID);
#endif

	if(VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x21 && DevID[1]==0x61 ){
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x33);
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x40);
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x00);
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x0c,(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(MIPI_LANE_COUNT<<4));
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x11, MP_MCLK_INV); 
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x19,(MP_CONTINUOUS_CLK<<1) | MP_LANE_DESKEW);  
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x4E,(MP_V_RESYNC<<3)|(MP_H_RESYNC<<2)|(MP_VPOL<<1)|(MP_HPOL));      
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x72,0x01); 
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x73,0x03); 
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0x80,MP_PCLK_DIV); 
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0xC0,0x13);   
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0xC1,0x01);  
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0xC2,0x47);  
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0xC3,0x67);  
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0xC4,0x04);  
		    it6151_reg_i2c_write_byte(MIPI_I2C_ADDR,0xCB,(LVDS_PN_SWAP<<5)|(LVDS_LANE_SWAP<<4)|(LVDS_6BIT<<2)|(LVDS_DC_BALANCE<<1)| VESA_MAP);  
			return 1;
	}	
	return -1;
//++ DT12
#endif //
//-- DT12	
}

static unsigned int IT6151_ESD_Check(void)
{
#ifndef BUILD_LK
	static 	unsigned char ucIsIT6151=0xFF;
	unsigned char ucReg, ucStat;
  	unsigned char cmdBuffer;

	if(ucIsIT6151==0xFF){
		unsigned char VenID[2], DevID[2];

#ifndef BUILD_LK
		printk("\nIT6151 1st IRQ !!!\n");
#endif		
    	cmdBuffer = 0x00;
    	VenID[0] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &VenID[0]);    
    	cmdBuffer = 0x01;
    	VenID[1] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &VenID[1]); 
    	cmdBuffer = 0x02;
   	DevID[0] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &DevID[0]); 
    	cmdBuffer = 0x03;
    	DevID[1] = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &DevID[1]);             
		//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x00, &VenID[0]);	
		//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x01, &VenID[1]);	
		//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x02, &DevID[0]);	
		//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x03, &DevID[1]);	
	
		//iTE_DEBUG_PRINTF(("Current DevID=%02X%02X\n", DevID[1], DevID[0]));
		//iTE_DEBUG_PRINTF(("Current VenID=%02X%02X\n", VenID[1], VenID[0]));
#ifndef BUILD_LK
		printk("Current DevID=%02X%02X\n", DevID[1], DevID[0]);
		printk("Current VenID=%02X%02X\n", VenID[1], VenID[0]);
#endif
					
		if( VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x51 && DevID[1]==0x61){
				ucIsIT6151 = 1;
		}else{
				ucIsIT6151 = 0;
		}	
	}
	if(ucIsIT6151==1){
		cmdBuffer = 0x21;
   		ucReg = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &ucReg);  
		//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x21, &ucReg);
		if(ucReg & 0x02){
			//iTE_DEBUG_PRINTF(("\nIT6151 Reg0x21=0x%x !!!\n", ucReg));
#ifndef BUILD_LK			
			printk("\nIT6151 Reg0x21=0x%x !!!\n", ucReg);
#endif			
			it6151_reg_i2c_write_byte(DP_I2C_ADDR, 0x21, ucReg);
		}

		cmdBuffer = 0x06;
    		ucReg = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &ucReg); 
		//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x06, &ucReg);
		if(ucReg){
			//iTE_DEBUG_PRINTF(("\nIT6151 Reg0x06=0x%x !!!\n", ucReg));
#ifndef BUILD_LK			
			printk("\nIT6151 Reg0x06=0x%x !!!\n", ucReg);
#endif			
			it6151_reg_i2c_write_byte(DP_I2C_ADDR, 0x06, ucReg);

		  	cmdBuffer = 0x0D;
      			ucStat = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &ucStat); 
			//it6151_reg_i2c_read_byte(DP_I2C_ADDR, 0x0D, &ucStat);
			//iTE_DEBUG_PRINTF(("\nIT6151 Reg0x0D=0x%x !!!\n", ucStat));
#ifndef BUILD_LK			
			printk("\nIT6151 Reg0x0D=0x%x !!!\n", ucStat);			
#endif			
			if(ucReg & 0x03){
				if(ucStat & 0x02){
//					IT6151_DPTX_init();
					return TRUE;
				}
			}else if(ucReg & 0x04){
				if(ucStat & 0x04){
//					IT6151_DPTX_init();
					return TRUE;
				}
			}
		}
	}
	return FALSE;
#endif	
}

static void IT6151_ESD_Recover(void)
{
	unsigned char ucReg, ucStat;
  	unsigned char cmdBuffer;

#ifndef BUILD_LK		
	printk("\nIT6151_ESD_Recover\n");
#endif

	cmdBuffer = 0x06;
    	ucStat = it6151_reg_i2c_read_byte(DP_I2C_ADDR, &cmdBuffer, &ucStat); 	

	IT6151_DPTX_init();
}



static void lcm_get_params(LCM_PARAMS *params)
{

		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

#if 0
		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
#endif

        #if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
        #else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE; //SYNC_PULSE_VDO_MODE; //BURST_VDO_MODE; //;
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.word_count=1920*3; //720*3;	

		params->dsi.ufoe_enable = 0;
			
		params->dsi.vertical_sync_active				= 5;//2; //4; //6; //(12-4-4); //1;
		params->dsi.vertical_backporch					= 4;//1; //4; //6; //4; //6; //10;
		params->dsi.vertical_frontporch					= 36;//2; //4;//6; //4//6; //10;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 44;//8; //40; //44; //(120-40-40); //1;
		params->dsi.horizontal_backporch				= 88;//8; //40;//44; //40; //44; //57;
		params->dsi.horizontal_frontporch				= 148;//29; //40;//44; //40; //44; //32;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		// Bit rate calculation
		//1 Every lane speed
		//params->dsi.pll_div1=1; //0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
		//params->dsi.pll_div2=1;		// div2=0,1,2,3;div1_real=1,2,4,4	
		//params->dsi.fbk_div =22;//31;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	
		//params->dsi.fbk_sel =0;
		//params->dsi.fbk_div =45;
              params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_442;//LCM_DSI_6589_PLL_CLOCK_NULL; //LCM_DSI_6589_PLL_CLOCK_396_5;
    
		//params->dsi.CLK_ZERO = 262; //47;
		//params->dsi.HS_ZERO = 117; //36;

		params->dsi.cont_clock = 1;
		//params->dsi.noncont_clock = TRUE; 
		//params->dsi.noncont_clock_period = 2; // Unit : frames

	       params->dsi.HS_TRAIL = 0x8; //36;
	       params->dsi.HS_ZERO = 0xA; //36;
	       params->dsi.HS_PRPR = 0x6; //36;
	       params->dsi.LPX = 0x5; //36;

	       params->dsi.DA_HS_EXIT = 0x6; //36;
		
}
//extern void DSI_clk_HS_mode(unsigned char enter);
static void lcm_init(void)
{
	//unsigned int data_array[16];
#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] lcm_init() enter\n");

    	dprintf(INFO, "[LK/LCM] enable VGP3 1.2V\n");
	//VGP3 1.2V
	pmic_config_interface(0x41E, 0x1, 0x1, 15); 
	pmic_config_interface(0x43a, 0x0, 0x7, 5);
	
	//mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	//mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	#if 0
	mt_set_gpio_mode(GPIO_LCD_LED_PWM_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_PWM_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_PWM_EN, GPIO_OUT_ONE);
	#endif
	MDELAY(20);
	
	//mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
	//mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ONE);
	MDELAY(20);
#endif

	MDELAY(50); 
	
//++
#if 1
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
#else

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] eDP RST pin...\n");
	#else
		printk("[IT6151] eDP RST pin...\n");
	#endif
	mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);

	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);	
	
#endif
//--

//	MDELAY(150);
    
//	MDELAY(10);
	//DSI_clk_HS_mode(1);
//	  MDELAY(180);
//	MDELAY(220);
	//init_lcm_registers();
	//data_array[0] = 0x55142300;  //turn on bist mode					 
	//dsi_set_cmdq(data_array, 1, 1);
	
//	MDELAY(150);

#if 1//ndef BUILD_LK
//OUTREG32(0xF4000110, INREG32(0xF4000110)&0xFFF7FFFF);
//OUTREG32(0xF400D02C, (INREG32(0xF400D02C)&0xFFFF0000)|0x600);
//OUTREG32(0xF400D01C, (INREG32(0xF400D01C)&0xFFFF0000)|0xC00);
//OUTREG32(0xF400DF50, 0x800);
//OUTREG32(0xF400DF54, 0x600);
//OUTREG32(0xF400D800, (INREG32(0xF400D800)&0xFFFFFFFE));
//OUTREG32(0xF400D804, 5);
//OUTREG32(0xF400DF00, 1);
//OUTREG32(0xF401102C, (INREG32(0xF401102C)&0xFFFFF7FF)|0x1<<11);
//OUTREG32(0xF4011024, (INREG32(0xF4011024)&0xFFFFFFFE));
//OUTREG32(0xF4011024, (INREG32(0xF4011024)&0xFFFFFFFE)|1);
//OUTREG32(0xF4011024, (INREG32(0xF4011024)&0xFFFFFFFE));

//OUTREG32(0xF400D020, 2);
//OUTREG32(0xF400D024, 1);
//OUTREG32(0xF400D028, 2);
//OUTREG32(0xF400D050, 4);
//OUTREG32(0xF400D054, 2);
//OUTREG32(0xF400D058, 0x24);


//OUTREG32(0xF400D110, 0x080A0605);
//OUTREG32(0xF400D114, (INREG32(0xF400D114)&0x00FFFFFF)|0x06000000);


//OUTREG32(0xF0012004, (INREG32(0xF0012004)&0xFFFFF0FF)|0x00000F00);
#endif

/*
0xF4000110[19:19] = 0
0xF400D02C[15: 0] 0x600
0xF400D01C[15: 0] 0xC00
0xF400DF50[31: 0] 0x800
0xF400DF54[31: 0] 0x600
0xF400D800[ 0: 0] 0x0
0xF400D804[31: 0] 0x5
0xF400DF00[31: 0] 0x1
0xF401102C[11:11] 0x1
0xF4011024[ 0: 0] 0x0 -> 0x1 -> 0x0 (toggle)

// H,Vsync porch的設定
0xF400D020[31: 0] 0x2
0xF400D024[31: 0] 0x1
0xF400D028[31: 0] 0x2
0xF400D050[31: 0] = 0x4
0xF400D054[31: 0] = 0x2
0xF400D058[31: 0] = 0x24

// 這個部分程式會依照clock計算
0xF400D110[31: 0] 0x080A0605
0xF400D114[31:24] 0x06

// Impendence 改動
0xF0012004[11: 8] 0x08-->0x0F

// clock 300.4MHz左右
0xF0012058[31: 0] 0x5C800000
0xF0012050[3:  0] 0x3 --> 0x0 --> 0x3

*/

vit6151_init();


}



static void lcm_suspend(void)
{
	//unsigned int data_array[16];

#ifdef BUILD_LK
	dprintf(INFO, "[LK/LCM] lcm_suspend() enter\n");
#else
	printk( "lcm_suspend() enter\n");
#endif
	//data_array[0]=0x00280500; // Display Off
	//dsi_set_cmdq(data_array, 1, 1);
	#if 0
	mt_set_gpio_mode(GPIO_LCD_LED_PWM_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_PWM_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_PWM_EN, GPIO_OUT_ZERO);
	#endif
	MDELAY(20);
	//
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ZERO);
	MDELAY(20);
	
	//data_array[0] = 0x00111500; // Sleep In
	//dsi_set_cmdq(data_array, 1, 1);

//	MDELAY(160);

//	MDELAY(10);
	//DSI_clk_HS_mode(0);
//	  MDELAY(80);
	//SET_RESET_PIN(0);
}


static void lcm_resume(void)
{
#ifdef BUILD_LK
	dprintf(INFO, "[LK/LCM] lcm_resume() enter\n");
	//VGP6 3.3V
	//VGP3 1.2V
	pmic_config_interface(0x41E, 0x1, 0x1, 15); 
	pmic_config_interface(0x43a, 0x0, 0x7, 5);	
		
	//mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	//mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	#if 0
	mt_set_gpio_mode(GPIO_LCD_LED_PWM_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_PWM_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_PWM_EN, GPIO_OUT_ONE);
	#endif
	MDELAY(20);
	MDELAY(20);
		
	//mt_set_gpio_mode(GPIO_LCD_STB_EN, GPIO_MODE_00);
	//mt_set_gpio_dir(GPIO_LCD_STB_EN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ONE);
	MDELAY(20);
#else
	printk( "lcm_resume() enter\n");
#endif

	//unsigned int data_array[16];
	lcm_init();

	//data_array[0] = 0x00101500; // Sleep Out
	//dsi_set_cmdq(data_array, 1, 1);

	//data_array[0] = 0x00290500; // Display On
	//dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(150);

}
         
#if (LCM_DSI_CMD_MODE)
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

	data_array[0]= 0x00290508; //HW bug, so need send one HS packet
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif
#if 0
static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(20); 

	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	
	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0]; //we only need ID
    #ifdef BUILD_LK
		dprintf(INFO, "%s, LK nt35590 debug: nt35590 id = 0x%08x\n", __func__, id);
    #else
		printk("%s, kernel nt35590 horse debug: nt35590 id = 0x%08x\n", __func__, id);
    #endif

    if(id == LCM_ID_NT35590)
    	return 1;
    else
        return 0;


}
#endif
static unsigned int	lcm_esd_check(void)
{

#ifndef BUILD_LK
	printk("lcm_esd_check\n\n");
#endif
	return IT6151_ESD_Check();

}

static unsigned int	lcm_esd_recover(void)
{
#ifndef BUILD_LK
		printk("lcm_esd_recover\n\n");
#endif
	IT6151_ESD_Recover();
	return 0;
}

LCM_DRIVER it6151_fhd_edp_dsi_video_auo_lcm_drv = 
{
    .name			= "it6151_fhd_edp_dsi_video_auo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	//.compare_id     = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
//	.esd_check				= lcm_esd_check,
//	.esd_recover			= lcm_esd_recover,
    };
