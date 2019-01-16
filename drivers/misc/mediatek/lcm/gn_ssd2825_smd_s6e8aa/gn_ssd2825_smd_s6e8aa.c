#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <platform/mt_pwm.h>
	#ifdef LCD_DEBUG
		#define LCM_DEBUG(format, ...)   printf("uboot ssd2825" format "\n", ## __VA_ARGS__)
	#else
		#define LCM_DEBUG(format, ...)
	#endif

#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt6577_gpio.h>
	#include <asm/arch/mt6577_pwm.h>
	#ifdef LCD_DEBUG
		#define LCM_DEBUG(format, ...)   printf("uboot ssd2825" format "\n", ## __VA_ARGS__)
	#else
		#define LCM_DEBUG(format, ...)
	#endif
#else
	#include <mach/mt_gpio.h>
	#include <mach/mt_pwm.h>
	#ifdef LCD_DEBUG
		#define LCM_DEBUG(format, ...)   printk("kernel ssd2825" format "\n", ## __VA_ARGS__)
	#else
		#define LCM_DEBUG(format, ...)
	#endif
#endif

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LSCE_GPIO_PIN                 (GPIO_DISP_LSCE_PIN) //(GPIO47)
#define LSCK_GPIO_PIN                 (GPIO_DISP_LSCK_PIN) //(GPIO51)
#define LSDA_GPIO_PIN                 (GPIO_DISP_LSDA_PIN) //(GPIO52)
#define LSDI_GPIO_PIN                 (GPIO_DISP_LSA0_PIN) //(GPIO48)

#define SSD2825_SHUT_GPIO_PIN         (GPIO_SSD2825_SHUT_PIN)   //(GPIO83) 
#define SSD2825_POWER_GPIO_PIN        (GPIO_SSD2825_POWER_PIN)  //(GPIO106)
#define SSD2825_MIPI_CLK_GPIO_PIN     (GPIO_SSD2825_CLK_PIN)      //(GPIO70)
#define LCD_POWER_GPIO_PIN            (GPIO_LCD_POWER_PIN)         //  (GPIO86)

#define FRAME_WIDTH                   (720)
#define FRAME_HEIGHT                  (1280)

#define SSD2825_ID                    (0x2825) 

#define GAMMABACKLIGHT_NUM             25
#define GAMMA_TABLE_MAX_INDEX          ARRAY_OF(gamma_table)-1

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static LCM_UTIL_FUNCS lcm_util = {0};

#define UDELAY(n)	                (lcm_util.udelay(n))
#define MDELAY(n)	                (lcm_util.mdelay(n))

#define SET_RESET_PIN(v)	        (lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)	        (lcm_util.set_gpio_out((n), (v)))


#define SET_LSCE_LOW                 SET_GPIO_OUT(LSCE_GPIO_PIN, 0)
#define SET_LSCE_HIGH                SET_GPIO_OUT(LSCE_GPIO_PIN, 1)
#define SET_LSCK_LOW   				 SET_GPIO_OUT(LSCK_GPIO_PIN, 0)
#define SET_LSCK_HIGH  				 SET_GPIO_OUT(LSCK_GPIO_PIN, 1)
#define SET_LSDA_LOW                 SET_GPIO_OUT(LSDA_GPIO_PIN, 0)
#define SET_LSDA_HIGH                SET_GPIO_OUT(LSDA_GPIO_PIN, 1)
#define GET_HX_SDI                   mt_get_gpio_in(LSDI_GPIO_PIN)


#define HX_WR_COM                   (0x70)
#define HX_WR_REGISTER              (0x72)
#define HX_RD_REGISTER              (0x73)

static void setGammaBacklight(unsigned int level);
static void setDynamicElvss(unsigned int level);
//extern int lcd_set_pwm(int pwm_num);
static void lcm_setbacklight(unsigned int level); 

static int   g_last_Backlight_level =  -1;   
#define    ARRAY_OF(x)      ((int)(sizeof(x)/sizeof(x[0])))

