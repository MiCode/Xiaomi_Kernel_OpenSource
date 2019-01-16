#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <cust_gpio_usage.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#include <linux/mutex.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#endif

#include <linux/i2c.h>
#include <linux/leds.h>



/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE
#define TAG_NAME "leds_strobe.c"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        xlog_printk(ANDROID_LOG_WARNING, TAG_NAME, KERN_WARNING  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_NOTICE  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        xlog_printk(ANDROID_LOG_INFO   , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME,  "<%s>\n", __FUNCTION__);
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	#define PK_DBG PK_DBG_FUNC
	#define PK_VER PK_TRC_VERBOSE
	#define PK_ERR PK_ERROR
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/

static DEFINE_SPINLOCK(g_strobeSMPLock); /* cotta-- SMP proection */
extern void flashlight_clear_brightness(void);

static u32 strobe_Res = 0;
static u32 strobe_Timeus = 0;
static BOOL g_strobe_On = 0;


static int g_timeOutTimeMs=0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
static DEFINE_MUTEX(g_strobeSem);
#else
static DECLARE_MUTEX(g_strobeSem);
#endif


#define STROBE_DEVICE_ID 0x63


static struct work_struct workTimeOut;

//#define FLASH_GPIO_ENF GPIO12
//#define FLASH_GPIO_ENT GPIO13
#define GPIO_LED_EN  GPIO_CAMERA_FLASH_EXT1_PIN


#define LM3643_REG_ENABLE      0x01
#define LM3643_REG_LED1_FLASH  0x03
#define LM3643_REG_LED2_FLASH  0x04
#define LM3643_REG_LED1_TORCH  0x05
#define LM3643_REG_LED2_TORCH  0x06
#define LM3643_REG_TIMING	   0x08		


// // enum lm3643_devid { 
// // 	ID_FLASH0 = 0x0, 
// // 	ID_FLASH1, 
// // 	ID_TORCH0, 
// // 	ID_TORCH1, 
// // 	ID_MAX 
// // }; 
 
// // enum lm3643_mode { 
// // 	MODE_STDBY = 0x0, 
// // 	MODE_IR, 
// // 	MODE_TORCH, 
// // 	MODE_FLASH, 
// // 	MODE_MAX 
// // }; 
 
// // enum lm3643_devfile { 
// // 	DFILE_FLASH0_ENABLE = 0, 
// // 	DFILE_FLASH0_ONOFF, 
// // 	DFILE_FLASH0_SOURCE, 
// // 	DFILE_FLASH0_TIMEOUT, 
// // 	DFILE_FLASH1_ENABLE, 
// // 	DFILE_FLASH1_ONOFF, 
// // 	DFILE_TORCH0_ENABLE, 
// // 	DFILE_TORCH0_ONOFF, 
// // 	DFILE_TORCH0_SOURCE, 
// // 	DFILE_TORCH1_ENABLE, 
// // 	DFILE_TORCH1_ONOFF, 
// // 	DFILE_MAX 
// // }; 
 
// //#define to_lm3643(_ctrl, _no) container_of(_ctrl, struct lm3643, cdev[_no]) 
// #define LM3643_NAME "lm3643"

// static struct i2c_board_info __initdata lm3643_dev={ I2C_BOARD_INFO(LM3643_NAME, 0x63)};

// // struct lm3643 { 
// // 	struct device *dev; 
 
// // 	u8 brightness[ID_MAX]; 
// // 	struct work_struct work[ID_MAX]; 
// // 	struct led_classdev cdev[ID_MAX]; 
 
// // 	struct lm3643_platform_data *pdata; 
// // 	struct regmap *regmap; 
// // 	struct mutex lock; 
// // }; 
struct i2c_client *g_lm3643_i2c_client = NULL;
EXPORT_SYMBOL(g_lm3643_i2c_client);
// // static void lm3643_read_flag(struct lm3643 *pchip) 
// // { 
 
// // 	int rval; 
// // 	unsigned int flag0, flag1; 
 
// // 	rval = regmap_read(pchip->regmap, REG_FLAG0, &flag0); 
// // 	rval |= regmap_read(pchip->regmap, REG_FLAG1, &flag1); 
 
// // 	if (rval < 0) 
// // 		dev_err(pchip->dev, "i2c access fail.\n"); 
 
// // 	dev_info(pchip->dev, "[flag1] 0x%x, [flag0] 0x%x\n", 
// // 		 flag1 & 0x1f, flag0); 
// // } 
 
// // /* torch0 brightness control */ 
// // static void lm3643_deferred_torch0_brightness_set(struct work_struct *work) 
// // { 
// // 	struct lm3643 *pchip = container_of(work, 
// // 					    struct lm3643, work[ID_TORCH0]); 
 
// // 	if (regmap_update_bits(pchip->regmap, 
// // 			       REG_TORCH_LED0_BR, 0x7f, 
// // 			       pchip->brightness[ID_TORCH0])) 
// // 		dev_err(pchip->dev, "i2c access fail.\n"); 
// // 	lm3643_read_flag(pchip); 
// // } 
 
// // static void lm3643_torch0_brightness_set(struct led_classdev *cdev, 
// // 					 enum led_brightness brightness) 
// // { 
// // 	struct lm3643 *pchip = 
// // 	    container_of(cdev, struct lm3643, cdev[ID_TORCH0]); 
 
// // 	pchip->brightness[ID_TORCH0] = brightness; 
// // 	schedule_work(&pchip->work[ID_TORCH0]); 
// // } 
 
