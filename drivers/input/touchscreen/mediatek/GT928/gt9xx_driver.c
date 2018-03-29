/* drivers/input/touchscreen/gt9xx_driver.c
 *
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.2
 *      V1.0:2012/08/31,first release.
 *      V1.2:2012/10/15,add force update,GT9110P pid map
 */

#include "tpd.h"
#define GUP_FW_INFO
#include "tpd_custom_gt9xx.h"
#include <mt_boot_common.h>


#ifdef TPD_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

/* #include <linux/mmprofile.h> */
#include <linux/device.h>
#include <linux/proc_fs.h>	/*proc */


#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#define USE_REGULATOR_FRAMEWORK

/* extern struct tpd_device *tpd; */
/* #ifdef VELOCITY_CUSTOM */
/* extern int tpd_v_magnify_x; */
/* extern int tpd_v_magnify_y; */
/* #endif */
static int tpd_flag;
static int tpd_halt;
static int tpd_eint_mode = 1;
static int tpd_polling_time = 50;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);

unsigned int tpd_rst_gpio_number = 0;
unsigned int tpd_int_gpio_number = 0;

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

#if GTP_HAVE_TOUCH_KEY
const u16 touch_key_array[] = TPD_KEYS;
/* #define GTP_MAX_KEY_NUM ( sizeof( touch_key_array )/sizeof( touch_key_array[0] ) ) */
struct touch_virtual_key_map_t {
	int point_x;
	int point_y;
};
static struct touch_virtual_key_map_t touch_key_point_maping_array[] = GTP_KEY_MAP_ARRAY;
#endif


unsigned int touch_irq = 0;


#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
/* static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX; */
/* static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX; */
static int tpd_def_calmat_local_normal[8] = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif

/* s32 gtp_send_cfg(struct i2c_client *client); */

static irqreturn_t tpd_interrupt_handler(int irq, void *dev_id);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
static void tpd_on(void);
static void tpd_off(void);

#if GTP_CREATE_WR_NODE
/* extern s32 init_wr_node(struct i2c_client *); */
/* extern void uninit_wr_node(void); */
#endif

#ifdef GTP_CHARGER_DETECT
/* extern bool upmu_get_pchr_chrdet(void); */
#define TPD_CHARGER_CHECK_CIRCLE    50
static struct delayed_work gtp_charger_check_work;
static struct workqueue_struct *gtp_charger_check_workqueue;
static void gtp_charger_check_func(struct work_struct *);
static u8 gtp_charger_mode;
#endif

#if GTP_ESD_PROTECT
#define TPD_ESD_CHECK_CIRCLE        2000
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static void gtp_esd_check_func(struct work_struct *);
#endif

#ifdef TPD_PROXIMITY
#define TPD_PROXIMITY_VALID_REG                   0x814E
#define TPD_PROXIMITY_ENABLE_REG                  0x8042
static u8 tpd_proximity_flag;
static u8 tpd_proximity_detect = 1;	/* 0-->close ; 1--> far away */
#endif

#ifndef GTP_REG_REFRESH_RATE
#define GTP_REG_REFRESH_RATE		0x8056
#endif

u32 gtp_eint_trigger_type = IRQF_TRIGGER_FALLING;

struct i2c_client *i2c_client_point = NULL;
static const struct i2c_device_id tpd_i2c_id[] = { {"gt9xx", 0}, {} };
static unsigned short force[] = { 0, 0xBA, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */



static const struct of_device_id gt9xx_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};

MODULE_DEVICE_TABLE(of, gt9xx_dt_match);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		   .of_match_table = of_match_ptr(gt9xx_dt_match),
		   },
	.probe = tpd_i2c_probe,
	.remove = tpd_i2c_remove,
	.detect = tpd_i2c_detect,
	.driver.name = "gt9xx",
	.id_table = tpd_i2c_id,
	.address_list = (const unsigned short *)forces,
};
#ifdef CONFIG_OF
static int of_get_gt9xx_platform_data(struct device *dev)
{
	/*int ret, num;*/

	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(gt9xx_dt_match), dev);
		if (!match) {
			GTP_ERROR("Error: No device match found\n");
			return -ENODEV;
		}
	}
	tpd_rst_gpio_number = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	tpd_int_gpio_number = of_get_named_gpio(dev->of_node, "int-gpio", 0);
	/*ret = of_property_read_u32(dev->of_node, "rst-gpio", &num);
	if (!ret)
		tpd_rst_gpio_number = num;
	ret = of_property_read_u32(dev->of_node, "int-gpio", &num);
	if (!ret)
		tpd_int_gpio_number = num;
  */
	GTP_ERROR("g_vproc_en_gpio_number %d\n", tpd_rst_gpio_number);
	GTP_ERROR("g_vproc_vsel_gpio_number %d\n", tpd_int_gpio_number);
	return 0;
}
#else
static int of_get_gt9xx_platform_data(struct device *dev)
{
	return 0;
}
#endif
static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };

#ifdef GTP_CHARGER_DETECT
static u8 config_charger[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };
#endif
#pragma pack(1)
struct st_tpd_info {
	u16 pid;		/* product id    */
	u16 vid;		/* version id    */
};
#pragma pack()

struct st_tpd_info tpd_info;
u8 int_type = 0;
u32 abs_x_max = 0;
u32 abs_y_max = 0;
u8 gtp_rawdiff_mode = 0;
u8 cfg_len = 0;


/* proc file system */
/* s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len); */
/* s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len); */
static struct proc_dir_entry *gt91xx_config_proc;