#ifdef BUILD_LK
int lcd_set_pwm(int pwm_num)
{
	struct pwm_spec_config pwm_setting;
	
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_FIFO; //new mode fifo and periodical mode
	pwm_setting.clk_div = CLK_DIV1;
	pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK;
	
	
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.HDURATION = 1;
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.LDURATION = 1;	
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
	
	//printf("[LEDS]uboot: lcd_set_pwm :duty is %d\n");
  
    pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xAAAAAAAA;
	pwm_setting.pwm_mode.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xAAAAAAAA ;
	pwm_set_spec_config(&pwm_setting);
       
	
	return 0;
	
}
#else
int lcd_set_pwm(int pwm_num)
{
	struct pwm_spec_config pwm_setting;
	
	pwm_setting.pwm_no = pwm_num;
	pwm_setting.mode = PWM_MODE_FIFO; //new mode fifo and periodical mode
	pwm_setting.clk_div = CLK_DIV1;
	pwm_setting.clk_src = PWM_CLK_NEW_MODE_BLOCK;
	
	
	pwm_setting.PWM_MODE_FIFO_REGS.HDURATION = 1;
	pwm_setting.PWM_MODE_FIFO_REGS.LDURATION = 1;	
	pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 63;
	pwm_setting.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
	
	//printf("[LEDS]uboot: lcd_set_pwm :duty is %d\n");
  
    pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xAAAAAAAA;
	pwm_setting.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0xAAAAAAAA ;
	pwm_set_spec_config(&pwm_setting);
       
	
	return 0;
	
}
#endif

#ifdef BUILD_UBOOT
static int g_resume_flag = 0;
#endif

struct __gamma_backlight {
    unsigned int backlight_level;                       //backlight level
    unsigned char gammaValue[GAMMABACKLIGHT_NUM]; //gamma value for backlight
};

