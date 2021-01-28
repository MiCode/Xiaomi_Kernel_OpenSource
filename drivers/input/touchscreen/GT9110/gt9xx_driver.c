/*
 * Copyright  (C)  2010 - 2016 Goodix., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Version: V2.6.0.3
 */

#include "gt9xx_config.h"
#include "include/tpd_gt9xx_common.h"
#include "mtk_boot_common.h"
#include "tpd.h"

#ifdef CONFIG_GTP_PROXIMITY
#include <hwmsen_dev.h>
#include <hwmsensor.h>
#include <sensors_io.h>
#endif

#ifdef CONFIG_GTP_SUPPORT_I2C_DMA
#include <linux/dma-mapping.h>
#endif
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/proc_fs.h> /*proc*/
#include <uapi/linux/sched/types.h>

static int tpd_flag;
int tpd_halt;
static int tpd_eint_mode = 1;
static struct task_struct *thread;
static struct task_struct *init_panel_thread;
static int tpd_polling_time = 50;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);
static atomic_t irq_enabled = ATOMIC_INIT(0);
static unsigned int touch_irq;

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
static const u16 touch_key_array[] = TPD_KEYS;
#define GTP_MAX_KEY_NUM (ARRAY_SIZE(touch_key_array))
#endif

#ifdef CONFIG_GTP_CHARGER_DETECT
static void gtp_charger_config_check(s32 dir_update);
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#ifdef CONFIG_GTP_SUPPORT_I2C_DMA
static u8 *gpDMABuf_va;
static dma_addr_t gpDMABuf_pa;
#endif

#ifdef CONFIG_GTP_WITH_HOVER
struct input_dev *pen_dev;
static void gtp_pen_init(void);
static void gtp_pen_down(s32 x, s32 y, s32 size, s32 id);
static void gtp_pen_up(void);
#endif

static irqreturn_t tpd_interrupt_handler(int irq, void *dev_id);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client,
			  struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
static void tpd_on(void);
static void tpd_off(void);
static s32 gtp_send_cfg(struct i2c_client *);

#ifdef CONFIG_GTP_CHARGER_DETECT
#define TPD_CHARGER_CHECK_CIRCLE 50
static struct delayed_work gtp_charger_check_work;
static int clk_tick_cnt_charger = 200;
static struct workqueue_struct *gtp_workqueue_charger;

#endif

static struct workqueue_struct *gtp_workqueue;
static int clk_tick_cnt = 200;
#ifdef CONFIG_GTP_ESD_PROTECT
static struct delayed_work gtp_esd_check_work;
static s32 gtp_init_ext_watchdog(struct i2c_client *client);
static void gtp_esd_check_func(struct work_struct *);
static u8 esd_running;
static spinlock_t esd_lock;
#endif

#ifdef CONFIG_HOTKNOT_BLOCK_RW
u8 hotknot_paired_flag;
#endif

#ifdef CONFIG_GTP_PROXIMITY
#define TPD_PROXIMITY_VALID_REG 0x814E
#define TPD_PROXIMITY_ENABLE_REG 0x8042
static u8 tpd_proximity_flag;
static u8 tpd_proximity_detect = 1; /* 0-->close ; 1--> far away */
#endif

#ifndef GTP_REG_REFRESH_RATE
#define GTP_REG_REFRESH_RATE 0x8056
#endif

static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH] = {
	GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
#ifdef CONFIG_GTP_CHARGER_DETECT
static u8 gtp_charger_config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH] = {
	GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
#endif
#pragma pack(1)
struct st_tpd_info {
	u16 pid; /* product id		*/
	u16 vid; /* version id		*/
};
#pragma pack()

u8 gtp_rawdiff_mode;
static struct st_tpd_info tpd_info;
static u8 int_type;
static u32 abs_x_max;
static u32 abs_y_max;
static u8 pnl_init_error;
u8 cfg_len;
u8 gtp_resetting;
static u8 chip_gt9xxs; /* true if chip type is gt9xxs,like gt915s */

#if defined(CONFIG_GTP_COMPATIBLE_MODE) || defined(CONFIG_GTP_HOTKNOT)
enum chip_type_t gtp_chip_type = CHIP_TYPE_GT9;
#endif

#ifdef CONFIG_GTP_COMPATIBLE_MODE
u8 driver_num;
u8 sensor_num;
u8 gtp_ref_retries;
u8 gtp_clk_retries;
u8 gtp_clk_buf[6];
u8 rqst_processing;

static u8 gtp_bak_ref_proc(struct i2c_client *client, u8 mode);
static u8 gtp_main_clk_proc(struct i2c_client *client);
static void gtp_recovery_reset(struct i2c_client *client);
#endif

static struct proc_dir_entry *gt91xx_config_proc;

struct i2c_client *i2c_client_point;
static const struct i2c_device_id tpd_i2c_id[] = {{"gt9xx", 0}, {} };
static unsigned short force[] = {0, 0xBA, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const forces[] = {force, NULL};
#ifdef CONFIG_OF
static const struct of_device_id gt9xx_dt_match[] = {
	{.compatible = "mediatek,cap_touch"}, {},
};
#endif
MODULE_DEVICE_TABLE(of, gt9xx_dt_match);
static struct i2c_driver tpd_i2c_driver = {
	.driver = {

			.name = "gt9xx",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(gt9xx_dt_match),
#endif
		},
	.probe = tpd_i2c_probe,
	.remove = tpd_i2c_remove,
	.detect = tpd_i2c_detect,
	.id_table = tpd_i2c_id,
	.address_list = (const unsigned short *)forces,
};

#ifndef CONFIG_GTP_USE_PINCTRL
static unsigned int tpd_rst_gpio;
static unsigned int tpd_int_gpio;
#endif
static int of_get_gt9xx_platform_data(struct device *dev)
{
#if defined(CONFIG_OF) && !defined(CONFIG_GTP_USE_PINCTRL)
	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(gt9xx_dt_match), dev);
		if (!match) {
			GTP_ERROR("Error: No device match found\n");
			return -ENODEV;
		}
		tpd_rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
		tpd_int_gpio = of_get_named_gpio(dev->of_node, "int-gpio", 0);

		GTP_ERROR("g_vproc_en_gpio_number %d\n", tpd_rst_gpio);
		GTP_ERROR("g_vproc_vsel_gpio_number %d\n", tpd_int_gpio);
	}
#endif
	return 0;
}

/*
 * MTK PHONE:  Use pinctrl api to set gpio
 * MTK TABLET: Use gpiolib api to set gpio
 * Distinguish by macro CONFIG_GTP_USE_PINCTRL
 */
static int gtp_get_gpio_res(void)
{
#if defined(CONFIG_OF) && !defined(CONFIG_GTP_USE_PINCTRL)
	int ret;
	/* configure the gpio pins */
	ret = gpio_request_one(tpd_rst_gpio, GPIOF_OUT_INIT_LOW,
			       "touchp_reset");
	if (ret < 0) {
		GTP_ERROR("Unable to request gpio reset_pin\n");
		return -1;
	}
	ret = gpio_request_one(tpd_int_gpio, GPIOF_IN, "tpd_int");
	if (ret < 0) {
		GTP_ERROR("Unable to request gpio int_pin\n");
		gpio_free(tpd_rst_gpio);
		return -1;
	}
#endif
	return 0;
}

static void gtp_free_gpio_res(void)
{
#if defined(CONFIG_OF) && !defined(CONFIG_GTP_USE_PINCTRL)
	gpio_free(tpd_rst_gpio);
	gpio_free(tpd_int_gpio);
#endif
}

void gtp_gpio_output(int gpio_type, int level)
{
#ifdef CONFIG_GTP_USE_PINCTRL
	tpd_gpio_output(gpio_type, level);
#else
	if (gpio_type == GTP_RST_GPIO)
		gpio_direction_output(tpd_rst_gpio, level);
	else if (gpio_type == GTP_IRQ_GPIO)
		gpio_direction_output(tpd_int_gpio, level);
#endif
}

void gtp_gpio_input(int gpio_type)
{
#ifdef CONFIG_GTP_USE_PINCTRL
	if (gpio_type == GTP_IRQ_GPIO)
		tpd_gpio_as_int(GTP_IRQ_GPIO);
#else
	if (gpio_type == GTP_RST_GPIO)
		gpio_direction_input(tpd_rst_gpio);
	else if (gpio_type == GTP_IRQ_GPIO)
		gpio_direction_input(tpd_int_gpio);
#endif
}

void gtp_irq_enable(void)
{
	if (!atomic_cmpxchg(&irq_enabled, 0, 1))
		enable_irq(touch_irq);
}

void gtp_irq_disable(void)
{
	if (atomic_cmpxchg(&irq_enabled, 1, 0))
		disable_irq(touch_irq);
}

#ifdef TPD_REFRESH_RATE
/*
 *******************************************************
 *Function:
 *	Write refresh rate
 *Input:
 *	rate: refresh rate N (Duration=5+N ms, N=0~15)
 *Output:
 *	Executive outcomes.0---succeed.
 ********************************************************
 */
static u8 gtp_set_refresh_rate(u8 rate)
{
	u8 buf[3] = {GTP_REG_REFRESH_RATE >> 8, GTP_REG_REFRESH_RATE & 0xff,
		     rate};

	if (rate > 0xf) {
		GTP_ERROR("Refresh rate is over range (%d)", rate);
		return FAIL;
	}

	GTP_INFO("Refresh rate change to %d", rate);
	return gtp_i2c_write(i2c_client_point, buf, sizeof(buf));
}

/*
 *******************************************************
 *Function:
 *	Get refresh rate
 *
 *Output:
 *	Refresh rate or error code
 ********************************************************
 */
static u8 gtp_get_refresh_rate(void)
{
	int ret;
	u8 buf[3] = {GTP_REG_REFRESH_RATE >> 8, GTP_REG_REFRESH_RATE & 0xff};

	ret = gtp_i2c_read(i2c_client_point, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	GTP_INFO("Refresh rate is %d", buf[GTP_ADDR_LENGTH]);
	return buf[GTP_ADDR_LENGTH];
}

/* ============================================================= */
static ssize_t show_refresh_rate(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = gtp_get_refresh_rate();

	if (ret < 0)
		return 0;

	return sprintf(buf, "%d\n", ret);
}
static ssize_t store_refresh_rate(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	unsigned long rate = 0;

	if (kstrtoul(buf, 16, &rate))
		return 0;
	gtp_set_refresh_rate(rate);
	return size;
}
static DEVICE_ATTR(tpd_refresh_rate, 0664, show_refresh_rate,
		   store_refresh_rate);

static struct device_attribute *gt9xx_attrs[] = {
	&dev_attr_tpd_refresh_rate,
};
#endif
/* ============================================================= */

static int tpd_i2c_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	strcpy(info->type, "mtk-tpd");
	return 0;
}

#ifdef CONFIG_GTP_PROXIMITY
static s32 tpd_get_ps_value(void)
{
	return tpd_proximity_detect;
}

static s32 tpd_enable_ps(s32 enable)
{
	u8 state;
	s32 ret = -1;

	if (enable) {
		state = 1;
		tpd_proximity_flag = 1;
		GTP_INFO("TPD proximity function to be on.");
	} else {
		state = 0;
		tpd_proximity_flag = 0;
		GTP_INFO("TPD proximity function to be off.");
	}

	ret = i2c_write_bytes(i2c_client_point, TPD_PROXIMITY_ENABLE_REG,
			      &state, 1);

	if (ret < 0) {
		GTP_ERROR("TPD %s proximity cmd failed.",
			  state ? "enable" : "disable");
		return ret;
	}

	GTP_INFO("TPD proximity function %s success.",
		 state ? "enable" : "disable");
	return 0;
}

s32 tpd_ps_operate(void *self, u32 command, void *buff_in, s32 size_in,
		   void *buff_out, s32 size_out, s32 *actualout)
{
	s32 err = 0;
	s32 value;
	struct hwm_sensor_data *sensor_data;
	struct hwm_sensor_data sensor_size;

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GTP_ERROR("Set delay parameter error!");
			err = -EINVAL;
		}

		/* Do nothing */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GTP_ERROR("Enable sensor parameter error!");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			err = tpd_enable_ps(value);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(sensor_size))) {
			GTP_ERROR("Get sensor data parameter error!");
			err = -EINVAL;
		} else {
			sensor_data = (struct hwm_sensor_data *)buff_out;
			sensor_data->values[0] = tpd_get_ps_value();
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}

		break;

	default:
		GTP_ERROR(
			"proxmy sensor operate function no this parameter %d!",
			command);
		err = -1;
		break;
	}

	return err;
}
#endif

