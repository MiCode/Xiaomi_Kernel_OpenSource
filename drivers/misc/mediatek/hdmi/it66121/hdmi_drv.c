/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/* #include "hdmitx.h" */
#ifdef SUPPORT_CEC
#include "hdmitx_cec.h"
#endif
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>

#include "debug_hdmi.h"
#include "hdmi_drv.h"
#include "hdmitx_drv.h"
#include "hdmitx_sys.h"
#include "itx_typedef.h"
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
/* #include <linux/earlysuspend.h> */
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/byteorder/generic.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
/*#include <linux/sched.h>*/
#include <linux/time.h>
/* #include <linux/rtpm_prio.h> */
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/uaccess.h>
/*#include <mt-plat/mt_gpio.h>*/
#include <mach/upmu_hw.h>
#include <mach/upmu_sw.h>
#include <mt-plat/upmu_common.h>

#include "hdmitx.h"
/* #include <cust_eint.h> */
/* #include "cust_gpio_usage.h" */
/* #include "mach/eint.h" */
/* #include "mach/irqs.h" */

/* #include <mach/devs.h> */
/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_gpio.h> */
/* #include <mach/mt_pm_ldo.h> */
#include <uapi/linux/sched/types.h>

#define FALLING_EDGE_TRIGGER

#define MSCOUNT 1000
#define LOADING_UPDATE_TIMEOUT (3000 / 32) /* 3sec */
/* unsigned short u8msTimer = 0 ; */
/* unsigned short TimerServF = TRUE ; */

/* //////////////////////////////////////////////////////////////////// */
/* Authentication status */
/* //////////////////////////////////////////////////////////////////// */

/* #define TIMEOUT_WAIT_AUTH MS(2000) */

/* I2C Relate Definitions */
static struct i2c_client *it66121_i2c_client;
static struct timer_list r_hdmi_timer;

#ifdef PCADR
#define IT66121_plus 0x02
/* Define it66121's I2c slave Address of all pages
 * by the status of PCADR pin.
 */
#else
#define IT66121_plus 0x00
/* Define it66121's I2c Address of all pages by the status of PCADR pin. */
#endif

/* I2C address */
#define _80MHz 80000000
#define HDMI_TX_I2C_SLAVE_ADDR 0x98
#define CEC_I2C_SLAVE_ADDR 0x9C

/*I2C Device name */
#define DEVICE_NAME "it66121"

#define MAX_TRANSACTION_LENGTH 8

static int hdmi_ite_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);

struct HDMI_UTIL_FUNCS hdmi_util = {0};
unsigned char hdmi_powerenable = 0xff;

static struct pinctrl *hdmi_pinctrl;
static struct pinctrl_state *pins_hdmi_func;
static struct pinctrl_state *pins_hdmi_gpio;
/*
 *static const struct i2c_device_id hdmi_ite_id[] = {{DEVICE_NAME, 0}, {} };
 *
 *static struct i2c_driver hdmi_ite_i2c_driver = {
 *	.probe = hdmi_ite_probe,
 *	.remove = NULL,
 *	.driver = {
 *
 *			.name = DEVICE_NAME,
 *		},
 *	.id_table = hdmi_ite_id,
 *};
 */
	static const struct i2c_device_id hdmi_ite_id[] = {
		{DEVICE_NAME, 0},
		{},
	};
	MODULE_DEVICE_TABLE(i2c, hdmi_ite_id);

	static const struct of_device_id hdmi_ite_of_match[] = {
		{.compatible = "ite,it66121-i2c"},
		{.compatible = "ite,it6620-basic-i2c"},
		{.compatible = "ite,it6620-cec-i2c"},
		{.compatible = "ite,it6620-cap-i2c"},
		{},
	};
	MODULE_DEVICE_TABLE(of, hdmi_ite_of_match);

	static struct i2c_driver hdmi_ite_i2c_driver = {
		.probe = hdmi_ite_probe,
		.remove = NULL,
		.driver = { .name = DEVICE_NAME,
					.of_match_table = hdmi_ite_of_match,
					.owner = THIS_MODULE,
		},
		.id_table = hdmi_ite_id,
	};

/*
 *static struct i2c_board_info it66121_i2c_hdmi __initdata = {I2C_BOARD_INFO(
 *	DEVICE_NAME, (HDMI_TX_I2C_SLAVE_ADDR >> 1) + IT66121_plus)};
 */

/* static struct it66121_i2c_data *obj_i2c_data = NULL; */

/*Declare and definition for a hdmi kthread, this thread is used to check the
 * HDMI Status
 */
