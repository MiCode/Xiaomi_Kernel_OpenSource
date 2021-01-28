// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014 Goodix Technology.
 */

#include "gt1x_tpd_common.h"
#if TPD_SUPPORT_I2C_DMA
#include <linux/dma-mapping.h>
#endif

#ifdef CONFIG_GTP_ICS_SLOT_REPORT
#include <linux/input/mt.h>
#endif

#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <linux/suspend.h>

/*1 enable,0 disable,touch_panel_eint default status,
 * need to confirm after register eint
 */
int irq_flag = 1;
static spinlock_t irq_flag_lock;
/*0 power off,default, 1 power on*/
static int power_flag;
static int tpd_flag;
static int tpd_pm_flag;
static int tpd_tui_flag;
static int tpd_tui_low_power_skipped;
DEFINE_MUTEX(tui_lock);
int tpd_halt;
static int tpd_eint_mode = 1;
static struct task_struct *thread;
static struct task_struct *update_thread;
static struct task_struct *probe_thread;
static struct notifier_block pm_notifier_block;
struct pinctrl *pinctrl1;
struct pinctrl_state *pins_default;
struct pinctrl_state *eint_as_int, *eint_output0,
		*eint_output1, *rst_output0,
		*pin_i2c_mode, *rst_output1;

static int tpd_polling_time = 50;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(pm_waiter);
static bool gtp_suspend;
DECLARE_WAIT_QUEUE_HEAD(init_waiter);
DEFINE_MUTEX(i2c_access);
unsigned int touch_irq;
u8 int_type;

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static int tpd_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client,
				struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);

static irqreturn_t tpd_eint_interrupt_handler(unsigned int irq,
							struct irq_desc *desc);

