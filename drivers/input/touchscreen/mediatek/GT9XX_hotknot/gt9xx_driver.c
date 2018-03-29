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
 * Version: 2.0
 * Revision Record:
 *      V1.0:  first release. by Andrew, 2012/08/27.
 *      V1.2:  modify gt9110p pid map, by Andrew, 2012/10/15
 *      V1.4:
 *          1. modify gup_enter_update_mode,
 *          2. rewrite i2c read/write func
 *          3. check update file checksum
 *                  by Andrew, 2012/12/12
 *      v1.6:
 *          1. delete GTP_FW_DOWNLOAD related things.
 *          2. add GTP_HEADER_FW_UPDATE switch to update fw by gtp_default_fw in *.h directly
 *                  by Meta, 2013/04/18
 *      V2.0:
 *          1. GT9XXF main clock calibration
 *          2. header fw update no fs related
 *          3. update file searching optimization
 *          4. config update as module, switchable
 *                  by Meta, 2013/08/28
 */

#include "tpd.h"

#include "gt9xx_config.h"
#include "include/tpd_gt9xx_common.h"
#if !((defined(CONFIG_GTP_AUTO_UPDATE) && defined(CONFIG_GTP_HEADER_FW_UPDATE)) && !defined(CONFIG_GTP_COMPATIBLE_MODE))
#include "gt9xx_firmware.h"
#endif
#define GUP_FW_INFO
#if defined(CONFIG_TPD_PROXIMITY)
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#include "mt_boot_common.h"
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#if defined(CONFIG_GTP_SUPPORT_I2C_DMA)
#include <linux/dma-mapping.h>
#endif
#include <linux/proc_fs.h>	/*proc */

#if TOUCH_FILTER
static struct tpd_filter_t tpd_filter_local = TPD_FILTER_PARA;
#endif
static int tpd_flag;
int tpd_halt = 0;
static int tpd_eint_mode = 1;
static struct task_struct *thread;
static struct task_struct *probe_thread;
static bool check_flag;
static int tpd_polling_time = 50;
static int tpd_tui_flag;
static int tpd_tui_low_power_skipped;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
DEFINE_MUTEX(i2c_access);
DEFINE_MUTEX(tp_wr_access);
DEFINE_MUTEX(tui_lock);


const u16 touch_key_array[] = TPD_KEYS;
#define GTP_MAX_KEY_NUM (sizeof(touch_key_array)/sizeof(touch_key_array[0]))
struct touch_virtual_key_map_t {
	int x;
	int y;
};
static struct touch_virtual_key_map_t maping[] = GTP_KEY_MAP_ARRAY;

unsigned int touch_irq = 0;

#if defined(CONFIG_GTP_SLIDE_WAKEUP)
enum DOZE_T {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
};
static enum DOZE_T doze_status = DOZE_DISABLED;
static s8 gtp_enter_doze(struct i2c_client *client);
#endif

#if defined(CONFIG_GTP_CHARGER_SWITCH)
static void gtp_charger_switch(s32 dir_update);
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif
static s8 gtp_wakeup_sleep(struct i2c_client *client);
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
/*static int tpd_calmat_local[8]		 = TPD_CALIBRATION_MATRIX;*/
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#if defined(CONFIG_GTP_SUPPORT_I2C_DMA)
#ifdef CONFIG_MTK_I2C_EXTENSION
static u8 *gpDMABuf_va;
static dma_addr_t gpDMABuf_pa;
#else
static char I2CDMABuf[4096];
#endif
#endif

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
static void tpd_on(void);
static void tpd_off(void);

#if defined(CONFIG_GTP_CHARGER_DETECT)
#define TPD_CHARGER_CHECK_CIRCLE		50
static struct delayed_work gtp_charger_check_work;
static struct workqueue_struct *gtp_charger_check_workqueue;
static void gtp_charger_check_func(struct work_struct *);
static u8 gtp_charger_mode;
#endif

#if defined(CONFIG_GTP_ESD_PROTECT)
static int clk_tick_cnt = 200;
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static s32 gtp_init_ext_watchdog(struct i2c_client *client);
static void gtp_esd_check_func(struct work_struct *);
u8 esd_running = 0;
spinlock_t esd_lock;
#endif

#if defined(CONFIG_HOTKNOT_BLOCK_RW)
u8 hotknot_paired_flag = 0;
#endif

#if defined(CONFIG_TPD_PROXIMITY)
#define TPD_PROXIMITY_VALID_REG                   0x814E
#define TPD_PROXIMITY_ENABLE_REG                  0x8042
static u8 tpd_proximity_flag;
static u8 tpd_proximity_detect = 1;	/* 0-->close ; 1--> far away */
#endif

#ifndef GTP_REG_REFRESH_RATE
#define GTP_REG_REFRESH_RATE		0x8056
#endif

struct i2c_client *i2c_client_tp_point = NULL;
static const struct i2c_device_id tpd_i2c_id[] = { {"gt9xx", 0}, {} };
static unsigned short force[] = { 0, 0xBA, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
/* static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("gt9xx", (0xBA >> 1))}; */

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
	.address_list = (const unsigned short *) forces,
};

static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };

#ifdef CONFIG_GTP_CHARGER_SWITCH
static u8 charger_config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
		= {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
static bool is_charger_cfg_updating;
#endif

#if defined(CONFIG_GTP_CHARGER_DETECT)
static u8 config_charger[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };
#endif
#pragma pack(1)
struct st_tpd_info {
	u16 pid;		/* product id	 // */
	u16 vid;		/* version id	 // */
};
#pragma pack()

struct st_tpd_info tpd_info;
u8 int_type = 0;
u32 abs_x_max = 0;
u32 abs_y_max = 0;
u8 gtp_rawdiff_mode = 0;
u8 cfg_len = 0;
#ifdef CONFIG_GTP_CHARGER_SWITCH
u8 charger_cfg_len = 0;
u8 charger_grp_cfg_version = 0;
#endif
u8 grp_cfg_version = 0;
u8 fixed_config = 0;
u8 pnl_init_error = 0;
static u8 chip_gt9xxs;	/* true if chip type is gt9xxs,like gt915s */

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
u8 driver_num = 0;
u8 sensor_num = 0;
u8 gtp_ref_retries = 0;
u8 gtp_clk_retries = 0;
enum CHIP_TYPE_T gtp_chip_type = CHIP_TYPE_GT9;
u8 gtp_clk_buf[6];
u8 rqst_processing = 0;

static void gtp_get_chip_type(struct i2c_client *client);
static u8 gtp_bak_ref_proc(struct i2c_client *client, u8 mode);
static u8 gtp_main_clk_proc(struct i2c_client *client);
static void gtp_recovery_reset(struct i2c_client *client);
#endif

#if (defined(CONFIG_GTP_ESD_PROTECT) || defined(CONFIG_GTP_COMPATIBLE_MODE))
u8 is_resetting = 0;
#endif

/* proc file system */
static struct proc_dir_entry *gt91xx_config_proc;

#ifdef TPD_REFRESH_RATE
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
	return gtp_i2c_write(i2c_client_tp_point, buf, sizeof(buf));
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

	ret = gtp_i2c_read(i2c_client_tp_point, buf, sizeof(buf));
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
	unsigned long rate = 0;

	if (kstrtoul(buf, 16, &rate))
		return 0;
	gtp_set_refresh_rate(rate);
	return size;
}

static DEVICE_ATTR(tpd_refresh_rate, 0664, show_refresh_rate, store_refresh_rate);

static struct device_attribute *gt9xx_attrs[] = {
	&dev_attr_tpd_refresh_rate,
};
#endif
/* ============================================================= */

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, "mtk-tpd");
	return 0;
}

#if defined(CONFIG_TPD_PROXIMITY)
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

	ret = i2c_write_bytes(i2c_client_tp_point, TPD_PROXIMITY_ENABLE_REG, &state, 1);

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
		GTP_ERROR("proxmy sensor operate function no this parameter %d!\n", command);
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
	i2c_read_bytes(i2c_client_tp_point, GTP_REG_CONFIG_DATA, temp_data, GTP_CONFIG_MAX_LENGTH);

	for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
		ptr += sprintf(ptr, "0x%02X ", temp_data[i]);

		if (i % 8 == 7)
			ptr += sprintf(ptr, "\n");
	}
	/* Touch PID & VID */
	ptr += sprintf(ptr, "\n");
	ptr += sprintf(ptr, "==== GT9XX Version ID ====\n");
	i2c_read_bytes(i2c_client_tp_point, GTP_REG_VERSION, temp_data, 6);
	ptr +=
			sprintf(ptr, "Chip PID: %c%c%c  VID: 0x%02X%02X\n", temp_data[0], temp_data[1],
				temp_data[2], temp_data[5], temp_data[4]);
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		ptr +=
				sprintf(ptr, "Driver VID: 0x%02X%02X\n", gtp_default_FW_fl[12],
					gtp_default_FW_fl[13]);
	} else {
		ptr +=
				sprintf(ptr, "Driver VID: 0x%02X%02X\n", gtp_default_FW[12],
					gtp_default_FW[13]);
	}
#else
	ptr += sprintf(ptr, "Driver VID: 0x%02X%02X\n", gtp_default_FW[12], gtp_default_FW[13]);
#endif
	i2c_read_bytes(i2c_client_tp_point, 0x41E4, temp_data, 1);
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
	int t_ret;

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
	t_ret = sscanf(temp, "%s %d", (char *)&mode_str, &mode);

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
		ret = i2c_write_bytes(i2c_client_tp_point, GTP_REG_SLEEP, buf, 1);
		return count;
	}

	if (copy_from_user(&config[2], buffer, count)) {
		GTP_ERROR("copy from user fail\n");
		return -EFAULT;
	}

		/***********clk operate reseved****************/
		/**********************************************/
	ret = gtp_send_cfg(i2c_client_tp_point);
	abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
	abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
	int_type = (config[TRIGGER_LOC]) & 0x03;

	if (ret < 0)
		GTP_ERROR("send config failed.");

	return count;
}

#if defined(CONFIG_GTP_SUPPORT_I2C_DMA)
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
		 .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .flags = I2C_M_RD,
		 .buf = gpDMABuf_pa,
#else
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .buf = I2CDMABuf,
#endif
		 .len = len,
#ifdef CONFIG_MTK_I2C_EXTENSION
		 .timing = I2C_MASTER_CLOCK
