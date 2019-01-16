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

//#define ESD_SUPPORT

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)

#define PHYSICAL_WIDTH  (56)  // mm
#define PHYSICAL_HEIGHT (93)  // mm

#define LCM_ID       (0x5510)

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

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define SET_CHIP_SELECT(v)    (lcm_util.set_chip_select((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

static __inline void send_ctrl_cmd(unsigned int cmd)
{
    unsigned char temp1 = (unsigned char)((cmd >> 8) & 0xFF);
    unsigned char temp2 = (unsigned char)(cmd & 0xFF);

    lcm_util.send_cmd(0x2000 | temp1);
    lcm_util.send_cmd(0x0000 | temp2);
}


static __inline void send_data_cmd(unsigned int data)
{
    lcm_util.send_data(0x4000 | data);
}


static __inline unsigned short read_data_cmd(void)
{
    return (unsigned short)(lcm_util.read_data());
}


static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}


static void init_lcm_registers(void)
{
    //PAGE1
    set_lcm_register(0xF000, 0x55);
    set_lcm_register(0xF001, 0xAA);
    set_lcm_register(0xF002, 0x52);
    set_lcm_register(0xF003, 0x08);
    set_lcm_register(0xF004, 0x01);

    //AVDD Set AVDD 5.2V
    set_lcm_register(0xB000, 0x0D);
    set_lcm_register(0xB001, 0x0D);
    set_lcm_register(0xB002, 0x0D);

    //AVDD ratio
    set_lcm_register(0xB600, 0x34);
    set_lcm_register(0xB601, 0x34);
    set_lcm_register(0xB602, 0x34);
    
    //AVEE  -5.2V
    set_lcm_register(0xB100, 0x0D);
    set_lcm_register(0xB101, 0x0D);
    set_lcm_register(0xB102, 0x0D);

    //AVEE ratio
    set_lcm_register(0xB700, 0x34);
    set_lcm_register(0xB701, 0x34);
    set_lcm_register(0xB702, 0x34);

    //VCL  -2.5V
    set_lcm_register(0xB200, 0x00);
    set_lcm_register(0xB201, 0x00);
    set_lcm_register(0xB202, 0x00);

    //VCL ratio
    set_lcm_register(0xB800, 0x24);
    set_lcm_register(0xB801, 0x24);
    set_lcm_register(0xB802, 0x24);

    //VGH 15V  (Free pump)
    set_lcm_register(0xBF00, 0x01);

    //VGH=15V(1V/step)	Free pump
    set_lcm_register(0xB300, 0x0F);
    set_lcm_register(0xB301, 0x0F);
    set_lcm_register(0xB302, 0x0F);

    //VGH ratio
    set_lcm_register(0xB900, 0x34);
    set_lcm_register(0xB901, 0x34);
    set_lcm_register(0xB902, 0x34);

    //VGL_REG -10V
    set_lcm_register(0xB500, 0x08);
    set_lcm_register(0xB501, 0x08);
    set_lcm_register(0xB502, 0x08);

    set_lcm_register(0xC200, 0x03);

    //VGLX ratio
    set_lcm_register(0xBA00, 0x24);
    set_lcm_register(0xBA01, 0x24);
    set_lcm_register(0xBA02, 0x24);

    //VGMP/VGSP 4.5V/0V
    set_lcm_register(0xBC00, 0x00);
    set_lcm_register(0xBC01, 0x78);
    set_lcm_register(0xBC02, 0x00);

    //VGMN/VGSN -4.5V/0V
    set_lcm_register(0xBD00, 0x00);
    set_lcm_register(0xBD01, 0x78);
    set_lcm_register(0xBD02, 0x00);

    //VCOM  
    set_lcm_register(0xBE00, 0x00);
    set_lcm_register(0xBE01, 0x64);

    //Gamma Setting
    set_lcm_register(0xD100, 0x00);
    set_lcm_register(0xD101, 0x33);
    set_lcm_register(0xD102, 0x00);
    set_lcm_register(0xD103, 0x34);
    set_lcm_register(0xD104, 0x00);
    set_lcm_register(0xD105, 0x3A);
    set_lcm_register(0xD106, 0x00);
    set_lcm_register(0xD107, 0x4A);
    set_lcm_register(0xD108, 0x00);
    set_lcm_register(0xD109, 0x5C);
    set_lcm_register(0xD10A, 0x00);
    set_lcm_register(0xD10B, 0x81);
    set_lcm_register(0xD10C, 0x00);
    set_lcm_register(0xD10D, 0xA6);
    set_lcm_register(0xD10E, 0x00);
    set_lcm_register(0xD10F, 0xE5);
    set_lcm_register(0xD110, 0x01);
    set_lcm_register(0xD111, 0x13);
    set_lcm_register(0xD112, 0x01);
    set_lcm_register(0xD113, 0x54);
    set_lcm_register(0xD114, 0x01);
    set_lcm_register(0xD115, 0x82);
    set_lcm_register(0xD116, 0x01);
    set_lcm_register(0xD117, 0xCA);
    set_lcm_register(0xD118, 0x02);
    set_lcm_register(0xD119, 0x00);
    set_lcm_register(0xD11A, 0x02);
    set_lcm_register(0xD11B, 0x01);
    set_lcm_register(0xD11C, 0x02);
    set_lcm_register(0xD11D, 0x34);
    set_lcm_register(0xD11E, 0x02);
    set_lcm_register(0xD11F, 0x67);
    set_lcm_register(0xD120, 0x02);
    set_lcm_register(0xD121, 0x84);
    set_lcm_register(0xD122, 0x02);
    set_lcm_register(0xD123, 0xA4);
    set_lcm_register(0xD124, 0x02);
    set_lcm_register(0xD125, 0xB7);
    set_lcm_register(0xD126, 0x02);
    set_lcm_register(0xD127, 0xCF);
    set_lcm_register(0xD128, 0x02);
    set_lcm_register(0xD129, 0xDE);
    set_lcm_register(0xD12A, 0x02);
    set_lcm_register(0xD12B, 0xF2);
    set_lcm_register(0xD12C, 0x02);
    set_lcm_register(0xD12D, 0xFE);
    set_lcm_register(0xD12E, 0x03);
    set_lcm_register(0xD12F, 0x10);
    set_lcm_register(0xD130, 0x03);
    set_lcm_register(0xD131, 0x33);
    set_lcm_register(0xD132, 0x03);
    set_lcm_register(0xD133, 0x6D);

    //Gamma (G+)
    set_lcm_register(0xD200, 0x00);
    set_lcm_register(0xD201, 0x33);
    set_lcm_register(0xD202, 0x00);
    set_lcm_register(0xD203, 0x34);
    set_lcm_register(0xD204, 0x00);
    set_lcm_register(0xD205, 0x3A);
    set_lcm_register(0xD206, 0x00);
    set_lcm_register(0xD207, 0x4A);
    set_lcm_register(0xD208, 0x00);
    set_lcm_register(0xD209, 0x5C);
    set_lcm_register(0xD20A, 0x00);
    set_lcm_register(0xD20B, 0x81);
    set_lcm_register(0xD20C, 0x00);
    set_lcm_register(0xD20D, 0xA6);
    set_lcm_register(0xD20E, 0x00);
    set_lcm_register(0xD20F, 0xE5);
    set_lcm_register(0xD210, 0x01);
    set_lcm_register(0xD211, 0x13);
    set_lcm_register(0xD212, 0x01);
    set_lcm_register(0xD213, 0x54);
    set_lcm_register(0xD214, 0x01);
    set_lcm_register(0xD215, 0x82);
    set_lcm_register(0xD216, 0x01);
    set_lcm_register(0xD217, 0xCA);
    set_lcm_register(0xD218, 0x02);
    set_lcm_register(0xD219, 0x00);
    set_lcm_register(0xD21A, 0x02);
    set_lcm_register(0xD21B, 0x01);
    set_lcm_register(0xD21C, 0x02);
    set_lcm_register(0xD21D, 0x34);
    set_lcm_register(0xD21E, 0x02);
    set_lcm_register(0xD21F, 0x67);
    set_lcm_register(0xD220, 0x02);
    set_lcm_register(0xD221, 0x84);
    set_lcm_register(0xD222, 0x02);
    set_lcm_register(0xD223, 0xA4);
    set_lcm_register(0xD224, 0x02);
    set_lcm_register(0xD225, 0xB7);
    set_lcm_register(0xD226, 0x02);
    set_lcm_register(0xD227, 0xCF);
    set_lcm_register(0xD228, 0x02);
    set_lcm_register(0xD229, 0xDE);
    set_lcm_register(0xD22A, 0x02);
    set_lcm_register(0xD22B, 0xF2);
    set_lcm_register(0xD22C, 0x02);
    set_lcm_register(0xD22D, 0xFE);
    set_lcm_register(0xD22E, 0x03);
    set_lcm_register(0xD22F, 0x10);
    set_lcm_register(0xD230, 0x03);
    set_lcm_register(0xD231, 0x33);
    set_lcm_register(0xD232, 0x03);
    set_lcm_register(0xD233, 0x6D);

    //Gamma (B+)
    set_lcm_register(0xD300, 0x00);
    set_lcm_register(0xD301, 0x33);
    set_lcm_register(0xD302, 0x00);
    set_lcm_register(0xD303, 0x34);
    set_lcm_register(0xD304, 0x00);
    set_lcm_register(0xD305, 0x3A);
    set_lcm_register(0xD306, 0x00);
    set_lcm_register(0xD307, 0x4A);
    set_lcm_register(0xD308, 0x00);
    set_lcm_register(0xD309, 0x5C);
    set_lcm_register(0xD30A, 0x00);
    set_lcm_register(0xD30B, 0x81);
    set_lcm_register(0xD30C, 0x00);
    set_lcm_register(0xD30D, 0xA6);
    set_lcm_register(0xD30E, 0x00);
    set_lcm_register(0xD30F, 0xE5);
    set_lcm_register(0xD310, 0x01);
    set_lcm_register(0xD311, 0x13);
    set_lcm_register(0xD312, 0x01);
    set_lcm_register(0xD313, 0x54);
    set_lcm_register(0xD314, 0x01);
    set_lcm_register(0xD315, 0x82);
    set_lcm_register(0xD316, 0x01);
    set_lcm_register(0xD317, 0xCA);
    set_lcm_register(0xD318, 0x02);
    set_lcm_register(0xD319, 0x00);
    set_lcm_register(0xD31A, 0x02);
    set_lcm_register(0xD31B, 0x01);
    set_lcm_register(0xD31C, 0x02);
    set_lcm_register(0xD31D, 0x34);
    set_lcm_register(0xD31E, 0x02);
    set_lcm_register(0xD31F, 0x67);
    set_lcm_register(0xD320, 0x02);
    set_lcm_register(0xD321, 0x84);
    set_lcm_register(0xD322, 0x02);
    set_lcm_register(0xD323, 0xA4);
    set_lcm_register(0xD324, 0x02);
    set_lcm_register(0xD325, 0xB7);
    set_lcm_register(0xD326, 0x02);
    set_lcm_register(0xD327, 0xCF);
    set_lcm_register(0xD328, 0x02);
    set_lcm_register(0xD329, 0xDE);
    set_lcm_register(0xD32A, 0x02);
    set_lcm_register(0xD32B, 0xF2);
    set_lcm_register(0xD32C, 0x02);
    set_lcm_register(0xD32D, 0xFE);
    set_lcm_register(0xD32E, 0x03);
    set_lcm_register(0xD32F, 0x10);
    set_lcm_register(0xD330, 0x03);
    set_lcm_register(0xD331, 0x33);
    set_lcm_register(0xD332, 0x03);
    set_lcm_register(0xD333, 0x6D);

    //Gamma (R-)
    set_lcm_register(0xD400, 0x00);
    set_lcm_register(0xD401, 0x33);
    set_lcm_register(0xD402, 0x00);
    set_lcm_register(0xD403, 0x34);
    set_lcm_register(0xD404, 0x00);
    set_lcm_register(0xD405, 0x3A);
    set_lcm_register(0xD406, 0x00);
    set_lcm_register(0xD407, 0x4A);
    set_lcm_register(0xD408, 0x00);
    set_lcm_register(0xD409, 0x5C);
    set_lcm_register(0xD40A, 0x00);
    set_lcm_register(0xD40B, 0x81);
    set_lcm_register(0xD40C, 0x00);
    set_lcm_register(0xD40D, 0xA6);
    set_lcm_register(0xD40E, 0x00);
    set_lcm_register(0xD40F, 0xE5);
    set_lcm_register(0xD410, 0x01);
    set_lcm_register(0xD411, 0x13);
    set_lcm_register(0xD412, 0x01);
    set_lcm_register(0xD413, 0x54);
    set_lcm_register(0xD414, 0x01);
    set_lcm_register(0xD415, 0x82);
    set_lcm_register(0xD416, 0x01);
    set_lcm_register(0xD417, 0xCA);
    set_lcm_register(0xD418, 0x02);
    set_lcm_register(0xD419, 0x00);
    set_lcm_register(0xD41A, 0x02);
    set_lcm_register(0xD41B, 0x01);
    set_lcm_register(0xD41C, 0x02);
    set_lcm_register(0xD41D, 0x34);
    set_lcm_register(0xD41E, 0x02);
    set_lcm_register(0xD41F, 0x67);
    set_lcm_register(0xD420, 0x02);
    set_lcm_register(0xD421, 0x84);
    set_lcm_register(0xD422, 0x02);
    set_lcm_register(0xD423, 0xA4);
    set_lcm_register(0xD424, 0x02);
    set_lcm_register(0xD425, 0xB7);
    set_lcm_register(0xD426, 0x02);
    set_lcm_register(0xD427, 0xCF);
    set_lcm_register(0xD428, 0x02);
    set_lcm_register(0xD429, 0xDE);
    set_lcm_register(0xD42A, 0x02);
    set_lcm_register(0xD42B, 0xF2);
    set_lcm_register(0xD42C, 0x02);
    set_lcm_register(0xD42D, 0xFE);
    set_lcm_register(0xD42E, 0x03);
    set_lcm_register(0xD42F, 0x10);
    set_lcm_register(0xD430, 0x03);
    set_lcm_register(0xD431, 0x33);
    set_lcm_register(0xD432, 0x03);
    set_lcm_register(0xD433, 0x6D);

    //Gamma (G+)
    set_lcm_register(0xD500, 0x00);
    set_lcm_register(0xD501, 0x33);
    set_lcm_register(0xD502, 0x00);
    set_lcm_register(0xD503, 0x34);
    set_lcm_register(0xD504, 0x00);
    set_lcm_register(0xD505, 0x3A);
    set_lcm_register(0xD506, 0x00);
    set_lcm_register(0xD507, 0x4A);
    set_lcm_register(0xD508, 0x00);
    set_lcm_register(0xD509, 0x5C);
    set_lcm_register(0xD50A, 0x00);
    set_lcm_register(0xD50B, 0x81);
    set_lcm_register(0xD50C, 0x00);
    set_lcm_register(0xD50D, 0xA6);
    set_lcm_register(0xD50E, 0x00);
    set_lcm_register(0xD50F, 0xE5);
    set_lcm_register(0xD510, 0x01);
    set_lcm_register(0xD511, 0x13);
    set_lcm_register(0xD512, 0x01);
    set_lcm_register(0xD513, 0x54);
    set_lcm_register(0xD514, 0x01);
    set_lcm_register(0xD515, 0x82);
    set_lcm_register(0xD516, 0x01);
    set_lcm_register(0xD517, 0xCA);
    set_lcm_register(0xD518, 0x02);
    set_lcm_register(0xD519, 0x00);
    set_lcm_register(0xD51A, 0x02);
    set_lcm_register(0xD51B, 0x01);
    set_lcm_register(0xD51C, 0x02);
    set_lcm_register(0xD51D, 0x34);
    set_lcm_register(0xD51E, 0x02);
    set_lcm_register(0xD51F, 0x67);
    set_lcm_register(0xD520, 0x02);
    set_lcm_register(0xD521, 0x84);
    set_lcm_register(0xD522, 0x02);
    set_lcm_register(0xD523, 0xA4);
    set_lcm_register(0xD524, 0x02);
    set_lcm_register(0xD525, 0xB7);
    set_lcm_register(0xD526, 0x02);
    set_lcm_register(0xD527, 0xCF);
    set_lcm_register(0xD528, 0x02);
    set_lcm_register(0xD529, 0xDE);
    set_lcm_register(0xD52A, 0x02);
    set_lcm_register(0xD52B, 0xF2);
    set_lcm_register(0xD52C, 0x02);
    set_lcm_register(0xD52D, 0xFE);
    set_lcm_register(0xD52E, 0x03);
    set_lcm_register(0xD52F, 0x10);
    set_lcm_register(0xD530, 0x03);
    set_lcm_register(0xD531, 0x33);
    set_lcm_register(0xD532, 0x03);
    set_lcm_register(0xD533, 0x6D);

    //Gamma (B+)
    set_lcm_register(0xD600, 0x00);
    set_lcm_register(0xD601, 0x33);
    set_lcm_register(0xD602, 0x00);
    set_lcm_register(0xD603, 0x34);
    set_lcm_register(0xD604, 0x00);
    set_lcm_register(0xD605, 0x3A);
    set_lcm_register(0xD606, 0x00);
    set_lcm_register(0xD607, 0x4A);
    set_lcm_register(0xD608, 0x00);
    set_lcm_register(0xD609, 0x5C);
    set_lcm_register(0xD60A, 0x00);
    set_lcm_register(0xD60B, 0x81);
    set_lcm_register(0xD60C, 0x00);
    set_lcm_register(0xD60D, 0xA6);
    set_lcm_register(0xD60E, 0x00);
    set_lcm_register(0xD60F, 0xE5);
    set_lcm_register(0xD610, 0x01);
    set_lcm_register(0xD611, 0x13);
    set_lcm_register(0xD612, 0x01);
    set_lcm_register(0xD613, 0x54);
    set_lcm_register(0xD614, 0x01);
    set_lcm_register(0xD615, 0x82);
    set_lcm_register(0xD616, 0x01);
    set_lcm_register(0xD617, 0xCA);
    set_lcm_register(0xD618, 0x02);
    set_lcm_register(0xD619, 0x00);
    set_lcm_register(0xD61A, 0x02);
    set_lcm_register(0xD61B, 0x01);
    set_lcm_register(0xD61C, 0x02);
    set_lcm_register(0xD61D, 0x34);
    set_lcm_register(0xD61E, 0x02);
    set_lcm_register(0xD61F, 0x67);
    set_lcm_register(0xD620, 0x02);
    set_lcm_register(0xD621, 0x84);
    set_lcm_register(0xD622, 0x02);
    set_lcm_register(0xD623, 0xA4);
    set_lcm_register(0xD624, 0x02);
    set_lcm_register(0xD625, 0xB7);
    set_lcm_register(0xD626, 0x02);
    set_lcm_register(0xD627, 0xCF);
    set_lcm_register(0xD628, 0x02);
    set_lcm_register(0xD629, 0xDE);
    set_lcm_register(0xD62A, 0x02);
    set_lcm_register(0xD62B, 0xF2);
    set_lcm_register(0xD62C, 0x02);
    set_lcm_register(0xD62D, 0xFE);
    set_lcm_register(0xD62E, 0x03);
    set_lcm_register(0xD62F, 0x10);
    set_lcm_register(0xD630, 0x03);
    set_lcm_register(0xD631, 0x33);
    set_lcm_register(0xD632, 0x03);
    set_lcm_register(0xD633, 0x6D);

    //LV2 Page 0 enable
    set_lcm_register(0xF000, 0x55);
    set_lcm_register(0xF001, 0xAA);
    set_lcm_register(0xF002, 0x52);
    set_lcm_register(0xF003, 0x08);
    set_lcm_register(0xF004, 0x00);

    //Display control
    set_lcm_register(0xB100, 0x00);
    set_lcm_register(0xB101, 0x00);

    set_lcm_register(0xB400, 0x10);
    set_lcm_register(0xB600, 0x05);

    //Gate EQ control
    set_lcm_register(0xB700, 0x70);
    set_lcm_register(0xB701, 0x70);

    //Source EQ control (Mode 2)
    set_lcm_register(0xB800, 0x01);
    set_lcm_register(0xB801, 0x03);
    set_lcm_register(0xB802, 0x03);
    set_lcm_register(0xB803, 0x03);

    //Inversion mode  (2-dot)
    set_lcm_register(0xBC00, 0x02);
    set_lcm_register(0xBC01, 0x00);
    set_lcm_register(0xBC02, 0x00);

    //Frame rate	(Nova non-used)
    set_lcm_register(0xBD00, 0x01);
    set_lcm_register(0xBD01, 0x8C);
    set_lcm_register(0xBD02, 0x14);
    set_lcm_register(0xBD03, 0x14);
    set_lcm_register(0xBD04, 0x00);

    set_lcm_register(0xBE00, 0x01);
    set_lcm_register(0xBE01, 0x8C);
    set_lcm_register(0xBE02, 0x14);
    set_lcm_register(0xBE03, 0x14);
    set_lcm_register(0xBE04, 0x00);

    set_lcm_register(0xBF00, 0x01);
    set_lcm_register(0xBF01, 0x8C);
    set_lcm_register(0xBF02, 0x14);
    set_lcm_register(0xBF03, 0x14);
    set_lcm_register(0xBF04, 0x00);

    //Timing control 4H w/ 4-delay 
    set_lcm_register(0xC900, 0xD0);
    set_lcm_register(0xC901, 0x02);
    set_lcm_register(0xC902, 0x50);
    set_lcm_register(0xC903, 0x50);
    set_lcm_register(0xC904, 0x50);

    set_lcm_register(0x2A00, 0x00);
    set_lcm_register(0x2A01, 0x00);
    set_lcm_register(0x2A02, 0x01);
    set_lcm_register(0x2A03, 0xDF);

    set_lcm_register(0x2B00, 0x00);
    set_lcm_register(0x2B01, 0x00);
    set_lcm_register(0x2B02, 0x03);
    set_lcm_register(0x2B03, 0x1F);

    set_lcm_register(0x3A00, 0x66);    // RGB666

    //Sleep out
    send_ctrl_cmd(0x1100);
    MDELAY(120);

    //Display on
    send_ctrl_cmd(0x2900);
    MDELAY(30);
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

    params->type = LCM_TYPE_DPI;
    params->ctrl = LCM_CTRL_SERIAL_DBI;
    params->width = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    // physical size
    params->physical_width = PHYSICAL_WIDTH;
    params->physical_height = PHYSICAL_HEIGHT;

    /* serial host interface configurations */
    params->dbi.port = 0;
    params->dbi.data_width = LCM_DBI_DATA_WIDTH_16BITS;
    params->dbi.cpu_write_bits = LCM_DBI_CPU_WRITE_16_BITS;
    params->dbi.io_driving_current = LCM_DRIVING_CURRENT_4MA;

    params->dbi.serial.css = 0xf;
    params->dbi.serial.csh = 0xf;
    params->dbi.serial.rd_1st = 0xf;
    params->dbi.serial.rd_2nd = 0xf;
    params->dbi.serial.wr_1st = 0xf;
    params->dbi.serial.wr_2nd = 0xf;

    params->dbi.serial.sif_3wire = 0;
    params->dbi.serial.sif_sdi = 0;
    params->dbi.serial.sif_1st_pol = 0;
    params->dbi.serial.sif_sck_def = 1;
    params->dbi.serial.sif_div2 = 0;
    params->dbi.serial.sif_hw_cs = 1;

    /* RGB interface configurations */
    params->dpi.clk_pol = LCM_POLARITY_RISING;
    params->dpi.de_pol = LCM_POLARITY_RISING;
    params->dpi.vsync_pol = LCM_POLARITY_RISING;
    params->dpi.hsync_pol = LCM_POLARITY_RISING;

    params->dpi.hsync_pulse_width = 14;
    params->dpi.hsync_back_porch  = 16;
    params->dpi.hsync_front_porch = 16;
    params->dpi.vsync_pulse_width = 16;
    params->dpi.vsync_back_porch  = 20;
    params->dpi.vsync_front_porch = 20;

    params->dpi.format = LCM_DPI_FORMAT_RGB666;
    params->dpi.rgb_order = LCM_COLOR_ORDER_RGB;
    params->dpi.io_driving_current = LCM_DRIVING_CURRENT_4MA;

    params->dpi.PLL_CLOCK = 221; // 8X DPI Clock
}

    
static unsigned int lcm_compare_id(void)
{
    unsigned int id = 0;

    SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(20);	

    //*************Enable CMD2 Page1  *******************//
    set_lcm_register(0xF000, 0x55);
    set_lcm_register(0xF001, 0xAA);
    set_lcm_register(0xF002, 0x52);
    set_lcm_register(0xF003, 0x08);
    set_lcm_register(0xF004, 0x01);
    MDELAY(10); 

    send_ctrl_cmd(0xC500);
    id = read_data_cmd() << 8;
    id = id | read_data_cmd();

    return (LCM_ID == id)?1:0;
}


static void lcm_init(void)
{
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(20);

    init_lcm_registers();
}


static void lcm_suspend(void)
{
    send_ctrl_cmd(0x1000);
    MDELAY(120);

    send_ctrl_cmd(0x2800);
    MDELAY(10);

    set_lcm_register(0x4F00, 0x01);
    MDELAY(40);
}


static void lcm_resume(void)
{
   send_ctrl_cmd(0x1100);
   MDELAY(200);

   send_ctrl_cmd(0x2900);
}

#ifdef ESD_SUPPORT

static unsigned int lcm_esd_check(void)
{
    unsigned int readData;
    unsigned int result = TRUE;

    send_ctrl_cmd(0x0A00);
    readData = read_data_cmd();
    if (readData == 0x009C)
        result = FALSE;

    return result;
} 

static unsigned int lcm_esd_recover(void)
{
    lcm_init();

    return TRUE;
}

#endif

LCM_DRIVER nt35510_dpi_6572_lcm_drv =
{
    .name           = "nt35510_dpi_6572",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
#ifdef ESD_SUPPORT
    .esd_check      = lcm_esd_check,
    .esd_recover    = lcm_esd_recover,
#endif
};

