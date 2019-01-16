#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/leds-mt65xx.h>

#include "hdmi_drv.h"
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include "mach/eint.h"
#include "mach/irqs.h"

#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "siHdmiTx_902x_TPI.h"

#define SINK_480P      (1<< 0)
#define SINK_720P60    (1<< 1)
#define SINK_1080P30   (1<< 9)



#define MAX_TRANSACTION_LENGTH 8

#define MHL_DRIVER_DESC "SiliconImage 902xA Tx Driver"
#define DEVICE_NAME "sii902xA"
#define sii902xA_DEVICE_ID   0xB0


extern struct i2c_client *sii902xA;
extern struct i2c_client *siiEDID;
extern struct i2c_client *siiSegEDID;
extern struct i2c_client *siiHDCP;
/* ----------------------------------- */
/* hdmi timer */
/* static struct timer_list r_hdmi_timer; */
/* static uint32_t gHDMI_CHK_INTERVAL = 300; */
/* static uint8_t ucHdmi_Plugin = 0; */
static uint8_t ucHdmi_isr_en;
/* static uint8_t ucHdmi_chip_exist = 0; */
static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

/* ------------------------------------ */

#ifdef CI2CA
#define SII902xA_plus 0x02	/* Define sii902xA's I2c Address of all pages by the status of CI2CA pin. */
#else
#define SII902xA_plus 0x00	/* Define sii902xA's I2c Address of all pages by the status of CI2CA pin. */
#endif

#if defined(GPIO_HDMI_PWR_1_2V_EN)
unsigned int hdmi_irq;
#endif
//------------------------------------

HDMI_UTIL_FUNCS hdmi_util = { 0 };