#endif
		 },
	};

	mutex_lock(&tp_wr_access);
	buffer[0] = (addr >> 8) & 0xFF;
	buffer[1] = addr & 0xFF;

	if (rxbuf == NULL) {
		mutex_unlock(&tp_wr_access);
		return -1;
	}
	/* GTP_DEBUG("dma i2c read: 0x%04X, %d bytes(s)", addr, len); */
	for (retry = 0; retry < 5; ++retry) {
		ret = i2c_transfer(client->adapter, &msg[0], 2);
		if (ret < 0)
			continue;
#ifdef CONFIG_MTK_I2C_EXTENSION
		memcpy(rxbuf, gpDMABuf_va, len);
#else
		memcpy(rxbuf, I2CDMABuf, len);
#endif
		mutex_unlock(&tp_wr_access);
		return 0;
	}
	GTP_ERROR("Dma I2C Read Error: 0x%04X, %d byte(s), err-code: %d", addr, len, ret);
	mutex_unlock(&tp_wr_access);
	return ret;
}


s32 i2c_dma_write(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{
	int ret;
	s32 retry = 0;
#ifdef CONFIG_MTK_I2C_EXTENSION
	u8 *wr_buf = gpDMABuf_va;
#else
	u8 *wr_buf = I2CDMABuf;
#endif

	struct i2c_msg msg = {
#ifdef CONFIG_MTK_I2C_EXTENSION
		.addr = (client->addr & I2C_MASK_FLAG),
		.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		.flags = 0,
		.buf = gpDMABuf_pa,
#else
		.addr = client->addr,
		.flags = 0,
		.buf = I2CDMABuf,
#endif
		.len = 2 + len,
#ifdef CONFIG_MTK_I2C_EXTENSION
		.timing = I2C_MASTER_CLOCK
#endif
	};

	mutex_lock(&tp_wr_access);
	wr_buf[0] = (u8) ((addr >> 8) & 0xFF);
	wr_buf[1] = (u8) (addr & 0xFF);

		if (txbuf == NULL) {
			mutex_unlock(&tp_wr_access);
		return -1;
			}
	/* GTP_DEBUG("dma i2c write: 0x%04X, %d bytes(s)", addr, len); */
	memcpy(wr_buf + 2, txbuf, len);
	for (retry = 0; retry < 5; ++retry) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret < 0)
			continue;
		mutex_unlock(&tp_wr_access);
		return 0;
	}
	GTP_ERROR("Dma I2C Write Error: 0x%04X, %d byte(s), err-code: %d", addr, len, ret);
	mutex_unlock(&tp_wr_access);
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


int i2c_read_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
	u8 buffer[GTP_ADDR_LENGTH];
	u8 retry;
	u16 left = len;
	u16 offset = 0;

	struct i2c_msg msg[2] = {
		{
#ifdef CONFIG_MTK_I2C_EXTENSION
		 .addr = ((client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
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
		 .addr = ((client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
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

	GTP_DEBUG("i2c_read_bytes to device %02X address %04X len %d", client->addr, addr, len);

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
				GTP_ERROR("I2C read 0x%X length=%d failed\n", addr + offset, len);
				return -1;
			}
		}
	}

	return 0;
}


int i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
#if defined(CONFIG_GTP_SUPPORT_I2C_DMA)
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
#if defined(CONFIG_GTP_SLIDE_WAKEUP)
	if (DOZE_ENABLED == doze_status)
		return ret;
#endif
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		gtp_recovery_reset(client);
	} else
#endif
	{
		gtp_reset_guitar(client, 20);
	}
	return ret;
}


s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
	u8 buf[16] = { 0 };
	u8 confirm_buf[16] = { 0 };
	u8 retry = 0;

	while (retry++ < 3) {
		memset(buf, 0xAA, 16);
		buf[0] = (u8) (addr >> 8);
		buf[1] = (u8) (addr & 0xFF);
		gtp_i2c_read(client, buf, len + 2);

		memset(confirm_buf, 0xAB, 16);
		confirm_buf[0] = (u8) (addr >> 8);
		confirm_buf[1] = (u8) (addr & 0xFF);
		gtp_i2c_read(client, confirm_buf, len + 2);

		if (!memcmp(buf, confirm_buf, len + 2)) {
			memcpy(rxbuf, confirm_buf + 2, len);
			return SUCCESS;
		}
	}
	GTP_ERROR("i2c read 0x%04X, %d bytes, double check failed!", addr, len);
	return FAIL;
}

int i2c_write_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
	u8 buffer[MAX_TRANSACTION_LENGTH];
	u16 left = len;
	u16 offset = 0;
	u8 retry = 0;

	struct i2c_msg msg = {
#ifdef CONFIG_MTK_I2C_EXTENSION
		.addr = ((client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
		/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
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

	GTP_DEBUG("i2c_write_bytes to device %02X address %04X len %d", client->addr, addr, len);

	while (left > 0) {
		retry = 0;

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

		/* GTP_DEBUG("byte left %d offset %d\n", left, offset); */

		while (i2c_transfer(client->adapter, &msg, 1) != 1) {
			retry++;

			if (retry == 5) {
				GTP_ERROR("I2C write 0x%X%X length=%d failed\n", buffer[0],
						buffer[1], len);
				return -1;
			}
		}
	}

	return 0;
}

int i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
#if defined(CONFIG_GTP_SUPPORT_I2C_DMA)
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
#if defined(CONFIG_GTP_SLIDE_WAKEUP)
	if (DOZE_ENABLED == doze_status)
		return ret;
#endif
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		gtp_recovery_reset(client);
	} else
#endif
	{
		gtp_reset_guitar(client, 20);
	}
		return ret;
}



/*******************************************************
Function:
    Send config Function.

Input:
    client: i2c client.

Output:
    Executive outcomes.0--success,non-0--fail.
*******************************************************/
s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret = 1;

#if defined(CONFIG_GTP_DRIVER_SEND_CFG)
	s32 retry = 0;

	if (fixed_config) {
		GTP_INFO("Ic fixed config, no config sent!");
		return 0;
	} else if (pnl_init_error) {
		GTP_INFO("Error occurred in init_panel, no config sent!");
		return 0;
	}

	GTP_DEBUG("Driver Send Config");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);

		if (ret > 0)
			break;
	}
#endif
	return ret;
}


/*******************************************************
Function:
    Send charger config Function.

Input:
    client: i2c client.

Output:
    Executive outcomes.0--success,non-0--fail.
*******************************************************/
#ifdef CONFIG_GTP_CHARGER_SWITCH
s32 gtp_send_cfg_for_charger(struct i2c_client *client)
{
	s32 ret = 1;
	int check_sum = 0;
	int i = 0;

#ifdef CONFIG_GTP_DRIVER_SEND_CFG
	s32 retry = 0;

	GTP_INFO("gtp_send_cfg_for_charger!");

	if (fixed_config) {
		GTP_INFO("Ic fixed config, no config sent!");
		return 0;
	} else if (pnl_init_error) {
		GTP_INFO("Error occurred in init_panel, no config sent!");
		return 0;
	}

	GTP_DEBUG("gtp_send_cfg_for_charger Send Config");
	/* charger_config[2] = 0x00; */
	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < charger_cfg_len; i++)
		check_sum += charger_config[i];

	charger_config[charger_cfg_len] = (~check_sum) + 1;

	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, charger_config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;

	}
#endif
	return ret;
}

#endif
/*******************************************************
Function:
	  Read goodix touchscreen version function.

Input:
    client: i2c client struct.
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

	for (i = 0; i < 4; i++) {
		if (buf[i + 2] < 0x30)
			break;

		tpd_info.pid |= ((buf[i + 2] - 0x30) << ((3 - i) * 4));
	}

	if (buf[5] == 0x00) {
		GTP_INFO("IC VERSION: %c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[7], buf[6]);
	} else {
		if (buf[5] == 'S' || buf[5] == 's')
			chip_gt9xxs = 1;
		GTP_INFO("IC VERSION:%c%c%c%c_%02x%02x",
			 buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
	}
	return ret;
}

#if defined(CONFIG_GTP_DRIVER_SEND_CFG)
/*******************************************************
Function:
    Get information from ic, such as resolution and
    int trigger type
Input:
    client: i2c client private struct.

Output:
    FAIL: i2c failed, SUCCESS: i2c ok
*******************************************************/
static s32 gtp_get_info(struct i2c_client *client)
{
	u8 opr_buf[6] = { 0 };
	s32 ret = 0;

	opr_buf[0] = (u8) ((GTP_REG_CONFIG_DATA + 1) >> 8);
	opr_buf[1] = (u8) ((GTP_REG_CONFIG_DATA + 1) & 0xFF);

	ret = gtp_i2c_read(client, opr_buf, 6);
	if (ret < 0)
		return FAIL;

	abs_x_max = (opr_buf[3] << 8) + opr_buf[2];
	abs_y_max = (opr_buf[5] << 8) + opr_buf[4];

	opr_buf[0] = (u8) ((GTP_REG_CONFIG_DATA + 6) >> 8);
	opr_buf[1] = (u8) ((GTP_REG_CONFIG_DATA + 6) & 0xFF);

	ret = gtp_i2c_read(client, opr_buf, 3);
	if (ret < 0)
		return FAIL;
	int_type = opr_buf[2] & 0x03;

	GTP_INFO("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x", abs_x_max, abs_y_max, int_type);

	return SUCCESS;
}
#endif


/*******************************************************
Function:
    GTP initialize function.

Input:
    client: i2c client private struct.

Output:
    Executive outcomes.0---succeed.
*******************************************************/
static s32 gtp_init_panel(struct i2c_client *client)
{
	s32 ret = 0;

#if defined(CONFIG_GTP_DRIVER_SEND_CFG)
	s32 i;
	u8 check_sum = 0;
	u8 opr_buf[16];
	u8 sensor_id = 0;

	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;
	u8 cfg_info_group6[] = CTP_CFG_GROUP6;
	u8 *send_cfg_buf[] = { cfg_info_group1, cfg_info_group2, cfg_info_group3,
		cfg_info_group4, cfg_info_group5, cfg_info_group6
	};
	u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group1),
		CFG_GROUP_LEN(cfg_info_group2),
		CFG_GROUP_LEN(cfg_info_group3),
		CFG_GROUP_LEN(cfg_info_group4),
		CFG_GROUP_LEN(cfg_info_group5),
		CFG_GROUP_LEN(cfg_info_group6)
	};
	#ifdef CONFIG_GTP_CHARGER_SWITCH
	u8 cfg_info_group1_charger[] = CTP_CFG_GROUP1_CHARGER;

	u8 *send_charger_cfg_buf[] = {cfg_info_group1_charger, cfg_info_group2, cfg_info_group3,
		cfg_info_group4, cfg_info_group5, cfg_info_group6};
	u8 charger_cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group1_charger),
		CFG_GROUP_LEN(cfg_info_group2),
		CFG_GROUP_LEN(cfg_info_group3),
		CFG_GROUP_LEN(cfg_info_group4),
		CFG_GROUP_LEN(cfg_info_group5),
		CFG_GROUP_LEN(cfg_info_group6)};
	GTP_DEBUG("Charger Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
		charger_cfg_info_len[0], charger_cfg_info_len[1], charger_cfg_info_len[2], charger_cfg_info_len[3],
		charger_cfg_info_len[4], charger_cfg_info_len[5]);
