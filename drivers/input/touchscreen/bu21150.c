/*
 * Japan Display Inc. BU21150 touch screen driver.
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013-2015 Japan Display Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 *
 */
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/input/bu21150.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <asm/byteorder.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/list.h>

/* define */
#define DEVICE_NAME   "jdi-bu21150"
#define SYSFS_PROPERTY_PATH   "afe_properties"
#define REG_INT_RUN_ENB (0x00CE)
#define REG_SENS_START (0x0086)
#define REG_READ_DATA (0x0400)
#define MAX_FRAME_SIZE (8*1024+16)  /* byte */
#define SPI_HEADER_SIZE (3)
#define SPI_BITS_PER_WORD_READ (8)
#define SPI_BITS_PER_WORD_WRITE (8)
#define FRAME_HEADER_SIZE (16)  /* byte */
#define GPIO_LOW  (0)
#define GPIO_HIGH (1)
#define WAITQ_WAIT   (0)
#define WAITQ_WAKEUP (1)
#define TIMEOUT_SCALE       (50)
#define ESD_TEST_TIMER_MS	(10000)
#define BU21150_MIN_VOLTAGE_UV	2700000
#define BU21150_MAX_VOLTAGE_UV	3300000
#define BU21150_VDD_DIG_VOLTAGE_UV	1800000
#define BU21150_MAX_OPS_LOAD_UA	150000
#define BU21150_PIN_ENABLE_DELAY_US		1000
#define BU21150_PINCTRL_VALID_STATE_CNT		2

#define AFE_SCAN_DEFAULT			0x00
#define AFE_SCAN_SELF_CAP			0x01
#define AFE_SCAN_MUTUAL_CAP			0x02
#define AFE_SCAN_GESTURE_SELF_CAP		0x04
#define AFE_SCAN_GESTURE_MUTUAL_CAP		0x08

#define BU21150_PINCTRL_TS_ACT			"pmx_ts_active"
#define BU21150_PINCTRL_TS_SUS			"pmx_ts_suspend"
#define BU21150_PINCTRL_AFE_PWR_ACT		"afe_pwr_active"
#define BU21150_PINCTRL_AFE_PWR_SUS		"afe_pwr_suspend"
#define BU21150_PINCTRL_MOD_EN_ACT		"mod_en_active"
#define BU21150_PINCTRL_MOD_EN_SUS		"mod_en_suspend"
#define BU21150_PINCTRL_DISP_VSN_ACT		"disp_vsn_active"
#define BU21150_PINCTRL_DISP_VSN_SUS		"disp_vsn_suspend"
#define BU21150_PINCTRL_DDIC_RST_ACT		"ddic_rst_active"
#define BU21150_PINCTRL_DDIC_RST_SUS		"ddic_rst_suspend"

#define BU21150_LIST_MAX_FRAMES			20

struct bu21150_frame {
	struct list_head list;
	u8 frame[MAX_FRAME_SIZE];
	struct timeval tv;
};

/* struct */
struct bu21150_data {
	/* system */
	struct spi_device *client;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	struct pinctrl_state *afe_pwr_state_active;
	struct pinctrl_state *afe_pwr_state_suspend;
	struct pinctrl_state *mod_en_state_active;
	struct pinctrl_state *mod_en_state_suspend;
	struct pinctrl_state *disp_vsn_state_active;
	struct pinctrl_state *disp_vsn_state_suspend;
	struct pinctrl_state *ddic_rst_state_active;
	struct pinctrl_state *ddic_rst_state_suspend;
	struct notifier_block fb_notif;
	/* frame */
	struct bu21150_ioctl_get_frame_data req_get;
	struct bu21150_frame frame_list;
	u8 frame_count;
	struct bu21150_ioctl_get_frame_data frame_get;
	struct mutex mutex_frame;
	struct mutex mutex_wake;
	bool irq_enabled;
	/* frame work */
	u8 frame_work[MAX_FRAME_SIZE];
	struct bu21150_ioctl_get_frame_data frame_work_get;
	struct kobject *bu21150_obj;
	/* waitq */
	u8 frame_waitq_flag;
	wait_queue_head_t frame_waitq;
	/* reset */
	u8 reset_flag;
	/* timeout */
	u8 timeout_enb;
	u8 set_timer_flag;
	u8 timeout_flag;
	u32 timeout;
	/* spi */
	u8 spi_buf[MAX_FRAME_SIZE];
	/* power */
	struct regulator *vcc_ana;
	struct regulator *vcc_dig;
	/* dtsi */
	int irq_gpio;
	int rst_gpio;
	int afe_pwr_gpio;
	int mod_en_gpio;
	int disp_vsn_gpio;
	int ddic_rst_gpio;
	const char *panel_model;
	const char *afe_version;
	const char *pitch_type;
	const char *afe_vendor;
	u16 scan_mode;
	bool wake_up;
	bool timeout_enable;
	bool stay_awake;
	bool suspended;
	u8 unblock_flag;
	u8 force_unblock_flag;
	bool lcd_on;
};

struct ser_req {
	struct spi_message    msg;
	struct spi_transfer    xfer[2];
	u16 sample ____cacheline_aligned;
};

/* static function declaration */
static int bu21150_probe(struct spi_device *client);
static int bu21150_remove(struct spi_device *client);
static int bu21150_open(struct inode *inode, struct file *filp);
static int bu21150_release(struct inode *inode, struct file *filp);
static long bu21150_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg);
static long bu21150_ioctl_get_frame(unsigned long arg);
static long bu21150_ioctl_reset(unsigned long arg);
static long bu21150_ioctl_spi_read(unsigned long arg);
static long bu21150_ioctl_spi_write(unsigned long arg);
static long bu21150_ioctl_suspend(void);
static long bu21150_ioctl_resume(void);
static long bu21150_ioctl_unblock(void);
static long bu21150_ioctl_unblock_release(void);
static long bu21150_ioctl_set_timeout(unsigned long arg);
static long bu21150_ioctl_set_scan_mode(unsigned long arg);
static irqreturn_t bu21150_irq_thread(int irq, void *dev_id);
static void swap_2byte(unsigned char *buf, unsigned int size);
static int bu21150_read_register(u32 addr, u16 size, u8 *data);
static int bu21150_write_register(u32 addr, u16 size, u8 *data);
static void wake_up_frame_waitq(struct bu21150_data *ts);
static long wait_frame_waitq(struct bu21150_data *ts, u8 flag);
static int is_same_bu21150_ioctl_get_frame_data(
	struct bu21150_ioctl_get_frame_data *data1,
	struct bu21150_ioctl_get_frame_data *data2);
static void copy_frame(struct bu21150_data *ts);
#ifdef CHECK_SAME_FRAME
static void check_same_frame(struct bu21150_data *ts);
#endif
static bool parse_dtsi(struct device *dev, struct bu21150_data *ts);
static void get_frame_timer_init(void);
static void get_frame_timer_handler(unsigned long data);
static void get_frame_timer_delete(void);
static int bu21150_fb_suspend(struct device *dev);
static int bu21150_fb_early_resume(struct device *dev);
static int bu21150_fb_resume(struct device *dev);
static int fb_notifier_callback(struct notifier_block *self,
					unsigned long event, void *data);