static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

struct regulator *hdmi_vcn33, *hdmi_vcn18, *hdmi_vrf12, *hdmi_vsim1;


/*
 *static int match_id(const struct i2c_device_id *id,
 *		    const struct i2c_client *client)
 *{
 *	if (strcmp(client->name, id->name) == 0)
 *		return true;
 *
 *	return false;
 *}
 */
void HDMI_reset(void)
{

	struct device_node *dn;
	int bus_switch_pin;
	int ret;

	IT66121_LOG("hdmi_ite66121 %s\n", __func__);

	IT66121_LOG(">>HDMI_Reset\n");

#if defined(GPIO_HDMI_9024_RESET)

	IT66121_LOG(">>Pull Down Reset Pin\n");
	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ZERO);

	msleep(100);

	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ONE);

	IT66121_LOG("<<Pull Up Reset Pin\n");
#else

	dn = of_find_compatible_node(NULL, NULL, "mediatek,mt8183-hdmitx");
	if (dn == NULL)
		IT66121_LOG("dn == NULL");
	bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
	gpio_direction_output(bus_switch_pin, 0);

	msleep(20);

	gpio_direction_output(bus_switch_pin, 1);

	ret = regulator_enable(hdmi_vsim1);
	if (ret != 0)
		IT66121_LOG("hdmi +5V regolator error\n");
#endif
	IT66121_LOG("<<HDMI_Reset\n");
}

int it66121_i2c_read_byte(u8 addr, u8 *data)
{
	u8 buf;
	int ret = 0;
	struct i2c_client *client = it66121_i2c_client;
	if (hdmi_powerenable == 1) {
		buf = addr;
		ret = i2c_master_send(client, (const char *)&buf, 1);
		if (ret < 0) {
			IT66121_LOG("send command error!!\n");
			return -EFAULT;
		}
		ret = i2c_master_recv(client, (char *)&buf, 1);
		if (ret < 0) {
			IT66121_LOG("reads data error!!\n");
			return -EFAULT;
		}
#if defined(HDMI_I2C_DEBUG)
		else
			IT66121_LOG("%s(0x%02X) = %02X\n", __func__, addr, buf);
#endif
		*data = buf;
		return 0;
	} else {
		return 0;
	}
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_read_byte);
/*----------------------------------------------------------------------------*/

int it66121_i2c_write_byte(u8 addr, u8 data)
{
	struct i2c_client *client = it66121_i2c_client;
	u8 buf[] = {addr, data};
	int ret = 0;
	if (hdmi_powerenable == 1) {
		ret = i2c_master_send(client, (const char *)buf, sizeof(buf));
		if (ret < 0) {
			IT66121_LOG("send command error!!\n");
			return -EFAULT;
		}
#if defined(HDMI_I2C_DEBUG)
		else
			IT66121_LOG("%s(0x%02X)= %02X\n", __func__, addr, data);
#endif
		return 0;
	} else {
		return 0;
	}
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_write_byte);
/*----------------------------------------------------------------------------*/

int it66121_i2c_read_block(u8 addr, u8 *data, int len)
{
	struct i2c_client *client = it66121_i2c_client;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{.addr = client->addr, .flags = 0, .len = 1, .buf = &beg},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		} };
	int err;

	if (len == 1)
		return it66121_i2c_read_byte(addr, data);

	if (!client) {
		return -EINVAL;
	} else if (len > MAX_TRANSACTION_LENGTH) {
		IT66121_LOG(" length %d exceeds %d\n", len,
			    MAX_TRANSACTION_LENGTH);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		IT66121_LOG("i2c_transfer error: (%d %p %d) %d\n", addr, data,
			    len, err);
		err = -EIO;
	} else {
		err = 0; /*no error */
	}
	return err;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_read_block);
/*----------------------------------------------------------------------------*/

int it66121_i2c_write_block(u8 addr, u8 *data, int len)
{
	/*because address also occupies one byte, the maximum length for write
	 * is 7 bytes
	 */
	int err, idx, num;
	char buf[MAX_TRANSACTION_LENGTH];
	struct i2c_client *client = it66121_i2c_client;

	if (!client) {
		return -EINVAL;
	} else if (len >= MAX_TRANSACTION_LENGTH) {
		IT66121_LOG(" length %d exceeds %d\n", len,
			    MAX_TRANSACTION_LENGTH);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		IT66121_LOG("send command error!!\n");
		return -EFAULT;
	}

	err = 0; /*no error */

	return err;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_write_block);