#endif
	GTP_DEBUG("Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
			cfg_info_len[0], cfg_info_len[1], cfg_info_len[2], cfg_info_len[3],
			cfg_info_len[4], cfg_info_len[5]);

	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) &&
			(!cfg_info_len[3]) && (!cfg_info_len[4]) && (!cfg_info_len[5])) {
		sensor_id = 0;
	} else {
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
		if (CHIP_TYPE_GT9F == gtp_chip_type)
			msleep(50);
#endif
		ret = gtp_i2c_read_dbl_check(client, GTP_REG_SENSOR_ID, &sensor_id, 1);
		if (SUCCESS == ret) {
			if (sensor_id >= 0x06) {
				GTP_ERROR("Invalid sensor_id(0x%02X), No Config Sent!", sensor_id);
				pnl_init_error = 1;

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
				if (CHIP_TYPE_GT9F == gtp_chip_type) {
					/* do nothing; */
				} else
#endif
				{
					gtp_get_info(client);
				}
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
#ifdef CONFIG_GTP_CHARGER_SWITCH
		charger_cfg_len = charger_cfg_info_len[sensor_id];
		GTP_INFO("CHARGER_CTP_CONFIG_GROUP%d used, config length: %d", sensor_id + 1, charger_cfg_len);

	if (charger_cfg_len < GTP_CONFIG_MIN_LENGTH) {
		GTP_ERROR("CHARGER_CTP_CONFIG_GROUP%d is INVALID CONFIG GROUP! NO Config Sent!", sensor_id+1);
		GTP_ERROR("You need to check you header file CFG_GROUP section!");
		pnl_init_error = 1;
		return -1;
	}
#endif
	GTP_INFO("CTP_CONFIG_GROUP%d used, config length: %d", sensor_id + 1, cfg_len);

	if (cfg_len < GTP_CONFIG_MIN_LENGTH) {
		GTP_ERROR
				("CTP_CONFIG_GROUP%d is INVALID CONFIG GROUP!",
				 sensor_id + 1);
		pnl_init_error = 1;
		return -1;
	}
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		fixed_config = 0;
	} else
#endif
	{
		ret = gtp_i2c_read_dbl_check(client, GTP_REG_CONFIG_DATA, &opr_buf[0], 1);

		if (ret == SUCCESS) {
			GTP_DEBUG
					("CFG_CONFIG_GROUP%d Config Version: %d, 0x%02X; IC Config Version: %d, 0x%02X",
					 sensor_id + 1, send_cfg_buf[sensor_id][0], send_cfg_buf[sensor_id][0],
					 opr_buf[0], opr_buf[0]);

			if (opr_buf[0] < 90) {
				grp_cfg_version = send_cfg_buf[sensor_id][0];	/* backup group config version */
				/* send_cfg_buf[sensor_id][0] = 0x00; */
#ifdef CONFIG_GTP_CHARGER_SWITCH
				charger_grp_cfg_version = send_charger_cfg_buf[sensor_id][0];
				/* send_charger_cfg_buf[sensor_id][0] = 0x00; */
#endif
		fixed_config = 0;
			} else {
				GTP_INFO("Ic fixed config with config version(%d)", opr_buf[0]);
				fixed_config = 1;
			}
		} else {
			GTP_ERROR("Failed to get ic config version!No config sent!");
			return -1;
		}
	}

	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], cfg_len);

	#ifdef CONFIG_GTP_CHARGER_SWITCH
	memset(&charger_config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&charger_config[GTP_ADDR_LENGTH], send_charger_cfg_buf[sensor_id], charger_cfg_len);
	#endif
#if defined(CONFIG_GTP_CUSTOM_CFG)
	config[RESOLUTION_LOC] = (u8) tpd_dts_data.tpd_resolution[0];
	config[RESOLUTION_LOC + 1] = (u8) (tpd_dts_data.tpd_resolution[0] >> 8);
	config[RESOLUTION_LOC + 2] = (u8) tpd_dts_data.tpd_resolution[1];
	config[RESOLUTION_LOC + 3] = (u8) (tpd_dts_data.tpd_resolution[1] >> 8);

	if (GTP_INT_TRIGGER == 0)	/* RISING */
		config[TRIGGER_LOC] &= 0xfe;
	else if (GTP_INT_TRIGGER == 1)	/* FALLING */
		config[TRIGGER_LOC] |= 0x01;
#ifdef CONFIG_GTP_CHARGER_SWITCH
		charger_config[RESOLUTION_LOC]		 = (u8) tpd_dts_data.tpd_resolution[0];
		charger_config[RESOLUTION_LOC + 1] = (u8) (tpd_dts_data.tpd_resolution[0] >> 8);
		charger_config[RESOLUTION_LOC + 2] = (u8) tpd_dts_data.tpd_resolution[1];
		charger_config[RESOLUTION_LOC + 3] = (tpd_dts_data.tpd_resolution[1] >> 8);

		if (GTP_INT_TRIGGER == 0)	/* RISING */
			charger_config[TRIGGER_LOC] &= 0xfe;
		else if (GTP_INT_TRIGGER == 1)	/* FALLING */
			charger_config[TRIGGER_LOC] |= 0x01;
#endif

#endif	/* GTP_CUSTOM_CFG */

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
		check_sum += config[i];
	config[cfg_len] = (~check_sum) + 1;

	#ifdef CONFIG_GTP_CHARGER_SWITCH
		check_sum = 0;
		for (i = GTP_ADDR_LENGTH; i < charger_cfg_len; i++)
			check_sum += charger_config[i];

		charger_config[charger_cfg_len] = (~check_sum) + 1;
	#endif
#else				/* DRIVER NOT SEND CONFIG */
	cfg_len = GTP_CONFIG_MAX_LENGTH;
	ret = gtp_i2c_read(client, config, cfg_len + GTP_ADDR_LENGTH);
	if (ret < 0) {
		GTP_ERROR("Read Config Failed, Using DEFAULT Resolution & INT Trigger!");
		abs_x_max = GTP_MAX_WIDTH;
		abs_y_max = GTP_MAX_HEIGHT;
		int_type = GTP_INT_TRIGGER;
	}
#endif

	GTP_DEBUG_FUNC();
	if ((abs_x_max == 0) && (abs_y_max == 0)) {
		abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
		abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
		int_type = (config[TRIGGER_LOC]) & 0x03;
	}
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		u8 have_key = 0;

		if (!memcmp(&gtp_default_FW_fl[4], "950", 3)) {
			driver_num = config[GTP_REG_MATRIX_DRVNUM - GTP_REG_CONFIG_DATA + 2];
			sensor_num = config[GTP_REG_MATRIX_SENNUM - GTP_REG_CONFIG_DATA + 2];
		} else {
			driver_num =
					(config[CFG_LOC_DRVA_NUM] & 0x1F) + (config[CFG_LOC_DRVB_NUM] & 0x1F);
			sensor_num =
					(config[CFG_LOC_SENS_NUM] & 0x0F) +
					((config[CFG_LOC_SENS_NUM] >> 4) & 0x0F);
		}

		have_key = config[GTP_REG_HAVE_KEY - GTP_REG_CONFIG_DATA + 2] & 0x01;	/* have key or not */
		if (1 == have_key)
			driver_num--;

		GTP_INFO
				("Driver * Sensor: %d * %d(Key: %d), X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
				 driver_num, sensor_num, have_key, abs_x_max, abs_y_max, int_type);
	} else
#endif
	{
#if defined(CONFIG_GTP_DRIVER_SEND_CFG)
		ret = gtp_send_cfg(client);
		if (ret < 0)
			GTP_ERROR("Send config error.");
		/* set config version to CTP_CFG_GROUP */
		/* for resume to send config */
	/* config[GTP_ADDR_LENGTH] = grp_cfg_version; */
		check_sum = 0;
		for (i = GTP_ADDR_LENGTH; i < cfg_len; i++)
			check_sum += config[i];
		config[cfg_len] = (~check_sum) + 1;
		/**********************/
		#ifdef CONFIG_GTP_CHARGER_SWITCH
	/* charger_config[GTP_ADDR_LENGTH] = charger_grp_cfg_version; */
	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < charger_cfg_len; i++)
			check_sum += charger_config[i];

	charger_config[charger_cfg_len] = (~check_sum) + 1;
		#endif
		/**********************/
		#endif
		GTP_INFO("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
			 abs_x_max, abs_y_max, int_type);
	}

	msleep(20);
	return 0;
}

static s8 gtp_i2c_test(struct i2c_client *client)
{

	u8 retry = 0;
	s8 ret = -1;
	u32 hw_info = 0;

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = i2c_read_bytes(client, GTP_REG_HW_INFO, (u8 *) &hw_info, sizeof(hw_info));

		if ((!ret) && (hw_info == 0x00900600))
			return ret;

		GTP_ERROR("GTP_REG_HW_INFO : %08X", hw_info);
		GTP_ERROR("GTP i2c test failed time %d.", retry);
		msleep(20);
	}

	return -1;
}



/*******************************************************
Function:
		Set INT pin	as input for FW sync.

Note:
	If the INT is high, It means there is pull up resistor attached on the INT pin.
	Pull low the INT pin manaully for FW sync.
*******************************************************/
void gtp_int_sync(s32 ms)
{
	tpd_gpio_output(1, 0);
	msleep(ms);
	tpd_gpio_as_int(1);
}

void gtp_reset_guitar(struct i2c_client *client, s32 us)
{
	GTP_INFO("GTP RESET!\n");
	tpd_gpio_output(0, 0);
	usleep_range(us, us + 100);
	tpd_gpio_output(1, client->addr == 0x14);

	usleep_range(2000, 2100);
	tpd_gpio_output(0, 1);

	usleep_range(6000, 6100);		/* must >= 6ms */

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type)
		return;
#endif

	gtp_int_sync(51);	/* for dbl-system */
