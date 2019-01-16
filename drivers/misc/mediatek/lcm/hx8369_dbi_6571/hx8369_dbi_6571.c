
#ifdef BUILD_LK
#include <stdio.h>
#include <string.h>
#else
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)
#define LCM_ID       (0x69)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef BUILD_LK
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

static __inline void send_ctrl_cmd(unsigned int cmd)
{
   lcm_util.send_cmd(cmd);
}


static __inline void send_data_cmd(unsigned int data)
{
   lcm_util.send_data(data&0xff);
}


static __inline unsigned int read_data_cmd(void)
{
   return 0xFF&lcm_util.read_data();
}


static __inline void set_lcm_register(unsigned int regIndex,
                                                            unsigned int regData)
{
   send_ctrl_cmd(regIndex);
   send_data_cmd(regData);
}


static void sw_clear_panel(unsigned int color)
{
   unsigned short x0, y0, x1, y1, x, y;
   unsigned short h_X_start,l_X_start,h_X_end,l_X_end,h_Y_start,l_Y_start,h_Y_end,l_Y_end;
   
   x0 = (unsigned short)0;
   y0 = (unsigned short)0;
   x1 = (unsigned short)FRAME_WIDTH-1;
   y1 = (unsigned short)FRAME_HEIGHT-1;
   
   h_X_start=((x0&0xFF00)>>8);
   l_X_start=(x0&0x00FF);
   h_X_end=((x1&0xFF00)>>8);
   l_X_end=(x1&0x00FF);
   
   h_Y_start=((y0&0xFF00)>>8);
   l_Y_start=(y0&0x00FF);
   h_Y_end=((y1&0xFF00)>>8);
   l_Y_end=(y1&0x00FF);
   
   send_ctrl_cmd(0x2A);
   send_data_cmd(h_X_start); 
   send_data_cmd(l_X_start); 
   send_data_cmd(h_X_end); 
   send_data_cmd(l_X_end); 
   
   send_ctrl_cmd(0x2B);
   send_data_cmd(h_Y_start); 
   send_data_cmd(l_Y_start); 
   send_data_cmd(h_Y_end); 
   send_data_cmd(l_Y_end); 
   
   for (y = y0; y <= y1; ++ y) {
      for (x = x0; x <= x1; ++ x) {
         lcm_util.send_data(color);
      }
   }
}