/*----------------------------------------------------------------------------*/

/* /it66121 power on */
/* Description: */
/*  */
int it66121_power_on(void)
{
	IT66121_LOG(">>> %s\n", __func__);

	if (hdmi_powerenable == 1) {
		IT66121_LOG("[hdmi]already power on, return\n");
		return 0;
	}
	hdmi_powerenable = 1;
	ite66121_pmic_power_on();

	/********This leave for mt6592 to power on it66121 ************/
	/* To Do */
	/* Reset The it66121 IC */
	HDMI_reset();
	msleep(20);
	/* This leave for it66121 internal init function */
	InitHDMITX_Variable();
	InitHDMITX();
	HDMITX_ChangeDisplayOption(HDMI_720p60, HDMI_RGB444);
	add_timer(&r_hdmi_timer);

/* Enable Interrupt of it66121 */
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif

	IT66121_LOG("<<< %s,\n", __func__);
	return 0;
}

enum HDMI_STATE it66121_get_state(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	if (HPDStatus)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);
}

void it66121_log_enable(bool enable)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);
}

static void it66121_power_down(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	if (hdmi_powerenable == 0) {
		IT66121_LOG("[hdmi]already power off, return\n");
		return;
	}
	hdmi_powerenable = 0;

	del_timer(&r_hdmi_timer);

	/* leave for it66121 internal power down */
	ite66121_pmic_power_off();

	it66121_FUNC();
	/* HDMITX_DisableVideoOutput(); */
	/* HDMITX_PowerDown(); */

	/* Leave for mt6592 to power down it66121 */

	IT66121_LOG("<<< %s,\n", __func__);
}
#if 0
static void _it66121_irq_handler(void)
{
	IT66121_LOG("it66121 irq\n");
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif

	/*Disable IT66121 HPD*/
	/*it66121_i2c_write_byte(0x09,0x01);*/

	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
}
#endif
/* Just For Test */
void HDMITX_DevLoopProc_Test(void)
{
	IT66121_LOG(">> %s\n", __func__);
}

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94};
	/* RTPM_PRIO_SCRN_UPDATE */

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_timer_wq,
					 atomic_read(&hdmi_timer_event));
		atomic_set(&hdmi_timer_event, 0);
		/* HDMITX_DevLoopProc_Test(); */
		if (hdmi_powerenable == 1)
			HDMITX_DevLoopProc();

#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
		mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif

		HDMITX_WriteI2C_Byte(REG_TX_INT_MASK1, 0x03);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

void it66121_dump(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);
}

static int it66121_audio_enable(bool enable)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}

static int it66121_video_enable(bool enable)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}

static int it66121_audio_config(enum HDMI_AUDIO_FORMAT aformat, int bitWidth)
{
	IT66121_LOG(">>> %s,\n", __func__);
	/* Leave for it66121  */
	dump_stack();
	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}

static int it66121_video_config(enum HDMI_VIDEO_RESOLUTION vformat,
				enum HDMI_VIDEO_INPUT_FORMAT vin, int vout)
{

	HDMI_Video_Type it66121_video_type = HDMI_480i60_16x9;

	IT66121_LOG(">>> %s vformat:0x%x\n", __func__, vformat);

	if (vformat == HDMI_VIDEO_720x480p_60Hz)
		it66121_video_type = HDMI_480p60;
	else if (vformat == HDMI_VIDEO_1280x720p_60Hz)
		it66121_video_type = HDMI_720p60;
	else if (vformat == HDMI_VIDEO_1920x1080p_30Hz)
		it66121_video_type = HDMI_1080p30;
	else {
		IT66121_LOG("error:sii9024_video_config vformat=%d\n", vformat);
		it66121_video_type = HDMI_720p60;
	}

	HDMITX_ChangeDisplayOption(it66121_video_type, HDMI_RGB444);
	/*mutex_lock(&mt66121_mutex_lock);*/
	HDMITX_SetOutput();
	/*mutex_unlock(&mt66121_mutex_lock);*/

	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}

static void it66121_suspend(void)
{

	IT66121_LOG(">>> %s,\n", __func__);
	/* leave for mt6592 operation */

	/*leave for it66121 operation */

	IT66121_LOG("<<< %s,\n", __func__);
}

static void it66121_resume(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* leave for mt6592 operation */

	/*leave for it66121 operation */

	IT66121_LOG("<<< %s,\n", __func__);
}