// // /* torch1 brightness control */ 
// // static void lm3643_deferred_torch1_brightness_set(struct work_struct *work) 
// // { 
// // 	struct lm3643 *pchip = container_of(work, 
// // 					    struct lm3643, work[ID_TORCH1]); 
 
// // 	if (regmap_update_bits(pchip->regmap, 
// // 			       REG_TORCH_LED1_BR, 0x7f, 
// // 			       pchip->brightness[ID_TORCH1])) 
// // 		dev_err(pchip->dev, "i2c access fail.\n"); 
// // 	lm3643_read_flag(pchip); 
// // } 
 
// // static void lm3643_torch1_brightness_set(struct led_classdev *cdev, 
// // 					 enum led_brightness brightness) 
// // { 
// // 	struct lm3643 *pchip = 
// // 	    container_of(cdev, struct lm3643, cdev[ID_TORCH1]); 
 
// // 	pchip->brightness[ID_TORCH1] = brightness; 
// // 	schedule_work(&pchip->work[ID_TORCH1]); 
// // } 
 
// // /* flash0 brightness control */ 
// // static void lm3643_deferred_flash0_brightness_set(struct work_struct *work) 
// // { 
// // 	struct lm3643 *pchip = container_of(work, 
// // 					    struct lm3643, work[ID_FLASH0]); 
 
// // 	if (regmap_update_bits(pchip->regmap, 
// // 			       REG_FLASH_LED0_BR, 0x7f, 
// // 			       pchip->brightness[ID_FLASH0])) 
// // 		dev_err(pchip->dev, "i2c access fail.\n"); 
// // 	lm3643_read_flag(pchip); 
// // } 
 
// // static void lm3643_flash0_brightness_set(struct led_classdev *cdev, 
// // 					 enum led_brightness brightness) 
// // { 
// // 	struct lm3643 *pchip = 
// // 	    container_of(cdev, struct lm3643, cdev[ID_FLASH0]); 
 
// // 	pchip->brightness[ID_FLASH0] = brightness; 
// // 	schedule_work(&pchip->work[ID_FLASH0]); 
// // } 
 
// // /* flash1 brightness control */ 
// // static void lm3643_deferred_flash1_brightness_set(struct work_struct *work) 
// // { 
// // 	struct lm3643 *pchip = container_of(work, 
// // 					    struct lm3643, work[ID_FLASH1]); 
 
// // 	if (regmap_update_bits(pchip->regmap, 
// // 			       REG_FLASH_LED1_BR, 0x7f, 
// // 			       pchip->brightness[ID_FLASH1])) 
// // 		dev_err(pchip->dev, "i2c access fail.\n"); 
// // 	lm3643_read_flag(pchip); 
// // } 
 
// // static void lm3643_flash1_brightness_set(struct led_classdev *cdev, 
// // 					 enum led_brightness brightness) 
// // { 
// // 	struct lm3643 *pchip = 
// // 	    container_of(cdev, struct lm3643, cdev[ID_FLASH1]); 
 
// // 	pchip->brightness[ID_FLASH1] = brightness; 
// // 	schedule_work(&pchip->work[ID_FLASH1]); 
// // } 
 
// // struct lm3643_devices { 
// // 	struct led_classdev cdev; 
// // 	work_func_t func; 
// // }; 
 
// // static struct lm3643_devices lm3643_leds[ID_MAX] = { 
// // 	[ID_FLASH0] = { 
// // 		       .cdev.name = "flash0", 
// // 		       .cdev.brightness = 0, 
// // 		       .cdev.max_brightness = 0x7f, 
// // 		       .cdev.brightness_set = lm3643_flash0_brightness_set, 
// // 		       .cdev.default_trigger = "flash0", 
// // 		       .func = lm3643_deferred_flash0_brightness_set}, 
// // 	[ID_FLASH1] = { 
// // 		       .cdev.name = "flash1", 
// // 		       .cdev.brightness = 0, 
// // 		       .cdev.max_brightness = 0x7f, 
// // 		       .cdev.brightness_set = lm3643_flash1_brightness_set, 
// // 		       .cdev.default_trigger = "flash1", 
// // 		       .func = lm3643_deferred_flash1_brightness_set}, 
// // 	[ID_TORCH0] = { 
// // 		       .cdev.name = "torch0", 
// // 		       .cdev.brightness = 0, 
// // 		       .cdev.max_brightness = 0x7f, 
// // 		       .cdev.brightness_set = lm3643_torch0_brightness_set, 
// // 		       .cdev.default_trigger = "torch0", 
// // 		       .func = lm3643_deferred_torch0_brightness_set}, 
// // 	[ID_TORCH1] = { 
// // 		       .cdev.name = "torch1", 
// // 		       .cdev.brightness = 0, 
// // 		       .cdev.max_brightness = 0x7f, 
// // 		       .cdev.brightness_set = lm3643_torch1_brightness_set, 
// // 		       .cdev.default_trigger = "torch1", 
// // 		       .func = lm3643_deferred_torch1_brightness_set}, 
// // }; 
 
// // static void lm3643_led_unregister(struct lm3643 *pchip, enum lm3643_devid id) 
// // { 
// // 	int icnt; 
 
// // 	for (icnt = id; icnt > 0; icnt--) 
// // 		led_classdev_unregister(&pchip->cdev[icnt - 1]); 
// // } 
 
// // static int lm3643_led_register(struct lm3643 *pchip) 
// // { 
// // 	int icnt, rval; 
 
