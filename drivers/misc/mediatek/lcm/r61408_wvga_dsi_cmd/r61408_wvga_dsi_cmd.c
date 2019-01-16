#include <linux/string.h>

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)									lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
       

#define LCM_DSI_CMD_MODE

static void init_lcm_registers(void)
{
	unsigned int data_array[16];
#if defined(LCM_DSI_CMD_MODE)
	{
		data_array[0] = 0x04B02300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);
#if 1
		data_array[0] = 0x00032902;
		data_array[1] = 0x000002B3; 
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(50);

		data_array[0] = 0x00032902;//DSI control
		data_array[1] = 0x008352B6; 
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(50);

		data_array[0] = 0x00052902;//MDDI
		data_array[1] = 0x118000B7; 
		data_array[2] = 0x00000025; 
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x00BD2300; //resizing
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

		data_array[0] = 0x00032902; // panel driving setting 1
		data_array[1] = 0x008702C0; 
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(50);

		data_array[0] = 0x00102902; // panel driving setting 2
		data_array[1] = 0x003543C1; 
		data_array[2] = 0x12222020;
		data_array[3] = 0xA5084426; 
		data_array[4] = 0x0121580F; 
		dsi_set_cmdq(&data_array, 5, 1);
		MDELAY(50);

		data_array[0] = 0x00072902; // V-timing setting
		data_array[1] = 0x060608C2; 
		data_array[2] = 0x00040107;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x00032902; // outline
		data_array[1] = 0x000000C6; 
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(50);

		data_array[0] = 0x00192902; // GAMMA A
		data_array[1] = 0x1E1600C8; 
		data_array[2] = 0x374D3528;
		data_array[3] = 0x040B1422;
		data_array[4] = 0x1E160000;
		data_array[5] = 0x374D3528;
		data_array[6] = 0x040B1422;
		data_array[7] = 0x00000000;
		dsi_set_cmdq(&data_array, 8, 1);
		MDELAY(50);

		data_array[0] = 0x00192902; // GAMMA B
		data_array[1] = 0x1E1600C9; 
		data_array[2] = 0x374D3528;
		data_array[3] = 0x040B1422;
		data_array[4] = 0x1E160000;
		data_array[5] = 0x374D3528;
		data_array[6] = 0x040B1422;
		data_array[7] = 0x00000000;
		dsi_set_cmdq(&data_array, 8, 1);
		MDELAY(50);

		data_array[0] = 0x00192902; // GAMMA C
		data_array[1] = 0x1E1600CA; 
		data_array[2] = 0x374D3528;
		data_array[3] = 0x040B1422;
		data_array[4] = 0x1E160000;
		data_array[5] = 0x374D3528;
		data_array[6] = 0x040B1422;
		data_array[7] = 0x00000000;
		dsi_set_cmdq(&data_array, 8, 1);
		MDELAY(50);

		data_array[0] = 0x00112902; // Power setting
		data_array[1] = 0xBD03A9D0; 
		data_array[2] = 0x20720CA5;
		data_array[3] = 0x01000110;
		data_array[4] = 0x01030001;
		data_array[5] = 0x00000000;
		dsi_set_cmdq(&data_array, 6, 1);
		MDELAY(50);

		data_array[0] = 0x00082902; // Power setting
		data_array[1] = 0x230C18D1; 
		data_array[2] = 0x50027503;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x33D32300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

		data_array[0] = 0x00032902;
		data_array[1] = 0x001B1BD5;
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(50);

		data_array[0] = 0xA8D62300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

		data_array[0] = 0x00032902;
		data_array[1] = 0x000001DE;
		dsi_set_cmdq(&data_array, 2, 1);
		MDELAY(50);

		data_array[0] = 0x00052902;
		data_array[1] = 0x000000E0;
		data_array[2] = 0x00000000;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x00072902;
		data_array[1] = 0x010101E1;
		data_array[2] = 0x00000101;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x00e62300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

		data_array[0] = 0x03FA2300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);
#endif
		data_array[0] = 0x00053902;
		data_array[1] = 0x0300002B;
		data_array[2] = 0x00000055;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x00053902;
		data_array[1] = 0x0100002A;
		data_array[2] = 0x000000DF;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x00361500;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

		data_array[0] = 0x773A1500;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