/*******************************************************
Function:
	Write refresh rate

Input:
	rate: refresh rate N (Duration=5+N ms, N=0~15)

Output:
	Executive outcomes.0---succeed.
*******************************************************/
static u8 gtp_set_refresh_rate(u8 rate)
{
	u8 buf[3] = { GTP_REG_REFRESH_RATE >> 8, GTP_REG_REFRESH_RATE & 0xff, rate };

	if (rate > 0xf) {
		GTP_ERROR("Refresh rate is over range (%d)", rate);
		return FAIL;
	}

	GTP_INFO("Refresh rate change to %d", rate);
	return gtp_i2c_write(i2c_client_point, buf, sizeof(buf));
}

/*******************************************************
Function:
	Get refresh rate

Output:
	Refresh rate or error code
*******************************************************/
static u8 gtp_get_refresh_rate(void)
{
	int ret;

	u8 buf[3] = { GTP_REG_REFRESH_RATE >> 8, GTP_REG_REFRESH_RATE & 0xff };

	ret = gtp_i2c_read(i2c_client_point, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	GTP_INFO("Refresh rate is %d", buf[GTP_ADDR_LENGTH]);
	return buf[GTP_ADDR_LENGTH];
}

/* ============================================================= */
static ssize_t show_refresh_rate(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = gtp_get_refresh_rate();

	if (ret < 0)
		return 0;
	else
		return sprintf(buf, "%d\n", ret);
}

static ssize_t store_refresh_rate(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	unsigned long rate;

	gtp_set_refresh_rate(kstrtoul(buf, 16, &rate));
	return size;
}

static DEVICE_ATTR(tpd_refresh_rate, 0664, show_refresh_rate, store_refresh_rate);

static struct device_attribute *gt9xx_attrs[] = {
	&dev_attr_tpd_refresh_rate,
};

/* ============================================================= */

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, "mtk-tpd");
	return 0;
}

#ifdef TPD_PROXIMITY
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

	ret = i2c_write_bytes(i2c_client_point, TPD_PROXIMITY_ENABLE_REG, &state, 1);

	if (ret < 0) {
		GTP_ERROR("TPD %s proximity cmd failed.", state ? "enable" : "disable");
		return ret;
	}

	GTP_INFO("TPD proximity function %s success.", state ? "enable" : "disable");
	return 0;
}

s32 tpd_ps_operate(void *self, u32 command, void *buff_in, s32 size_in,
		   void *buff_out, s32 size_out, s32 *actualout)
{
	s32 err = 0;
	s32 value;
	hwm_sensor_data *sensor_data;

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
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			GTP_ERROR("Get sensor data parameter error!");
			err = -EINVAL;
		} else {
			sensor_data = (hwm_sensor_data *) buff_out;
			sensor_data->values[0] = tpd_get_ps_value();
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}

		break;

	default:
		GTP_ERROR("proxmy sensor operate function no this parameter %d!", command);
		err = -1;
		break;
	}

	return err;
}
#endif

static ssize_t gt91xx_config_read_proc(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = { 0 };
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
	i2c_read_bytes(i2c_client_point, GTP_REG_CONFIG_DATA, temp_data, GTP_CONFIG_MAX_LENGTH);

	for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
		ptr += sprintf(ptr, "0x%02X ", temp_data[i]);

		if (i % 8 == 7)
			ptr += sprintf(ptr, "\n");
	}
	/* Touch PID & VID */
	ptr += sprintf(ptr, "\n");
	ptr += sprintf(ptr, "==== GT9XX Version ID ====\n");
	i2c_read_bytes(i2c_client_point, GTP_REG_VERSION, temp_data, 6);
	ptr +=
	    sprintf(ptr, "Chip PID: %c%c%c  VID: 0x%02X%02X\n", temp_data[0], temp_data[1],
		    temp_data[2], temp_data[5], temp_data[4]);
	ptr += sprintf(ptr, "Driver VID: 0x%02X%02X\n", gtp_default_FW[12], gtp_default_FW[13]);
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