static ssize_t gt91xx_config_read_proc(struct file *file, char *buffer,
				       size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {0};
	int i, len, err = -1;

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!page) {
		kfree(page);
		return -ENOMEM;
	}

	ptr = page;
	ptr += sprintf(ptr, "==== GT9XX config init value====\n");

	for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
		ptr += sprintf(ptr, "0x%02X ", config[i + 2]);

		if (i % 8 == 7)
			ptr += sprintf(ptr, "\n");
	}

	ptr += sprintf(ptr, "\n");

	ptr += sprintf(ptr, "==== GT9XX config real value====\n");
	i2c_read_bytes(i2c_client_point, GTP_REG_CONFIG_DATA, temp_data,
		       GTP_CONFIG_MAX_LENGTH);

	for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
		ptr += sprintf(ptr, "0x%02X ", temp_data[i]);

		if (i % 8 == 7)
			ptr += sprintf(ptr, "\n");
	}
	/* Touch PID & VID */
	ptr += sprintf(ptr, "\n");
	ptr += sprintf(ptr, "==== GT9XX Version ID ====\n");
	i2c_read_bytes(i2c_client_point, GTP_REG_VERSION, temp_data, 6);
	ptr += sprintf(ptr, "Chip PID: %c%c%c	VID: 0x%02X%02X\n",
		       temp_data[0], temp_data[1], temp_data[2], temp_data[5],
		       temp_data[4]);
	i2c_read_bytes(i2c_client_point, 0x41E4, temp_data, 1);
	ptr += sprintf(ptr, "Boot status 0x%X\n", temp_data[0]);

	/* Touch Status and Clock Gate */
	ptr += sprintf(ptr, "\n");
	ptr += sprintf(ptr, "==== Touch Status and Clock Gate ====\n");
	ptr += sprintf(ptr, "status: 1: on, 0 :off\n");
	ptr += sprintf(ptr, "status:%d\n", (tpd_halt + 1) & 0x1);

	len = ptr - page;
	if (*ppos >= len) {
		kfree(page);
		return 0;
	}
	err = copy_to_user(buffer, (char *)page, len);
	*ppos += len;
	if (err) {
		kfree(page);
		return err;
	}
	kfree(page);
	return len;

	/* return (ptr - page); */
}

static ssize_t gt91xx_config_write_proc(struct file *file, const char *buffer,
					size_t count, loff_t *ppos)
{
	s32 ret = 0;
	char temp[25] = {0}; /* for store special format cmd */
	char mode_str[15] = {0};
	unsigned int mode;
	u8 buf[1];

	GTP_DEBUG("write count %ld\n", (unsigned long)count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("size not match [%d:%ld]", GTP_CONFIG_MAX_LENGTH,
			  (unsigned long)count);
		return -EFAULT;
	}

	/**********************************************/
	/* for store special format cmd	*/
	if (copy_from_user(temp, buffer, sizeof(temp))) {
		GTP_ERROR("copy from user fail 2");
		return -EFAULT;
	}
	ret = sscanf(temp, "%s %d", (char *)&mode_str, &mode);

	/***********POLLING/EINT MODE switch****************/
	if (strcmp(mode_str, "polling") == 0) {
		if (mode >= 10 && mode <= 200) {
			GTP_INFO("Switch to polling mode, polling time is %d",
				 mode);
			tpd_eint_mode = 0;
			tpd_polling_time = mode;
			tpd_flag = 1;
			wake_up_interruptible(&waiter);
		} else {
			GTP_INFO(
				"Wrong polling time, please set between 10~200ms");
		}
		return count;
	}
	if (strcmp(mode_str, "eint") == 0) {
		GTP_INFO("Switch to eint mode");
		tpd_eint_mode = 1;
		return count;
	}

	if (strcmp(mode_str, "switch") == 0) {
		if (mode == 0) /* turn off */
			tpd_off();
		else if (mode == 1) /* turn on */
			tpd_on();
		else
			GTP_ERROR("error mode :%d", mode);
		return count;
	}
	/* force clear config */
	if (strcmp(mode_str, "clear_config") == 0) {
		GTP_INFO("Force clear config");
		buf[0] = 0x10;
		ret = i2c_write_bytes(i2c_client_point, GTP_REG_SLEEP, buf, 1);
		return count;
	}

	if (copy_from_user(&config[2], buffer, count)) {
		GTP_ERROR("copy from user fail\n");
		return -EFAULT;
	}

	/***********clk operate reseved****************/
	ret = gtp_send_cfg(i2c_client_point);
	abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
	abs_y_max =
		(config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
	int_type = (config[TRIGGER_LOC]) & 0x03;

	if (ret < 0)
		GTP_ERROR("send config failed.");

	return count;
}

#ifdef CONFIG_GTP_SUPPORT_I2C_DMA
s32 i2c_dma_read(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len)
{
	int ret;
	s32 retry = 0;
	u8 buffer[2];

	struct i2c_msg msg[2] = {
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
			.addr = (client->addr & I2C_MASK_FLAG),
#else
			.addr = client->addr,
#endif
			.flags = 0,
			.buf = buffer,
			.len = 2,
#ifdef CONFIG_MTK_I2C_EXTENSION
			.timing = I2C_MASTER_CLOCK
#endif
		},
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
			.addr = (client->addr & I2C_MASK_FLAG),
			.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG |
				     I2C_DMA_FLAG),
#else
			.addr = client->addr,
/*.ext_flag = client->ext_flag,*/
#endif
			.flags = I2C_M_RD,
			.buf = (u8 *)gpDMABuf_pa,
			.len = len,
#ifdef CONFIG_MTK_I2C_EXTENSION
			.timing = I2C_MASTER_CLOCK
#endif
		},
	};

	buffer[0] = (addr >> 8) & 0xFF;
	buffer[1] = addr & 0xFF;

	if (rxbuf == NULL)
		return -1;

	/* GTP_DEBUG("dma i2c read: 0x%04X, %d bytes(s)", addr, len); */
	for (retry = 0; retry < 5; ++retry) {
		ret = i2c_transfer(client->adapter, &msg[0], 2);
		if (ret < 0)
			continue;
		memcpy(rxbuf, gpDMABuf_va, len);
		return 0;
	}
	GTP_ERROR("Dma I2C Read Error: 0x%04X, %d byte(s), err-code: %d", addr,
		  len, ret);
	return ret;
}

s32 i2c_dma_write(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{
	int ret;
	s32 retry = 0;
	u8 *wr_buf = gpDMABuf_va;

	struct i2c_msg msg = {
#ifdef CONFIG_MTK_I2C_EXTENSION
		.addr = (client->addr & I2C_MASK_FLAG),
		.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
#else
		.addr = client->addr,
/*.ext_flag = client->ext_flag,*/
#endif
		.flags = 0,
		.buf = (u8 *)gpDMABuf_pa,
		.len = 2 + len,
#ifdef CONFIG_MTK_I2C_EXTENSION
		.timing = I2C_MASTER_CLOCK
#endif
	};

	wr_buf[0] = (u8)((addr >> 8) & 0xFF);
	wr_buf[1] = (u8)(addr & 0xFF);

	if (txbuf == NULL)
		return -1;

	/* GTP_DEBUG("dma i2c write: 0x%04X, %d bytes(s)", addr, len); */
	memcpy(wr_buf + 2, txbuf, len);
	for (retry = 0; retry < 5; ++retry) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret < 0)
			continue;
		return 0;
	}
	GTP_ERROR("Dma I2C Write Error: 0x%04X, %d byte(s), err-code: %d", addr,
		  len, ret);
	return ret;
}

s32 i2c_read_bytes_dma(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len)
{
	s32 left = len;
	s32 read_len = 0;
	u8 *rd_buf = rxbuf;
	s32 ret = 0;

	/* GTP_DEBUG("Read bytes dma: 0x%04X, %d byte(s)", addr, len); */
	while (left > 0) {
		if (left > GTP_DMA_MAX_TRANSACTION_LENGTH)
			read_len = GTP_DMA_MAX_TRANSACTION_LENGTH;
		else
			read_len = left;

		ret = i2c_dma_read(client, addr, rd_buf, read_len);
		if (ret < 0) {
			GTP_ERROR("dma read failed");
			return -1;
		}

		left -= read_len;
		addr += read_len;
		rd_buf += read_len;
	}
	return 0;
}

s32 i2c_write_bytes_dma(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{

	s32 ret = 0;
	s32 write_len = 0;
	s32 left = len;
	u8 *wr_buf = txbuf;

	/* GTP_DEBUG("Write bytes dma: 0x%04X, %d byte(s)", addr, len); */
	while (left > 0) {
		if (left > GTP_DMA_MAX_I2C_TRANSFER_SIZE)
			write_len = GTP_DMA_MAX_I2C_TRANSFER_SIZE;
		else
			write_len = left;

		ret = i2c_dma_write(client, addr, wr_buf, write_len);

		if (ret < 0) {
			GTP_ERROR("dma i2c write failed!");
			return -1;
		}

		left -= write_len;
		addr += write_len;
		wr_buf += write_len;
	}
	return 0;
}
#endif

int i2c_read_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *rxbuf,
			   int len)
{
	u8 buffer[GTP_ADDR_LENGTH];
	u8 retry;
	u16 left = len;
	u16 offset = 0;

	struct i2c_msg msg[2] = {
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
			.addr = ((client->addr & I2C_MASK_FLAG) |
				 (I2C_ENEXT_FLAG)),
/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
#else
			.addr = client->addr,
#endif
			.flags = 0,
			.buf = buffer,
			.len = GTP_ADDR_LENGTH,
#ifdef CONFIG_MTK_I2C_EXTENSION
			.timing = I2C_MASTER_CLOCK
#endif
		},
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
			.addr = ((client->addr & I2C_MASK_FLAG) |
				 (I2C_ENEXT_FLAG)),
/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
#else
			.addr = client->addr,
#endif
			.flags = I2C_M_RD,
#ifdef CONFIG_MTK_I2C_EXTENSION
			.timing = I2C_MASTER_CLOCK
#endif
		},
	};

	if (rxbuf == NULL)
		return -1;

	while (left > 0) {
		buffer[0] = ((addr + offset) >> 8) & 0xFF;
		buffer[1] = (addr + offset) & 0xFF;

		msg[1].buf = &rxbuf[offset];

		if (left > MAX_TRANSACTION_LENGTH) {
			msg[1].len = MAX_TRANSACTION_LENGTH;
			left -= MAX_TRANSACTION_LENGTH;
			offset += MAX_TRANSACTION_LENGTH;
		} else {
			msg[1].len = left;
			left = 0;
		}

		retry = 0;

		while (i2c_transfer(client->adapter, &msg[0], 2) != 2) {
			retry++;

			if (retry == 5) {
				GTP_ERROR("I2C read 0x%X length=%d failed\n",
					  addr + offset, len);
				return -1;
			}
		}
	}

	return 0;
}

int i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
#ifdef CONFIG_GTP_SUPPORT_I2C_DMA
	return i2c_read_bytes_dma(client, addr, rxbuf, len);
#else
	return i2c_read_bytes_non_dma(client, addr, rxbuf, len);
#endif
}

s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = (buf[0] << 8) + buf[1];

	ret = i2c_read_bytes_non_dma(client, addr, &buf[2], len - 2);

	if (!ret)
		return 2;
#ifdef CONFIG_GTP_GESTURE_WAKEUP
	if (gesture_data.doze_status == DOZE_ENABLED)
		return ret;
#endif
#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F)
		gtp_recovery_reset(client);
	else
#endif
		gtp_reset_guitar(client, 20);

	return ret;
}