#if defined(CONFIG_GTP_ESD_PROTECT)
	gtp_init_ext_watchdog(i2c_client_tp_point);
#endif
}

static int tpd_power_on(struct i2c_client *client)
{
	int ret = 0;
	int reset_count = 0;

reset_proc:
	tpd_gpio_output(0, 0);
	tpd_gpio_output(1, 0);

	ret = regulator_set_voltage(tpd->reg, 2800000, 2800000);	/* set 2.8v */
	if (ret)
		GTP_DEBUG("regulator_set_voltage() failed!\n");
	ret = regulator_enable(tpd->reg);	/* enable regulator */
	if (ret)
		GTP_DEBUG("regulator_enable() failed!\n");

	gtp_reset_guitar(client, 11000);

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	gtp_get_chip_type(client);

	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		ret = (int)gup_load_main_system(NULL);
		if (FAIL == ret) {
			GTP_ERROR("[tpd_power_on]Download fw failed.");
			if (reset_count++ < TPD_MAX_RESET_COUNT)
				goto reset_proc;
			else
				return ret;
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

/* **************** For GT9XXF Start ********************/
#if defined(CONFIG_GTP_COMPATIBLE_MODE)

void gtp_get_chip_type(struct i2c_client *client)
{
	u8 opr_buf[10] = { 0x00 };
	s32 ret = 0;

	msleep(20);

	ret = gtp_i2c_read_dbl_check(client, GTP_REG_CHIP_TYPE, opr_buf, 10);

	if (FAIL == ret) {
		GTP_ERROR("Failed to get chip-type, set chip type default: GOODIX_GT9");
		gtp_chip_type = CHIP_TYPE_GT9;
		tpd_load_status = 0;
		return;
	}

	if (!memcmp(opr_buf, "GOODIX_GT9", 10)) {
		GTP_INFO("Chip Type: %s",
			 (gtp_chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
		gtp_chip_type = CHIP_TYPE_GT9;
	} else {
		gtp_chip_type = CHIP_TYPE_GT9F;
		GTP_INFO("Chip Type: %s",
			 (gtp_chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
	}
	#ifdef CUSTOM_CHIP_TYPE
		gtp_chip_type = CUSTOM_CHIP_TYPE; /* for test */
		#endif
	GTP_INFO("Chip Type: %s", (gtp_chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");

	/*tpd_load_status = 1;
	check_flag = true;*/
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

	GTP_DEBUG("[gtp_bak_ref_proc]Driver:%d,Sensor:%d.", driver_num, sensor_num);

	/* check file-system mounted */
	GTP_DEBUG("[gtp_bak_ref_proc]Waiting for FS %d", gtp_ref_retries);
	if (gup_check_fs_mounted("/data") == FAIL) {
		GTP_DEBUG("[gtp_bak_ref_proc]/data not mounted");
		if (gtp_ref_retries++ < GTP_CHK_FS_MNT_MAX)
			return FAIL;
	} else {
		GTP_DEBUG("[gtp_bak_ref_proc]/data mounted !!!!");
	}

	if (!memcmp(&gtp_default_FW_fl[4], "950", 3)) {
		ref_seg_len = (driver_num * (sensor_num - 1) + 2) * 2;
		ref_grps = 6;
		ref_len = ref_seg_len * 6;	/* for GT950, backup-reference for six segments */
	} else {
		ref_len = driver_num * (sensor_num - 2) * 2 + 4;
		ref_seg_len = ref_len;
		ref_grps = 1;
	}

	refp = kzalloc(ref_len, GFP_KERNEL);
	if (refp == NULL) {
		GTP_ERROR("[gtp_bak_ref_proc]Alloc memory for ref failed.use default ref");
		return FAIL;
	}
	memset(refp, 0, ref_len);
	if (gtp_ref_retries >= GTP_CHK_FS_MNT_MAX) {
		for (j = 0; j < ref_grps; ++j)
			refp[ref_seg_len + j * ref_seg_len - 1] = 0x01;
		ret = i2c_write_bytes(client, 0x99D0, refp, ref_len);
		if (-1 == ret) {
			GTP_ERROR("[gtp_bak_ref_proc]Write ref i2c error.");
			ret = FAIL;
		}

		GTP_ERROR("[gtp_bak_ref_proc]Bak file or path is not exist,send default ref.");
		ret = SUCCESS;
		goto exit_ref_proc;
	}
	/* get ref file data */
	flp = filp_open(GTP_BAK_REF_PATH, O_RDWR | O_CREAT, 0660);
	if (IS_ERR(flp)) {
		GTP_ERROR("[gtp_bak_ref_proc]Ref File not found!Creat ref file.");
		/* flp->f_op->llseek(flp, 0, SEEK_SET); */
		/* flp->f_op->write(flp, (char *)refp, ref_len, &flp->f_pos); */
		gtp_ref_retries++;
		ret = FAIL;
		goto exit_ref_proc;
	} else if (GTP_BAK_REF_SEND == mode) {
		flp->f_op->llseek(flp, 0, SEEK_SET);
		ret = flp->f_op->read(flp, (char *)refp, ref_len, &flp->f_pos);
		if (ret < 0) {
			GTP_ERROR("[gtp_bak_ref_proc]Read ref file failed.");
			memset(refp, 0, ref_len);
		}
	}

	if (GTP_BAK_REF_STORE == mode) {
		ret = i2c_read_bytes(client, 0x99D0, refp, ref_len);
		if (-1 == ret) {
			GTP_ERROR("[gtp_bak_ref_proc]Read ref i2c error.");
			ret = FAIL;
			goto exit_ref_proc;
		}
		flp->f_op->llseek(flp, 0, SEEK_SET);
		flp->f_op->write(flp, (char *)refp, ref_len, &flp->f_pos);
	} else {
		/* checksum ref file */
		for (j = 0; j < ref_grps; ++j) {
			ref_chksum = 0;
			for (i = 0; i < ref_seg_len - 2; i += 2) {
				ref_chksum +=
						((refp[i + j * ref_seg_len] << 8) +
						 refp[i + 1 + j * ref_seg_len]);
			}

			GTP_DEBUG("[gtp_bak_ref_proc]Calc ref chksum:0x%04X", ref_chksum & 0xFF);
			tmp =
					ref_chksum + (refp[ref_seg_len + j * ref_seg_len - 2] << 8) +
					refp[ref_seg_len + j * ref_seg_len - 1];
			if (1 != tmp) {
				GTP_DEBUG
						("[gtp_bak_ref_proc]Ref file chksum error,use default ref");
				memset(&refp[j * ref_seg_len], 0, ref_seg_len);
				refp[ref_seg_len - 1 + j * ref_seg_len] = 0x01;
			} else {
				if (j == (ref_grps - 1))
					GTP_DEBUG("[gtp_bak_ref_proc]Ref file chksum success.");
			}

		}

		ret = i2c_write_bytes(client, 0x99D0, refp, ref_len);
		if (-1 == ret) {
			GTP_ERROR("[gtp_bak_ref_proc]Write ref i2c error.");
			ret = FAIL;
			goto exit_ref_proc;
		}
	}

	ret = SUCCESS;

exit_ref_proc:
	kfree(refp);
	if (flp && !IS_ERR(flp))
		filp_close(flp, NULL);
	return ret;
}

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
	if (0xAA == wr_buf[0]) {
		GTP_ERROR("IC works abnormally,startup failed.");
		return FAIL;
	}

	GTP_DEBUG("IC works normally,Startup success.");
	wr_buf[0] = 0xAA;
	i2c_write_bytes(client, 0x8041, wr_buf, 1);
	return SUCCESS;
}

static void gtp_recovery_reset(struct i2c_client *client)
{
		/* mutex_lock(&i2c_access); */
	if (tpd_halt == 0) {
#if defined(CONFIG_GTP_ESD_PROTECT)
		gtp_esd_switch(client, SWITCH_OFF);
#endif
		force_reset_guitar();
#if defined(CONFIG_GTP_ESD_PROTECT)
		gtp_esd_switch(client, SWITCH_ON);
#endif
	}
		/* mutex_unlock(&i2c_access); */
}

static u8 gtp_check_clk_legality(void)
{
	u8 i = 0;
	u8 clk_chksum = gtp_clk_buf[5];

	for (i = 0; i < 5; i++) {
		if ((gtp_clk_buf[i] < 50) || (gtp_clk_buf[i] > 120) ||
				(gtp_clk_buf[i] != gtp_clk_buf[0])) {
			break;
		}
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

	/* check clk legality */
	ret = gtp_check_clk_legality();
	if (SUCCESS == ret)
		goto send_main_clk;

	GTP_DEBUG("[gtp_main_clk_proc]Waiting for FS %d", gtp_ref_retries);
	if (gup_check_fs_mounted("/data") == FAIL) {
		GTP_DEBUG("[gtp_main_clk_proc]/data not mounted");
		if (gtp_clk_retries++ < GTP_CHK_FS_MNT_MAX)
			return FAIL;
		GTP_ERROR("[gtp_main_clk_proc]Wait for file system timeout,need cal clk");
	} else {
		GTP_DEBUG("[gtp_main_clk_proc]/data mounted !!!!");
		flp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0660);
		if (!IS_ERR(flp)) {
			flp->f_op->llseek(flp, 0, SEEK_SET);
			ret = flp->f_op->read(flp, (char *)gtp_clk_buf, 6, &flp->f_pos);
			if (ret > 0) {
				ret = gtp_check_clk_legality();
				if (SUCCESS == ret) {
					GTP_DEBUG
							("[gtp_main_clk_proc]Open & read & check clk file success.");
					goto send_main_clk;
				}
			}
		}
		GTP_ERROR("[gtp_main_clk_proc]Check clk file failed,need cal clk");
	}

	/* cal clk */
#if defined(CONFIG_GTP_ESD_PROTECT)
	gtp_esd_switch(client, SWITCH_OFF);
#endif
	clk_cal_result = gup_clk_calibration();
	force_reset_guitar();
	GTP_DEBUG("&&&&&&&&&&clk cal result:%d", clk_cal_result);

#if defined(CONFIG_GTP_ESD_PROTECT)
	gtp_esd_switch(client, SWITCH_ON);
#endif

	if (clk_cal_result < 50 || clk_cal_result > 120) {
		GTP_ERROR("[gtp_main_clk_proc]cal clk result is illegitimate");
		ret = FAIL;
		goto exit_clk_proc;
	}

	for (i = 0; i < 5; i++) {
		gtp_clk_buf[i] = clk_cal_result;
		clk_chksum += gtp_clk_buf[i];
	}
	gtp_clk_buf[5] = 0 - clk_chksum;

	if (IS_ERR(flp)) {
		flp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0660);
	} else {
		flp->f_op->llseek(flp, 0, SEEK_SET);
		flp->f_op->write(flp, (char *)gtp_clk_buf, 6, &flp->f_pos);
	}


send_main_clk:

	ret = i2c_write_bytes(client, 0x8020, gtp_clk_buf, 6);
	if (-1 == ret) {
		GTP_ERROR("[gtp_main_clk_proc]send main clk i2c error!");
		ret = FAIL;
		goto exit_clk_proc;
	}

	ret = SUCCESS;

exit_clk_proc:
	if (flp && !IS_ERR(flp))
		filp_close(flp, NULL);
	return ret;
}

#endif
/* ************* For GT9XXF End **********************/

static const struct file_operations gt_upgrade_proc_fops = {
	.write = gt91xx_config_write_proc,
	.read = gt91xx_config_read_proc
};

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = { 0, 0 };

	GTP_INFO("Device Tree Tpd_irq_registration!");

	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);

		touch_irq = irq_of_parse_and_map(node, 0);
		if (!int_type) {/*EINTF_TRIGGER*/
			ret =
			    request_irq(touch_irq, (irq_handler_t) tpd_eint_interrupt_handler, IRQF_TRIGGER_RISING,
					"TOUCH_PANEL-eint", NULL);
			if (ret > 0) {
				ret = -1;
				GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
			}
		} else {
			ret =
			    request_irq(touch_irq, (irq_handler_t) tpd_eint_interrupt_handler, IRQF_TRIGGER_FALLING,
					"TOUCH_PANEL-eint", NULL);
			if (ret > 0) {
				ret = -1;
				GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
			}
		}
	} else {
		GTP_ERROR("tpd request_irq can not find touch eint device node!.");
		ret = -1;
	}
	GTP_INFO("[%s]irq:%d, debounce:%d-%d:", __func__, touch_irq, ints[0], ints[1]);
	return ret;
}

int tpd_reregister_from_tui(void)
{
	int ret = 0;

	free_irq(touch_irq, NULL);

	ret = tpd_irq_registration();
	if (ret < 0) {
			ret = -1;
			GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	}
	return ret;
}


static int tpd_registration(void *unused)
{
	s32 err = 0;
	s32 ret = 0;

	u16 version_info;
	s32 idx = 0;
#if defined(CONFIG_TPD_PROXIMITY)
	struct hwmsen_object obj_ps;
#endif

	ret = tpd_power_on(i2c_client_tp_point);

	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		goto exit_fail;
	}
	tpd_load_status = 1;
	check_flag = true;
#ifdef VELOCITY_CUSTOM
	tpd_v_magnify_x = TPD_VELOCITY_CUSTOM_X;
	tpd_v_magnify_y = TPD_VELOCITY_CUSTOM_Y;

#endif
#if TOUCH_FILTER
		memcpy(&tpd_filter, &tpd_filter_local, sizeof(struct tpd_filter_t));

#endif

	ret = gtp_read_version(i2c_client_tp_point, &version_info);

	if (ret < 0) {
		GTP_ERROR("Read version failed.");
		goto exit_fail;
	}

	ret = gtp_init_panel(i2c_client_tp_point);

	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		goto exit_fail;
	}
	/* Create proc file system */
	gt91xx_config_proc =
			proc_create(GT91XX_CONFIG_PROC_FILE, 0660, NULL,
			&gt_upgrade_proc_fops);
	if (gt91xx_config_proc == NULL) {
		GTP_ERROR("create_proc_entry %s failed\n",
				GT91XX_CONFIG_PROC_FILE);
		goto exit_fail;
	}

#if defined(CONFIG_GTP_CREATE_WR_NODE)
	init_wr_node(i2c_client_tp_point);
#endif

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_INFO(TPD_DEVICE "failed create thread: %d\n", err);
		goto exit_fail;
	}

	if (tpd_dts_data.use_tpd_button) {
		for (idx = 0; idx < tpd_dts_data.tpd_key_num; idx++)
			input_set_capability(tpd->dev, EV_KEY,
							 tpd_dts_data.tpd_key_local[idx]);
	}
#if defined(CONFIG_GTP_SLIDE_WAKEUP)
	input_set_capability(tpd->dev, EV_KEY, KEY_POWER);
#endif

#if defined(CONFIG_GTP_WITH_PEN)
	/* pen support */
	__set_bit(BTN_TOOL_PEN, tpd->dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);
	/* __set_bit(INPUT_PROP_POINTER, tpd->dev->propbit); // 20130722 */
#endif
	/* set INT mode */
	tpd_gpio_as_int(1);

	msleep(50);

	/* EINT device tree, default EINT enable */
	tpd_irq_registration();


#if defined(CONFIG_GTP_AUTO_UPDATE)
	ret = gup_init_update_proc(i2c_client_tp_point);

	if (ret < 0) {
		GTP_ERROR("Create update thread error.");
		goto exit_fail;
	}
#endif

#if defined(CONFIG_TPD_PROXIMITY)
	/* obj_ps.self = cm3623_obj; */
	obj_ps.polling = 0;	/* 0--interrupt mode;1--polling mode; */
	obj_ps.sensor_operate = tpd_ps_operate;

	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err) {
		GTP_ERROR("hwmsen attach fail, return:%d.", err);
		goto exit_fail;
	}
