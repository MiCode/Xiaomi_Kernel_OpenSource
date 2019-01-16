#include <linux/string.h>
#ifdef BUILD_UBOOT
#include <asm/arch/mt6573_gpio.h>
#else
#include <mach/mt6573_gpio.h>
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
//HX8363-B DSI

#define FRAME_WIDTH  (320)
#define FRAME_HEIGHT (480)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

LCM_UTIL_FUNCS lcm_util = {0};

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define SET_RESET_PIN(v)                                (lcm_util.set_reset_pin((v)))
#define SET_GPIO_MODE(n, v)                             (lcm_util.set_gpio_mode((n), (v)))
#define SET_GPIO_DIR(n, v)                              (lcm_util.set_gpio_dir((n), (v)))
#define SET_GPIO_PULL_ENABLE(n, v)                      (lcm_util.set_gpio_pull_enable((n), (v)))
#define SET_GPIO_OUT(n, v)                              (lcm_util.set_gpio_out((n), (v)))

#define UDELAY(n)                                       (lcm_util.udelay(n))
#define MDELAY(n)                                       (lcm_util.mdelay(n))

#define dsi_set_cmdq(pdata, queue_size, force_update)	(lcm_util.dsi_set_cmdq(pdata, queue_size, force_update))
#define wrtie_cmd(cmd)									(lcm_util.dsi_write_cmd(cmd))
#define write_regs(addr, pdata, byte_nums)				(lcm_util.dsi_write_regs(addr, pdata, byte_nums))
#define read_reg										(lcm_util.dsi_read_reg())

#define GPIO_LCM_RST_PIN            GPIO138
#define GPIO_LCM_RST_PIN_M_GPIO     GPIO_MODE_00

#define LCM_DSI_CMD_MODE

