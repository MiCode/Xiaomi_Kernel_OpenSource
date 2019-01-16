#ifdef BUILD_LK
#include <string.h>
//Evan add
#include <debug.h>
#include <sys/types.h>
#include <platform/mt_i2c.h>
//Evan add end
#else
#include <linux/string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/upmu_common.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#define TPS_I2C_BUS 0
#define TPS_I2C_ID I2C2
#define TPS_SLAVE_ADDR 0x74
#define TPS_data_size 13 //1024
const uint8_t e2prom_data[TPS_data_size] = {0xFA,0x3C,0x28,0x09,0x09,0x08,0x23,0x03,0x01,0x06,0x01,0x7D,0x7D};
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)


#define LCM_DSI_CMD_MODE    0

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static LCM_UTIL_FUNCS lcm_util = {
    .set_gpio_out = NULL,
};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)               lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

#ifdef BUILD_LK
/*----------------------------------------*/
/*--------------TPS65640------------------*/ 
static int tps65640_write_data(void)
{
	printf("Evan:%s\n",__func__);
	int ret=0;
	mt_i2c i2c = {0};
    i2c.id = TPS_I2C_ID;
    i2c.addr = TPS_SLAVE_ADDR;
    i2c.mode = ST_MODE;
    i2c.speed = 100;
    i2c.dma_en = 0;

	u8 buf1[TPS_data_size+1];
	buf1[0] = 0x00;
	//lens=TPS_data_size+1;
	int i=0;
	for(i=0;i<TPS_data_size;i++)
		buf1[i+1]=e2prom_data[i];
	ret = i2c_write(&i2c, buf1, TPS_data_size+1); 
	if(!ret)
		{
			printf("--------Evan:%s step1 error---------\n",__func__);
			return ret;
		}
	
	u8 buf2[2];
	buf2[0]=0xFF;
	buf2[1]=0x80;
	ret = i2c_write(&i2c, buf2, 2);
	if(!ret)
		{
			printf("--------Evan:%s step2 error---------\n",__func__);
			return ret;
		}
	
	return ret;

/*
	uint8_t buf1[TPS_data_size+1];
	buf1[0] = 0x00;
	int i=0,ret=0;
	for(i=0;i<TPS_data_size;i++)
		buf1[i+1]=e2prom_data[i];
	ret = i2c_transmit(TPS_I2C_BUS, TPS_SLAVE_ADDR, buf1, TPS_data_size+1);
	if(!ret)
		{
			printf("--------Evan:%s step1 error---------\n",__func__);
			return ret;
		}
	uint8_t buf2[2];
	buf2[0]=0xFF;
	buf2[1]=0x80;
	ret = i2c_transmit(TPS_I2C_BUS, TPS_SLAVE_ADDR, buf2, 2);
	if(!ret)
		{
			printf("--------Evan:%s step2 error---------\n",__func__);
			return ret;
		}
	return ret;
*/
}

static int tps65640_read_data(uint8_t *data)
{
	printf("Evan:%s\n",__func__);
	int ret=0;
	mt_i2c i2c = {0};
    i2c.id = TPS_I2C_ID;
    i2c.addr = TPS_SLAVE_ADDR;
    i2c.mode = ST_MODE;
    i2c.speed = 100;
    i2c.dma_en = 0;

	u8 buf1[2];
	buf1[0]=0xFF;
	buf1[1]=0x01;
	ret = i2c_write(&i2c, buf1, 2);
	if(!ret)
		{
			printf("--------Evan:%s step1 error---------\n",__func__);
			return ret;
		}

	uint8_t buf2=0x00;
	ret = i2c_write(&i2c, buf2, 1);
	if(!ret)
		{
			printf("--------Evan:%s step2-1 error---------\n",__func__);
			return ret;
		}
	//uint8_t buf3[TPS_data_size];
	ret=i2c_read(&i2c, data, TPS_data_size);
	if(!ret)
		{
			printf("--------Evan:%s step2-2 error---------\n",__func__);
			return ret;
		}
	return ret;
		
/*

	int ret=0;
	uint8_t buf1[2];
	buf1[0]=0xFF;
	buf1[1]=0x01;
	ret = i2c_transmit(TPS_I2C_BUS, TPS_SLAVE_ADDR, buf1, 2);
	if(!ret)
		{
			printf("--------Evan:%s step1 error---------\n",__func__);
			return ret;
		}

	uint8_t buf2=0x00;
	ret = i2c_transmit(TPS_I2C_BUS, TPS_SLAVE_ADDR, buf2, 1);
	if(!ret)
		{
			printf("--------Evan:%s step2-1 error---------\n",__func__);
			return ret;
		}
	//uint8_t buf3[TPS_data_size];
	ret=i2c_receive(TPS_I2C_BUS, TPS_SLAVE_ADDR, data, TPS_data_size);
	if(!ret)
		{
			printf("--------Evan:%s step2-2 error---------\n",__func__);
			return ret;
		}
	return ret;
*/
}