static ssize_t gt91xx_config_write_proc(struct file *file, const char *buffer, size_t count,
					loff_t *ppos)
{
	s32 ret = 0;
	char temp[25] = { 0 };	/* for store special format cmd */
	char mode_str[15] = { 0 };
	unsigned int mode;
	u8 buf[1];

	GTP_DEBUG("write count %ld\n", (unsigned long)count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("size not match [%d:%ld]", GTP_CONFIG_MAX_LENGTH, (unsigned long)count);
		return -EFAULT;
	}

    /**********************************************/
	/* for store special format cmd  */
	if (copy_from_user(temp, buffer, sizeof(temp))) {
		GTP_ERROR("copy from user fail 2");
		return -EFAULT;
	}
	if (sscanf(temp, "%s %d", (char *)&mode_str, &mode) == -1)
		return -EINVAL;

    /***********POLLING/EINT MODE switch****************/
	if (strcmp(mode_str, "polling") == 0) {
		if (mode >= 10 && mode <= 200) {
			GTP_INFO("Switch to polling mode, polling time is %d", mode);
			tpd_eint_mode = 0;
			tpd_polling_time = mode;
			tpd_flag = 1;
			wake_up_interruptible(&waiter);
		} else {
			GTP_INFO("Wrong polling time, please set between 10~200ms");
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
		if (mode == 0)	/* turn off */
			tpd_off();
		else if (mode == 1)	/* turn on */
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
		GTP_ERROR("copy from user fail");
		return -EFAULT;
	}

    /***********clk operate reseved****************/
    /**********************************************/
	ret = gtp_send_cfg(i2c_client_point);
	abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
	abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
	int_type = (config[TRIGGER_LOC]) & 0x03;

	if (ret < 0)
		GTP_ERROR("send config failed.");

	return count;
}

int i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
	u8 buffer[GTP_ADDR_LENGTH];
	u16 left = len;
	u16 offset = 0;

	struct i2c_msg msg[2] = {
		{
		 .addr = (client->addr),
		 /* .addr = (client->addr &I2C_MASK_FLAG), */
		 /* .ext_flag = I2C_ENEXT_FLAG, */
		 /* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
		 .flags = 0,
		 .buf = buffer,
		 .len = GTP_ADDR_LENGTH},
		{
		 .addr = (client->addr),
		 /* .addr = (client->addr &I2C_MASK_FLAG), */
		 /* .ext_flag = I2C_ENEXT_FLAG, */
		 /* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
		 .flags = I2C_M_RD},
	};

	if (rxbuf == NULL)
		return -1;

	GTP_ERROR("i2c_read_bytes to device %02X address %04X len %d", client->addr, addr, len);

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

		if (i2c_transfer(client->adapter, &msg[0], 2) != 2) {
			GTP_ERROR("I2C read 0x%X length=%d failed", addr + offset, len);
			return -1;
		}
	}

	return 0;
}

s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = (buf[0] << 8) + buf[1];

	ret = i2c_read_bytes(client, addr, &buf[2], len - 2);

	if (!ret)
		return 2;

	gtp_reset_guitar(client, 20);
	return ret;
}

int i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
	u8 buffer[MAX_TRANSACTION_LENGTH];
	u16 left = len;
	u16 offset = 0;

	struct i2c_msg msg = {
		.addr = (client->addr),
		/* .addr = (client->addr &I2C_MASK_FLAG), */
		/* .ext_flag = I2C_ENEXT_FLAG, */
		/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
		.flags = 0,
		.buf = buffer,
	};


	if (txbuf == NULL)
		return -1;

	GTP_ERROR("i2c_write_bytes to device %02X address %04X len %d", client->addr, addr, len);

	while (left > 0) {
		buffer[0] = ((addr + offset) >> 8) & 0xFF;
		buffer[1] = (addr + offset) & 0xFF;

		if (left > MAX_I2C_TRANSFER_SIZE) {
			memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], MAX_I2C_TRANSFER_SIZE);
			msg.len = MAX_TRANSACTION_LENGTH;
			left -= MAX_I2C_TRANSFER_SIZE;
			offset += MAX_I2C_TRANSFER_SIZE;
		} else {
			memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], left);
			msg.len = left + GTP_ADDR_LENGTH;
			left = 0;
		}

		/* GTP_DEBUG("byte left %d offset %d", left, offset); */

		if (i2c_transfer(client->adapter, &msg, 1) != 1) {
			GTP_ERROR("I2C write 0x%X%X length=%d failed", buffer[0], buffer[1], len);
			return -1;
		}
	}

	return 0;
}

s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = (buf[0] << 8) + buf[1];

	ret = i2c_write_bytes(client, addr, &buf[2], len - 2);

	if (!ret)
		return 1;

	gtp_reset_guitar(client, 20);
	return ret;
}