/* static variables */
static struct spi_device *g_client_bu21150;
static int g_io_opened;
static struct timer_list get_frame_timer;

static ssize_t bu21150_wake_up_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	unsigned long state;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	if (!!state == ts->wake_up)
		return count;

	mutex_lock(&ts->mutex_wake);
	if (state == 0) {
		if (!ts->wake_up)
			goto exit;
		disable_irq_wake(ts->client->irq);
		device_init_wakeup(&ts->client->dev, false);
		ts->wake_up = false;
	} else {
		if (ts->wake_up)
			goto exit;
		device_init_wakeup(&ts->client->dev, true);
		enable_irq_wake(ts->client->irq);
		ts->wake_up = true;
	}

exit:
	mutex_unlock(&ts->mutex_wake);

	return count;
}

static ssize_t bu21150_wake_up_enable_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	return snprintf(buf, PAGE_SIZE, "%u", ts->wake_up);
}

static ssize_t bu21150_trigger_esd_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	disable_irq(ts->client->irq);
	msleep(ESD_TEST_TIMER_MS);
	enable_irq(ts->client->irq);

	return count;
}


static ssize_t bu21150_hallib_name_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	int index = snprintf(buf, PAGE_SIZE, "libafehal");

	if (ts->afe_vendor)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->afe_vendor);
	if (ts->panel_model)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->panel_model);

	if (ts->afe_version)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->afe_version);

	if (ts->pitch_type)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->pitch_type);

	return index;
}

static ssize_t bu21150_cfg_name_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	int index = snprintf(buf, PAGE_SIZE, "hbtp");

	if (ts->afe_vendor)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->afe_vendor);
	if (ts->panel_model)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->panel_model);
	if (ts->afe_version)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->afe_version);
	if (ts->pitch_type)
		index += snprintf(buf + index, PAGE_SIZE, "_%s",
							ts->pitch_type);

	return index;
}

static struct kobj_attribute bu21150_prop_attrs[] = {
	__ATTR(hallib_name, S_IRUGO, bu21150_hallib_name_show, NULL),
	__ATTR(cfg_name, S_IRUGO, bu21150_cfg_name_show, NULL),
	__ATTR(wake_up_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
					bu21150_wake_up_enable_show,
					bu21150_wake_up_enable_store),
	__ATTR(trigger_esd, (S_IRUGO | S_IWUSR | S_IWGRP),
					NULL,
					bu21150_trigger_esd_store),

};

static const struct of_device_id g_bu21150_psoc_match_table[] = {
	{	.compatible = "jdi,bu21150", },
	{ },
};

static const struct file_operations g_bu21150_fops = {
	.owner = THIS_MODULE,
	.open = bu21150_open,
	.release = bu21150_release,
	.unlocked_ioctl = bu21150_ioctl,
};

static struct miscdevice g_bu21150_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &g_bu21150_fops,
};

static const struct spi_device_id g_bu21150_device_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, g_bu21150_device_id);

static struct spi_driver g_bu21150_spi_driver = {
	.probe = bu21150_probe,
	.remove = bu21150_remove,
	.id_table = g_bu21150_device_id,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.bus = &spi_bus_type,
		.of_match_table = g_bu21150_psoc_match_table,
	},
};

module_spi_driver(g_bu21150_spi_driver);
MODULE_AUTHOR("Japan Display Inc");
MODULE_DESCRIPTION("JDI BU21150 Device Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:bu21150");

/* static functions */
static int reg_set_optimum_mode_check(struct regulator *reg, int load_ua)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_ua) : 0;
}

static int bu21150_pinctrl_init(struct bu21150_data *data)
{
	const char *statename;
	int rc;
	int state_cnt, i;
	bool pinctrl_state_act_found = false;
	bool pinctrl_state_sus_found = false;

	data->ts_pinctrl = devm_pinctrl_get(&(data->client->dev));
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		dev_err(&data->client->dev,
			"Target does not use pinctrl\n");
		rc = PTR_ERR(data->ts_pinctrl);
		goto error;
	}

	state_cnt = of_property_count_strings(data->client->dev.of_node,
							"pinctrl-names");
	if (state_cnt < BU21150_PINCTRL_VALID_STATE_CNT) {
		dev_err(&data->client->dev,
				"failed to read required pinctrl names\n");
		rc = -EINVAL;
		goto error;
	}

	for (i = 0; i < state_cnt; i++) {
		rc = of_property_read_string_index(data->client->dev.of_node,
					"pinctrl-names", i, &statename);
		if (rc) {
			dev_err(&data->client->dev,
				"failed to read pinctrl states by index\n");
			goto error;
		}

		if (!strcmp(statename, BU21150_PINCTRL_TS_ACT)) {
			data->gpio_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR_OR_NULL(data->gpio_state_active)) {
				dev_dbg(&data->client->dev,
					"Can not get ts default state\n");
				rc = PTR_ERR(data->gpio_state_active);
				goto error;
			}
			pinctrl_state_act_found = true;
		} else if (!strcmp(statename, BU21150_PINCTRL_TS_SUS)) {
			data->gpio_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR_OR_NULL(data->gpio_state_suspend)) {
				dev_dbg(&data->client->dev,
					"Can not get ts sleep state\n");
				rc = PTR_ERR(data->gpio_state_suspend);
				goto error;
			}
			pinctrl_state_sus_found = true;
		} else if (!strcmp(statename, BU21150_PINCTRL_AFE_PWR_ACT)) {
			data->afe_pwr_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->afe_pwr_state_active)) {
				dev_err(&data->client->dev,
					"Can not get pwr default state\n");
				rc = PTR_ERR(data->afe_pwr_state_active);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_AFE_PWR_SUS)) {
			data->afe_pwr_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->afe_pwr_state_suspend)) {
				dev_err(&data->client->dev,
					"Can not get pwr sleep state\n");
				rc = PTR_ERR(data->afe_pwr_state_suspend);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_MOD_EN_ACT)) {
			data->mod_en_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->mod_en_state_active)) {
				dev_err(&data->client->dev,
					"Can not get mod en default state\n");
				rc = PTR_ERR(data->mod_en_state_active);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_MOD_EN_SUS)) {
			data->mod_en_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->mod_en_state_suspend)) {
				dev_err(&data->client->dev,
					"Can not get mod en sleep state\n");
				rc = PTR_ERR(data->mod_en_state_suspend);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_DISP_VSN_ACT)) {
			data->disp_vsn_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->disp_vsn_state_active)) {
				dev_err(&data->client->dev,
					"Can not get disp vsn default state\n");
				rc = PTR_ERR(data->disp_vsn_state_active);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_DISP_VSN_SUS)) {
			data->disp_vsn_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->disp_vsn_state_suspend)) {
				dev_err(&data->client->dev,
					"Can not get disp vsn sleep state\n");
				rc = PTR_ERR(data->disp_vsn_state_suspend);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_DDIC_RST_ACT)) {
			data->ddic_rst_state_active
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->ddic_rst_state_active)) {
				dev_err(&data->client->dev,
					"Can not get DDIC rst default state\n");
				rc = PTR_ERR(data->ddic_rst_state_active);
				goto error;
			}
		} else if (!strcmp(statename, BU21150_PINCTRL_DDIC_RST_SUS)) {
			data->ddic_rst_state_suspend
				= pinctrl_lookup_state(data->ts_pinctrl,
								statename);
			if (IS_ERR(data->ddic_rst_state_suspend)) {
				dev_err(&data->client->dev,
					"Can not get DDIC rst sleep state\n");
				rc = PTR_ERR(data->ddic_rst_state_suspend);
				goto error;
			}
		} else {
			dev_err(&data->client->dev, "invalid pinctrl state\n");
			rc = -EINVAL;
			goto error;
		}
	}

	if (!pinctrl_state_act_found || !pinctrl_state_sus_found) {
		dev_err(&data->client->dev,
					"missing required pinctrl states\n");
		rc = -EINVAL;
		goto error;
	}

	return 0;