#define GTP_DRIVER_NAME  "gt1151"
static const struct i2c_device_id tpd_i2c_id[] = { {GTP_DRIVER_NAME, 0}, {} };
static unsigned short force[] = {
	0, GTP_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

static const struct of_device_id tpd_of_match[] = {
	{.compatible = "goodix,gt1151"},
	{},
};
static struct i2c_driver tpd_i2c_driver = {
	.probe = tpd_i2c_probe,
	.remove = tpd_i2c_remove,
	.detect = tpd_i2c_detect,
	.driver.name = GTP_DRIVER_NAME,
	.driver = {
		   .name = GTP_DRIVER_NAME,
		   .of_match_table = tpd_of_match,
		   },
	.id_table = tpd_i2c_id,
	.address_list = (const unsigned short *)forces,
};

#if TPD_SUPPORT_I2C_DMA
static u8 *gpDMABuf_va;
static dma_addr_t gpDMABuf_pa;
struct mutex dma_mutex;
DEFINE_MUTEX(dma_mutex);

static s32 i2c_dma_write_mtk(u16 addr, u8 *buffer, s32 len)
{
	s32 ret = 0;
	s32 pos = 0;
	s32 transfer_length;
	u16 address = addr;

	struct i2c_msg msg = {
		.flags = !I2C_M_RD,
		.ext_flag = (gt1x_i2c_client->ext_flag |
					I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		.addr = (gt1x_i2c_client->addr & I2C_MASK_FLAG),
		.timing = I2C_MASTER_CLOCK,
		.buf = (u8 *)(uintptr_t)gpDMABuf_pa,
	};

	mutex_lock(&dma_mutex);
	while (pos != len) {
		if (len - pos > (IIC_DMA_MAX_TRANSFER_SIZE - GTP_ADDR_LENGTH))
			transfer_length =
				IIC_DMA_MAX_TRANSFER_SIZE - GTP_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		gpDMABuf_va[0] = (address >> 8) & 0xFF;
		gpDMABuf_va[1] = address & 0xFF;
		memcpy(&gpDMABuf_va[GTP_ADDR_LENGTH], &buffer[pos],
					transfer_length);

		msg.len = transfer_length + GTP_ADDR_LENGTH;
		if (!gtp_suspend) {/*workround log too much*/
			ret = i2c_transfer(gt1x_i2c_client->adapter, &msg, 1);
			if (ret != 1) {
				GTP_INFO("I2c Transfer error! (%d)", ret);
				ret = ERROR_IIC;
				break;
			}
		} else {
			ret = ERROR_IIC;
			break;
		}
		ret = 0;
		pos += transfer_length;
		address += transfer_length;
	}
	mutex_unlock(&dma_mutex);
	return ret;
}

static s32 i2c_dma_read_mtk(u16 addr, u8 *buffer, s32 len)
{
	s32 ret = ERROR;
	s32 pos = 0;
	s32 transfer_length;
	u16 address = addr;
	u8 addr_buf[GTP_ADDR_LENGTH] = { 0 };

	struct i2c_msg msgs[2] = {
		{
		 .flags = 0,	/*!I2C_M_RD,*/
		 .addr = (gt1x_i2c_client->addr & I2C_MASK_FLAG),
		 .timing = I2C_MASTER_CLOCK,
		 .len = GTP_ADDR_LENGTH,
		 .buf = addr_buf,
		 },
		{
		 .flags = I2C_M_RD,
		 .ext_flag = (gt1x_i2c_client->ext_flag |
			I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .addr = (gt1x_i2c_client->addr & I2C_MASK_FLAG),
		 .timing = I2C_MASTER_CLOCK,
		 .buf = (u8 *)(uintptr_t)gpDMABuf_pa,
		},
	};
	mutex_lock(&dma_mutex);
	while (pos != len) {
		if (len - pos > IIC_DMA_MAX_TRANSFER_SIZE)
			transfer_length = IIC_DMA_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;

		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;

		ret = i2c_transfer(gt1x_i2c_client->adapter, msgs, 2);
		if (ret != 2) {
			GTP_ERROR("I2C Transfer error! (%d)", ret);
			ret = ERROR_IIC;
			break;
		}
		ret = 0;
		memcpy(&buffer[pos], gpDMABuf_va, transfer_length);
		pos += transfer_length;
		address += transfer_length;
	};
	mutex_unlock(&dma_mutex);
	return ret;
}

#else

static s32 i2c_write_mtk(u16 addr, u8 *buffer, s32 len)
{
	s32 ret;

	struct i2c_msg msg = {
		.flags = 0,
#ifdef CONFIG_MTK_I2C_EXTENSION
		.addr = (gt1x_i2c_client->addr & I2C_MASK_FLAG) |
				(I2C_ENEXT_FLAG),
		.timing = I2C_MASTER_CLOCK,
#else
		.addr = gt1x_i2c_client->addr,
#endif
	};

	ret = _do_i2c_write(&msg, addr, buffer, len);
	return ret;
}

static s32 i2c_read_mtk(u16 addr, u8 *buffer, s32 len)
{
	int ret;
	u8 addr_buf[GTP_ADDR_LENGTH] = { (addr >> 8) & 0xFF, addr & 0xFF };

	struct i2c_msg msgs[2] = {
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
		 .addr = ((gt1x_i2c_client->addr & I2C_MASK_FLAG) |
			(I2C_ENEXT_FLAG)),
		 .timing = I2C_MASTER_CLOCK,
#else
		 .addr = gt1x_i2c_client->addr,
#endif
		 .flags = 0,
		 .buf = addr_buf,
		 .len = GTP_ADDR_LENGTH,
		},
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
		 .addr = ((gt1x_i2c_client->addr & I2C_MASK_FLAG) |
			(I2C_ENEXT_FLAG)),
		 .timing = I2C_MASTER_CLOCK,
#else
		 .addr = gt1x_i2c_client->addr,
#endif
		 .flags = I2C_M_RD,
		},
	};

	ret = _do_i2c_read(msgs, addr, buffer, len);
	return ret;
}
#endif/* TPD_SUPPORT_I2C_DMA */

/**
 * @return: return 0 if success, otherwise return a negative number
 *          which contains the error code.
 */
s32 gt1x_i2c_read(u16 addr, u8 *buffer, s32 len)
{
#if TPD_SUPPORT_I2C_DMA
	return i2c_dma_read_mtk(addr, buffer, len);
#else
	return i2c_read_mtk(addr, buffer, len);
#endif
}

/**
 * @return: return 0 if success, otherwise return a negative number
 *          which contains the error code.
 */
s32 gt1x_i2c_write(u16 addr, u8 *buffer, s32 len)
{
#if TPD_SUPPORT_I2C_DMA
	return i2c_dma_write_mtk(addr, buffer, len);
#else
	return i2c_write_mtk(addr, buffer, len);
#endif
}

#ifdef TPD_REFRESH_RATE
/*******************************************************
 * Function:
 *   Write refresh rate
 *
 * Input:
 *   rate: refresh rate N (Duration=5+N ms, N=0~15)
 *
 * Output:
 *   Executive outcomes.0---succeed.
 *******************************************************/
static u8 gt1x_set_refresh_rate(u8 rate)
{
	u8 buf[1] = { rate };

	if (rate > 0xf) {
		GTP_ERROR("Refresh rate is over range (%d)", rate);
		return ERROR_VALUE;
	}

	GTP_INFO("Refresh rate change to %d", rate);
	return gt1x_i2c_write(GTP_REG_REFRESH_RATE, buf, sizeof(buf));
}

/*******************************************************
 * Function:
 *    Get refresh rate
 *
 * Output:
 *    Refresh rate or error code
 *******************************************************/
static u8 gt1x_get_refresh_rate(void)
{
	int ret;
	u8 buf[1] = { 0x00 };

	ret = gt1x_i2c_read(GTP_REG_REFRESH_RATE, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	GTP_INFO("Refresh rate is %d", buf[0]);
	return buf[0];
}

/*=============================================================*/
static ssize_t tpd_refresh_rate_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret = gt1x_get_refresh_rate();

	if (ret < 0)
		return 0;
	else
		return sprintf(buf, "%d\n", ret);
}

static ssize_t tpd_refresh_rate_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long rate;
	int ret;

	ret = kstrtoul(buf, 0, &rate);
	gt1x_set_refresh_rate(rate);
	return size;
}

static DEVICE_ATTR_RW(tpd_refresh_rate);

static struct device_attribute *gt9xx_attrs[] = {
	&dev_attr_tpd_refresh_rate,
};
#endif
/*=============================================================*/

static int tpd_i2c_detect(struct i2c_client *client,
				struct i2c_board_info *info)
{
	strncpy(info->type, "mtk-tpd", sizeof(info->type));
	return 0;
}

