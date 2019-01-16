/* BEGIN PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
//add Touch driver for G610-T11
/* BEGIN PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* BEGIN PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* BEGIN PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
#if 1 /* def CONFIG_CYPRESS_TTSP */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <mach/mt_boot.h>

#include "cyttsp4_bus.h"
#include "cyttsp4_core.h"
#include "cyttsp4_i2c.h"
#include "cyttsp4_btn.h"
#include "cyttsp4_mt.h"

#include <mach/mt_gpio.h>
#include <mach/mt_typedefs.h>
#include <linux/input.h>
#include <mach/mt_pm_ldo.h>
#ifdef HW_HAVE_TP_THREAD
#include <mach/pmic_mt6323_sw.h>
#endif
#include <mach/eint.h>
#include <cust_eint.h>
#include <cust_gpio_usage.h>
#include <mach/sync_write.h>

/* BEGIN PN: DTS2013031908354  ,Added by l00184147, 2013/3/19*/
//#include <linux/hardware_self_adapt.h>
/* END PN: DTS2013031908354  ,Added by l00184147, 2013/3/19*/

#define CYTTSP4_I2C_TCH_ADR 0x1a
#define CYTTSP4_I2C_IRQ_GPIO 70	/* sample value from Blue */
#define CYTTSP4_I2C_RST_GPIO 10	/* sample value from Blue */

//#define CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
/* BEGIN PN:SPBB-1254 ,Added by F00184246, 2013/2/18*/
#define CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE 1
/* END PN:SPBB-1254 ,Added by F00184246, 2013/2/18*/


extern int tpd_type_cap;
extern int tpd_load_status;
#if 0
#define CUST_EINT_TOUCH_PANEL_NUM              5
#define CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN      0
#define CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE
#define CUST_EINTF_TRIGGER_FALLING  2

#define CUST_EINT_POLARITY_LOW              0
#define CUST_EINT_POLARITY_HIGH             1
#define CUST_EINT_DEBOUNCE_DISABLE          0
#define CUST_EINT_DEBOUNCE_ENABLE           1
#define CUST_EINT_EDGE_SENSITIVE            0
#define CUST_EINT_LEVEL_SENSITIVE           1
extern void printGPIO_Status(int gpioIdx);  // printGPIO_Status(GPIO9)

/* // Orig Eint5 */
#define GPIO_CTP_EINT_PIN         GPIO5 //for G610-T11 
#define GPIO_CTP_EINT_PIN_M_GPIO  GPIO_MODE_00
#define GPIO_CTP_EINT_PIN_M_EINT  GPIO_MODE_00

#define GPIO_CTP_RST_PIN         GPIO8  //for G610-T11

#define GPIO_CTP_RST_PIN_M_GPIO  GPIO_MODE_00
#endif

//lm DMA
#include <linux/dma-mapping.h> 

static u8 *I2CDMABuf_va = NULL;
static u32 I2CDMABuf_pa = NULL;
  
static void cyttsp4_init_i2c_alloc_dma_buffer(void)
{
  I2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL);
  if(!I2CDMABuf_va){
	  pr_err("Allocate DMA I2C Buffer failed!\n");
	}
  else{
	  pr_err("Allocate DMA I2C Buffer Success!\n");
	}

}
static void cyttsp4_init_i2c_free_dma_buffer(void)
{
  dma_free_coherent(NULL, 4096, I2CDMABuf_va, I2CDMABuf_pa);
  I2CDMABuf_va = NULL;
  I2CDMABuf_pa = 0;
  pr_info("Free DMA I2C Buffer Success!\n");
}

int cyttsp4_MTK_i2c_write(struct i2c_client *client, const uint8_t *buf, int len)
{
  int i = 0;

  if(len <= 8){
	  //pr_info("cyttsp4_MTK_i2c_write() length < 8! Normal mode\n");
	  client->addr = client->addr & I2C_MASK_FLAG;
	  return i2c_master_send(client, buf, len);
	}
  else{
	  //pr_info("cyttsp4_MTK_i2c_write() length > 8! DMA mode\n");
	  for(i = 0 ; i < len; i++){
		I2CDMABuf_va[i] = buf[i];
	  }

	  client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
	  return i2c_master_send(client, I2CDMABuf_pa, len);
	}    
}