s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf,
			   int len)
{
	u8 buf[16] = {0};
	u8 confirm_buf[16] = {0};
	u8 retry = 0;

	while (retry++ < 3) {
		memset(buf, 0xAA, 16);
		buf[0] = (u8)(addr >> 8);
		buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, buf, len + 2);

		memset(confirm_buf, 0xAB, 16);
		confirm_buf[0] = (u8)(addr >> 8);
		confirm_buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, confirm_buf, len + 2);

		if (!memcmp(buf, confirm_buf, len + 2)) {
			memcpy(rxbuf, confirm_buf + 2, len);
			return SUCCESS;
		}
	}
	GTP_ERROR("i2c read 0x%04X, %d bytes, double check failed!", addr, len);
	return FAIL;
}

int i2c_write_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *txbuf,
			    int len)
{
	u8 buffer[MAX_TRANSACTION_LENGTH];
	u16 left = len;
	u16 offset = 0;
	u8 retry = 0;

	struct i2c_msg msg = {
#ifdef CONFIG_MTK_I2C_EXTENSION
		.addr = ((client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
#else
		.addr = client->addr,
#endif
		.flags = 0,
		.buf = buffer,
#ifdef CONFIG_MTK_I2C_EXTENSION
		.timing = I2C_MASTER_CLOCK,
#endif
	};

	if (txbuf == NULL)
		return -1;

	while (left > 0) {
		retry = 0;

		buffer[0] = ((addr + offset) >> 8) & 0xFF;
		buffer[1] = (addr + offset) & 0xFF;

		if (left > MAX_I2C_TRANSFER_SIZE) {
			memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset],
			       MAX_I2C_TRANSFER_SIZE);
			msg.len = MAX_TRANSACTION_LENGTH;
			left -= MAX_I2C_TRANSFER_SIZE;
			offset += MAX_I2C_TRANSFER_SIZE;
		} else {
			memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], left);
			msg.len = left + GTP_ADDR_LENGTH;
			left = 0;
		}

		/* GTP_DEBUG("byte left %d offset %d\n", left, offset); */
		while (i2c_transfer(client->adapter, &msg, 1) != 1) {
			retry++;

			if (retry == 5) {
				GTP_ERROR("I2C write 0x%X%X length=%d failed\n",
					  buffer[0], buffer[1], len);
				return -1;
			}
		}
	}

	return 0;
}

int i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
#ifdef CONFIG_GTP_SUPPORT_I2C_DMA
	return i2c_write_bytes_dma(client, addr, txbuf, len);
#else
	return i2c_write_bytes_non_dma(client, addr, txbuf, len);
#endif
}

s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = (buf[0] << 8) + buf[1];

	ret = i2c_write_bytes_non_dma(client, addr, &buf[2], len - 2);

	if (!ret)
		return 1;
#ifdef CONFIG_GTP_GESTURE_WAKEUP
	if (gesture_data.doze_status == DOZE_ENABLED)
		return ret;
#endif
#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F)
		gtp_recovery_reset(client);
	else
#endif
		gtp_reset_guitar(client, 20);

	return ret;
}

/*
 *******************************************************
 *Function:
 *		Send config Function.
 *Input:
 *		client: i2c client.
 *Output:
 *		Executive outcomes.0--success,non-0--fail.
 ********************************************************
 */
static s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret = 1;
#ifdef CONFIG_GTP_DRIVER_SEND_CFG
	s32 retry = 0;

	if (pnl_init_error) {
		GTP_INFO("Error occurred in init_panel, no config sent!");
		return 0;
	}

	GTP_DEBUG("Driver Send Config");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, config,
				    GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;
	}
#endif
	return ret;
}

#ifdef CONFIG_GTP_CHARGER_DETECT
static int gtp_send_chr_cfg(struct i2c_client *client)
{
	s32 ret = 1;
#ifdef CONFIG_GTP_DRIVER_SEND_CFG
	s32 retry = 0;

	if (pnl_init_error) {
		GTP_INFO("Error occurred in init_panel, no config sent!");
		return 0;
	}

	GTP_INFO("Driver Send Config");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, gtp_charger_config,
				    GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;
	}
#endif
	return ret;
}
#endif
/*
 *******************************************************
 *Function:
 *		Read goodix touchscreen version function.
 *Input:
 *		client: i2c client struct.
 *		version:address to store version info
 *Output:
 *		Executive outcomes.0---succeed.
 ********************************************************
 */
s32 gtp_read_version(struct i2c_client *client, u16 *version)
{
	s32 ret = -1;
	s32 i;
	u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

	GTP_DEBUG_FUNC();
	ret = gtp_i2c_read(client, buf, sizeof(buf));
	if (ret < 0) {
		GTP_ERROR("GTP read version failed");
		return ret;
	}

	if (version)
		*version = (buf[7] << 8) | buf[6];

	tpd_info.vid = *version;
	tpd_info.pid = 0x00;

	for (i = 0; i < 4; i++) {
		if (buf[i + 2] < 0x30)
			break;

		tpd_info.pid |= ((buf[i + 2] - 0x30) << ((3 - i) * 4));
	}

	if (buf[5] == 0x00) {
		GTP_INFO("IC VERSION: %c%c%c_%02x%02x", buf[2], buf[3], buf[4],
			 buf[7], buf[6]);
	} else {
		if (buf[5] == 'S' || buf[5] == 's')
			chip_gt9xxs = 1;

		GTP_INFO("IC VERSION:%c%c%c%c_%02x%02x", buf[2], buf[3], buf[4],
			 buf[5], buf[7], buf[6]);
	}
	return ret;
}

/*
 *******************************************************
 *Function:
 *		GTP initialize function.
 *
 *Input:
 *		client: i2c client private struct.
 *
 *Output:
 *		Executive outcomes.0---succeed.
 ********************************************************
 */
s32 gtp_init_panel(void *v_client)
{
	s32 ret = 0;
	u16 version_info;
	struct i2c_client *client = (struct i2c_client *)v_client;

#ifdef CONFIG_GTP_DRIVER_SEND_CFG
	s32 i = 0;
	u8 check_sum = 0;
	u8 opr_buf[16];
	u8 sensor_id = 0;
	u8 retry = 0;
	u8 flash_cfg_version = 0;
	u8 drv_cfg_version = 0;

	u8 cfg_info_group0[] = CTP_CFG_GROUP0;
	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;
	u8 *send_cfg_buf[] = {cfg_info_group0, cfg_info_group1,
			      cfg_info_group2, cfg_info_group3,
			      cfg_info_group4, cfg_info_group5};
	u8 cfg_info_len[] = {
		CFG_GROUP_LEN(cfg_info_group0), CFG_GROUP_LEN(cfg_info_group1),
		CFG_GROUP_LEN(cfg_info_group2), CFG_GROUP_LEN(cfg_info_group3),
		CFG_GROUP_LEN(cfg_info_group4), CFG_GROUP_LEN(cfg_info_group5)};

#ifdef CONFIG_GTP_CHARGER_DETECT
	const u8 cfg_grp0_charger[] = GTP_CFG_GROUP0_CHARGER;
	const u8 cfg_grp1_charger[] = GTP_CFG_GROUP1_CHARGER;
	const u8 cfg_grp2_charger[] = GTP_CFG_GROUP2_CHARGER;
	const u8 cfg_grp3_charger[] = GTP_CFG_GROUP3_CHARGER;
	const u8 cfg_grp4_charger[] = GTP_CFG_GROUP4_CHARGER;
	const u8 cfg_grp5_charger[] = GTP_CFG_GROUP5_CHARGER;
	const u8 *cfgs_charger[] = {cfg_grp0_charger, cfg_grp1_charger,
				    cfg_grp2_charger, cfg_grp3_charger,
				    cfg_grp4_charger, cfg_grp5_charger};
	u8 cfg_lens_charger[] = {CFG_GROUP_LEN(cfg_grp0_charger),
				 CFG_GROUP_LEN(cfg_grp1_charger),
				 CFG_GROUP_LEN(cfg_grp2_charger),
				 CFG_GROUP_LEN(cfg_grp3_charger),
				 CFG_GROUP_LEN(cfg_grp4_charger),
				 CFG_GROUP_LEN(cfg_grp5_charger)};
#endif
#endif

	ret = gtp_read_version(client, &version_info);
	if (ret < 0) {
		GTP_ERROR("Read version failed.");
		goto out;
	}

#ifdef CONFIG_GTP_DRIVER_SEND_CFG

	GTP_DEBUG("Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
		  cfg_info_len[0], cfg_info_len[1], cfg_info_len[2],
		  cfg_info_len[3], cfg_info_len[4], cfg_info_len[5]);

	pnl_init_error = 0;
	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) && (!cfg_info_len[3]) &&
	    (!cfg_info_len[4]) && (!cfg_info_len[5])) {
		sensor_id = 0;
	} else {
#ifdef CONFIG_GTP_COMPATIBLE_MODE
		if (gtp_chip_type == CHIP_TYPE_GT9F)
			/* msleep(50); */
#endif
			ret = gtp_i2c_read_dbl_check(client, GTP_REG_SENSOR_ID,
					     &sensor_id, 1);
		if (ret == SUCCESS) {

			while ((sensor_id == 0xff) && (retry++ < 3)) {
				/* msleep(100); */
				ret = gtp_i2c_read_dbl_check(client,
							     GTP_REG_SENSOR_ID,
							     &sensor_id, 1);
				GTP_ERROR("GTP sensor_ID read failed time %d.",
					  retry);
			}

			if (sensor_id >= 0x06) {
				GTP_ERROR(
					"Invalid sensor_id(0x%02X), No Config Sent!",
					sensor_id);
				pnl_init_error = 1;
				return -1;
			}
		} else {
			GTP_ERROR("Failed to get sensor_id, No config sent!");
			pnl_init_error = 1;
			return -1;
		}
		GTP_INFO("Sensor_ID: %d", sensor_id);
	}

	cfg_len = cfg_info_len[sensor_id];

	GTP_INFO("CTP_CONFIG_GROUP%d used, config length: %d", sensor_id,
		 cfg_len);

	if (cfg_len < GTP_CONFIG_MIN_LENGTH) {
		GTP_ERROR(
			"CTP_CONFIG_GROUP%d is INVALID CONFIG GROUP! NO Config Sent!",
			sensor_id);
		GTP_ERROR(
			" You need to check you header file CFG_GROUP section!");
		pnl_init_error = 1;
		return -1;
	}

#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type != CHIP_TYPE_GT9F) {
#else
	{
#endif
		ret = gtp_i2c_read_dbl_check(client, GTP_REG_CONFIG_DATA,
					     &opr_buf[0], 1);
		if (ret == SUCCESS) {
			GTP_DEBUG(
				"CFG_CONFIG_GROUP%d Config Version: %d, 0x%02X; IC Config Version: %d, 0x%02X",
				sensor_id, send_cfg_buf[sensor_id][0],
				send_cfg_buf[sensor_id][0], opr_buf[0],
				opr_buf[0]);

			flash_cfg_version = opr_buf[0];
			drv_cfg_version =
				send_cfg_buf[sensor_id][0];
				/* backup	config version */
			if (flash_cfg_version < 90 &&
			    flash_cfg_version > drv_cfg_version)
				send_cfg_buf[sensor_id][0] = 0x00;

		} else {
			GTP_ERROR(
				"Failed to get ic config version!No config sent!");
			return -1;
		}
	}

	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], cfg_len);

#ifdef CONFIG_GTP_CUSTOM_CFG
	config[RESOLUTION_LOC] = (u8)GTP_MAX_WIDTH;
	config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH >> 8);
	config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
	config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT >> 8);

	if (GTP_INT_TRIGGER == 0) /* RISING */
		config[TRIGGER_LOC] &= 0xfe;
	else if (GTP_INT_TRIGGER == 1) /* FALLING */
		config[TRIGGER_LOC] |= 0x01;

#endif /* CONFIG_GTP_CUSTOM_CFG */

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
		check_sum += config[i];

	config[cfg_len] = (~check_sum) + 1;