// // 	for (icnt = 0; icnt < ID_MAX; icnt++) { 
// // 		INIT_WORK(&pchip->work[icnt], lm3643_leds[icnt].func); 
// // 		pchip->cdev[icnt].name = lm3643_leds[icnt].cdev.name; 
// // 		pchip->cdev[icnt].max_brightness = 
// // 		    lm3643_leds[icnt].cdev.max_brightness; 
// // 		pchip->cdev[icnt].brightness = 
// // 		    lm3643_leds[icnt].cdev.brightness; 
// // 		pchip->cdev[icnt].brightness_set = 
// // 		    lm3643_leds[icnt].cdev.brightness_set; 
// // 		pchip->cdev[icnt].default_trigger = 
// // 		    lm3643_leds[icnt].cdev.default_trigger; 
// // 		rval = led_classdev_register((struct device *) 
// // 					     pchip->dev, &pchip->cdev[icnt]); 
// // 		if (rval < 0) { 
// // 			lm3643_led_unregister(pchip, icnt); 
// // 			return rval; 
// // 		} 
// // 	} 
// // 	return 0; 
// // } 
 
// // /* device files to control registers */ 
// // struct lm3643_commands { 
// // 	char *str; 
// // 	int size; 
// // }; 
 
// // enum lm3643_cmd_id { 
// // 	CMD_ENABLE = 0, 
// // 	CMD_DISABLE, 
// // 	CMD_ON, 
// // 	CMD_OFF, 
// // 	CMD_IRMODE, 
// // 	CMD_OVERRIDE, 
// // 	CMD_MAX 
// // }; 
 
// // struct lm3643_commands cmds[CMD_MAX] = { 
// // 	[CMD_ENABLE] = {"enable", 6}, 
// // 	[CMD_DISABLE] = {"disable", 7}, 
// // 	[CMD_ON] = {"on", 2}, 
// // 	[CMD_OFF] = {"off", 3}, 
// // 	[CMD_IRMODE] = {"irmode", 6}, 
// // 	[CMD_OVERRIDE] = {"override", 8}, 
// // }; 
 
// // struct lm3643_files { 
// // 	enum lm3643_devid id; 
// // 	struct device_attribute attr; 
// // }; 
 
// // static size_t lm3643_ctrl(struct device *dev, 
// // 			  const char *buf, enum lm3643_devid id, 
// // 			  enum lm3643_devfile dfid, size_t size) 
// // { 
// // 	struct led_classdev *led_cdev = dev_get_drvdata(dev); 
// // 	struct lm3643 *pchip = to_lm3643(led_cdev, id); 
// // 	enum lm3643_cmd_id icnt; 
// // 	int tout, rval; 
 
// // 	mutex_lock(&pchip->lock); 
// // 	for (icnt = 0; icnt < CMD_MAX; icnt++) { 
// // 		if (strncmp(buf, cmds[icnt].str, cmds[icnt].size) == 0) 
// // 			break; 
// // 	} 
 
// // 	switch (dfid) { 
// // 		/* led 0 enable */ 
// // 	case DFILE_FLASH0_ENABLE: 
// // 	case DFILE_TORCH0_ENABLE: 
// // 		if (icnt == CMD_ENABLE) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x1, 
// // 					       0x1); 
// // 		else if (icnt == CMD_DISABLE) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x1, 
// // 					       0x0); 
// // 		break; 
// // 		/* led 1 enable, flash override */ 
// // 	case DFILE_FLASH1_ENABLE: 
// // 		if (icnt == CMD_ENABLE) { 
// // 			rval = regmap_update_bits(pchip->regmap, 
// // 						  REG_FLASH_LED0_BR, 0x80, 0x0); 
// // 			rval |= 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
// // 					       0x2); 
// // 		} else if (icnt == CMD_DISABLE) { 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
// // 					       0x0); 
// // 		} else if (icnt == CMD_OVERRIDE) { 
// // 			rval = regmap_update_bits(pchip->regmap, 
// // 						  REG_FLASH_LED0_BR, 0x80, 
// // 						  0x80); 
// // 			rval |= 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
// // 					       0x2); 
// // 		} 
// // 		break; 
// // 		/* led 1 enable, torch override */ 
// // 	case DFILE_TORCH1_ENABLE: 
// // 		if (icnt == CMD_ENABLE) { 
// // 			rval = regmap_update_bits(pchip->regmap, 
// // 						  REG_TORCH_LED0_BR, 0x80, 0x0); 
// // 			rval |= 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
// // 					       0x2); 
// // 		} else if (icnt == CMD_DISABLE) { 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
// // 					       0x0); 
// // 		} else if (icnt == CMD_OVERRIDE) { 
// // 			rval = regmap_update_bits(pchip->regmap, 
// // 						  REG_TORCH_LED0_BR, 0x80, 
// // 						  0x80); 
// // 			rval |= 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2, 
// // 					       0x2); 
// // 		} 
// // 		break; 
// // 		/* mode control flash/ir */ 
// // 	case DFILE_FLASH0_ONOFF: 
// // 	case DFILE_FLASH1_ONOFF: 
// // 		if (icnt == CMD_ON) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
// // 					       0xc); 
// // 		else if (icnt == CMD_OFF) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
// // 					       0x0); 
// // 		else if (icnt == CMD_IRMODE) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
// // 					       0x4); 
// // 		break; 
// // 		/* mode control torch */ 
// // 	case DFILE_TORCH0_ONOFF: 
// // 	case DFILE_TORCH1_ONOFF: 
// // 		if (icnt == CMD_ON) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
// // 					       0x8); 
// // 		else if (icnt == CMD_OFF) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc, 
// // 					       0x0); 
// // 		break; 
// // 		/* strobe pin control */ 
// // 	case DFILE_FLASH0_SOURCE: 
// // 		if (icnt == CMD_ON) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 
// // 					       0x20); 
// // 		else if (icnt == CMD_OFF) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 
// // 					       0x0); 
// // 		break; 
// // 	case DFILE_TORCH0_SOURCE: 
// // 		if (icnt == CMD_ON) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10, 
// // 					       0x10); 
// // 		else if (icnt == CMD_OFF) 
// // 			rval = 
// // 			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10, 
// // 					       0x0); 
// // 		break; 
// // 		/* flash time out */ 
// // 	case DFILE_FLASH0_TIMEOUT: 
// // 		rval = kstrtouint((const char *)buf, 10, &tout); 
// // 		if (rval < 0) 
// // 			break; 
// // 		rval = regmap_update_bits(pchip->regmap, 
// // 					  REG_FLASH_TOUT, 0x0f, tout); 
// // 		break; 
// // 	default: 
// // 		dev_err(pchip->dev, "error : undefined dev file\n"); 
// // 		break; 
// // 	} 
// // 	lm3643_read_flag(pchip); 
// // 	mutex_unlock(&pchip->lock); 
// // 	return size; 
// // } 
 