/*******************************************************
Function:
	Send config Function.

Input:
	client:	i2c client.

Output:
	Executive outcomes.0--success,non-0--fail.
*******************************************************/
s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret = 0;
#if GTP_DRIVER_SEND_CFG
	s32 retry = 0;

	for (retry = 0; retry < 5; retry++) {
#ifdef GTP_CHARGER_DETECT

		if (gtp_charger_mode == 1) {
			GTP_DEBUG("Write charger config");
			ret =
			    gtp_i2c_write(client, config_charger,
					  GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		} else {
			GTP_DEBUG("Write normal config");
			ret =
			    gtp_i2c_write(client, config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		}

#else
		ret = gtp_i2c_write(client, config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
#endif


		if (ret > 0)
			break;
	}

#endif

	return ret;
}

/*******************************************************
Function:
	Read goodix touchscreen version function.

Input:
	client:	i2c client struct.
	version:address to store version info

Output:
	Executive outcomes.0---succeed.
*******************************************************/
s32 gtp_read_version(struct i2c_client *client, u16 *version)
{
	s32 ret = -1;
	s32 i;
	u8 buf[8] = { GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff };

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

	/* for gt9xx series */
	for (i = 0; i < 3; i++) {
		if (buf[i + 2] < 0x30)
			break;

		tpd_info.pid |= ((buf[i + 2] - 0x30) << ((2 - i) * 4));
	}

	GTP_INFO("IC VERSION:%c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[7], buf[6]);

	return ret;
}

/*******************************************************
Function:
	GTP initialize function.

Input:
	client:	i2c client private struct.

Output:
	Executive outcomes.0---succeed.
*******************************************************/
static s32 gtp_init_panel(struct i2c_client *client)
{
	s32 ret = -1;

#if GTP_DRIVER_SEND_CFG
	s32 i;
	u8 check_sum = 0;
	u8 rd_cfg_buf[16];

	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 *send_cfg_buf[3] = { cfg_info_group1, cfg_info_group2, cfg_info_group3 };
#ifdef GTP_CHARGER_DETECT
	u8 cfg_info_group1_charger[] = CTP_CFG_GROUP1_CHARGER;
	u8 cfg_info_group2_charger[] = CTP_CFG_GROUP2_CHARGER;
	u8 cfg_info_group3_charger[] = CTP_CFG_GROUP3_CHARGER;
	u8 *send_cfg_buf_charger[3] = { cfg_info_group1_charger,
		cfg_info_group2_charger, cfg_info_group3_charger };
#endif
	u8 cfg_info_len[3] = { sizeof(cfg_info_group1) / sizeof(cfg_info_group1[0]),
		sizeof(cfg_info_group2) / sizeof(cfg_info_group2[0]),
		sizeof(cfg_info_group3) / sizeof(cfg_info_group3[0])
	};

	for (i = 0; i < 3; i++) {
		if (cfg_info_len[i] > cfg_len)
			cfg_len = cfg_info_len[i];
	}

	GTP_DEBUG("len1=%d,len2=%d,len3=%d,get_len=%d", cfg_info_len[0], cfg_info_len[1],
		  cfg_info_len[2], cfg_len);

	if ((!cfg_info_len[1]) && (!cfg_info_len[2])) {
		rd_cfg_buf[GTP_ADDR_LENGTH] = 0;
	} else {
		rd_cfg_buf[0] = GTP_REG_SENSOR_ID >> 8;
		rd_cfg_buf[1] = GTP_REG_SENSOR_ID & 0xff;
		ret = gtp_i2c_read(client, rd_cfg_buf, 3);

		if (ret < 0) {
			GTP_ERROR("Read SENSOR ID failed,default use group1 config!");
			rd_cfg_buf[GTP_ADDR_LENGTH] = 0;
			goto out;
		}

		rd_cfg_buf[GTP_ADDR_LENGTH] &= 0x03;
	}

	GTP_INFO("SENSOR ID:%d", rd_cfg_buf[GTP_ADDR_LENGTH]);
	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[rd_cfg_buf[GTP_ADDR_LENGTH]], cfg_len);
#ifdef GTP_CHARGER_DETECT
	memset(&config_charger[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config_charger[GTP_ADDR_LENGTH], send_cfg_buf_charger[rd_cfg_buf[GTP_ADDR_LENGTH]],
	       cfg_len);
#endif

#if GTP_CUSTOM_CFG
	config[RESOLUTION_LOC] = (u8) GTP_MAX_WIDTH;
	config[RESOLUTION_LOC + 1] = (u8) (GTP_MAX_WIDTH >> 8);
	config[RESOLUTION_LOC + 2] = (u8) GTP_MAX_HEIGHT;
	config[RESOLUTION_LOC + 3] = (u8) (GTP_MAX_HEIGHT >> 8);

	if (GTP_INT_TRIGGER == 0) {	/* RISING */
		config[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1) {	/* FALLING */
		config[TRIGGER_LOC] |= 0x01;
	}
#endif				/* endif GTP_CUSTOM_CFG */

	check_sum = 0;

	for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
		check_sum += config[i];

	config[cfg_len] = (~check_sum) + 1;
#ifdef GTP_CHARGER_DETECT
	check_sum = 0;

	for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
		check_sum += config_charger[i];


	config_charger[cfg_len] = (~check_sum) + 1;
#endif
#else				/* else DRIVER NEED NOT SEND CONFIG */

	if (cfg_len == 0)
		cfg_len = GTP_CONFIG_MAX_LENGTH;

	ret = gtp_i2c_read(client, config, cfg_len + GTP_ADDR_LENGTH);

	if (ret < 0) {
		GTP_ERROR("GTP read resolution & max_touch_num failed, use default value!");
		abs_x_max = GTP_MAX_WIDTH;
		abs_y_max = GTP_MAX_HEIGHT;
		int_type = GTP_INT_TRIGGER;
		goto out;
	}
#endif				/* endif GTP_DRIVER_SEND_CFG */

	abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
	abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
	int_type = (config[TRIGGER_LOC]) & 0x03;

	if ((!abs_x_max) || (!abs_y_max)) {
		GTP_ERROR("GTP resolution & max_touch_num invalid, use default value!");
		abs_x_max = GTP_MAX_WIDTH;
		abs_y_max = GTP_MAX_HEIGHT;
	}

	ret = gtp_send_cfg(client);

	if (ret < 0) {
		GTP_ERROR("Send config error.");
		goto out;
	}

	GTP_DEBUG("X_MAX = %d,Y_MAX = %d,TRIGGER = 0x%02x", abs_x_max, abs_y_max, int_type);

	msleep(20);
 out:
	return ret;
}

static s8 gtp_i2c_test(struct i2c_client *client)
{

	u8 retry = 0;
	s8 ret = -1;
	u32 hw_info = 0;

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = i2c_read_bytes(client, GTP_REG_HW_INFO, (u8 *) &hw_info, sizeof(hw_info));

		if ((!ret) && (hw_info == 0x00900600)) {	/* 20121212 */
			return ret;
		}

		GTP_ERROR("GTP_REG_HW_INFO : %08X", hw_info);
		GTP_ERROR("GTP i2c test failed time %d.", retry);
		msleep(20);
	}

	return -1;
}

/*******************************************************
Function:
	Set INT pin  as input for FW sync.

Note:
  If the INT is high, It means there is pull up resistor attached on the INT pin.
  Pull low the INT pin manaully for FW sync.
*******************************************************/
void gtp_int_sync(void)
{
	GTP_DEBUG("There is pull up resisitor attached on the INT pin~!");
	gpio_direction_output(tpd_int_gpio_number, 0);
	msleep(50);
	gpio_direction_input(tpd_int_gpio_number);
}

void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	GTP_INFO("GTP RESET! %d\n", client->addr);
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(ms);
	gpio_direction_output(tpd_int_gpio_number, client->addr == 0x14);
	msleep(20);
	gpio_direction_output(tpd_rst_gpio_number, 1);
	msleep(20);
	gtp_int_sync();

}

static int tpd_power_on(struct i2c_client *client)
{
	int ret = 0;
	int reset_count = 0;
	/* int i=0; */

 reset_proc:
	gpio_direction_output(tpd_int_gpio_number, 0);
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(20);
	/* power on, need confirm with SA */
	GTP_ERROR("turn on power reg-vgp6\n");
	ret = regulator_enable(tpd->reg);
	if (ret != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", ret);

	GTP_ERROR("turn on power reg-vgp4\n");
	ret = regulator_enable(tpd->io_reg);
	if (ret != 0)
		TPD_DMESG("Failed to enable reg-vgp4: %d\n", ret);




	gtp_reset_guitar(client, 20);
	GTP_ERROR("tpd_int_gpio_number:0x%x, tpd_rst_gpio_number:0x%x", tpd_int_gpio_number, tpd_rst_gpio_number);
	ret = gtp_i2c_test(client);

	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");

		if (reset_count < TPD_MAX_RESET_COUNT) {
			reset_count++;
			goto reset_proc;
		} else {
			goto out;
		}
	}
#if GTP_FW_DOWNLOAD
	ret = gup_init_fw_proc(client);

	if (ret < 0)
		GTP_ERROR("Create fw download thread error.");

#endif
 out:
	return ret;
}

#ifdef MTK_CTP_RESET_CONFIG
static int tpd_clear_config(void *unused)
{
	int ret = 0, check_sum = 0;
	u8 temp_data = 0, i = 0;
	u8 config_1st[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
	= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };

	GTP_INFO("Clear Config Begin......");
	msleep(10000);		/* wait main thread to be completed */

	ret = i2c_read_bytes(i2c_client_point, GTP_REG_CONFIG_DATA, &temp_data, 1);
	if (ret < 0) {
		GTP_ERROR("GTP read config failed!");
		return -1;
	}

	GTP_INFO("IC config version: 0x%x; Driver config version: 0x%x", temp_data,
		 config[GTP_ADDR_LENGTH]);
	if ((temp_data < (u8) 0x5A) && (temp_data > config[GTP_ADDR_LENGTH])) {
		memset(&config_1st[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
		memcpy(&config_1st[GTP_ADDR_LENGTH], &config[GTP_ADDR_LENGTH], cfg_len);
		config_1st[GTP_ADDR_LENGTH] = 0;
		check_sum = 0;

		for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
			check_sum += config_1st[i];


		config_1st[cfg_len] = (~check_sum) + 1;
		ret =
		    gtp_i2c_write(i2c_client_point, config_1st,
				  GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret < 0)
			GTP_ERROR("GTP write 00 config failed!");
		else
			GTP_INFO("Force clear cfg done");

	} else {
		GTP_INFO("No need clear cfg");
	}
	return 0;
}
#endif

static const struct file_operations gt_upgrade_proc_fops = {
	.write = gt91xx_config_write_proc,
	.read = gt91xx_config_read_proc
};



static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	TPD_DEBUG("Device Tree Tpd_irq_registration!");

	node = of_find_compatible_node(NULL, NULL, "mediatek,cap_touch");

	if (node) {
		/*touch_irq = gpio_to_irq(tpd_int_gpio_number);*/
		touch_irq = irq_of_parse_and_map(node, 0);
		TPD_DEBUG("touch_irq number %d\n", touch_irq);
		if (!int_type) {/* EINTF_TRIGGER */
			ret =
			    request_irq(touch_irq, tpd_interrupt_handler, IRQF_TRIGGER_RISING,
					TPD_DEVICE, NULL);
			gtp_eint_trigger_type = IRQF_TRIGGER_RISING;
			if (ret > 0)
				GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
		} else {
			ret =
			    request_irq(touch_irq, tpd_interrupt_handler,
					IRQF_TRIGGER_FALLING, TPD_DEVICE, NULL);
			gtp_eint_trigger_type = IRQF_TRIGGER_FALLING;
			if (ret > 0)
				GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
		}

	} else {
		TPD_DMESG("tpd request_irq can not find touch eint device node!.");
	}

	return ret;
}

static s32 tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	s32 err = 0;
	s32 ret = 0;
	/*int i;*/
	u16 version_info;
	struct task_struct *thread = NULL;
#if 0				/* GTP_HAVE_TOUCH_KEY */
	s32 idx = 0;
#endif
#ifdef TPD_PROXIMITY
	struct hwmsen_object obj_ps;
#endif

	of_get_gt9xx_platform_data(&client->dev);
	/* configure the gpio pins */
	ret = gpio_request_one(tpd_rst_gpio_number, GPIOF_OUT_INIT_LOW,
				 "touchp_reset");
	if (ret < 0) {
		GTP_ERROR("Unable to request gpio reset_pin\n");
		return -1;
	}
	ret = gpio_request_one(tpd_int_gpio_number, GPIOF_IN,
				 "tpd_int");
	if (ret < 0) {
		GTP_ERROR("Unable to request gpio int_pin\n");
		gpio_free(tpd_rst_gpio_number);
		return -1;
	}
	i2c_client_point = client;



	ret = tpd_power_on(client);

	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		goto out;
	}
#ifdef MTK_CTP_RESET_CONFIG
	thread = kthread_run(tpd_clear_config, 0, "mtk-tpd-clear-config");
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_INFO(TPD_DEVICE " failed to create kernel thread for clearing config: %d", err);
	}
	thread = NULL;
#endif

#if GTP_AUTO_UPDATE
	ret = gup_init_update_proc(client);

	if (ret < 0) {
		GTP_ERROR("Create update thread error.");
		goto out;
	}
#endif



#ifdef VELOCITY_CUSTOM
	tpd_v_magnify_x = TPD_VELOCITY_CUSTOM_X;
	tpd_v_magnify_y = TPD_VELOCITY_CUSTOM_Y;

#endif

	ret = gtp_read_version(client, &version_info);

	if (ret < 0) {
		GTP_ERROR("Read version failed.");
		goto out;
	}

	ret = gtp_init_panel(client);

	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		goto out;
	}
	GTP_DEBUG("gtp_init_panel success");
	/* Create proc file system */
	gt91xx_config_proc =
	    proc_create(GT91XX_CONFIG_PROC_FILE, 0660, NULL, &gt_upgrade_proc_fops);

	if (gt91xx_config_proc == NULL) {
		GTP_ERROR("create_proc_entry %s failed", GT91XX_CONFIG_PROC_FILE);
		goto out;
	}