#ifdef CONFIG_GTP_CHARGER_DETECT
	GTP_DEBUG("Charger Config Groups Length: %d, %d, %d, %d, %d, %d",
		  cfg_lens_charger[0], cfg_lens_charger[1], cfg_lens_charger[2],
		  cfg_lens_charger[3], cfg_lens_charger[4],
		  cfg_lens_charger[5]);

	memset(&gtp_charger_config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	if (cfg_lens_charger[sensor_id] == cfg_len)
		memcpy(&gtp_charger_config[GTP_ADDR_LENGTH],
		       cfgs_charger[sensor_id], cfg_len);

#ifdef CONFIG_GTP_CUSTOM_CFG
	gtp_charger_config[RESOLUTION_LOC] = (u8)GTP_MAX_WIDTH;
	gtp_charger_config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH >> 8);
	gtp_charger_config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
	gtp_charger_config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT >> 8);

	if (GTP_INT_TRIGGER == 0) /* RISING	*/
		gtp_charger_config[TRIGGER_LOC] &= 0xfe;
	else if (GTP_INT_TRIGGER == 1) /* FALLING */
		gtp_charger_config[TRIGGER_LOC] |= 0x01;
#endif /* END CONFIG_GTP_CUSTOM_CFG */
	if (cfg_lens_charger[sensor_id] != cfg_len)
		memset(&gtp_charger_config[GTP_ADDR_LENGTH], 0,
		       GTP_CONFIG_MAX_LENGTH);

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
		check_sum += gtp_charger_config[i];

	gtp_charger_config[cfg_len] = (~check_sum) + 1;

#endif /* END CONFIG_GTP_CHARGER_DETECT */

#else  /* DRIVER NOT SEND CONFIG */
	cfg_len = GTP_CONFIG_MAX_LENGTH;
	ret = gtp_i2c_read(client, config, cfg_len + GTP_ADDR_LENGTH);
	if (ret < 0) {
		GTP_ERROR(
			"Read Config Failed, Using DEFAULT Resolution & INT Trigger!");
		abs_x_max = GTP_MAX_WIDTH;
		abs_y_max = GTP_MAX_HEIGHT;
		int_type = GTP_INT_TRIGGER;
	}
#endif /* CONFIG_GTP_DRIVER_SEND_CFG */

	GTP_DEBUG_FUNC();
	if ((abs_x_max == 0) && (abs_y_max == 0)) {
		abs_x_max = (config[RESOLUTION_LOC + 1] << 8) +
			    config[RESOLUTION_LOC];
		abs_y_max = (config[RESOLUTION_LOC + 3] << 8) +
			    config[RESOLUTION_LOC + 2];
		int_type = (config[TRIGGER_LOC]) & 0x03;
	}

#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F) {
		u8 have_key = 0;

		driver_num = (config[CFG_LOC_DRVA_NUM] & 0x1F) +
			     (config[CFG_LOC_DRVB_NUM] & 0x1F);
		sensor_num = (config[CFG_LOC_SENS_NUM] & 0x0F) +
			     ((config[CFG_LOC_SENS_NUM] >> 4) & 0x0F);
		have_key = config[GTP_REG_HAVE_KEY - GTP_REG_CONFIG_DATA + 2] &
			   0x01; /* have key or not */
		if (have_key == 1)
			driver_num--;

		GTP_INFO(
			"Driver * Sensor: %d * %d(Key: %d), X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
			driver_num, sensor_num, have_key, abs_x_max, abs_y_max,
			int_type);
	} else
#endif
	{
#ifdef CONFIG_GTP_DRIVER_SEND_CFG
		ret = gtp_send_cfg(client);
		if (ret < 0)
			GTP_ERROR("Send config error.");

#ifdef CONFIG_GTP_COMPATIBLE_MODE
		if (gtp_chip_type != CHIP_TYPE_GT9F) {
#else
		{
#endif
			/* for resume to send config */
			if (flash_cfg_version < 90 &&
			    flash_cfg_version > drv_cfg_version) {
				config[GTP_ADDR_LENGTH] = drv_cfg_version;
				check_sum = 0;
				for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
					check_sum += config[i];

				config[cfg_len] = (~check_sum) + 1;
			}
		}
#endif
		GTP_INFO("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x", abs_x_max,
			 abs_y_max, int_type);
	}

	/* msleep(20); */
	return 0;

out:
	gtp_free_gpio_res();
	return -1;
}

static s8 gtp_i2c_test(struct i2c_client *client)
{

	u8 retry = 0;
	s8 ret = -1;
	u32 hw_info = 0;

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = i2c_read_bytes(client, GTP_REG_HW_INFO, (u8 *)&hw_info,
				     sizeof(hw_info));

		if ((!ret) && (hw_info == 0x00900600)) /* 20121212 */
			return ret;

		GTP_ERROR("GTP_REG_HW_INFO : %08X", hw_info);
		GTP_ERROR("GTP i2c test failed time %d.", retry);
		msleep(20);
	}

	return -1;
}

/*
 *******************************************************
 *Function:
 *		Set INT pin	as input for FW sync.
 *Note:
 *	If the INT is high, It means there is pull up resistor attached on the
 *INT pin.
 *	Pull low the INT pin manaully for FW sync.
 ********************************************************
 */
void gtp_int_sync(s32 ms)
{
	gtp_gpio_output(GTP_IRQ_GPIO, 0);
	msleep(ms);
	gtp_gpio_input(GTP_IRQ_GPIO);
}

void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	GTP_INFO("GTP RESET!\n");

	if (gtp_resetting)
		return;
	gtp_resetting = 1;

	gtp_gpio_output(GTP_RST_GPIO, 0);
	msleep(ms);

	/* select client address */
	gtp_gpio_output(GTP_IRQ_GPIO, client->addr == 0x14);
	/* usleep_range(2000, 2010); */

	gtp_gpio_output(GTP_RST_GPIO, 1);
	/* usleep_range(6000, 6030); */ /* must >= 6ms */

#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F) {
		gtp_resetting = 0;
		return;
	}
#endif

	gtp_int_sync(0);
#ifdef CONFIG_GTP_ESD_PROTECT
	gtp_init_ext_watchdog(i2c_client_point);
#endif
	gtp_resetting = 0;
}

static int tpd_power_on(struct i2c_client *client)
{
	int ret = 0;
	int reset_count = 0;

reset_proc:
	gtp_gpio_output(GTP_IRQ_GPIO, 0);
	gtp_gpio_output(GTP_RST_GPIO, 0);
	/* msleep(20); */

	/* power on, need confirm with SA */
	GTP_ERROR("turn on power reg-vgp6\n");
	ret = regulator_enable(tpd->reg);
	if (ret != 0)
		TPD_ERR("Failed to enable reg-vgp6: %d\n", ret);

	gtp_reset_guitar(client, 0);

#ifdef CONFIG_GTP_COMPATIBLE_MODE
	gtp_get_chip_type(client);

	if (gtp_chip_type == CHIP_TYPE_GT9F) {
		ret = gup_fw_download_proc(NULL, GTP_FL_FW_BURN);
		if (ret == FAIL) {
			GTP_ERROR("[tpd power_on]Download fw failed.");
			if (reset_count++ < TPD_MAX_RESET_COUNT)
				goto reset_proc;
			else
				return ret;
		}

		ret = gtp_fw_startup(client);
		if (ret == FAIL) {
			GTP_ERROR("[tpd power_on]Startup fw failed.");
			if (reset_count++ < TPD_MAX_RESET_COUNT)
				goto reset_proc;
			else
				return -1;
		}
	} else
#endif
	{
		ret = gtp_i2c_test(client);
		if (ret < 0) {
			GTP_ERROR("I2C communication ERROR!");

			if (reset_count < TPD_MAX_RESET_COUNT) {
				reset_count++;
				goto reset_proc;
			}
		}
	}

	return ret;
}

#if defined(CONFIG_GTP_COMPATIBLE_MODE) || defined(CONFIG_GTP_HOTKNOT)
u8 gtp_fw_startup(struct i2c_client *client)
{
	u8 wr_buf[4];
	s32 ret = 0;

	/* init sw WDT */
	wr_buf[0] = 0xAA;
	ret = i2c_write_bytes(client, 0x8041, wr_buf, 1);
	if (ret < 0) {
		GTP_ERROR("I2C error to firmware startup.");
		return FAIL;
	}
	/* release SS51 & DSP */
	wr_buf[0] = 0x00;
	i2c_write_bytes(client, 0x4180, wr_buf, 1);

	/* int sync */
	gtp_int_sync(20);

	/* check fw run status */
	i2c_read_bytes(client, 0x8041, wr_buf, 1);
	if (wr_buf[0] == 0xAA) {
		GTP_ERROR("IC works abnormally,startup failed.");
		return FAIL;
	}
	GTP_DEBUG("IC works normally,Startup success.");
	wr_buf[0] = 0xAA;
	i2c_write_bytes(client, 0x8041, wr_buf, 1);
	return SUCCESS;
}
#endif

/* **************** For GT9XXF Start *********************/
#ifdef CONFIG_GTP_COMPATIBLE_MODE

void gtp_get_chip_type(struct i2c_client *client)
{
	u8 opr_buf[10] = {0x00};
	s32 ret = 0;

	msleep(20);
	ret = gtp_i2c_read_dbl_check(client, GTP_REG_CHIP_TYPE, opr_buf, 10);
	if (ret == FAIL) {
		GTP_ERROR(
			"Failed to get chip-type, set chip type default: GOODIX_GT9");
		gtp_chip_type = CHIP_TYPE_GT9;
		return;
	}

	if (!memcmp(opr_buf, "GOODIX_GT9", 10)) {
		GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9)
						  ? "GOODIX_GT9"
						  : "GOODIX_GT9F");
		gtp_chip_type = CHIP_TYPE_GT9;
	} else { /* GT9XXF */
		gtp_chip_type = CHIP_TYPE_GT9F;
		GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9)
						  ? "GOODIX_GT9"
						  : "GOODIX_GT9F");
	}
	gtp_chip_type = CHIP_TYPE_GT9; /* for test */
	GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9)
					  ? "GOODIX_GT9"
					  : "GOODIX_GT9F");
}

static u8 gtp_bak_ref_proc(struct i2c_client *client, u8 mode)
{
	s32 i = 0;
	s32 j = 0;
	s32 ret = 0;
	struct file *flp = NULL;
	u8 *refp = NULL;
	u32 ref_len = 0;
	u32 ref_seg_len = 0;
	s32 ref_grps = 0;
	s32 ref_chksum = 0;
	u16 tmp = 0;

	mm_segment_t old_fs;

	GTP_DEBUG("[gtp bak_ref_proc]Driver:%d,Sensor:%d.", driver_num,
		  sensor_num);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* check file-system mounted */
	GTP_DEBUG("[gtp bak_ref_proc]Waiting for FS %d", gtp_ref_retries);
	if (gup_check_fs_mounted("/data") == FAIL) {
		GTP_DEBUG("[gtp bak_ref_proc]/data not mounted");
		if (gtp_ref_retries++ < GTP_CHK_FS_MNT_MAX) {
			set_fs(old_fs);
			return FAIL;
		}

	} else {
		GTP_DEBUG("[gtp bak_ref_proc]/data mounted !!!!");
	}

	ref_len = driver_num * (sensor_num - 2) * 2 + 4;
	ref_seg_len = ref_len;
	ref_grps = 1;

	refp = kzalloc(ref_len, GFP_KERNEL);
	if (refp == NULL) {
		GTP_ERROR(
			"[gtp bak_ref_proc]Alloc memory for ref failed.use default ref");
		set_fs(old_fs);
		return FAIL;
	}

	memset(refp, 0, ref_len);
	if (gtp_ref_retries >= GTP_CHK_FS_MNT_MAX) {
		for (j = 0; j < ref_grps; ++j)
			refp[ref_seg_len + j * ref_seg_len - 1] = 0x01;

		ret = i2c_write_bytes(client, 0x99D0, refp, ref_len);
		if (-1 == ret) {
			GTP_ERROR("[gtp bak_ref_proc]Write ref i2c error.");
			ret = FAIL;
		}

		GTP_ERROR(
			"[gtp bak_ref_proc]Bak file or path is not exist,send default ref.");
		ret = SUCCESS;
		goto exit_ref_proc;
	}

	/* get ref file data */
	flp = filp_open(GTP_BAK_REF_PATH, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(flp)) {
		GTP_ERROR(
			"[gtp bak_ref_proc]Ref File not found!Creat ref file.");
		/* flp->f_op->llseek(flp, 0, SEEK_SET); */
		/* flp->f_op->write(flp, (char *)refp, ref_len, &flp->f_pos); */
		gtp_ref_retries++;
		ret = FAIL;
		goto exit_ref_proc;
	} else if (mode == GTP_BAK_REF_SEND) {
		flp->f_op->llseek(flp, 0, SEEK_SET);
		ret = flp->f_op->read(flp, (char *)refp, ref_len, &flp->f_pos);
		if (ret < 0) {
			GTP_ERROR("[gtp bak_ref_proc]Read ref file failed.");
			memset(refp, 0, ref_len);
		}
	}

	if (mode == GTP_BAK_REF_STORE) {
		ret = i2c_read_bytes(client, 0x99D0, refp, ref_len);
		if (-1 == ret) {
			GTP_ERROR("[gtp bak_ref_proc]Read ref i2c error.");
			ret = FAIL;
			goto exit_ref_proc;
		}
		flp->f_op->llseek(flp, 0, SEEK_SET);
		flp->f_op->write(flp, (char *)refp, ref_len, &flp->f_pos);
	} else {
		/* checksum ref file */
		for (j = 0; j < ref_grps; ++j) {
			ref_chksum = 0;
			for (i = 0; i < ref_seg_len - 2; i += 2)
				ref_chksum +=
					((refp[i + j * ref_seg_len] << 8) +
					 refp[i + 1 + j * ref_seg_len]);

			GTP_DEBUG("[gtp bak_ref_proc]Calc ref chksum:0x%04X",
				  ref_chksum & 0xFF);
			tmp = ref_chksum +
			      (refp[ref_seg_len + j * ref_seg_len - 2] << 8) +
			      refp[ref_seg_len + j * ref_seg_len - 1];
			if (tmp != 1) {
				GTP_DEBUG(
					"[gtp bak_ref_proc]Ref file chksum error,use default ref");
				memset(&refp[j * ref_seg_len], 0, ref_seg_len);
				refp[ref_seg_len - 1 + j * ref_seg_len] = 0x01;
			} else {
				if (j == (ref_grps - 1))
					GTP_DEBUG(
						"[gtp bak_ref_proc]Ref file chksum success.");
			}
		}

		ret = i2c_write_bytes(client, 0x99D0, refp, ref_len);
		if (-1 == ret) {
			GTP_ERROR("[gtp bak_ref_proc]Write ref i2c error.");
			ret = FAIL;
			goto exit_ref_proc;
		}
	}

	ret = SUCCESS;

exit_ref_proc:
	kfree(refp);
	set_fs(old_fs);
	if (flp && !IS_ERR(flp))
		filp_close(flp, NULL);
	return ret;
}