static void it66121_get_params(struct HDMI_PARAMS *params)
{
	enum HDMI_VIDEO_RESOLUTION input_resolution;

	input_resolution = params->init_config.vformat - 2;
	memset(params, 0, sizeof(struct HDMI_PARAMS));

	IT66121_LOG("%s res = %d\n", __func__, input_resolution);

	switch (input_resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_RISING;
		params->vsync_pol = HDMI_POLARITY_RISING;
		params->hsync_pulse_width = 62;
		params->hsync_back_porch = 60;
		params->hsync_front_porch = 16;
		params->vsync_pulse_width = 6;
		params->vsync_back_porch = 30;
		params->vsync_front_porch = 9;
		params->width = 720;
		params->height = 480;
		params->input_clock = HDMI_VIDEO_720x480p_60Hz;
		params->init_config.vformat = HDMI_VIDEO_720x480p_60Hz;
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 40;
		params->hsync_back_porch = 220;
		params->hsync_front_porch = 110;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch = 20;
		params->vsync_front_porch = 5;
		params->width = 1280;
		params->height = 720;
		params->input_clock = HDMI_VIDEO_1280x720p_60Hz;
		params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
		break;
	case HDMI_VIDEO_1920x1080p_30Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 44;
		params->hsync_back_porch = 148;
		params->hsync_front_porch = 88;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch = 36;
		params->vsync_front_porch = 4;
		params->width = 1920;
		params->height = 1080;
		params->input_clock = HDMI_VIDEO_1920x1080p_30Hz;
		params->init_config.vformat = HDMI_VIDEO_1920x1080p_30Hz;
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 44;
		params->hsync_back_porch = 148;
		params->hsync_front_porch = 88;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch = 36;
		params->vsync_front_porch = 4;
		params->width = 1920;
		params->height = 1080;
		params->input_clock = HDMI_VIDEO_1920x1080p_60Hz;
		params->init_config.vformat = HDMI_VIDEO_1920x1080p_60Hz;
		break;
	default:
		IT66121_LOG("Unknown support resolution\n");
		break;
	}

	params->init_config.aformat = HDMI_AUDIO_48K_2CH;
	params->rgb_order = HDMI_COLOR_ORDER_RGB;
	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->output_mode = HDMI_OUTPUT_MODE_LCD_MIRROR;
	params->is_force_awake = 1;
	params->is_force_landscape = 1;
}

static void it66121_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(struct HDMI_UTIL_FUNCS));
}

static int it66121_init(void)
{
	int ret = 0;

	IT66121_LOG(">>> %s,\n", __func__);

	/* HDMI_reset(); */

	/* This leave for MT6592 initialize it66121 */
	/* register i2c device */
	if (ret)
		IT66121_LOG(KERN_ERR "%s: failed to add it66121 i2c driver\n",
			    __func__);

	/* This leave for MT6592 internal initialization */

	IT66121_LOG("<<< %s,\n", __func__);
	return ret;
}

#if 0
static int hdmi_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0, ret = -1;
	u8 ids[4] = { 0 };
	struct it66121_i2c_data *obj;

	IT66121_LOG("MediaTek HDMI i2c probe\n");

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL) {
		ret = -ENOMEM;
		IT66121_LOG(DEVICE_NAME ": Allocate ts memory fail\n");
		return ret;
	}
	obj_i2c_data = obj;
	obj->client = client;
	it66121_i2c_client = obj->client;
	i2c_set_clientdata(client, obj);


	/* check if chip exist */
	it66121_i2c_read_byte(0x0, &ids[0]);
	it66121_i2c_read_byte(0x1, &ids[1]);
	it66121_i2c_read_byte(0x2, &ids[2]);
	it66121_i2c_read_byte(0x3, &ids[3]);
	IT66121_LOG("HDMITX ID: %x-%x-%x-%x\n", ids[0], ids[1], ids[2], ids[3]);

	/* 54-49-12-6 */
	if (ids[0] != 0x54 || ids[1] != 0x49) {
		/* || ids[2]!=0x12 || ids[3]!=0x06) */
		IT66121_LOG("chip ID incorrect: %x-%x-%x-%x !!\n",
					ids[0], ids[1], ids[2], ids[3]);
		/* it66121_power_off(); */
		return -1;
	}
	IT66121_LOG("MediaTek HDMI i2c probe success\n");

	IT66121_LOG("\n============================================\n");
	IT66121_LOG("IT66121 HDMI Version\n");
	IT66121_LOG("============================================\n");

	init_waitqueue_head(&hdmi_timer_wq);
	hdmi_timer_task = kthread_create(hdmi_timer_kthread, NULL,
										"hdmi_timer_kthread");
	wake_up_process(hdmi_timer_task);