int cyttsp4_MTK_i2c_read(struct i2c_client *client, uint8_t *buf, int len)
{
  int i = 0, err = 0;

  if(len <= 8){
	  //pr_info("cyttsp4_MTK_i2c_read() length < 8! Normal mode\n");
	  client->addr = client->addr & I2C_MASK_FLAG;
	  return i2c_master_recv(client, buf, len);
	}
  else{
	  //pr_info("cyttsp4_MTK_i2c_read() length > 8! DMA mode\n");
	  client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
	  err = i2c_master_recv(client, I2CDMABuf_pa, len);
                   
	  if(err < 0){
		  return err;
		}

	  for(i = 0; i < len; i++){
		  buf[i] = I2CDMABuf_va[i];
		}
	  return err;

	}
}


extern void eint_interrupt_handler(void) ;
/* BEGIN PN:DTS2013033006231 ,Deleted by l00184147, 2013/3/27*/ 
/* END PN:DTS2013033006231 ,Deleted by l00184147, 2013/3/27*/ 


void cyttsp4_mtk_gpio_interrupt_register()
{
	//mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINTF_TRIGGER_FALLING, eint_interrupt_handler, 0);
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
}

void cyttsp4_mtk_gpio_interrupt_enable()
{
  mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}

void cyttsp4_mtk_gpio_interrupt_disable()
{
  mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
}


static int cyttsp4_xres(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{
  int rc = 0;
  
  mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);  
  msleep(20);
  mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
  msleep(40);
  mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);  
  msleep(20);
  
  dev_info(dev,"%s: RESET CYTTSP gpio=%d r=%d\n", __func__,GPIO_CTP_RST_PIN, rc);
  
  return rc;
}

//add begin by linghai
/* BEGIN PN:DTS2013020108492  ,Modified by l00184147, 2013/1/26*/ 
static ssize_t cyttps4_virtualkeys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		__stringify(EV_KEY) ":"
		__stringify(KEY_BACK) ":120:1340:200:100"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_HOMEPAGE) ":360:1340:200:100"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_MENU) ":600:1340:200:100"
		"\n");
}
/* END PN:DTS2013020108492  ,Modified by l00184147, 2013/1/26*/ 

static struct kobj_attribute cyttsp4_virtualkeys_attr = {
	.attr = {		
        	.name = "virtualkeys.mtk-tpd",
		//.name = "virtualkeys.cyttsp4_mt",
		.mode = S_IRUGO,
	},
	.show = &cyttps4_virtualkeys_show,
};

static struct attribute *cyttsp4_properties_attrs[] = {
	&cyttsp4_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group cyttsp4_properties_attr_group = {
	.attrs = cyttsp4_properties_attrs,
};
// add end by linghai


static int cyttsp4_init(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
  	printk("cyttsp4_init\n");

	/* BEGIN PN: DTS2013031908354  ,Added by l00184147, 2013/3/19*/
	//hw_product_type board_id;
	//board_id=get_hardware_product_version();
	/* END PN: DTS2013031908354  ,Added by l00184147, 2013/3/19*/

	int rc = 0;
	struct kobject *properties_kobj;
	kal_uint16 temp;
	int ret;
	
	tpd_type_cap = 1;
	tpd_load_status = 1;
	/* BEGIN PN: DTS2013053100307  ,Modified by l00184147, 2013/05/31*/
	/* BEGIN PN: DTS2013041600131  ,Modified by l00184147, 2013/4/16*/
	/* BEGIN PN: DTS2013031908354  ,Modified by l00184147, 2013/3/19*/
	if (on) {
		cyttsp4_init_i2c_alloc_dma_buffer();
		
		/* BEGIN PN: DTS2013060600352 ,Deleted by l00184147, 2013/06/06*/
		//pull up reset pin after poweron the touch controller
		/* END PN: DTS2013060600352 ,Deleted by l00184147, 2013/06/06*/

		mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
		mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
   		temp = DRV_Reg(0xF0005920);
   		temp |= (0x04);
   		mt65xx_reg_sync_writew(temp, 0xF0005920);

		if(1/*(board_id & HW_VER_MAIN_MASK) == HW_G750_VER*/)
		{
#ifdef HW_HAVE_TP_THREAD		//for HUAWEI
			//increasing VGP2 to 1.85, please help to measure it from HW
			hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_2800, "TP");
			hwPowerOn(MT6323_POWER_LDO_VGP3, VOL_1800, "TP");
    			//pmic_config_interface(0x0534, 0xd, 0xf, 8);	//+60mV
    			pmic_config_interface(0x0534, 0xb, 0xf, 8);	//+100mV
#else
			//TODO
#endif
			properties_kobj = kobject_create_and_add("board_properties", NULL);
	  		if (properties_kobj)
			ret = sysfs_create_group(properties_kobj,
					&cyttsp4_properties_attr_group);

			if (!properties_kobj || ret)
			pr_err("%s: failed to create board_properties\n", __func__);
		}
		else
			pr_err("power on cyttsp4 error\n");
		/* BEGIN PN: DTS2013060600352 ,Added by l00184147, 2013/06/06*/
		//pull up reset pin after poweron the touch controller
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		/* END PN: DTS2013060600352 ,Added by l00184147, 2013/06/06*/
	}
	else {
			if(1/*(board_id & HW_VER_MAIN_MASK) == HW_G750_VER*/)
			{
				hwPowerDown(MT6323_POWER_LDO_VGP1, "TP");
				hwPowerDown(MT6323_POWER_LDO_VGP2, "TP");
			}
			else
				pr_err("power down cyttsp4 error\n");
				
	  		cyttsp4_init_i2c_free_dma_buffer();
	}
	/* END PN: DTS2013031908354  ,Modified by l00184147, 2013/3/19*/
	/* END PN: DTS2013041600131  ,Modified by l00184147, 2013/4/16*/
	/* END PN: DTS2013053100307  ,Modified by l00184147, 2013/05/31*/
	return rc;
}