// // /* flash enable control */ 
// // static ssize_t lm3643_flash0_enable_store(struct device *dev, 
// // 					  struct device_attribute *devAttr, 
// // 					  const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ENABLE, size); 
// // } 
 
// // static ssize_t lm3643_flash1_enable_store(struct device *dev, 
// // 					  struct device_attribute *devAttr, 
// // 					  const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ENABLE, size); 
// // } 
 
// // /* flash onoff control */ 
// // static ssize_t lm3643_flash0_onoff_store(struct device *dev, 
// // 					 struct device_attribute *devAttr, 
// // 					 const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ONOFF, size); 
// // } 
 
// // static ssize_t lm3643_flash1_onoff_store(struct device *dev, 
// // 					 struct device_attribute *devAttr, 
// // 					 const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ONOFF, size); 
// // } 
 
// // /* flash timeout control */ 
// // static ssize_t lm3643_flash0_timeout_store(struct device *dev, 
// // 					   struct device_attribute *devAttr, 
// // 					   const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_TIMEOUT, size); 
// // } 
 
// // /* flash source control */ 
// // static ssize_t lm3643_flash0_source_store(struct device *dev, 
// // 					  struct device_attribute *devAttr, 
// // 					  const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_SOURCE, size); 
// // } 
 
// // /* torch enable control */ 
// // static ssize_t lm3643_torch0_enable_store(struct device *dev, 
// // 					  struct device_attribute *devAttr, 
// // 					  const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_FLASH0, DFILE_TORCH0_ENABLE, size); 
// // } 
 
// // static ssize_t lm3643_torch1_enable_store(struct device *dev, 
// // 					  struct device_attribute *devAttr, 
// // 					  const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ENABLE, size); 
// // } 
 
// // /* torch onoff control */ 
// // static ssize_t lm3643_torch0_onoff_store(struct device *dev, 
// // 					 struct device_attribute *devAttr, 
// // 					 const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_ONOFF, size); 
// // } 
 
// // static ssize_t lm3643_torch1_onoff_store(struct device *dev, 
// // 					 struct device_attribute *devAttr, 
// // 					 const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ONOFF, size); 
// // } 
 
// // /* torch source control */ 
// // static ssize_t lm3643_torch0_source_store(struct device *dev, 
// // 					  struct device_attribute *devAttr, 
// // 					  const char *buf, size_t size) 
// // { 
// // 	return lm3643_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_SOURCE, size); 
// // } 



// // #define lm3643_attr(_name, _show, _store)\ 
// // {\ 
// // 	.attr = {\ 
// // 		.name = _name,\ 
// // 		.mode = 0200,\ 
// // 	},\ 
// // 	.show = _show,\ 
// // 	.store = _store,\ 
// // } 
 
// // static struct lm3643_files lm3643_devfiles[DFILE_MAX] = { 
// // 	[DFILE_FLASH0_ENABLE] = { 
// // 				 .id = ID_FLASH0, 
// // 				 .attr = 
// // 				 lm3643_attr("enable", NULL, 
// // 					     lm3643_flash0_enable_store), 
// // 				 }, 
// // 	[DFILE_FLASH0_ONOFF] = { 
// // 				.id = ID_FLASH0, 
// // 				.attr = 
// // 				lm3643_attr("onoff", NULL, 
// // 					    lm3643_flash0_onoff_store), 
// // 				}, 
// // 	[DFILE_FLASH0_SOURCE] = { 
// // 				 .id = ID_FLASH0, 
// // 				 .attr = 
// // 				 lm3643_attr("source", NULL, 
// // 					     lm3643_flash0_source_store), 
// // 				 }, 
// // 	[DFILE_FLASH0_TIMEOUT] = { 
// // 				  .id = ID_FLASH0, 
// // 				  .attr = 
// // 				  lm3643_attr("timeout", NULL, 
// // 					      lm3643_flash0_timeout_store), 
// // 				  }, 
// // 	[DFILE_FLASH1_ENABLE] = { 
// // 				 .id = ID_FLASH1, 
// // 				 .attr = 
// // 				 lm3643_attr("enable", NULL, 
// // 					     lm3643_flash1_enable_store), 
// // 				 }, 
// // 	[DFILE_FLASH1_ONOFF] = { 
// // 				.id = ID_FLASH1, 
// // 				.attr = 
// // 				lm3643_attr("onoff", NULL, 
// // 					    lm3643_flash1_onoff_store), 
// // 				}, 
// // 	[DFILE_TORCH0_ENABLE] = { 
// // 				 .id = ID_TORCH0, 
// // 				 .attr = 
// // 				 lm3643_attr("enable", NULL, 
// // 					     lm3643_torch0_enable_store), 
// // 				 }, 
// // 	[DFILE_TORCH0_ONOFF] = { 
// // 				.id = ID_TORCH0, 
// // 				.attr = 
// // 				lm3643_attr("onoff", NULL, 
// // 					    lm3643_torch0_onoff_store), 
// // 				}, 
// // 	[DFILE_TORCH0_SOURCE] = { 
// // 				 .id = ID_TORCH0, 
// // 				 .attr = 
// // 				 lm3643_attr("source", NULL, 
// // 					     lm3643_torch0_source_store), 
// // 				 }, 
// // 	[DFILE_TORCH1_ENABLE] = { 
// // 				 .id = ID_TORCH1, 
// // 				 .attr = 
// // 				 lm3643_attr("enable", NULL, 
// // 					     lm3643_torch1_enable_store), 
// // 				 }, 
// // 	[DFILE_TORCH1_ONOFF] = { 
// // 				.id = ID_TORCH1, 
// // 				.attr = 
// // 				lm3643_attr("onoff", NULL, 
// // 					    lm3643_torch1_onoff_store), 
// // 				} 
// // }; 
 