#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)

	IT66121_LOG(">>IT66121 Request IRQ\n");

	mt_eint_set_sens(CUST_EINT_EINT_HDMI_HPD_NUM, MT_LEVEL_SENSITIVE);
	mt_eint_registration(CUST_EINT_EINT_HDMI_HPD_NUM,
				EINTF_TRIGGER_LOW, &_it66121_irq_handler, 0);
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);

	IT66121_LOG("<<IT66121 Request IRQ\n");
#endif



	return 0;
}
#endif

/*----------------------------------------------------------------------------*/

static int hdmi_ite_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	//int ret = 0;

	IT66121_LOG(">>%s\n", __func__);
	/* static struct mxc_lcd_platform_data *plat_data; */
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE | I2C_FUNC_I2C))
		return -ENODEV;
#if 0
	if (match_id(&hdmi_ite_id[0], client)) {

		IT66121_LOG(">>Match id Done\n");

		it66121_i2c_client = client;
		dev_info(
			&client->adapter->dev,
			"attached hmdi_ite_id[0] %s into i2c adapter successfully\n",
			id->name);

		if (it66121_i2c_client != NULL) {
			IT66121_LOG(
				"\n============================================\n");
			IT66121_LOG("IT66121 HDMI Version\n");
			IT66121_LOG(
				"============================================\n");

			init_waitqueue_head(&hdmi_timer_wq);
			hdmi_timer_task = kthread_create(
				hdmi_timer_kthread, NULL, "hdmi_timer_kthread");
			wake_up_process(hdmi_timer_task);

#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)

			IT66121_LOG(">>IT66121 Request IRQ\n");
			mt_eint_set_sens(CUST_EINT_EINT_HDMI_HPD_NUM,
					 MT_LEVEL_SENSITIVE);
			mt_eint_registration(CUST_EINT_EINT_HDMI_HPD_NUM,
					     EINTF_TRIGGER_LOW,
					     &_it66121_irq_handler, 0);
			mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
			IT66121_LOG("<<IT66121 Request IRQ\n");
#endif
		}
	} else {
		IT66121_LOG(
			"invalid i2c adapter: can not found dev_id matched\n");
		return -EIO;
	}
#else
	if (strcmp(client->name, "it66121-i2c") == 0) {

		pr_debug(">>Match id Done\n");

		it66121_i2c_client = client;

		if (it66121_i2c_client != NULL) {
			IT66121_LOG("======\n");
			IT66121_LOG("IT66121 HDMI Version\n");
			IT66121_LOG("======\n");

			init_waitqueue_head(&hdmi_timer_wq);
			hdmi_timer_task =
			    kthread_create(hdmi_timer_kthread,
				NULL, "hdmi_timer_kthread");
			wake_up_process(hdmi_timer_task);
		}
	}
#endif

	IT66121_LOG("<<%s\n", __func__);

	return 0;
}

#define HDMI_MAX_INSERT_CALLBACK 10
static CABLE_INSERT_CALLBACK hdmi_callback_table[HDMI_MAX_INSERT_CALLBACK];
void hdmi_register_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
	int i = 0;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == cb)
			break;
	}
	if (i < HDMI_MAX_INSERT_CALLBACK)
		return;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == NULL)
			break;
	}
	if (i == HDMI_MAX_INSERT_CALLBACK) {
		IT66121_LOG("not enough mhl callback entries for module\n");
		return;
	}

	hdmi_callback_table[i] = cb;
	IT66121_LOG("callback: %p,i: %d\n", hdmi_callback_table[i], i);
}

void hdmi_unregister_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
	int i;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == cb) {
			IT66121_LOG(
				"unregister cable insert callback: %p, i: %d\n",
				hdmi_callback_table[i], i);
			hdmi_callback_table[i] = NULL;
			break;
		}
	}
	if (i == HDMI_MAX_INSERT_CALLBACK) {
		IT66121_LOG(
			"Try to unregister callback function 0x%lx which was not registered\n",
			(unsigned long int)cb);
		return;
	}
}

void hdmi_invoke_cable_callbacks(enum HDMI_STATE state)
{
	int i = 0, j = 0;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i])
			j = i;
	}

	if (hdmi_callback_table[j]) {
		IT66121_LOG("callback: %p, state: %d, j: %d\n",
			    hdmi_callback_table[j], state, j);
		hdmi_callback_table[j](state);
	}
}