static int cyttsp4_wakeup(struct device *dev)
{
	int rc = 0;

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EINT_PIN, GPIO_OUT_ZERO);
	udelay(2000);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	dev_info(dev,
		"%s: WAKEUP CYTTSP gpio=%d r=%d\n", __func__,
		GPIO_CTP_EINT_PIN, rc);
	return rc;
}

static int cyttsp4_sleep(struct device *dev)
{
	return 0;
}

static int cyttsp4_power(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	if (on)
		return cyttsp4_wakeup(dev);

	return cyttsp4_sleep(dev);
}
/* BEGIN PN: DTS2013021602307 ,Modified by l00184147, 2013/2/16*/
/* Button to keycode conversion */
static u16 cyttsp4_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_BACK,		/* 158 */
	KEY_HOMEPAGE,	/* 172 */
	KEY_MENU,		/* 139 */
	KEY_SEARCH,		/* 217 */
	KEY_VOLUMEDOWN,	/* 114 */
	KEY_VOLUMEUP,		/* 115 */
	KEY_CAMERA,		/* 212 */
	KEY_POWER		/* 116 */
};
/* END PN: DTS2013021602307 ,Modified by l00184147, 2013/2/16*/

static struct touch_settings cyttsp4_sett_btn_keys = {
	.data = (uint8_t *)&cyttsp4_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp4_btn_keys),
	.tag = 0,
};

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
#include "HUAWEI_G700_FW_V0004.h"
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
/* BEGIN PN:SPBB-1254 ,Modified by F00184246, 2013/2/18*/
#include "cyttsp4_params.h"
/* END PN:SPBB-1254 ,Modified by F00184246, 2013/2/18*/
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *)&cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *)&cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};




#else
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = NULL,
	.size = 0,
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = NULL,
	.size = 0,
	.tag = 0,
};
#endif



/* BEGIN PN:DTS2013053100307 ,Added by l00184147, 2013/05/31*/
#include "Ofilm_G750_config.h"
static struct touch_settings cyttsp4_G750_sett_ofilm_param_regs = {
       .data = (uint8_t *)&cyttsp4_G750_ofilm_param_regs[0],
       .size = ARRAY_SIZE(cyttsp4_G750_ofilm_param_regs),
       .tag = 0,
};

#include "Truely_G750_config.h"
static struct touch_settings cyttsp4_G750_sett_truly_param_regs = {
       .data = (uint8_t *)&cyttsp4_G750_truly_param_regs[0],
       .size = ARRAY_SIZE(cyttsp4_G750_truly_param_regs),
       .tag = 0,
};

struct cyttsp4_sett_param_map cyttsp4_G750_config_param_map[] = {
    
	[0] = {
			  .id = 0,
			  .param = &cyttsp4_G750_sett_ofilm_param_regs,
		  },
	
