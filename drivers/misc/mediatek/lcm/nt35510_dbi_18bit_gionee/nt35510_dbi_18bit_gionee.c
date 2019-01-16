// alps/mediatek/custom/common/kernel/lcm/nt35510_dbi_18bit_gionee/nt35510_dbi_18bit_gionee.c
#ifdef BUILD_LK
#else
    #include <linux/string.h>
#endif
#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)
#define LCM_ID       (0x5510)

#ifdef BUILD_LK
#define LCM_PRINT printf
#else
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/bitmap.h>
#include <linux/signal.h>
#include <linux/printk.h>
#define LCM_PRINT printk
#endif

#define LCM_PRINT_FUNC()  LCM_PRINT("LCM35510" "`%s:%d [%s] " "\n", __FILE__, __LINE__, __FUNCTION__)

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

static __inline void send_ctrl_cmd(unsigned int cmd)
{
  //  printf("c:%x", cmd);
    lcm_util.send_cmd(cmd);
}


static __inline void send_data_cmd(unsigned int data)
{
  //  printf("d:%x", data);
    lcm_util.send_data(data&0xff);
}


static __inline unsigned short read_data_cmd(void)
{
  LCM_PRINT_FUNC();    

    return (unsigned short)(lcm_util.read_data());
}


static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
  LCM_PRINT_FUNC();    

    send_ctrl_cmd(regIndex);
    send_data_cmd(regData);
}

    /* send_ctrl_cmd(0x1100);//sleep out */
    /* MDELAY(200); */

    /* set_lcm_register(0xC000,0x86);//power control PWCTR1 */
// (query-replace-regexp "Init_DriverIC" "set_lcm_register")
// (query-replace-regexp "DelayX1ms" "MDELAY")
// (query-replace-regexp "DelayX1ms" "MDELAY")