static void gtp_recovery_reset(struct i2c_client *client)
{
	mutex_lock(&i2c_access);
	if (tpd_halt == 0) {
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(client, SWITCH_OFF);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
		gtp_charger_switch(0);
#endif
		force_reset_guitar();
#ifdef CONFIG_GTP_ESD_PROTECT
		gtp_esd_switch(client, SWITCH_ON);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
		gtp_charger_switch(1);
#endif
	}
	mutex_unlock(&i2c_access);
}

static u8 gtp_check_clk_legality(void)
{
	u8 i = 0;
	u8 clk_chksum = gtp_clk_buf[5];

	for (i = 0; i < 5; i++) {
		if ((gtp_clk_buf[i] < 50) || (gtp_clk_buf[i] > 120) ||
		    (gtp_clk_buf[i] != gtp_clk_buf[0]))
			break;

		clk_chksum += gtp_clk_buf[i];
	}

	if ((i == 5) && (clk_chksum == 0)) {
		GTP_INFO("Clk ram legality check success");
		return SUCCESS;
	}
	GTP_ERROR("main clock freq in clock buf is wrong");
	return FAIL;
}

static u8 gtp_main_clk_proc(struct i2c_client *client)
{
	s32 ret = 0;
	u8 i = 0;
	u8 clk_cal_result = 0;
	u8 clk_chksum = 0;
	struct file *flp = NULL;

	mm_segment_t old_fs1;

	old_fs1 = get_fs();
	set_fs(KERNEL_DS);

	/* check clk legality */
	ret = gtp_check_clk_legality();
	if (ret == SUCCESS)
		goto send_main_clk;

	GTP_DEBUG("[gtp main_clk_proc]Waiting for FS %d", gtp_ref_retries);
	if (gup_check_fs_mounted("/data") == FAIL) {
		GTP_DEBUG("[gtp main_clk_proc]/data not mounted");
		if (gtp_clk_retries++ < GTP_CHK_FS_MNT_MAX) {
			set_fs(old_fs1);
			return FAIL;
		}
		GTP_ERROR(
			"[gtp main_clk_proc]Wait for file system timeout,need cal clk");
	} else {
		GTP_DEBUG("[gtp main_clk_proc]/data mounted !!!!");
		flp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0666);
		if (!IS_ERR(flp)) {
			flp->f_op->llseek(flp, 0, SEEK_SET);
			ret = flp->f_op->read(flp, (char *)gtp_clk_buf, 6,
					      &flp->f_pos);
			if (ret > 0) {
				ret = gtp_check_clk_legality();
				if (ret == SUCCESS) {
					GTP_DEBUG(
						"[gtp main_clk_proc]Open & read & check clk file success.");
					goto send_main_clk;
				}
			}
		}
		GTP_ERROR(
			"[gtp main_clk_proc]Check clk file failed,need cal clk");
	}

/* cal clk */
#ifdef CONFIG_GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_OFF);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
	gtp_charger_switch(0);
#endif
	clk_cal_result = gup_clk_calibration();
	force_reset_guitar();
	GTP_DEBUG("clk cal result:%d", clk_cal_result);

#ifdef CONFIG_GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_ON);
#endif
#ifdef CONFIG_GTP_CHARGER_DETECT
	gtp_charger_switch(1);
#endif
	if (clk_cal_result < 50 || clk_cal_result > 120) {
		GTP_ERROR("[gtp main_clk_proc]cal clk result is illegitimate");
		ret = FAIL;
		goto exit_clk_proc;
	}

	for (i = 0; i < 5; i++) {
		gtp_clk_buf[i] = clk_cal_result;
		clk_chksum += gtp_clk_buf[i];
	}
	gtp_clk_buf[5] = 0 - clk_chksum;

	if (IS_ERR(flp)) {
		flp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0666);
	} else {
		flp->f_op->llseek(flp, 0, SEEK_SET);
		flp->f_op->write(flp, (char *)gtp_clk_buf, 6, &flp->f_pos);
	}

send_main_clk:

	ret = i2c_write_bytes(client, 0x8020, gtp_clk_buf, 6);
	if (-1 == ret) {
		GTP_ERROR("[gtp main_clk_proc]send main clk i2c error!");
		ret = FAIL;
		goto exit_clk_proc;
	}

	ret = SUCCESS;

exit_clk_proc:
	set_fs(old_fs1);
	if (flp && !IS_ERR(flp))
		filp_close(flp, NULL);

	return ret;
}

#endif
/* ************* For GT9XXF End ***********************/

static const struct file_operations gt_upgrade_proc_fops = {
	.write = gt91xx_config_write_proc, .read = gt91xx_config_read_proc};
static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	unsigned long irqf_val = 0;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL,
	"mediatek,cap_touch"); /* 0704 */
	/*node = of_find_matching_node(NULL, touch_of_match);*/
	if (node) {
		/*touch_irq = gpio_to_irq(tpd_int_gpio);*/
		touch_irq = irq_of_parse_and_map(node, 0);

		irqf_val =
			!int_type ? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;

		ret = request_irq(touch_irq, tpd_interrupt_handler, irqf_val,
				  TPD_DEVICE, NULL);
		if (ret < 0)
			GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
		else
			atomic_set(&irq_enabled, 1);
	} else {
		GTP_ERROR(
			"[%s] tpd request_irq can not find touch eint device node!.",
			__func__);
	}
	return ret;
}

static s32 tpd_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
	s32 idx = 0;
#endif
	s32 err = 0;
	s32 ret = 0;
#ifdef CONFIG_GTP_PROXIMITY
	struct hwmsen_object obj_ps;
#endif

	i2c_client_point = client;
	of_get_gt9xx_platform_data(&client->dev);

	ret = gtp_get_gpio_res();
	if (ret < 0) {
		GTP_ERROR("Failed to get gpio resources");
		return ret;
	}

	ret = tpd_power_on(client);
	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		goto out;
	}

#if 0
	ret = gtp_init_panel(client);
	if (ret < 0)
		GTP_ERROR("GTP init panel failed.");
#else
	init_panel_thread = kthread_run(gtp_init_panel,
		(void *)client, "gtp_init_panel");
	if (IS_ERR(init_panel_thread)) {
		err = PTR_ERR(init_panel_thread);
		GTP_INFO(TPD_DEVICE
			" failed to create auto-update thread: %d\n", err);
	}
#endif
	GTP_DEBUG("gtp_init_panel success");
	/* Create proc file system */
	gt91xx_config_proc = proc_create(GT91XX_CONFIG_PROC_FILE, 0660, NULL,
					 &gt_upgrade_proc_fops);
	if (gt91xx_config_proc == NULL) {
		GTP_ERROR("create_proc_entry %s failed",
			  GT91XX_CONFIG_PROC_FILE);
	}

#ifdef CONFIG_GTP_CREATE_WR_NODE
	init_wr_node(client);
#endif

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_ERROR(TPD_DEVICE " failed to create kernel thread: %d",
			  err);
	}

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
	for (idx = 0; idx < GTP_MAX_KEY_NUM; idx++)
		input_set_capability(tpd->dev, EV_KEY, touch_key_array[idx]);

#endif

#ifdef CONFIG_GTP_WITH_HOVER
	gtp_pen_init();
#endif

#ifdef CONFIG_GTP_GESTURE_WAKEUP
	gtp_extents_init();
	input_set_capability(tpd->dev, EV_KEY, KEY_F2);
	input_set_capability(tpd->dev, EV_KEY, KEY_F3);
#endif

	/* msleep(50); */
	tpd_irq_registration();
/*gtp_irq_enable();*/

#ifdef CONFIG_GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_ON);
#endif

#ifdef CONFIG_GTP_CHARGER_DETECT
	gtp_charger_switch(1);
#endif

#ifdef CONFIG_GTP_AUTO_UPDATE
	ret = gup_init_update_proc(client);
	if (ret < 0)
		GTP_ERROR("Create update thread error.");
#endif

#ifdef CONFIG_GTP_PROXIMITY
	/* obj_ps.self = cm3623_obj; */
	obj_ps.polling = 0; /* 0--interrupt mode;1--polling mode; */
	obj_ps.sensor_operate = tpd_ps_operate;

	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err)
		GTP_ERROR("hwmsen attach fail, return:%d.", err);
#endif

	tpd_load_status = 1;
	GTP_INFO("%s, success run Done", __func__);
	return 0;
out:
	gtp_free_gpio_res();
	return -1;
}

static irqreturn_t tpd_interrupt_handler(int irq, void *dev_id)
{
	TPD_DEBUG_PRINT_INT;

	if (atomic_cmpxchg(&irq_enabled, 1, 0))
		disable_irq_nosync(touch_irq);

	tpd_flag = 1;
	wake_up_interruptible(&waiter);

	return IRQ_HANDLED;
}
static int tpd_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_GTP_CREATE_WR_NODE
	uninit_wr_node();
#endif

#ifdef CONFIG_GTP_GESTURE_WAKEUP
	gtp_extents_exit();