#if GTP_CREATE_WR_NODE
	init_wr_node(client);
#endif

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_ERROR(TPD_DEVICE " failed to create kernel thread: %d", err);
		goto out;
	}
#if 0				/* GTP_HAVE_TOUCH_KEY */

	for (idx = 0; idx < TPD_KEY_COUNT; idx++)
		input_set_capability(tpd->dev, EV_KEY, touch_key_array[idx]);

#endif


	tpd_irq_registration();
	/*enable_irq(touch_irq);*/


#ifdef TPD_PROXIMITY
	/* obj_ps.self = cm3623_obj; */
	obj_ps.polling = 0;	/* 0--interrupt mode;1--polling mode; */
	obj_ps.sensor_operate = tpd_ps_operate;

	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err)
		GTP_ERROR("hwmsen attach fail, return:%d.", err);

#endif

#if GTP_ESD_PROTECT
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif

#ifdef GTP_CHARGER_DETECT
	INIT_DELAYED_WORK(&gtp_charger_check_work, gtp_charger_check_func);
	gtp_charger_check_workqueue = create_workqueue("gtp_charger_check");
	queue_delayed_work(gtp_charger_check_workqueue, &gtp_charger_check_work,
			   TPD_CHARGER_CHECK_CIRCLE);
#endif
	tpd_load_status = 1;

	GTP_INFO("%s, success run Done", __func__);
	return 0;
 out:
	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);
	return -1;
}