// // static void lm3643_df_remove(struct lm3643 *pchip, enum lm3643_devfile dfid) 
// // { 
// // 	enum lm3643_devfile icnt; 
 
// // 	for (icnt = dfid; icnt > 0; icnt--) 
// // 		device_remove_file(pchip->cdev[lm3643_devfiles[icnt - 1].id]. 
// // 				   dev, &lm3643_devfiles[icnt - 1].attr); 
// // } 
 
// // static int lm3643_df_create(struct lm3643 *pchip) 
// // { 
// // 	enum lm3643_devfile icnt; 
// // 	int rval; 
 
// // 	for (icnt = 0; icnt < DFILE_MAX; icnt++) { 
// // 		rval = 
// // 		    device_create_file(pchip->cdev[lm3643_devfiles[icnt].id]. 
// // 				       dev, &lm3643_devfiles[icnt].attr); 
// // 		if (rval < 0) { 
// // 			lm3643_df_remove(pchip, icnt); 
// // 			return rval; 
// // 		} 
// // 	} 
// // 	return 0; 
// // } 
 
// // static const struct regmap_config lm3643_regmap = { 
// // 	.reg_bits = 8, 
// // 	.val_bits = 8, 
// // 	.max_register = 0xff, 
// // }; 


// int iReadRegI2C_lm(struct i2c_client *client, u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId)
// {
//     int  i4RetValue = 0;

// 	//spin_lock(&kdsensor_drv_lock);
// 	client->addr = i2cId;
// 	client->ext_flag = (client->ext_flag)&(~I2C_DMA_FLAG);

// 	/* Remove i2c ack error log during search sensor */
// 	/* PK_ERR("client->ext_flag: %d", g_IsSearchSensor); */
// 	//if (g_IsSearchSensor == 1)
// 	 //   client->ext_flag = (client->ext_flag) | I2C_A_FILTER_MSG;
// 	//else
// 	    //client->ext_flag = (client->ext_flag)&(~I2C_A_FILTER_MSG);

// 	//spin_unlock(&kdsensor_drv_lock);
// 	/*  */
// 	i4RetValue = i2c_master_send(client, a_pSendData, a_sizeSendData);
// 	if (i4RetValue != a_sizeSendData) {
// 	    printk("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
// 	    return -1;
// 	}

// 	i4RetValue = i2c_master_recv(client, (char *)a_pRecvData, a_sizeRecvData);
// 	if (i4RetValue != a_sizeRecvData) {
// 	    printk("[CAMERA SENSOR] I2C read failed!!\n");
// 	    return -1;
// 	}
    
//     return 0;
// } 

// static kal_uint16 read_data(struct i2c_client *client, kal_uint8 addr, kal_uint8 data_rec, kal_uint8 rec_size)
// {
//     //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
//     kal_uint8 get_byte=0;
//     char pusendcmd[2] = {addr & 0xFF, 0};
//     iReadRegI2C_lm(client, pusendcmd , 1, data_rec, rec_size, 0x63);
//     return rec_size;
// }

// static kal_uint16 write_data(struct i2c_client *client, kal_uint8 addr, kal_uint8 data)
// {
//     //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
//     kal_uint8 get_byte=0;
//     int  i4RetValue = 0;
//     char pusendcmd[2] = {addr, data};
//     client->addr = 0x63;
// 	client->ext_flag = (client->ext_flag)&(~I2C_DMA_FLAG);
// 	i4RetValue = i2c_master_send(client, pusendcmd, 2);
	
// 	// pusendcmd[0] = data;
// 	// i4RetValue = i2c_master_send(client, pusendcmd, 1);
// 	// if (i4RetValue != 1) {
// 	//     printk("[CAMERA SENSOR] I2C send failed!!, Addr = 0x%x\n", pusendcmd[0]);
// 	//     return -1;
// 	// }
//     //iReadRegI2C_lm(client, pusendcmd , 1, &get_byte, 1, 0x63);
//     return ((get_byte)&0x00ff);
// }

// static int lm3643_probe(struct i2c_client *client, 
// 			const struct i2c_device_id *id) 
// { 
// 	//struct lm3643 *pchip; 
// 	int rval; 
 