const struct HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = it66121_set_util_funcs, /*  */
		.get_params = it66121_get_params,	 /*  */
		.init = it66121_init,			  /* InitHDMITX */
		/* .enter          = it66121_enter, */
		/* .exit           = it66121_exit, */
		.suspend = it66121_suspend,
		.resume = it66121_resume,
		.video_config =
			it66121_video_config,
			/* it66121_video_config,HDMITX_SetOutput*/
		.audio_config =
			it66121_audio_config,
			/* it66121_audio_config,HDMITX_SetAudioOutput*/
		.video_enable =
			it66121_video_enable,
			/* HDMITX_EnableVideoOutput */
		.audio_enable =
			it66121_audio_enable,    /* HDMITX_SetAudioOutput */
		.power_on = it66121_power_on,    /* HDMITX_PowerOn */
		.power_off = it66121_power_down, /* HDMITX_PowerDown */
		/* .set_mode             = it66121_set_mode, */
		.dump = it66121_dump, /* it66121_dump,DumpHDMITXReg */
		.getedid = ite66121_AppGetEdidInfo,
		/* .read           = it66121_read, */
		/* .write          = it66121_write, */
		.get_state = it66121_get_state,
		.log_enable = it66121_log_enable,
		.register_callback = hdmi_register_cable_insert_callback,
		.unregister_callback = hdmi_unregister_cable_insert_callback,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);


int ite66121_pmic_power_on(void)
{
	int ret;

	pinctrl_select_state(hdmi_pinctrl, pins_hdmi_func);

	ret = regulator_enable(hdmi_vrf12);
	ret = regulator_enable(hdmi_vcn18);
	ret = regulator_enable(hdmi_vcn33);

	if (ret != 0)
		IT66121_LOG("hdmi regolator error\n");

	IT66121_LOG("%s\n", __func__);
	return 1;
}
int ite66121_pmic_power_off(void)
{
	struct device_node *dn;
	int bus_switch_pin;
	int ret;

	pinctrl_select_state(hdmi_pinctrl, pins_hdmi_gpio);
	ret = regulator_disable(hdmi_vcn33);
	ret = regulator_disable(hdmi_vcn18);
	ret = regulator_disable(hdmi_vrf12);

	if (ret != 0)
		IT66121_LOG("hdmi regolator error\n");

	dn = of_find_compatible_node(NULL, NULL, "mediatek,mt8183-hdmitx");
	if (dn == NULL)
		IT66121_LOG("dn == NULL");
	bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
	gpio_direction_output(bus_switch_pin, 0);

	ret = regulator_disable(hdmi_vsim1);
	if (ret != 0)
		IT66121_LOG("hdmi regolator error\n");
	IT66121_LOG("%s\n", __func__);
	return 1;
}

static char debug_buffer[2048];