static struct __gamma_backlight gamma_table[] = {
{0,  {0x01,0x52,0x24,0x5D,0x00,0x00,0x00,0x74,0x78,0x5F,0xA8,0xB6,0xAE,0x8A,0xA5,0x93,0xB8,0xBF,0xB3,0x00,0x58,0x00,0x39,0x00,0x62}},
{10, {0x01,0x52,0x24,0x5D,0x00,0x00,0x00,0x74,0x78,0x5F,0xA8,0xB6,0xAE,0x8A,0xA5,0x93,0xB8,0xBF,0xB3,0x00,0x58,0x00,0x39,0x00,0x62}},
{20, {0x01,0x52,0x24,0x5D,0x00,0x00,0x00,0x74,0x78,0x5F,0xA8,0xB6,0xAE,0x8A,0xA5,0x93,0xB8,0xBF,0xB3,0x00,0x58,0x00,0x39,0x00,0x62}},
{30, {0x01,0x52,0x24,0x5D,0x00,0x00,0x00,0x74,0x78,0x5F,0xA8,0xB6,0xAE,0x8A,0xA5,0x93,0xB8,0xBF,0xB3,0x00,0x58,0x00,0x39,0x00,0x62}},
{40, {0x01,0x52,0x24,0x5D,0x00,0x00,0x08,0x7E,0x84,0x6D,0xAD,0xBE,0xB7,0x8C,0xA3,0x91,0xB7,0xC0,0xB3,0x00,0x61,0x00,0x41,0x00,0x6B}},
{50, {0x01,0x52,0x24,0x5D,0x22,0x09,0x1C,0x83,0x8D,0x77,0xAF,0xC4,0xBC,0x8F,0xA3,0x91,0xB7,0xBC,0xB1,0x00,0x68,0x00,0x49,0x00,0x73}},	
{60, {0x01,0x52,0x24,0x5D,0x37,0x23,0x28,0x87,0x95,0x83,0xB2,0xC5,0xBD,0x90,0xA0,0x8E,0xB8,0xBE,0xB7,0x00,0x6E,0x00,0x50,0x00,0x78}},
{70, {0x01,0x52,0x24,0x5D,0x47,0x38,0x34,0x8B,0x9B,0x8C,0xB3,0xC7,0xBD,0x93,0x9F,0x8E,0xB6,0xBD,0xB4,0x00,0x74,0x00,0x57,0x00,0x80}},
{80, {0x01,0x52,0x24,0x5D,0x51,0x45,0x3B,0x8E,0x9F,0x94,0xB5,0xC8,0xBC,0x91,0x9D,0x8E,0xB8,0xBD,0xB2,0x00,0x79,0x00,0x5C,0x00,0x86}},
{90, {0x01,0x52,0x24,0x5D,0x58,0x4E,0x40,0x90,0xA4,0x97,0xB6,0xC8,0xBC,0x90,0x9D,0x8F,0xB9,0xBB,0xB1,0x00,0x7D,0x00,0x61,0x00,0x8A}},
{100,{0x01,0x52,0x24,0x5D,0x5F,0x58,0x47,0x92,0xA8,0x9C,0xB7,0xC8,0xBC,0x91,0x9B,0x8D,0xB7,0xBC,0xB2,0x00,0x83,0x00,0x66,0x00,0x90}},
{110,{0x01,0x52,0x24,0x5D,0x65,0x5F,0x4B,0x94,0xA9,0x9F,0xB6,0xC6,0xBB,0x91,0x9C,0x8C,0xB7,0xBB,0xB3,0x00,0x87,0x00,0x6A,0x00,0x94}},
{120,{0x01,0x52,0x24,0x5D,0x68,0x66,0x50,0x95,0xAC,0xA2,0xB8,0xC7,0xB9,0x93,0x9B,0x8D,0xB5,0xBA,0xB2,0x00,0x88,0x00,0x6F,0x00,0x99}},
{130,{0x01,0x52,0x24,0x5D,0x6B,0x69,0x54,0x97,0xAE,0xA3,0xB7,0xC7,0xBB,0x92,0x9A,0x8B,0xB8,0xB9,0xB1,0x00,0x8D,0x00,0x73,0x00,0x9D}},
{140,{0x01,0x52,0x24,0x5D,0x6E,0x6F,0x57,0x97,0xB1,0xA4,0xB8,0xC5,0xBA,0x92,0x9A,0x8C,0xB6,0xB9,0xB1,0x00,0x92,0x00,0x77,0x00,0xA1}},
{150,{0x01,0x52,0x24,0x5D,0x71,0x73,0x5B,0x98,0xB1,0xA6,0xBA,0xC4,0xB7,0x90,0x9B,0x8D,0xB6,0xB8,0xB0,0x00,0x96,0x00,0x7A,0x00,0xA5}},
{160,{0x01,0x52,0x24,0x5D,0x74,0x77,0x5F,0x98,0xB3,0xA6,0xBA,0xC4,0xB9,0x92,0x9B,0x8B,0xB4,0xB6,0xB2,0x00,0x99,0x00,0x7E,0x00,0xA7}},
{170,{0x01,0x52,0x24,0x5D,0x76,0x7A,0x63,0x9B,0xB4,0xA7,0xB7,0xC3,0xB7,0x93,0x9A,0x8D,0xB4,0xB6,0xB0,0x00,0x9D,0x00,0x82,0x00,0xAC}},
{180,{0x01,0x52,0x24,0x5D,0x77,0x7E,0x66,0x9B,0xB5,0xA6,0xB9,0xC3,0xB8,0x92,0x99,0x8C,0xB3,0xB7,0xAE,0x00,0xA0,0x00,0x85,0x00,0xB0}},
{190,{0x01,0x52,0x24,0x5D,0x7A,0x81,0x69,0x9C,0xB5,0xA6,0xB9,0xC3,0xB9,0x92,0x99,0x8B,0xB3,0xB6,0xAF,0x00,0xA3,0x00,0x89,0x00,0xB3}},
{200,{0x01,0x52,0x24,0x5D,0x7B,0x83,0x6B,0x9C,0xB5,0xA7,0xBA,0xC3,0xB9,0x91,0x98,0x8B,0xB3,0xB6,0xAD,0x00,0xA6,0x00,0x8C,0x00,0xB7}},
{210,{0x01,0x52,0x24,0x5D,0x7D,0x87,0x6E,0x9D,0xB6,0xA7,0xB7,0xC2,0xB9,0x93,0x99,0x8B,0xB1,0xB5,0xAD,0x00,0xAA,0x00,0x8F,0x00,0xBA}},
{220,{0x01,0x52,0x24,0x5D,0x80,0x84,0x70,0x9E,0xB6,0xA8,0xB9,0xC2,0xB7,0x92,0x98,0x8B,0xB0,0xB4,0xAD,0x00,0xAC,0x00,0x92,0x00,0xBD}},
{230,{0x01,0x52,0x24,0x5D,0x7E,0x8A,0x72,0x9E,0xB6,0xA8,0xBA,0xC3,0xB8,0x90,0x97,0x8A,0xB1,0xB5,0xAD,0x00,0xAF,0x00,0x95,0x00,0xC0}},
//{240,{0x01,0x52,0x24,0x5D,0x81,0x8D,0x75,0x9D,0xB5,0xA8,0xBA,0xCC,0xB7,0x92,0x97,0x8A,0xAE,0xB4,0xAD,0x00,0xB3,0x00,0x98,0x00,0xC3}},
{250,{0x01,0x52,0x24,0x5D,0x82,0x8A,0x75,0x9E,0xB6,0xA7,0xBA,0xC2,0xB9,0x92,0x97,0x8A,0xAE,0xB3,0xAE,0x00,0xB5,0x00,0x9A,0x00,0xC4}},
{260,{0x01,0x52,0x24,0x5D,0x84,0x91,0x79,0x9F,0xB5,0xA9,0xBA,0xC2,0xB6,0x91,0x97,0x8A,0xAF,0xB2,0xAB,0x00,0xB7,0x00,0x9E,0x00,0xC9}},
//remove for LCM backlight start
//{270,{0x01,0x52,0x24,0x5D,0x84,0x92,0x7A,0x9F,0xB6,0xA8,0xB8,0xC2,0xB8,0x8F,0x97,0x89,0xB0,0xB2,0xAB,0x00,0xBA,0x00,0xA1,0x00,0xCC}},
//{280,{0x01,0x52,0x24,0x5D,0x85,0x94,0x7C,0xA0,0xB6,0xA7,0xB9,0xC1,0xB5,0x91,0x97,0x8A,0xAE,0xB1,0xAB,0x00,0xBC,0x00,0xA3,0x00,0xCF}},
//{290,{0x01,0x52,0x24,0x5D,0x86,0x97,0x7E,0xA1,0xB5,0xA7,0xBA,0xC3,0xB7,0x90,0x96,0x8A,0xAE,0xB0,0xAB,0x00,0xBF,0x00,0xA7,0x00,0xD1}},
//{300,{0x01,0x52,0x24,0x5D,0xBA,0xCD,0xB3,0xAD,0xC0,0xB1,0xBF,0xC7,0xBC,0x90,0x97,0x8A,0xAA,0xAE,0xA5,0x00,0xC2,0x00,0xA8,0x00,0xD7}},
//remove for LCM backlight
};


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
static __inline void spi_send_data(unsigned int data)
{
    unsigned int i;

    SET_LSCE_HIGH;
    SET_LSCK_HIGH;
    SET_LSDA_HIGH;
    UDELAY(10);

    SET_LSCE_LOW;
	UDELAY(10);
	

    for (i = 0; i < 24; ++i)
    {
        if (data & (1 << 23)) {
            SET_LSDA_HIGH;
        } else {
            SET_LSDA_LOW;
        }
		data <<= 1;
        UDELAY(10);
        SET_LSCK_LOW;
        UDELAY(10);
		SET_LSCK_HIGH;
        UDELAY(10);
    }

    SET_LSDA_HIGH;
    SET_LSCE_HIGH;
}