static DEFINE_MUTEX(tpd_set_gpio_mutex);
void tpd_gpio_as_int(int pin)
{
	mutex_lock(&tpd_set_gpio_mutex);
	GTP_INFO("[tpd] %s\n", __func__);
	if (pin == 1)
		pinctrl_select_state(pinctrl1, eint_as_int);
	mutex_unlock(&tpd_set_gpio_mutex);
}

void tpd_gpio_output(int pin, int level)
{
	mutex_lock(&tpd_set_gpio_mutex);

	GTP_DEBUG("%s pin = EINT, level = %d\n", __func__, level);

	if (pin == 1) {
		if (level)
			pinctrl_select_state(pinctrl1, eint_output1);
		else
			pinctrl_select_state(pinctrl1, eint_output0);
	} else {
		if (level)
			pinctrl_select_state(pinctrl1, rst_output1);
		else
			pinctrl_select_state(pinctrl1, rst_output0);
	}
	mutex_unlock(&tpd_set_gpio_mutex);
}

int tpd_get_pinctrl(struct i2c_client *client)
{
	int ret;

	GTP_INFO("[tpd] mt_tpd_pinctrl+\n");
	pinctrl1 = devm_pinctrl_get(client->adapter->dev.parent);
	if (IS_ERR(pinctrl1)) {
		ret = PTR_ERR(pinctrl1);
		GTP_ERROR("Cannot find pinctrl1!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		GTP_INFO("Cannot find pinctrl default %d!\n", ret);
	}

	/* default i2c mode */
	pin_i2c_mode = pinctrl_lookup_state(pinctrl1, "ts_i2c_mode");
	if (IS_ERR(pin_i2c_mode)) {
		ret = PTR_ERR(pin_i2c_mode);
		GTP_ERROR("Failed to get pinctrl state:%s, ret:%d",
				"ts_i2c_mode", ret);
	}

	eint_as_int = pinctrl_lookup_state(pinctrl1, "ts_eint_as_int");
	if (IS_ERR(eint_as_int)) {
		ret = PTR_ERR(eint_as_int);
		GTP_ERROR("Cannot find pinctrl ts_eint_as_int!\n");
		return ret;
	}
	eint_output0 = pinctrl_lookup_state(pinctrl1, "ts_int_suspend");
	if (IS_ERR(eint_output0)) {
		ret = PTR_ERR(eint_output0);
		GTP_ERROR("Cannot find pinctrl ts_int_suspend!\n");
		return ret;
	}
	eint_output1 = pinctrl_lookup_state(pinctrl1, "ts_int_active");
	if (IS_ERR(eint_output1)) {
		ret = PTR_ERR(eint_output1);
		GTP_ERROR("Cannot find pinctrl ts_int_active!\n");
		return ret;
	}
	if (tpd_dts_data.tpd_use_ext_gpio == false) {
		rst_output0 =
			pinctrl_lookup_state(pinctrl1, "ts_reset_suspend");
		if (IS_ERR(rst_output0)) {
			ret = PTR_ERR(rst_output0);
			GTP_ERROR("Cannot find pinctrl ts_reset_suspend!\n");
			return ret;
		}
		rst_output1 =
			pinctrl_lookup_state(pinctrl1, "ts_reset_active");
		if (IS_ERR(rst_output1)) {
			ret = PTR_ERR(rst_output1);
			GTP_ERROR("Cannot find pinctrl ts_reset_active!\n");
			return ret;
		}
	}
	GTP_INFO("[tpd] mt_tpd_pinctrl-\n");
	return 0;
}

static int tpd_power_on(void)
{
	gt1x_power_switch(SWITCH_ON);

	gt1x_select_addr();
	msleep(20);

	if (gt1x_get_chip_type() != 0)
		return -1;

	if (gt1x_reset_guitar() != 0)
		return -1;

	return 0;
}

void gt1x_irq_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_flag_lock, flags);

	if (irq_flag == 0) {
		irq_flag = 1;
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		enable_irq(touch_irq);
		GTP_DEBUG("%s, irq_flag=%d", __func__, irq_flag);
	} else if (irq_flag == 1) {
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		GTP_INFO("Touch Eint already enabled!");
	} else {
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		GTP_ERROR("Invalid irq_flag %d!", irq_flag);
	}
	/*GTP_INFO("Enable irq_flag=%d",irq_flag);*/

}

void gt1x_irq_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_flag_lock, flags);

	if (irq_flag == 1) {
		irq_flag = 0;
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		disable_irq(touch_irq);
		GTP_DEBUG("%s, irq_flag=%d", __func__, irq_flag);
	} else if (irq_flag == 0) {
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		GTP_INFO("Touch Eint already disabled!");
	} else {
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		GTP_ERROR("Invalid irq_flag %d!", irq_flag);
	}
	/*GTP_INFO("Disable irq_flag=%d",irq_flag);*/
}

void gt1x_power_off(void)
{
	int ret = 0;

	if (power_flag == 1) {
		GTP_INFO("Power switch off!");
		/*disable regulator*/
		ret = regulator_disable(tpd->reg);
		if (ret)
			GTP_ERROR("regulator_disable() failed!\n");
		power_flag = 0;
	}
}