static void init_lcm_registers(void)
{
   send_ctrl_cmd(0xB9);  // SET password
   send_data_cmd(0xFF);  
   send_data_cmd(0x83);  
   send_data_cmd(0x69);
   
   send_ctrl_cmd(0xB1);  //Set Power
   send_data_cmd(0x85);
   send_data_cmd(0x00);
   send_data_cmd(0x34);
   send_data_cmd(0x0A);
   send_data_cmd(0x00);
   send_data_cmd(0x0F);
   send_data_cmd(0x0F);
   send_data_cmd(0x2A);
   send_data_cmd(0x32);
   send_data_cmd(0x3F);
   send_data_cmd(0x3F);
   send_data_cmd(0x01);
   send_data_cmd(0x23);
   send_data_cmd(0x01);
   send_data_cmd(0xE6);
   send_data_cmd(0xE6);
   send_data_cmd(0xE6);
   send_data_cmd(0xE6);
   send_data_cmd(0xE6);
   
   send_ctrl_cmd(0xB2);  // SET Display  480x800
   send_data_cmd(0x00);  
   send_data_cmd(0x20);  
   send_data_cmd(0x03);  
   send_data_cmd(0x03);  
   send_data_cmd(0x70);  
   send_data_cmd(0x00);  
   send_data_cmd(0xFF);  
   send_data_cmd(0x06);  
   send_data_cmd(0x00);  
   send_data_cmd(0x00);  
   send_data_cmd(0x00);  
   send_data_cmd(0x03);  
   send_data_cmd(0x03);  
   send_data_cmd(0x00);  
   send_data_cmd(0x01);  
   
   send_ctrl_cmd(0xB4);  // SET Display  column inversion
   send_data_cmd(0x00);  
   send_data_cmd(0x18);  
   send_data_cmd(0x80);  
   send_data_cmd(0x06);  
   send_data_cmd(0x02);  
   
   send_ctrl_cmd(0xB6);  // SET VCOM
   send_data_cmd(0x3A);  
   send_data_cmd(0x3A);  
   
   send_ctrl_cmd(0xD5);
   send_data_cmd(0x00);
   send_data_cmd(0x02);
   send_data_cmd(0x03);
   send_data_cmd(0x00);
   send_data_cmd(0x01);
   send_data_cmd(0x03);
   send_data_cmd(0x28);
   send_data_cmd(0x70);
   send_data_cmd(0x01);
   send_data_cmd(0x03);
   send_data_cmd(0x00);
   send_data_cmd(0x00);
   send_data_cmd(0x40);
   send_data_cmd(0x06);
   send_data_cmd(0x51);
   send_data_cmd(0x07);
   send_data_cmd(0x00);
   send_data_cmd(0x00);
   send_data_cmd(0x41);
   send_data_cmd(0x06);
   send_data_cmd(0x50);
   send_data_cmd(0x07);
   send_data_cmd(0x07);
   send_data_cmd(0x0F);
   send_data_cmd(0x04);
   send_data_cmd(0x00);
   
   send_ctrl_cmd(0xE0); // Set Gamma
   send_data_cmd(0x00);  
   send_data_cmd(0x13);  
   send_data_cmd(0x19);  
   send_data_cmd(0x38);  
   send_data_cmd(0x3D);  
   send_data_cmd(0x3F);  
   send_data_cmd(0x28);  
   send_data_cmd(0x46);  
   send_data_cmd(0x07);  
   send_data_cmd(0x0D);  
   send_data_cmd(0x0E);  
   send_data_cmd(0x12);  
   send_data_cmd(0x15);  
   send_data_cmd(0x12);  
   send_data_cmd(0x14);  
   send_data_cmd(0x0F);  
   send_data_cmd(0x17);  
   send_data_cmd(0x00);  
   send_data_cmd(0x13);  
   send_data_cmd(0x19);  
   send_data_cmd(0x38);  
   send_data_cmd(0x3D);  
   send_data_cmd(0x3F);  
   send_data_cmd(0x28);  
   send_data_cmd(0x46);  
   send_data_cmd(0x07);  
   send_data_cmd(0x0D);  
   send_data_cmd(0x0E);  
   send_data_cmd(0x12);  
   send_data_cmd(0x15);  
   send_data_cmd(0x12);  
   send_data_cmd(0x14);  
   send_data_cmd(0x0F);  
   send_data_cmd(0x17);  
   
   send_ctrl_cmd(0xC1); // Set DGC
   send_data_cmd(0x01);  
   send_data_cmd(0x04);  
   send_data_cmd(0x13);  
   send_data_cmd(0x1A);  
   send_data_cmd(0x20);  
   send_data_cmd(0x27);  
   send_data_cmd(0x2C);  
   send_data_cmd(0x32);  
   send_data_cmd(0x36);  
   send_data_cmd(0x3F);  
   send_data_cmd(0x47);  
   send_data_cmd(0x50);  
   send_data_cmd(0x59);  
   send_data_cmd(0x60);  
   send_data_cmd(0x68);  
   send_data_cmd(0x71);  
   send_data_cmd(0x7B);  
   send_data_cmd(0x82);  
   send_data_cmd(0x89);  
   send_data_cmd(0x91);  
   send_data_cmd(0x98);  
   send_data_cmd(0xA0);  
   send_data_cmd(0xA8);  
   send_data_cmd(0xB0);  
   send_data_cmd(0xB8);  
   send_data_cmd(0xC1);  
   send_data_cmd(0xC9);  
   send_data_cmd(0xD0);  
   send_data_cmd(0xD7);  
   send_data_cmd(0xE0);  
   send_data_cmd(0xE7);  
   send_data_cmd(0xEF);  
   send_data_cmd(0xF7);  
   send_data_cmd(0xFE);  
   send_data_cmd(0xCF);  
   send_data_cmd(0x52);  
   send_data_cmd(0x34);  
   send_data_cmd(0xF8);  
   send_data_cmd(0x51);  
   send_data_cmd(0xF5);  
   send_data_cmd(0x9D);  
   send_data_cmd(0x75);  
   send_data_cmd(0x00);  
   send_data_cmd(0x04);  
   send_data_cmd(0x13);  
   send_data_cmd(0x1A);  
   send_data_cmd(0x20);  
   send_data_cmd(0x27);  
   send_data_cmd(0x2C);  
   send_data_cmd(0x32);  
   send_data_cmd(0x36);  
   send_data_cmd(0x3F);  
   send_data_cmd(0x47);  
   send_data_cmd(0x50);  
   send_data_cmd(0x59);  
   send_data_cmd(0x60);  
   send_data_cmd(0x68);  
   send_data_cmd(0x71);  
   send_data_cmd(0x7B);  
   send_data_cmd(0x82);  
   send_data_cmd(0x89);  
   send_data_cmd(0x91);  
   send_data_cmd(0x98);  
   send_data_cmd(0xA0);  
   send_data_cmd(0xA8);  
   send_data_cmd(0xB0);  
   send_data_cmd(0xB8);  
   send_data_cmd(0xC1); 
   send_data_cmd(0xC9);  
   send_data_cmd(0xD0);  
   send_data_cmd(0xD7);  
   send_data_cmd(0xE0);  
   send_data_cmd(0xE7);  
   send_data_cmd(0xEF);  
   send_data_cmd(0xF7);  
   send_data_cmd(0xFE);  
   send_data_cmd(0xCF);  
   send_data_cmd(0x52);  
   send_data_cmd(0x34);  
   send_data_cmd(0xF8);  
   send_data_cmd(0x51);  
   send_data_cmd(0xF5);  
   send_data_cmd(0x9D);  
   send_data_cmd(0x75);  
   send_data_cmd(0x00);  
   send_data_cmd(0x04);  
   send_data_cmd(0x13);  
   send_data_cmd(0x1A);  
   send_data_cmd(0x20);  
   send_data_cmd(0x27);  
   send_data_cmd(0x2C);  
   send_data_cmd(0x32);  
   send_data_cmd(0x36);  
   send_data_cmd(0x3F);  
   send_data_cmd(0x47);  
   send_data_cmd(0x50);  
   send_data_cmd(0x59);  
   send_data_cmd(0x60);  
   send_data_cmd(0x68);  
   send_data_cmd(0x71);  
   send_data_cmd(0x7B);  
   send_data_cmd(0x82); 
   send_data_cmd(0x89);  
   send_data_cmd(0x91);  
   send_data_cmd(0x98);  
   send_data_cmd(0xA0);  
   send_data_cmd(0xA8);  
   send_data_cmd(0xB0);  
   send_data_cmd(0xB8);  
   send_data_cmd(0xC1);  
   send_data_cmd(0xC9);  
   send_data_cmd(0xD0);  
   send_data_cmd(0xD7);  
   send_data_cmd(0xE0);  
   send_data_cmd(0xE7);  
   send_data_cmd(0xEF);  
   send_data_cmd(0xF7);  
   send_data_cmd(0xFE);  
   send_data_cmd(0xCF);  
   send_data_cmd(0x52);  
   send_data_cmd(0x34);  
   send_data_cmd(0xF8);  
   send_data_cmd(0x51);  
   send_data_cmd(0xF5);  
   send_data_cmd(0x9D);  
   send_data_cmd(0x75);  
   send_data_cmd(0x00);

   send_ctrl_cmd(0x2D);
   send_data_cmd(0x00);
   send_data_cmd(0x04);
   send_data_cmd(0x08);
   send_data_cmd(0x0c);
   send_data_cmd(0x10);
   send_data_cmd(0x14);
   send_data_cmd(0x18);
   send_data_cmd(0x1c);
   send_data_cmd(0x20);
   send_data_cmd(0x24);
   send_data_cmd(0x28);
   send_data_cmd(0x2c);
   send_data_cmd(0x30);
   send_data_cmd(0x34);
   send_data_cmd(0x38);
   send_data_cmd(0x3c);
   send_data_cmd(0x41);
   send_data_cmd(0x45);
   send_data_cmd(0x49);
   send_data_cmd(0x4d);
   send_data_cmd(0x51);
   send_data_cmd(0x55);
   send_data_cmd(0x59);
   send_data_cmd(0x5d);
   send_data_cmd(0x61);
   send_data_cmd(0x65);
   send_data_cmd(0x69);
   send_data_cmd(0x6d);
   send_data_cmd(0x71);
   send_data_cmd(0x75);
   send_data_cmd(0x79);
   send_data_cmd(0x7d);
   send_data_cmd(0x82);
   send_data_cmd(0x86);
   send_data_cmd(0x8a);
   send_data_cmd(0x8e);
   send_data_cmd(0x92);
   send_data_cmd(0x96);
   send_data_cmd(0x9a);
   send_data_cmd(0x9e);
   send_data_cmd(0xa2);
   send_data_cmd(0xa6);
   send_data_cmd(0xaa);
   send_data_cmd(0xae);
   send_data_cmd(0xb2);
   send_data_cmd(0xb6);
   send_data_cmd(0xba);
   send_data_cmd(0xbe);
   send_data_cmd(0xc3);
   send_data_cmd(0xc7);
   send_data_cmd(0xcb);
   send_data_cmd(0xcf);
   send_data_cmd(0xd3);
   send_data_cmd(0xd7);
   send_data_cmd(0xdb);
   send_data_cmd(0xdf);
   send_data_cmd(0xe3);
   send_data_cmd(0xe7);
   send_data_cmd(0xeb);
   send_data_cmd(0xef);
   send_data_cmd(0xf3);
   send_data_cmd(0xf7);
   send_data_cmd(0xfb);
   send_data_cmd(0xff);
   send_data_cmd(0x00);
   send_data_cmd(0x04);
   send_data_cmd(0x08);
   send_data_cmd(0x0c);
   send_data_cmd(0x10);
   send_data_cmd(0x14);
   send_data_cmd(0x18);
   send_data_cmd(0x1c);
   send_data_cmd(0x20);
   send_data_cmd(0x24);
   send_data_cmd(0x28);
   send_data_cmd(0x2c);
   send_data_cmd(0x30);
   send_data_cmd(0x34);
   send_data_cmd(0x38);
   send_data_cmd(0x3c);
   send_data_cmd(0x41);
   send_data_cmd(0x45);
   send_data_cmd(0x49);
   send_data_cmd(0x4d);
   send_data_cmd(0x51);
   send_data_cmd(0x55);
   send_data_cmd(0x59);
   send_data_cmd(0x5d);
   send_data_cmd(0x61);
   send_data_cmd(0x65);
   send_data_cmd(0x69);
   send_data_cmd(0x6d);
   send_data_cmd(0x71);
   send_data_cmd(0x75);
   send_data_cmd(0x79);
   send_data_cmd(0x7d);
   send_data_cmd(0x82);
   send_data_cmd(0x86);
   send_data_cmd(0x8a);
   send_data_cmd(0x8e);
   send_data_cmd(0x92);
   send_data_cmd(0x96);
   send_data_cmd(0x9a);
   send_data_cmd(0x9e);
   send_data_cmd(0xa2);
   send_data_cmd(0xa6);
   send_data_cmd(0xaa);
   send_data_cmd(0xae);
   send_data_cmd(0xb2);
   send_data_cmd(0xb6);
   send_data_cmd(0xba);
   send_data_cmd(0xbe);
   send_data_cmd(0xc3);
   send_data_cmd(0xc7);
   send_data_cmd(0xcb);
   send_data_cmd(0xcf);
   send_data_cmd(0xd3);
   send_data_cmd(0xd7);
   send_data_cmd(0xdb);
   send_data_cmd(0xdf);
   send_data_cmd(0xe3);
   send_data_cmd(0xe7);
   send_data_cmd(0xeb);
   send_data_cmd(0xef);
   send_data_cmd(0xf3);
   send_data_cmd(0xf7);
   send_data_cmd(0xfb);
   send_data_cmd(0xff);
   send_data_cmd(0x00);
   send_data_cmd(0x04);
   send_data_cmd(0x08);
   send_data_cmd(0x0c);
   send_data_cmd(0x10);
   send_data_cmd(0x14);
   send_data_cmd(0x18);
   send_data_cmd(0x1c);
   send_data_cmd(0x20);
   send_data_cmd(0x24);
   send_data_cmd(0x28);
   send_data_cmd(0x2c);
   send_data_cmd(0x30);
   send_data_cmd(0x34);
   send_data_cmd(0x38);
   send_data_cmd(0x3c);
   send_data_cmd(0x41);
   send_data_cmd(0x45);
   send_data_cmd(0x49);
   send_data_cmd(0x4d);
   send_data_cmd(0x51);
   send_data_cmd(0x55);
   send_data_cmd(0x59);
   send_data_cmd(0x5d);
   send_data_cmd(0x61);
   send_data_cmd(0x65);
   send_data_cmd(0x69);
   send_data_cmd(0x6d);
   send_data_cmd(0x71);
   send_data_cmd(0x75);
   send_data_cmd(0x79);
   send_data_cmd(0x7d);
   send_data_cmd(0x82);
   send_data_cmd(0x86);
   send_data_cmd(0x8a);
   send_data_cmd(0x8e);
   send_data_cmd(0x92);
   send_data_cmd(0x96);
   send_data_cmd(0x9a);
   send_data_cmd(0x9e);
   send_data_cmd(0xa2);
   send_data_cmd(0xa6);
   send_data_cmd(0xaa);
   send_data_cmd(0xae);
   send_data_cmd(0xb2);
   send_data_cmd(0xb6);
   send_data_cmd(0xba);
   send_data_cmd(0xbe);
   send_data_cmd(0xc3);
   send_data_cmd(0xc7);
   send_data_cmd(0xcb);
   send_data_cmd(0xcf);
   send_data_cmd(0xd3);
   send_data_cmd(0xd7);
   send_data_cmd(0xdb);
   send_data_cmd(0xdf);
   send_data_cmd(0xe3);
   send_data_cmd(0xe7);
   send_data_cmd(0xeb);
   send_data_cmd(0xef);
   send_data_cmd(0xf3);
   send_data_cmd(0xf7);
   send_data_cmd(0xfb);
   send_data_cmd(0xff);
 
   send_ctrl_cmd(0x36);  
   send_data_cmd(0x00);   // normal direction

   send_ctrl_cmd(0x35);  
   send_data_cmd(0x01);   // TE on
   
   send_ctrl_cmd(0x3A);  // set Interface Pixel Format
   send_data_cmd(0x05);   // 0x07=24 Bit/Pixel; 0x06=18 Bit/Pixel; 0x05=16 Bit/Pixel
    
   send_ctrl_cmd(0x51);//write display brightness
   send_data_cmd(0xff);//set brightness 0x00-0xff
   MDELAY(50);
   
   send_ctrl_cmd(0x53);//write ctrl display
   send_data_cmd(0x24);
   MDELAY(50);
   
   send_ctrl_cmd(0x55);
   send_data_cmd(0x02);//still picture
   MDELAY(50);
   
   send_ctrl_cmd(0x5e);//write CABC minumum brightness
   send_data_cmd(0x70);//
   MDELAY(50);
   
   send_ctrl_cmd(0x11); 	
   MDELAY(120);
   send_ctrl_cmd(0x29); 
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

    params->type = LCM_TYPE_DBI;
    params->ctrl = LCM_CTRL_PARALLEL_DBI;
    params->width = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dbi.port = 0;
    params->dbi.data_width = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format = LCM_DBI_FORMAT_RGB565;
    params->dbi.data_format.width = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.cpu_write_bits = LCM_DBI_CPU_WRITE_8_BITS;
    params->dbi.io_driving_current = LCM_DRIVING_CURRENT_8MA;

    params->dbi.parallel.write_setup = 1;
    params->dbi.parallel.write_hold = 1;
    params->dbi.parallel.write_wait = 3;
    params->dbi.parallel.read_setup = 3;
    params->dbi.parallel.read_hold = 0;
    params->dbi.parallel.read_latency = 20;
    params->dbi.parallel.wait_period = 0;
    params->dbi.parallel.cs_high_width = 0;

    params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity = LCM_POLARITY_RISING;
}