static void init_lcm_registers(void)
{
    unsigned int data_array[20];
#if 0
#if defined(LCM_DSI_CMD_MODE)
    {
        data_array[0] = 0x00110500; //0x11,exit sleep mode,1byte
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(130);

        data_array[0]=0x00043902;
        data_array[1]=0x1C4107D0; //Power_Setting (D0h),4bytes
        dsi_set_cmdq(&data_array, 2, 1);
        MDELAY(10);

        data_array[0]=0x00043902;
        data_array[1]=0x1B1600D1; //VCOM Control (D1h),4bytes
        dsi_set_cmdq(&data_array, 2, 1);
        MDELAY(10);

        data_array[0]=0x00033902;
        data_array[1]=0x001101D2; //Power_Setting for Normal Mode(D2h),3bytes
        dsi_set_cmdq(&data_array, 2, 1);
        MDELAY(10);

        data_array[0]=0x00063902;
        data_array[1]=0x003B00C0; //Panel Driving Setting (C0h),6bytes
        data_array[2]=0x00000112;
        dsi_set_cmdq(&data_array, 3, 1);
        MDELAY(10);

        data_array[0]=0x00043902;
        data_array[1]=0x221210C1; //set DGC related setting(C1h),4bytes
        dsi_set_cmdq(&data_array, 2, 1);
        MDELAY(10);

        data_array[0]=0x02C51500; //0xC5,2bytes 
        dsi_set_cmdq(&data_array,1,1);
        MDELAY(10);

        data_array[0]=0x000D3902;
        data_array[1]=0x432403C8; //0xC8,13bytes  
        data_array[2]=0x43000807;
        data_array[3]=0x00704735;
        data_array[4]=0x00000000;
        dsi_set_cmdq(&data_array, 5, 1);
        MDELAY(10);

        data_array[0]=0x01F81500; //0xF8,2bytes 
        dsi_set_cmdq(&data_array,1,1);
        MDELAY(10);

        data_array[0]=0x00033902;
        data_array[1]=0x000200FE; //Set SPI Read Index(FEh),3bytes
        dsi_set_cmdq(&data_array, 2, 1);
        MDELAY(10);

        data_array[0] = 0x0A361500; //0x36,set address mode,2bytes
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(10);

        data_array[0] = 0x063A1500; //0x3A,set pixel format,2bytes
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(120);

        data_array[0] = 0x00210500; //0x21,enter inversion mode,1byte
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(10);

        data_array[0] = 0x00290500; //0x29,Display On,1byte
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(10);
    }
#endif
#else
{

		data_array[0]=0x00043902;//Enable external Command
		data_array[1]=0x5783FFB9; 
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(5);
        data_array[0]=0x53B61500; //
        dsi_set_cmdq(&data_array,1,1);
  		
		
        data_array[0] = 0x00110500; //0x11,exit sleep mode,1byte
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(150);

        data_array[0]=0x00350500; //TE on
        dsi_set_cmdq(&data_array,1,1);
		
        data_array[0] = 0x663A1500; //0x3A,set pixel format,2bytes: 18BPP
        dsi_set_cmdq(&data_array, 1, 1);

        //data_array[0] = 0xD0361500; //0x36,set address mode,2bytes
        //dsi_set_cmdq(&data_array, 1, 1);

       // data_array[0] = 0x00210500; //0x21,enter inversion mode,1byte
        //dsi_set_cmdq(&data_array, 1, 1);
		
        data_array[0] = 0x68B01500; //70HZ
        dsi_set_cmdq(&data_array, 1, 1);

		MDELAY(100);//If not  delay before Command 0x44, the command doesn't take effect, i dont why?
        data_array[0] = 0x05CC1500; //set Pannel  soso pannel V inveserse
        dsi_set_cmdq(&data_array, 1, 1);

        data_array[0]=0x00073902;
        data_array[1]=0x1C1500B1;
		data_array[2]=0x0044831C;
        dsi_set_cmdq(&data_array, 3, 1);
		
        data_array[0]=0x00083902;
        data_array[1]=0x004002B4;
		data_array[2]=0x780D2A2A;
        dsi_set_cmdq(&data_array, 3, 1);

        data_array[0]=0x00073902;//Panel Driving Setting (C0h),6byte
        data_array[1]=0x015050C0;
		data_array[2]=0x0008C83C;
        dsi_set_cmdq(&data_array, 3, 1);



		//Gamma
		/*
        data_array[0]=0x00463902;
        data_array[1]=0x300000E0;
		data_array[2]=0x3B003700;
		data_array[3]=0x44003F00;
		data_array[4]=0x59005200;
		data_array[5]=0x3A006000;	
		data_array[6]=0x2F003500;
		data_array[7]=0x22002900;
		data_array[8]=0x19001C00;
		data_array[9]=0x30000300;
		data_array[10]=0x3B003700;	
		data_array[11]=0x44003F00;	
		data_array[12]=0x59005200;
		data_array[13]=0x3A006000;
		data_array[14]=0x2F003500;
		data_array[15]=0x22002900;	
		data_array[16]=0x19001C00;
		data_array[17]=0x00000300;
		data_array[18]=0x00000100;
        dsi_set_cmdq(&data_array, 19, 1);
*/
			


        data_array[0] = 0x00290500; //0x29,Display On,1byte
        dsi_set_cmdq(&data_array, 1, 1);
        MDELAY(25);
    }


#endif
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
#if 1
		   memset(params, 0, sizeof(LCM_PARAMS)); 
		
		   params->type   = LCM_TYPE_DSI; 
#if defined(LCM_DSI_CMD_MODE) 
		   params->dsi.mode   = CMD_MODE; 
#else 
		   params->dsi.mode   = SYNC_EVENT_VDO_MODE; 
#endif 
		   params->ctrl   = LCM_CTRL_PARALLEL_DBI; 
		   //params->ctrl	= LCM_CTRL_NONE; 
		   params->width  = FRAME_WIDTH; 
		   params->height = FRAME_HEIGHT; 
		
		   // DBI 
		   //params->dbi.port					 = 0; 
		   params->dbi.clock_freq			   = LCM_DBI_CLOCK_FREQ_104M; 
		   params->dbi.data_width			   = LCM_DBI_DATA_WIDTH_16BITS; 
		   params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB; 
		   params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST; 
		   params->dbi.data_format.padding	   = LCM_DBI_PADDING_ON_LSB; 
		   params->dbi.data_format.format	   = LCM_DBI_FORMAT_RGB888; 
		   params->dbi.data_format.width	   = LCM_DBI_DATA_WIDTH_24BITS; 
		  params->dbi.cpu_write_bits		  = LCM_DBI_CPU_WRITE_16_BITS; 
		  params->dbi.io_driving_current	  = 0; 
	  
		   // enable tearing-free 
		   //params->dbi.te_mode			  = LCM_DBI_TE_MODE_VSYNC_ONLY; 
		   params->dbi.te_mode			   = LCM_DBI_TE_MODE_DISABLED; 
		   params->dbi.te_edge_polarity 	  = LCM_POLARITY_RISING; 
		
		   // DPI 
		   params->dpi.format			 = LCM_DPI_FORMAT_RGB888; 
		   params->dpi.rgb_order		 = LCM_COLOR_ORDER_RGB; 
		   params->dpi.intermediat_buffer_num = 2; 
		
		   // DSI 
		   params->dsi.DSI_WMEM_CONTI=0x3C; 
			params->dsi.DSI_RMEM_CONTI=0x3E; 
     #if 1
		   params->dsi.LANE_NUM=LCM_ONE_LANE; 
     #else 
		   params->dsi.LANE_NUM=LCM_TWO_LANE; 
     #endif 
		   params->dsi.VC_NUM=0x0; 
		   params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888; 
		   params->dsi.word_count=480*3; 
		
		   params->dsi.vertical_sync_active=2; 
		   params->dsi.vertical_backporch=2; 
		   params->dsi.vertical_frontporch=2; 
		   params->dsi.vertical_active_line=800; 
		
		   params->dsi.line_byte=2180;	   // 2256 = 752*3 
		   params->dsi.horizontal_sync_active_byte=26; 
		   params->dsi.horizontal_backporch_byte=206; 
		   params->dsi.horizontal_frontporch_byte=206;	 
		   params->dsi.rgb_byte=(480*3+6); 
		
		   params->dsi.horizontal_sync_active_word_count=20;	 
		   params->dsi.horizontal_backporch_word_count=200; 
		   params->dsi.horizontal_frontporch_word_count=200; 
	  
	  
	  params->dsi.HS_TRAIL=15;//5; 
	  params->dsi.HS_ZERO=6;
	  params->dsi.HS_PRPR=4;
	  params->dsi.LPX=12;
	  params->dsi.TA_SACK=1;
	  params->dsi.TA_GET=60;
	  params->dsi.TA_SURE=18; 
	  params->dsi.TA_GO=48;
	  params->dsi.CLK_TRAIL=5;
	  params->dsi.CLK_ZERO=15;		
	  params->dsi.LPX_WAIT	  = 0x0A;
	  params->dsi.CONT_DET	  = 0x00;
	  params->dsi.CLK_HS_PRPR=3;
	  
	  params->dsi.pll_div1=0x1a;	  // fref=26MHz, fvco=fref*(div1+1)   (div1=0~63, fvco=500MHZ~1GHz)
	  
	  params->dsi.pll_div2=1;		  // div2=0~15: fout=fvo/(2*div2)

#else
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;
#if defined(LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_EVENT_VDO_MODE;
#endif	
		params->ctrl   = LCM_CTRL_PARALLEL_DBI;
		//params->ctrl   = LCM_CTRL_NONE;
		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;
	
		// DBI
		//params->dbi.port                    = 0;
		params->dbi.clock_freq              = LCM_DBI_CLOCK_FREQ_104M;
		params->dbi.data_width              = LCM_DBI_DATA_WIDTH_16BITS;
		params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
		params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
		params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB888;
		params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_24BITS;
	  params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
	  params->dbi.io_driving_current      = LCM_DRIVING_CURRENT_8MA;

		// enable tearing-free
		//params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
	
		// DPI
		params->dpi.format			  = LCM_DPI_FORMAT_RGB888;
		params->dpi.rgb_order		  = LCM_COLOR_ORDER_RGB;
		params->dpi.intermediat_buffer_num = 2;
	
		// DSI
		params->dsi.DSI_WMEM_CONTI=0x3C;
		params->dsi.DSI_RMEM_CONTI=0x3E;
		params->dsi.LANE_NUM=LCM_ONE_LANE;
		//params->dsi.LANE_NUM=LCM_TWO_LANE;
		params->dsi.VC_NUM=0x0;
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.word_count=320*3;
	
		params->dsi.vertical_sync_active=2;
		params->dsi.vertical_backporch=2;
		params->dsi.vertical_frontporch=2;
		params->dsi.vertical_active_line=480;
	
		params->dsi.line_byte=2180;		// 2256 = 752*3
		params->dsi.horizontal_sync_active_byte=26;
		params->dsi.horizontal_backporch_byte=206;
		params->dsi.horizontal_frontporch_byte=206;	
		params->dsi.rgb_byte=(320*3+6);	
	
		params->dsi.horizontal_sync_active_word_count=20;	
		params->dsi.horizontal_backporch_word_count=200;
		params->dsi.horizontal_frontporch_word_count=200;

		#if 0
/*
		params->dsi.HS_TRAIL=0x14;
		params->dsi.HS_ZERO=0x14;
		params->dsi.HS_PRPR=0x0A;
		params->dsi.LPX=0x05;
	
		params->dsi.TA_SACK=0x01;
		params->dsi.TA_GET=0x37;
		params->dsi.TA_SURE=0x16;	
		params->dsi.TA_GO=0x10;
	
		params->dsi.CLK_TRAIL=0x14;
		params->dsi.CLK_ZERO=0x14;	
		params->dsi.LPX_WAIT=0x0A;
		params->dsi.CONT_DET=0x00;
	
		params->dsi.CLK_HS_PRPR=0x0A;
*/	
		params->dsi.pll_div1=37;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	
		//params->dsi.pll_div2=4;			// div2=0~15: fout=fvo/(2*div2)
		params->dsi.pll_div2=1;			// div2=0~15: fout=fvo/(2*div2)
#endif
#if 1
params->dsi.HS_TRAIL	= 5;
params->dsi.HS_ZERO 	= 6;
params->dsi.HS_PRPR 	= 4;
params->dsi.LPX 		= 12;
params->dsi.TA_SACK 	= 1;
params->dsi.TA_GET		= 60;
params->dsi.TA_SURE 	= 18;	
params->dsi.TA_GO		= 48;
params->dsi.CLK_TRAIL	= 5;
params->dsi.CLK_ZERO	= 15;  
params->dsi.LPX_WAIT	= 0x0A;
params->dsi.CONT_DET	= 0x00;
params->dsi.CLK_HS_PRPR = 3;
params->dsi.pll_div1	= 0x1a; 	 // fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
params->dsi.pll_div2	= 1;		 // div2=0~15: fout=fvo/(2*div2)


params->dsi.HS_TRAIL	= 0x0f;
params->dsi.HS_ZERO 	= 0x08;
params->dsi.HS_PRPR 	= 0x03;
params->dsi.LPX 		= 0x0e;
params->dsi.TA_SACK 	= 0x01;
params->dsi.TA_GET		= 0x46;
params->dsi.TA_SURE 	= 0x15;   
params->dsi.TA_GO		= 0x38;
params->dsi.CLK_TRAIL	= 0x0f;
params->dsi.CLK_ZERO	= 0x13;  
params->dsi.LPX_WAIT	= 0x0a;
params->dsi.CONT_DET	= 0x00;
params->dsi.CLK_HS_PRPR = 0x03;
params->dsi.pll_div1	= 0x1d; 	   // fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
params->dsi.pll_div2	= 0x01; 	   // div2=0~15: fout=fvo/(2*div2)


params->dsi.HS_TRAIL=0x14;
params->dsi.HS_ZERO=0x14;
params->dsi.HS_PRPR=0x0A;
params->dsi.LPX=0x05;

params->dsi.TA_SACK=0x01;
params->dsi.TA_GET=0x37;
params->dsi.TA_SURE=0x16;	
params->dsi.TA_GO=0x10;

params->dsi.CLK_TRAIL=0x14;
params->dsi.CLK_ZERO=0x14;	
params->dsi.LPX_WAIT=0x0A;
params->dsi.CONT_DET=0x00;

params->dsi.CLK_HS_PRPR=0x0A;

params->dsi.pll_div1=37;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)