static int tps65640_e2prom_data_check(void)
{
	printf("Evan:%s\n",__func__);
	int i=0,ret=1;
	uint8_t data[TPS_data_size];
	//read e2prom data
	tps65640_read_data(data);
	//check e2prom data
	for(i=0;i<TPS_data_size;i++)
		if(data[i]!=e2prom_data[i])
			{
				ret=0;
				printf("---------Evan:e2prom_data need update--------\n");
				break;
			}
	if(ret)
		printf("---------Evan:e2prom_data is OK--------\n");
	else
		ret=tps65640_write_data();
	return ret; 
}

static int tps65640_enable()
{
	printf("Evan:%s\n",__func__);
	//todo:IC power on, 
}

/*----------------------------------------*/   
#endif


static void lcd_power_en(unsigned char enabled)
{
    if (enabled)
    {      
#ifdef BUILD_LK
        /* VGP2_PMU 3V */
/*
        pmic_config_interface(DIGLDO_CON29, 0x6, PMIC_RG_VGP2_VOSEL_MASK, PMIC_RG_VGP2_VOSEL_SHIFT);
        pmic_config_interface(DIGLDO_CON8, 0x1, PMIC_RG_VGP2_EN_MASK, PMIC_RG_VGP2_EN_SHIFT);
        */
        upmu_set_rg_vgp1_vosel(0x3);
		upmu_set_rg_vgp1_en(0x1);
#else
		/*
        upmu_set_rg_vgp2_vosel(0x6);
        upmu_set_rg_vgp2_en(0x1);
        */
        upmu_set_rg_vgp1_vosel(0x3);
		upmu_set_rg_vgp1_en(0x1);
#endif
        mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ONE);
    }
    else
    {      
#ifdef BUILD_LK
        /* VGP2_PMU 3V */
/*
        pmic_config_interface(DIGLDO_CON8, 0x0, PMIC_RG_VGP2_EN_MASK, PMIC_RG_VGP2_EN_SHIFT);
        pmic_config_interface(DIGLDO_CON29, 0x0, PMIC_RG_VGP2_VOSEL_MASK, PMIC_RG_VGP2_VOSEL_SHIFT); 
*/
	upmu_set_rg_vgp1_en(0);
	upmu_set_rg_vgp1_vosel(0);
#else
        //upmu_set_rg_vgp2_en(0x0);        
        //upmu_set_rg_vgp2_vosel(0x0);
		upmu_set_rg_vgp1_en(0);
		upmu_set_rg_vgp1_vosel(0);
#endif
        mt_set_gpio_out(GPIO_LCM_PWR, GPIO_OUT_ZERO);
    }
}