error:
	data->ts_pinctrl = NULL;
	return rc;
}

static int bu21150_pinctrl_select(struct bu21150_data *data, bool on)
{
	struct pinctrl_state *pins_state;
	int ret = 0;

	pins_state = on ? data->gpio_state_active : data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(data->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(&data->client->dev,
				"can not set %s pins\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else {
		dev_err(&data->client->dev,
			"not a valid '%s' pinstate\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
		return -EINVAL;
	}

	return ret;
}

static int bu21150_pinctrl_enable(struct bu21150_data *ts, bool on)
{
	int rc = 0;

	if (!on)
		goto pinctrl_suspend;

	rc = bu21150_pinctrl_select(ts, true);
	if (rc < 0)
		return -EINVAL;

	if (ts->afe_pwr_state_active) {
		rc = pinctrl_select_state(ts->ts_pinctrl,
					ts->afe_pwr_state_active);
		if (rc) {
			dev_err(&ts->client->dev, "can not set afe pwr pins\n");
			goto err_afe_pwr_pinctrl_enable;
		}

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	if (ts->mod_en_state_active) {
		rc = pinctrl_select_state(ts->ts_pinctrl,
					ts->mod_en_state_active);
		if (rc) {
			dev_err(&ts->client->dev,
					"can not set mod en pins\n");
			goto err_mod_en_pinctrl_enable;
		}

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	if (ts->disp_vsn_state_active) {
		rc = pinctrl_select_state(ts->ts_pinctrl,
					ts->disp_vsn_state_active);
		if (rc) {
			dev_err(&ts->client->dev,
					"can not set disp vsn pins\n");
			goto err_disp_vsn_pinctrl_enable;
		}

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	if (ts->ddic_rst_state_active) {
		rc = pinctrl_select_state(ts->ts_pinctrl,
					ts->ddic_rst_state_active);
		if (rc) {
			dev_err(&ts->client->dev,
					"can not set DDIC rst pins\n");
			goto err_ddic_rst_pinctrl_enable;
		}
	}

	return 0;

pinctrl_suspend:
	if (ts->ddic_rst_state_suspend)
		pinctrl_select_state(ts->ts_pinctrl,
						ts->ddic_rst_state_suspend);
err_ddic_rst_pinctrl_enable:
	if (ts->disp_vsn_state_suspend)
		pinctrl_select_state(ts->ts_pinctrl,
						ts->disp_vsn_state_suspend);
err_disp_vsn_pinctrl_enable:
	if (ts->mod_en_state_suspend)
		pinctrl_select_state(ts->ts_pinctrl, ts->mod_en_state_suspend);
err_mod_en_pinctrl_enable:
	if (ts->afe_pwr_state_suspend)
		pinctrl_select_state(ts->ts_pinctrl, ts->afe_pwr_state_suspend);
err_afe_pwr_pinctrl_enable:
	bu21150_pinctrl_select(ts, false);

	return rc;
}

static int bu21150_gpio_enable(struct bu21150_data *ts, bool on)
{
	int rc = 0;

	if (!on)
		goto gpio_disable;

	/* set reset */
	rc = gpio_request(ts->rst_gpio, "bu21150_ts_reset");
	if (rc) {
		pr_err("%s: reset gpio_request failed\n", __func__);
		return -EINVAL;
	}

	gpio_direction_output(ts->rst_gpio, GPIO_LOW);

	/* Panel and AFE Power on sequence */
	if (of_find_property(ts->client->dev.of_node, "afe_pwr", NULL)) {
		rc = gpio_request(ts->afe_pwr_gpio, "afe_pwr");
		if (rc) {
			pr_err("%s: afe power gpio request failed\n", __func__);
			goto err_afe_pwr_gpio_enable;
		}
		gpio_direction_output(ts->afe_pwr_gpio, 1);
		gpio_set_value(ts->afe_pwr_gpio, 1);

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	if (of_find_property(ts->client->dev.of_node, "mod_en", NULL)) {
		rc = gpio_request(ts->mod_en_gpio, "mod_en");
		if (rc) {
			pr_err("%s: mod enablement gpio request failed\n",
								__func__);
			goto err_mod_en_gpio_enable;
		}
		gpio_direction_output(ts->mod_en_gpio, 1);
		gpio_set_value(ts->mod_en_gpio, 1);

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	if (of_find_property(ts->client->dev.of_node, "disp_vsn", NULL)) {
		rc = gpio_request(ts->disp_vsn_gpio, "disp_vsn");
		if (rc) {
			pr_err("%s: disp_vsn gpio request failed\n", __func__);
			goto err_disp_vsn_gpio_enable;
		}
		gpio_direction_output(ts->disp_vsn_gpio, 1);
		gpio_set_value(ts->disp_vsn_gpio, 1);

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	if (of_find_property(ts->client->dev.of_node, "ddic_rst", NULL)) {
		rc = gpio_request(ts->disp_vsn_gpio, "ddic_rst");
		if (rc) {
			pr_err("%s: DDIC rst gpio request failed\n", __func__);
			goto err_ddic_rst_gpio_enable;
		}
		gpio_direction_output(ts->ddic_rst_gpio, 1);
		gpio_set_value(ts->ddic_rst_gpio, 1);

		/*
		 * Wait among pin enablements to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);
	}

	rc = gpio_request(ts->irq_gpio, "bu21150_ts_int");
	if (rc) {
		pr_err("%s: IRQ gpio_request failed\n", __func__);
		goto err_irq_gpio_enable;
	}
	gpio_direction_input(ts->irq_gpio);

	return 0;

gpio_disable:
	gpio_free(ts->irq_gpio);
err_irq_gpio_enable:
	if (of_find_property(ts->client->dev.of_node, "ddic_rst", NULL))
		gpio_free(ts->ddic_rst_gpio);
err_ddic_rst_gpio_enable:
	if (of_find_property(ts->client->dev.of_node, "disp_vsn", NULL))
		gpio_free(ts->disp_vsn_gpio);
err_disp_vsn_gpio_enable:
	if (of_find_property(ts->client->dev.of_node, "mod_en", NULL))
		gpio_free(ts->mod_en_gpio);
err_mod_en_gpio_enable:
	if (of_find_property(ts->client->dev.of_node, "afe_pwr", NULL))
		gpio_free(ts->afe_pwr_gpio);
err_afe_pwr_gpio_enable:
	gpio_free(ts->rst_gpio);

	return rc;
}

static int bu21150_pin_enable(struct bu21150_data *ts, bool on)
{
	int rc = 0;

	if (!on)
		goto pin_disable;

	if (ts->ts_pinctrl)
		rc = bu21150_pinctrl_enable(ts, true);
	else
		rc = bu21150_gpio_enable(ts, true);

	return rc;

pin_disable:
	if (ts->ts_pinctrl)
		bu21150_pinctrl_enable(ts, false);
	else
		bu21150_gpio_enable(ts, false);

	return rc;
}

static int bu21150_power_enable(struct bu21150_data *ts, bool on)
{
	int rc = 0;

	if (!on)
		goto power_disable;

	if (regulator_count_voltages(ts->vcc_ana) > 0) {
		rc = regulator_set_voltage(ts->vcc_ana,
			BU21150_MIN_VOLTAGE_UV, BU21150_MAX_VOLTAGE_UV);
		if (rc) {
			dev_err(&ts->client->dev,
				"regulator vcc_ana set_vtg failed rc=%d\n", rc);
			return rc;
		}
	}

	rc = reg_set_optimum_mode_check(ts->vcc_ana,
						BU21150_MAX_OPS_LOAD_UA);
	if (rc < 0) {
		dev_err(&ts->client->dev,
			"Regulator vcc_ana set_opt failed rc=%d\n", rc);
		goto err_set_vcc_ana_opt_mode;
	}

	rc = regulator_enable(ts->vcc_ana);
	if (rc) {
		dev_err(&ts->client->dev,
			"Regulator vcc_ana enable failed rc=%d\n", rc);
		goto err_enable_vcc_ana;
	}

	if (regulator_count_voltages(ts->vcc_dig) > 0) {
		rc = regulator_set_voltage(ts->vcc_dig,
						BU21150_VDD_DIG_VOLTAGE_UV,
						BU21150_VDD_DIG_VOLTAGE_UV);
		if (rc) {
			dev_err(&ts->client->dev,
				"regulator vcc_dig set_vtg failed rc=%d\n", rc);
			goto err_set_vcc_dig_voltage;
		}
	}

	rc = reg_set_optimum_mode_check(ts->vcc_dig,
						BU21150_MAX_OPS_LOAD_UA);
	if (rc < 0) {
		dev_err(&ts->client->dev,
			"Regulator vcc_dig set_opt failed rc=%d\n", rc);
		goto err_set_vcc_dig_opt_mode;
	}

	rc = regulator_enable(ts->vcc_dig);
	if (rc) {
		dev_err(&ts->client->dev,
			"Regulator vcc_dig enable failed rc=%d\n", rc);
		goto err_enable_vcc_dig;
	}

	return 0;

power_disable:
	regulator_disable(ts->vcc_dig);
err_enable_vcc_dig:
	reg_set_optimum_mode_check(ts->vcc_dig, 0);
err_set_vcc_dig_opt_mode:
	if (regulator_count_voltages(ts->vcc_dig) > 0)
		regulator_set_voltage(ts->vcc_dig, 0,
					BU21150_VDD_DIG_VOLTAGE_UV);
err_set_vcc_dig_voltage:
	regulator_disable(ts->vcc_ana);
err_enable_vcc_ana:
	reg_set_optimum_mode_check(ts->vcc_ana, 0);
err_set_vcc_ana_opt_mode:
	if (regulator_count_voltages(ts->vcc_ana) > 0)
		regulator_set_voltage(ts->vcc_ana, 0, BU21150_MAX_VOLTAGE_UV);

	return rc;
}

static int bu21150_regulator_config(struct bu21150_data *ts, bool enable)
{
	int rc = 0;

	if (!enable)
		goto regulator_release;

	ts->vcc_ana = regulator_get(&ts->client->dev, "vdd_ana");
	if (IS_ERR_OR_NULL(ts->vcc_ana)) {
		rc = PTR_ERR(ts->vcc_ana);
		dev_err(&ts->client->dev,
			"Regulator get failed vcc_ana rc=%d\n", rc);
		return rc;
	}

	ts->vcc_dig = regulator_get(&ts->client->dev, "vdd_dig");
	if (IS_ERR_OR_NULL(ts->vcc_dig)) {
		rc = PTR_ERR(ts->vcc_dig);
		dev_err(&ts->client->dev,
			"Regulator get failed vcc_dig rc=%d\n", rc);
		goto err_get_vdd_dig;
	}

	return 0;

regulator_release:
	regulator_put(ts->vcc_dig);
err_get_vdd_dig:
	regulator_put(ts->vcc_ana);

	return rc;
}

static int bu21150_probe(struct spi_device *client)
{
	struct bu21150_data *ts;
	int rc, i;

	ts = kzalloc(sizeof(struct bu21150_data), GFP_KERNEL);
	if (!ts) {
		dev_err(&client->dev, "Out of memory\n");
		return -ENOMEM;
	}

	/* parse dtsi */
	if (!parse_dtsi(&client->dev, ts)) {
		dev_err(&client->dev, "Invalid dtsi\n");
		rc = -EINVAL;
		goto err_parse_dt;
	}

	g_client_bu21150 = client;
	ts->client = client;

	rc = bu21150_pinctrl_init(ts);
	if (rc) {
		dev_err(&client->dev, "Pinctrl init failed\n");
		goto err_parse_dt;
	}

	rc = bu21150_regulator_config(ts, true);
	if (rc) {
		dev_err(&client->dev, "Failed to get power rail\n");
		goto err_regulator_config;
	}

	rc = bu21150_power_enable(ts, true);
	if (rc) {
		dev_err(&client->dev, "Power enablement failed\n");
		goto err_power_enable;
	}

	rc = bu21150_pin_enable(ts, true);
	if (rc) {
		dev_err(&client->dev, "Pin enable failed\n");
		goto err_pin_enable;
	}

	INIT_LIST_HEAD(&ts->frame_list.list);

	mutex_init(&ts->mutex_frame);
	init_waitqueue_head(&(ts->frame_waitq));

	ts->fb_notif.notifier_call = fb_notifier_callback;
	rc = fb_register_client(&ts->fb_notif);
	if (rc) {
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
									rc);
		goto err_register_fb_notif;
	}

	rc = misc_register(&g_bu21150_misc_device);
	if (rc) {
		dev_err(&client->dev, "Failed to register misc device\n");
		goto err_register_misc;
	}

	dev_set_drvdata(&client->dev, ts);

	ts->bu21150_obj = kobject_create_and_add(SYSFS_PROPERTY_PATH, NULL);
	if (!ts->bu21150_obj) {
		dev_err(&client->dev, "unable to create kobject\n");
		goto err_create_and_add_kobj;
	}

	for (i = 0; i < ARRAY_SIZE(bu21150_prop_attrs); i++) {
		rc = sysfs_create_file(ts->bu21150_obj,
						&bu21150_prop_attrs[i].attr);
		if (rc) {
			dev_err(&client->dev, "failed to create attributes\n");
			goto err_create_sysfs;
		}
	}

	if (ts->wake_up)
		device_init_wakeup(&client->dev, ts->wake_up);

	mutex_init(&ts->mutex_wake);

	return 0;

err_create_sysfs:
	for (i--; i >= 0; i--)
		sysfs_remove_file(ts->bu21150_obj, &bu21150_prop_attrs[i].attr);
	kobject_put(ts->bu21150_obj);
err_create_and_add_kobj:
	misc_deregister(&g_bu21150_misc_device);
err_register_misc:
	fb_unregister_client(&ts->fb_notif);
err_register_fb_notif:
	mutex_destroy(&ts->mutex_frame);
	bu21150_pin_enable(ts, false);
err_pin_enable:
	bu21150_power_enable(ts, false);
err_power_enable:
	bu21150_regulator_config(ts, false);
err_regulator_config:
	if (ts->ts_pinctrl)
		devm_pinctrl_put(ts->ts_pinctrl);
err_parse_dt:
	kfree(ts);
	return rc;
}

static int bu21150_fb_suspend(struct device *dev)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	struct spi_device *client = ts->client;
	int rc;
	u8 buf1[2] = {0x00, 0x00};
	u8 buf2[2] = {0x04, 0x00};
	struct bu21150_frame *temp;
	struct list_head *pos, *n;

	if (ts->suspended)
		return 0;

	rc = bu21150_write_register(REG_SENS_START, (u16)sizeof(buf1), buf1);
	if (rc)
		dev_err(&ts->client->dev,
			"%s: failed to disable sensing (%d)\n", __func__, rc);

	ts->timeout_enb = 0;
	get_frame_timer_delete();

	ts->unblock_flag = 1;
	/* wake up */
	wake_up_frame_waitq(ts);

	/* empty list */
	mutex_lock(&ts->mutex_frame);
	list_for_each_safe(pos, n, &ts->frame_list.list) {
		 temp = list_entry(pos, struct bu21150_frame, list);
		 list_del(pos);
		 kfree(temp);
	}
	ts->frame_count = 0;
	mutex_unlock(&ts->mutex_frame);

	rc = bu21150_write_register(REG_INT_RUN_ENB, (u16)sizeof(buf2), buf2);
	if (rc)
		dev_err(&ts->client->dev,
			"%s: failed to write to REG_INT_RUN_ENB (%d)\n",
			__func__, rc);

	if (!ts->wake_up) {
		disable_irq(client->irq);
		rc = bu21150_pin_enable(ts, false);
		if (rc) {
			dev_err(&ts->client->dev,
				"failed to disable GPIO pins\n");
			return rc;
		}

		rc = bu21150_power_enable(ts, false);
		if (rc) {
			dev_err(&ts->client->dev,
				"failed to disable panel power\n");
			goto err_power_disable;
		}
	}

	ts->suspended = true;

	return 0;

err_power_disable:
	bu21150_pin_enable(ts, true);

	return rc;
}

static int bu21150_fb_early_resume(struct device *dev)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	int rc;
	u8 buf[2] = {0x01, 0x00};
	struct bu21150_frame *temp;
	struct list_head *pos, *n;

	if (!ts->suspended)
		return 0;

	if (!ts->wake_up) {
		rc = bu21150_power_enable(ts, true);
		if (rc) {
			dev_err(&ts->client->dev,
				"failed to enable panel power\n");
			return rc;
		}

		/*
		 * Wait before pin enablement to comply
		 * with hardware requirement.
		 */
		usleep(BU21150_PIN_ENABLE_DELAY_US);

		rc = bu21150_pin_enable(ts, true);
		if (rc) {
			dev_err(&ts->client->dev,
				"failed to enable panel power\n");
			goto err_pin_enable;
		}
	}

	ts->timeout_enb = 0;
	get_frame_timer_delete();

	rc = bu21150_write_register(REG_INT_RUN_ENB, (u16)sizeof(buf), buf);
	if (rc)
		dev_err(&ts->client->dev,
			"%s: failed to write %d to REG_INT_RUN_ENB (%d)\n",
			__func__, buf[0], rc);
	/* empty list */
	mutex_lock(&ts->mutex_frame);
	list_for_each_safe(pos, n, &ts->frame_list.list) {
		 temp = list_entry(pos, struct bu21150_frame, list);
		 list_del(pos);
		 kfree(temp);
	}
	ts->frame_count = 0;
	mutex_unlock(&ts->mutex_frame);

	ts->suspended = false;

	return 0;

err_pin_enable:
	bu21150_power_enable(ts, false);

	return rc;
}

static int bu21150_fb_resume(struct device *dev)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	int rc;
	u8 buf[2] = {0x01, 0x00};

	buf[0] = 0x03;
	rc = bu21150_write_register(REG_INT_RUN_ENB, (u16)sizeof(buf), buf);
	if (rc)
		dev_err(&ts->client->dev,
			"%s: failed to write %d to REG_INT_RUN_ENB (%d)\n",
			__func__, buf[0], rc);
	usleep(1000);

	return 0;
}

static int fb_notifier_callback(struct notifier_block *self,
					unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	bool cont_splash = msm_fb_get_cont_splash();
	struct bu21150_data *ts =
			container_of(self, struct bu21150_data, fb_notif);
	struct device *dev;

	if (evdata && evdata->data && ts && ts->client) {
		dev = &ts->client->dev;
		blank = evdata->data;
		if (event == FB_EARLY_EVENT_BLANK) {
			if (*blank == FB_BLANK_UNBLANK) {
				ts->lcd_on = true;
				if (!cont_splash)
					bu21150_fb_early_resume(dev);
			} else if (*blank == FB_BLANK_POWERDOWN) {
				ts->lcd_on = false;
			}
		} else if (event == FB_R_EARLY_EVENT_BLANK) {
			if (*blank == FB_BLANK_UNBLANK) {
				ts->lcd_on = false;
				if (!cont_splash)
					bu21150_fb_early_resume(dev);
			} else if (*blank == FB_BLANK_POWERDOWN) {
				ts->lcd_on = true;
			}
		} else if (event == FB_EVENT_BLANK && *blank ==
							FB_BLANK_POWERDOWN) {
			if (!cont_splash)
				bu21150_fb_suspend(dev);
		} else if (event == FB_EVENT_BLANK && *blank ==
							FB_BLANK_UNBLANK) {
			bu21150_fb_resume(dev);
		}
	}

	return 0;
}

static void get_frame_timer_init(void)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	if (ts->set_timer_flag == 1) {
		del_timer_sync(&get_frame_timer);
		ts->set_timer_flag = 0;
	}

	if (ts->timeout > 0) {
		ts->set_timer_flag = 1;
		ts->timeout_flag = 0;

		init_timer(&get_frame_timer);

		get_frame_timer.expires  = jiffies + ts->timeout;
		get_frame_timer.data     = (unsigned long)jiffies;
		get_frame_timer.function = get_frame_timer_handler;

		add_timer(&get_frame_timer);
	}
}

static void get_frame_timer_handler(unsigned long data)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	ts->timeout_flag = 1;
	/* wake up */
	wake_up_frame_waitq(ts);
}

static void get_frame_timer_delete(void)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	if (ts->set_timer_flag == 1) {
		ts->set_timer_flag = 0;
		del_timer_sync(&get_frame_timer);
	}
}

static int bu21150_remove(struct spi_device *client)
{
	struct bu21150_data *ts = spi_get_drvdata(client);
	int i;

	mutex_destroy(&ts->mutex_wake);
	if (ts->wake_up)
		device_init_wakeup(&client->dev, 0);

	for (i = 0; i < ARRAY_SIZE(bu21150_prop_attrs); i++)
		sysfs_remove_file(ts->bu21150_obj,
					&bu21150_prop_attrs[i].attr);
	kobject_put(ts->bu21150_obj);
	fb_unregister_client(&ts->fb_notif);
	misc_deregister(&g_bu21150_misc_device);
	bu21150_power_enable(ts, false);
	bu21150_regulator_config(ts, false);
	mutex_destroy(&ts->mutex_frame);
	bu21150_pin_enable(ts, false);
	kfree(ts);

	return 0;
}

static int bu21150_open(struct inode *inode, struct file *filp)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	if (g_io_opened) {
		pr_err("%s: g_io_opened not zero.\n", __func__);
		return -EBUSY;
	}
	++g_io_opened;

	get_frame_timer_delete();
	ts->reset_flag = 0;
	ts->set_timer_flag = 0;
	ts->timeout_flag = 0;
	ts->timeout_enb = 0;
	ts->unblock_flag = 0;
	ts->force_unblock_flag = 0;
	ts->scan_mode = AFE_SCAN_MUTUAL_CAP;
	memset(&(ts->req_get), 0, sizeof(struct bu21150_ioctl_get_frame_data));
	/* set default value. */
	ts->req_get.size = FRAME_HEADER_SIZE;
	memset(&(ts->frame_get), 0,
		sizeof(struct bu21150_ioctl_get_frame_data));
	memset(&(ts->frame_work_get), 0,
		sizeof(struct bu21150_ioctl_get_frame_data));

	return 0;
}

static int bu21150_release(struct inode *inode, struct file *filp)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	struct spi_device *client = ts->client;

	if (!g_io_opened) {
		pr_err("%s: !g_io_opened\n", __func__);
		return -ENOTTY;
	}
	--g_io_opened;

	if (g_io_opened < 0)
		g_io_opened = 0;

	wake_up_frame_waitq(ts);
	if (ts->timeout_enb)
		get_frame_timer_delete();

	if (ts->irq_enabled) {
		free_irq(client->irq, ts);
		ts->irq_enabled = false;
	}

	return 0;
}