#endif
	if (gtp_workqueue)
		destroy_workqueue(gtp_workqueue);

	gtp_free_gpio_res();
	return 0;
}
#if (defined(CONFIG_GTP_ESD_PROTECT) || defined(CONFIG_GTP_COMPATIBLE_MODE))
void force_reset_guitar(void)
{
	s32 i = 0;
	s32 ret = 0;

	if (gtp_resetting || (gtp_loading_fw == 1))
		return;

	GTP_INFO("force_reset_guitar");
	gtp_irq_disable();

	gtp_gpio_output(GTP_RST_GPIO, 0);
	gtp_gpio_output(GTP_IRQ_GPIO, 0);

	/* Power off TP */
	ret = regulator_disable(tpd->reg);
	if (ret != 0)
		TPD_ERR("Failed to disable reg-vgp6: %d\n", ret);

	/* msleep(30); */
	/* Power on TP */
	ret = regulator_enable(tpd->reg);
	if (ret != 0)
		TPD_ERR("Failed to enable reg-vgp6: %d\n", ret);
	/* msleep(30); */

	for (i = 0; i < 5; i++) {
		gtp_reset_guitar(i2c_client_point, 20);

#ifdef CONFIG_GTP_COMPATIBLE_MODE
		if (gtp_chip_type == CHIP_TYPE_GT9F) {
			/* check code ram */
			ret = gup_fw_download_proc(NULL, GTP_FL_ESD_RECOVERY);
			if (ret == FAIL) {
				GTP_ERROR(
					"[force reset_guitar]Check & repair fw failed.");
				continue;
			}

			ret = gtp_fw_startup(i2c_client_point);
			if (ret == FAIL) {
				GTP_ERROR(
					"[force reset_guitar]Startup fw failed.");
				continue;
			}
		} else
#endif
		{
			/* Send config */
			ret = gtp_send_cfg(i2c_client_point);
			if (ret < 0)
				continue;
		}
		break;
	}

	gtp_irq_enable();
}
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
static s32 gtp_init_ext_watchdog(struct i2c_client *client)
{
	u8 opr_buffer[2] = {0xAA};

	GTP_DEBUG("Init external watchdog.");
	return i2c_write_bytes(client, 0x8041, opr_buffer, 1);
}

void gtp_esd_switch(struct i2c_client *client, s32 on)
{
	spin_lock(&esd_lock);
	if (on == SWITCH_ON) /* switch on esd */ {
		if (!esd_running) {
			esd_running = 1;
			spin_unlock(&esd_lock);
			GTP_INFO("Esd started");
			queue_delayed_work(gtp_workqueue, &gtp_esd_check_work,
					   clk_tick_cnt);
		} else {
			spin_unlock(&esd_lock);
		}
	} else { /* switch off esd */
		if (esd_running) {
			esd_running = 0;
			spin_unlock(&esd_lock);
			GTP_INFO("Esd cancelled");
			cancel_delayed_work_sync(&gtp_esd_check_work);
		} else {
			spin_unlock(&esd_lock);
		}
	}
}

static void gtp_esd_check_func(struct work_struct *work)
{
	s32 i = 0;
	s32 ret = -1;
	u8 esd_buf[3] = {0x00};

	if (tpd_halt) {
		GTP_INFO("Esd suspended!");
		return;
	}
	if (gtp_loading_fw == 1) {
		GTP_INFO("Load FW process is running");
		return;
	}
	for (i = 0; i < 3; i++) {
		ret = i2c_read_bytes_non_dma(i2c_client_point, 0x8040, esd_buf,
					     2);

		GTP_DEBUG("[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X", esd_buf[0],
			  esd_buf[1]);
		if (ret < 0) {
			/* IIC communication problem */
			continue;
		} else {
			if ((esd_buf[0] == 0xAA) || (esd_buf[1] != 0x00)) {
				u8 chk_buf[2] = {0x00};

				i2c_read_bytes_non_dma(i2c_client_point, 0x8040,
						       chk_buf, 2);
				GTP_DEBUG(
					"[Check]0x8040 = 0x%02X, 0x8041 = 0x%02X",
					chk_buf[0], chk_buf[1]);

				if ((chk_buf[0] == 0xAA) ||
				    (chk_buf[1] != 0xAA)) {
					i = 3; /* jump to reset guitar */
					break;
				}
				continue;
			} else {
				/* IC works normally, Write 0x8040 0xAA, feed */
				/* the watchdog */
				esd_buf[0] = 0xAA;
				i2c_write_bytes_non_dma(i2c_client_point,
							0x8040, esd_buf, 1);

				break;
			}
		}
	}

	if (i >= 3) {
#ifdef CONFIG_GTP_COMPATIBLE_MODE
		if ((gtp_chip_type == CHIP_TYPE_GT9F) &&
		    (rqst_processing == 1)) {
			GTP_INFO("Request Processing, no reset guitar.");
		} else
#endif
		{
			GTP_INFO("IC works abnormally! Process reset guitar.");
			memset(esd_buf, 0x01, sizeof(esd_buf));
			i2c_write_bytes(i2c_client_point, 0x4226, esd_buf,
					sizeof(esd_buf));
			msleep(50);
			force_reset_guitar();
		}
	}

	if (!tpd_halt && esd_running)
		queue_delayed_work(gtp_workqueue, &gtp_esd_check_work,
				   clk_tick_cnt);
	else
		GTP_INFO("Esd suspended!");

}
#endif

#ifdef CONFIG_GTP_WITH_HOVER
static void gtp_pen_init(void)
{
	s32 ret = 0;

	pen_dev = input_allocate_device();
	if (pen_dev == NULL) {
		GTP_ERROR("Failed to allocate input device for pen/stylus.");
		return;
	}

	pen_dev->evbit[0] =
		BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	pen_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	set_bit(BTN_TOOL_PEN, pen_dev->keybit);

#if GTP_PEN_HAVE_BUTTON
	input_set_capability(pen_dev, EV_KEY, BTN_STYLUS);
	input_set_capability(pen_dev, EV_KEY, BTN_STYLUS2);
#endif

	input_set_abs_params(pen_dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	// input_set_abs_params(pen_dev, ABS_MT_DISTANCE, 0, 1, 0, 0);
	pen_dev->name = "goodix-pen";
	pen_dev->phys = "input/ts";
	pen_dev->id.bustype = BUS_I2C;

	ret = input_register_device(pen_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", pen_dev->name);
		return;
	}
}

static void gtp_pen_down(s32 x, s32 y, s32 size, s32 id)
{
	input_report_key(pen_dev, BTN_TOOL_PEN, 1);
	input_report_abs(pen_dev, ABS_MT_POSITION_X, x);
	input_report_abs(pen_dev, ABS_MT_POSITION_Y, y);
	input_report_key(pen_dev, BTN_TOUCH, 0);
	GTP_INFO("Goodix Pen Down");
	input_mt_sync(pen_dev);
}

static void gtp_pen_up(void)
{
	input_report_key(pen_dev, BTN_TOOL_PEN, 0);
	input_report_key(pen_dev, BTN_TOUCH, 0);
}
#endif

static int tpd_history_x = 0, tpd_history_y;
static void tpd_down(s32 x, s32 y, s32 size, s32 id)
{
	if ((!size) && (!id)) {
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, 100);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
	} else {
		if ((id & 0x80)) { // pen
			id = 10;
			if (!size) {
				input_report_abs(tpd->dev, ABS_MT_PRESSURE,
						 0); // hover
				input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,
						 100);
			} else {
				input_report_abs(tpd->dev, ABS_MT_PRESSURE,
						 size);
				input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,
						 size);
			}
			input_report_abs(tpd->dev, ABS_MT_TOOL_TYPE, 1);
		} else { // finger
			id = id & 0x0F;
			input_report_abs(tpd->dev, ABS_MT_TOOL_TYPE, 0);
			input_report_abs(tpd->dev, ABS_MT_PRESSURE, size);
			input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
		}
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
	}

	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(x, y, x, y, id, 1);
	tpd_history_x = x;
	tpd_history_y = y;

	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 1, x+y); */
	if (tpd_dts_data.use_tpd_button) {
		if (get_boot_mode() == FACTORY_BOOT ||
		    get_boot_mode() == RECOVERY_BOOT)
			tpd_button(x, y, 1);
	}
}

static void tpd_up(s32 x, s32 y, s32 id)
{
	/* input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0); */
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	/* input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0); */
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y,
		     id, 0);
	tpd_history_x = 0;
	tpd_history_y = 0;
	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 0, x+y); */

	if (tpd_dts_data.use_tpd_button) {
		if (get_boot_mode() == FACTORY_BOOT ||
		    get_boot_mode() == RECOVERY_BOOT)
			tpd_button(x, y, 0);
	}
}

#ifdef CONFIG_GTP_CHARGER_DETECT
static atomic_t gtp_chr_running = ATOMIC_INIT(0);
void gtp_charger_switch(int on)
{
	if (on) {
		GTP_INFO("Charger On");
		if (!atomic_cmpxchg(&gtp_chr_running, 0, 1))
			queue_delayed_work(gtp_workqueue,
					   &gtp_charger_check_work,
					   clk_tick_cnt);
	} else {
		GTP_INFO("Charger Off");
		if (atomic_cmpxchg(&gtp_chr_running, 1, 0))
			cancel_delayed_work_sync(&gtp_charger_check_work);
	}
}

static void gtp_charger_config_check(s32 dir_update)
{
	u32 chr_status = 0;
	u8 chr_cmd[3] = {0x80, 0x40};
	static u8 chr_pluggedin;
	int ret = 0;

#ifdef MT6573
	chr_status = *(u32 *)CHR_CON0;
	chr_status &= (1 << 13);
#else /* ( defined(MT6575) || defined(MT6577) || defined(MT6589) ) */
	chr_status = upmu_is_chr_det();
#endif
	GTP_INFO("Check status for Charger");
	if (chr_status) { /* charger plugged in */
		if (!chr_pluggedin || dir_update) {
			chr_cmd[2] = 6;
			ret = gtp_i2c_write(i2c_client_point, chr_cmd, 3);
			if (ret > 0) {
				GTP_INFO("Update status for Charger Plugin");
				if (gtp_send_chr_cfg(i2c_client_point) < 0)
					GTP_ERROR(
						"Send charger config failed.");
				else
					GTP_DEBUG("Send charger config.");
			}
			chr_pluggedin = 1;
		}
	} else { /* charger plugged out */
		if (chr_pluggedin || dir_update) {
			chr_cmd[2] = 7;
			ret = gtp_i2c_write(i2c_client_point, chr_cmd, 3);
			if (ret > 0) {
				GTP_INFO("Update status for Charger Plugout");
				if (gtp_send_cfg(i2c_client_point) < 0)
					GTP_ERROR("Send normal config failed.");
				else
					GTP_DEBUG("Send normal config.");
			}
			chr_pluggedin = 0;
		}
	}
}

static void gtp_charger_check_func(struct work_struct *work)
{
	if (!atomic_read(&gtp_chr_running))
		return;

	gtp_charger_config_check(0);

	if (atomic_read(&gtp_chr_running))
		queue_delayed_work(gtp_workqueue, &gtp_charger_check_work,
				   clk_tick_cnt);
}
#endif

static int touch_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = 4};
	u8 end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
	u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {
		GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
	u8 touch_num = 0, finger = 0, key_value = 0, *coor_data = NULL;
	static u8 pre_touch, pre_key;

	s32 input_x = 0, input_y = 0, input_w = 0;
	s32 id = 0, i = 0, ret = -1;
#ifdef CONFIG_GTP_WITH_HOVER
	u8 pen_active = 0;
	static u8 pre_pen;
#endif
	u8 pre_finger = 0;
	u8 dev_active = 0;
#ifdef CONFIG_HOTKNOT_BLOCK_RW
	u8 hn_state_buf[10] = {(u8)(GTP_REG_HN_STATE >> 8),
			       (u8)(GTP_REG_HN_STATE & 0xFF), 0};
	u8 hn_pxy_state = 0, hn_pxy_state_bak = 0;
	u8 hn_paired_cnt = 0;
#endif

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		if (tpd_eint_mode) {
			wait_event_interruptible(waiter, tpd_flag != 0);
			tpd_flag = 0;
		} else {
			msleep(tpd_polling_time);
		}

		set_current_state(TASK_RUNNING);
		mutex_lock(&i2c_access);

#ifdef CONFIG_GTP_GESTURE_WAKEUP
		if (gesture_data.enabled) {
			ret = gesture_event_handler(tpd->dev);
			if (ret > 0) { /* event handled */
				gtp_irq_enable();
				mutex_unlock(&i2c_access);
				continue;
			}
		}
#endif

		if (tpd_halt || gtp_resetting || gtp_loading_fw) {
			GTP_DEBUG("Interrupt exit,halt:%d,reset:%d,ld_fw:%d",
				  tpd_halt, gtp_resetting, gtp_loading_fw);
			goto exit_unlock;
		}

		ret = gtp_i2c_read(i2c_client_point, point_data, 12);
		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
			goto exit_unlock;
		}
		finger = point_data[GTP_ADDR_LENGTH];

