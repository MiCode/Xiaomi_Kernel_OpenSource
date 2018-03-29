/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "anx_ohio_driver.h"
#include "anx_ohio_private_interface.h"
#include "anx_ohio_public_interface.h"

#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <typec.h>

u32 anx_debug_level = 255;

/* Ohio access register function */
static int create_sysfs_interfaces(struct device *dev);

static struct usbtypc *g_exttypec;

/* to access global platform data */
static struct ohio_platform_data *g_pdata;

static atomic_t power_status;
static atomic_t ohio_sys_is_ready;

#define DONGLE_CABLE_INSERT  0

static int cbl_det_irq;

struct i2c_client *ohio_client;

struct ohio_platform_data {
	int gpio_p_on;
	int gpio_reset;
	int gpio_cbl_det;
	int gpio_intr_comm;
	int gpio_v33_ctrl;
	spinlock_t lock;
};

struct ohio_data {
	struct ohio_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock ohio_lock;
	int intp_irq;
	int intp_irq_en;
	int cable_det_irq;
};

static void trigger_driver(struct usbtypc *typec, int type, int stat)
{
	anx_printk(K_INFO, "trigger_driver: type:%d, stat:%d\n", type, stat);

	if (type == DEVICE_TYPE && typec->device_driver) {
		struct typec_switch_data *drv = typec->device_driver;

		if ((stat == DISABLE) && (drv->disable)
		    && (drv->on == ENABLE)) {
			drv->disable(drv->priv_data);
			drv->on = DISABLE;

			anx_printk(K_INFO, "Device Disabe\n");
		} else if ((stat == ENABLE) && (drv->enable)
			   && (drv->on == DISABLE)) {
			drv->enable(drv->priv_data);
			drv->on = ENABLE;

			anx_printk(K_INFO, "Device Enable\n");
		} else {
			anx_printk(K_INFO, "No device driver to enable\n");
		}
	} else if (type == HOST_TYPE && typec->host_driver) {
		struct typec_switch_data *drv = typec->host_driver;

		if ((stat == DISABLE) && (drv->disable)
		    && (drv->on == ENABLE)) {
			drv->disable(drv->priv_data);
			drv->on = DISABLE;

			anx_printk(K_INFO, "Host Disable\n");
		} else if ((stat == ENABLE) &&
			   (drv->enable) && (drv->on == DISABLE)) {
			drv->enable(drv->priv_data);
			drv->on = ENABLE;

			anx_printk(K_INFO, "Host Enable\n");
		} else {
			anx_printk(K_INFO, "No device driver to enable\n");
		}
	} else {
		anx_printk(K_ERR, "wrong way\n");
	}
}

bool ohio_is_connected(void)
{
	struct ohio_platform_data *pdata = NULL;
	bool result = false;

	anx_printk(K_INFO, "enter ohio_is_connected\n");

	if (!ohio_client)
		return false;
/* FIXME */
	pdata = g_pdata;
/*
#ifdef CONFIG_OF
	pdata = g_pdata;
#else
	pdata = ohio_client->dev.platform_data;
#endif
*/

	if (!pdata)
		return false;

	anx_printk(K_INFO, "get cable detect gpio value\n");
	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			anx_printk(K_INFO, "%s %s : Slimport Dongle is detected\n",
				LOG_TAG, __func__);
			result = true;
		}
	}

	return result;
}
EXPORT_SYMBOL(ohio_is_connected);




int ohio_read_reg_byte(uint8_t slave_addr, uint8_t offset)
{
	int ret = 0;

	ohio_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(ohio_client, offset);
	if (ret < 0) {
		pr_err("%s %s: failed to read i2c addr=%x\n", LOG_TAG,
		       __func__, slave_addr);
		return ret;
	}
	return 0;
}