// 	/* i2c check */ 
// 	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) { 
// 		dev_err(&client->dev, "i2c functionality check fail.\n"); 
// 		return -EOPNOTSUPP; 
// 	} 
 
// 	//pchip = devm_kzalloc(&client->dev, sizeof(struct lm3643), GFP_KERNEL); 
// 	//if (!pchip) 
// 	//	return -ENOMEM; 
//  	mt_set_gpio_mode(GPIO_CAMERA_FLASH_EXT1_PIN, GPIO_MODE_00);
// 	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EXT1_PIN, GPIO_DIR_OUT);
// 	mt_set_gpio_out(GPIO_CAMERA_FLASH_EXT1_PIN, 1);
// 	mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN, GPIO_MODE_00);
// 	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN, GPIO_DIR_OUT);
// 	//mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN, 1);
// 	mt_set_gpio_mode(GPIO_CAMERA_FLASH_MODE_PIN, GPIO_MODE_00);
// 	mt_set_gpio_dir(GPIO_CAMERA_FLASH_MODE_PIN, GPIO_DIR_OUT);
// 	//mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN, 1);
// 	//pchip->dev = &client->dev; 
// 	g_lm3643_i2c_client = client;
// 	//printk("<%s:%d>data[%x][%x][%x]\n", __func__, __LINE__, read_data(client, 0x1), read_data(client, 0xA), read_data(client, 0xc));
// 	//write_data(client, 0x01, 0x8a);
// 	//mdelay(200);
// 	//printk("<%s:%d>data[%x][%x][%x]\n", __func__, __LINE__, read_data(client, 0x1), read_data(client, 0xA), read_data(client, 0xc));
// 	printk("<%s:%d>\n", __func__, __LINE__);
// 	write_data(client, 0x01, 0x8a);
// 	mdelay(200);
// 	write_data(client, 0x01, 0x80);
// 	//printk("<%s:%d>data[%x][%x][%x]\n", __func__, __LINE__, read_data(client, 0x1), read_data(client, 0xA), read_data(client, 0xc));
// 	//pchip->regmap = devm_regmap_init_i2c(client, &lm3643_regmap); 
// 	// if (IS_ERR(pchip->regmap)) { 
// 	// 	rval = PTR_ERR(pchip->regmap); 
// 	// 	dev_err(&client->dev, "Failed to allocate register map: %d\n", 
// 	// 		rval); 
// 	// 	return rval; 
// 	// } 
// 	//mutex_init(&pchip->lock); 
// 	//i2c_set_clientdata(client, pchip); 
 
// 	/* led class register */ 
// 	// rval = lm3643_led_register(pchip); 
// 	// if (rval < 0) 
// 	// 	return rval; 
 
// 	// /* create dev files */ 
// 	// rval = lm3643_df_create(pchip); 
// 	// if (rval < 0) { 
// 	// 	lm3643_led_unregister(pchip, ID_MAX); 
// 	// 	return rval; 
// 	// } 
 
// 	//dev_info(pchip->dev, "lm3643 leds initialized\n"); 
// 	return 0; 
// } 
 
// static int lm3643_remove(struct i2c_client *client) 
// { 
// 	//struct lm3643 *pchip = i2c_get_clientdata(client); 
 
// 	//lm3643_df_remove(pchip, DFILE_MAX); 
// 	//lm3643_led_unregister(pchip, ID_MAX); 
 
// 	return 0; 
// } 
 
// static const struct i2c_device_id lm3643_id[] = { 
// 	{LM3643_NAME, 0}, 
// 	{} 
// }; 
 
// //MODULE_DEVICE_TABLE(i2c, lm3643_id); 
 
// static struct i2c_driver lm3643_i2c_driver = { 
// 	.driver = { 
// 		   .name = LM3643_NAME, 
// 		   .owner = THIS_MODULE, 
// 		   //.pm = NULL, 
// 		   }, 
// 	.probe = lm3643_probe, 
// 	.remove = lm3643_remove, 
// 	.id_table = lm3643_id, 
// }; 
 
// //module_i2c_driver(lm3643_i2c_driver); 
// static struct platform_device lm3643_i2c_device = {
//     .name = LM3643_NAME,
//     .id = 0,
//     .dev = {}
// };

// static int __init lm3643_i2C_init(void)
// {
//     i2c_register_board_info(3, &lm3643_dev, 1);


//     if(i2c_add_driver(&lm3643_i2c_driver)){
//         printk("Failed to register lm3643_i2c_driver\n");
//         return -ENODEV;
//     }

//     return 0;
// }

// static void __exit lm3643_i2C_exit(void)
// {
//     platform_driver_unregister(&lm3643_i2c_driver);
// }

// module_init(lm3643_i2C_init);
// module_exit(lm3643_i2C_exit);

// MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM3643"); 
// MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>"); 
// MODULE_LICENSE("GPL v2"); 



/*****************************************************************************
Functions
*****************************************************************************/
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
static void work_timeOutFunc(struct work_struct *data);
extern kal_uint16 read_data_lm3643(struct i2c_client *client, kal_uint8 addr);
extern kal_uint16 write_data_lm3643(struct i2c_client *client, kal_uint8 addr, kal_uint8 data);
int readReg(int reg)
{
    char buf[2];
    char bufR[2];
    buf[0]=reg;
    //iReadRegI2C(buf , 1, bufR,1, STROBE_DEVICE_ID);
    bufR[0] = read_data_lm3643(g_lm3643_i2c_client, (char)reg);
    printk("qq reg=%x val=%x qq\n", buf[0],bufR[0]);
    return (int)bufR[0];
}