static void init_lcm_registers(void)
{
  int i =0;
  LCM_PRINT_FUNC();
    set_lcm_register(0x1100, 0x00);    //It is necessary
    MDELAY(150);             //It is necessary

    set_lcm_register(0xF000, 0x55);
    set_lcm_register(0xF001, 0xAA);
    set_lcm_register(0xF002, 0x52);
    set_lcm_register(0xF003, 0x08);
    set_lcm_register(0xF004, 0x01);

    set_lcm_register(0xBC00, 0x00);    // EQ Control Function for Source Driver
    set_lcm_register(0xBC01, 0x70);
    set_lcm_register(0xBC02, 0x0D);    //q

    set_lcm_register(0xBD00, 0x00);    // Display Timing Control
    set_lcm_register(0xBD01, 0x70);
    set_lcm_register(0xBD02, 0x0D);

    set_lcm_register(0xBE00, 0x00);
    set_lcm_register(0xBE01, 0x48);

    set_lcm_register(0xD100, 0x00);
    set_lcm_register(0xD101, 0x00);
    set_lcm_register(0xD102, 0x00);
    set_lcm_register(0xD103, 0x13);
    set_lcm_register(0xD104, 0x00);
    set_lcm_register(0xD105, 0x27);
    set_lcm_register(0xD106, 0x00);
    set_lcm_register(0xD107, 0x46);
    set_lcm_register(0xD108, 0x00);
    set_lcm_register(0xD109, 0x6A);
    set_lcm_register(0xD10A, 0x00);
    set_lcm_register(0xD10B, 0xA4);
    set_lcm_register(0xD10C, 0x00);
    set_lcm_register(0xD10D, 0xD5);
    set_lcm_register(0xD10E, 0x01);
    set_lcm_register(0xD10F, 0x1E);
    set_lcm_register(0xD110, 0x01);
    set_lcm_register(0xD111, 0x53);
    set_lcm_register(0xD112, 0x01);
    set_lcm_register(0xD113, 0x9B);
    set_lcm_register(0xD114, 0x01);
    set_lcm_register(0xD115, 0xCB);
    set_lcm_register(0xD116, 0x02);
    set_lcm_register(0xD117, 0x16);
    set_lcm_register(0xD118, 0x02);
    set_lcm_register(0xD119, 0x4E);
    set_lcm_register(0xD11A, 0x02);
    set_lcm_register(0xD11B, 0x4F);
    set_lcm_register(0xD11C, 0x02);
    set_lcm_register(0xD11D, 0x7F);
    set_lcm_register(0xD11E, 0x02);
    set_lcm_register(0xD11F, 0xB3);
    set_lcm_register(0xD120, 0x02);
    set_lcm_register(0xD121, 0xCF);
    set_lcm_register(0xD122, 0x02);
    set_lcm_register(0xD123, 0xEE);
    set_lcm_register(0xD124, 0x03);
    set_lcm_register(0xD125, 0x01);
    set_lcm_register(0xD126, 0x03);
    set_lcm_register(0xD127, 0x1B);
    set_lcm_register(0xD128, 0x03);
    set_lcm_register(0xD129, 0x2A);
    set_lcm_register(0xD12A, 0x03);
    set_lcm_register(0xD12B, 0x40);
    set_lcm_register(0xD12C, 0x03);
    set_lcm_register(0xD12D, 0x50);
    set_lcm_register(0xD12E, 0x03);
    set_lcm_register(0xD12F, 0x67);
    set_lcm_register(0xD130, 0x03);
    set_lcm_register(0xD131, 0xA8);
    set_lcm_register(0xD132, 0x03);
    set_lcm_register(0xD133, 0xD8);
//Positive Gamma for GREEN
    set_lcm_register(0xD200, 0x00);
    set_lcm_register(0xD201, 0x00);
    set_lcm_register(0xD202, 0x00);
    set_lcm_register(0xD203, 0x13);
    set_lcm_register(0xD204, 0x00);
    set_lcm_register(0xD205, 0x27);
    set_lcm_register(0xD206, 0x00);
    set_lcm_register(0xD207, 0x46);
    set_lcm_register(0xD208, 0x00);
    set_lcm_register(0xD209, 0x6A);
    set_lcm_register(0xD20A, 0x00);
    set_lcm_register(0xD20B, 0xA4);
    set_lcm_register(0xD20C, 0x00);
    set_lcm_register(0xD20D, 0xD5);
    set_lcm_register(0xD20E, 0x01);
    set_lcm_register(0xD20F, 0x1E);
    set_lcm_register(0xD210, 0x01);
    set_lcm_register(0xD211, 0x53);
    set_lcm_register(0xD212, 0x01);
    set_lcm_register(0xD213, 0x9B);
    set_lcm_register(0xD214, 0x01);
    set_lcm_register(0xD215, 0xCB);
    set_lcm_register(0xD216, 0x02);
    set_lcm_register(0xD217, 0x16);
    set_lcm_register(0xD218, 0x02);
    set_lcm_register(0xD219, 0x4E);
    set_lcm_register(0xD21A, 0x02);
    set_lcm_register(0xD21B, 0x4F);
    set_lcm_register(0xD21C, 0x02);
    set_lcm_register(0xD21D, 0x7F);
    set_lcm_register(0xD21E, 0x02);
    set_lcm_register(0xD21F, 0xB3);
    set_lcm_register(0xD220, 0x02);
    set_lcm_register(0xD221, 0xCF);
    set_lcm_register(0xD222, 0x02);
    set_lcm_register(0xD223, 0xEE);
    set_lcm_register(0xD224, 0x03);
    set_lcm_register(0xD225, 0x01);
    set_lcm_register(0xD226, 0x03);
    set_lcm_register(0xD227, 0x1B);
    set_lcm_register(0xD228, 0x03);
    set_lcm_register(0xD229, 0x2A);
    set_lcm_register(0xD22A, 0x03);
    set_lcm_register(0xD22B, 0x40);
    set_lcm_register(0xD22C, 0x03);
    set_lcm_register(0xD22D, 0x50);
    set_lcm_register(0xD22E, 0x03);
    set_lcm_register(0xD22F, 0x67);
    set_lcm_register(0xD230, 0x03);
    set_lcm_register(0xD231, 0xA8);
    set_lcm_register(0xD232, 0x03);
    set_lcm_register(0xD233, 0xD8);
//Positive Gamma for BLUE
    set_lcm_register(0xD300, 0x00);
    set_lcm_register(0xD301, 0x00);
    set_lcm_register(0xD302, 0x00);
    set_lcm_register(0xD303, 0x13);
    set_lcm_register(0xD304, 0x00);
    set_lcm_register(0xD305, 0x27);
    set_lcm_register(0xD306, 0x00);
    set_lcm_register(0xD307, 0x46);
    set_lcm_register(0xD308, 0x00);
    set_lcm_register(0xD309, 0x6A);
    set_lcm_register(0xD30A, 0x00);
    set_lcm_register(0xD30B, 0xA4);
    set_lcm_register(0xD30C, 0x00);
    set_lcm_register(0xD30D, 0xD5);
    set_lcm_register(0xD30E, 0x01);
    set_lcm_register(0xD30F, 0x1E);
    set_lcm_register(0xD310, 0x01);
    set_lcm_register(0xD311, 0x53);
    set_lcm_register(0xD312, 0x01);
    set_lcm_register(0xD313, 0x9B);
    set_lcm_register(0xD314, 0x01);
    set_lcm_register(0xD315, 0xCB);
    set_lcm_register(0xD316, 0x02);
    set_lcm_register(0xD317, 0x16);
    set_lcm_register(0xD318, 0x02);
    set_lcm_register(0xD319, 0x4E);
    set_lcm_register(0xD31A, 0x02);
    set_lcm_register(0xD31B, 0x4F);
    set_lcm_register(0xD31C, 0x02);
    set_lcm_register(0xD31D, 0x7F);
    set_lcm_register(0xD31E, 0x02);
    set_lcm_register(0xD31F, 0xB3);
    set_lcm_register(0xD320, 0x02);
    set_lcm_register(0xD321, 0xCF);
    set_lcm_register(0xD322, 0x02);
    set_lcm_register(0xD323, 0xEE);
    set_lcm_register(0xD324, 0x03);
    set_lcm_register(0xD325, 0x01);
    set_lcm_register(0xD326, 0x03);
    set_lcm_register(0xD327, 0x1B);
    set_lcm_register(0xD328, 0x03);
    set_lcm_register(0xD329, 0x2A);
    set_lcm_register(0xD32A, 0x03);
    set_lcm_register(0xD32B, 0x40);
    set_lcm_register(0xD32C, 0x03);
    set_lcm_register(0xD32D, 0x50);
    set_lcm_register(0xD32E, 0x03);
    set_lcm_register(0xD32F, 0x67);
    set_lcm_register(0xD330, 0x03);
    set_lcm_register(0xD331, 0xA8);
    set_lcm_register(0xD332, 0x03);
    set_lcm_register(0xD333, 0xD8);
//Negative Gamma for RED
    set_lcm_register(0xD400, 0x00);
    set_lcm_register(0xD401, 0x00);
    set_lcm_register(0xD402, 0x00);
    set_lcm_register(0xD403, 0x13);
    set_lcm_register(0xD404, 0x00);
    set_lcm_register(0xD405, 0x27);
    set_lcm_register(0xD406, 0x00);
    set_lcm_register(0xD407, 0x46);
    set_lcm_register(0xD408, 0x00);
    set_lcm_register(0xD409, 0x6A);
    set_lcm_register(0xD40A, 0x00);
    set_lcm_register(0xD40B, 0xA4);
    set_lcm_register(0xD40C, 0x00);
    set_lcm_register(0xD40D, 0xD5);
    set_lcm_register(0xD40E, 0x01);
    set_lcm_register(0xD40F, 0x1E);
    set_lcm_register(0xD410, 0x01);
    set_lcm_register(0xD411, 0x53);
    set_lcm_register(0xD412, 0x01);
    set_lcm_register(0xD413, 0x9B);
    set_lcm_register(0xD414, 0x01);
    set_lcm_register(0xD415, 0xCB);
    set_lcm_register(0xD416, 0x02);
    set_lcm_register(0xD417, 0x16);
    set_lcm_register(0xD418, 0x02);
    set_lcm_register(0xD419, 0x4E);
    set_lcm_register(0xD41A, 0x02);
    set_lcm_register(0xD41B, 0x4F);
    set_lcm_register(0xD41C, 0x02);
    set_lcm_register(0xD41D, 0x7F);
    set_lcm_register(0xD41E, 0x02);
    set_lcm_register(0xD41F, 0xB3);
    set_lcm_register(0xD420, 0x02);
    set_lcm_register(0xD421, 0xCF);
    set_lcm_register(0xD422, 0x02);
    set_lcm_register(0xD423, 0xEE);
    set_lcm_register(0xD424, 0x03);
    set_lcm_register(0xD425, 0x01);
    set_lcm_register(0xD426, 0x03);
    set_lcm_register(0xD427, 0x1B);
    set_lcm_register(0xD428, 0x03);
    set_lcm_register(0xD429, 0x2A);
    set_lcm_register(0xD42A, 0x03);
    set_lcm_register(0xD42B, 0x40);
    set_lcm_register(0xD42C, 0x03);
    set_lcm_register(0xD42D, 0x50);
    set_lcm_register(0xD42E, 0x03);
    set_lcm_register(0xD42F, 0x67);
    set_lcm_register(0xD430, 0x03);
    set_lcm_register(0xD431, 0xA8);
    set_lcm_register(0xD432, 0x03);
    set_lcm_register(0xD433, 0xD8);
//Negative Gamma for GERREN
    set_lcm_register(0xD500, 0x00);
    set_lcm_register(0xD501, 0x00);
    set_lcm_register(0xD502, 0x00);
    set_lcm_register(0xD503, 0x13);
    set_lcm_register(0xD504, 0x00);
    set_lcm_register(0xD505, 0x27);
    set_lcm_register(0xD506, 0x00);
    set_lcm_register(0xD507, 0x46);
    set_lcm_register(0xD508, 0x00);
    set_lcm_register(0xD509, 0x6A);
    set_lcm_register(0xD50A, 0x00);
    set_lcm_register(0xD50B, 0xA4);
    set_lcm_register(0xD50C, 0x00);
    set_lcm_register(0xD50D, 0xD5);
    set_lcm_register(0xD50E, 0x01);
    set_lcm_register(0xD50F, 0x1E);
    set_lcm_register(0xD510, 0x01);
    set_lcm_register(0xD511, 0x53);
    set_lcm_register(0xD512, 0x01);
    set_lcm_register(0xD513, 0x9B);
    set_lcm_register(0xD514, 0x01);
    set_lcm_register(0xD515, 0xCB);
    set_lcm_register(0xD516, 0x02);
    set_lcm_register(0xD517, 0x16);
    set_lcm_register(0xD518, 0x02);
    set_lcm_register(0xD519, 0x4E);
    set_lcm_register(0xD51A, 0x02);
    set_lcm_register(0xD51B, 0x4F);
    set_lcm_register(0xD51C, 0x02);
    set_lcm_register(0xD51D, 0x7F);
    set_lcm_register(0xD51E, 0x02);
    set_lcm_register(0xD51F, 0xB3);
    set_lcm_register(0xD520, 0x02);
    set_lcm_register(0xD521, 0xCF);
    set_lcm_register(0xD522, 0x02);
    set_lcm_register(0xD523, 0xEE);
    set_lcm_register(0xD524, 0x03);
    set_lcm_register(0xD525, 0x01);
    set_lcm_register(0xD526, 0x03);
    set_lcm_register(0xD527, 0x1B);
    set_lcm_register(0xD528, 0x03);
    set_lcm_register(0xD529, 0x2A);
    set_lcm_register(0xD52A, 0x03);
    set_lcm_register(0xD52B, 0x40);
    set_lcm_register(0xD52C, 0x03);
    set_lcm_register(0xD52D, 0x50);
    set_lcm_register(0xD52E, 0x03);
    set_lcm_register(0xD52F, 0x67);
    set_lcm_register(0xD530, 0x03);
    set_lcm_register(0xD531, 0xA8);
    set_lcm_register(0xD532, 0x03);
    set_lcm_register(0xD533, 0xD8);
//Negative Gamma for BLUE
    set_lcm_register(0xD600, 0x00);
    set_lcm_register(0xD601, 0x00);
    set_lcm_register(0xD602, 0x00);
    set_lcm_register(0xD603, 0x13);
    set_lcm_register(0xD604, 0x00);
    set_lcm_register(0xD605, 0x27);
    set_lcm_register(0xD606, 0x00);
    set_lcm_register(0xD607, 0x46);
    set_lcm_register(0xD608, 0x00);
    set_lcm_register(0xD609, 0x6A);
    set_lcm_register(0xD60A, 0x00);
    set_lcm_register(0xD60B, 0xA4);
    set_lcm_register(0xD60C, 0x00);
    set_lcm_register(0xD60D, 0xD5);
    set_lcm_register(0xD60E, 0x01);
    set_lcm_register(0xD60F, 0x1E);
    set_lcm_register(0xD610, 0x01);
    set_lcm_register(0xD611, 0x53);
    set_lcm_register(0xD612, 0x01);
    set_lcm_register(0xD613, 0x9B);
    set_lcm_register(0xD614, 0x01);
    set_lcm_register(0xD615, 0xCB);
    set_lcm_register(0xD616, 0x02);
    set_lcm_register(0xD617, 0x16);
    set_lcm_register(0xD618, 0x02);
    set_lcm_register(0xD619, 0x4E);
    set_lcm_register(0xD61A, 0x02);
    set_lcm_register(0xD61B, 0x4F);
    set_lcm_register(0xD61C, 0x02);
    set_lcm_register(0xD61D, 0x7F);
    set_lcm_register(0xD61E, 0x02);
    set_lcm_register(0xD61F, 0xB3);
    set_lcm_register(0xD620, 0x02);
    set_lcm_register(0xD621, 0xCF);
    set_lcm_register(0xD622, 0x02);
    set_lcm_register(0xD623, 0xEE);
    set_lcm_register(0xD624, 0x03);
    set_lcm_register(0xD625, 0x01);
    set_lcm_register(0xD626, 0x03);
    set_lcm_register(0xD627, 0x1B);
    set_lcm_register(0xD628, 0x03);
    set_lcm_register(0xD629, 0x2A);
    set_lcm_register(0xD62A, 0x03);
    set_lcm_register(0xD62B, 0x40);
    set_lcm_register(0xD62C, 0x03);
    set_lcm_register(0xD62D, 0x50);
    set_lcm_register(0xD62E, 0x03);
    set_lcm_register(0xD62F, 0x67);
    set_lcm_register(0xD630, 0x03);
    set_lcm_register(0xD631, 0xA8);
    set_lcm_register(0xD632, 0x03);
    set_lcm_register(0xD633, 0xD8);

    set_lcm_register(0xB000, 0x00);    // Setting AVDD Voltage
    set_lcm_register(0xB001, 0x00);
    set_lcm_register(0xB002, 0x00);

    set_lcm_register(0xB600, 0x36);
    set_lcm_register(0xB601, 0x36);
    set_lcm_register(0xB602, 0x36);

    set_lcm_register(0xB800, 0x26);
    set_lcm_register(0xB801, 0x26);
    set_lcm_register(0xB802, 0x26);


    set_lcm_register(0xB100, 0x00);    // Display Option Control
    set_lcm_register(0xB101, 0x00);
    set_lcm_register(0xB102, 0x00);

    set_lcm_register(0xB700, 0x35);    //AVEE
    set_lcm_register(0xB701, 0x35);
    set_lcm_register(0xB702, 0x35);

    set_lcm_register(0xB900, 0x34);    // Setting VGH Voltage
    set_lcm_register(0xB901, 0x34);
    set_lcm_register(0xB902, 0x34);

    set_lcm_register(0xBA00, 0x16);    // Setting VGH Voltage
    set_lcm_register(0xBA01, 0x16);
    set_lcm_register(0xBA02, 0x16);


    // Select Command Page '0'
    set_lcm_register(0xF000, 0x55);
    set_lcm_register(0xF001, 0xAA);
    set_lcm_register(0xF002, 0x52);
    set_lcm_register(0xF003, 0x08);
    set_lcm_register(0xF004, 0x00);

    set_lcm_register(0xB100, 0xfc);    //RAM Keep//
    set_lcm_register(0xB101, 0x00);    //00

    set_lcm_register(0xB400, 0x10);    //Vivid Color// 
    set_lcm_register(0xB401, 0x00);    //null

    set_lcm_register(0xB600, 0x04);    //SDT// 
    set_lcm_register(0xB601, 0x00);    //null

    set_lcm_register(0xB700, 0x72);    //Set Gate EQ  
    set_lcm_register(0xB701, 0x72);

    set_lcm_register(0xB800, 0x01);    //Set Source EQ//  
    set_lcm_register(0xB801, 0x04);
    set_lcm_register(0xB802, 0x04);
    set_lcm_register(0xB803, 0x04);

    set_lcm_register(0xBC00, 0x02);    //Inversion Control 
    set_lcm_register(0xBC01, 0x02);
    set_lcm_register(0xBC02, 0x02);

    set_lcm_register(0xBD00, 0x01);    //Porch Adjust //e
    set_lcm_register(0xBD01, 0x84);    //84  B9
    set_lcm_register(0xBD02, 0x07);    //07
    set_lcm_register(0xBD03, 0x31);    //31
    set_lcm_register(0xBD04, 0x00);

    set_lcm_register(0xBe00, 0x01);    //Porch Adjust //e
    set_lcm_register(0xBe01, 0x79);
    set_lcm_register(0xBe02, 0x07);
    set_lcm_register(0xBe03, 0x31);
    set_lcm_register(0xBe04, 0x00);

    set_lcm_register(0xBf00, 0x01);    //Porch Adjust //e
    set_lcm_register(0xBf01, 0x79);
    set_lcm_register(0xBf02, 0x07);
    set_lcm_register(0xBf03, 0x31);
    set_lcm_register(0xBf04, 0x00);

    set_lcm_register(0x3a00, 0x66);    //18bit  66  18

    set_lcm_register(0x3600, 0x00);    //c0


    set_lcm_register(0x1100, 0x00);


    set_lcm_register(0x2900, 0x00);

    set_lcm_register(0xC700, 0x02);

    set_lcm_register(0xc900, 0x11);
    set_lcm_register(0xc901, 0x00);
    set_lcm_register(0xc902, 0x00);
    set_lcm_register(0xc903, 0x00);
    set_lcm_register(0xc904, 0x00);
    set_lcm_register(0xcA00, 0x01);
    set_lcm_register(0xcA01, 0xE4);
    set_lcm_register(0xcA02, 0xE4);
    set_lcm_register(0xcA03, 0xE4);
    set_lcm_register(0xcA04, 0xE4);
    set_lcm_register(0xcA05, 0xE4);
    set_lcm_register(0xcA06, 0xE4);
    set_lcm_register(0xcA07, 0xE4);
    set_lcm_register(0xcA08, 0x08);
    set_lcm_register(0xcA09, 0x08);
    set_lcm_register(0xcA0A, 0x00);
    set_lcm_register(0xcA0B, 0x00);
    set_lcm_register(0xB100, 0xFc);
    set_lcm_register(0xB101, 0x00);
    send_ctrl_cmd(0x2c00);             // Write GRAM Start

    //Display on
    send_ctrl_cmd(0x2900);

    send_ctrl_cmd(0x2C00);
    for (i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; i++) {
        send_data_cmd(0xff00);
        send_data_cmd(0xff00);
    }

}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
  LCM_PRINT_FUNC();    

    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
  LCM_PRINT_FUNC();    

    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DBI;
    params->ctrl   = LCM_CTRL_PARALLEL_DBI;
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->io_select_mode = 1; //note:this para is different between 6573 and 6575

    params->dbi.port                    = 0;  //DBI port must be 0 or 1 on mt6575, should not be 2
    params->dbi.data_width              = LCM_DBI_DATA_WIDTH_18BITS;
    params->dbi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dbi.data_format.trans_seq   = LCM_DBI_TRANS_SEQ_MSB_FIRST;
    params->dbi.data_format.padding     = LCM_DBI_PADDING_ON_LSB;
    params->dbi.data_format.format      = LCM_DBI_FORMAT_RGB666;
    params->dbi.data_format.width       = LCM_DBI_DATA_WIDTH_18BITS;
      params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_32_BITS;
	  //params->dbi.cpu_write_bits          = LCM_DBI_CPU_WRITE_16_BITS;
    params->dbi.io_driving_current      = LCM_DRIVING_CURRENT_8MA;


    /* params->dbi.parallel.write_setup    = 2; */
    /* params->dbi.parallel.write_hold     = 2; */
    /* params->dbi.parallel.write_wait     = 10; */
    /* params->dbi.parallel.read_setup     = 2; */
    /* params->dbi.parallel.read_hold      = 2; */
    /* params->dbi.parallel.read_latency   = 10; */
    /* params->dbi.parallel.wait_period    = 1; */

    params->dbi.parallel.write_setup    = 1;
    params->dbi.parallel.write_hold     = 1;
    params->dbi.parallel.write_wait     = 3;
    params->dbi.parallel.read_setup     = 4;
    params->dbi.parallel.read_hold      = 0;
    params->dbi.parallel.read_latency   = 18;
    params->dbi.parallel.wait_period    = 1;

    params->dbi.parallel.cs_high_width  = 0; //cycles of cs high level between each transfer
    // enable tearing-free
    params->dbi.te_mode                 = 0;
    //params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;
}

