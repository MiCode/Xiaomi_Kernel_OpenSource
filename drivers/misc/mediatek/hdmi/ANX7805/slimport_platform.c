/*
SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             
*/

#include "slimport_platform.h"
#include <mach/irqs.h>
/*#include "mach/eint.h"*/

#ifdef CONFIG_MTK_LEGACY
/*#include <cust_eint.h>*/
#include <linux/gpio.h>
#include <mt-plat/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_pm_ldo.h>
#endif

///#include <pmic_drv.h>

/*#include "hdmi_cust.h"*/

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#endif

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

/* GPIOs assigned to control various starter kit signals */
/*
#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
#define GPIO_MHL_INT		CUST_EINT_MHL_NUM		// BeagleBoard pin ID for TX interrupt		// 135 is pin 'SDMMC2_DAT3', which is pin 11 of EXP_HDR on BeagleBoard
#else
#define GPIO_MHL_INT 0
#endif
#endif

#ifdef GPIO_MHL_RST_B_PIN
#define GPIO_MHL_RESET		GPIO_MHL_RST_B_PIN		// BeagleBoard pin ID for TX reset		// 139 is pin 'SDMMC2_DAT7', which is pin 03 of EXP_HDR on BeagleBoard
#else
#define GPIO_MHL_RESET 0
#endif
*/

/*unsigned int eint_pin_num = 140;*/

/**
* LOG For HDMI Driver
*/

#define SLIMPORT_DBG(fmt, arg...) \
	do { \
	pr_err("[EXTD][DISP]"fmt, ##arg); \
	}while (0)

struct i2c_dev_info {
	uint8_t			dev_addr;
	struct i2c_client	*client;
};

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}


extern int I2S_Enable;
int	debug_msgs	= 3;	// print all msgs, default should be '0'
//int	debug_msgs	= 3;	// print all msgs, default should be '0'

static bool reset_on_exit = 0; // request to reset hw before unloading driver

module_param(debug_msgs, int, S_IRUGO);
module_param(reset_on_exit, bool, S_IRUGO);

#define USE_DEFAULT_I2C_CODE 0
extern struct mhl_dev_context *si_dev_context;
/*
int mhl_mutex_init(struct mutex *m)
{
	mutex_init(m);
	return 0;
}
int mhl_sw_mutex_lock(struct mutex*m)
{
	mutex_lock(m);
	return 0;
}
int mhl_sw_mutex_unlock(struct mutex*m)
{
	mutex_unlock(m);
	return 0;
}
*/
/*********************dynamic switch I2C address*******************************/
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
uint8_t reGetI2cAddress(uint8_t device_ID)
{
	uint8_t address;

	address = 0;
	switch(device_ID)
	{
		case 0x72:
			address = 0x76;
			break;
		case 0x7A:
			address = 0x7E;
			break;
		case 0x9A:
			address = 0x9E;
			break;
		case 0x92:
			address = 0x96;
			break;
		case 0xC8:
			address = 0xCC;
			break;
		case 0xA0:
			address = 0xA0;
			break;
		default:
			SLIMPORT_DBG("Error: invaild device ID\n");
	}

	return address;
}

struct i2c_client *mClient = NULL;
unsigned int mhl_eint_number = 0xffff;
unsigned int mhl_eint_gpio_number = 132;
static unsigned int mask_flag = 0;

extern wait_queue_head_t mhl_irq_wq;
extern atomic_t mhl_irq_event ;

#ifndef CONFIG_MTK_LEGACY
int get_mhl_irq_num(void)
{
    return mhl_eint_number;
}
#endif

void Mask_Slimport_Intr(bool irq_context)
{
	SLIMPORT_DBG("Mask_Slimport_Intr: in\n");
#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
	mt_eint_mask(CUST_EINT_MHL_NUM);
#endif	
#else
	SLIMPORT_DBG("Mask_Slimport_Intr: mask_flag:%d\n", mask_flag);

	if(mask_flag == 0) {
		if(irq_context)
			disable_irq_nosync(get_mhl_irq_num());
		else	
			disable_irq(get_mhl_irq_num());
		mask_flag++;
	}
#endif  

	return ;
}