//		data_array[0] = 0x03B02300;
//		dsi_set_cmdq(&data_array, 1, 1);
#if 0
		data_array[0] = 0x28D60500;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);

		data_array[0] = 0x000E2902;
		data_array[1] = 0x700000FD;
		data_array[2] = 0x34313200;
		data_array[3] = 0x04313230;
		data_array[4] = 0x00000000;
		dsi_set_cmdq(&data_array, 5, 1);
		MDELAY(50);

		data_array[0] = 0x00052902;
		data_array[1] = 0x000000FE;
		data_array[2] = 0x00000020;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);	
/*
		data_array[0] = 0x00112902; // Power setting
		data_array[1] = 0xBD03A9D0; 
		data_array[2] = 0x20720CA5;
		data_array[3] = 0x01000110;
		data_array[4] = 0x01030001;
		data_array[5] = 0x00000000;
		dsi_set_cmdq(&data_array, 6, 1);
		MDELAY(50);

		data_array[0] = 0x00082902; // Power setting
		data_array[1] = 0x230C18D1; 
		data_array[2] = 0x50027503;
		dsi_set_cmdq(&data_array, 3, 1);
		MDELAY(50);

		data_array[0] = 0x33D32300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);
*/
#endif
		data_array[0] = 0x00110500;
		dsi_set_cmdq(&data_array, 1, 1);
		
		MDELAY(125);
		
		data_array[0] = 0x00290500;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(125);

		data_array[0] = 0x03B02300;
		dsi_set_cmdq(&data_array, 1, 1);
		MDELAY(50);
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
	  params->dbi.io_driving_current      = 0;

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
		//params->dsi.LANE_NUM=LCM_ONE_LANE;
		params->dsi.LANE_NUM=LCM_TWO_LANE;
		params->dsi.VC_NUM=0x0;
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.word_count=480*3;
	
		params->dsi.vertical_sync_active=2;
		params->dsi.vertical_backporch=2;
		params->dsi.vertical_frontporch=2;
		params->dsi.vertical_active_line=800;
	
		params->dsi.line_byte=2180;		// 2256 = 752*3
		params->dsi.horizontal_sync_active_byte=26;
		params->dsi.horizontal_backporch_byte=206;
		params->dsi.horizontal_frontporch_byte=206;	
		params->dsi.rgb_byte=(480*3+6);	
	
		params->dsi.horizontal_sync_active_word_count=20;	
		params->dsi.horizontal_backporch_word_count=200;
		params->dsi.horizontal_frontporch_word_count=200;
	
/*		params->dsi.HS_TRAIL=0x14;
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
		params->dsi.HS_TRAIL=0x0C;
		params->dsi.HS_ZERO=0x04;
		params->dsi.HS_PRPR=0x02;
		params->dsi.LPX=0x06;
	
		params->dsi.TA_SACK=0x01;
		params->dsi.TA_GET=0x1E;
		params->dsi.TA_SURE=0x09;	
		params->dsi.TA_GO=0x18;
	
		params->dsi.CLK_TRAIL=0x01;
		params->dsi.CLK_ZERO=0x06;	
		params->dsi.LPX_WAIT=0x0A;
		params->dsi.CONT_DET=0x00;
	
		params->dsi.CLK_HS_PRPR=0x01;

		params->dsi.pll_div1=0x1D;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	
		//params->dsi.pll_div2=4;			// div2=0~15: fout=fvo/(2*div2)
		params->dsi.pll_div2=1;			// div2=0~15: fout=fvo/(2*div2)

}

static void lcm_init(void)
{
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);

    init_lcm_registers();
	MDELAY(500);
//	clear_panel();
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

	data_array[0] = 0x00110500; // Sleep Out
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(100);
	data_array[0] = 0x00290500; // Display On
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(10);
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
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;
//	data_array[0]= 0x002c3901;

	dsi_set_cmdq(&data_array, 7, 0);

}

LCM_DRIVER r61408_wvga_dsi_cmd_drv = 
{
    .name			= "r61408",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if defined(LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
    };