byte sii9024_i2c_read_byte(byte addr);
byte sii9024_i2c_write_byte(struct i2c_client *client, byte addr, byte data);
byte sii9024_i2c_read_block(struct i2c_client *client, byte addr, byte *data, word len);
int sii9024_i2c_write_block(struct i2c_client *client, byte addr, byte *data, word len);
/* ----------------------------------------- */
byte sii9024_i2c_read_byte(byte addr)
{
	byte buf;
	byte ret = 0;
	struct i2c_client *client = sii902xA;
	if (client == NULL)
		return ret;

	buf = addr;
	ret = i2c_master_send(client, (byte *) &buf, 1);

	ret = i2c_master_recv(client, (byte *) &buf, 1);



	return buf;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL(sii9024_i2c_read_byte);
/*----------------------------------------------------------------------------*/

byte sii9024_i2c_write_byte(struct i2c_client *client, byte addr, byte data)
{
	/* struct i2c_client *client = sii902xA; */
	byte buf[] = { addr, data };
	byte ret = 0;
	if (client == NULL)
		return ret;

	ret = i2c_master_send(client, (const char *)buf, sizeof(buf));
	if (ret < 0) {

		return -EFAULT;
	}
#if defined(HDMI_DEBUG)
	else {

	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL(sii9024_i2c_write_byte);
/*----------------------------------------------------------------------------*/

byte sii9024_i2c_read_block(struct i2c_client *client, byte addr, byte *data, word len)
{

	if (len == 1) {
		return sii9024_i2c_read_byte(addr);
	} else {
		byte beg = addr;
		struct i2c_msg msgs[2] = {
			{
			 .addr = client->addr, .flags = 0,
			 .len = 1, .buf = &beg},
			{
			 .addr = client->addr, .flags = I2C_M_RD,
			 .len = len, .buf = data,
			 }
		};
		byte err;

		if (!client) {
			return -EINVAL;
		} else if (len > MAX_TRANSACTION_LENGTH) {

			return -EINVAL;
		}

		err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
		if (err != 2) {

			err = -EIO;
		} else {
			err = 0;	/*no error */
		}
		return err;
	}
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(sii9024_i2c_read_block);
/*----------------------------------------------------------------------------*/

int sii9024_i2c_write_block(struct i2c_client *client, byte addr, byte *data, word len)
{
	/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err, idx, num;
	char buf[MAX_TRANSACTION_LENGTH];


	if (!client) {
		return -EINVAL;
	} else if (len >= MAX_TRANSACTION_LENGTH) {

		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}

	err = i2c_master_send(client, buf, num);
	if (err < 0) {

		return -EFAULT;
	} else {
		err = 0;	/*no error */
	}
	return err;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(sii9024_i2c_write_block);
/* --------------------------------------------------- */
/*------------------------------------------------------*/
static int sii9024_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
				HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	byte sii9024_format;

	if (vformat == HDMI_VIDEO_720x480p_60Hz)
		sii9024_format = HDMI_480P60_4X3;
	else if (vformat == HDMI_VIDEO_1280x720p_60Hz)
		sii9024_format = HDMI_720P60;
	else if (vformat == HDMI_VIDEO_1920x1080p_30Hz)
		sii9024_format = HDMI_1080P30;
	else {
		TPI_DEBUG_PRINT(("error:sii9024_video_config vformat=%d\n", vformat));
		sii9024_format = HDMI_720P60;
	}

	switch (sii9024_format) {
	case HDMI_480P60_4X3:
		siHdmiTx_VideoSel(HDMI_480P60_4X3);
		break;

	case HDMI_720P60:
		siHdmiTx_VideoSel(HDMI_720P60);
		break;

	case HDMI_1080P30:
		siHdmiTx_VideoSel(HDMI_1080P30);
		break;

	default:
		siHdmiTx_VideoSel(HDMI_720P60);
		break;
	}

	siHdmiTx_VideoSet();

	/* siHdmiTx_TPI_Init(); */
	/* siHdmiTx_PowerStateD3(); */
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

static void sii9024_get_params(HDMI_PARAMS *params)
{
	memset(params, 0, sizeof(HDMI_PARAMS));

	pr_debug("720p\n");
	params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
	params->init_config.aformat = HDMI_AUDIO_48K_2CH;

	params->clk_pol = HDMI_POLARITY_RISING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_FALLING;
	params->hsync_pol = HDMI_POLARITY_FALLING;

	params->hsync_pulse_width = 128;
	params->hsync_back_porch = 152;
	params->hsync_front_porch = 40;
	params->vsync_pulse_width = 3;
	params->vsync_back_porch = 12;
	params->vsync_front_porch = 10;

	params->rgb_order = HDMI_COLOR_ORDER_RGB;

	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->output_mode = HDMI_OUTPUT_MODE_LCD_MIRROR;
	params->is_force_awake = 1;
	params->is_force_landscape = 1;
}

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/

static void sii9024_set_util_funcs(const HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/


static int match_id(const struct i2c_device_id *id, const struct i2c_client *client)
{
	if (strcmp(client->name, id->name) == 0)
		return true;

	return false;
}

void HDMI_reset(void)
{
#if defined(GPIO_HDMI_9024_RESET)
	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ZERO);

	msleep(10);

	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ONE);
#endif
}

struct platform_data {
	void (*reset) (void);
};

static struct platform_data HDMI_data = {
	.reset = HDMI_reset,
};


static struct i2c_board_info sii9024_i2c_hdmi[] = {
	{
	 .type = "siiEDID",
	 .addr = 0x50,
	 },
	{
	 .type = "siiSegEDID",
	 .addr = 0x30,
	 },
	{
	 .type = "siiHDCP",
	 .addr = 0x3A + SII902xA_plus,
	 },
	{
	 .type = "siiCEC",
	 .addr = 0x60 + SII902xA_plus,
	 },
	{
	 .type = "sii902xA",
	 .addr = 0x39 + SII902xA_plus,
	 /* .irq = IOMUX_TO_IRQ(MX51_PIN_EIM_OE),  //define the interrupt signal input pin */
	 .platform_data = &HDMI_data,
	 }
};

static const struct i2c_device_id hmdi_sii_id[] = {
	{"sii902xA", 0},
	{"siiEDID", 0},
	{"siiSegEDID", 0},
	{"siiHDCP", 0},
};

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_timer_wq, atomic_read(&hdmi_timer_event));
		atomic_set(&hdmi_timer_event, 0);
		siHdmiTx_TPI_Poll();
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
        #if defined(GPIO_HDMI_PWR_1_2V_EN)
		enable_irq(hdmi_irq);
	#else
		mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
#endif
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int sii9024_enter(void)	/* ch7035 re-power on */
{
	return 0;
}

/*----------------------------------------------------------------------------*/

static int sii9024_exit(void)	/* ch7035 power off */
{
	return 0;
}

/*----------------------------------------------------------------------------*/

static void sii9024_suspend(void)
{
#if defined(CONFIG_SINGLE_PANEL_OUTPUT)
#if defined(GPIO_HDMI_LCD_SW_EN)
	mt_set_gpio_mode(GPIO_HDMI_LCD_SW_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_LCD_SW_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_LCD_SW_EN, GPIO_OUT_ZERO);
#endif

	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	/* mdelay(200); */

	/* mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, (LED_HALF-1)/2); */
#endif
}

/*----------------------------------------------------------------------------*/

static void sii9024_resume(void)
{
#if defined(CONFIG_SINGLE_PANEL_OUTPUT)
#if defined(GPIO_HDMI_LCD_SW_EN)
	mt_set_gpio_mode(GPIO_HDMI_LCD_SW_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_LCD_SW_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_LCD_SW_EN, GPIO_OUT_ONE);
#endif
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	/* mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF); */
#endif
}

/*----------------------------------------------------------------------------*/

static int sii9024_audio_config(HDMI_AUDIO_FORMAT aformat)
{


	return 0;
}

/*----------------------------------------------------------------------------*/

static int sii9024_video_enable(bool enable)
{

	return 0;
}

/*----------------------------------------------------------------------------*/

void Set_I2S_Pin(bool enable)
{

	if((1 == ucHdmi_isr_en) &&
		(enable == true))
	{
#ifdef GPIO_HDMI_I2S_OUT_WS_PIN
			mt_set_gpio_mode(GPIO_HDMI_I2S_OUT_WS_PIN, GPIO_MODE_01);
			mt_set_gpio_mode(GPIO_HDMI_I2S_OUT_CK_PIN, GPIO_MODE_01);
			mt_set_gpio_mode(GPIO_HDMI_I2S_OUT_DAT_PIN, GPIO_MODE_01);

			mt_set_gpio_dir(GPIO_HDMI_I2S_OUT_WS_PIN, GPIO_DIR_OUT);
#else
			HDMI_LOG("%s,%d. GPIO_HDMI_I2S_OUT_WS_PIN is not defined\n", __func__, __LINE__);
#endif
	}
else
	{
#ifdef GPIO_HDMI_I2S_OUT_WS_PIN
			mt_set_gpio_mode(GPIO_HDMI_I2S_OUT_WS_PIN, GPIO_MODE_02);
			mt_set_gpio_mode(GPIO_HDMI_I2S_OUT_CK_PIN, GPIO_MODE_01);
			mt_set_gpio_mode(GPIO_HDMI_I2S_OUT_DAT_PIN, GPIO_MODE_02);

			mt_set_gpio_dir(GPIO_HDMI_I2S_OUT_WS_PIN, GPIO_DIR_IN);
#endif
	}

    return ;

}


static int sii9024_audio_enable(bool enable)
{
    bool flag = enable;
	TPI_DEBUG_PRINT(("%s,Audio enable flag = %d\n",__func__,flag));

    Set_I2S_Pin(flag);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void sii9024_set_mode(unsigned char ucMode)
{

}

/*----------------------------------------------------------------------------*/



void sii9024_dump(void)
{

}

/*----------------------------------------------------------------------------*/

void sii9024_read(unsigned char u8Reg)
{

}

/*----------------------------------------------------------------------------*/

void sii9024_write(unsigned char u8Reg, unsigned char u8Data)
{

}

/*----------------------------------------------------------------------------*/

HDMI_STATE sii9024_get_state(void)
{

	return HDMI_STATE_NO_DEVICE;

}

/*----------------------------------------------------------------------------*/
void sii9024_log_enable(bool enable)
{

}

/*----------------------------------------------------------------------------*/


int sii9024_power_on(void)	/* sii9024 suspend */
{
	if (0 == ucHdmi_isr_en) {
#if defined(GPIO_HDMI_POWER_CONTROL)
		mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ONE);
#endif
      
      /* For 6595, use gpio132 as the 1.2v output enable pin */
#if defined(GPIO_HDMI_PWR_1_2V_EN)
		mt_set_gpio_mode(GPIO_HDMI_PWR_1_2V_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_HDMI_PWR_1_2V_EN, GPIO_DIR_OUT);
		mt_set_gpio_pull_enable(GPIO_HDMI_PWR_1_2V_EN, TRUE);
		mt_set_gpio_pull_select(GPIO_HDMI_PWR_1_2V_EN, GPIO_PULL_UP);
	  mt_set_gpio_out(GPIO_HDMI_PWR_1_2V_EN, GPIO_OUT_ONE);
#elif defined(CONFIG_ARCH_MT6582) || defined(CONFIG_ARCH_MT6592)
      /* For 92&82, PMIC output 1.2v for 9024*/
		hwPowerOn(MT6323_POWER_LDO_VGP3, VOL_1200, "HDMI");
#elif defined(CONFIG_ARCH_MT6752)
	/*For K2(MT6752, use PMIC 6325 for 1.2v output) */
      hwPowerOn(MT6325_POWER_LDO_VGP2,VOL_1200, "HDMI");
#endif
		ucHdmi_isr_en = 1;
		HDMI_reset();
		siHdmiTx_VideoSel(HDMI_720P60);
		siHdmiTx_AudioSel(AFS_44K1);
		siHdmiTx_TPI_Init();
		/* siHdmiTx_PowerStateD3(); */
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	 #if defined(GPIO_HDMI_PWR_1_2V_EN)
	 enable_irq(hdmi_irq);
	 #else
		mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
      #endif      
	}
	return 0;
}

void sii9024_power_off(void)	/* sii9024 resume */
{
	if (ucHdmi_isr_en) {
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
    #if defined(GPIO_HDMI_PWR_1_2V_EN)
	  disable_irq(hdmi_irq);
    #else
		mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
#endif

#if defined(GPIO_HDMI_PWR_1_2V_EN)
		 mt_set_gpio_mode(GPIO_HDMI_PWR_1_2V_EN, GPIO_MODE_00);
		 mt_set_gpio_dir(GPIO_HDMI_PWR_1_2V_EN, GPIO_DIR_OUT);
		 mt_set_gpio_pull_enable(GPIO_HDMI_PWR_1_2V_EN, TRUE);
		 mt_set_gpio_pull_select(GPIO_HDMI_PWR_1_2V_EN,GPIO_PULL_DOWN);
		 mt_set_gpio_out(GPIO_HDMI_PWR_1_2V_EN, GPIO_OUT_ZERO);
#elif defined(CONFIG_ARCH_MT6582) || defined(CONFIG_ARCH_MT6592)
		 /* For 92&82, PMIC output 1.2v for 9024*/
		hwPowerDown(MT6323_POWER_LDO_VGP3, "HDMI");
#elif defined(CONFIG_ARCH_MT6752)

		 hwPowerDown(MT6325_POWER_LDO_VGP2, "HDMI");
#endif

		/* hwPowerDown(MT6323_POWER_LDO_VGP3, "HDMI"); */

		ucHdmi_isr_en = 0;
#if defined(GPIO_HDMI_POWER_CONTROL)
		mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ZERO);
#endif
#if defined(CONFIG_SINGLE_PANEL_OUTPUT)
#if defined(GPIO_HDMI_LCD_SW_EN)
		mt_set_gpio_mode(GPIO_HDMI_LCD_SW_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_HDMI_LCD_SW_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_HDMI_LCD_SW_EN, GPIO_OUT_ZERO);
#endif
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
	}
}

static irqreturn_t _sil9024_irq_handler(int irq, void *data)
{
	pr_debug("9024 irq\n");
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
    #if defined(GPIO_HDMI_PWR_1_2V_EN)
	disable_irq_nosync(hdmi_irq);
    #else
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
#endif
	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);

	return IRQ_HANDLED;
}

static int hdmi_sii_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int ret = 0;

	/* static struct mxc_lcd_platform_data *plat_data; */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_I2C))
		return -ENODEV;


	if (match_id(&hmdi_sii_id[1], client)) {
		siiEDID = client;
		dev_info(&client->adapter->dev, "attached hmdi_EDID: %s "
			 "into i2c adapter successfully\n", id->name);
	} else if (match_id(&hmdi_sii_id[2], client)) {
		siiSegEDID = client;
		dev_info(&client->adapter->dev, "attached hmdi_Seg_EDID: %s "
			 "into i2c adapter successfully\n", id->name);
	} else if (match_id(&hmdi_sii_id[3], client)) {
		siiHDCP = client;
		dev_info(&client->adapter->dev, "attached hmdi_HDCP: %s "
			 "into i2c adapter successfully\n", id->name);
	} else if (match_id(&hmdi_sii_id[0], client)) {
		sii902xA = client;
		dev_info(&client->adapter->dev, "attached hmdi_sii_id[0] %s "
			 "into i2c adapter successfully\n", id->name);

		if (sii902xA != NULL) {
			pr_debug("\n============================================\n");
			pr_debug("SiI-902xA Driver Version 1.4\n");
			pr_debug("============================================\n");

			init_waitqueue_head(&hdmi_timer_wq);
			hdmi_timer_task =
			    kthread_create(hdmi_timer_kthread, NULL, "hdmi_timer_kthread");
			wake_up_process(hdmi_timer_task);

			/* mt_set_gpio_mode(GPIO_HDMI_EINT_9024, GPIO_MODE_00); */
			/* mt_set_gpio_dir(GPIO_HDMI_EINT_9024, GPIO_DIR_IN); */
			/* mt_set_gpio_pull_enable(GPIO_HDMI_EINT_9024, true); */
			/* mt_set_gpio_pull_select(GPIO_HDMI_EINT_9024,  GPIO_PULL_UP); */
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
                #if defined(GPIO_HDMI_PWR_1_2V_EN)
                hdmi_irq = mt_gpio_to_irq(CUST_EINT_EINT_HDMI_HPD_NUM);
                if(hdmi_irq)
                {
                   irq_set_irq_type(hdmi_irq,MT_LEVEL_SENSITIVE);

                   ret =  request_irq(hdmi_irq,&_sil9024_irq_handler,IRQF_TRIGGER_LOW,"EINT_HDMI_HPD-eint",NULL);
                   if(ret)
                   {
                       printk("HDMI IRQ LINE NOT AVAILABLE\n");    
                   }
                   else
                   {
                       disable_irq(hdmi_irq);
                   }                           
		}
                else 
                {
                   printk("[%s] can't find hdmi eint node\n",__func__);
                }
	        #else
                mt_eint_set_sens(CUST_EINT_EINT_HDMI_HPD_NUM, MT_LEVEL_SENSITIVE);
                mt_eint_registration(CUST_EINT_EINT_HDMI_HPD_NUM,  EINTF_TRIGGER_LOW, &_sil9024_irq_handler, 0);
			mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
#endif
		}
	} else {
		dev_err(&client->adapter->dev,
			"invalid i2c adapter: can not found dev_id matched\n");
		return -EIO;
	}
	return ret;

}