void gt1x_power_switch(s32 state)
{
	int ret = 0;

	GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
	GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
	msleep(20);

	switch (state) {
	case SWITCH_ON:
		if (power_flag == 0) {
			GTP_INFO("Power switch on!");
			/*enable regulator*/
			ret = regulator_enable(tpd->reg);
			if (ret)
				GTP_ERROR("regulator_enable() failed!\n");

			power_flag = 1;
		} else
			GTP_DEBUG("Power already is on!");
		break;
	case SWITCH_OFF:
		if (power_flag == 1) {
			GTP_INFO("Power switch off!");

			/*disable regulator*/
			ret = regulator_disable(tpd->reg);
			if (ret)
				GTP_ERROR("regulator_disable() failed!\n");

			power_flag = 0;
		} else
			GTP_INFO("Power already is off!");
		break;
	default:
		GTP_ERROR("Invalid power switch command!");
		break;
	}
}

int gt1x_is_tpd_halt(void)
{
	return tpd_halt;
}

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = { 0, 0 };

	GTP_INFO("Device Tree Tpd_irq_registration!");

	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		if (of_property_read_u32_array(node, "debounce",
						ints, ARRAY_SIZE(ints)) == 0) {
			GTP_INFO("debounce:%d-%d\n", ints[0], ints[1]);
			gpio_set_debounce(ints[0], ints[1]);
		} else
			GTP_INFO("debounce time not found\n");

		touch_irq = irq_of_parse_and_map(node, 0);
		GTP_INFO("Device gt1x_int_type = %d!", gt1x_int_type);
		if (!gt1x_int_type) {/*EINTF_TRIGGER*/
			ret = request_irq(touch_irq,
				(irq_handler_t) tpd_eint_interrupt_handler,
				IRQF_TRIGGER_RISING,
				"TOUCH_PANEL-eint", NULL);
			if (ret > 0) {
				ret = -1;
				GTP_ERROR("request_irq IRQ NOT AVAILABLE!.");
			}
		} else {
			ret = request_irq(touch_irq,
				(irq_handler_t) tpd_eint_interrupt_handler,
				IRQF_TRIGGER_FALLING,
				"TOUCH_PANEL-eint", NULL);
			if (ret > 0) {
				ret = -1;
				GTP_ERROR("request_irq IRQ NOT AVAILABLE!.");
			}
		}
	} else {
		GTP_ERROR("can not find touch eint device node!.");
		ret = -1;
	}
	GTP_INFO("[%s]irq:%d", __func__, touch_irq);
	return ret;
}

void gt1x_auto_update_done(void)
{
	tpd_pm_flag = 1;
	wake_up(&pm_waiter);
}

static int gt1x_ts_gpio_setup(void)
{
	int ret = 0;

	GTP_INFO("GPIO setup,reset-gpio:%d, irq-gpio:%d",
		tpd_dts_data.rst_gpio_num, tpd_dts_data.eint_gpio_num);

	if (gpio_is_valid(tpd_dts_data.rst_gpio_num)) {
		ret = gpio_request(tpd_dts_data.rst_gpio_num,
					"goodix,reset-gpio");
		if (ret < 0) {
			GTP_ERROR("Failed to request reset gpio, ret:%d", ret);
			goto err_request_reset_gpio;
		}
	}

	if (gpio_is_valid(tpd_dts_data.eint_gpio_num)) {
		ret = gpio_request(tpd_dts_data.eint_gpio_num,
					"goodix,eint-gpio");
		if (ret < 0) {
			GTP_ERROR("Failed to request irq gpio, ret:%d", ret);
			goto err_request_eint_gpio;
		}
	}
	return ret;

err_request_eint_gpio:
	gpio_free(tpd_dts_data.rst_gpio_num);
err_request_reset_gpio:
	return ret;
}

static int gt1x_ts_power_init(void)
{
	int ret = 0;

	GTP_INFO("Power init");
	/* dev:i2c client device or spi slave device*/

	tpd->reg = devm_regulator_get(tpd->tpd_dev, "vtouch");
	if (IS_ERR(tpd->reg)) {
		GTP_ERROR("regulator_get() failed!\n");
		tpd->reg = NULL;
		return PTR_ERR(tpd->reg);
	}

	/*set 2.8v*/
	ret = regulator_set_voltage(tpd->reg, 3000000, 3000000);
	if (ret)
		GTP_ERROR("regulator_set_voltage(%d) failed!\n", ret);

	return ret;
}