int ohio_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	ohio_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(ohio_client, offset);
	if (ret < 0) {
		pr_err("%s %s: failed to read i2c addr=%x\n", LOG_TAG,
		       __func__, slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int ohio_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	ohio_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(ohio_client, offset, value);
	if (ret < 0) {
		pr_err("%s %s: failed to write i2c addr=%x\n", LOG_TAG,
		       __func__, slave_addr);
	}
	return ret;
}


void ohio_power_standby(void)
{
/*
#ifdef CONFIG_OF
	struct ohio_platform_data *pdata = g_pdata;
#else
	struct ohio_platform_data *pdata = ohio_client->dev.platform_data;
#endif
*/
	/* when  system firstl initializate,
	  * pull all power sequence pin low,
	  * to make chip enter a clean status
	  */
	/* J: We can NOT control 3.3v */
	/* gpio_set_value(pdata->gpio_v33_ctrl, 0); */
	ohio_hardware_powerdown();
	mdelay(100);
	/* J: We can NOT control 3.3v */
	/* gpio_set_value(pdata->gpio_v33_ctrl, 1); */
	/* mdelay(100); */

	anx_printk(K_INFO, "ohio power standby\n");

}


void ohio_hardware_poweron(void)
{
	int i = 0;
	int retry_count = 0;

/*
#ifdef CONFIG_OF
	struct ohio_platform_data *pdata = g_pdata;
#else
	struct ohio_platform_data *pdata = ohio_client->dev.platform_data;
#endif
*/
	anx_printk(K_INFO, "ohio_hardware_poweron\n");

	retry_count = 3;
	while (retry_count) {
		ohio_hardware_powerdown();

		atomic_set(&power_status, 1);

		/* power enable */
		/*gpio_set_value(pdata->gpio_p_on, 1);*/
		pinctrl_select_state(g_exttypec->pinctrl,
				g_exttypec->pin_cfg->pwr_en_high);
		mdelay(10*2);
		/*gpio_set_value(pdata->gpio_reset, 1);*/
		pinctrl_select_state(g_exttypec->pinctrl,
				g_exttypec->pin_cfg->rst_n_high);
		mdelay(10);

		/* eeprom load */
		for (i = 0; i < 3200; i++) {	  /* delay T3 (3.2s)*/
			/*Interface work?*/
			if ((OhioReadReg(0x16) & 0x80) == 0x80) {
				atomic_set(&ohio_sys_is_ready, 1);

				interface_init();
				send_initialized_setting();
				anx_printk(K_INFO, "init interface setting\n");
				break;
			}
			mdelay(1);
		}

		if (atomic_read(&ohio_sys_is_ready) != 1)
			continue;

		if (OhioReadReg(0x66) == 0x1b) {
			anx_printk(K_INFO, "EEPROM load success, CRC *correct*!\n");
			break;
		} else if (OhioReadReg(0x66) == 0x4b) {
			pr_err("EEPROM load success, CRC *incorrect*!\n");
			break;
		} else if (retry_count > 0) {
			pr_err("EEPROM load retry!\n");
			retry_count--;
			continue;
		}
	}
}

void ohio_hardware_powerdown(void)
{
#ifdef NEVER
#ifdef CONFIG_OF
	struct ohio_platform_data *pdata = g_pdata;
#else
	struct ohio_platform_data *pdata = ohio_client->dev.platform_data;
#endif
#endif /* NEVER */

	anx_printk(K_INFO, "ohio_hardware_powerdown\n");

	atomic_set(&power_status, 0);
	atomic_set(&ohio_sys_is_ready, 0);

	/*pull down reset chip*/
	/*gpio_set_value(pdata->gpio_reset, 0);*/
	pinctrl_select_state(g_exttypec->pinctrl,
		g_exttypec->pin_cfg->rst_n_low);
	mdelay(1);

	/*gpio_set_value(pdata->gpio_p_on, 0);*/
	pinctrl_select_state(g_exttypec->pinctrl,
		g_exttypec->pin_cfg->pwr_en_low);
	mdelay(1);
}



static void ohio_free_gpio(struct ohio_data *ohio)
{
	gpio_free(ohio->pdata->gpio_cbl_det);
	gpio_free(ohio->pdata->gpio_reset);
	gpio_free(ohio->pdata->gpio_p_on);
	gpio_free(ohio->pdata->gpio_intr_comm);
	gpio_free(ohio->pdata->gpio_v33_ctrl);
}

static int ohio_init_gpio(struct ohio_data *ohio)
{
	int ret = 0;

	pinctrl_select_state(g_exttypec->pinctrl,
		g_exttypec->pin_cfg->pwr_en_init);

	pinctrl_select_state(g_exttypec->pinctrl,
		g_exttypec->pin_cfg->rst_n_init);

	pinctrl_select_state(g_exttypec->pinctrl,
		g_exttypec->pin_cfg->pwr_en_init);

	pinctrl_select_state(g_exttypec->pinctrl,
		g_exttypec->pin_cfg->intp_init);

	anx_printk(K_INFO, "rst_n=%d, p_on=%d\n",
		gpio_get_value(ohio->pdata->gpio_reset),
		gpio_get_value(ohio->pdata->gpio_p_on));

	return ret;

/* Use MTK gpio api first, not from DTS */
#ifdef NEVER
	pr_info("%s %s: ohio init gpio\n", LOG_TAG, __func__);
	/*  gpio for chip power down  */
	ret = gpio_request(ohio->pdata->gpio_p_on, "ohio_p_on_ctl");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
		       ohio->pdata->gpio_p_on);
		goto err0;
	}
	gpio_direction_output(ohio->pdata->gpio_p_on, 0);
	/*  gpio for chip reset  */
	ret = gpio_request(ohio->pdata->gpio_reset, "ohio_reset_n");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
		       ohio->pdata->gpio_reset);
		goto err1;
	}
	gpio_direction_output(ohio->pdata->gpio_reset, 0);


	/*  gpio for ohio cable detect  */
	ret = gpio_request(ohio->pdata->gpio_cbl_det, "ohio_cbl_det");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
		       ohio->pdata->gpio_cbl_det);
		goto err2;
	}
	gpio_direction_input(ohio->pdata->gpio_cbl_det);
	pr_info("cabled detect successfully set up\n");


	/*  gpio for chip interface communaction */
	ret = gpio_request(ohio->pdata->gpio_intr_comm, "ohio_intr_comm");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
		       ohio->pdata->gpio_intr_comm);
		goto err3;
	}
	gpio_direction_input(ohio->pdata->gpio_intr_comm);



	/*  gpio for chip standby control DVDD33 */
	ret = gpio_request(ohio->pdata->gpio_v33_ctrl, "ohio_v33_ctrl");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
		       ohio->pdata->gpio_v33_ctrl);
		goto err4;
	}
	gpio_direction_output(ohio->pdata->gpio_v33_ctrl, 0);

	pr_info("ohio init gpio successfully\n");

	goto out;
err4:
	gpio_free(ohio->pdata->gpio_v33_ctrl);
err3:
	gpio_free(ohio->pdata->gpio_intr_comm);
err2:
	gpio_free(ohio->pdata->gpio_cbl_det);
err1:
	gpio_free(ohio->pdata->gpio_reset);
err0:
	gpio_free(ohio->pdata->gpio_p_on);
	return 1;
out:
	return 0;
#endif /* NEVER */
}

static int ohio_system_init(void)
{
	/*init interface between AP and Ohio*/
	/*interface_init();*/
	return 0;
}

void cable_disconnect(void *data)
{
	struct ohio_data *ohio = data;

	cancel_delayed_work_sync(&ohio->work);
	flush_workqueue(ohio->workqueue);
	ohio_hardware_powerdown();
	/*ohio_clean_state_machine();*/
	wake_unlock(&ohio->ohio_lock);
	wake_lock_timeout(&ohio->ohio_lock, 2 * HZ);

}