#endif

#if defined(CONFIG_GTP_ESD_PROTECT)
	gtp_esd_switch(client, SWITCH_ON);
#endif
	GTP_INFO("tpd registration done.");
	return 0;

exit_fail:
	GTP_ERROR("tpd registration fail.");
	tpd_load_status = 0;
	check_flag = false;
	return -1;
}
static s32 tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	int count = 0;

	GTP_INFO("tpd_i2c_probe start.");

	i2c_client_tp_point = client;
	if (RECOVERY_BOOT == get_boot_mode())
			return 0;

	probe_thread = kthread_run(tpd_registration, client, "tpd_probe");
	usleep_range(1000, 1100);
	if (IS_ERR(probe_thread)) {
		err = PTR_ERR(probe_thread);
		GTP_INFO(TPD_DEVICE " failed to create probe thread: %d\n", err);
		return err;
	}

	do {
		usleep_range(3000, 3100);
		count++;
		if (check_flag)
			break;
	} while (count < 120);
	GTP_INFO("tpd_i2c_probe done.count = %d, flag = %d", count, check_flag);
	return 0;
}

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	TPD_DEBUG_PRINT_INT;

	tpd_flag = 1;
	/* enter EINT handler disable INT, make sure INT is disable when handle touch event including top/bottom half */
	/* use _nosync to avoid deadlock */
	/*disable_irq_nosync(touch_irq);*/
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

static int tpd_i2c_remove(struct i2c_client *client)
{
#if defined(CONFIG_GTP_CREATE_WR_NODE)
	uninit_wr_node();
#endif

#if defined(CONFIG_GTP_ESD_PROTECT)
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

	return 0;
}

#if (defined(CONFIG_GTP_ESD_PROTECT) || defined(CONFIG_GTP_COMPATIBLE_MODE))
void force_reset_guitar(void)
{
	s32 i = 0;
	s32 ret = 0;
	/* static u8 is_resetting = 0; */

	if (is_resetting || (load_fw_process == 1))
		return;
	GTP_INFO("force_reset_guitar");
	is_resetting = 1;
	/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);

	tpd_gpio_output(0, 0);
	tpd_gpio_output(1, 0);

	/* Power off TP */
	ret = regulator_disable(tpd->reg);
	if (ret)
		GTP_DEBUG("regulator_disable() failed!\n");
#ifdef TPD_POWER_SOURCE_1800
	hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
#endif
	msleep(30);

	/* Power on TP */
	ret = regulator_set_voltage(tpd->reg,
						2800000, 2800000);
	if (ret)
		GTP_DEBUG("regulator_set_voltage() failed!\n");
	ret = regulator_enable(tpd->reg);	/* enable regulator */
	if (ret)
		GTP_DEBUG("regulator_enable() failed!\n");
	msleep(30);

	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);

	for (i = 0; i < 5; i++) {
		/* Reset Guitar */
		gtp_reset_guitar(i2c_client_tp_point, 20);

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
		if (CHIP_TYPE_GT9F == gtp_chip_type) {
			/* check code ram */
			ret = gup_load_main_system(NULL);
			if (FAIL == ret) {
				GTP_ERROR("[force_reset_guitar]Check & repair fw failed.");
				continue;
			}
		} else
#endif
		{
			/* Send config */
			ret = gtp_send_cfg(i2c_client_tp_point);

			if (ret < 0)
				continue;
		}
		break;
	}
	is_resetting = 0;
}
#endif

#if defined(CONFIG_GTP_ESD_PROTECT)
static s32 gtp_init_ext_watchdog(struct i2c_client *client)
{
	u8 opr_buffer[2] = { 0xAA };

	GTP_DEBUG("Init external watchdog.");
	return i2c_write_bytes(client, 0x8041, opr_buffer, 1);
}

void gtp_esd_switch(struct i2c_client *client, s32 on)
{
	spin_lock(&esd_lock);
	if (SWITCH_ON == on) {
		if (!esd_running) {
			esd_running = 1;
			spin_unlock(&esd_lock);
			GTP_INFO("Esd started");
			queue_delayed_work(gtp_esd_check_workqueue,
						 &gtp_esd_check_work,
						 clk_tick_cnt);
		} else {
			spin_unlock(&esd_lock);
		}
	} else {
		if (esd_running) {
			esd_running = 0;
			spin_unlock(&esd_lock);
			GTP_INFO("Esd cancelled");
			cancel_delayed_work(&gtp_esd_check_work);
		} else {
			spin_unlock(&esd_lock);
		}
	}
}