void Unmask_Slimport_Intr(void)
{
	SLIMPORT_DBG("Unmask_Slimport_Intr: mask_flag:%d\n", mask_flag);
#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
	mt_eint_unmask(CUST_EINT_MHL_NUM);
#endif	
#else	
	if (mask_flag != 0) {
		enable_irq(get_mhl_irq_num());
		mask_flag = 0;
	}
#endif  
}

#ifdef CONFIG_MTK_LEGACY
#ifdef CUST_EINT_MHL_NUM
static void mhl8338_irq_handler(void)
{
 	atomic_set(&mhl_irq_event, 1);
    wake_up_interruptible(&mhl_irq_wq); 
	//mt65xx_eint_unmask(CUST_EINT_HDMI_HPD_NUM);   
}
#endif

void register_slimport_eint(void)
{
#ifdef CUST_EINT_MHL_NUM
    mt_eint_registration(CUST_EINT_MHL_NUM, CUST_EINT_MHL_TYPE, &mhl8338_irq_handler, 0);
    SLIMPORT_DBG("%s,CUST_EINT_MHL_NUM is %d \n", __func__, CUST_EINT_MHL_NUM);
#else
    SLIMPORT_DBG("%s,%d Error: CUST_EINT_MHL_NUM is not defined\n", __func__, __LINE__);
#endif    
    Mask_MHL_Intr(false);    
}

#else
/*
static irqreturn_t mhl_eint_irq_handler(int irq, void *data)
{
	atomic_set(&mhl_irq_event, 1);
    wake_up_interruptible(&mhl_irq_wq); 
    
    Mask_MHL_Intr(true);
	return IRQ_HANDLED;
}
*/
extern irqreturn_t anx7805_cbl_det_isr(int irq, void *data);
void register_slimport_eint(void)
{
    struct device_node *node = NULL;
    u32 ints[2]={0, 0};

	SLIMPORT_DBG("register_slimport_eint\n");
    node = of_find_compatible_node(NULL, NULL, "mediatek,extd_dev");
    if(node)
    {
        of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		///mt_eint_set_hw_debounce(ints[0],ints[1]);
		/*mt_gpio_set_debounce(ints[0],ints[1]);*/
		mhl_eint_number = irq_of_parse_and_map(node, 0);
		SLIMPORT_DBG("mhl_eint_number, node %p-irq %d!!\n", node, get_mhl_irq_num());
		/*irq_set_irq_type(mhl_eint_number,IRQ_TYPE_EDGE_RISING);*/
		gpio_set_debounce(mhl_eint_gpio_number, 50000);    /*debounce time is microseconds*/
    	/*if(request_irq(mhl_eint_number, anx7805_cbl_det_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "mediatek,sii8348-hdmi", NULL))*/ ///IRQF_TRIGGER_LOW
    	if(request_irq(mhl_eint_number, anx7805_cbl_det_isr, IRQ_TYPE_LEVEL_HIGH, "mediatek,extd_dev", NULL))
    	{
    		 SLIMPORT_DBG("request_irq fail\n");
    	}
    	else
    	{
    	    	SLIMPORT_DBG("MHL EINT IRQ available, node %p-irq %d!!\n", node, get_mhl_irq_num());
    	    	return;
		}
	}
	else
	{
		SLIMPORT_DBG("of_find_compatible_node error\n");
	}
	Mask_Slimport_Intr(false);
    SLIMPORT_DBG("Error: MHL EINT IRQ NOT AVAILABLE, node %p-irq %d!!\n", node, get_mhl_irq_num());
}



struct pinctrl *mhl_pinctrl;
struct pinctrl_state *pin_state;
extern struct device *ext_dev_context;
struct regulator *reg_v12_power = NULL;

char* dpi_gpio_name[32] = {
"dpi_d0_def", "dpi_d0_cfg","dpi_d1_def", "dpi_d1_cfg","dpi_d2_def", "dpi_d2_cfg","dpi_d3_def", "dpi_d3_cfg",	
"dpi_d4_def", "dpi_d4_cfg","dpi_d5_def", "dpi_d5_cfg","dpi_d6_def", "dpi_d6_cfg","dpi_d7_def", "dpi_d7_cfg",	
"dpi_d8_def", "dpi_d8_cfg","dpi_d9_def", "dpi_d9_cfg","dpi_d10_def", "dpi_d10_cfg","dpi_d11_def", "dpi_d11_cfg",	
"dpi_ck_def", "dpi_ck_cfg","dpi_de_def", "dpi_de_cfg","dpi_hsync_def", "dpi_hsync_cfg","dpi_vsync_def", "dpi_vsync_cfg"
};