/*#define CABLE_DET_PIN_HAS_GLITCH*/
#define DONGLE_CABLE_ISERT 1

int cable_connected = 0;

/*
  * example 1: update power's source capability from AP to ohio
  *
  */
void update_pwr_src_caps(void)
{
	/*
	*example 1: source capability for customer's reference,
	*you can change it using PDO_XXX() macro easily
	*/
	u32 src_caps[] = {
		/*5V, 0.9A, Fixed*/
		PDO_FIXED(PD_VOLTAGE_5V, PD_CURRENT_900MA, PDO_FIXED_FLAGS)
	};

	anx_printk(K_INFO, "update_pwr_src_caps\n");
	/* send source capability from AP to ohio. */
	send_pd_msg(TYPE_PWR_SRC_CAP,
			(const char *)src_caps,
			sizeof(src_caps));
}

/*
  * example 2: send sink capability from AP to ohio
  *
  */
void update_pwr_sink_caps(void)
{
	/*
	  *example 2: sink power capability for customer's reference,
	  *you can change it using PDO_XXX() macro easily
	  */
	u32 sink_caps[] = {
		PDO_FIXED(PD_VOLTAGE_5V, PD_CURRENT_900MA, PDO_FIXED_FLAGS)
	};

	anx_printk(K_INFO, "update_pwr_sink_caps\n");
	/* send source capability from AP to ohio. */
	send_pd_msg(TYPE_PWR_SNK_CAP,
			(const char *)sink_caps,
			4 /*sizeof(sink_caps)*/);
}

/*
  * example 3: send VDM  from AP to ohio
  */
void update_VDM(void)
{
	u8 vdm[] = {
		0x00, 0x00, 0x01, 0x00,
		0x43, 0x45, 0x54, 0x56
	};

	anx_printk(K_INFO, "update_vdm\n");
	/* send source capability from AP to ohio. */
	send_pd_msg(TYPE_VDM, (const char *)vdm, sizeof(vdm));
}

void ohio_main_process(struct ohio_data *ohio)
{
	anx_printk(K_INFO, "cable_connected=%d, power_status=%d\n",
		cable_connected, atomic_read(&power_status));
	if (cable_connected) {
		if (atomic_read(&power_status) == 0) {
			int anlg_sts = 0;

			ohio_hardware_poweron();
			OhioWriteReg(IRQ_EXT_SOURCE_2, 0xff);

			if (!ohio->intp_irq_en) {
				/*enable_irq(ohio->intp_irq);*/
				ohio->intp_irq_en = 1;
			}

			anlg_sts = OhioReadReg(REG_ANALOG_STATUS);

			if (!!(anlg_sts & UFP_PLUG_MSK) &&
				!!(anlg_sts & DFP_OR_UFP_MSK))
				trigger_driver(g_exttypec, DEVICE_TYPE, ENABLE);
			else if (!(anlg_sts & DFP_OR_UFP_MSK))
				trigger_driver(g_exttypec, HOST_TYPE, ENABLE);
		}
	} else {
		if (atomic_read(&power_status) == 1) {
			ohio_power_standby();
			/*ohio_sys_is_ready = 0;*/
		}
	}
}

static unsigned char confirmed_cable_det(void *data)
{
	struct ohio_data *anxohio = data;
	u8 val = 0;

#ifdef CABLE_DET_PIN_HAS_GLITCH
	unsigned char count = 20;
	unsigned char cable_det_count = 0;

	do {
		val = gpio_get_value(anxohio->pdata->gpio_cbl_det);
		if ((val) == DONGLE_CABLE_INSERT) {
			anx_printk(K_INFO, "cable detect value :%x counter= %d\n",
				val, cable_det_count);
			cable_det_count++;
		}
		if (cable_det_count > 10)
			return 1;

		mdelay(1);
	} while (count--);

	return (cable_det_count > 10) ? 1 : 0;
#else
	val = gpio_get_value(anxohio->pdata->gpio_cbl_det);
	return val;
#endif
}

static irqreturn_t ohio_cbl_det_isr(int irq, void *data)
{
	/********************************************************************
	defaultly when DVDDIO and AVDD33 is pull up by AP, Ohio enter standby
	mode, cable detect pin will be toggled every 100ms, so you need to
	deglith this pause!!! cable detect pin will be pulled up when
	downstream cable is plugged, the driver will power on chip, and then
	the driver control when to powerdown the chip.
	********************************************************************/

	struct ohio_data *ohio = data;

	anx_printk(K_INFO, "ohio_cbl_det_isr+ cable_connected=%d gpio=%d\n",
		cable_connected, gpio_get_value(ohio->pdata->gpio_cbl_det));

	if (cable_connected) {
		if (confirmed_cable_det((void *)ohio) > 0)
			goto end;
		else
			cable_connected = 0;
	} else
		cable_connected = confirmed_cable_det((void *)ohio);

	anx_printk(K_INFO, "detect cable insertion, cable_connected = %d\n",
					cable_connected);

	if (cable_connected != 0) {
		wake_lock(&ohio->ohio_lock);
		anx_printk(K_INFO, "detect cable insertion\n");
		queue_delayed_work(ohio->workqueue, &ohio->work, 0);
		irq_set_irq_type(cbl_det_irq,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT);
	} else {
		anx_printk(K_INFO, "detect cable removal\n");
		if (ohio->intp_irq_en) {
			/*disable_irq_nosync(ohio->intp_irq);*/
			ohio->intp_irq_en = 0;
		}
		cable_disconnect(ohio);

		if (g_exttypec->device_driver &&
			(g_exttypec->device_driver->on == ENABLE))
			trigger_driver(g_exttypec, DEVICE_TYPE, DISABLE);
		else if (g_exttypec->host_driver &&
			(g_exttypec->host_driver->on == ENABLE))
			trigger_driver(g_exttypec, HOST_TYPE, DISABLE);

		irq_set_irq_type(cbl_det_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT);
	}
end:
	return IRQ_HANDLED;
}