static irqreturn_t tpd_interrupt_handler(int irq, void *dev_id)
{
	TPD_DEBUG_PRINT_INT;

	tpd_flag = 1;

	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}


static int tpd_i2c_remove(struct i2c_client *client)
{
#if GTP_CREATE_WR_NODE
	uninit_wr_node();
#endif

#if GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

#if GTP_ESD_PROTECT
	destroy_workqueue(gtp_charger_check_workqueue);
#endif
	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);
	return 0;
}

#ifdef GTP_CHARGER_DETECT
static void gtp_charger_check_func(struct work_struct *work)
{
	int cur_charger_state;

	cur_charger_state = upmu_get_pchr_chrdet();

	GTP_DEBUG("Charger mode = %d", cur_charger_state);

	if (gtp_charger_mode != cur_charger_state) {
		GTP_DEBUG("Charger state change detected~!");
		GTP_DEBUG("Charger mode = %d", cur_charger_state);
		gtp_charger_mode = cur_charger_state;
		gtp_send_cfg(i2c_client_point);
	}

	if (!tpd_halt)
		queue_delayed_work(gtp_charger_check_workqueue, &gtp_charger_check_work,
				   TPD_CHARGER_CHECK_CIRCLE);

}
#endif

#if GTP_ESD_PROTECT
static void force_reset_guitar(void)
{
	s32 i;
	s32 ret;

	GTP_INFO("force_reset_guitar");

	/* Power off TP */
	ret = regulator_disable(tpd->reg);
	if (ret != 0)
		TPD_DMESG("Failed to disable reg-vgp6: %d\n", ret);

	ret = regulator_disable(tpd->io_reg);
	if (ret != 0)
		TPD_DMESG("Failed to disable reg-vgp4: %d\n", ret);

	msleep(30);
	/* Power on TP */
	ret = regulator_enable(tpd->reg);
	if (ret != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", ret);

	ret = regulator_enable(tpd->io_reg);
	if (ret != 0)
		TPD_DMESG("Failed to enable reg-vgp4: %d\n", ret);

	msleep(30);
	for (i = 0; i < 5; i++) {
		/* Reset Guitar */
		gtp_reset_guitar(i2c_client_point, 20);

		/* Send config */
		ret = gtp_send_cfg(i2c_client_point);

		if (ret < 0)
			continue;

		break;
	}

}

static void gtp_esd_check_func(struct work_struct *work)
{
	int i;
	int ret = -1;
	u8 test[3] = { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };

	if (tpd_halt)
		return;

	for (i = 0; i < 3; i++) {
		ret = gtp_i2c_read(i2c_client_point, test, 3);

		if (ret > 0)
			break;
	}

	if (i >= 3)
		force_reset_guitar();

	if (!tpd_halt) {
		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work,
				   TPD_ESD_CHECK_CIRCLE);
	}

}
#endif
static int tpd_history_x = 0, tpd_history_y;
static void tpd_down(s32 x, s32 y, s32 size, s32 id)
{
	if ((!size) && (!id)) {
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, 100);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
	} else {
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, size);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
		/* track id Start 0 */
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

	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 1, x + y); */
#ifdef TPD_HAVE_BUTTON

	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
		tpd_button(x, y, 1);
#endif
}

static void tpd_up(s32 x, s32 y, s32 id)
{
	/* input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0); */
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	/* input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0); */
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, id, 0);
	tpd_history_x = 0;
	tpd_history_y = 0;
	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 0, x + y); */

#ifdef TPD_HAVE_BUTTON

	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
		tpd_button(x, y, 0);
#endif
}

/*Coordination mapping*/
static void tpd_calibrate_driver(int *x, int *y)
{
	int tx;

	GTP_DEBUG("Call tpd_calibrate of this driver ..\n");

	tx = ((tpd_def_calmat[0] * (*x)) + (tpd_def_calmat[1] * (*y)) + (tpd_def_calmat[2])) >> 12;
	*y = ((tpd_def_calmat[3] * (*x)) + (tpd_def_calmat[4] * (*y)) + (tpd_def_calmat[5])) >> 12;
	*x = tx;
}

static int touch_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = 4 };
	u8 end_cmd[3] = { GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0 };
	u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = { GTP_READ_COOR_ADDR >> 8,
		GTP_READ_COOR_ADDR & 0xFF };
	u8 touch_num = 0;
	u8 finger = 0;
	static u8 pre_touch;
	static u8 pre_key;
	u8 key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i = 0;
	s32 ret = -1;