static long bu21150_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	long ret;

	switch (cmd) {
	case BU21150_IOCTL_CMD_GET_FRAME:
		ret = bu21150_ioctl_get_frame(arg);
		return ret;
	case BU21150_IOCTL_CMD_RESET:
		ret = bu21150_ioctl_reset(arg);
		return ret;
	case BU21150_IOCTL_CMD_SPI_READ:
		ret = bu21150_ioctl_spi_read(arg);
		return ret;
	case BU21150_IOCTL_CMD_SPI_WRITE:
		ret = bu21150_ioctl_spi_write(arg);
		return ret;
	case BU21150_IOCTL_CMD_UNBLOCK:
		ret = bu21150_ioctl_unblock();
		return ret;
	case BU21150_IOCTL_CMD_UNBLOCK_RELEASE:
		ret = bu21150_ioctl_unblock_release();
		return ret;
	case BU21150_IOCTL_CMD_SUSPEND:
		ret = bu21150_ioctl_suspend();
		return ret;
	case BU21150_IOCTL_CMD_RESUME:
		ret = bu21150_ioctl_resume();
		return ret;
	case BU21150_IOCTL_CMD_SET_TIMEOUT:
		ret = bu21150_ioctl_set_timeout(arg);
		return ret;
	case BU21150_IOCTL_CMD_SET_SCAN_MODE:
		ret = bu21150_ioctl_set_scan_mode(arg);
		return ret;
	default:
		pr_err("%s: cmd unknown.\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static long bu21150_ioctl_get_frame(unsigned long arg)
{
	long ret;
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	void __user *argp = (void __user *)arg;
	struct bu21150_ioctl_get_frame_data data;
	u32 frame_size;

	if (arg == 0) {
		pr_err("%s: arg == 0.\n", __func__);
		return -EINVAL;
	}
	if (copy_from_user(&data, argp,
		sizeof(struct bu21150_ioctl_get_frame_data))) {
		pr_err("%s: Failed to copy_from_user().\n", __func__);
		return -EFAULT;
	}
	if (data.buf == 0 || data.size == 0 ||
		MAX_FRAME_SIZE < data.size || data.tv == 0) {
		pr_err("%s: data.buf == 0 ...\n", __func__);
		return -EINVAL;
	}


	if (!ts->irq_enabled) {
		ret = request_threaded_irq(ts->client->irq, NULL,
					bu21150_irq_thread,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					ts->client->dev.driver->name, ts);
		if (ret) {
			dev_err(&ts->client->dev, "Failed to register interrupt\n");
			return ret;
		}

		ts->irq_enabled = true;
	}

	if (ts->timeout_enb == 1)
		get_frame_timer_init();

	do {
		ts->req_get = data;
		ret = wait_frame_waitq(ts, data.keep_block_flag);
		ts->unblock_flag = 0;
		if (ret != 0)
			return ret;
	} while (!is_same_bu21150_ioctl_get_frame_data(&data,
				&(ts->frame_get)));

	if (ts->timeout_enb == 1)
		get_frame_timer_delete();

	/* copy frame */
	mutex_lock(&ts->mutex_frame);
	frame_size = ts->frame_get.size;

	if (!list_empty(&ts->frame_list.list)) {
		struct bu21150_frame *temp;

		temp = list_first_entry(&ts->frame_list.list,
				struct bu21150_frame, list);
		if (copy_to_user(data.buf, temp->frame, frame_size)) {
			mutex_unlock(&ts->mutex_frame);
			pr_err("%s: Failed to copy_to_user().\n", __func__);
			return -EFAULT;
		}
		if (copy_to_user(data.tv, &(temp->tv),
				sizeof(struct timeval))) {
			mutex_unlock(&ts->mutex_frame);
			pr_err("%s: Failed to copy_to_user().\n", __func__);
			return -EFAULT;
		}
		list_del(&temp->list);
		kfree(temp);
		ts->frame_count--;
	} else {
		pr_debug("%s: no frame!!! unblock\n", __func__);
		mutex_unlock(&ts->mutex_frame);
		return BU21150_UNBLOCK;
	}

	if (!list_empty(&ts->frame_list.list))
		wake_up_frame_waitq(ts);

	mutex_unlock(&ts->mutex_frame);

	return 0;
}

static long bu21150_ioctl_reset(unsigned long reset)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	int rc;

	if (!(reset == BU21150_RESET_LOW || reset == BU21150_RESET_HIGH)) {
		pr_err("%s: arg unknown.\n", __func__);
		return -EINVAL;
	}

	if (!ts->ts_pinctrl) {
		gpio_set_value(ts->rst_gpio, reset);
		goto reset_exit;
	}

	if (reset == BU21150_RESET_LOW) {
		disable_irq(ts->client->irq);
		rc = bu21150_pinctrl_select(ts, false);
		if (rc) {
			pr_err("%s: failed to pull low reset line\n",
								__func__);
			enable_irq(ts->client->irq);
			return rc;
		}
	} else if (reset == BU21150_RESET_HIGH) {
		rc = bu21150_pinctrl_select(ts, true);
		if (rc) {
			pr_err("%s: failed to pull high reset line\n",
								__func__);
			return rc;
		}
		enable_irq(ts->client->irq);
	}

reset_exit:
	ts->frame_waitq_flag = WAITQ_WAIT;
	if (reset == BU21150_RESET_LOW)
		ts->reset_flag = 1;

	return 0;
}

static long bu21150_ioctl_spi_read(unsigned long arg)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	void __user *argp = (void __user *)arg;
	struct bu21150_ioctl_spi_data data;
	int ret;

	if (arg == 0) {
		pr_err("%s: arg == 0.\n", __func__);
		return -EINVAL;
	}
	if (copy_from_user(&data, argp,
		sizeof(struct bu21150_ioctl_spi_data))) {
		pr_err("%s: Failed to copy_from_user().\n", __func__);
		return -EFAULT;
	}
	if (data.buf == 0 || data.count == 0 ||
		MAX_FRAME_SIZE < data.count) {
		pr_err("%s: data.buf == 0 ...\n", __func__);
		return -EINVAL;
	}

	ret = bu21150_read_register(data.addr, data.count, ts->spi_buf);
	if (ret) {
		pr_err("%s: Failed to read register (%d).\n", __func__, ret);
		return ret;
	}

	if (copy_to_user(data.buf, ts->spi_buf, data.count)) {
		pr_err("%s: Failed to copy_to_user().\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static long bu21150_ioctl_spi_write(unsigned long arg)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	void __user *argp = (void __user *)arg;
	struct bu21150_ioctl_spi_data data;
	unsigned int afe_active_mode = AFE_SCAN_SELF_CAP | AFE_SCAN_MUTUAL_CAP;
	unsigned int afe_gesture_mode = AFE_SCAN_GESTURE_SELF_CAP |
						AFE_SCAN_GESTURE_MUTUAL_CAP;
	bool valid_op;
	int ret;

	if (arg == 0) {
		pr_err("%s: arg == 0.\n", __func__);
		return -EINVAL;
	}
	if (copy_from_user(&data, argp,
		sizeof(struct bu21150_ioctl_spi_data))) {
		pr_err("%s: Failed to copy_from_user().\n", __func__);
		return -EFAULT;
	}

	valid_op = (data.next_mode == AFE_SCAN_DEFAULT) ||
			((data.next_mode & afe_active_mode) && ts->lcd_on) ||
			((data.next_mode & afe_gesture_mode) && !ts->lcd_on);
	if (!valid_op) {
		pr_err("%s: AFE scan mode(%d) and LCD state(%d) conflict\n",
					__func__, data.next_mode, ts->lcd_on);
		return -EINVAL;
	}

	if (data.buf == 0 || data.count == 0 ||
		MAX_FRAME_SIZE < data.count) {
		pr_err("%s: data.buf == 0 ...\n", __func__);
		return -EINVAL;
	}
	if (copy_from_user(ts->spi_buf, data.buf, data.count)) {
		pr_err("%s: Failed to copy_from_user()..\n", __func__);
		return -EFAULT;
	}

	ret = bu21150_write_register(data.addr, data.count, ts->spi_buf);
	if (ret)
		pr_err("%s: Failed to write register (%d).\n", __func__, ret);

	return ret;
}

static long bu21150_ioctl_unblock(void)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	ts->force_unblock_flag = 1;
	/* wake up */
	wake_up_frame_waitq(ts);

	return 0;
}

static long bu21150_ioctl_unblock_release(void)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	ts->force_unblock_flag = 0;

	ts->frame_waitq_flag = WAITQ_WAIT;

	return 0;
}