static __inline void Write_com(unsigned int cmd)
{
    unsigned int out = ((HX_WR_COM<<16) | (cmd & 0xFFFF));
    spi_send_data(out);
}

static __inline void Write_register(unsigned int data)
{
    unsigned int out = ((HX_WR_REGISTER<<16) |(data & 0xFFFF));
    spi_send_data(out);
}
  
static __inline unsigned short Read_register(void)
{
	 unsigned char i,j,front_data;
	 unsigned short value = 0;

	 front_data=HX_RD_REGISTER;

	 SET_LSCE_HIGH;
     SET_LSCK_HIGH;
     SET_LSDA_HIGH;
	 UDELAY(10);
	 SET_LSCE_LOW;
	 UDELAY(10); 
  
	 for(i=0;i<8;i++) // 8  Data
	 {

		if ( front_data& 0x80)
		   SET_LSDA_HIGH;
		else
		   SET_LSDA_LOW;
		front_data<<= 1;
		UDELAY(10);
		SET_LSCK_LOW;
		UDELAY(10); 
		SET_LSCK_HIGH;
		UDELAY(10);		  
	  }
	  MDELAY(1);
	  
	  for(j=0;j<16;j++) // 16 Data
	  {
	  
		SET_LSCK_HIGH;
		UDELAY(10);
		SET_LSCK_LOW;
		value<<= 1;
		value |= GET_HX_SDI;
		   
		UDELAY(10); 
	   }
  
       SET_LSCE_HIGH;
       return value;
	  
	}

 //this function only for test for the function have bug,do not call this 