static void gtp_esd_check_func(struct work_struct *work)
{
	s32 i = 0;
	s32 ret = -1;
	u8 esd_buf[2] = { 0x00 };

	if (tpd_halt) {
		GTP_INFO("Esd suspended!");
		return;
	}
	if (1 == load_fw_process) {
		GTP_INFO("Load FW process is running");
		return;
	}
	for (i = 0; i < 3; i++) {
		ret = i2c_read_bytes_non_dma(i2c_client_tp_point,
							 0x8040, esd_buf, 2);

		GTP_DEBUG("[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X",
				esd_buf[0], esd_buf[1]);
		if (ret < 0) {
			/* IIC communication problem */
			continue;
		} else {
			if ((esd_buf[0] == 0xAA) || (esd_buf[1] != 0xAA)) {
				u8 chk_buf[2] = { 0x00 };

				i2c_read_bytes_non_dma(i2c_client_tp_point,
									 0x8040, chk_buf, 2);

				GTP_DEBUG("0x8040 = 0x%02X, 0x8041 = 0x%02X",
						chk_buf[0],
						chk_buf[1]);

				if ((chk_buf[0] == 0xAA) ||
						(chk_buf[1] != 0xAA)) {
					i = 3;	/* jump to reset guitar */
					break;
				}
				continue;
			} else {
				esd_buf[0] = 0xAA;
				i2c_write_bytes_non_dma(i2c_client_tp_point,
							0x8040, esd_buf, 1);

				break;
			}
		}
	}

	if (i >= 3) {
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
		if ((CHIP_TYPE_GT9F == gtp_chip_type) &&
				(1 == rqst_processing)) {
			GTP_INFO("Request Processing, no reset guitar.");
		} else
#endif
		{
			GTP_INFO("IC works abnormally! Process reset guitar.");
			memset(esd_buf, 0x01, sizeof(esd_buf));
			i2c_write_bytes(i2c_client_tp_point, 0x4226,
					esd_buf, sizeof(esd_buf));
			msleep(50);
			force_reset_guitar();
		}
	}
#if FLASHLESS_FLASH_WORKROUND
	{
		u8 version_buff[6];
		int retry = 0;
		u8 temp = 0;

		while (retry++ < 3) {
			ret =
					i2c_read_bytes_non_dma(i2c_client_tp_point,
							 0x8140, version_buff,
							 sizeof(version_buff));
			if (ret < 0)
				continue;
			/* match pid */
			if (memcmp(version_buff,
					 &gtp_default_FW_fl[4], 4) != 0)
				continue;
			temp = version_buff[5];
			version_buff[5] = version_buff[4];
			version_buff[4] = temp;
			/* match vid */
			if (memcmp(&version_buff[4],
					 &gtp_default_FW_fl[12], 2) != 0)
				continue;
			break;
		}
		if (retry >= 3) {
			GTP_INFO("IC version error., force reset!");
			force_reset_guitar();
		}
	}
#endif

	if (!tpd_halt)
		queue_delayed_work(gtp_esd_check_workqueue,
					 &gtp_esd_check_work, clk_tick_cnt);
	else
		GTP_INFO("Esd suspended!");
}
#endif
static int tpd_history_x = 0, tpd_history_y;
static void tpd_down(s32 x, s32 y, s32 size, s32 id)
{
#ifdef CONFIG_GTP_CHARGER_SWITCH
	if (is_charger_cfg_updating) {
		GTP_ERROR("tpd_down ignored when CFG changing\n");
		return;
	}
#endif
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
	GTP_INFO("x:%d, y:%d, lcm_x:%d, lcm_y:%d\n",
	x, y, tpd_dts_data.tpd_resolution[0], tpd_dts_data.tpd_resolution[1]);

	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(x, y, x, y, id, 1);
	tpd_history_x = x;
	tpd_history_y = y;

	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 1, x+y); */
	if (tpd_dts_data.use_tpd_button) {
		if (FACTORY_BOOT == get_boot_mode() ||
				RECOVERY_BOOT == get_boot_mode())
			tpd_button(x, y, 1);
	}
}

static void tpd_up(s32 x, s32 y, s32 id)
{
#ifdef CONFIG_GTP_CHARGER_SWITCH
	if (is_charger_cfg_updating) {
		GTP_ERROR("tpd_up change is_charger_cfg_updating status\n");
		is_charger_cfg_updating = false;
		return;
	}
#endif
	/* input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0); */
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	/* input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0); */
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(tpd_history_x, tpd_history_y,
				 tpd_history_x, tpd_history_y, id, 0);
	tpd_history_x = 0;
	tpd_history_y = 0;
	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 0, x+y); */

	if (tpd_dts_data.use_tpd_button) {
		if (FACTORY_BOOT == get_boot_mode() ||
				RECOVERY_BOOT == get_boot_mode())
			tpd_button(x, y, 0);
	}
}
#if defined(CONFIG_GTP_CHARGER_SWITCH)
static u64 CFG_time_interval;
static void gtp_charger_switch(s32 dir_update)
{
	u32 chr_status = 0;
	u8 chr_cmd[3] = { 0x80, 0x40 };
	static u8 chr_pluggedin;
	int ret = 0;
	u8 buf[3] = {0x81, 0xaa, 0};
	u64 cfg_timestamp = 0;


	if (!g_bat_init_flag)
		return;
	chr_status = upmu_is_chr_det();

	gtp_i2c_read(i2c_client_tp_point, buf, sizeof(buf));
	if (buf[2] == 0x55) {
		GTP_INFO("GTP gtp_charger_switch in Hotknot status CFG update ignored");
		return;
	}

	if (chr_status) {
		if (!chr_pluggedin || dir_update) {
			cfg_timestamp = sched_clock();
			if ((cfg_timestamp - CFG_time_interval) < 500000000) {
				GTP_INFO("Update CFG Operation too fast, ignored");
				return;
			}

		gtp_send_cfg_for_charger(i2c_client_tp_point);
			chr_cmd[2] = 6;
			ret = gtp_i2c_write(i2c_client_tp_point, chr_cmd, 3);
			if (ret > 0)
				GTP_INFO("Update status for Charger Plugin");
			chr_pluggedin = 1;
			if (dir_update != 1)
				is_charger_cfg_updating = true;

			CFG_time_interval = cfg_timestamp;

		}
	} else {
		if (chr_pluggedin || dir_update) {
			cfg_timestamp = sched_clock();
			if ((cfg_timestamp - CFG_time_interval) < 500000000) {
				GTP_INFO("Update CFG Operation too fast, ignored");
				return;
			}

		gtp_send_cfg(i2c_client_tp_point);
			chr_cmd[2] = 7;
			ret = gtp_i2c_write(i2c_client_tp_point, chr_cmd, 3);
			if (ret > 0)
				GTP_INFO("Update status for Charger Plugout");
			chr_pluggedin = 0;
			if (dir_update != 1)
				is_charger_cfg_updating = true;

			CFG_time_interval = cfg_timestamp;
		}
	}
}
#endif

static int touch_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = 4 };
	u8 end_cmd[3] = {GTP_READ_COOR_ADDR >> 8,
		GTP_READ_COOR_ADDR & 0xFF, 0 };
	u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {GTP_READ_COOR_ADDR >> 8,
		GTP_READ_COOR_ADDR & 0xFF};
	u8 touch_num = 0;
	u8 finger = 0;
	static u8 pre_touch;
	static u8 pre_key;
#if defined(CONFIG_GTP_WITH_PEN)
	static u8 pre_pen;
#endif
	u8 key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i = 0;
	s32 ret = -1;

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	u8 rqst_data[3] = {(u8)(GTP_REG_RQST >> 8),
				 (u8)(GTP_REG_RQST & 0xFF), 0 };
#endif

#if defined(CONFIG_HOTKNOT_BLOCK_RW)
	u8 pxy_state = 0;
	u8 pxy_state_bak = 0;
	u8 hn_paired_cnt = 0;
	u8 state[10] = {(u8)(GTP_REG_HN_STATE >> 8),
			(u8)(GTP_REG_HN_STATE & 0xFF), 0 };
#endif

#if defined(CONFIG_TPD_PROXIMITY)
	s32 err = 0;
	hwm_sensor_data sensor_data;
	u8 proximity_status;
#endif

#if defined(CONFIG_GTP_SLIDE_WAKEUP)
	u8 doze_buf[3] = { 0x81, 0x4B };
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
#if defined(CONFIG_GTP_CHARGER_SWITCH)
		gtp_charger_switch(0);
#endif

#if defined(CONFIG_GTP_SLIDE_WAKEUP)
		if (DOZE_ENABLED == doze_status) {
			ret = gtp_i2c_read(i2c_client_tp_point, doze_buf, 3);
			GTP_DEBUG("0x814B = 0x%02X", doze_buf[2]);
			if (ret > 0) {
				if (0xAA == doze_buf[2]) {
					GTP_INFO("Forward slide up screen!");
					doze_status = DOZE_WAKEUP;
					input_report_key(tpd->dev,
							 KEY_POWER, 1);
					input_sync(tpd->dev);
					input_report_key(tpd->dev,
							 KEY_POWER, 0);
					input_sync(tpd->dev);
					/* clear 0x814B */
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_client_tp_point,
									doze_buf, 3);
				} else if (0xBB == doze_buf[2]) {
					GTP_INFO("Back slide up screen!");
					doze_status = DOZE_WAKEUP;
					input_report_key(tpd->dev,
							 KEY_POWER, 1);
					input_sync(tpd->dev);
					input_report_key(tpd->dev,
							 KEY_POWER, 0);
					input_sync(tpd->dev);
					/* clear 0x814B */
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_client_tp_point,
									doze_buf, 3);

				} else if (0xC0 == (doze_buf[2] & 0xC0)) {
					GTP_INFO("double click up screen!");
					doze_status = DOZE_WAKEUP;
					input_report_key(tpd->dev,
							 KEY_POWER, 1);
					input_sync(tpd->dev);
					input_report_key(tpd->dev,
							 KEY_POWER, 0);
					input_sync(tpd->dev);
					/* clear 0x814B */
					doze_buf[2] = 0x00;
					gtp_i2c_write(i2c_client_tp_point,
									doze_buf, 3);
				} else {
					gtp_enter_doze(i2c_client_tp_point);
				}
			}
			continue;
		}