#ifdef TPD_PROXIMITY
	s32 err = 0;
	hwm_sensor_data sensor_data;
	u8 proximity_status;
#endif
#if GTP_CHANGE_X2Y
	s32 temp;
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
		disable_irq(touch_irq);

		if (tpd_halt) {
			mutex_unlock(&i2c_access);
			GTP_DEBUG("return for interrupt after suspend...  ");
			continue;
		}

		ret = gtp_i2c_read(i2c_client_point, point_data, 12);

		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d ", ret);
			goto exit_work_func;
		}

		finger = point_data[GTP_ADDR_LENGTH];

		if ((finger & 0x80) == 0) {

			enable_irq(touch_irq);

			mutex_unlock(&i2c_access);
			GTP_ERROR("buffer not ready");
			continue;
		}
#ifdef TPD_PROXIMITY

		if (tpd_proximity_flag == 1) {
			proximity_status = point_data[GTP_ADDR_LENGTH];
			GTP_DEBUG("REG INDEX[0x814E]:0x%02X", proximity_status);

			if (proximity_status & 0x60) {	/* proximity or large touch detect,enable hwm_sensor. */
				tpd_proximity_detect = 0;
				/* sensor_data.values[0] = 0; */
			} else {
				tpd_proximity_detect = 1;
				/* sensor_data.values[0] = 1; */
			}

			/* get raw data */
			GTP_DEBUG(" ps change");
			GTP_DEBUG("PROXIMITY STATUS:0x%02X", tpd_proximity_detect);
			/* map and store data to hwm_sensor_data */
			sensor_data.values[0] = tpd_get_ps_value();
			sensor_data.value_divide = 1;
			sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
			/* report to the up-layer */
			ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);

			if (ret)
				GTP_ERROR("Call hwmsen_get_interrupt_data fail = %d", err);
		}
#endif

		touch_num = finger & 0x0f;

		if (touch_num > GTP_MAX_TOUCH) {
			GTP_ERROR("Bad number of fingers!");
			goto exit_work_func;
		}

		if (touch_num > 1) {
			u8 buf[8 * GTP_MAX_TOUCH] = { (GTP_READ_COOR_ADDR + 10) >> 8,
				(GTP_READ_COOR_ADDR + 10) & 0xff };

			ret = gtp_i2c_read(i2c_client_point, buf, 2 + 8 * (touch_num - 1));
			memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
		}
#if GTP_HAVE_TOUCH_KEY
		key_value = point_data[3 + 8 * touch_num];

		if (key_value || pre_key) {
			for (i = 0; i < TPD_KEY_COUNT; i++) {
				/* input_report_key(tpd->dev, touch_key_array[i], key_value & (0x01 << i)); */
				if (key_value & (0x01 << i)) {	/* key=1 menu ;key=2 home; key =4 back; */
					input_x = touch_key_point_maping_array[i].point_x;
					input_y = touch_key_point_maping_array[i].point_y;
					GTP_DEBUG("button =%d %d", input_x, input_y);

					tpd_down(input_x, input_y, 0, 0);
				}
			}

			if ((pre_key != 0) && (key_value == 0))
				tpd_up(0, 0, 0);

			touch_num = 0;
			pre_touch = 0;
		}
#endif
		pre_key = key_value;

		GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);

		if (touch_num) {
			for (i = 0; i < touch_num; i++) {
				coor_data = &point_data[i * 8 + 3];

				id = coor_data[0] & 0x0F;
				input_x = coor_data[1] | coor_data[2] << 8;
				input_y = coor_data[3] | coor_data[4] << 8;
				input_w = coor_data[5] | coor_data[6] << 8;

				GTP_DEBUG("Original touch point : [X:%04d, Y:%04d]", input_x,
					  input_y);

				input_x = TPD_WARP_X(abs_x_max, input_x);
				input_y = TPD_WARP_Y(abs_y_max, input_y);
				tpd_calibrate_driver(&input_x, &input_y);

				GTP_DEBUG("Touch point after calibration: [X:%04d, Y:%04d]",
					  input_x, input_y);

#if GTP_CHANGE_X2Y
				temp = input_x;
				input_x = input_y;
				input_y = temp;
#endif

				tpd_down(input_x, input_y, input_w, id);
			}
		} else if (pre_touch) {
			GTP_DEBUG("Touch Release!");
			tpd_up(0, 0, 0);
		} else {
			GTP_DEBUG("Additional Eint!");
		}
		pre_touch = touch_num;
		/* input_report_key(tpd->dev, BTN_TOUCH, (touch_num || key_value)); */

		if (tpd != NULL && tpd->dev != NULL)
			input_sync(tpd->dev);

 exit_work_func:

		if (!gtp_rawdiff_mode) {
			ret = gtp_i2c_write(i2c_client_point, end_cmd, 3);

			if (ret < 0)
				GTP_INFO("I2C write end_cmd  error!");
		}

		enable_irq(touch_irq);

		mutex_unlock(&i2c_access);

	} while (!kthread_should_stop());

	return 0;
}

static int tpd_local_init(void)
{
	int retval;

	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	tpd->io_reg = regulator_get(tpd->tpd_dev, "vtouchio");
	retval = regulator_set_voltage(tpd->reg, 3300000, 3300000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp6 voltage: %d\n", retval);
		return -1;
	}
	retval = regulator_set_voltage(tpd->io_reg, 1800000, 1800000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp4 voltage: %d\n", retval);
		return -1;
	}

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		GTP_INFO("unable to add i2c driver.");
		return -1;
	}

	if (tpd_load_status == 0) {
		/* if(tpd_load_status == 0) disable auto load touch driver for linux3.0 porting */
		GTP_INFO("add error touch panel driver.");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (GTP_MAX_TOUCH - 1), 0, 0);