static long bu21150_ioctl_suspend(void)
{
	bu21150_ioctl_unblock();

	return 0;
}

static long bu21150_ioctl_resume(void)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);

	ts->force_unblock_flag = 0;

	enable_irq(ts->client->irq);

	return 0;
}

static long bu21150_ioctl_set_timeout(unsigned long arg)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	void __user *argp = (void __user *)arg;
	struct bu21150_ioctl_timeout_data data;

	if (!ts->timeout_enable)
		return 0;

	if (copy_from_user(&data, argp,
		sizeof(struct bu21150_ioctl_timeout_data))) {
		pr_err("%s: Failed to copy_from_user().\n", __func__);
		return -EFAULT;
	}

	ts->timeout_enb = data.timeout_enb;
	if (data.timeout_enb == 1) {
		ts->timeout = (unsigned int)(data.report_interval_us
			* TIMEOUT_SCALE * HZ / 1000000);
	} else {
		get_frame_timer_delete();
	}

	return 0;
}

static long bu21150_ioctl_set_scan_mode(unsigned long arg)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&ts->scan_mode, argp,
		sizeof(u16))) {
		pr_err("%s: Failed to copy_from_user().\n", __func__);
		return -EFAULT;
	}

	mutex_lock(&ts->mutex_wake);

	if (ts->stay_awake && ts->wake_up) {
		pm_relax(&ts->client->dev);
		ts->stay_awake = false;
	}

	mutex_unlock(&ts->mutex_wake);

	return 0;
}