/*
static int hdmi_sii_remove(struct i2c_client *client)
{
	dev_info(&client->adapter->dev, "detached from i2c adapter successfully\n");

	return 0;
}
*/

static struct i2c_driver hdmi_sii_i2c_driver = {
	.driver = {
		   .name = DEVICE_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = hdmi_sii_probe,
	.remove = NULL,		/* __exit_p(hdmi_sii_remove), */
	.id_table = hmdi_sii_id,
};



static int sii9024_init(void)
{
	int ret;

#if defined(GPIO_HDMI_PWR_1_2V_EN)
	i2c_register_board_info(2, sii9024_i2c_hdmi, 5);
#else
	i2c_register_board_info(0, sii9024_i2c_hdmi, 5);
#endif
	ret = i2c_add_driver(&hdmi_sii_i2c_driver);
	if (ret)
		pr_debug("%s: failed to add sii902xA i2c driver\n", __func__);
	return ret;
}

void hdmi_AppGetEdidInfo(HDMI_EDID_INFO_T *pv_get_info)
{
    unsigned int ui4CEA_NTSC = SINK_480P | SINK_720P60 | SINK_1080P30 ;	 
    pv_get_info->ui4_ntsc_resolution |= ui4CEA_NTSC;
   
   
}
/*----------------------------------------------------------------------------*/
const HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = sii9024_set_util_funcs,	/*  */
		.get_params = sii9024_get_params,	/*  */
		.init = sii9024_init,	/*  */
		.enter = sii9024_enter,
		.exit = sii9024_exit,
		.suspend = sii9024_suspend,
		.resume = sii9024_resume,
		.video_config = sii9024_video_config,	/*  */
		.audio_config = sii9024_audio_config,
		.video_enable = sii9024_video_enable,
		.audio_enable = sii9024_audio_enable,
		.power_on = sii9024_power_on,	/*  */
		.power_off = sii9024_power_off,	/*  */
		.set_mode = sii9024_set_mode,
		.dump = sii9024_dump,
		.read = sii9024_read,
		.write = sii9024_write,
		.get_state = sii9024_get_state,
		.log_enable     = sii9024_log_enable,
		.getedid          = hdmi_AppGetEdidInfo,

	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