void ssd2825_read_s6e8aa_reg(void)
{
	int id=0,i;
	int temp=0;

	//F0
	Write_com(0x00B7);
	Write_register(0x034B);
	Write_com(0x00BC);
	Write_register(0x0002);
	Write_com(0x00BF);
	Write_register(0x00B0);
	
	Write_com(0x00B7);
	Write_register(0x0782);
	
	Write_com(0x00C4);
	Write_register(0x0001);

	Write_com(0x00C1);
	Write_register(0x0004);
	
	Write_com(0x00BC);
	Write_register(0x0001);
	
	Write_com(0x00BF);
	Write_register(0x00FA);

	
	Write_com(0x00C6);
	temp=Read_register();
	while(!(temp&0x1))
	{
		Write_com(0x00C6);
		temp=Read_register();
	}
	Write_com(0x00FF);

	for(i=0;i<2;i++)
	{
	 id=Read_register();
     LCM_DEBUG("Read_register--data%d id is: %x\n",i,id);
   
	}

}
 static void init_lcm_registers(void)
 {
	 LCM_DEBUG("[LCM************]: init_lcm_registers. \n");
	 
	 Write_com(0x00B1);
	 Write_register(0x0102);
	 Write_com(0x00B2);
	 Write_register(0x040E); //0x030E , a white line displays on the top of the screen
	 Write_com(0x00B3);
	 Write_register(0x0D40); //0x2940
	 Write_com(0x00B4);
	 Write_register(0x02D0);
	 Write_com(0x00B5);
	 Write_register(0x0500);
	 Write_com(0x00B6);
	 Write_register(0x008B); //0x0007---0x008B	,CABC out of control
	 //MDELAY(2);
 
	 Write_com(0x00DE);
	 Write_register(0x0003);
	 Write_com(0x00D6);
	 Write_register(0x0004);
	 Write_com(0x00B9);
	 Write_register(0x0000);
 
	 Write_com(0x00BA);
	 Write_register(0x801F);
 
	 Write_com(0x00BB);
	 Write_register(0x0009);
	 Write_com(0x00B9);
	 Write_register(0x0001);
	 Write_com(0x00B8);
	 Write_register(0x0000);
 
	 //F0
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0003);
	 Write_com(0x00BF);
	 Write_register(0x5AF0);
	 Write_register(0x005A);
	 //F1
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0003);
	 Write_com(0x00BF);
	 Write_register(0x5AF1);
	 Write_register(0x005A);
 
	 //0x11
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0001);
	 Write_com(0x00BF);
	 Write_register(0x0011);
	 MDELAY(5);  //100
 
	 //F8
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0027);
	 Write_com(0x00BF);
	 Write_register(0x2DF8);
	 Write_register(0x0035);
	 Write_register(0x0000);
	 Write_register(0x0093);
	 Write_register(0x7D3C);
	 Write_register(0x2708);
	 Write_register(0x3f7d);
	 Write_register(0x0000);
	 Write_register(0x2000);
	 Write_register(0x0804);
	 Write_register(0x006E);
	 Write_register(0x0000);
	 Write_register(0x0802);
	 Write_register(0x2308);
	 Write_register(0xC023);
	 Write_register(0x08C8);
	 Write_register(0xC148);
	 Write_register(0xC100);
	 Write_register(0xFFFF);
	 Write_register(0x00C8);
 
	 //F2
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0004);
	 Write_com(0x00BF);
	 Write_register(0x80F2);
	 Write_register(0x0D04); //0x0D03, a white line displays on the top of the screen
	 
	 //F6
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0003);
	 Write_com(0x00BF);
	 Write_register(0x00F6);
	 Write_register(0x0002);
 
	 
	 //B6
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x000A);
	 Write_com(0x00BF);
	 Write_register(0x0CB6);
	 Write_register(0x0302);
	 Write_register(0xFF32);
	 Write_register(0x4444);
	 Write_register(0x00C0);
	  
	 //D9
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x000F);
	 Write_com(0x00BF);
	 Write_register(0x14D9);
	 Write_register(0x0C40);
	 Write_register(0xCECB);
	 Write_register(0xC46E);
	 Write_register(0x4007);
	 Write_register(0xCB41);
	 Write_register(0x6000);
	 Write_register(0x0019);
 
	 //E1
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0006);
	 Write_com(0x00BF);
	 Write_register(0x10E1);
	 Write_register(0x171C);
	 Write_register(0x1D08);
	 
	 //E2
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0007);
	 Write_com(0x00BF);
	 Write_register(0xEDE2);
	 Write_register(0xC307);
	 Write_register(0x0D13);
	 Write_register(0x0003);
 
	 //E3
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0002);
	 Write_com(0x00BF);
	 Write_register(0x40E3);
	 
	 //E4
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0008);
	 Write_com(0x00BF);
	 Write_register(0x00E4);
	 Write_register(0x1400);
	 Write_register(0x0080);
	 Write_register(0x0000);
	 
	 //F4
	 Write_com(0x00B7);
	 Write_register(0x034B);
	 Write_com(0x00B8);
	 Write_register(0x0000);
	 Write_com(0x00BC);
	 Write_register(0x0008);
	 Write_com(0x00BF);
	 Write_register(0xCFF4);
	 Write_register(0x120A);
	 Write_register(0x1E10);
	 Write_register(0x0233);
 }



// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void config_gpio(void)
{
    lcm_util.set_gpio_mode(LSCE_GPIO_PIN, GPIO_DISP_LSCE_PIN_M_GPIO);
    lcm_util.set_gpio_mode(LSCK_GPIO_PIN, GPIO_DISP_LSCK_PIN_M_GPIO);
    lcm_util.set_gpio_mode(LSDA_GPIO_PIN, GPIO_DISP_LSDA_PIN_M_GPIO);
    lcm_util.set_gpio_mode(LSDI_GPIO_PIN, GPIO_MODE_00);

    lcm_util.set_gpio_dir(LSCE_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSCK_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSDA_GPIO_PIN, GPIO_DIR_OUT);
    lcm_util.set_gpio_dir(LSDI_GPIO_PIN, GPIO_DIR_IN);

	
	lcm_util.set_gpio_pull_enable(LSCE_GPIO_PIN, GPIO_PULL_DISABLE);
	lcm_util.set_gpio_pull_enable(LSCK_GPIO_PIN, GPIO_PULL_DISABLE);
	lcm_util.set_gpio_pull_enable(LSDA_GPIO_PIN, GPIO_PULL_DISABLE);
	lcm_util.set_gpio_pull_enable(LSDI_GPIO_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(LSDI_GPIO_PIN, GPIO_PULL_UP);

	//set pwm output clk
	lcm_util.set_gpio_mode(SSD2825_MIPI_CLK_GPIO_PIN, GPIO_MODE_02);
	lcm_util.set_gpio_dir(SSD2825_MIPI_CLK_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(SSD2825_MIPI_CLK_GPIO_PIN, GPIO_PULL_DISABLE); 
	lcd_set_pwm(PWM0);
	MDELAY(10);
	
	//set ssd2825 shut ping high
	lcm_util.set_gpio_mode(SSD2825_SHUT_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(SSD2825_SHUT_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(SSD2825_SHUT_GPIO_PIN, GPIO_PULL_DISABLE); 
	SET_GPIO_OUT(SSD2825_SHUT_GPIO_PIN , 1);
	MDELAY(1);
	
	//set ssd2825 poweron
	lcm_util.set_gpio_mode(SSD2825_POWER_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(SSD2825_POWER_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(SSD2825_POWER_GPIO_PIN, GPIO_PULL_DISABLE); 
	SET_GPIO_OUT(SSD2825_POWER_GPIO_PIN , 1);
	MDELAY(1);

	//set s6e8aa poweron
	lcm_util.set_gpio_mode(LCD_POWER_GPIO_PIN, GPIO_MODE_00);
	lcm_util.set_gpio_dir(LCD_POWER_GPIO_PIN, GPIO_DIR_OUT); 
	lcm_util.set_gpio_pull_enable(LCD_POWER_GPIO_PIN, GPIO_PULL_DISABLE); 
	SET_GPIO_OUT(LCD_POWER_GPIO_PIN , 1);
	MDELAY(50);
	
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

	params->type   = LCM_TYPE_DPI;
	params->ctrl   = LCM_CTRL_GPIO;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dpi.mipi_pll_clk_ref  = 0;
	params->dpi.mipi_pll_clk_div1 = 46;
	params->dpi.mipi_pll_clk_div2 = 5;
	params->dpi.dpi_clk_div       = 2;
	params->dpi.dpi_clk_duty      = 1;

	params->dpi.clk_pol           = LCM_POLARITY_FALLING;
	params->dpi.de_pol            = LCM_POLARITY_RISING;
	params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
	params->dpi.hsync_pol         = LCM_POLARITY_FALLING;

	params->dpi.hsync_pulse_width = 2;
	params->dpi.hsync_back_porch  = 14;
	params->dpi.hsync_front_porch = 64;
	params->dpi.vsync_pulse_width = 1;
	params->dpi.vsync_back_porch  = 3;
	params->dpi.vsync_front_porch = 13;

	params->dpi.format            = LCM_DPI_FORMAT_RGB888;
	params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
	params->dpi.is_serial_output  = 0;

	params->dpi.intermediat_buffer_num = 2;
	params->dpi.io_driving_current = LCM_DRIVING_CURRENT_4MA;

	params->dpi.i2x_en = 0;
	params->dpi.i2x_edge = 1;
}

static void lcm_init(void)
{ 
	 //unsigned short id;
	 config_gpio();

	 SET_RESET_PIN(0);
	 MDELAY(25);
	 SET_RESET_PIN(1);
	 MDELAY(120);
	 init_lcm_registers();
}

static void lcm_suspend(void)
{

#ifdef BUILD_UBOOT
	g_resume_flag=1;
#endif

	Write_com(0x00B7); 
	Write_register(0x034B);  
	Write_com(0x00B8); 
	Write_register(0x0000);  
	Write_com(0x00BC); 
	Write_register(0x0001);
	Write_com(0x00BF);
	Write_register(0x0010);  
	MDELAY(120);

	Write_com(0x00B7); 
	Write_register(0x0344);  
	Write_com(0x00B9);
	Write_register(0x0000); 
	LCM_DEBUG("lcm_suspend\n");
	
}

static void lcm_resume(void)
{
#ifdef BUILD_UBOOT
	if(g_resume_flag==0)
	{
		g_resume_flag=1;
		return;
	}
#endif
	config_gpio();
	SET_RESET_PIN(0);
	MDELAY(25);
	SET_RESET_PIN(1);
	MDELAY(120);
	init_lcm_registers();
}

static unsigned int lcm_compare_id(void)
{
	unsigned short id;
	Write_com(0x00b0);	  
	id=Read_register();
    LCM_DEBUG("lcm_compare_id id is: %x\n",id);
 
	return (SSD2825_ID == id)?1:0;

}
static void setDynamicElvss(unsigned int index)
{

	if(gamma_table[index].backlight_level>200)
	{
		Write_com(0x00B7);
		Write_register(0x034B);
		Write_com(0x00B8);
		Write_register(0x0000);
		Write_com(0x00BC);
		Write_register(0x0003);
		Write_com(0x00BF);
		Write_register(0x04B1);
		Write_register(0x008B);
	}
	else
	{
		Write_com(0x00B7);
		Write_register(0x034B);
		Write_com(0x00B8);
		Write_register(0x0000);
		Write_com(0x00BC);
		Write_register(0x0003);
		Write_com(0x00BF);
		Write_register(0x04B1);
		Write_register(0x0095);
	}
    
}

static void setGammaBacklight(unsigned int index)
{
	LCM_DEBUG("setGammaBacklight  index=%d ARRAY_OF(gamma_table)=%d\n", index,ARRAY_OF(gamma_table));
    //FA
	Write_com(0x00B7);
	Write_register(0x034B);
	Write_com(0x00B8);
	Write_register(0x0000);
	Write_com(0x00BC);
	Write_register(0x001A);
	Write_com(0x00BF);
	Write_register((gamma_table[index].gammaValue[0]<<8)|0xFA);
	Write_register((gamma_table[index].gammaValue[2]<<8)|(gamma_table[index].gammaValue[1]));
	Write_register((gamma_table[index].gammaValue[4]<<8)|(gamma_table[index].gammaValue[3]));
	Write_register((gamma_table[index].gammaValue[6]<<8)|(gamma_table[index].gammaValue[5]));
	Write_register((gamma_table[index].gammaValue[8]<<8)|(gamma_table[index].gammaValue[7]));
	Write_register((gamma_table[index].gammaValue[10]<<8)|(gamma_table[index].gammaValue[9]));
	Write_register((gamma_table[index].gammaValue[12]<<8)|(gamma_table[index].gammaValue[11]));
	Write_register((gamma_table[index].gammaValue[14]<<8)|(gamma_table[index].gammaValue[13]));
	Write_register((gamma_table[index].gammaValue[16]<<8)|(gamma_table[index].gammaValue[15]));
	Write_register((gamma_table[index].gammaValue[18]<<8)|(gamma_table[index].gammaValue[17]));
	Write_register((gamma_table[index].gammaValue[20]<<8)|(gamma_table[index].gammaValue[19]));
	Write_register((gamma_table[index].gammaValue[22]<<8)|(gamma_table[index].gammaValue[21]));
	Write_register((gamma_table[index].gammaValue[24]<<8)|(gamma_table[index].gammaValue[23]));
}

static void lcm_setbacklight(unsigned int level)   //back_light setting 
{

	int index;	
	
	index = level*(ARRAY_OF(gamma_table)-1)/255;

	if(g_last_Backlight_level == index)
	{
		return;
	}


	if(level == 0)
	{
	   
		//display off
		Write_com(0x00B7);
		Write_register(0x034B);
		Write_com(0x00B8);
		Write_register(0x0000);
		Write_com(0x00BC);
		Write_register(0x0001);
		Write_com(0x00BF);
		Write_register(0x0028);
	}
	else
	{    
		setGammaBacklight(index);
		setDynamicElvss(index);

		Write_com(0x00B7);
		Write_register(0x034B);
		Write_com(0x00B8);
		Write_register(0x0000);
		Write_com(0x00BC);
		Write_register(0x0002);
		Write_com(0x00BF);
		Write_register(0x03F7);

	  if(!g_last_Backlight_level)
		{
	   MDELAY(120);	//for lcm flash

       //0x29  display on
	   Write_com(0x00B7);
	   Write_register(0x034B);
	   Write_com(0x00B8);
	   Write_register(0x0000);
	   Write_com(0x00BC);
	   Write_register(0x0001);
	   Write_com(0x00BF);
	   Write_register(0x0029);
		}
    }
	g_last_Backlight_level=index;
    LCM_DEBUG("lcd-backlight  level=%d, index=%d\n", level,index);
}


#ifndef BUILD_UBOOT

#ifdef GN_MTK_BSP_LCM_DEBUG   //add by chenqiang for lcd_debug
int lcd_f9_register_write(const char * buf)
{
    int n_idx;
    int max_nu=0;
    int max_np=0;
    int max_no=0;
    int num_n=0;
    int num_q=0;
    const char *p = buf ; 
    char qq[2];
    char num[25];
    while( (*p++ != '{' )&&( max_nu++ <= 200));    
    {
        if(max_nu>200)       
			return -1;
    }
    for(num_n=0;num_n<=24;num_n++)
	{       
		for(num_q=0;num_q<=1;num_q++)
		{
			max_np=0;
			while( (*(p) == 32)&&(max_np++ <= 20) )
			{
				p++;
				if(max_np>20)      
					return -1;
			}
			if( (*p>='0') && (*p <='9'))
				qq[num_q]=(*p++ -'0');
			else if( (*p>='A') && (*p <='F'))
				qq[num_q]=(*p++ -'A' + 10);
			else if( (*p>='a') && (*p <='f'))
				qq[num_q]=(*p++ -'a' + 10);
			else
			 	return -1;
		}
		num[num_n]=qq[0]*16 + qq[1];
	}
    while( (*p++!= '}' )&&( max_no++ <= 20));    
    {
        if(max_no>20)     
			return -1;
    }
	for(num_n =0;num_n<=24;num_n++)
	{
		LCM_DEBUG("register lcd_f9_register_write num[%d]=%d\n", num_n,num[num_n]);
	}
   //0xF7
	Write_com(0x00B7);
	Write_register(0x034B);
	Write_com(0x00B8);
	Write_register(0x0000);
	Write_com(0x00BC);
	Write_register(0x0002);
	Write_com(0x00BF);
	Write_register(0x00F7);

    //FA
	Write_com(0x00B7);
	Write_register(0x034B);
	Write_com(0x00B8);
	Write_register(0x0000);
	Write_com(0x00BC);
	Write_register(0x001A);
	Write_com(0x00BF);
	Write_register((num[0]<<8)|0xFA);
	Write_register((num[2]<<8)|(num[1]));
	Write_register((num[4]<<8)|(num[3]));
	Write_register((num[6]<<8)|(num[5]));
	Write_register((num[8]<<8)|(num[7]));
	Write_register((num[10]<<8)|(num[9]));
	Write_register((num[12]<<8)|(num[11]));
	Write_register((num[14]<<8)|(num[13]));
	Write_register((num[16]<<8)|(num[15]));
	Write_register((num[18]<<8)|(num[17]));
	Write_register((num[20]<<8)|(num[19]));
	Write_register((num[22]<<8)|(num[21]));
	Write_register((num[24]<<8)|(num[23]));
	
    //0xF7
	Write_com(0x00B7);
	Write_register(0x034B);
	Write_com(0x00B8);
	Write_register(0x0000);
	Write_com(0x00BC);
	Write_register(0x0002);
	Write_com(0x00BF);
	Write_register(0x03F7);
		
	//0x29  display on
	Write_com(0x00B7);
	Write_register(0x034b);
	Write_com(0x00B8);
	Write_register(0x0000);
	Write_com(0x00BC);
	Write_register(0x0001);
	Write_com(0x00BF);
	Write_register(0x0029);
    return 0;
}
#endif
#endif

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------

LCM_DRIVER gn_ssd2825_smd_s6e8aa = 
{
	.name			= "gn_ssd2825_smd_s6e8aa",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.set_backlight  = lcm_setbacklight,
	
};