#ifdef CONFIG_GTP_COMPATIBLE_MODE
		if ((finger == 0x00) && (gtp_chip_type == CHIP_TYPE_GT9F)) {
			u8 rqst_data[3] = {(u8)(GTP_REG_RQST >> 8),
					   (u8)(GTP_REG_RQST & 0xFF), 0};

			ret = gtp_i2c_read(i2c_client_point, rqst_data, 3);
			if (ret < 0) {
				GTP_ERROR("I2C transfer error. errno:%d\n ",
					  ret);
				goto exit_unlock;
			}

			switch (rqst_data[2] & 0x0F) {
			case GTP_RQST_BAK_REF:
				GTP_INFO("Request Ref.");
				ret = gtp_bak_ref_proc(i2c_client_point,
						       GTP_BAK_REF_SEND);
				if (ret == SUCCESS) {
					GTP_INFO("Send ref success.");
					rqst_data[2] = GTP_RQST_RESPONDED;
					gtp_i2c_write(i2c_client_point,
						      rqst_data, 3);
				}
				goto exit_work_func;
			case GTP_RQST_CONFIG:
				GTP_INFO("Request Config.");
				ret = gtp_send_cfg(i2c_client_point);
				if (ret < 0) {
					GTP_ERROR("Send config error.");
				} else {
					GTP_INFO("Send config success.");
					rqst_data[2] = GTP_RQST_RESPONDED;
					gtp_i2c_write(i2c_client_point,
						      rqst_data, 3);
				}
				goto exit_work_func;
			case GTP_RQST_MAIN_CLOCK:
				GTP_INFO("Request main clock.");
				rqst_processing = 1;
				ret = gtp_main_clk_proc(i2c_client_point);
				if (ret == SUCCESS) {
					GTP_INFO("Send main clk success.");
					rqst_data[2] = GTP_RQST_RESPONDED;
					gtp_i2c_write(i2c_client_point,
						      rqst_data, 3);
					rqst_processing = 0;
				}
				goto exit_work_func;
			case GTP_RQST_RESET:
				GTP_INFO("Request Reset.");
				mutex_unlock(&i2c_access);
				gtp_recovery_reset(i2c_client_point);
				goto exit_work_func;
			default:
				break;
			}
		}
#endif

#ifdef CONFIG_GTP_HOTKNOT
		if (finger == 0x00 && gtp_hotknot_enabled) {
			u8 rqst_data[3] = {(u8)(GTP_REG_RQST >> 8),
					   (u8)(GTP_REG_RQST & 0xFF), 0};

			ret = gtp_i2c_read(i2c_client_point, rqst_data, 3);
			if (ret < 0) {
				GTP_ERROR("I2C transfer error. errno:%d\n ",
					  ret);
				goto exit_unlock;
			}

			if ((rqst_data[2] & 0x0F) == GTP_RQST_HOTKNOT_CODE) {
				GTP_INFO("Request HotKnot Code.");
				gup_load_hotknot_fw();
				goto exit_unlock;
			}
		}
#endif

		if ((finger & 0x80) == 0) {
#ifdef CONFIG_HOTKNOT_BLOCK_RW
			if (!hotknot_paired_flag) {
#else
			{
#endif
				GTP_DEBUG("buffer not ready");
				goto exit_unlock;
			}
		}

#ifdef CONFIG_HOTKNOT_BLOCK_RW
		if (!hotknot_paired_flag && (finger & 0x0F)) {
			id = point_data[GTP_ADDR_LENGTH + 1];
			hn_pxy_state = point_data[GTP_ADDR_LENGTH + 2] & 0x80;
			hn_pxy_state_bak =
				point_data[GTP_ADDR_LENGTH + 3] & 0x80;
			if ((id == 32) && (hn_pxy_state == 0x80) &&
			    (hn_pxy_state_bak == 0x80)) {
#ifdef HN_DBLCFM_PAIRED
				if (hn_paired_cnt++ < 2)
					goto exit_work_func;
#endif
				GTP_DEBUG("HotKnot paired!");
				if (wait_hotknot_state & HN_DEVICE_PAIRED) {
					GTP_DEBUG(
						"INT wakeup HN_DEVICE_PAIRED block polling waiter");
					got_hotknot_state |= HN_DEVICE_PAIRED;
					wake_up_interruptible(&bp_waiter);
				}
				hotknot_paired_flag = 1;
				goto exit_work_func;
			} else {
				got_hotknot_state &= (~HN_DEVICE_PAIRED);
				hn_paired_cnt = 0;
			}
		}

		if (hotknot_paired_flag) {
			ret = gtp_i2c_read(i2c_client_point, hn_state_buf, 6);

			if (ret < 0) {
				GTP_ERROR("I2C transfer error. errno:%d\n ",
					  ret);
				goto exit_unlock;
			}

			got_hotknot_state = 0;

			GTP_DEBUG("[0xAB10~0xAB13]=0x%x,0x%x,0x%x,0x%x",
				  hn_state_buf[GTP_ADDR_LENGTH],
				  hn_state_buf[GTP_ADDR_LENGTH + 1],
				  hn_state_buf[GTP_ADDR_LENGTH + 2],
				  hn_state_buf[GTP_ADDR_LENGTH + 3]);

			if (wait_hotknot_state & HN_MASTER_SEND) {
				if ((hn_state_buf[GTP_ADDR_LENGTH] == 0x03) ||
				    (hn_state_buf[GTP_ADDR_LENGTH] == 0x04) ||
				    (hn_state_buf[GTP_ADDR_LENGTH] == 0x07)) {
					GTP_DEBUG(
						"Wakeup HN_MASTER_SEND block polling waiter");
					got_hotknot_state |= HN_MASTER_SEND;
					got_hotknot_extra_state =
						hn_state_buf[GTP_ADDR_LENGTH];
					wake_up_interruptible(&bp_waiter);
				}
			} else if (wait_hotknot_state & HN_SLAVE_RECEIVED) {
				if ((0x03 ==
				     hn_state_buf[GTP_ADDR_LENGTH + 1]) ||
				    (0x04 ==
				     hn_state_buf[GTP_ADDR_LENGTH + 1]) ||
				    (0x07 ==
				     hn_state_buf[GTP_ADDR_LENGTH + 1])) {
					GTP_DEBUG(
						"Wakeup HN_SLAVE_RECEIVED block polling waiter:0x%x",
						hn_state_buf[GTP_ADDR_LENGTH +
							     1]);
					got_hotknot_state |= HN_SLAVE_RECEIVED;
					got_hotknot_extra_state =
						hn_state_buf[GTP_ADDR_LENGTH +
							     1];
					wake_up_interruptible(&bp_waiter);
				}
			} else if (wait_hotknot_state & HN_MASTER_DEPARTED) {
				if (hn_state_buf[GTP_ADDR_LENGTH] == 0x07) {
					GTP_DEBUG(
						"Wakeup HN_MASTER_DEPARTED block polling waiter");
					got_hotknot_state |= HN_MASTER_DEPARTED;
					wake_up_interruptible(&bp_waiter);
				}
			} else if (wait_hotknot_state & HN_SLAVE_DEPARTED) {
				if (hn_state_buf[GTP_ADDR_LENGTH + 1] == 0x07) {
					GTP_DEBUG(
						"Wakeup HN_SLAVE_DEPARTED block polling waiter");
					got_hotknot_state |= HN_SLAVE_DEPARTED;
					wake_up_interruptible(&bp_waiter);
				}
			}
		}

#endif

#ifdef CONFIG_GTP_PROXIMITY
		if (tpd_proximity_flag == 1) {
			struct hwm_sensor_data sensor_data;
			u8 proximity_status = point_data[GTP_ADDR_LENGTH];

			GTP_DEBUG("REG INDEX[0x814E]:0x%02X\n",
				  proximity_status);
			/* proximity or large touch detect,enable hwm_sensor. */
			if (proximity_status & 0x60)
				tpd_proximity_detect = 0;
			else
				tpd_proximity_detect = 1;

			/* get raw data */
			GTP_DEBUG("PS change,PROXIMITY STATUS:0x%02X\n",
				  tpd_proximity_detect);
			/* map and store data to hwm_sensor_data */
			sensor_data.values[0] = tpd_get_ps_value();
			sensor_data.value_divide = 1;
			sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
			/* report to the up-layer */
			ret = hwmsen_get_interrupt_data(ID_PROXIMITY,
							&sensor_data);

			if (ret)
				GTP_ERROR(
					"Call hwmsen_get_interrupt_data fail = %d\n",
					ret);
		}

#endif

		touch_num = finger & 0x0f;
		if (touch_num > GTP_MAX_TOUCH) {
			GTP_ERROR("Bad number of fingers!");
			goto exit_work_func;
		}

		if (touch_num > 1) {
			u8 buf[8 * GTP_MAX_TOUCH] = {
				(GTP_READ_COOR_ADDR + 10) >> 8,
				(GTP_READ_COOR_ADDR + 10) & 0xff};

			ret = gtp_i2c_read(i2c_client_point, buf,
					   2 + 8 * (touch_num - 1));
			if (ret < 0)
				goto exit_unlock;
			memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
		}

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
		key_value = point_data[3 + 8 * touch_num];

		if (key_value || pre_key) {
			for (i = 0; i < TPD_KEY_COUNT; i++) {
				input_report_key(tpd->dev, touch_key_array[i],
						 key_value & (0x01 << i));
			}

			if ((pre_key == 0x20) || (key_value == 0x20) ||
			    (pre_key == 0x10) || (key_value == 0x10) ||
			    (pre_key == 0x40) || (key_value == 0x40)) {
				// do nothing
			} else {
				touch_num = 0;
			}
			dev_active = 1;
		}

#endif
		pre_key = key_value;
		GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);
		if (touch_num) {
			for (i = 0; i < touch_num; i++) {
				coor_data = &point_data[i * 8 + 3];
				if (coor_data[0] == 32)
					goto exit_work_func;

				id = coor_data[0];
				input_x = coor_data[1] | coor_data[2] << 8;
				input_y = coor_data[3] | coor_data[4] << 8;
				input_w = coor_data[5] | coor_data[6] << 8;

				input_x = TPD_WARP_X(abs_x_max, input_x);
				input_y = TPD_WARP_Y(abs_y_max, input_y);
#ifdef CONFIG_GTP_WITH_HOVER
				id = coor_data[0];
				if ((id & 0x80) &&
				    !input_w) {/* pen/stylus is activated */
					GTP_DEBUG("Pen touch DOWN!");
					pre_pen = 1;
					GTP_DEBUG("(%d)(%d, %d)[%d]", id,
						  input_x, input_y, input_w);
					gtp_pen_down(input_x, input_y, input_w,
						     id);
					pen_active = 1;
					if (pre_finger) {
						tpd_up(0, 0, 0);
						dev_active = 1;
						pre_finger = 0;
						// input_sync(tpd->dev);
					}
				} else
#endif
				{
#ifdef CONFIG_GTP_WITH_HOVER
					if (pre_pen) {
						GTP_DEBUG("Pen touch UP!");
						gtp_pen_up();
						pre_pen = 0;
						pen_active = 1;
					}
#endif
					GTP_DEBUG(" (%d)(%d, %d)[%d]", id,
						  input_x, input_y, input_w);
					dev_active = 1;
					pre_finger = 1;
					tpd_down(input_x, input_y, input_w, id);
				}
			}
		} else if (pre_touch) {
#ifdef CONFIG_GTP_WITH_HOVER
			if (pre_pen) {
				GTP_DEBUG("Pen touch UP!");
				gtp_pen_up();
				pre_pen = 0;
				pen_active = 1;
			}
#endif
			if (pre_finger) {
				GTP_DEBUG("Touch Release!");
				dev_active = 1;
				pre_finger = 0;
				tpd_up(0, 0, 0);
			}
		} else {
			GTP_DEBUG("Additional Eint!");
		}
		pre_touch = touch_num;
/*
 *		if (tpd != NULL && tpd->dev != NULL)
 *			input_sync(tpd->dev);
 */
#ifdef CONFIG_GTP_WITH_HOVER
		if (pen_active) {
			pen_active = 0;
			input_sync(pen_dev);
		}
#endif
		if (dev_active) {
			dev_active = 0;
			input_sync(tpd->dev);
		}