	[1] = {
			  .id = 2,
			  .param = &cyttsp4_G750_sett_truly_param_regs,
		  },
       [2] = {
			  .param = NULL,
		  },
		  
};

static struct cyttsp4_loader_platform_data _cyttsp4_G750_loader_platform_data = {
	.fw = &cyttsp4_firmware,
	.param_regs = &cyttsp4_sett_param_regs,
	.param_size = &cyttsp4_sett_param_size,
	.param_map =cyttsp4_G750_config_param_map,  
	.flags = 1,
};

static struct cyttsp4_core_platform_data _cyttsp4_G750_core_platform_data = {
	.irq_gpio = CYTTSP4_I2C_IRQ_GPIO,
	.use_configure_sensitivity = 1,
	.xres = cyttsp4_xres,
	.init = cyttsp4_init,
	.power = cyttsp4_power,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL, /* &cyttsp4_sett_param_regs, */
		NULL, /* &cyttsp4_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp4_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp4_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		&cyttsp4_sett_btn_keys,	/* button-to-keycode table */
	},
	.loader_pdata = & _cyttsp4_G750_loader_platform_data,
};
/* END PN:DTS2013053100307 ,Added by l00184147, 2013/05/31*/

#define CY_MAXX 880
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

/* BEGIN PN: DTS2013031908354  ,Modified by l00184147, 2013/3/19*/
static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -128, 127, 0, 0,
};
/* END PN: DTS2013031908354  ,Modified by l00184147, 2013/3/19*/

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *)&cyttsp4_abs[0],
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};
#endif /* CONFIG_CYPRESS_TTSP */


/* BEGIN PN:DTS2013022102040,Modified by l00211038, 2013/02/21*/
static struct i2c_board_info mtk_ttsp_i2c_tpd=
/* END   PN: DTS2013022102040,Modified by l00211038, 2013/02/21*/
{ // KEVKEV
		I2C_BOARD_INFO(CYTTSP4_I2C_NAME, CYTTSP4_I2C_TCH_ADR),
		.irq =  -1,
		.platform_data = CYTTSP4_I2C_NAME,
};

/* BEGIN PN:DTS2013053100307 ,Added by l00184147, 2013/05/31*/
struct cyttsp4_core_info cyttsp4_G750_core_info = {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
	.platform_data = &_cyttsp4_G750_core_platform_data,
};
/* END PN:DTS2013053100307 ,Added by l00184147, 2013/05/31*/



/* BEGIN PN: DTS2013021602307 ,Modified by l00184147, 2013/2/16*/
static struct cyttsp4_mt_platform_data _cyttsp4_mt_virtualkey_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = 0x40,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

/* BEGIN PN:DTS2013022102040,Modified by l00211038, 2013/02/21*/
struct cyttsp4_device_info cyttsp4_mt_virtualkey_info  = {
/* END   PN: DTS2013022102040,Modified by l00211038, 2013/02/21*/
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_mt_virtualkey_platform_data,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_novirtualkey_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = 0x00,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

/* BEGIN PN:DTS2013022102040,Modified by l00211038, 2013/02/21*/
struct cyttsp4_device_info cyttsp4_mt_novirtualkey_info  = {
/* END   PN: DTS2013022102040,Modified by l00211038, 2013/02/21*/
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_mt_novirtualkey_platform_data,
};
/* END PN: DTS2013021602307 ,Modified by l00184147, 2013/2/16*/

static struct cyttsp4_btn_platform_data _cyttsp4_btn_platform_data = {
	.inp_dev_name = CYTTSP4_BTN_NAME,
};

/* BEGIN PN:DTS2013022102040,Modified by l00211038, 2013/02/21*/
struct cyttsp4_device_info cyttsp4_btn_info = {
/* END   PN: DTS2013022102040,Modified by l00211038, 2013/02/21*/
	.name = CYTTSP4_BTN_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_btn_platform_data,
};

/* BEGIN PN:DTS2013033006231 ,Modified by l00184147, 2013/3/27*/
static int __init tpd_ttsp_init(void) {
  printk("MediaTek TTDA ttsp touch panel driver init\n");
  i2c_register_board_info(0, &mtk_ttsp_i2c_tpd, 1);
  return 0;
}
/* END PN:DTS2013033006231 ,Modified by l00184147, 2013/1/27*/
module_init(tpd_ttsp_init);
/* END PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/* END PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* END PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* END PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