#endif
	if (tpd_halt || (load_fw_process == 1)
		#if (defined(CONFIG_GTP_ESD_PROTECT) || defined(CONFIG_GTP_COMPATIBLE_MODE))
		|| (is_resetting == 1)
		#endif
		) {
			/* mutex_unlock(&i2c_access); */
			GTP_ERROR("return for interrupt after suspend...	");
			goto exit_work_func;
	}
	ret = gtp_i2c_read(i2c_client_tp_point, point_data, 12);
	if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
			goto exit_work_func;
	}
	finger = point_data[GTP_ADDR_LENGTH];

		#ifdef CONFIG_GTP_COMPATIBLE_MODE
	if ((finger == 0x00) && (CHIP_TYPE_GT9F == gtp_chip_type)) {
		ret = gtp_i2c_read(i2c_client_tp_point, rqst_data, 3);

		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
			goto exit_work_func;
		}
		switch (rqst_data[2]&0x0F) {
		case GTP_RQST_BAK_REF:
				GTP_INFO("Request Ref.");
				ret = gtp_bak_ref_proc(i2c_client_tp_point, GTP_BAK_REF_SEND);
				if (SUCCESS == ret) {
					GTP_INFO("Send ref success.");
					rqst_data[2] = GTP_RQST_RESPONDED;
					gtp_i2c_write(i2c_client_tp_point, rqst_data, 3);
				}
				goto exit_work_func;
		case GTP_RQST_CONFIG:
				GTP_INFO("Request Config.");
				ret = gtp_send_cfg(i2c_client_tp_point);
				if (ret < 0) {
					GTP_ERROR("Send config error.");
				}	else {
					GTP_INFO("Send config success.");
					rqst_data[2] = GTP_RQST_RESPONDED;
					gtp_i2c_write(i2c_client_tp_point, rqst_data, 3);
				}
				goto exit_work_func;
		case GTP_RQST_MAIN_CLOCK:
				GTP_INFO("Request main clock.");
				rqst_processing = 1;
				ret = gtp_main_clk_proc(i2c_client_tp_point);
				if (SUCCESS == ret) {
					GTP_INFO("Send main clk success.");
					rqst_data[2] = GTP_RQST_RESPONDED;
					gtp_i2c_write(i2c_client_tp_point, rqst_data, 3);
					rqst_processing = 0;
				}
				goto exit_work_func;
		case GTP_RQST_RESET:
				mutex_unlock(&i2c_access);
				GTP_INFO("Request Reset.");
				gtp_recovery_reset(i2c_client_tp_point);
				goto exit_work_func;
		case GTP_RQST_HOTKNOT_CODE:
					GTP_INFO("Request HotKnot Code.");
					gup_load_hotknot_system();
					goto exit_work_func;
		default:
				break;
		}
	}
		#endif

	if ((finger & 0x80) == 0) {
#ifdef CONFIG_HOTKNOT_BLOCK_RW
		if (!hotknot_paired_flag) {
#else
		{
#endif
				enable_irq(touch_irq);
				mutex_unlock(&i2c_access);
				GTP_INFO("buffer not ready");
				continue;
		}
	}

#ifdef CONFIG_HOTKNOT_BLOCK_RW
	if (!hotknot_paired_flag && (finger&0x0F)) {
		id = point_data[GTP_ADDR_LENGTH+1];
		hn_pxy_state = point_data[GTP_ADDR_LENGTH+2]&0x80;
		hn_pxy_state_bak = point_data[GTP_ADDR_LENGTH+3]&0x80;
		if ((32 == id) && (0x80 == hn_pxy_state) && (0x80 == hn_pxy_state_bak)) {
#ifdef HN_DBLCFM_PAIRED
			if (hn_paired_cnt++ < 2)
				goto exit_work_func;
#endif
			GTP_DEBUG("HotKnot paired!");
			if (wait_hotknot_state & HN_DEVICE_PAIRED) {
				GTP_DEBUG("INT wakeup HN_DEVICE_PAIRED block polling waiter");
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
		ret = gtp_i2c_read(i2c_client_tp_point, hn_state_buf, 6);

		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
			goto exit_work_func;
		}

		got_hotknot_state = 0;

		GTP_DEBUG("[0xAB10~0xAB13]=0x%x,0x%x,0x%x,0x%x", hn_state_buf[GTP_ADDR_LENGTH],
			 hn_state_buf[GTP_ADDR_LENGTH+1],
			 hn_state_buf[GTP_ADDR_LENGTH+2],
			 hn_state_buf[GTP_ADDR_LENGTH+3]);

		if (wait_hotknot_state & HN_MASTER_SEND) {
			if ((0x03 == hn_state_buf[GTP_ADDR_LENGTH]) ||
				(0x04 == hn_state_buf[GTP_ADDR_LENGTH]) ||
				(0x07 == hn_state_buf[GTP_ADDR_LENGTH])) {
				GTP_DEBUG("Wakeup HN_MASTER_SEND block polling waiter");
				got_hotknot_state |= HN_MASTER_SEND;
				got_hotknot_extra_state = hn_state_buf[GTP_ADDR_LENGTH];
				wake_up_interruptible(&bp_waiter);
			}
		} else if (wait_hotknot_state & HN_SLAVE_RECEIVED) {
			if ((0x03 == hn_state_buf[GTP_ADDR_LENGTH+1]) ||
				(0x04 == hn_state_buf[GTP_ADDR_LENGTH+1]) ||
				(0x07 == hn_state_buf[GTP_ADDR_LENGTH+1])) {
				GTP_DEBUG("Wakeup HN_SLAVE_RECEIVED block polling waiter:0x%x",
				hn_state_buf[GTP_ADDR_LENGTH+1]);
				got_hotknot_state |= HN_SLAVE_RECEIVED;
				got_hotknot_extra_state = hn_state_buf[GTP_ADDR_LENGTH+1];
				wake_up_interruptible(&bp_waiter);
			}
		} else if (wait_hotknot_state & HN_MASTER_DEPARTED) {
			if (0x07 == hn_state_buf[GTP_ADDR_LENGTH]) {
				GTP_DEBUG("Wakeup HN_MASTER_DEPARTED block polling waiter");
				got_hotknot_state |= HN_MASTER_DEPARTED;
				wake_up_interruptible(&bp_waiter);
			}
		} else if (wait_hotknot_state & HN_SLAVE_DEPARTED) {
			if (0x07 == hn_state_buf[GTP_ADDR_LENGTH+1]) {
				GTP_DEBUG("Wakeup HN_SLAVE_DEPARTED block polling waiter");
				got_hotknot_state |= HN_SLAVE_DEPARTED;
				wake_up_interruptible(&bp_waiter);
			}
		}
	}

#endif

#if defined(CONFIG_TPD_PROXIMITY)
		if (tpd_proximity_flag == 1) {
			proximity_status = point_data[GTP_ADDR_LENGTH];
			GTP_DEBUG("REG INDEX[0x814E]:0x%02X\n",
					proximity_status);

			if (proximity_status & 0x60) {
				tpd_proximity_detect = 0;
				/* sensor_data.values[0] = 0; */
			} else {
				tpd_proximity_detect = 1;
				/* sensor_data.values[0] = 1; */
			}

			/* get raw data */
			GTP_DEBUG(" ps change\n");
			GTP_DEBUG("PROXIMITY STATUS:0x%02X\n",
					tpd_proximity_detect);
			/* map and store data to hwm_sensor_data */
			sensor_data.values[0] = tpd_get_ps_value();
			sensor_data.value_divide = 1;
			sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
			/* report to the up-layer */
			ret = hwmsen_get_interrupt_data(ID_PROXIMITY,
							&sensor_data);

			if (ret)
				GTP_ERROR("Call hwmsen_get_interrupt_data fail = %d\n", err);
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
					(GTP_READ_COOR_ADDR + 10) & 0xff };

			ret = gtp_i2c_read(i2c_client_tp_point, buf,
						 2 + 8 * (touch_num - 1));
			memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
		}

		if (tpd_dts_data.use_tpd_button) {
			key_value = point_data[3 + 8 * touch_num];

			if (key_value || pre_key) {
				for (i = 0; i < TPD_KEY_COUNT; i++) {
					if (key_value & (0x01 << i)) {
						input_x = maping[i].x;
						input_y = maping[i].y;
						GTP_DEBUG("button =%d %d",
								input_x, input_y);
						tpd_down(input_x,
							 input_y,
							 0, 0);
					}
				}

				if ((pre_key != 0) && (key_value == 0))
					tpd_up(0, 0, 0);

				touch_num = 0;
				pre_touch = 0;
			}
		}
		pre_key = key_value;

		GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);

		if (touch_num) {
			for (i = 0; i < touch_num; i++) {
				coor_data = &point_data[i * 8 + 3];

				id = coor_data[0] & 0x0F;
				input_x = coor_data[1] | coor_data[2] << 8;
				input_y = coor_data[3] | coor_data[4] << 8;
				input_w = coor_data[5] | coor_data[6] << 8;

				input_x = TPD_WARP_X(abs_x_max, input_x);
				input_y = TPD_WARP_Y(abs_y_max, input_y);


#if defined(CONFIG_GTP_WITH_PEN)
				id = coor_data[0];
				if (id & 0x80) {
					GTP_DEBUG("Pen touch DOWN!");
					input_report_key(tpd->dev,
							 BTN_TOOL_PEN, 1);
					pre_pen = 1;
					id = 0;
				}
#endif
				GTP_DEBUG(" %d)(%d, %d)[%d]",
						id, input_x, input_y, input_w);
				tpd_down(input_x, input_y, input_w, id);
			}
		} else if (pre_touch) {
#if defined(CONFIG_GTP_WITH_PEN)
			if (pre_pen) {
				GTP_DEBUG("Pen touch UP!");
				input_report_key(tpd->dev, BTN_TOOL_PEN, 0);
				pre_pen = 0;
			}
#endif
			GTP_DEBUG("Touch Release!");
			tpd_up(0, 0, 0);
		} else {
			GTP_DEBUG("Additional Eint!");
		}
		pre_touch = touch_num;

		if (tpd != NULL && tpd->dev != NULL)
			input_sync(tpd->dev);

exit_work_func:

		if (!gtp_rawdiff_mode) {
			ret = gtp_i2c_write(i2c_client_tp_point, end_cmd, 3);

			if (ret < 0)
				GTP_INFO("I2C write end_cmd	error!");
		}
		/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
		enable_irq(touch_irq);

		mutex_unlock(&i2c_access);

	} while (!kthread_should_stop());

	return 0;
}

static int tpd_local_init(void)
{
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	if (IS_ERR(tpd->reg))
		GTP_ERROR("regulator_get() failed!\n");

#if defined(CONFIG_GTP_ESD_PROTECT)
	clk_tick_cnt = 2 * HZ;
	GTP_DEBUG("Clock ticks for an esd cycle: %d", clk_tick_cnt);
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	spin_lock_init(&esd_lock);	/* 2.6.39 & later */
	/* esd_lock = SPIN_LOCK_UNLOCKED;	 // 2.6.39 & before */
#endif

#if defined(CONFIG_GTP_SUPPORT_I2C_DMA)
#ifdef CONFIG_MTK_I2C_EXTENSION
		tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		gpDMABuf_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev,
		GTP_DMA_MAX_TRANSACTION_LENGTH, &gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va)
		GTP_INFO("[Error] Allocate DMA I2C Buffer failed!\n");

	memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