static void process_dbg_opt(const char *opt)
{
	unsigned int vadr_regstart, val_temp;
	u8 val;
	int i, ret;
	struct device_node *dn;
	int bus_switch_pin;
	unsigned int res;
	if (strncmp(opt, "edid", 4) == 0) {
		IT66121_LOG("resolution = 0x%x\n", sink_support_resolution);
	}
	if (strncmp(opt, "res:", 4) == 0) {
		ret = sscanf(opt + 4, "%x", &res);
		IT66121_LOG("hdmi %d\n", res);
		it66121_video_config((enum HDMI_VIDEO_RESOLUTION)res,
				     HDMI_VIN_FORMAT_RGB888,
				     HDMI_VOUT_FORMAT_RGB888);
	}

	if (strncmp(opt, "disable", 7) == 0) {
		IT66121_LOG("disable vrf12\n");
		ret = regulator_disable(hdmi_vrf12);
	}
	if (strncmp(opt, "enable", 6) == 0) {
		IT66121_LOG("enable vrf12\n");
		ret = regulator_enable(hdmi_vrf12);
	}
	if (strncmp(opt, "on", 2) == 0) {
		dn = of_find_compatible_node(NULL, NULL,
					     "mediatek,mt8183-hdmitx");
		if (dn == NULL)
			IT66121_LOG("dn == NULL");
		bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
		gpio_direction_output(bus_switch_pin, 1);
		ret = regulator_enable(hdmi_vsim1);
	}

	if (strncmp(opt, "off", 3) == 0) {
		dn = of_find_compatible_node(NULL, NULL,
				"mediatek,mt8183-hdmitx");
		if (dn == NULL)
			IT66121_LOG("dn == NULL");
		bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
		gpio_direction_output(bus_switch_pin, 0);

		ret = regulator_disable(hdmi_vsim1);
	}
	if (strncmp(opt, "power_on", 8) == 0) {
		IT66121_LOG("hdmi power_on\n");
		ite66121_pmic_power_on();
	}
	if (strncmp(opt, "power_off", 9) == 0) {
		IT66121_LOG("hdmi power_off\n");
		ite66121_pmic_power_off();
	}
	if (strncmp(opt, "init", 4) == 0) {
		IT66121_LOG("hdmi it66121_init\n");
		it66121_init();
	}
	if (strncmp(opt, "itepower_on", 11) == 0) {
		IT66121_LOG("hdmi it66121_power_on\n");
		ite66121_pmic_power_on();
		HDMI_reset();
	}
	if (strncmp(opt, "itepower_off", 12) == 0) {
		IT66121_LOG("hdmi it66121_power_off\n");
		ite66121_pmic_power_off();
	}
	if (strncmp(opt, "read:", 5) == 0) {
		ret = sscanf(opt + 5, "%x", &vadr_regstart);
		IT66121_LOG("r:0x%08x\n", vadr_regstart);
		it66121_i2c_read_byte(vadr_regstart, &val);
		IT66121_LOG("0x%08x = 0x%x\n", vadr_regstart, val);
	}
	if (strncmp(opt, "write:", 6) == 0) {
		ret = sscanf(opt + 6, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("w:0x%08x=0x%x\n", vadr_regstart, val);
		it66121_i2c_write_byte(vadr_regstart, val);
	}

	if (strncmp(opt, "reg_dump", 8) == 0) {
		IT66121_LOG("***** basic reg bank0 dump start *****\n");
		//IT662x_eARC_RX_Bank(0);
		Switch_HDMITX_Bank(0);
		IT66121_LOG("|   00 01 02 03 04 05 06 07 08\n");
		for (i = 0; i <= 248; i = i+8) {
			IT66121_LOG("%x: %x %x %x %x %x %x %x %x\n",
				i, HDMITX_ReadI2C_Byte(i),
			HDMITX_ReadI2C_Byte(i+1),
			HDMITX_ReadI2C_Byte(i+2),
			HDMITX_ReadI2C_Byte(i+3),
			HDMITX_ReadI2C_Byte(i+4),
			HDMITX_ReadI2C_Byte(i+5),
			HDMITX_ReadI2C_Byte(i+6),
			HDMITX_ReadI2C_Byte(i+7));
		}
		IT66121_LOG("***** basic reg bank0 dump end *****\n");

		IT66121_LOG("***** basic reg bank1 dump start *****\n");
		//IT662x_eARC_RX_Bank(1);
		Switch_HDMITX_Bank(1);
		IT66121_LOG("|   00 01 02 03 04 05 06 07 08\n");
		for (i = 0; i <= 248; i = i+8) {
			IT66121_LOG("%x: %x %x %x %x %x %x %x %x\n",
				i, HDMITX_ReadI2C_Byte(i),
			HDMITX_ReadI2C_Byte(i+1),
			HDMITX_ReadI2C_Byte(i+2),
			HDMITX_ReadI2C_Byte(i+3),
			HDMITX_ReadI2C_Byte(i+4),
			HDMITX_ReadI2C_Byte(i+5),
			HDMITX_ReadI2C_Byte(i+6),
			HDMITX_ReadI2C_Byte(i+7));
		}
		//IT662x_eARC_RX_Bank(0);
		Switch_HDMITX_Bank(0);
		IT66121_LOG("***** basic reg bank1 dump end *****\n");
	}

}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[extd] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}
static const char STR_HELP[] = "\n"
			       "USAGE\n"
			       "HDMI power on:\n"
			       "		echo power_on>hdmi_test"

			       "\n";

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	int n = 0;

	n += scnprintf(debug_buffer + n, debug_bufmax - n, STR_HELP);
	debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static const struct file_operations debug_fops = {
	.read = debug_read, .write = debug_write, .open = debug_open,
};
struct dentry *ite66121_dbgfs;

void ITE66121_DBG_Init(void)
{
	ite66121_dbgfs = debugfs_create_file("hdmi_test", S_IFREG | 0444,
					     NULL, (void *)0, &debug_fops);
}

void hdmi_poll_isr(unsigned long n)
{
	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
	mod_timer(&r_hdmi_timer, jiffies + 1000 / (1000 / HZ));
}