char* i2s_gpio_name[10] = {
"i2s_dat_def","i2s_dat_cfg","i2s_dat1_def","i2s_dat1_cfg","i2s_dat2_def","i2s_dat2_cfg","i2s_ws_def","i2s_ws_cfg","i2s_ck_def","i2s_ck_cfg"
};

char* rst_gpio_name[2] ={
    "rst_low_cfg", "rst_high_cfg"
};

char* pd_gpio_name[2] ={
    "pd_low_cfg", "pd_high_cfg"
};

void dpi_gpio_ctrl(int enable)
{
    int offset = 0;
    int ret = 0;
    SLIMPORT_DBG("dpi_gpio_ctrl+  %ld !!\n",sizeof(dpi_gpio_name)); 
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        SLIMPORT_DBG("Cannot find MHL RST pinctrl for dpi_gpio_ctrl!\n");
        return;
    }  
    
    if(enable)
        offset = 1;

    for(; offset < 32 ;)
    {
        pin_state = pinctrl_lookup_state(mhl_pinctrl, dpi_gpio_name[offset]);
        if (IS_ERR(pin_state)) {
            ret = PTR_ERR(pin_state);
            SLIMPORT_DBG("Cannot find MHL pinctrl--%s!!\n", dpi_gpio_name[offset]);
        }
        else
            pinctrl_select_state(mhl_pinctrl, pin_state); 
            
        offset +=2;
    }
}

void i2s_gpio_ctrl(int enable)
{
    int offset = 0;
    int ret = 0;

    SLIMPORT_DBG("i2s_gpio_ctrl+  %ld !!\n", sizeof(i2s_gpio_name)); 
    
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        SLIMPORT_DBG("Cannot find MHL RST pinctrl for i2s_gpio_ctrl!\n");
        return;
    }  
    
    if(enable)
        offset = 1;
    for(; offset < 10 ;)
    {

        pin_state = pinctrl_lookup_state(mhl_pinctrl, i2s_gpio_name[offset]);
        if (IS_ERR(pin_state)) {
            ret = PTR_ERR(pin_state);
            SLIMPORT_DBG("Cannot find MHL pinctrl--%s!!\n", i2s_gpio_name[offset]);
        }
        else
            pinctrl_select_state(mhl_pinctrl, pin_state);   
        
        offset +=2;
    }
}

void mhl_power_ctrl(int enable)
{

}

void reset_mhl_board(int hwResetPeriod, int hwResetDelay, int is_power_on)
{
    struct pinctrl_state *rst_low_state = NULL;
    struct pinctrl_state *rst_high_state = NULL;
    int err_cnt = 0;
    int ret = 0;
    
    SLIMPORT_DBG("reset_mhl_board+  %ld, is_power_on: %d !!\n", sizeof(rst_gpio_name), is_power_on); 
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        SLIMPORT_DBG("Cannot find MHL RST pinctrl for reset_mhl_board!\n");
        return;
    }    
    
    rst_low_state = pinctrl_lookup_state(mhl_pinctrl, rst_gpio_name[0]);
    if (IS_ERR(rst_low_state)) {
        ret = PTR_ERR(rst_low_state);
        SLIMPORT_DBG("Cannot find MHL pinctrl--%s!!\n", rst_gpio_name[0]);
        err_cnt++;
    }
    
    rst_high_state = pinctrl_lookup_state(mhl_pinctrl, rst_gpio_name[1]);
    if (IS_ERR(rst_high_state)) {
        ret = PTR_ERR(rst_high_state);
        SLIMPORT_DBG("Cannot find MHL pinctrl--%s!!\n", rst_gpio_name[1]);
        err_cnt++;
    }

    if(err_cnt > 0)
        return;

	if(is_power_on) {
		pinctrl_select_state(mhl_pinctrl, rst_high_state);	 
		mdelay(hwResetPeriod);
		pinctrl_select_state(mhl_pinctrl, rst_low_state);	
		mdelay(hwResetPeriod);
		pinctrl_select_state(mhl_pinctrl, rst_high_state);	 
		mdelay(hwResetDelay);
	} else {
		pinctrl_select_state(mhl_pinctrl, rst_low_state);	
		mdelay(hwResetPeriod);
	}
}