static irqreturn_t bu21150_irq_thread(int irq, void *dev_id)
{
	struct bu21150_data *ts = dev_id;
	u8 *psbuf = (u8 *)ts->frame_work;
	int ret;

	mutex_lock(&ts->mutex_wake);

	if (!ts->stay_awake && ts->wake_up &&
			ts->scan_mode == AFE_SCAN_GESTURE_SELF_CAP) {
		pm_stay_awake(&ts->client->dev);
		ts->stay_awake = true;
	}

	mutex_unlock(&ts->mutex_wake);

	/* get frame */
	ts->frame_work_get = ts->req_get;
	ret = bu21150_read_register(REG_READ_DATA,
			ts->frame_work_get.size, psbuf);
	if (ret) {
		pr_err("%s: failed to read frame (%d)\n", __func__, ret);
		goto err_read_reg;
	}

	if (ts->reset_flag == 0) {
#ifdef CHECK_SAME_FRAME
		check_same_frame(ts);
#endif
		copy_frame(ts);
		wake_up_frame_waitq(ts);
	} else {
		ts->reset_flag = 0;
	}

err_read_reg:
	return IRQ_HANDLED;
}

static int bu21150_read_register(u32 addr, u16 size, u8 *data)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	struct spi_device *client = ts->client;
	struct ser_req *req;
	int ret;
	u8 *input;
	u8 *output;

	input = kzalloc(sizeof(u8)*(size)+SPI_HEADER_SIZE, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	output = kzalloc(sizeof(u8)*(size)+SPI_HEADER_SIZE, GFP_KERNEL);
	if (!output) {
		ret = -ENOMEM;
		goto err_alloc_output;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto err_alloc_req;
	}

	/* set header */
	input[0] = 0x03;                 /* read command */
	input[1] = (addr & 0xFF00) >> 8; /* address hi */
	input[2] = (addr & 0x00FF) >> 0; /* address lo */

	/* read data */
	spi_message_init(&req->msg);
	req->xfer[0].tx_buf = input;
	req->xfer[0].rx_buf = output;
	req->xfer[0].len = size+SPI_HEADER_SIZE;
	req->xfer[0].cs_change = 0;
	req->xfer[0].bits_per_word = SPI_BITS_PER_WORD_READ;
	spi_message_add_tail(&req->xfer[0], &req->msg);
	ret = spi_sync(client, &req->msg);
	if (ret) {
		pr_err("%s: spi_sync read data error:ret=[%d]", __func__, ret);
	} else {
		memcpy(data, output+SPI_HEADER_SIZE, size);
		swap_2byte(data, size);
	}

	kfree(req);
err_alloc_req:
	kfree(output);
err_alloc_output:
	kfree(input);

	return ret;
}