int writeReg(int reg, int data)
{
    char buf[2];
    buf[0]=reg;
    buf[1]=data;
    printk("<%s:%d>reg[%d][%d]\n", __FUNCTION__, __LINE__, reg, data);
	write_data_lm3643(g_lm3643_i2c_client, buf[0], buf[1]);
    //iWriteRegI2C(buf, 2, STROBE_DEVICE_ID);

   return 0;
}

#define e_DutyNum 26
#define TORCHDUTYNUM 4
static int isMovieMode[e_DutyNum] = {1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int torchLEDReg[e_DutyNum] = {35,71,106,127,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 35 17-->25ma

static int torchLEDReg_ktd1[e_DutyNum] = {16,33,50,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 35 17-->25ma
static int torchLEDReg_ktd2[e_DutyNum] = {8,16,25,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 35 17-->25ma
//50,100,150,179ma
static int flashLEDReg[e_DutyNum] = {3,8,12,14,16,20,25,29,33,37,42,46,50,55,59,63,67,72,76,80,84,93,101,110,118,127};
//200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000,1100,1200,1300,1400,1500ma
static int flashLEDReg_ktd[e_DutyNum] = {2,5,9,10,12,15,18,21,25,28,31,34,37,41,44,47,50,54,57,60,63,63,63,63,63,63};
static int *flashLEDReg_led2=NULL;
static int *torchLEDReg_led2=NULL;
extern int g_is_ktd2684;


int m_duty1=0;
int m_duty2=0;
int LED1Closeflag = 0;
int LED2Closeflag = 0;


int flashEnable_LM3643_1(void)
{
	int temp;
	return 0;
}
int flashDisable_LM3643_1(void)
{
	int temp;
    return 0;
}

int setDuty_LM3643_1(int duty)
{

	if(duty<0)
		duty=0;
	else if(duty>=e_DutyNum)
		duty=e_DutyNum-1;
	m_duty1=duty;
	
	return 0;
}



int flashEnable_LM3643_2(void)
{
	int temp;

	PK_DBG("flashEnable_LM3643_2\n");
	PK_DBG("LED1Closeflag = %d, LED2Closeflag = %d\n", LED1Closeflag, LED2Closeflag);

	temp = readReg(LM3643_REG_ENABLE);

	if((LED1Closeflag == 1) && (LED2Closeflag == 1))
	{
		writeReg(LM3643_REG_ENABLE, temp & 0xF0);//close		
	}
	else if(LED1Closeflag == 1)
	{
		if(isMovieMode[m_duty2] == 1)
			writeReg(LM3643_REG_ENABLE, (temp&0xF0) | 0xFA);//torch mode
		else
			writeReg(LM3643_REG_ENABLE, (temp&0xF0) | 0xFE);//flash mode
	}
	else if(LED2Closeflag == 1)
	{
		if(isMovieMode[m_duty1] == 1)
			writeReg(LM3643_REG_ENABLE, (temp&0xF0) | 0xF9);//torch mode
		else
			writeReg(LM3643_REG_ENABLE, (temp&0xF0) | 0xFD);//flash mode		
	}
	else
	{
		if((isMovieMode[m_duty1] == 1) & (isMovieMode[m_duty2] == 1))
			writeReg(LM3643_REG_ENABLE, (temp&0xF0) | 0xFB);//torch mode
		else
			writeReg(LM3643_REG_ENABLE, (temp&0xF0) | 0xFF);//flash mode
	}
	return 0;

}
int flashDisable_LM3643_2(void)
{
	flashEnable_LM3643_2();
	return 0;
}


int setDuty_LM3643_2(int duty)
{
	if(duty<0)
		duty=0;
	else if(duty>=e_DutyNum)
		duty=e_DutyNum-1;
	m_duty2=duty;

	PK_DBG("setDuty_LM3643_2:m_duty = %d, m_duty2 = %d!\n", m_duty1, m_duty2);
	PK_DBG("LED1Closeflag = %d, LED2Closeflag = %d\n", LED1Closeflag, LED2Closeflag);

	if((LED1Closeflag == 1) && (LED2Closeflag == 1))
	{
		
	}
	else if(LED1Closeflag == 1)
	{
		if(isMovieMode[m_duty2] == 1)
		{
			if (g_is_ktd2684 == 1)
			{
				writeReg(LM3643_REG_LED2_TORCH, torchLEDReg_ktd2[m_duty2]);
			}
			else
			{
				writeReg(LM3643_REG_LED2_TORCH, torchLEDReg[m_duty2]);
			}
			
		}
		else
		{
			if (flashLEDReg_led2 == NULL)
			{
				if (g_is_ktd2684 == 1)
					flashLEDReg_led2 = flashLEDReg_ktd;
				else
					flashLEDReg_led2 = flashLEDReg;
			}
	 	    writeReg(LM3643_REG_LED2_FLASH, flashLEDReg_led2[m_duty2]);
		}
	}
	else if(LED2Closeflag == 1)
	{
		if(isMovieMode[m_duty1] == 1)
		{
			if (g_is_ktd2684 == 1)
			{
				writeReg(LM3643_REG_LED1_TORCH, torchLEDReg_ktd1[m_duty1]);
			}
			else
			{
				writeReg(LM3643_REG_LED1_TORCH, torchLEDReg[m_duty1]);
			}
			//writeReg(LM3643_REG_LED1_TORCH, torchLEDReg[m_duty1]);
		}
		else
		{
			writeReg(LM3643_REG_LED1_FLASH, flashLEDReg[m_duty1]);	
		}		
	}
	else
	{
		if((isMovieMode[m_duty1] == 1) && ((isMovieMode[m_duty2] == 1)))
		{
			if (g_is_ktd2684 == 1)
			{
				writeReg(LM3643_REG_LED1_TORCH, torchLEDReg_ktd1[m_duty1]);
				writeReg(LM3643_REG_LED2_TORCH, torchLEDReg_ktd2[m_duty2]);
			}
			else
			{
				writeReg(LM3643_REG_LED1_TORCH, torchLEDReg[m_duty1]);
				writeReg(LM3643_REG_LED2_TORCH, torchLEDReg[m_duty2]);
			}
			//writeReg(LM3643_REG_LED1_TORCH, torchLEDReg[m_duty1]);
			//writeReg(LM3643_REG_LED2_TORCH, torchLEDReg[m_duty2]);
		}
		else
		{
			if (flashLEDReg_led2 == NULL)
			{
				if (g_is_ktd2684 == 1)
					flashLEDReg_led2 = flashLEDReg_ktd;
				else
					flashLEDReg_led2 = flashLEDReg;
			}
	 	    writeReg(LM3643_REG_LED1_FLASH, flashLEDReg[m_duty1]);
			writeReg(LM3643_REG_LED2_FLASH, flashLEDReg_led2[m_duty2]);
		}
	}

	return 0;
}


int FL_Enable(void)
{

	PK_DBG(" FL_Enable line=%d\n",__LINE__);


    return 0;
}



int FL_Disable(void)
{
	PK_DBG(" FL_Disable line=%d\n",__LINE__);
    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
    setDuty_LM3643_1(duty);

    PK_DBG(" FL_dim_duty line=%d\n",__LINE__);
    return 0;
}




int FL_Init(void)
{
	PK_DBG("LED1_FL_Init!\n");
    if(mt_set_gpio_mode(GPIO_LED_EN,GPIO_MODE_00)){PK_DBG(" set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_LED_EN,GPIO_DIR_OUT)){PK_DBG(" set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_LED_EN,GPIO_OUT_ONE)){PK_DBG(" set gpio failed!! \n");}
    if (g_is_ktd2684 == 1)
    	writeReg(LM3643_REG_TIMING, 0x1A);
    else
		writeReg(LM3643_REG_TIMING, 0x1F);

    INIT_WORK(&workTimeOut, work_timeOutFunc);
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


int FL_Uninit(void)
{
	PK_DBG("LED1_FL_Uninit!\n");
    return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/

static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
    PK_DBG("LED1TimeOut_callback\n");
}



enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}
static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
  	INIT_WORK(&workTimeOut, work_timeOutFunc);
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;

}



static int constant_flashlight_ioctl(unsigned int cmd, unsigned long arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("LM3643_LED1_constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift, arg);
    switch(cmd)
    {

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
			g_timeOutTimeMs=arg;
		break;


    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
			m_duty1 = arg;
    		break;


    	case FLASH_IOC_SET_STEP:
    		PK_DBG("FLASH_IOC_SET_STEP: %d\n",arg);

    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
    		if(arg==1)
    		{
				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( 0, g_timeOutTimeMs*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
				LED1Closeflag = 0;
    			FL_Enable();
    		}
    		else
    		{
    			LED1Closeflag = 1;
    			FL_Disable();
				hrtimer_cancel( &g_timeOutTimer );
    		}
    		break;
    	case FLASH_IOC_SET_REG_ADR:
    	    break;
    	case FLASH_IOC_SET_REG_VAL:
    	    break;
    	case FLASH_IOC_SET_REG:
    	    break;
    	case FLASH_IOC_GET_REG:
    	    break;



		default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
	    FL_Init();
		timerInit();
		flashlight_clear_brightness();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }


    spin_unlock_irq(&g_strobeSMPLock);
    PK_DBG("<%s:%d>g_is_ktd2684[%d]\n", __func__, __LINE__, g_is_ktd2684);
    if (g_is_ktd2684 == 1)
    	flashLEDReg_led2 = flashLEDReg_ktd;
    else
    	flashLEDReg_led2 = flashLEDReg;
    PK_DBG("constant_flashlight_open line=%d g_is_ktd2684[%d]\n", __LINE__, g_is_ktd2684);

    return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);

        strobe_Res = 0;
        strobe_Timeus = 0;

        /* LED On Status */
        g_strobe_On = FALSE;

        spin_unlock_irq(&g_strobeSMPLock);

    	FL_Uninit();
    }

    PK_DBG(" Done\n");

    return 0;

}


FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &constantFlashlightFunc;
    }
    return 0;
}



/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);

void flashlight_onoff(int level)
{
	 if (strobe_Res)
    {
        flashlight_clear_brightness();
        return;
    }
#if 1
    printk("<%s:%d>level[%x]\n", __func__, __LINE__, level);
    if (level == 100)
    	level = 0x31;
    if (level == 0x33)
        level = 0x31;

    FL_Init();
    if (level & 0xf0)
    	LED1Closeflag = 0;
    else
    	LED1Closeflag = 1;
    
    if (level & 0xf)
    	LED2Closeflag = 0;
    else
    	LED2Closeflag = 1;
    setDuty_LM3643_1((level & 0xf0) >> 4);
    setDuty_LM3643_2((level & 0xf));
    mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN, 1);
	mt_set_gpio_mode(GPIO_CAMERA_FLASH_MODE_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_MODE_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAMERA_FLASH_MODE_PIN, 1);
    flashEnable_LM3643_2();
    return;
	#endif
}
EXPORT_SYMBOL(flashlight_onoff);