exit_work_func:
		if (!gtp_rawdiff_mode) {
			ret = gtp_i2c_write(i2c_client_point, end_cmd, 3);
			if (ret < 0)
				GTP_INFO("I2C write end_cmd	error!");
		}

exit_unlock:
		gtp_irq_enable();
		mutex_unlock(&i2c_access);

	} while (!kthread_should_stop());

	return 0;
}

static int tpd_local_init(void)
{
	int retval;

	gtp_workqueue = create_workqueue("gtp-workqueue");
#ifdef CONFIG_GTP_ESD_PROTECT
	clk_tick_cnt =
		2 * HZ; /* HZ: clock ticks in 1 second generated by system */
	GTP_DEBUG("Clock ticks for an esd cycle: %d", clk_tick_cnt);
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	spin_lock_init(&esd_lock); /* 2.6.39 & later */
#endif

#ifdef CONFIG_GTP_CHARGER_DETECT
	gtp_workqueue_charger = create_workqueue("charger-workqueue");
	clk_tick_cnt_charger = 2 * HZ;
	GTP_DEBUG("Clock ticks for an charger cycle: %d", clk_tick_cnt_charger);
	INIT_DELAYED_WORK(&gtp_charger_check_work, gtp_charger_check_func);
#endif

#ifdef CONFIG_GTP_SUPPORT_I2C_DMA
	gpDMABuf_va = (u8 *)dma_alloc_coherent(
		NULL, GTP_DMA_MAX_TRANSACTION_LENGTH, &gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va)
		GTP_INFO("[Error] Allocate DMA I2C Buffer failed!\n");

	memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
#endif
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 2800000, 3300000);
	if (retval != 0) {
		TPD_ERR("Failed to set reg-vgp6 voltage: %d\n", retval);
		return -1;
	}
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		GTP_INFO("unable to add i2c driver.");
		return -1;
	}

	if (tpd_load_status == 0) {
		GTP_INFO("add error touch panel driver.");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0,
			     (GTP_MAX_TOUCH - 1), 0, 0);
	if (tpd_dts_data.use_tpd_button) {
		/*initialize tpd button data*/
		tpd_button_setting(tpd_dts_data.tpd_key_num,
				   tpd_dts_data.tpd_key_local,
				   tpd_dts_data.tpd_key_dim_local);
	}

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

	/* set vendor string */
	tpd->dev->id.vendor = 0x00;
	tpd->dev->id.product = tpd_info.pid;
	tpd->dev->id.version = tpd_info.vid;

	GTP_INFO("end %s, %d", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

/*
 *******************************************************
 *Function:
 *		Eter sleep function.
 *Input:
 *		client:i2c_client.
 *Output:
 *		Executive outcomes.0--success,non-0--fail.
 ********************************************************
 */
static s8 gtp_enter_sleep(struct i2c_client *client)
{
#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F) {
		u8 i2c_status_buf[3] = {0x80, 0x44, 0x00};
		s32 ret = 0;

		ret = gtp_i2c_read(client, i2c_status_buf, 3);
		if (ret <= 0)
			GTP_ERROR(
				"[gtp enter_sleep]Read ref status reg error.");

		if (i2c_status_buf[2] & 0x80) {
			/* Store bak ref */
			ret = gtp_bak_ref_proc(client, GTP_BAK_REF_STORE);
			if (ret == FAIL)
				GTP_ERROR(
					"[gtp enter_sleep]Store bak ref failed.");
		}
	}
#endif

#ifdef GTP_POWER_CTRL_SLEEP
	s32 ret_p = 0;

	gtp_gpio_output(GTP_RST_GPIO, 0);
	msleep(20);

	ret_p = regulator_disable(tpd->reg);
	if (ret_p != 0)
		TPD_ERR("Failed to disable reg-vgp6: %d\n", ret_p);

	GTP_INFO("GTP enter sleep by poweroff!");
	return 0;
#else
	{
		s8 ret = -1;
		s8 retry = 0;
		u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8),
					 (u8)GTP_REG_SLEEP, 5};

		gtp_gpio_output(GTP_IRQ_GPIO, 0);
		msleep(20);

		while (retry++ < 5) {
			ret = gtp_i2c_write(client, i2c_control_buf, 3);

			if (ret > 0) {
				GTP_INFO("GTP enter sleep!");
				return ret;
			}
			msleep(20);
		}

		GTP_ERROR("GTP send sleep cmd failed.");
		return ret;
	}
#endif
}

/*
 *******************************************************
 *Function:
 *		Wakeup from sleep mode Function.
 *Input:
 *		client:i2c_client.
 *Output:
 *		Executive outcomes.0--success,non-0--fail.
 ********************************************************
 */
static s8 gtp_wakeup_sleep(struct i2c_client *client)
{
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG("GTP wakeup begin.");

#ifdef GTP_POWER_CTRL_SLEEP

#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F) {
		force_reset_guitar();
		GTP_INFO("Esd recovery wakeup.");
		return 0;
	}
#endif

	while (retry++ < 5) {
		ret = tpd_power_on(client);
		if (ret < 0) {
			GTP_ERROR("I2C Power on ERROR!");
			continue;
		}
		GTP_INFO("Ic wakeup by poweron");
		return 0;
	}
#else

#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if (gtp_chip_type == CHIP_TYPE_GT9F) {
		u8 opr_buf[2] = {0};

		while (retry++ < 10) {
			gtp_gpio_output(GTP_IRQ_GPIO, 1);
			msleep(20);

			ret = gtp_i2c_test(client);
			if (ret >= 0) {
				/* Hold ss51 & dsp */
				opr_buf[0] = 0x0C;
				ret = i2c_write_bytes(client, 0x4180, opr_buf,
						      1);
				if (ret < 0) {
					GTP_DEBUG(
						"Hold ss51 & dsp I2C error,retry:%d",
						retry);
					continue;
				}

				/* Confirm hold */
				opr_buf[0] = 0x00;
				ret = i2c_read_bytes(client, 0x4180, opr_buf,
						     1);
				if (ret < 0) {
					GTP_DEBUG(
						"confirm ss51 & dsp hold, I2C error,retry:%d",
						retry);
					continue;
				}
				if (opr_buf[0] != 0x0C) {
					GTP_DEBUG(
						"ss51 & dsp not hold, val: %d, retry: %d",
						opr_buf[0], retry);
					continue;
				}
				GTP_DEBUG("ss51 & dsp has been hold");

				ret = gtp_fw_startup(client);
				if (ret == FAIL) {
					GTP_ERROR(
						"[gtp wakeup_sleep]Startup fw failed.");
					continue;
				}
				GTP_INFO("flashless wakeup sleep success");
				return ret;
			}
			force_reset_guitar();
			retry = 0;
			break;
		}
		if (retry >= 10) {
			GTP_ERROR("wakeup retry timeout, process esd reset");
			force_reset_guitar();
		}
		GTP_ERROR("GTP wakeup sleep failed.");
		return ret;
	}
#endif
	while (retry++ < 10) {
#ifdef CONFIG_GTP_GESTURE_WAKEUP
		if (gesture_data.enabled) {
			if (gesture_data.doze_status != DOZE_WAKEUP)
				GTP_INFO("Powerkey wakeup.");
			else
				GTP_INFO("Gesture wakeup.");

			gesture_data.doze_status = DOZE_DISABLED;

			gtp_irq_disable();
			gtp_reset_guitar(client, 20);
			gtp_irq_enable();
		} else {
#else
		{
#endif
			gtp_gpio_output(GTP_IRQ_GPIO, 1);
			msleep(20);
		}

		ret = gtp_i2c_test(client);
		if (ret >= 0) {
			GTP_INFO("GTP wakeup sleep.");
#ifndef CONFIG_GTP_GESTURE_WAKEUP
			gtp_int_sync(25);
#ifdef CONFIG_GTP_ESD_PROTECT
			gtp_init_ext_watchdog(client);
#endif
#endif
			return ret;
		}
		gtp_reset_guitar(client, 20);
	}
#endif
	GTP_ERROR("GTP wakeup sleep failed.");
	return ret;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct device *h)
{
	s32 ret = -1;

	GTP_INFO("System suspend.");
#ifdef CONFIG_GTP_PROXIMITY
	if (tpd_proximity_flag == 1)
		return;
#endif

#ifdef CONFIG_GTP_HOTKNOT
	if (gtp_hotknot_enabled) {
		u8 buf[3] = {0x81, 0xaa, 0};
#ifdef CONFIG_HOTKNOT_BLOCK_RW
		if (hotknot_paired_flag)
			return;
#endif
		/* check hotknot pair state */
		gtp_i2c_read(i2c_client_point, buf, sizeof(buf));
		if (buf[2] == 0x55) {
			GTP_INFO("GTP early suspend	pair success");
			return;
		}
	}
#endif

	tpd_halt = 1;
	mutex_lock(&i2c_access);
	gtp_irq_disable();

#ifdef CONFIG_GTP_ESD_PROTECT
	gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif

#ifdef CONFIG_GTP_CHARGER_DETECT
	gtp_charger_switch(0);
#endif

#ifdef CONFIG_GTP_GESTURE_WAKEUP
	if (gesture_data.enabled) {
		ret = gtp_enter_doze();
		gtp_irq_enable();
		enable_irq_wake(touch_irq);
	} else {
#else
	{
#endif
		ret = gtp_enter_sleep(i2c_client_point);
		if (ret < 0)
			GTP_ERROR("GTP early suspend failed.");
	}

	mutex_unlock(&i2c_access);
	msleep(58);
}

/* Function to manage power-on resume */
static void tpd_resume(struct device *h)
{
	s32 ret = -1;

	GTP_INFO("System resume.");
#ifdef CONFIG_GTP_PROXIMITY
	if (tpd_proximity_flag == 1)
		return;
#endif

#ifdef CONFIG_HOTKNOT_BLOCK_RW
	if (hotknot_paired_flag) {
		hotknot_paired_flag = 0;
		return;
	}
#endif

	if (gtp_loading_fw) {
		GTP_INFO("Loading fw, abort resume");
		return;
	}

	mutex_lock(&i2c_access);

	ret = gtp_wakeup_sleep(i2c_client_point);
	if (ret < 0)
		GTP_ERROR("GTP later resume failed.");

#ifdef CONFIG_GTP_HOTKNOT
	if (!gtp_hotknot_enabled) {
		u8 exit_slave_cmd = 0x28;

		GTP_DEBUG("hotknot is disabled,exit slave mode.");
		i2c_write_bytes_non_dma(i2c_client_point, 0x8046,
					&exit_slave_cmd, 1);
		i2c_write_bytes_non_dma(i2c_client_point, 0x8040,
					&exit_slave_cmd, 1);
	}
#endif

#ifndef CONFIG_GTP_GESTURE_WAKEUP
	gtp_irq_enable();
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
	gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif

#ifdef CONFIG_GTP_CHARGER_DETECT
	gtp_charger_config_check(1); /* force update */
	gtp_charger_switch(1);
#endif

	mutex_unlock(&i2c_access);
	tpd_halt = 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gt9xx",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static void tpd_off(void)
{

	int ret;

	ret = regulator_disable(tpd->reg);
	if (ret != 0)
		TPD_ERR("Failed to disable reg-vgp6: %d\n", ret);

	GTP_INFO("GTP enter sleep!");

	tpd_halt = 1;
	gtp_irq_disable();
}

static void tpd_on(void)
{
	s32 ret = -1, retry = 0;

	while (retry++ < 5) {
		ret = tpd_power_on(i2c_client_point);
		if (ret < 0)
			GTP_ERROR("I2C Power on ERROR!");

		ret = gtp_send_cfg(i2c_client_point);
		if (ret > 0)
			GTP_DEBUG("Wakeup sleep send config success.");
	}
	if (ret < 0)
		GTP_ERROR("GTP later resume failed.");

	gtp_irq_enable();
	tpd_halt = 0;
}

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
	GTP_INFO("GT9 series touch panel driver init");
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		GTP_INFO("add generic driver failed");

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	GTP_INFO("GT9 series touch panel driver exit");
	tpd_driver_remove(&tpd_device_driver);
}
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
MODULE_LICENSE(GTP v2);
MODULE_DESCRIPTION("GT9 Series Touch Panel Driver");