static int bu21150_write_register(u32 addr, u16 size, u8 *data)
{
	struct bu21150_data *ts = spi_get_drvdata(g_client_bu21150);
	struct spi_device *client = ts->client;
	struct ser_req *req;
	int ret;
	u8 *input;

	input = kzalloc(sizeof(u8)*(size)+SPI_HEADER_SIZE, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto err_alloc_req_write;
	}

	/* set header */
	input[0] = 0x02;                 /* write command */
	input[1] = (addr & 0xFF00) >> 8; /* address hi */
	input[2] = (addr & 0x00FF) >> 0; /* address lo */

	/* set data */
	memcpy(input+SPI_HEADER_SIZE, data, size);
	swap_2byte(input+SPI_HEADER_SIZE, size);

	/* write data */
	spi_message_init(&req->msg);
	req->xfer[0].tx_buf = input;
	req->xfer[0].rx_buf = NULL;
	req->xfer[0].len = size+SPI_HEADER_SIZE;
	req->xfer[0].cs_change = 0;
	req->xfer[0].bits_per_word = SPI_BITS_PER_WORD_WRITE;
	spi_message_add_tail(&req->xfer[0], &req->msg);
	ret = spi_sync(client, &req->msg);
	if (ret)
		pr_err("%s: spi_sync write data error:ret=[%d]", __func__, ret);

	kfree(req);
err_alloc_req_write:
	kfree(input);

	return ret;
}