static void lcm_init(void)
{
   SET_RESET_PIN(1);
   SET_RESET_PIN(0);
   MDELAY(25);
   SET_RESET_PIN(1);
   MDELAY(50);

   init_lcm_registers();
}


static void lcm_suspend(void)
{
   send_ctrl_cmd(0x28);
   send_ctrl_cmd(0x10);
   MDELAY(5);
}


static void lcm_resume(void)
{
   send_ctrl_cmd(0x11);
   MDELAY(120);
   send_ctrl_cmd(0x29);
}


static void lcm_update(unsigned int x, unsigned int y,
                                       unsigned int width, unsigned int height)
{
   unsigned short x0, y0, x1, y1;
   unsigned short h_X_start,l_X_start,h_X_end,l_X_end,h_Y_start,l_Y_start,h_Y_end,l_Y_end;


   x0 = (unsigned short)x;
   y0 = (unsigned short)y;
   x1 = (unsigned short)x+width-1;
   y1 = (unsigned short)y+height-1;
   
   h_X_start=((x0&0xFF00)>>8);
   l_X_start=(x0&0x00FF);
   h_X_end=((x1&0xFF00)>>8);
   l_X_end=(x1&0x00FF);
   
   h_Y_start=((y0&0xFF00)>>8);
   l_Y_start=(y0&0x00FF);
   h_Y_end=((y1&0xFF00)>>8);
   l_Y_end=(y1&0x00FF);
   
   send_ctrl_cmd(0x2A);
   send_data_cmd(h_X_start); 
   send_data_cmd(l_X_start); 
   send_data_cmd(h_X_end); 
   send_data_cmd(l_X_end); 
   
   send_ctrl_cmd(0x2B);
   send_data_cmd(h_Y_start); 
   send_data_cmd(l_Y_start); 
   send_data_cmd(h_Y_end); 
   send_data_cmd(l_Y_end); 
   
   send_ctrl_cmd(0x2C);
}