#ifdef TPD_HAVE_BUTTON
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);	/* initialize tpd button data */
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	/* memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4); */
	/* memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4); */
	if (FACTORY_BOOT == get_boot_mode()) {
		TPD_DEBUG("Factory mode is detected!\n");
		memcpy(tpd_calmat, tpd_def_calmat_local_factory, 8 * 4);
		memcpy(tpd_def_calmat, tpd_def_calmat_local_factory, 8 * 4);
	} else {
		TPD_DEBUG("Normal mode is detected!\n");
		memcpy(tpd_calmat, tpd_def_calmat_local_normal, 8 * 4);
		memcpy(tpd_def_calmat, tpd_def_calmat_local_normal, 8 * 4);
	}
#endif

	/* set vendor string */
	tpd->dev->id.vendor = 0x00;
	tpd->dev->id.product = tpd_info.pid;
	tpd->dev->id.version = tpd_info.vid;

	GTP_INFO("end %s, %d", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}


/*******************************************************
Function:
	Eter sleep function.

Input:
	client:i2c_client.

Output:
	Executive outcomes.0--success,non-0--fail.
*******************************************************/
static s8 gtp_enter_sleep(struct i2c_client *client)
{
	s8 ret = -1;
#if !GTP_POWER_CTRL_SLEEP
	s8 retry = 0;
	u8 i2c_control_buf[3] = { (u8) (GTP_REG_SLEEP >> 8), (u8) GTP_REG_SLEEP, 5 };

	gpio_direction_output(tpd_int_gpio_number, 0);
	msleep(20);

	while (retry++ < 5) {
		ret = gtp_i2c_write(client, i2c_control_buf, 3);

		if (ret > 0) {
			GTP_INFO("GTP enter sleep!");
			return ret;
		}

		msleep(20);
	}

#else

	gpio_direction_output(tpd_int_gpio_number, 0);
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(20);


	ret = regulator_disable(tpd->io_reg);
	if (ret != 0)
		TPD_DMESG("Failed to disable reg-vgp4: %d\n", ret);

	ret = regulator_disable(tpd->reg);
	if (ret != 0)
		TPD_DMESG("Failed to disable reg-vgp6: %d\n", ret);

	GTP_INFO("GTP enter sleep!");
	return 0;

#endif
	GTP_ERROR("GTP send sleep cmd failed.");
	return ret;
}

/*******************************************************
Function:
	Wakeup from sleep mode Function.

Input:
	client:i2c_client.

Output:
	Executive outcomes.0--success,non-0--fail.
*******************************************************/
static s8 gtp_wakeup_sleep(struct i2c_client *client)
{
	u8 retry = 0;
	s8 ret = -1;


	GTP_INFO("GTP wakeup begin.");
#if GTP_POWER_CTRL_SLEEP

	while (retry++ < 5) {
		ret = tpd_power_on(client);

		if (ret < 0)
			GTP_ERROR("I2C Power on ERROR!");

		ret = gtp_send_cfg(client);

		if (ret > 0) {
			GTP_DEBUG("Wakeup sleep send config success.");
			return ret;
		}
	}

#else

	while (retry++ < 10) {
		gpio_direction_output(tpd_int_gpio_number, 1);
		msleep(20);
		gpio_direction_output(tpd_int_gpio_number, 0);
		msleep(20);
		ret = gtp_i2c_test(client);

		if (ret >= 0) {
			gtp_int_sync();
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

	mutex_lock(&i2c_access);

	disable_irq(touch_irq);

	tpd_halt = 1;
	mutex_unlock(&i2c_access);

	ret = gtp_enter_sleep(i2c_client_point);
	if (ret < 0)
		GTP_ERROR("GTP early suspend failed.");
#if GTP_ESD_PROTECT
	cancel_delayed_work_sync(&gtp_esd_check_work);
#endif

#ifdef GTP_CHARGER_DETECT
	cancel_delayed_work_sync(&gtp_charger_check_work);
#endif
#ifdef TPD_PROXIMITY

	if (tpd_proximity_flag == 1)
		return;
#endif
}

/* Function to manage power-on resume */
static void tpd_resume(struct device *h)
{
	s32 ret = -1;

	ret = gtp_wakeup_sleep(i2c_client_point);

	if (ret < 0)
		GTP_ERROR("GTP later resume failed.");


	GTP_INFO("GTP wakeup sleep.");

	mutex_lock(&i2c_access);
	tpd_halt = 0;

	enable_irq(touch_irq);

	mutex_unlock(&i2c_access);

#ifdef TPD_PROXIMITY
	if (tpd_proximity_flag == 1)
		return;
#endif

#if GTP_ESD_PROTECT
	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif

#ifdef GTP_CHARGER_DETECT
	queue_delayed_work(gtp_charger_check_workqueue, &gtp_charger_check_work,
			   TPD_CHARGER_CHECK_CIRCLE);
#endif

}

static void tpd_off(void)
{
	int ret;

	ret = regulator_disable(tpd->reg);
	if (ret != 0)
		TPD_DMESG("Failed to disable reg-vgp6: %d\n", ret);

	ret = regulator_disable(tpd->io_reg);
	if (ret != 0)
		TPD_DMESG("Failed to disable reg-vgp4: %d\n", ret);

	GTP_INFO("GTP enter sleep!");

	tpd_halt = 1;
	disable_irq(touch_irq);

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

	enable_irq(touch_irq);

	tpd_halt = 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gt9xx",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
	.attrs = {
		  .attr = gt9xx_attrs,
		  .num = ARRAY_SIZE(gt9xx_attrs),
		  },
};

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
	GTP_INFO("MediaTek gt91xx touch panel driver init");

	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		GTP_INFO("add generic driver failed");

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	GTP_INFO("MediaTek gt91xx touch panel driver exit");
	/* input_unregister_device(tpd->dev); */
	tpd_driver_remove(&tpd_device_driver);
}
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