static unsigned int lcm_compare_id(void)
{
  LCM_PRINT_FUNC();    
    unsigned int id = 0;
	//	while(1){
    send_ctrl_cmd(0x1180);
    id = read_data_cmd();
    
    LCM_PRINT("%s, id1 = 0x%08x\n", __func__, id);
    
    send_ctrl_cmd(0x1080);
    id |= (read_data_cmd() << 8);
    
    LCM_PRINT("%s, id2 = 0x%08x\n", __func__, id);

	//    }

    return (LCM_ID == id)?1:0;
}

static void lcm_init(void)
{
  LCM_PRINT_FUNC();    

    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);

    init_lcm_registers();

	/* while(1){ */
	/*   lcm_compare_id(); */
	/* } */

}


static void lcm_suspend(void)
{
  LCM_PRINT_FUNC();    

    send_ctrl_cmd(0x1000);
    MDELAY(20);
}


static void lcm_resume(void)
{
  LCM_PRINT_FUNC();    

    send_ctrl_cmd(0x1100);
    MDELAY(200);
    
    // xuecheng, do we need to write 0x2900??
    send_ctrl_cmd(0x2900);
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
  LCM_PRINT_FUNC();    

    short  x0, y0, x1, y1;
    short   h_X_start,l_X_start,h_X_end,l_X_end,h_Y_start,l_Y_start,h_Y_end,l_Y_end;
    
    
    x0 = (short)x;
    y0 = (short)y;
    x1 = (short)x+width-1;
    y1 = (short)y+height-1;
    
    h_X_start=((x0&0x0300)>>8);
    l_X_start=(x0&0x00FF);
    h_X_end=((x1&0x0300)>>8);
    l_X_end=(x1&0x00FF);
    
    h_Y_start=((y0&0x0300)>>8);
    l_Y_start=(y0&0x00FF);
    h_Y_end=((y1&0x0300)>>8);
    l_Y_end=(y1&0x00FF);
    
    send_ctrl_cmd( 0x2A00 );
    send_data_cmd( h_X_start);
    send_ctrl_cmd( 0x2A01 );
    send_data_cmd( l_X_start);
    send_ctrl_cmd( 0x2A02);
    send_data_cmd( h_X_end );
    send_ctrl_cmd( 0x2A03);
    send_data_cmd( l_X_end );
    send_ctrl_cmd( 0x2B00 );
    send_data_cmd( h_Y_start);
    send_ctrl_cmd( 0x2B01 );
    send_data_cmd( l_Y_start);
    send_ctrl_cmd( 0x2B02);
    send_data_cmd( h_Y_end );
    send_ctrl_cmd( 0x2B03);
    send_data_cmd( l_Y_end );
    send_ctrl_cmd(0x3601);  //enable HSM mode
    send_data_cmd(0x01);
    send_ctrl_cmd( 0x2C00 );
}




LCM_DRIVER nt35510_dbi_18bit_gionee_lcm_drv = 
{
    .name			= "nt35510_mcu_6572_gionnee",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .update         = lcm_update,
    .compare_id     = lcm_compare_id
};