#ifdef GTP_AUTO_UPDATE
int gt1x_pm_notifier(struct notifier_block *nb, unsigned long val, void *ign)
{
	switch (val) {
	case PM_RESTORE_PREPARE:
		GTP_INFO("%s: PM_RESTORE_PREPARE enter\n", __func__);
		if (!IS_ERR(update_thread) && update_thread)
			wait_event(waiter, tpd_pm_flag == 1);
		GTP_INFO("%s: PM_RESTORE_PREPARE leave\n", __func__);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}
#endif

int tpd_reregister_from_tui(void)
{
	int ret = 0;

	free_irq(touch_irq, NULL);

	ret = tpd_irq_registration();
	if (ret < 0) {
		ret = -1;
	    GTP_INFO("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	}
	return ret;
}

static int tpd_registration(void *client)
{
	s32 err = 0;
	s32 idx = 0;
	gt1x_i2c_client = client;

	GTP_INFO(" start %s ++\n", __func__);

	tpd_get_pinctrl(client);
	err = pinctrl_select_state(pinctrl1, pin_i2c_mode);
	if (err < 0)
		GTP_ERROR("Failed to select default pinstate, err:%d", err);

	gt1x_power_switch(SWITCH_ON);
	/* select i2c address */
	gt1x_select_addr();

	if (gt1x_init()) {
		/* TP resolution == LCD resolution,
		 * no need to match resolution when initialized fail
		 */
		gt1x_abs_x_max = 0;
		gt1x_abs_y_max = 0;
		gt1x_power_off();
		wake_up(&init_waiter);
		GTP_INFO("gt1x_init failed!++");
		return -1;
	}

	thread = kthread_run(tpd_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_INFO(" failed to create kernel thread: %d\n", err);
	}
	if (tpd_dts_data.use_tpd_button) {
		for (idx = 0; idx < tpd_dts_data.tpd_key_num; idx++)
			input_set_capability(tpd->dev, EV_KEY,
					tpd_dts_data.tpd_key_local[idx]);
	}

#ifdef CONFIG_GTP_GESTURE_WAKEUP
	input_set_capability(tpd->dev, EV_KEY, KEY_GESTURE);
#endif

	/* EINT device tree, default EINT enable */
	tpd_irq_registration();

#ifdef CONFIG_GTP_ESD_PROTECT
	/*  must before auto update */
	gt1x_init_esd_protect();
	gt1x_esd_switch(SWITCH_ON);
#endif
	update_thread = kthread_run(gt1x_auto_update_proc,
					(void *)NULL, "gt1x_auto_update");
	if (IS_ERR(update_thread)) {
		err = PTR_ERR(update_thread);
		GTP_INFO(" failed to create auto-update thread: %d\n", err);
	}
	pm_notifier_block.notifier_call = gt1x_pm_notifier;
	pm_notifier_block.priority = 0;
	register_pm_notifier(&pm_notifier_block);
#ifdef CONFIG_MTK_LENS
	AF_PowerDown();
#endif
	return 0;
}

static s32 tpd_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int err = 0;
	/*int count = 0;*/

	GTP_INFO("%s enter.", __func__);
#ifdef CONFIG_MTK_BOOT
	if (get_boot_mode() == RECOVERY_BOOT)
		return 0;
#endif

	probe_thread = kthread_run(tpd_registration,
					(void *)client, "tpd_probe");
	if (IS_ERR(probe_thread)) {
		err = PTR_ERR(probe_thread);
		GTP_INFO(" failed to create probe thread: %d\n", err);
		return err;
	}

	GTP_INFO("%s start.wait_event_interruptible", __func__);
	wait_event_timeout(init_waiter,
					check_flag == true, 5 * HZ);
	GTP_INFO("%s end.wait_event_interruptible", __func__);
	return 0;
}

static irqreturn_t tpd_eint_interrupt_handler(unsigned int irq,
							struct irq_desc *desc)
{
	unsigned long flags;

	TPD_DEBUG_PRINT_INT;
	tpd_flag = 1;
	spin_lock_irqsave(&irq_flag_lock, flags);
	if (irq_flag == 0) {
		spin_unlock_irqrestore(&irq_flag_lock, flags);
		return IRQ_HANDLED;
	}
	/* enter EINT handler disable INT, make sure INT is disable when
	 * handle touch event including top/bottom half
	 * use _nosync to avoid deadlock
	 */
	irq_flag = 0;
	spin_unlock_irqrestore(&irq_flag_lock, flags);
	disable_irq_nosync(touch_irq);
	GTP_DEBUG("eint disable irq_flat=%d", irq_flag);
	/*GTP_INFO("disable irq_flag=%d",irq_flag);*/
	wake_up(&waiter);
	return IRQ_HANDLED;
}
static int tpd_history_x, tpd_history_y;
void gt1x_touch_down(s32 x, s32 y, s32 size, s32 id)
{
#ifdef CONFIG_GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif
#ifndef CONFIG_GTP_ICS_SLOT_REPORT
#ifdef CONFIG_CUSTOM_LCM_X
	unsigned long lcm_x = 0, lcm_y = 0;
	int ret;
#endif
#endif
	input_report_key(tpd->dev, BTN_TOUCH, 1);
#ifdef CONFIG_GTP_ICS_SLOT_REPORT
	input_mt_slot(tpd->dev, id);
	input_report_abs(tpd->dev, ABS_MT_PRESSURE, size);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
#else
	if ((!size) && (!id)) {
		/* for virtual button */
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, 100);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
	} else {
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, size);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
	}
#ifdef CONFIG_CUSTOM_LCM_X
	ret = kstrtoul(CONFIG_CUSTOM_LCM_X, 0, &lcm_x);
	if (ret)
		GTP_ERROR("Touch down get lcm_x failed");
	ret = kstrtoul(CONFIG_CUSTOM_LCM_Y, 0, &lcm_y);
	if (ret)
		GTP_ERROR("Touch down get lcm_y failed");

	if (x < lcm_x)
		x = 0;
	else
		x = x - lcm_x;
	if (y < lcm_y)
		y = 0;
	else
		y = y - lcm_y;

	GTP_DEBUG("x:%d, y:%d, lcm_x:%lu, lcm_y:%lu", x, y, lcm_x, lcm_y);

#endif
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(tpd->dev);
#endif
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(x, y, x, y, id, 1);
	tpd_history_x = x;
	tpd_history_y = y;
#ifdef CONFIG_MTK_BOOT
	if (tpd_dts_data.use_tpd_button) {
		if (get_boot_mode() == FACTORY_BOOT ||
			get_boot_mode() == RECOVERY_BOOT)
			tpd_button(x, y, 1);
	}
#endif
}