void set_pin_high_low(PIN_TYPE pin, bool is_high)
{
    struct pinctrl_state *pin_state = NULL;
	char* str = NULL;
    int ret = 0;
    
    SLIMPORT_DBG("reset_mhl_board+  %d, is_power_on: %d !!\n", pin, is_high);
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        SLIMPORT_DBG("Cannot find MHL RST pinctrl for reset_mhl_board!\n");
        return;
    }  

	if (pin == RESET_PIN && is_high == false)
		str = rst_gpio_name[0];
	else if (pin == RESET_PIN && is_high == true)
		str = rst_gpio_name[1];
	else if (pin == PD_PIN && is_high == false)
		str = pd_gpio_name[0];
	else if (pin == PD_PIN && is_high == true)
		str = pd_gpio_name[1];

	SLIMPORT_DBG("pinctrl--%s!!\n", str);
	pin_state = pinctrl_lookup_state(mhl_pinctrl, str);
	if (IS_ERR(pin_state)) {
		ret = PTR_ERR(pin_state);
		SLIMPORT_DBG("Cannot find MHL pinctrl--%s!!\n", str);
	}

	pinctrl_select_state(mhl_pinctrl, pin_state);
	return;
}

void cust_power_init(void)
{

}

void cust_power_on(int enable)
{
/*
    if(enable)
        regulator_enable(reg_v12_power);
    else
        regulator_disable(reg_v12_power);
*/
}

void slimport_platform_init(void)
{
    int ret =0;
/*
    struct device_node *kd_node =NULL;
    const char *name = NULL;
*/
    
    SLIMPORT_DBG("mhl_platform_init start !!\n");

    if(ext_dev_context == NULL)
    {
        SLIMPORT_DBG("Cannot find device in platform_init!\n");
        goto plat_init_exit;
        
    }
    mhl_pinctrl = devm_pinctrl_get(ext_dev_context);
    if (IS_ERR(mhl_pinctrl)) {
        ret = PTR_ERR(mhl_pinctrl);
        SLIMPORT_DBG("Cannot find MHL Pinctrl!!!!\n");
        goto plat_init_exit;
    }
    
    pin_state = pinctrl_lookup_state(mhl_pinctrl, rst_gpio_name[1]);
    if (IS_ERR(pin_state)) {
        ret = PTR_ERR(pin_state);
        SLIMPORT_DBG("Cannot find MHL RST pinctrl low!!\n");
    }
    else
        pinctrl_select_state(mhl_pinctrl, pin_state);

    SLIMPORT_DBG("mhl_platform_init reset gpio init done!!\n");

    i2s_gpio_ctrl(0);
    dpi_gpio_ctrl(0);
/*  
    kd_node = of_find_compatible_node(NULL, NULL, "mediatek,regulator_supply");
  
    if(kd_node)
    {
		name = of_get_property(kd_node, "MHL_POWER_LD01", NULL);
		if (name == NULL)
		{
		    SLIMPORT_DBG("mhl_platform_init can't get MHL_POWER_LD01!\n");
		    
		    if (reg_v12_power == NULL)
		    {
		 	    reg_v12_power = regulator_get(ext_dev_context, "HDMI_12v");		 	    
		    }
		}
		else
		{			
			kd_node = ext_dev_context->of_node;
			ext_dev_context->of_node = of_find_compatible_node(NULL,NULL,"mediatek,regulator_supply");			
		    if (reg_v12_power == NULL) 
		    {
		 	    reg_v12_power = regulator_get(ext_dev_context, "mhl_12v");
		    }
		    SLIMPORT_DBG("mhl_platform_init regulator name !\n" );
		    ext_dev_context->of_node = kd_node;
		}
	}
	else
	{
			SLIMPORT_DBG("mhl_platform_init get node failed!\n");
			goto plat_init_exit  ;
	}
	

    if (IS_ERR(reg_v12_power))
    {
        SLIMPORT_DBG("mhl_platform_init ldo %p!!!!!!!!!!!!!!\n", reg_v12_power );
    }
    else
    {        
        SLIMPORT_DBG("mhl_platform_init ldo init %p\n", reg_v12_power );
        regulator_set_voltage(reg_v12_power, 1200000, 1200000);
        
    }
*/
plat_init_exit:
    SLIMPORT_DBG("mhl_platform_init init done !!\n");
    
}

#endif