interface_msg_t *cur_intr_msg_ptr = 0;
long interface_intr_counter = 0;

static irqreturn_t ohio_intr_comm_isr(int irq, void *data)
{
	/********************************************************************
	Ohio's interrupt process function:
	when ohio's interrupt pin is driven(now low active) , one interrupt
	happen, currently, Ohio use interrupt event for communaction between
	AP and Ohio FM simple Algorithm when a new message arrived, we should
	check it not only once, for the circle buffer commucation with Ohio
	will empty sometimes. so to improve the perfomance avoiding to this
	situation, we should check the loop buffer more times .
	********************************************************************/
	if (atomic_read(&ohio_sys_is_ready) == 0) {
		pr_err("%s sys not ready\n", __func__);
		return IRQ_NONE;
	}

	if (atomic_read(&power_status) == 0) {
		pr_err("%s power off\n", __func__);
		return IRQ_NONE;
	}

	anx_printk(K_INFO, "%s %02x\n", __func__,
			OhioReadReg(IRQ_EXT_SOURCE_2));

	interface_intr_counter = 0;

	/*recv command from ohio*/
	while (1) {
		if (interface_intr_counter++ > 10000)
			break;

		if (is_soft_reset_intr()) {
			OhioWriteReg(IRQ_EXT_SOURCE_2, 0xff);
			polling_interface_msg(200);
		}

		cur_intr_msg_ptr = imsg_fetch();
		if (cur_intr_msg_ptr != NULL)
			dispatch_rcvd_pd_msg(
				(PD_MSG_TYPE)cur_intr_msg_ptr->data[1],
				&(cur_intr_msg_ptr->data[2]),
				cur_intr_msg_ptr->data[0]-2);
		else
			interface_intr_counter += 2000;

		/*when interrupt bit cleared, should also check msg
		  arrvied or not*/
		polling_interface_msg(30);
	}
	return IRQ_HANDLED;
}

static void ohio_work_func(struct work_struct *work)
{
	struct ohio_data *ohio = container_of(work, struct ohio_data,
					       work.work);

	/*int workqueu_timer = 10;*/

	anx_printk(K_INFO, "ohio_work_func - start\n");

	mutex_lock(&ohio->lock);
	ohio_main_process(ohio);
	mutex_unlock(&ohio->lock);

	/*Queue this work, when cable has changed the status.*/
	/*queue_delayed_work(td->workqueue, &td->work,
	  msecs_to_jiffies(workqueu_timer));*/

	anx_printk(K_INFO, "ohio_work_func - end\n");
}