static void lcd_reset(unsigned char enabled)
{
    if (enabled)
    {
        mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    }
    else
    {	
        mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);    	
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

    params->type     = LCM_TYPE_DSI;
    params->width    = FRAME_WIDTH;
    params->height   = FRAME_HEIGHT;
    params->dsi.mode = SYNC_EVENT_VDO_MODE; //BURST_VDO_MODE;BURST_VDO_MODE;

    // DSI
    /* Command mode setting */
    params->dsi.LANE_NUM				= LCM_FOUR_LANE;
    //The following defined the fomat for data coming from LCD engine.
    params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;  //LCM_DSI_FORMAT_RGB666;

    // Video mode setting		
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;  //LCM_PACKED_PS_18BIT_RGB666;

    params->dsi.vertical_sync_active				= 4;//0;
    params->dsi.vertical_backporch					= 8;//23;
    params->dsi.vertical_frontporch 				= 8;
    params->dsi.vertical_active_line				= FRAME_HEIGHT; 

    params->dsi.horizontal_sync_active				= 4;//0;
    params->dsi.horizontal_backporch				= 132;//160;
    params->dsi.horizontal_frontporch				= 24; //100;//160;
    params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 200;//148;
}


static void init_lcm_registers(void)
{
#ifdef BUILD_LK
    printf("%s, LK \n", __func__);
#else
    printk("%s, kernel", __func__);
#endif
}

//extern void tps65640_en(void);

static void lcm_init(void)
{
    unsigned int data_array[16];

#ifdef BUILD_LK
    printf("%s, LK \n", __func__);
#else
    printk("%s, kernel", __func__);
#endif
	//tps65640_en();
    //mt_set_gpio_out(GPIO118, GPIO_OUT_ONE);
    lcd_reset(0);
    lcd_power_en(0);
    lcd_power_en(1);
    MDELAY(50);//Must > 5ms
    lcd_reset(1);
    MDELAY(200);//Must > 50ms
    
#ifdef BUILD_LK
	//extern int tps65640_e2prom_data_check(void);
	tps65640_e2prom_data_check();	
#endif
    /* software reset */
    //data_array[0] = 0x00010500;
   // dsi_set_cmdq(data_array, 1, 1);

    MDELAY(80);
	
	/* customer */
	//data_array[0] = 0xFEA11500;
	//dsi_set_cmdq(data_array, 1, 1);
	//MDELAY(80);
    /* set display on */
    //data_array[0] = 0x00290500; 
   // dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00033902;
	data_array[1] = 0x005A5AF0;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(5);
	
	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

	data_array[0] = 0x00043902;
	data_array[1] = 0x280040c3;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(170);
}


static void lcm_suspend(void)
{
    unsigned int data_array[16];

#ifdef BUILD_LK
    printf("%s, LK \n", __func__);
#else
    printk("%s, kernel", __func__);
#endif
	//mt_set_gpio_out(GPIO118, GPIO_OUT_ZERO);//set standby LOW
    /* set display off */
    //data_array[0] = 0x00280500;
   // dsi_set_cmdq(data_array, 1, 1);

    /* enter sleep mode */
   // data_array[0] = 0x00100500;
    //dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00280500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(30);

	data_array[0] = 0x00043902;
	data_array[1] = 0x200040c3;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);
	
	data_array[0] = 0x00100500;
	dsi_set_cmdq(data_array, 1, 1);
    MDELAY(30);
	
    lcd_reset(0);
    lcd_power_en(0);
    MDELAY(10);
}


static void lcm_resume(void)
{
    unsigned int data_array[16];

#ifdef BUILD_LK
    printf("%s, LK \n", __func__);
#else
    printk("%s, kernel", __func__);
#endif
	
//mt_set_gpio_out(GPIO118, GPIO_OUT_ONE);//set standby HIGH
    lcd_reset(0);
    lcd_power_en(0);
    lcd_power_en(1);
    MDELAY(50);//Must > 5ms
    lcd_reset(1);
    MDELAY(200);//Must > 50ms
    
    /* software reset */
   // data_array[0] = 0x00010500;
    //dsi_set_cmdq(data_array, 1, 1);
	/* customer */
	//data_array[0] = 0xFEA11500;
	//dsi_set_cmdq(data_array, 1, 1);
    //MDELAY(80);

    /* set display on */
  //  data_array[0] = 0x00290500; 
  //  dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x005A5AF0;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(5);
	
	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

	data_array[0] = 0x00043902;
	data_array[1] = 0x280040c3;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(170);
}


LCM_DRIVER a080ean01_dsi_vdo_lcm_drv = 
{
    .name           = "a080ean01_dsi_vdo",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
};