void vGet_Pinctrl_Mode(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev == NULL)
		IT66121_LOG("vGet_DDC_Mode Error, Invalid device pointer\n");

	hdmi_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(hdmi_pinctrl)) {
		ret = PTR_ERR(hdmi_pinctrl);
		IT66121_LOG("HDMI pins, failure of setting\n");
	} else {
		pins_hdmi_func =
			pinctrl_lookup_state(hdmi_pinctrl, "hdmi_poweron");
		if (IS_ERR(pins_hdmi_func)) {
			ret = PTR_ERR(pins_hdmi_func);
			IT66121_LOG(
				"cannot find pins_hdmi_func pinctrl hdmi_poweron\n");
		}

		pins_hdmi_gpio =
			pinctrl_lookup_state(hdmi_pinctrl, "hdmi_poweroff");
		if (IS_ERR(pins_hdmi_gpio)) {
			ret = PTR_ERR(pins_hdmi_gpio);
			IT66121_LOG(
				"cannot find pins_hdmi_gpio pinctrl hdmi_poweroff\n");
		}
	}
}

int hdmi_internal_probe(struct platform_device *pdev)
{
	//int ret = 0;

	IT66121_LOG(">>> %s,\n", __func__);

	/* HDMI_reset(); */


	/* This leave for MT6592 initialize it66121 */
	/* register i2c device */
	/*if (ret)
	 *	IT66121_LOG(KERN_ERR "%s: failed to add it66121 i2c driver\n",
	 *		__func__);
	 */
	vGet_Pinctrl_Mode(pdev);
	hdmi_vcn33 = devm_regulator_get(&pdev->dev, "vcn33");
	hdmi_vcn18 = devm_regulator_get(&pdev->dev, "vcn18");
	hdmi_vrf12 = devm_regulator_get(&pdev->dev, "vrf12");
	hdmi_vsim1 = devm_regulator_get(&pdev->dev, "vsim1");

	if (IS_ERR(hdmi_vsim1))
		IT66121_LOG("hdmi hdmi_vsim1 error\n");
	if (IS_ERR(hdmi_vcn33))
		IT66121_LOG("hdmi hdmi_vcn33 error\n");
	if (IS_ERR(hdmi_vcn18))
		IT66121_LOG("hdmi hdmi_vcn18 error\n");
	if (IS_ERR(hdmi_vrf12))
		IT66121_LOG("hdmi hdmi_vrf12 error\n");

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires =
		jiffies + 1000 / (1000 / HZ); /* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);

	ITE66121_DBG_Init();
	IT66121_LOG("%s done successful\n", __func__);
	return 0;
}
static int hdmi_internal_remove(struct platform_device *dev)
{
	return 0;
}





static const struct of_device_id hdmi_of_ids[] = {
	{
		.compatible = "mediatek,mt8183-hdmitx",
	},
	{} };

static struct platform_driver hdmi_of_driver = {
	.probe = hdmi_internal_probe,
	.remove = hdmi_internal_remove,
	.driver = {
		.name = "mtkhdmi", .of_match_table = hdmi_of_ids,
	} };

static int __init mtk_hdmitx_init(void)
{
	int ret;

	IT66121_LOG("%s\n", __func__);
	if (platform_driver_register(&hdmi_of_driver)) {
		IT66121_LOG("failed to register disp driver\n");
		ret = -1;
	}

	return 0;
}
static void __exit mtk_hdmitx_exit(void)
{
	IT66121_LOG("%s\n", __func__);
}
/*----------------------------------------------------------------------------*/
static int __init ite66121_i2c_board_init(void)
{
	int ret = 0;
#if 0
	unsigned int i2c_port = 0;
	struct device_node *dn;

	IT66121_LOG("hdmi %s\n", __func__);
	dn = of_find_compatible_node(NULL, NULL, "mediatek,mt8183-hdmitx");
	if (!dn) {
		IT66121_LOG("Failed to find HDMI node\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(dn, "mediatek,hdmi_bridgeic_port",
				   &i2c_port);
	if (ret < 0)
		i2c_port = 6;
	IT66121_LOG("i2c_port %d\n", i2c_port);
	ret = i2c_register_board_info(i2c_port, &it66121_i2c_hdmi, 1);
	if (ret)
		pr_debug("failed register hdmi i2c,please check port %d\n",
			 i2c_port);
	return ret;
#else
	ret = i2c_add_driver(&hdmi_ite_i2c_driver);
	return ret;
#endif
}
/*----------------------------------------------------------------------------*/
//core_initcall(ite66121_i2c_board_init);
module_init(ite66121_i2c_board_init);
late_initcall(mtk_hdmitx_init);
module_exit(mtk_hdmitx_exit);