int register_typec_switch_callback(struct typec_switch_data *new_driver)
{
	anx_printk(K_INFO, "Register driver %s %d\n",
		new_driver->name, new_driver->type);

	if (new_driver->type == DEVICE_TYPE) {
		g_exttypec->device_driver = new_driver;
		g_exttypec->device_driver->on = 0;
		return 0;
	}

	if (new_driver->type == HOST_TYPE) {
		g_exttypec->host_driver = new_driver;
		g_exttypec->host_driver->on = 0;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(register_typec_switch_callback);

int unregister_typec_switch_callback(struct typec_switch_data *new_driver)
{
	anx_printk(K_INFO, "Unregister driver %s %d\n",
		new_driver->name, new_driver->type);

	if ((new_driver->type == DEVICE_TYPE) &&
		(g_exttypec->device_driver == new_driver))
		g_exttypec->device_driver = NULL;

	if ((new_driver->type == HOST_TYPE) &&
		(g_exttypec->host_driver == new_driver))
		g_exttypec->host_driver = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_typec_switch_callback);

#ifdef CONFIG_OF
int ohio_regulator_configure(struct device *dev,
				struct ohio_platform_data *pdata)
{

	return 0;
}

static int ohio_parse_dt(struct device *dev,
			    struct ohio_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int gpio;

	gpio = of_get_named_gpio(np, "analogix,p-on-gpio", 0);
	if (gpio_is_valid(gpio))
		pdata->gpio_p_on = gpio;
	else
		pr_err("can not get valid gpio analogix,p-on-gpio\n");

	gpio = of_get_named_gpio(np, "analogix,reset-gpio", 0);
	if (gpio_is_valid(gpio))
		pdata->gpio_reset = gpio;
	else
		pr_err("can not get valid gpio analogix,reset-gpio\n");

	gpio = of_get_named_gpio(np, "analogix,cbl-det-gpio", 0);
	if (gpio_is_valid(gpio))
		pdata->gpio_cbl_det = gpio;
	else
		pr_err("can not get valid gpio analogix,cbl-det-gpio\n");

	gpio = of_get_named_gpio(np, "analogix,intr-comm-gpio", 0);
	if (gpio_is_valid(gpio))
		pdata->gpio_intr_comm = gpio;
	else
		pr_err("can not get valid gpio analogix,intr-comm-gpio\n");

	anx_printk(K_INFO, "p_on=%d, reset=%d, cbl_det=%d, intp=%d\n",
	       pdata->gpio_p_on, pdata->gpio_reset,
	       pdata->gpio_cbl_det, pdata->gpio_intr_comm);

	return 0;
}
#else
static int ohio_parse_dt(struct device *dev,
			    struct ohio_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int usbc_pinctrl_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct usbtypc *typec;
	struct ohio_platform_data *pdata;

	anx_printk(K_INFO, "%s\n", __func__);

	if (!g_exttypec) {
		typec = kzalloc(sizeof(struct usbtypc), GFP_KERNEL);
		g_exttypec = typec;
	} else {
		typec = g_exttypec;
	}

	typec->pinctrl_dev = &pdev->dev;

	typec->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(typec->pinctrl)) {
		pr_err("Cannot find usb pinctrl!\n");
	} else {
		typec->pin_cfg = kzalloc(sizeof(struct usbc_pin_ctrl),
			GFP_KERNEL);

		anx_printk(K_INFO, "pinctrl=%p\n", typec->pinctrl);

		/********************************************************/
		typec->pin_cfg->rst_n_init =
				pinctrl_lookup_state(typec->pinctrl,
					"anx_rst_n_init");
		if (IS_ERR(typec->pin_cfg->rst_n_init))
			pr_err("Can *NOT* find rst_n_init\n");
		else
			anx_printk(K_INFO, "Find rst_n_init\n");

		typec->pin_cfg->rst_n_low =
				pinctrl_lookup_state(typec->pinctrl,
					"anx_rst_n_low");
		if (IS_ERR(typec->pin_cfg->rst_n_low))
			pr_err("Can *NOT* find rst_n_low\n");
		else
			anx_printk(K_INFO, "Find rst_n_low\n");

		typec->pin_cfg->rst_n_high =
				pinctrl_lookup_state(typec->pinctrl,
					"anx_rst_n_high");
		if (IS_ERR(typec->pin_cfg->rst_n_high))
			pr_err("Can *NOT* find rst_n_high\n");
		else
			anx_printk(K_INFO, "Find rst_n_high\n");
		/********************************************************/
		typec->pin_cfg->pwr_en_init =
				pinctrl_lookup_state(typec->pinctrl,
					"anx_pwr_en_init");
		if (IS_ERR(typec->pin_cfg->pwr_en_init))
			pr_err("Can *NOT* find pwr_en_init\n");
		else
			anx_printk(K_INFO, "Find pwr_en_init\n");

		typec->pin_cfg->pwr_en_low =
				pinctrl_lookup_state(typec->pinctrl,
					"anx_pwr_en_low");
		if (IS_ERR(typec->pin_cfg->pwr_en_low))
			pr_err("Can *NOT* find pwr_en_low\n");
		else
			anx_printk(K_INFO, "Find pwr_en_low\n");

		typec->pin_cfg->pwr_en_high =
				pinctrl_lookup_state(typec->pinctrl,
					"anx_pwr_en_high");
		if (IS_ERR(typec->pin_cfg->pwr_en_high))
			pr_err("Can *NOT* find pwr_en_high\n");
		else
			anx_printk(K_INFO, "Find pwr_en_high\n");
		/********************************************************/
		typec->pin_cfg->cbl_det_init = pinctrl_lookup_state(typec->pinctrl,
			"anx_cable_detect_init");
		if (IS_ERR(typec->pin_cfg->cbl_det_init))
			pr_err("Can *NOT* find cbl_det_init\n");
		else
			anx_printk(K_INFO, "Find cbl_det_init\n");

		typec->pin_cfg->intp_init = pinctrl_lookup_state(typec->pinctrl,
			"anx_intp_init");
		if (IS_ERR(typec->pin_cfg->intp_init))
			pr_err("Can *NOT* find intp_init\n");
		else
			anx_printk(K_INFO, "Find intp_init\n");


		anx_printk(K_INFO, "Finish parsing pinctrl\n");
	}

	pdata = devm_kzalloc(&pdev->dev,
		     sizeof(struct ohio_platform_data),
		     GFP_KERNEL);

	pdev->dev.platform_data = pdata;

	anx_printk(K_INFO, "%s pdata=%p\n", __func__, pdata);

	/* device tree parsing function call */
	retval = ohio_parse_dt(&pdev->dev, pdata);
	if (retval != 0)	/* if occurs error */
		retval = -1;

	return retval;
}

static const struct of_device_id usb_pinctrl_ids[] = {
	{.compatible = "mediatek,usb_c_pinctrl",},
	{},
};

static struct platform_driver usbc_pinctrl_driver = {
	.probe = usbc_pinctrl_probe,
	.driver = {
		.name = "usbc_pinctrl",
#ifdef CONFIG_OF
		.of_match_table = usb_pinctrl_ids,
#endif
	},
};

static int ohio_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{

	struct ohio_data *ohio;
	/*struct ohio_platform_data *pdata;*/
	int ret = 0;
	struct usbtypc *typec;
	struct device_node *node;

	if (!g_exttypec) {
		typec = kzalloc(sizeof(struct usbtypc), GFP_KERNEL);
		g_exttypec = typec;
	} else {
		typec = g_exttypec;
	}

	typec->i2c_hd = client;

	if (!i2c_check_functionality(client->adapter,
	I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c bus does not support the ohio\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	ohio = kzalloc(sizeof(struct ohio_data), GFP_KERNEL);
	if (!ohio) {
		ret = -ENOMEM;
		goto exit;
	}

	if (typec->pinctrl_dev)
		ohio->pdata = client->dev.platform_data =
			typec->pinctrl_dev->platform_data;

	anx_printk(K_INFO, "%s ohio->pdata=%p\n", __func__, ohio->pdata);

#ifdef NEVER
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct ohio_platform_data),
				     GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;

		/* device tree parsing function call */
		ret = ohio_parse_dt(&client->dev, pdata);
		if (ret != 0)	/* if occurs error */
			goto err0;

		ohio->pdata = pdata;
	} else {
		ohio->pdata = client->dev.platform_data;
	}
#endif /* NEVER */

	/* to access global platform data */
	g_pdata = ohio->pdata;
	ohio_client = client;

	mutex_init(&ohio->lock);

	if (!ohio->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = ohio_init_gpio(ohio);
	if (ret) {
		pr_err("%s: failed to initialize gpio\n", __func__);
		goto err0;
	}

	INIT_DELAYED_WORK(&ohio->work, ohio_work_func);

	ohio->workqueue = create_singlethread_workqueue("ohio_work");
	if (ohio->workqueue == NULL) {
		pr_err("%s: failed to create work queue\n", __func__);
		ret = -ENOMEM;
		goto err1;
	}

	ret = ohio_system_init();
	if (ret) {
		pr_err("%s: failed to initialize ohio\n", __func__);
		goto err2;
	}

	wake_lock_init(&ohio->ohio_lock,
		       WAKE_LOCK_SUSPEND, "ohio_wake_lock");

#ifdef NEVER
	cbl_det_irq = gpio_to_irq(ohio->pdata->gpio_cbl_det);
	if (cbl_det_irq < 0) {
		pr_err("%s : failed to get gpio irq\n", __func__);
		goto err2;
	}
#endif /* NEVER */

	node = of_find_compatible_node(NULL, NULL,
				"mediatek,usb_iddig_bi_eint");
	if (node) {
		u32 ints[2] = { 0, 0 };
		unsigned int gpiopin, debounce;

		of_property_read_u32_array(node, "debounce",
			ints, ARRAY_SIZE(ints));

		debounce = ints[1];
		gpiopin = ints[0];

		gpio_set_debounce(gpiopin, debounce);

		anx_printk(K_INFO, "gpiopin=%d, debounce=%d\n",
			gpiopin, debounce);
	}

	cbl_det_irq = irq_of_parse_and_map(node, 0);

	anx_printk(K_INFO, "%s cbl_det_irq=%d\n", __func__, cbl_det_irq);

	ret = request_threaded_irq(cbl_det_irq, NULL, ohio_cbl_det_isr,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "ohio-cbl-det", ohio);
	if (ret < 0) {
		pr_err("%s : failed to request irq\n", __func__);
		goto err3;
	}

#ifdef NEVER
	ret = irq_set_irq_wake(cbl_det_irq, 1);
	if (ret < 0) {
		pr_err("%s : Request irq for cable detect", __func__);
		pr_err("interrupt wake set fail\n");
		goto err4;
	}

	ret = enable_irq_wake(cbl_det_irq);
	if (ret < 0) {
		pr_err("%s : Enable irq for cable detect", __func__);
		pr_err("interrupt wake enable fail\n");
		goto err4;
	}
#endif /* NEVER */

#ifdef NEVER
	client->irq = gpio_to_irq(ohio->pdata->gpio_intr_comm);
	if (client->irq < 0) {
		pr_err("%s : failed to get ohio gpio comm irq\n", __func__);
		goto err3;
	}
#endif /* NEVER */

	node = of_find_compatible_node(NULL, NULL, "mediatek,fusb300-eint");
	if (node) {
		u32 ints[2] = { 0, 0 };
		unsigned int gpiopin, debounce;

		of_property_read_u32_array(node, "debounce",
			ints, ARRAY_SIZE(ints));

		debounce = ints[1];
		gpiopin = ints[0];

		gpio_set_debounce(gpiopin, debounce);

		anx_printk(K_INFO, "gpiopin=%d, debounce=%d\n",
			gpiopin, debounce);
	}
	ohio->intp_irq = client->irq = irq_of_parse_and_map(node, 0);

	anx_printk(K_INFO, "%s intp_irq=%d\n", __func__, ohio->intp_irq);

	ret = request_threaded_irq(client->irq, NULL, ohio_intr_comm_isr,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
		IRQF_ONESHOT, "ohio-intr-comm", ohio);
	if (ret < 0) {
		pr_err("%s : failed to request interface irq\n", __func__);
		goto err4;
	}
	ohio->intp_irq_en = 1;

#ifdef NEVER
	ret = irq_set_irq_wake(client->irq, 1);
	if (ret < 0) {
		pr_err("%s : Request irq for interface communaction", __func__);
		goto err4;
	}

	ret = enable_irq_wake(client->irq);
	if (ret < 0) {
		pr_err("%s : Enable irq for interface communaction", __func__);
		goto err4;
	}
#endif /* NEVER */

	anx_printk(K_INFO, "%s 11\n", __func__);

	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		pr_err("%s : sysfs register failed", __func__);
		goto err4;
	}

	/*enable driver*/
	queue_delayed_work(ohio->workqueue, &ohio->work, 0);

	atomic_set(&power_status, 0);
	atomic_set(&ohio_sys_is_ready, 0);
	cable_connected = 0;
	ohio_power_standby();

	anx_printk(K_INFO, "ohio_i2c_probe successfully end\n");
	goto exit;

err4:
	free_irq(client->irq, ohio);
err3:
	free_irq(cbl_det_irq, ohio);
err2:
	destroy_workqueue(ohio->workqueue);
err1:
	/*ohio_free_gpio(ohio);*/
err0:
	ohio_client = NULL;
	kfree(ohio);
exit:
	return ret;
}

static int ohio_i2c_remove(struct i2c_client *client)
{
	struct ohio_data *ohio = i2c_get_clientdata(client);

	free_irq(client->irq, ohio);
	ohio_free_gpio(ohio);
	destroy_workqueue(ohio->workqueue);
	wake_lock_destroy(&ohio->ohio_lock);
	kfree(ohio);
	return 0;
}

static const struct i2c_device_id ohio_id[] = {
	{"ohio", 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id anx_match_table[] = {
	{.compatible = "mediatek,usb_type_c",},
	{},
};
#endif

static struct i2c_driver ohio_driver = {
	.driver = {
		   .name = "ohio",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = anx_match_table,
#endif
		   },
	.probe = ohio_i2c_probe,
	.remove = ohio_i2c_remove,
	.id_table = ohio_id,
};

static void __init ohio_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	anx_printk(K_INFO, "%s\n", __func__);

	ret = platform_driver_register(&usbc_pinctrl_driver);

	if (!ret) {
		anx_printk(K_INFO, "register usbc pinctrl succeed!!\n");

		ret = i2c_add_driver(&ohio_driver);
		if (ret < 0)
			pr_err("%s: failed to register ohio i2c drivern",
				__func__);
		else
			anx_printk(K_INFO, "ohio_init_async initialization succeed!!\n");
	} else
		pr_err("register usbc pinctrl fail!!\n");
}

static int __init ohio_init(void)
{
	async_schedule(ohio_init_async, NULL);
	return 0;
}

static void __exit ohio_exit(void)
{
	i2c_del_driver(&ohio_driver);
}


/*****************************
 * power control interface:
 *  to power on chip(enable=0)
 *  or power down chip(enable = 1)
 */
static void hardware_power_ctl(unchar enable)
{
	if (enable == 0)
		ohio_hardware_powerdown();
	else
		ohio_hardware_poweron();
}

/******************************
 * detect ohio chip id
 * ohio Vendor ID definition, low and high byte
 */
#define OHIO_SLVAVE_I2C_ADDR 0x50
#define VENDOR_ID_L 0x00
#define VENDOR_ID_H 0x01
#define OHIO_NUMS 7
uint chipid_list[OHIO_NUMS] = {
	0x7418,
	0x7428,
	0x7408,
	0x7409,
	0x7401,
	0x7402,
	0x7403
};

bool ohio_chip_detect(void)
{
	uint c;
	bool big_endian;
	unchar *ptemp;
	int i;
	/*check whether CPU is big endian*/
	c = 0x1222;
	ptemp = (unchar *)&c;
	if (*ptemp == 0x11 && *(ptemp + 1) == 0x22)
		big_endian = 1;
	else
		big_endian = 0;
	hardware_power_ctl(1);
	c = 0;
	/*check chip id*/
	if (big_endian) {
		ohio_read_reg(OHIO_SLVAVE_I2C_ADDR, VENDOR_ID_L,
			(unchar *)(&c) + 1);
		ohio_read_reg(OHIO_SLVAVE_I2C_ADDR, VENDOR_ID_H,
			(unchar *)(&c));

	} else {
		ohio_read_reg(OHIO_SLVAVE_I2C_ADDR, VENDOR_ID_L,
			(unchar *)(&c));
		ohio_read_reg(OHIO_SLVAVE_I2C_ADDR, VENDOR_ID_H,
			(unchar *)(&c) + 1);
	}

	anx_printk(K_INFO, "%s : CHIPID: ANX%x\n", __func__, c & 0x0000FFFF);
	for (i = 0; i < OHIO_NUMS; i++) {
		if (c == chipid_list[i])
			return 1;
	}
	return 0;
}

void dump_reg(void)
{
	int i, j;

	for (i = 0; i < 0x10; i++) {
		char tmp[0x10] = {0};

		for (j = 0; j < 0x10; j++)
			tmp[j] = OhioReadReg(i*0x10 + j);

		anx_printk(K_INFO, "[%02x] %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i*0x10,
			tmp[0x0], tmp[0x1], tmp[0x2], tmp[0x3],
			tmp[0x4], tmp[0x5], tmp[0x6], tmp[0x7],
			tmp[0x8], tmp[0x9], tmp[0xA], tmp[0xB],
			tmp[0xC], tmp[0xD], tmp[0xE], tmp[0xF]);
	}
}

ssize_t anx_ohio_send_pd_cmd(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int cmd;
	int result;

	result = kstrtoint(buf, 0, &cmd);
	switch (cmd) {
	case TYPE_PWR_SRC_CAP:
		update_pwr_src_caps();
		/*sp_tx_chip_located();*/
	break;

	case TYPE_PWR_SNK_CAP:
		update_pwr_sink_caps();
	break;

	case TYPE_DP_SNK_IDENDTITY:
		send_pd_msg(TYPE_DP_SNK_IDENDTITY, 0, 0);
	break;

	case TYPE_PSWAP_REQ:
		send_pd_msg(TYPE_PSWAP_REQ, 0, 0);
	break;

	case TYPE_DSWAP_REQ:
		send_pd_msg(TYPE_DSWAP_REQ, 0, 0);
	break;

	case TYPE_GOTO_MIN_REQ:
		send_pd_msg(TYPE_GOTO_MIN_REQ, 0, 0);
	break;

	case TYPE_VDM:
		update_VDM();
	break;

	case TYPE_PWR_OBJ_REQ:
		interface_send_request();
	break;

	case TYPE_ACCEPT:
		interface_send_accept();
	break;

	case TYPE_REJECT:
		interface_send_reject();
	break;

	case TYPE_SOFT_RST:
		send_pd_msg(TYPE_SOFT_RST, 0, 0);
	break;

	case TYPE_HARD_RST:
		send_pd_msg(TYPE_HARD_RST, 0, 0);
	break;

	case 0xff:
		dump_reg();
	break;
	}
	return count;
}

ssize_t anx_ohio_send_thr_cmd(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int cmd;
	int result;

	result = kstrtoint(buf, 0, &cmd);
	/*usb_pd_cmd = cmd;*/
	return count;
}

ssize_t anx_ohio_get_data_role(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", misc_status&0x2);
}

ssize_t anx_ohio_get_power_role(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", misc_status&0x1);
}

ssize_t anx_ohio_rd_reg(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int cmd;
	int result;

	result = kstrtoint(buf, 0, &cmd);
	if (!result)
		anx_printk(K_INFO, "reg[%x] = %x\n", cmd, OhioReadReg(cmd));

	return count;
}

ssize_t anx_ohio_wr_reg(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int cmd, val;
	int result;

	result = sscanf(buf, "%d  %d", &cmd, &val);
	OhioWriteReg(cmd, val);
	anx_printk(K_INFO, "reg[%x] = %x\n", cmd, OhioReadReg(cmd));
	return count;
}

ssize_t anx_ohio_dump_register(struct device *dev,
				struct device_attribute *attr, char *buf)
{
#ifdef NEVER
	int i = 0;
	int j = 0;
	int len = 0;

	for (i = 0; i < 0x10; i++) {
		unsigned int tmp[16] = {0};

		for (j = 0; j < 0x10; j++)
			tmp[j] = OhioReadReg(i*0x10+j);

		/*len += sprintf(&buf[i*53], "[%02x] %02x %02x %02x %02x %02x %02x"
			" %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i*0x10,
			tmp[0x0], tmp[0x1], tmp[0x2], tmp[0x3],
			tmp[0x4], tmp[0x5], tmp[0x6], tmp[0x7],
			tmp[0x8], tmp[0x9], tmp[0xA], tmp[0xB],
			tmp[0xC], tmp[0xD], tmp[0xE], tmp[0xF]);*/
	}
	anx_printk(K_INFO, "len=%d\n", len);
	anx_printk(K_INFO, ">>>%s<<<\n", buf);
	return len;
#endif
	return 0;
}

ssize_t store_anx_ohio_set_gpio(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int cmd, val;
	int result;
/*test gpio p_on GPIO251
 * 1. control by pinctrl
 * 2. control by gpiolib (gpio_direction_input/
 *                        gpio_direction_output/gpio_set_value)
 * --> monitor by  cat /sys/devices/virtual/misc/mtgpio/pin
 * --> get value by gpio_get_value
  test cbl_det
 * 1. control by pinctrl -> mode1(iddig)/mode0(gpio)
 * 2.
*/
/*
	cmd=1 -> use pinctrl
	val=0 -> output low
	val=1 -> output high
	--------------------
	cmd=2 -> use gpiolib + gpio number get from of
	val=0 -> gpio_direction_output + gpio_set_value(0)
	val=1 -> gpio_direction_output + gpio_set_value(1)
	val=2 -> gpio_direction_input
	--------------------
	cmd=3 -> use gpiolib + hard code gpio number(GPIO251)
	val=0 -> gpio_direction_output + gpio_set_value(0)
	val=1 -> gpio_direction_output + gpio_set_value(1)
	val=2 -> gpio_direction_input
*/
	result = sscanf(buf, "%d  %d", &cmd, &val);
	if (cmd == 1) {
		if (val == 0)
			pinctrl_select_state(g_exttypec->pinctrl,
					g_exttypec->pin_cfg->pwr_en_low);
		else if (val == 1)
			pinctrl_select_state(g_exttypec->pinctrl,
					g_exttypec->pin_cfg->pwr_en_high);
	} else if (cmd == 2) {
		if (val == 0)
			gpio_direction_output(g_pdata->gpio_p_on, 0);
		else if (val == 1)
			gpio_direction_output(g_pdata->gpio_p_on, 1);
		else if (val == 2)
			gpio_direction_input(g_pdata->gpio_p_on);
	} else if (cmd == 3) {
		if (val == 0)
			gpio_direction_output(251, 0);
		else if (val == 1)
			gpio_direction_output(251, 1);
		else if (val == 2)
			gpio_direction_input(251);

	}
	anx_printk(K_INFO, "gpio_p_on=[%d]%x [%d]%x\n",
					g_pdata->gpio_p_on,
					gpio_get_value(g_pdata->gpio_p_on),
					251,
					gpio_get_value(251));
	return count;
}

static struct device_attribute anx_ohio_device_attrs[] = {
		__ATTR(pdcmd, S_IWUSR, NULL, anx_ohio_send_pd_cmd),
		__ATTR(thrcmd, S_IWUSR, NULL, anx_ohio_send_thr_cmd),
		__ATTR(rdreg, S_IWUSR, NULL, anx_ohio_rd_reg),
		__ATTR(wrreg, S_IWUSR, NULL, anx_ohio_wr_reg),
		__ATTR(dumpreg, S_IRUGO, anx_ohio_dump_register, NULL),
		__ATTR(drole, S_IRUGO, anx_ohio_get_power_role, NULL),
		__ATTR(prole, S_IRUGO, anx_ohio_get_data_role,	NULL)
};

static DEVICE_ATTR(gpio, S_IWUSR, NULL, store_anx_ohio_set_gpio);

static struct attribute *balloon_info_attrs[] = {
	&dev_attr_gpio.attr,
	NULL
};

static const struct attribute_group balloon_info_group = {
	.name = "ohio",
	.attrs = balloon_info_attrs
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	int ret = 0;

	anx_printk(K_INFO, "ohio create system fs interface ...\n");

	for (i = 0; i < ARRAY_SIZE(anx_ohio_device_attrs); i++)
		if (device_create_file(dev, &anx_ohio_device_attrs[i]))
			goto error;

	ret = sysfs_create_group(&dev->kobj, &balloon_info_group);
	if (ret) {
		pr_err("fail sysfs\n");
		goto error;
	}

	anx_printk(K_INFO, "success\n");
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, &anx_ohio_device_attrs[i]);

	pr_err("%s %s: ohio Unable to create interface", LOG_TAG, __func__);
	return -EINVAL;
}

fs_initcall(ohio_init);
/*module_init(ohio_init);*/
/*module_exit(ohio_exit);*/

MODULE_DESCRIPTION("USB PD Ohio driver");
MODULE_AUTHOR("Zhentian Tang <ztang@analogixsemi.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.4");
