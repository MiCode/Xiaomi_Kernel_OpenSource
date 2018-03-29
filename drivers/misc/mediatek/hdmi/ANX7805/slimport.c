/*
* Copyright(c) 2012-2013, Analogix Semiconductor All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/ 
  
#define pr_fmt(fmt) "%s %s: " fmt, "anx7805", __func__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include "slimport.h"
#include <linux/async.h>
#include <linux/of_platform.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>

#include "hdmi_drv.h"
#include "slimport_edid.h"
#include "slimport_edid_3d_api.h"

/*#include "mach/eint.h"*/
#include "mach/irqs.h"
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#endif
/*#include <cust_eint.h>*/
#include <mt-plat/mt_gpio.h>
/* #include <cust_gpio_usage.h> */


/*SLIMPORT_FEATURE*/
#include "slimport_platform.h"

struct anx7805_data {
	struct i2c_client *client;
	struct anx7805_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock slimport_lock;
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;
	const char *vdd10_name;
	const char *avdd33_name;
	struct regulator *avdd_reg;
	struct regulator *vdd_reg;
//struct platform_device *hdmi_pdev;
//	struct msm_hdmi_sp_ops *hdmi_sp_ops;
	bool update_chg_type;
};

struct anx7805_data *the_chip;
static struct mutex dp_i2c_lock;

/*
#ifdef HDCP_EN
static bool hdcp_enable = 1;
#else
static bool hdcp_enable;
#endif
*/
struct completion init_aux_ch_completion;
//static uint32_t sp_tx_chg_current_ma = NORMAL_CHG_I_MA;
static wait_queue_head_t mhl_irq_wq;
static struct task_struct *mhl_irq_task = NULL;
static atomic_t mhl_irq_event = ATOMIC_INIT(0);
void *slimport_edid_p = NULL;