//params->dsi.pll_div2=4;			// div2=0~15: fout=fvo/(2*div2)
params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)




params->dsi.HS_TRAIL=15;//5; 
params->dsi.HS_ZERO=6;
params->dsi.HS_PRPR=4;
params->dsi.LPX=12;
params->dsi.TA_SACK=1;
params->dsi.TA_GET=60;
params->dsi.TA_SURE=18; 
params->dsi.TA_GO=48;
params->dsi.CLK_TRAIL=5;
params->dsi.CLK_ZERO=15;	  
params->dsi.LPX_WAIT	= 0x0A;
params->dsi.CONT_DET	= 0x00;
params->dsi.CLK_HS_PRPR=3;

params->dsi.pll_div1=0x1a;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)

params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)

#endif

#endif
}

static void lcm_init(void)
{
//SET_GPIO_MODE(GPIO_LCM_RST_PIN, GPIO_LCM_RST_PIN_M_GPIO);
   /// SET_GPIO_MODE(GPIO_LCM_RST_PIN, GPIO_DIR_OUT);
    //SET_GPIO_PULL_ENABLE(GPIO_LCM_RST_PIN, GPIO_PULL_DISABLE);
    SET_RESET_PIN(1); 
    MDELAY(20);

    SET_RESET_PIN(0);
    //SET_GPIO_OUT(GPIO_LCM_RST_PIN, GPIO_OUT_ZERO);
    MDELAY(100);

    SET_RESET_PIN(1);
   // SET_GPIO_OUT(GPIO_LCM_RST_PIN, GPIO_OUT_ONE);
    MDELAY(50);

    init_lcm_registers();
}