static void wake_up_frame_waitq(struct bu21150_data *ts)
{
	ts->frame_waitq_flag = WAITQ_WAKEUP;
	wake_up_interruptible(&(ts->frame_waitq));
}

static long wait_frame_waitq(struct bu21150_data *ts, u8 flag)
{
	if (ts->force_unblock_flag == 1)
		return BU21150_UNBLOCK;

	if (ts->unblock_flag == 1 && flag == 0)
		return BU21150_UNBLOCK;

	/* wait event */
	if (wait_event_interruptible(ts->frame_waitq,
			ts->frame_waitq_flag == WAITQ_WAKEUP)) {
		pr_err("%s: -ERESTARTSYS\n", __func__);
		return -ERESTARTSYS;
	}
	ts->frame_waitq_flag = WAITQ_WAIT;

	if (ts->timeout_enb == 1) {
		if (ts->timeout_flag == 1) {
			ts->set_timer_flag = 0;
			ts->timeout_flag = 0;
			pr_err("%s: return BU21150_TIMEOUT\n", __func__);
			return BU21150_TIMEOUT;
		}
	}

	if (ts->force_unblock_flag == 1)
		return BU21150_UNBLOCK;

	if (ts->unblock_flag == 1 && flag == 0)
		return BU21150_UNBLOCK;

	return 0;
}

static int is_same_bu21150_ioctl_get_frame_data(
	struct bu21150_ioctl_get_frame_data *data1,
	struct bu21150_ioctl_get_frame_data *data2)
{
	int i;
	u8 *p1 = (u8 *)data1;
	u8 *p2 = (u8 *)data2;

	for (i = 0; i < sizeof(struct bu21150_ioctl_get_frame_data); i++) {
		if (p1[i] != p2[i])
			return 0;
	}

	return 1;
}

static void copy_frame(struct bu21150_data *ts)
{
	struct bu21150_frame *temp;

	mutex_lock(&(ts->mutex_frame));
	/* check for max limit */
	if (ts->frame_count >= BU21150_LIST_MAX_FRAMES) {
		struct bu21150_frame *tmp;

		pr_err("max limit!!! frame_count=%d\n", ts->frame_count);
		tmp = list_first_entry(&ts->frame_list.list,
				struct bu21150_frame, list);
		list_del(&tmp->list);
		kfree(tmp);
		ts->frame_count--;
	}

	temp = kzalloc(sizeof(struct bu21150_frame), GFP_KERNEL);
	ts->frame_get = ts->frame_work_get;
	memcpy(temp->frame, ts->frame_work, MAX_FRAME_SIZE);
	do_gettimeofday(&(temp->tv));
	list_add_tail(&(temp->list), &(ts->frame_list.list));
	ts->frame_count++;
	mutex_unlock(&(ts->mutex_frame));
}

static void swap_2byte(unsigned char *buf, unsigned int size)
{
	int i;
	u16 *psbuf = (u16 *)buf;

	if (size%2 == 1) {
		pr_err("%s: error size is odd. size=[%u]\n", __func__, size);
		return;
	}

	for (i = 0; i < size/2; i++)
		be16_to_cpus(psbuf+i);
}

#ifdef CHECK_SAME_FRAME
static void check_same_frame(struct bu21150_data *ts)
{
	static int frame_no = -1;
	u16 *ps;
	struct bu21150_frame *temp;
	struct list_head *pos, *n;

	mutex_lock(&ts->mutex_frame);
	if (!list_empty(&ts->frame_list.list)) {
		/* get the last node */
		temp = list_entry(&ts->frame_list.list->prev,
				struct bu21150_frame, list);

		ps = (u16 *)temp->frame;

		if (ps[2] == frame_no)
			pr_err("%s:same_frame_no=[%d]\n", __func__, frame_no);
		frame_no = ps[2];
	}
	mutex_unlock(&ts->mutex_frame);
}
#endif

static bool parse_dtsi(struct device *dev, struct bu21150_data *ts)
{
	enum of_gpio_flags dummy;
	struct device_node *np = dev->of_node;
	int rc;

	if (of_find_property(np, "irq-gpio", NULL))
		ts->irq_gpio = of_get_named_gpio_flags(np,
						"irq-gpio", 0, &dummy);
	if (of_find_property(np, "rst-gpio", NULL))
		ts->rst_gpio = of_get_named_gpio_flags(np,
						"rst-gpio", 0, &dummy);

	rc = of_property_read_string(np, "jdi,panel-model", &ts->panel_model);
	if (rc < 0 && rc != -EINVAL) {
		dev_err(dev, "Unable to read panel model\n");
		return false;
	}

	rc = of_property_read_string(np, "jdi,afe-version", &ts->afe_version);
	if (rc < 0 && rc != -EINVAL) {
		dev_err(dev, "Unable to read AFE version\n");
		return false;
	}

	rc = of_property_read_string(np, "jdi,pitch-type", &ts->pitch_type);
	if (rc < 0 && rc != -EINVAL) {
		dev_err(dev, "Unable to read pitch type\n");
		return false;
	}

	rc = of_property_read_string(np, "jdi,afe-vendor", &ts->afe_vendor);
	if (rc < 0 && rc != -EINVAL) {
		dev_err(dev, "Unable to read AFE vendor\n");
		return false;
	}

	ts->wake_up = of_property_read_bool(np, "jdi,wake-up");

	ts->timeout_enable = of_property_read_bool(np, "jdi,timeout-enable");

	return true;
}