static int anx7805_avdd_3p3_power(struct anx7805_data *chip, int on)
{
	static int on_state;
	int ret = 0;

#ifndef dp_to_do
	return 0;
#endif

	if (on_state == on) {
		pr_info("avdd 3.3V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->avdd_reg) {
		chip->avdd_reg = regulator_get(NULL, chip->avdd33_name);
		if (IS_ERR(chip->avdd_reg)) {
			ret = PTR_ERR(chip->avdd_reg);
			pr_err("regulator_get %s failed. rc = %d\n",
			       chip->avdd33_name, ret);
			chip->avdd_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->avdd_reg);
		if (ret) {
			pr_err("avdd_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->avdd_reg);
		if (ret) {
			pr_err("avdd_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->avdd_reg);
	chip->avdd_reg = NULL;
out:
	return ret;
}

static int anx7805_vdd_1p0_power(struct anx7805_data *chip, int on)
{
	static int on_state;
	int ret = 0;
#ifndef dp_to_do
		return 0;
#endif

	if (on_state == on) {
		pr_info("vdd 1.0V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->vdd_reg) {
		chip->vdd_reg = regulator_get(NULL, chip->vdd10_name);
		if (IS_ERR(chip->vdd_reg)) {
			ret = PTR_ERR(chip->vdd_reg);
			pr_err("regulator_get %s failed. ret = %d\n",
			       chip->vdd10_name, ret);
			chip->vdd_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->vdd_reg);
		if (ret) {
			pr_err("vdd_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->vdd_reg);
		if (ret) {
			pr_err("vdd_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->vdd_reg);
	chip->vdd_reg = NULL;
out:
	return ret;
}

#if 0
int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	if (!the_chip)
		return -EINVAL;

	the_chip->client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(the_chip->client, offset);
	if (ret < 0) {
		pr_err("failed(%d) to read i2c addr=%x-%x-%x\n",ret, slave_addr, offset,*buf);
		return ret;
	}
	*buf = (uint8_t) ret;
	///pr_err("sp_r=%x-%x-%x\n", slave_addr, offset,*buf);

	return 0;
}
 
int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;
	I2C_flag |= 0x10;

	if (!the_chip)
		return -EINVAL;

	the_chip->client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(the_chip->client, offset, value);
	if (ret < 0) {
		pr_err("failed(%d) to write i2c addr=%x-%x-%x\n",ret, slave_addr, offset, value);
	}
	///pr_err("sp_w=%x-%x-%x\n", slave_addr, offset,value);
	return ret;
}
#else
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

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;
	uint8_t regAddress =offset;
	/*int old_ext_flag = 0;*/

	mhl_sw_mutex_lock(&dp_i2c_lock);
	regAddress =offset;

	///pr_err("%x-%x\n", slave_addr, I2C_flag);
	if (!the_chip)
	{
		mhl_sw_mutex_unlock(&dp_i2c_lock);
		return -EINVAL;
	}

	the_chip->client->addr = (slave_addr >> 1);///& I2C_MASK_FLAG)|I2C_WR_FLAG;
	
	/*the_chip->client->timing = 100;*/
#if 1
	ret = i2c_master_send(the_chip->client, (const char*)&regAddress, sizeof(uint8_t));  
	if(ret < 0)
	{
		pr_err("failed(%d) to read i2c addr=%x-%x-%x\n",ret, slave_addr, offset,*buf);
		mhl_sw_mutex_unlock(&dp_i2c_lock);
		/*
		mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_OUT_ONE);
		*/
		return 0;
	}
	else
	{
		ret = i2c_master_recv(the_chip->client, (char*)buf, 1);
		if(ret < 0)
		{
			pr_err("failed(%d) to read i2c addr2=%x-%x-%x\n",ret, slave_addr, offset,*buf);
			mhl_sw_mutex_unlock(&dp_i2c_lock);
			/*
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MODE_00);
			mt_set_gpio_dir(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_OUT_ONE);
			*/
			return ret;
		}
	}	

	
	if (ret < 0) {
		pr_err("failed(%d) to read i2c addr=%x-%x-%x\n",ret, slave_addr, offset,*buf);
		mhl_sw_mutex_unlock(&dp_i2c_lock);
		/*
		mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_OUT_ONE);
		*/
		return ret;
	}
#else
	///the_chip->client->addr = (the_chip->client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG| I2C_RS_FLAG;
	old_ext_flag = the_chip->client->ext_flag;
	the_chip->client->ext_flag |=( I2C_WR_FLAG | I2C_RS_FLAG);
	ret = i2c_master_send(the_chip->client, &regAddress, 0x101);
	*buf = regAddress;

	if (ret < 0) {
			pr_err("failed(%d) to read i2c addr=%x-%x-%x\n",ret, slave_addr, offset,*buf);
			mhl_sw_mutex_unlock(&dp_i2c_lock);
			mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MODE_00);
			mt_set_gpio_dir(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_OUT_ONE);
			return ret;
	}
	the_chip->client->ext_flag = old_ext_flag;
#endif
	///*buf = (uint8_t) ret;
	mhl_sw_mutex_unlock(&dp_i2c_lock);
	/*pr_err("sp_r=%x-%x-%x\n", slave_addr, offset,*buf);*/
	return 0;
}
 
int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;
	char write_data[2];
	
	mhl_sw_mutex_lock(&dp_i2c_lock);
	write_data[0]= offset;
	write_data[1]= value;
	///pr_err("%x-%x\n", slave_addr, I2C_flag);

	if (!the_chip)
	{
		mhl_sw_mutex_unlock(&dp_i2c_lock);
		return -EINVAL;
	}
	
	the_chip->client->addr = (slave_addr >> 1);
	/*the_chip->client->timing = 100;*/
	ret = i2c_master_send(the_chip->client, write_data, 2);  
	if (ret < 0) {
		pr_err("failed(%d) to write i2c addr=%x-%x-%x\n",ret, slave_addr, offset, value);
		mhl_sw_mutex_unlock(&dp_i2c_lock);
		/*
		mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_OUT_ONE);
		*/
		return ret;
	}

	mhl_sw_mutex_unlock(&dp_i2c_lock);
	/*pr_err("sp_w=%x-%x-%x\n", slave_addr, offset,value);*/
	return ret;
}

#endif
void sp_tx_hardware_poweron(void)
{
#ifndef dp_to_do
		reset_mhl_board(5, 5, true);
/*
		set_pin_high_low(RESET_PIN, 0);
		msleep(1);
		set_pin_high_low(PD_PIN, 0);
		msleep(2);
		set_pin_high_low(RESET_PIN, 1);
*/
		pr_info("anx7805 power on, NOT dp_to_do\n");

		return;
#endif

	if (!the_chip)
		return;

	gpio_set_value(the_chip->gpio_reset, 0);
	msleep(1);
	gpio_set_value(the_chip->gpio_p_dwn, 0);
	msleep(2);
	anx7805_vdd_1p0_power(the_chip, 1);
	msleep(5);
	gpio_set_value(the_chip->gpio_reset, 1);
	pr_info("anx7805 power on\n");
}

void sp_tx_hardware_powerdown(void)
{
//int status = 0;
#ifndef dp_to_do
	reset_mhl_board(5, 5, false);
/*
	set_pin_high_low(RESET_PIN, 0);
	msleep(2);
	set_pin_high_low(PD_PIN, 1);
	msleep(1);
*/
	pr_info("anx7805 power down, NOT dp_to_do\n");

	return;
#endif

	if (!the_chip)
		return;

	gpio_set_value(the_chip->gpio_reset, 0);
	msleep(1);
	anx7805_vdd_1p0_power(the_chip, 0);
	msleep(2);
	gpio_set_value(the_chip->gpio_p_dwn, 1);
	msleep(1);

	/* turn off hpd */
	/*
	if (the_chip->hdmi_sp_ops->set_upstream_hpd) {
	status = the_chip->hdmi_sp_ops->set_upstream_hpd(
	the_chip->hdmi_pdev, 0);
	if (status)
	pr_err("failed to turn off hpd");
	}
	*/
	pr_info("anx7805 power down\n");
}

/*
static void sp_tx_power_down_and_init(void)
{
	vbus_power_ctrl();
	sp_tx_power_down(SP_TX_PWR_REG);
	sp_tx_power_down(SP_TX_PWR_TOTAL);
	sp_tx_hardware_powerdown();
	sp_tx_pd_mode = 1;
	sp_tx_link_config_done = 0;
	sp_tx_hw_lt_enable = 0;
	sp_tx_hw_lt_done = 0;
	sp_tx_rx_type = RX_NULL;
	sp_tx_rx_type_backup = RX_NULL;
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
}

*/

int slimport_read_edid_break(uint8_t *edid_break)
{
 	edid_break= (uint8_t *)&bEDIDBreak;

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_break);

int slimport_read_edid_All(uint8_t *edid_buf)
{
	int block;
	block = bEDID_firstblock[0x7e];
	if (block  == 0) {
		memcpy(edid_buf, bEDID_firstblock, sizeof(bEDID_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bEDID_firstblock, sizeof(bEDID_firstblock));
		memcpy((edid_buf+128), bEDID_extblock, sizeof(bEDID_extblock));
	} else if (block == 3) {
		memcpy(edid_buf, bEDID_firstblock, sizeof(bEDID_firstblock));
		memcpy((edid_buf+128), bEDID_extblock, sizeof(bEDID_extblock));
		memcpy((edid_buf+256), bEDID_fourblock, sizeof(bEDID_fourblock));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_All);

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bEDID_firstblock, sizeof(bEDID_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bEDID_extblock, sizeof(bEDID_extblock));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);


int update_audio_format_setting(unsigned char  bAudio_Fs, unsigned char bAudio_word_len, int Channel_Num, I2SLayOut layout)
{
	SP_CTRL_AUDIO_FORMAT_Set(AUDIO_I2S,bAudio_Fs ,bAudio_word_len);
	SP_CTRL_I2S_CONFIG_Set(Channel_Num , layout);
	audio_format_change=1;
	
	return 0;
}
EXPORT_SYMBOL(update_audio_format_setting);

int update_video_format_setting(int video_format)
{
	pr_err("video_format:%d, three_3d_format:%d\n", video_format, three_3d_format);
	if (video_format != three_3d_format) {
		video_format_change=1;
		SP_TX_Video_Mute(1);
		SP_TX_Enable_Audio_Output(0);
	}	
	three_3d_format = video_format;
	return 0;
}

bool slimport_is_connected(void)
{
	bool result = false;

	if (!the_chip)
		return false;

	if (gpio_get_value(mhl_eint_gpio_number)) {
		mdelay(10);
		if (gpio_get_value(mhl_eint_gpio_number)) {
			pr_info("slimport cable is detected\n");
			result = true;
		}
	}

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);










static void anx7805_free_gpio(struct anx7805_data *anx7805)
{
#ifndef dp_to_do
		return;
#endif

	gpio_free(anx7805->gpio_cbl_det);
	gpio_free(anx7805->gpio_int);
	gpio_free(anx7805->gpio_reset);
	gpio_free(anx7805->gpio_p_dwn);
}

static int anx7805_init_gpio(struct anx7805_data *anx7805)
{
	int ret = 0;
#ifndef dp_to_do
		return 0;
#endif

	ret = gpio_request_one(anx7805->gpio_p_dwn,
	                       GPIOF_OUT_INIT_HIGH, "anx_p_dwn_ctl");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7805->gpio_p_dwn);
		goto out;
	}

	ret = gpio_request_one(anx7805->gpio_reset,
	                       GPIOF_OUT_INIT_LOW, "anx7805_reset_n");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7805->gpio_reset);
		goto err0;
	}

	ret = gpio_request_one(anx7805->gpio_int,
	                       GPIOF_IN, "anx7805_int_n");

	if (ret) {
		pr_err("failed to request gpio %d\n", anx7805->gpio_int);
		goto err1;
	}

	ret = gpio_request_one(anx7805->gpio_cbl_det,
	                       GPIOF_IN, "anx7805_cbl_det");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7805->gpio_cbl_det);
		goto err2;
	}

	gpio_set_value(anx7805->gpio_reset, 0);
	gpio_set_value(anx7805->gpio_p_dwn, 1);

	goto out;

err2:
	gpio_free(anx7805->gpio_int);
err1:
	gpio_free(anx7805->gpio_reset);
err0:
	gpio_free(anx7805->gpio_p_dwn);
out:
	return ret;
}

static int anx7805_system_init(void)
{
	int ret = 0;

	ret = SP_CTRL_Chip_Detect();
	if (ret == 0) {
		pr_err("failed to detect anx7805\n");
		return -ENODEV;
	}

	SP_CTRL_Chip_Initial();
	return 0;
}

static int irq_count = 0;
/*
static int irq_count_for_cable_plugin = 0;
static int irq_count_for_cable_plugout = 0;
*/

irqreturn_t anx7805_cbl_det_isr(int irq, void *data)
{
	irq_count++;
	/*
	if (irq_count < 2) {
		pr_err("anx7805_cbl_det_isr, irq_count: %d\n", irq_count);
		return IRQ_HANDLED;
	}
	*/

	if (gpio_get_value(mhl_eint_gpio_number)) {
		pr_err("slimport detect cable insertion\n");
		Mask_Slimport_Intr(true);
		irq_set_irq_type(mhl_eint_number,IRQ_TYPE_LEVEL_LOW);
		
		atomic_set(&mhl_irq_event, 0x1);
		wake_up_interruptible(&mhl_irq_wq);
	} 
	else {
		pr_err("slimport detect cable removal\n");
		Mask_Slimport_Intr(true);
		irq_set_irq_type(mhl_eint_number,IRQ_TYPE_LEVEL_HIGH);
		
		atomic_set(&mhl_irq_event, 0x10);
		wake_up_interruptible(&mhl_irq_wq);
	}

	return IRQ_HANDLED;
}

static void anx7805_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7805_data *td = container_of(work, struct anx7805_data,
	                                       work.work);
	/*pr_err(" cable- GPIO-%d\n", gpio_get_value(mhl_eint_gpio_number));*/
	SP_CTRL_Main_Procss();
	queue_delayed_work(td->workqueue, &td->work,
	                   msecs_to_jiffies(100));
#endif
}

static int anx7805_irq_kthread(void *data)
{
    int status = 0;
    int cable_status = 0;

	struct sched_param param = { .sched_priority = 94 }; /*RTPM_PRIO_SCRN_UPDATE*/
	sched_setscheduler(current, SCHED_RR, &param);

    for( ;; ) {
		wait_event_interruptible(mhl_irq_wq, atomic_read(&mhl_irq_event));
		cable_status = atomic_read(&mhl_irq_event);
		atomic_set(&mhl_irq_event, 0);

		if (cable_status == 0x01) {
			pr_err("cable plug-in, and create context\n");
			slimport_edid_p = si_edid_create_context(NULL, NULL);

			wake_lock(&the_chip->slimport_lock);
			queue_delayed_work(the_chip->workqueue, &the_chip->work, 0);
		} else {
			pr_err("cable plug-out, and destory context\n");
			si_edid_destroy_context(slimport_edid_p);
			slimport_edid_p = NULL;
			Notify_AP_MHL_TX_Event(SLIMPORT_TX_EVENT_HPD_CLEAR, 0, NULL);

			status = cancel_delayed_work_sync(&the_chip->work);
			pr_err("true: %d\n", status);
			if (status == true)
				flush_workqueue(the_chip->workqueue);
			else
				pr_err("cancel_delayed_work_sync error!\n");
			wake_unlock(&the_chip->slimport_lock);
			wake_lock_timeout(&the_chip->slimport_lock, 2*HZ);
			if (sp_tx_pd_mode != 1)
				system_power_ctrl(0);
		}

		Unmask_Slimport_Intr();
        if (kthread_should_stop())
            break;
    }

    return 0;
}

/*
static int anx7805_parse_dt(struct device_node *node,
                            struct anx7805_data *anx7805)
{
	int ret = 0;
//struct platform_device *hdmi_pdev = NULL;
	struct device_node *hdmi_tx_node = NULL;

	anx7805->gpio_p_dwn =
	    of_get_named_gpio(node, "analogix,p-dwn-gpio", 0);
	if (anx7805->gpio_p_dwn < 0) {
		pr_err("failed to get analogix,p-dwn-gpio.\n");
		ret = anx7805->gpio_p_dwn;
		goto out;
	}

	anx7805->gpio_reset =
	    of_get_named_gpio(node, "analogix,reset-gpio", 0);
	if (anx7805->gpio_reset < 0) {
		pr_err("failed to get analogix,reset-gpio.\n");
		ret = anx7805->gpio_reset;
		goto out;
	}

	anx7805->gpio_int =
	    of_get_named_gpio(node, "analogix,irq-gpio", 0);
	if (anx7805->gpio_int < 0) {
		pr_err("failed to get analogix,irq-gpio.\n");
		ret = anx7805->gpio_int;
		goto out;
	}

	anx7805->gpio_cbl_det =
	    of_get_named_gpio(node, "analogix,cbl-det-gpio", 0);
	if (anx7805->gpio_cbl_det < 0) {
		pr_err("failed to get analogix,cbl-det-gpio.\n");
		ret = anx7805->gpio_cbl_det;
		goto out;
	}

	ret = of_property_read_string(node, "analogix,vdd10-name",
	                              &anx7805->vdd10_name);
	if (ret) {
		pr_err("failed to get vdd10-name.\n");
		goto out;
	}

	ret = of_property_read_string(node, "analogix,avdd33-name",
	                              &anx7805->avdd33_name);
	if (ret) {
		pr_err("failed to get avdd33-name.\n");
		goto out;
	}

	hdmi_tx_node = of_parse_phandle(node, "analogix,hdmi-tx-map", 0);
	if (!hdmi_tx_node) {
		pr_err("can't find hdmi phandle\n");
		ret = -EINVAL;
		goto out;
	}
out:
	return ret;
}
*/

static unsigned int i2c_probe_count = 0;
static int anx7805_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id)
{
	struct anx7805_data *anx7805 = NULL;
	/*
	struct anx7805_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	*/
	//struct msm_hdmi_sp_ops *hdmi_sp_ops = NULL;
	int ret = 0;

	if (i2c_probe_count >= 1) {
		pr_err("i2c_probe_count:%d\n", i2c_probe_count);
		return 0;
	}
	i2c_probe_count++;

	pr_err("anx7805 anx7805_i2c_probe + \n");
	if (!i2c_check_functionality(client->adapter,
	                             I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("i2c bus does not support anx7805\n");
		ret = -ENODEV;
		goto exit;
	}

	anx7805 = kzalloc(sizeof(struct anx7805_data), GFP_KERNEL);
	if (!anx7805) {
		pr_err("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}

	anx7805->client = client;
	i2c_set_clientdata(client, anx7805);

#ifdef dp_to_do

	if (dev_node) {
		ret = anx7805_parse_dt(dev_node, anx7805);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto err0;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("no platform data.\n");
			goto err0;
		}

		anx7805->gpio_p_dwn = pdata->gpio_p_dwn;
		anx7805->gpio_reset = pdata->gpio_reset;
		anx7805->gpio_int = pdata->gpio_int;
		anx7805->gpio_cbl_det = pdata->gpio_cbl_det;
		anx7805->vdd10_name = pdata->vdd10_name;
		anx7805->avdd33_name = pdata->avdd33_name;		
	}
#endif
	/* initialize hdmi_sp_ops */
	/*
	hdmi_sp_ops = devm_kzalloc(&client->dev,
	                           sizeof(struct msm_hdmi_sp_ops),
	                           GFP_KERNEL);
	if (!hdmi_sp_ops) {
		pr_err("alloc hdmi sp ops failed\n");
		goto err0;
	}
	
	if (anx7805->hdmi_pdev) {
	ret = msm_hdmi_register_sp(anx7805->hdmi_pdev,
	hdmi_sp_ops);
	if (ret) {
	pr_err("register with hdmi_failed\n");
	goto err0;
	}
	}
	
	anx7805->hdmi_sp_ops = hdmi_sp_ops;
*/
	the_chip = anx7805;

	mutex_init(&anx7805->lock);
	init_completion(&init_aux_ch_completion);
	ret = anx7805_init_gpio(anx7805);
	if (ret) {
		pr_err("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7805->work, anx7805_work_func);

	anx7805->workqueue = create_singlethread_workqueue("anx7805_work");
	if (!anx7805->workqueue) {
		pr_err("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}

	ret = anx7805_avdd_3p3_power(anx7805, true);
	if (ret)
		goto err2;

	ret = anx7805_vdd_1p0_power(anx7805, false);
	if (ret)
		goto err3;

	ret = anx7805_system_init();
	if (ret) {
		pr_err("failed to initialize anx7805\n");
		goto err4;
	}

#ifdef dp_to_do
	client->irq = gpio_to_irq(anx7805->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("failed to get gpio irq\n");
		goto err4;
	}

	wake_lock_init(&anx7805->slimport_lock, WAKE_LOCK_SUSPEND,
	               "slimport_wake_lock");

	ret = request_threaded_irq(client->irq, NULL, anx7805_cbl_det_isr,
	                           IRQF_TRIGGER_RISING
	                           | IRQF_TRIGGER_FALLING
	                           | IRQF_ONESHOT,
	                           "anx7805", anx7805);
#else
	wake_lock_init(&anx7805->slimport_lock, WAKE_LOCK_SUSPEND,
					   "slimport_wake_lock");

	init_waitqueue_head(&mhl_irq_wq);	
	mhl_irq_task = kthread_create(anx7805_irq_kthread, NULL, "anx7805_irq_kthread"); 
	wake_up_process(mhl_irq_task);

#ifdef CONFIG_MTK_LEGACY
	mt_eint_set_sens(CUST_EINT_MHL_NUM, MT_LEVEL_SENSITIVE);   
	mt_eint_set_hw_debounce(CUST_EINT_MHL_NUM, 20);
	mt_eint_registration(CUST_EINT_MHL_NUM, EINTF_TRIGGER_HIGH, &anx7805_cbl_det_isr, 0);
	mt_eint_set_polarity(CUST_EINT_MHL_NUM, MT_EINT_POL_POS);
#else
	register_slimport_eint();
#endif
#endif	 

	if (ret < 0) {
		pr_err("failed to request irq\n");
		goto err5;
	}

#ifdef dp_to_do
	ret = enable_irq_wake(client->irq);
	if (ret < 0) {
		pr_err("interrupt wake enable fail\n");
		goto err6;
	}
#endif
	goto exit;

#ifdef dp_to_do
err6:
	free_irq(client->irq, anx7805);
#endif

err5:
	wake_lock_destroy(&anx7805->slimport_lock);
err4:
	if (!anx7805->vdd_reg)
		regulator_put(anx7805->vdd_reg);
err3:
	if (!anx7805->avdd_reg)
		regulator_put(anx7805->avdd_reg);
err2:
	destroy_workqueue(anx7805->workqueue);
err1:
	anx7805_free_gpio(anx7805);
err0:
	pr_err("anx7805, error0\n");
	the_chip = NULL;
	kfree(anx7805);
exit:
	/*
	pr_err("enable_irq, mhl_eint_number:%d\n", mhl_eint_number);
	if (mhl_eint_number != 0xffff)
		Unmask_Slimport_Intr();
	*/
	pr_err("anx7805 anx7805_i2c_probe - \n");
	return ret;
}

static int anx7805_i2c_remove(struct i2c_client *client)
{
	struct anx7805_data *anx7805 = i2c_get_clientdata(client);

	pr_err("anx7805_i2c_remove\n");
	///free_irq(client->irq, anx7805);
	wake_lock_destroy(&anx7805->slimport_lock);
	if (!anx7805->vdd_reg)
		regulator_put(anx7805->vdd_reg);
	if (!anx7805->avdd_reg)
		regulator_put(anx7805->avdd_reg);
	destroy_workqueue(anx7805->workqueue);
	anx7805_free_gpio(anx7805);
	the_chip = NULL;
	kfree(anx7805);
	return 0;
}

static const struct i2c_device_id anx7805_id[] = {
	{ "anx7805", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, anx7805_id);

static struct of_device_id anx_match_table[] = {
	{ .compatible = "mediatek,ext_disp",},
	{ },
};

static struct i2c_driver anx7805_driver = {
	.driver = {
		.name = "anx7805",
		.owner = THIS_MODULE,
		.of_match_table = anx_match_table,
	},
	.probe = anx7805_i2c_probe,
	.remove = anx7805_i2c_remove,
	.id_table = anx7805_id,
};

static struct i2c_board_info __initdata anx7805_i2c_client = { 
	.type = "anx7805",
	.addr = 0x39,
	.irq = 8,
};

int dp_i2c_mutex_init(struct mutex *m)
{
	mutex_init(m);
	return 0;
}

static void __init anx7805_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	dp_i2c_mutex_init(&dp_i2c_lock);
	slimport_platform_init();
	i2c_register_board_info(2, &anx7805_i2c_client, 1); 

	ret = i2c_add_driver(&anx7805_driver);
	if (ret)
		pr_err("%s: failed to register anx7805 driver\n", __func__);
	/*
	mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_OUT_ZERO);
	*/
	
}

int __init anx7805_init(void)
{	
	async_schedule(anx7805_init_async, NULL);
	return 0;
}

int slimport_anx7805_init(void)
{
	pr_warn("%s: \n", __func__);
	anx7805_init();
	return 0;
}

static void __exit anx7805_exit(void)
{
	i2c_del_driver(&anx7805_driver);
}

///module_init(anx7805_init);
///module_exit(anx7805_exit);

///MODULE_DESCRIPTION("Slimport transmitter ANX7805 driver");
///MODULE_AUTHOR("swang@analogixsemi.com");
///MODULE_LICENSE("GPL");
///MODULE_VERSION("1.1");