static void lcm_setbacklight(unsigned int level)
{
   if(level > 255) level = 255;

   send_ctrl_cmd(0x51);
   send_data_cmd(level);	
}


static unsigned int lcm_compare_id(void)
{
   unsigned int id = 0;

   SET_RESET_PIN(1);
   SET_RESET_PIN(0);
   MDELAY(25);
   SET_RESET_PIN(1);
   MDELAY(50);

   send_ctrl_cmd(0xB9);  // SET password
   send_data_cmd(0xFF);  
   send_data_cmd(0x83);  
   send_data_cmd(0x69);

   send_ctrl_cmd(0xC3);
   send_data_cmd(0x00);
   send_data_cmd(0x80);
   send_data_cmd(0x00);
   
   send_ctrl_cmd(0xF4);
   id = read_data_cmd();  // dummy read
   id = read_data_cmd();

   LCM_PRINT("[HX] ID:0x%x, 0x%x\n", id, LCM_ID);
   return (LCM_ID == id)?1:0;
}


LCM_DRIVER hx8369_dbi_6571_lcm_drv = 
{
   .name			= "hx8369_dbi_6571",
   .set_util_funcs = lcm_set_util_funcs,
   .get_params     = lcm_get_params,
   .init           = lcm_init,
   .suspend        = lcm_suspend,
   .resume         = lcm_resume,
   .update         = lcm_update,
   .set_backlight	= lcm_setbacklight,
   .compare_id     = lcm_compare_id,
};