void gt1x_touch_up(s32 id)
{
#ifdef CONFIG_GTP_ICS_SLOT_REPORT
	input_mt_slot(tpd->dev, id);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);
#else
	input_mt_sync(tpd->dev);
#endif
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(tpd_history_x, tpd_history_y,
		tpd_history_x, tpd_history_y, id, 0);
	tpd_history_x = 0;
	tpd_history_y = 0;
#ifdef CONFIG_MTK_BOOT
	if (tpd_dts_data.use_tpd_button) {
		if (get_boot_mode() == FACTORY_BOOT ||
			get_boot_mode() == RECOVERY_BOOT)
			tpd_button(0, 0, 0);
	}
#endif
}

#ifdef CONFIG_GTP_CHARGER_SWITCH
u32 gt1x_get_charger_status(void)
{
	u32 chr_status = 0;
#ifdef MT6573
	chr_status = *(u32 *)CHR_CON0;
	chr_status &= (1 << 13);
#else
	/* ( defined(MT6575) || defined(MT6577) || defined(MT6589) ) */
	chr_status = upmu_is_chr_det();
#endif
	return chr_status;
}
#endif

static int tpd_event_handler(void *unused)
{
	u8 finger = 0;
	u8 end_cmd = 0;
	s32 ret = 0;
	u8 point_data[11] = { 0 };
	struct sched_param param = {.sched_priority = 4};

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		if (tpd_eint_mode) {
			wait_event(waiter, tpd_flag != 0);
			tpd_flag = 0;
		} else {
			GTP_DEBUG("Polling coordinate mode!");
			msleep(tpd_polling_time);
		}

		set_current_state(TASK_RUNNING);
		mutex_lock(&i2c_access);
		/* don't reset before "if (tpd_halt..."  */

#ifdef CONFIG_GTP_GESTURE_WAKEUP
		ret = gesture_event_handler(tpd->dev);
		if (ret >= 0) {
			gt1x_irq_enable();
			mutex_unlock(&i2c_access);
			continue;
		}
#endif
		if (tpd_halt) {
			mutex_unlock(&i2c_access);
			GTP_DEBUG("return for interrupt after suspend...  ");
			continue;
		}

		/* read coordinates */
		ret = gt1x_i2c_read(GTP_READ_COOR_ADDR,
					point_data, sizeof(point_data));
		if (ret < 0) {
			GTP_ERROR("I2C transfer error!");
#ifndef CONFIG_GTP_ESD_PROTECT
			gt1x_power_reset();
#endif
			goto exit_work_func;
		}
		finger = point_data[0];

		/* response to a ic request */
		if (finger == 0x00)
			gt1x_request_event_handler();

		if ((finger & 0x80) == 0) {
#ifdef CONFIG_HOTKNOT_BLOCK_RW
			if (!hotknot_paired_flag) {
#endif
				gt1x_irq_enable();
				mutex_unlock(&i2c_access);
				continue;
			}
#ifdef CONFIG_HOTKNOT_BLOCK_RW
		}
		ret = hotknot_event_handler(point_data);
		if (!ret)
			goto exit_work_func;
#endif

#ifdef CONFIG_GTP_PROXIMITY
		ret = gt1x_prox_event_handler(point_data);
		if (ret > 0)
			goto exit_work_func;
#endif

#ifdef CONFIG_GTP_WITH_STYLUS
		ret = gt1x_touch_event_handler(point_data, tpd->dev, pen_dev);
#else
		ret = gt1x_touch_event_handler(point_data, tpd->dev, NULL);
#endif
		if (ret) {
			gt1x_irq_enable();
			mutex_unlock(&i2c_access);
			continue;
		}

 exit_work_func:

		if (!gt1x_rawdiff_mode) {
			ret = gt1x_i2c_write(GTP_READ_COOR_ADDR, &end_cmd, 1);
			if (ret < 0)
				GTP_INFO("I2C write end_cmd  error!");
		}
		gt1x_irq_enable();
		mutex_unlock(&i2c_access);

	} while (!kthread_should_stop());

	return 0;
}