static void lcm_suspend(void)
{
    unsigned int data_array[16];

    data_array[0]=0x00280500; // Display Off
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(10); 


    data_array[0] = 0x00100500; // Sleep In
    dsi_set_cmdq(&data_array, 1, 1);
    
    MDELAY(100);
}

static void lcm_resume(void)
{
    unsigned int data_array[16];

/*
    SET_RESET_PIN(1);
    SET_GPIO_OUT(GPIO_LCM_RST_PIN, GPIO_OUT_ONE);
    MDELAY(25);

    SET_RESET_PIN(0);
    SET_GPIO_OUT(GPIO_LCM_RST_PIN, GPIO_OUT_ZERO);
    MDELAY(125);

*/

    data_array[0] = 0x00110500; // Sleep Out
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(200);
    data_array[0] = 0x00290500; // Display On
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(50);
}

static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1 ;

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
    data_array[3]= 0x00053902;
    data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
    data_array[5]= (y1_LSB);
    data_array[6]= 0x002c3909;

    dsi_set_cmdq(&data_array, 7, 0);
#ifndef BUILD_UBOOT
	printk("\n\n\n\n\n\n\n\n\n\n\nx=%x,x0=%x",x,x0);
	printk("\nx1=%x,x10=%x",(x0 + width - 1 ),x1);

#endif
}

#if 0
const LCM_DRIVER* LCM_GetDriver()
{
    static const LCM_DRIVER hx8357c_hvga_dsi_cmd_drv =
    {
		.set_util_funcs = lcm_set_util_funcs,
		.get_params 	= lcm_get_params,
		.init			= lcm_init,
		.suspend		= lcm_suspend,
		.resume 		= lcm_resume,
//#if defined(LCM_DSI_CMD_MODE)
		.update 		= lcm_update,
//#endif
    };

    return &hx8357c_hvga_dsi_cmd_drv;
}
#else
LCM_DRIVER hx8357c_hvga_dsi_cmd_drv = 
{
	.name		= "hx8357c_hvga_dsi_cmd_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params 	= lcm_get_params,
	.init			= lcm_init,
	.suspend		= lcm_suspend,
	.resume 		= lcm_resume,
//#if defined(LCM_DSI_CMD_MODE)
	.update 		= lcm_update,
//#endif

};


#endif