#else
	memset(I2CDMABuf, 0x00, sizeof(I2CDMABuf));
#endif
#endif
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		GTP_INFO("unable to add i2c driver.\n");
		return -1;
	}

	if (tpd_load_status == 0) {
		GTP_INFO("add error touch panel driver.\n");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID,
					 0, (GTP_MAX_TOUCH - 1), 0, 0);
	if (tpd_dts_data.use_tpd_button) {
		/*initialize tpd button data */
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

	GTP_INFO("end %s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

#if defined(CONFIG_GTP_SLIDE_WAKEUP)
static s8 gtp_enter_doze(struct i2c_client *client)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8),
				 (u8)GTP_REG_SLEEP, 8};

	GTP_DEBUG_FUNC();
#if defined(CONFIG_GTP_DBL_CLK_WAKEUP)
	i2c_control_buf[2] = 0x09;
#endif

	GTP_DEBUG("entering doze mode...");
	while (retry++ < 5) {
		i2c_control_buf[0] = 0x80;
		i2c_control_buf[1] = 0x46;
		ret = gtp_i2c_write(client, i2c_control_buf, 3);
		if (ret < 0) {
			GTP_DEBUG("failed to set doze flag into 0x8046, %d", retry);
			continue;
		}
		i2c_control_buf[0] = 0x80;
		i2c_control_buf[1] = 0x40;
		ret = gtp_i2c_write(client, i2c_control_buf, 3);
		if (ret > 0) {
			doze_status = DOZE_ENABLED;
			GTP_DEBUG("GTP has been working in doze mode!");
			return ret;
		}
		msleep(20);
	}
	GTP_ERROR("GTP send doze cmd failed.");
	return ret;
}

#else
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
	int ret = 0;

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		u8 i2c_status_buf[3] = { 0x80, 0x44, 0x00 };
		s32 ret = 0;

		ret = gtp_i2c_read(client, i2c_status_buf, 3);
		if (ret <= 0)
			GTP_ERROR("[gtp_enter_sleep]Read ref status reg error.");

		if (i2c_status_buf[2] & 0x80) {
			/* Store bak ref */
			ret = gtp_bak_ref_proc(client, GTP_BAK_REF_STORE);
			if (FAIL == ret)
				GTP_ERROR("[gtp_enter_sleep]Store bak ref failed.");
		}
	}
#endif

#if defined(CONFIG_GTP_POWER_CTRL_SLEEP)

	tpd_gpio_output(0, 0);
	tpd_gpio_output(1, 0);
	msleep(20);

#ifdef TPD_POWER_SOURCE_1800
	hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
#endif

	ret = regulator_disable(tpd->reg);	/* disable regulator */
	if (ret)
		GTP_DEBUG("regulator_disable() failed!\n");

	GTP_INFO("GTP enter sleep by poweroff!");
	return 0;

#else
	{
		s8 ret = -1;
		s8 retry = 0;
		u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8),
					 (u8)GTP_REG_SLEEP, 5};

		tpd_gpio_output(1, 0);
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
#endif

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

	GTP_DEBUG("GTP wakeup begin.");

#if defined(CONFIG_GTP_POWER_CTRL_SLEEP)

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
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

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		u8 opr_buf[2] = { 0 };

		while (retry++ < 10) {
			tpd_gpio_output(1, 1);
			msleep(20);

			ret = gtp_i2c_test(client);

			if (ret >= 0) {
				/* Hold ss51 & dsp */
				opr_buf[0] = 0x0C;
				ret = i2c_write_bytes(client,
									0x4180,
									opr_buf,
									1);
				if (ret < 0) {
					GTP_DEBUG("I2C error,retry:%d", retry);
					continue;
				}
				/* Confirm hold */
				opr_buf[0] = 0x00;
				ret = i2c_read_bytes(client,
								 0x4180,
								 opr_buf,
								 1);
				if (ret < 0) {
					GTP_DEBUG("I2C error,retry:%d",
							retry);
					continue;
				}
				if (0x0C != opr_buf[0]) {
					GTP_DEBUG("val: %d, retry: %d",
							opr_buf[0], retry);
					continue;
				}
				GTP_DEBUG("ss51 & dsp has been hold");

				ret = gtp_fw_startup(client);
				if (FAIL == ret) {
					GTP_ERROR("Startup fw failed.");
					continue;
				}
				GTP_INFO("flashless wakeup sleep success");
				return ret;
			}
			force_reset_guitar();
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
#if defined(CONFIG_GTP_SLIDE_WAKEUP)
		if (DOZE_WAKEUP != doze_status) {
			GTP_DEBUG("power wakeup, reset guitar");
			doze_status = DOZE_DISABLED;

			/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
			disable_irq(touch_irq);
			gtp_reset_guitar(client, 20);
			/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
			enable_irq(touch_irq);
		} else {
			GTP_DEBUG("slide(double click) wakeup, no reset guitar");
			doze_status = DOZE_DISABLED;
#if defined(CONFIG_GTP_ESD_PROTECT)
			gtp_init_ext_watchdog(client);
#endif
		}
#else
/* if (chip_gt9xxs == 1) */
/* { */
/* gtp_reset_guitar(client, 10); */
/* } */
/* else */
/* { */
/* gpio_direction_output(tpd_int_gpio_number, 1); */
/* msleep(5); */
/* } */

		/* gpio_direction_input(tpd_int_gpio_number); */
		gtp_reset_guitar(client, 20);

		return 2;
#endif

		ret = gtp_i2c_test(client);

		if (ret >= 0) {
			GTP_INFO("GTP wakeup sleep.");
#if (!defined(CONFIG_GTP_SLIDE_WAKEUP))
			if (chip_gt9xxs == 0) {
				gtp_int_sync(25);
#if defined(CONFIG_GTP_ESD_PROTECT)
				gtp_init_ext_watchdog(client);
#endif
			}
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
	u8 buf[3] = { 0x81, 0xaa, 0 };

#if defined(CONFIG_TPD_PROXIMITY)
	if (tpd_proximity_flag == 1)
		return;
#endif

#if defined(CONFIG_HOTKNOT_BLOCK_RW)
	if (hotknot_paired_flag)
		return;
#endif

	mutex_lock(&tui_lock);
	if (tpd_tui_flag) {
		GTP_INFO("[TPD] skip tpd_suspend due to TUI in used\n");
		tpd_tui_low_power_skipped = 1;
		mutex_unlock(&tui_lock);
		return;
	}
	mutex_unlock(&tui_lock);

	mutex_lock(&i2c_access);

	gtp_i2c_read(i2c_client_tp_point, buf, sizeof(buf));
	if (buf[2] == 0x55) {
		mutex_unlock(&i2c_access);
		GTP_INFO("GTP early suspend	pair success");
		return;
	}
	tpd_halt = 1;

	mutex_unlock(&i2c_access);
#if defined(CONFIG_GTP_ESD_PROTECT)
	cancel_delayed_work_sync(&gtp_esd_check_work);
#endif

#if defined(CONFIG_GTP_CHARGER_DETECT)
	cancel_delayed_work_sync(&gtp_charger_check_work);
#endif
	mutex_lock(&i2c_access);

#if defined(CONFIG_GTP_SLIDE_WAKEUP)
	ret = gtp_enter_doze(i2c_client_tp_point);
#else
	/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);
	ret = gtp_enter_sleep(i2c_client_tp_point);
	if (ret < 0)
		GTP_ERROR("GTP early suspend failed.");
#endif
	mutex_unlock(&i2c_access);

	msleep(58);
}

/* Function to manage power-on resume */
static void tpd_resume(struct device *h)
{
	s32 ret = -1;

	GTP_DEBUG("mtk-tpd: %s start\n", __func__);
#if defined(CONFIG_TPD_PROXIMITY)

	if (tpd_proximity_flag == 1)
		return;
#endif
#if defined(CONFIG_HOTKNOT_BLOCK_RW)
	if (hotknot_paired_flag)
		return;
#endif
	if (load_fw_process == 0) {
		ret = gtp_wakeup_sleep(i2c_client_tp_point);

		if (ret < 0)
			GTP_ERROR("GTP later resume failed.");
	}

#if defined(CONFIG_GTP_CHARGER_SWITCH)
	if (g_bat_init_flag)
		gtp_charger_switch(1);	/* force update */
#endif

#if defined(CONFIG_GTP_SLIDE_WAKEUP)
	doze_status = DOZE_DISABLED;
#else
	mutex_lock(&i2c_access);
	tpd_halt = 0;
	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
	mutex_unlock(&i2c_access);
#endif

#if defined(CONFIG_GTP_ESD_PROTECT)
	queue_delayed_work(gtp_esd_check_workqueue,
				 &gtp_esd_check_work, clk_tick_cnt);
#endif

#if defined(CONFIG_GTP_CHARGER_DETECT)
	queue_delayed_work(gtp_charger_check_workqueue,
				 &gtp_charger_check_work, clk_tick_cnt);
#endif
	GTP_DEBUG("mtk-tpd: %s end\n", __func__);
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gt9xx",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static void tpd_off(void)
{
	int ret = 0;

	ret = regulator_disable(tpd->reg);	/* disable regulator */
	if (ret)
		GTP_DEBUG("regulator_disable() failed!\n");
#ifdef TPD_POWER_SOURCE_1800
	hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
#endif
	GTP_INFO("GTP enter sleep!");

	tpd_halt = 1;
	/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);
}

static void tpd_on(void)
{
	s32 ret = -1, retry = 0;

	while (retry++ < 5) {
		ret = tpd_power_on(i2c_client_tp_point);

		if (ret < 0)
			GTP_ERROR("I2C Power on ERROR!");

		ret = gtp_send_cfg(i2c_client_tp_point);

		if (ret > 0)
			GTP_DEBUG("Wakeup sleep send config success.");
	}
	if (ret < 0)
		GTP_ERROR("GTP later resume failed.");
	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
	tpd_halt = 0;
}

int tpd_enter_tui(void)
{
	int ret = 0;

	tpd_tui_flag = 1;
	GTP_INFO("[%s] enter TUI", __func__);
	return ret;
}

int tpd_exit_tui(void)
{
	int ret = 0;

	GTP_INFO("[%s] exit TUI+", __func__);
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

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
	GTP_INFO("MediaTek gt91xx touch panel driver init\n");
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		GTP_INFO("add generic driver failed\n");

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	GTP_INFO("MediaTek gt91xx touch panel driver exit\n");
	tpd_driver_remove(&tpd_device_driver);
}
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