int gt1x_debug_proc(u8 *buf, int count)
{
	char mode_str[50] = { 0 };
	int mode;
	int ret;

	ret = sscanf(buf, "%49s %d", (char *)&mode_str, &mode);
	if (ret < 0) {
		GTP_ERROR("%s sscanf failed", __func__);
		return ret;
	}
	/***********POLLING/EINT MODE switch****************/
	if (strcmp(mode_str, "polling") == 0) {
		if (mode >= 10 && mode <= 200) {
			GTP_INFO("Switch to polling mode, polling time is %d",
					mode);
			tpd_eint_mode = 0;
			tpd_polling_time = mode;
			tpd_flag = 1;
			wake_up(&waiter);
		} else {
			/* please set between 10~200ms */
			GTP_INFO("Wrong polling time\n");
		}
		return count;
	}
	if (strcmp(mode_str, "eint") == 0) {
		GTP_INFO("Switch to eint mode");
		tpd_eint_mode = 1;
		return count;
	}
	/**********************************************/
	if (strcmp(mode_str, "switch") == 0) {
		if (mode == 0)	/*turn off*/
			tpd_off();
		else if (mode == 1)	/*turn on*/
			tpd_on();
		else
			GTP_ERROR("error mode :%d", mode);
		return count;
	} else if (strcmp(mode_str, "enable_irq") == 0) {
		if (mode == 0) {
			GTP_ERROR("enable_irq 0, touch_irq = %d, irq_flag = %d",
				(int)touch_irq, irq_flag);
			disable_irq(touch_irq);
		} else if (mode == 1) {
			GTP_ERROR("enable_irq 1, touch_irq = %d, irq_flag = %d",
				(int)touch_irq, irq_flag);
			enable_irq(touch_irq);
		} else
			GTP_ERROR("error mode :%d", mode);
	} else if (strcmp(mode_str, "rerequest_irq") == 0) {
		int ret;

		GTP_ERROR("rerequest_irq, touch_irq = %d, irq_flag = %d",
			(int)touch_irq, irq_flag);
		free_irq(touch_irq, NULL);
		ret = tpd_irq_registration();
		if (ret < 0)
			GTP_ERROR("rerequest_irq fail, %d!", ret);
	} else if (strcmp(mode_str, "eint_dump_status") == 0) {
		GTP_ERROR("eint_dump_status, %u", touch_irq);
		/*mt_eint_dump_status(1);*/
	}

	return -1;
}

static u16 convert_productname(u8 *name)
{
	int i;
	u16 product = 0;

	for (i = 0; i < 4; i++) {
		product <<= 4;
		if (name[i] < '0' || name[i] > '9')
			product += '*';
		else
			product += name[i] - '0';
	}
	return product;
}

static int tpd_i2c_remove(struct i2c_client *client)
{
	gt1x_deinit();

	return 0;
}

static int tpd_local_init(void)
{
	int ret;

	ret = gt1x_ts_gpio_setup();
	if (ret < 0)
		goto gpio_err;

	ret = gt1x_ts_power_init();
	if (ret < 0)
		goto power_init_err;

#if TPD_SUPPORT_I2C_DMA
		tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		gpDMABuf_va = (u8 *) dma_alloc_coherent(&tpd->dev->dev,
			IIC_DMA_MAX_TRANSFER_SIZE,
			&gpDMABuf_pa, GFP_KERNEL);
		if (!gpDMABuf_va) {
			GTP_ERROR("Allocate DMA I2C Buffer failed!");
			goto regulator_out;
		}
		memset(gpDMABuf_va, 0, IIC_DMA_MAX_TRANSFER_SIZE);
#endif

	spin_lock_init(&irq_flag_lock);

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		GTP_ERROR("unable to add i2c driver.");
		goto regulator_out;
	}

	/*disable auto load touch driver for linux3.0 porting*/
	if (tpd_load_status == 0) {
		GTP_ERROR("add error touch panel driver.");
		goto touch_probe_err;
	}
#ifdef CONFIG_GTP_ICS_SLOT_REPORT
	input_mt_init_slots(tpd->dev, 10, 0);
#endif
	if (!tpd_dts_data.touch_max_num)
		tpd_dts_data.touch_max_num = DEFAULT_MAX_TOUCH_NUM;
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0,
				 (tpd_dts_data.touch_max_num - 1), 0, 0);
	if (tpd_dts_data.use_tpd_button) {
		/*initialize tpd button data*/
		tpd_button_setting(tpd_dts_data.tpd_key_num,
					tpd_dts_data.tpd_key_local,
					tpd_dts_data.tpd_key_dim_local);
	}

	/*set vendor string*/
	tpd->dev->id.vendor = 0x00;
	tpd->dev->id.product = convert_productname(gt1x_version.product_id);
	tpd->dev->id.version = (gt1x_version.patch_id >> 8);

	GTP_INFO("end %s --, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;
	return 0;

touch_probe_err:
	i2c_del_driver(&tpd_i2c_driver);
regulator_out:
	regulator_put(tpd->reg);
	gpio_free(tpd_dts_data.rst_gpio_num);
	gpio_free(tpd_dts_data.eint_gpio_num);
gpio_err:
power_init_err:
	return ret;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct device *h)
{
	s32 ret = -1;
#if defined(CONFIG_GTP_HOTKNOT)
#ifndef CONFIG_HOTKNOT_BLOCK_RW
	u8 buf[1] = { 0 };
#endif
#endif
	if (is_resetting || update_info.status)
		return;
	GTP_INFO("TPD suspend start...");

	mutex_lock(&tui_lock);
	if (tpd_tui_flag) {
		GTP_INFO("[TPD] skip %s due to TUI in used\n", __func__);
		tpd_tui_low_power_skipped = 1;
		mutex_unlock(&tui_lock);
		return;
	}
	mutex_unlock(&tui_lock);

#ifdef CONFIG_GTP_PROXIMITY
	if (gt1x_proximity_flag == 1) {
		GTP_INFO("Suspend: proximity is detected!");
		return;
	}
#endif

#ifdef CONFIG_GTP_HOTKNOT
	if (hotknot_enabled) {
#ifdef CONFIG_HOTKNOT_BLOCK_RW
		if (hotknot_paired_flag) {
			GTP_INFO("Suspend: hotknot is paired!");
			return;
		}
#else
		gt1x_i2c_read(GTP_REG_HN_PAIRED, buf, sizeof(buf));
		GTP_DEBUG("0x81AA: 0x%02X", buf[0]);
		if (buf[0] == 0x55) {
			GTP_INFO("Suspend: hotknot is paired!");
			return;
		}
#endif
	}
#endif
	tpd_halt = 1;

#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_OFF);
#endif

#ifdef CONFIG_GTP_CHARGER_SWITCH
	gt1x_charger_switch(SWITCH_OFF);
#endif

	mutex_lock(&i2c_access);

#ifdef CONFIG_GTP_GESTURE_WAKEUP
	gesture_clear_wakeup_data();
	if (gesture_enabled) {
		gesture_enter_doze();
	} else
#endif
	{
		gt1x_irq_disable();
		ret = gt1x_enter_sleep();
		if (ret < 0)
			GTP_ERROR("GTP early suspend failed.");
		else
			gtp_suspend = true;
	}

	mutex_unlock(&i2c_access);
	msleep(58);
}

/* Function to manage power-on resume */
static void tpd_resume(struct device *h)
{
	s32 ret = -1;

	if (is_resetting || update_info.status)
		return;

	GTP_INFO("TPD resume start...");
	gtp_suspend = false;

#ifdef CONFIG_GTP_PROXIMITY
	if (gt1x_proximity_flag == 1) {
		GTP_INFO("Resume: proximity is on!");
		return;
	}
#endif

#ifdef CONFIG_GTP_HOTKNOT
	if (hotknot_enabled) {
#ifdef CONFIG_HOTKNOT_BLOCK_RW
		if (hotknot_paired_flag) {
			hotknot_paired_flag = 0;
			GTP_INFO("Resume: hotknot is paired!");
			return;
		}
#endif
	}
#endif
	gt1x_irq_disable();
	ret = gt1x_wakeup_sleep();
	if (ret < 0)
		GTP_ERROR("GTP later resume failed.");
#ifdef CONFIG_GTP_HOTKNOT
	if (!hotknot_enabled)
		gt1x_send_cmd(GTP_CMD_HN_EXIT_SLAVE, 0);
#endif

#ifdef CONFIG_GTP_CHARGER_SWITCH
	gt1x_charger_config(0);
	gt1x_charger_switch(SWITCH_ON);
#endif

	tpd_halt = 0;
	gt1x_irq_enable();

#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_ON);
#endif

#ifdef CONFIG_MTK_LENS
	AF_PowerDown();
#endif
	GTP_DEBUG("tpd resume end.");
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gt1151",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

void tpd_off(void)
{
	gt1x_power_switch(SWITCH_OFF);
	tpd_halt = 1;
	gt1x_irq_disable();
}

int tpd_enter_tui(void)
{
	int ret = 0;

	tpd_tui_flag = 1;
	GTP_INFO("[%s] enter tui", __func__);
	return ret;
}

int tpd_exit_tui(void)
{
	int ret = 0;

	GTP_INFO("[%s] exit TUI+", __func__);
	tpd_reregister_from_tui();
	mutex_lock(&tui_lock);
	tpd_tui_flag = 0;
	mutex_unlock(&tui_lock);
	if (tpd_tui_low_power_skipped) {
		tpd_tui_low_power_skipped = 0;
		GTP_INFO("[%s] do low power again+", __func__);
		tpd_suspend(NULL);
		GTP_INFO("[%s] do low power again-", __func__);
	}
	GTP_INFO("[%s] exit TUI-", __func__);
	return ret;
}

void tpd_on(void)
{
	s32 ret = -1, retry = 0;

	while (retry++ < 5) {
		ret = tpd_power_on();
		if (ret < 0)
			GTP_ERROR("I2C Power on ERROR!");
		ret = gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
		if (ret == 0) {
			GTP_DEBUG("Wakeup sleep send gt1x_config success.");
			break;
		}
	}
	if (ret < 0)
		GTP_ERROR("GTP later resume failed.");
	tpd_halt = 0;
}

int  gt1x_driver_init(void)
{
	GTP_INFO("Goodix touch panel driver init.");
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		GTP_INFO("add generic driver failed\n");

	return 0;
}
EXPORT_SYMBOL_GPL(gt1x_driver_init);

void  gt1x_driver_exit(void)
{
	unregister_pm_notifier(&pm_notifier_block);
	regulator_put(tpd->reg);
	disable_irq(touch_irq);
	free_irq(touch_irq, NULL);
	tpd_driver_remove(&tpd_device_driver);
}
EXPORT_SYMBOL_GPL(gt1x_driver_exit);
