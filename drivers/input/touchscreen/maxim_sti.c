/* drivers/input/touchscreen/maxim_sti.c
 *
 * Maxim SmartTouch Imager Touchscreen Driver
 *
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kmod.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/firmware.h>
#include <linux/crc16.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <net/genetlink.h>
#include <net/sock.h>
#include <uapi/linux/maxim_sti.h>
#include <asm/byteorder.h>  /* MUST include this header to get byte order */
#ifdef CONFIG_OF
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#endif

/****************************************************************************\
* Custom features                                                            *
\****************************************************************************/

#define VTG_DVDD_MIN_UV		1800000
#define VTG_DVDD_MAX_UV		1800000
#define VTG_AVDD_MIN_UV		2850000
#define VTG_AVDD_MAX_UV		2850000
#define INPUT_DEVICES		1
#define CFG_FILE_NAME_MAX	64

#define BUF_SIZE		4100
#define DEF_NL_MC_GROUPS	5
#define DEF_CHIP_ACC_METHOD	1
#define TOUCH_FUSION		"touch_fusion"

#if defined(CONFIG_FB)
#include <linux/fb.h>
#endif

/****************************************************************************\
* Device context structure, globals, and macros                              *
\****************************************************************************/
#define MAXIM_STI_NAME  "maxim_sti"

struct maxim_sti_pdata {
	char      *touch_fusion;
	char      *config_file;
	char      *nl_family;
	char      *fw_name;
	u32       nl_mc_groups;
	u32       chip_access_method;
	u32       default_reset_state;
	u32       tx_buf_size;
	u32       rx_buf_size;
	int       gpio_reset;
	int       gpio_irq;
	int       (*init)(struct maxim_sti_pdata *pdata, bool init);
	void      (*reset)(struct maxim_sti_pdata *pdata, int value);
	int       (*irq)(struct maxim_sti_pdata *pdata);
};

struct dev_data;

struct chip_access_method {
	int (*read)(struct dev_data *dd, u16 address, u8 *buf, u16 len);
	int (*write)(struct dev_data *dd, u16 address, u8 *buf, u16 len);
};

struct dev_data {
	u8                           *tx_buf;
	u8                           *rx_buf;
	u8                           send_fail_count;
	u32                          nl_seq;
	u8                           nl_mc_group_count;
	bool                         nl_enabled;
	bool                         start_fusion;
	bool                         suspended;
	bool                         suspend_in_progress;
	bool                         resume_in_progress;
	bool                         expect_resume_ack;
	bool                         eraser_active;
	bool                         legacy_acceleration;
	bool                         input_no_deconfig;
	bool                         irq_registered;
	u16                          irq_param[MAX_IRQ_PARAMS];
	pid_t                        fusion_process;
	char                         input_phys[128];
	struct input_dev             *input_dev[INPUT_DEVICES];
	struct completion            suspend_resume;
	struct chip_access_method    chip;
	struct spi_device            *spi;
	struct genl_family           nl_family;
	struct genl_ops              *nl_ops;
	struct genl_multicast_group  *nl_mc_groups;
	struct sk_buff               *outgoing_skb;
	struct sk_buff_head          incoming_skb_queue;
	struct task_struct           *thread;
	struct sched_param           thread_sched;
	struct list_head             dev_list;
	struct regulator             *reg_avdd;
	struct regulator             *reg_dvdd;
	bool                         supply;
	void                         (*service_irq)(struct dev_data *dd);
	struct notifier_block        fb_notifier;
	struct kobject               *parent;
	char                         config_file[CFG_FILE_NAME_MAX];
	u8                           sysfs_created;
	u16                          chip_id;
	u16                          glove_enabled;
	u16                          charger_mode_en;
	u16                          lcd_fps;
	u16                          tf_ver;
	u16                          drv_ver;
	u16                          fw_ver;
	u16            	             fw_protocol;
	u32                          tf_status;
	bool                         idle;
	bool                         gesture_reset;
	u16                          gesture_en;
	unsigned long int            sysfs_update_type;
	struct completion            sysfs_ack_glove;
	struct completion            sysfs_ack_charger;
	struct completion            sysfs_ack_lcd_fps;
	struct mutex                 sysfs_update_mutex;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspended;
	struct pinctrl *ts_pinctrl;
};

static unsigned short    panel_id;
static struct list_head  dev_list;
static spinlock_t        dev_lock;

static irqreturn_t irq_handler(int irq, void *context);
static void service_irq(struct dev_data *dd);
static void service_irq_legacy_acceleration(struct dev_data *dd);
static int send_sysfs_info(struct dev_data *dd);

#define ERROR(a, b...) dev_err(&dd->spi->dev, "maxim: %s (ERROR:%s:%d): " \
				a "\n", dd->nl_family.name, __func__, \
				__LINE__, ##b)
#define INFO(a, b...) dev_info(&dd->spi->dev, "maxim: %s: " a "\n", \
			     dd->nl_family.name, ##b)
#define DBG(a, b...) dev_dbg(&dd->spi->dev, "maxim: %s: " a "\n", \
			     dd->nl_family.name, ##b)

#define DRV_MSG_DBG(...)
#define DRV_PT_DBG(...)

/****************************************************************************\
* Chip access methods                                                        *
\****************************************************************************/

static int
spi_read_123(struct dev_data *dd, u16 address, u8 *buf, u16 len, bool add_len)
{
	struct spi_message   message;
	struct spi_transfer  transfer;
	u16                  *tx_buf = (u16 *)dd->tx_buf;
	u16                  *rx_buf = (u16 *)dd->rx_buf;
	u16                  words = len / sizeof(u16), header_len = 1;
	u16                  *ptr2 = rx_buf + 1;
#ifdef __LITTLE_ENDIAN
	u16                  *ptr1 = (u16 *)buf, i;
#endif
	int                  ret;

	if (tx_buf == NULL || rx_buf == NULL)
		return -ENOMEM;

	tx_buf[0] = (address << 1) | 0x0001;
#ifdef __LITTLE_ENDIAN
	tx_buf[0] = (tx_buf[0] << 8) | (tx_buf[0] >> 8);
#endif

	if (add_len) {
		tx_buf[1] = words;
#ifdef __LITTLE_ENDIAN
		tx_buf[1] = (tx_buf[1] << 8) | (tx_buf[1] >> 8);
#endif
		ptr2++;
		header_len++;
	}

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	transfer.len = len + header_len * sizeof(u16);
	transfer.tx_buf = tx_buf;
	transfer.rx_buf = rx_buf;
	spi_message_add_tail(&transfer, &message);

	do {
		ret = spi_sync(dd->spi, &message);
	} while (ret == -EAGAIN);

#ifdef __LITTLE_ENDIAN
	for (i = 0; i < words; i++)
		ptr1[i] = (ptr2[i] << 8) | (ptr2[i] >> 8);
#else
	memcpy(buf, ptr2, len);
#endif

	return ret;
}

static int
spi_write_123(struct dev_data *dd, u16 address, u8 *buf, u16 len,
	      bool add_len)
{
	struct maxim_sti_pdata  *pdata = dd->spi->dev.platform_data;
	u16                     *tx_buf = (u16 *)dd->tx_buf;
	u16                     words = len / sizeof(u16), header_len = 1;
#ifdef __LITTLE_ENDIAN
	u16                     i;
#endif
	int  ret;

	if (tx_buf == NULL)
		return -ENOMEM;

	tx_buf[0] = address << 1;
	if (add_len) {
		tx_buf[1] = words;
		header_len++;
	}
	memcpy(tx_buf + header_len, buf, len);
#ifdef __LITTLE_ENDIAN
	for (i = 0; i < (words + header_len); i++)
		tx_buf[i] = (tx_buf[i] << 8) | (tx_buf[i] >> 8);
#endif

	do {
		ret = spi_write(dd->spi, tx_buf,
				len + header_len * sizeof(u16));
	} while (ret == -EAGAIN);

	memset(dd->tx_buf, 0xFF, pdata->tx_buf_size);
	return ret;
}

/* ======================================================================== */

static int
spi_read_1(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_read_123(dd, address, buf, len, true);
}

static int
spi_write_1(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_write_123(dd, address, buf, len, true);
}

/* ======================================================================== */

static inline int
stop_legacy_acceleration(struct dev_data *dd)
{
	u16  value = 0xDEAD, status, i;
	int  ret;

	ret = spi_write_123(dd, 0x0003, (u8 *)&value,
				sizeof(value), false);
	if (ret < 0)
		return -1;
	usleep_range(100, 120);

	for (i = 0; i < 200; i++) {
		ret = spi_read_123(dd, 0x0003, (u8 *)&status, sizeof(status),
				   false);
		if (ret < 0)
			return -1;
		if (status == 0xABCD)
			return 0;
	}

	return -2;
}

static inline int
start_legacy_acceleration(struct dev_data *dd)
{
	u16  value = 0xBEEF;
	int  ret;

	ret = spi_write_123(dd, 0x0003, (u8 *)&value, sizeof(value), false);
	usleep_range(100, 120);

	return ret;
}

static inline int
spi_rw_2_poll_status(struct dev_data *dd)
{
	u16  status, i;
	int  ret;

	for (i = 0; i < 200; i++) {
		ret = spi_read_123(dd, 0x0000, (u8 *)&status, sizeof(status),
				   false);
		if (ret < 0)
			return -1;
		if (status == 0xABCD)
			return 0;
	}

	return -2;
}

static inline int
spi_read_2_page(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	u16  request[] = {0xFEDC, (address << 1) | 0x0001, len / sizeof(u16)};
	int  ret;

	/* write read request header */
	ret = spi_write_123(dd, 0x0000, (u8 *)request, sizeof(request),
			    false);
	if (ret < 0)
		return -1;

	/* poll status */
	ret = spi_rw_2_poll_status(dd);
	if (ret < 0)
		return ret;

	/* read data */
	ret = spi_read_123(dd, 0x0004, (u8 *)buf, len, false);
	return ret;
}

static inline int
spi_write_2_page(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	u16  page[254];
	int  ret;

	page[0] = 0xFEDC;
	page[1] = address << 1;
	page[2] = len / sizeof(u16);
	page[3] = 0x0000;
	memcpy(page + 4, buf, len);

	/* write data with write request header */
	ret = spi_write_123(dd, 0x0000, (u8 *)page, len + 4 * sizeof(u16),
			    false);
	if (ret < 0)
		return -1;

	/* poll status */
	return spi_rw_2_poll_status(dd);
}

static inline int
spi_rw_2(struct dev_data *dd, u16 address, u8 *buf, u16 len,
	 int (*func)(struct dev_data *dd, u16 address, u8 *buf, u16 len))
{
	u16  rx_len, rx_limit = 250 * sizeof(u16), offset = 0;
	int  ret;

	while (len > 0) {
		rx_len = (len > rx_limit) ? rx_limit : len;
		if (dd->legacy_acceleration)
			stop_legacy_acceleration(dd);
		ret = func(dd, address + (offset / sizeof(u16)), buf + offset,
			   rx_len);
		if (dd->legacy_acceleration)
			start_legacy_acceleration(dd);
		if (ret < 0)
			return ret;
		offset += rx_len;
		len -= rx_len;
	}

	return 0;
}

static int
spi_read_2(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_rw_2(dd, address, buf, len, spi_read_2_page);
}

static int
spi_write_2(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_rw_2(dd, address, buf, len, spi_write_2_page);
}

/* ======================================================================== */

static int
spi_read_3(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_read_123(dd, address, buf, len, false);
}

static int
spi_write_3(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_write_123(dd, address, buf, len, false);
}

/* ======================================================================== */

static struct chip_access_method chip_access_methods[] = {
	{
		.read = spi_read_1,
		.write = spi_write_1,
	},
	{
		.read = spi_read_2,
		.write = spi_write_2,
	},
	{
		.read = spi_read_3,
		.write = spi_write_3,
	},
};

static int
set_chip_access_method(struct dev_data *dd, u8 method)
{
	if (method == 0 || method > ARRAY_SIZE(chip_access_methods))
		return -1;

	memcpy(&dd->chip, &chip_access_methods[method - 1], sizeof(dd->chip));
	return 0;
}

/* ======================================================================== */

static inline int
stop_legacy_acceleration_canned(struct dev_data *dd)
{
	u16  value = dd->irq_param[18];

	return dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
			      sizeof(value));
}

static inline int
start_legacy_acceleration_canned(struct dev_data *dd)
{
	u16  value = dd->irq_param[17];

	return dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
			      sizeof(value));
}

/* ======================================================================== */

#define FLASH_BLOCK_SIZE  64      /* flash write buffer in words */
#define FIRMWARE_SIZE     0xC000  /* fixed 48Kbytes */

static int bootloader_wait_ready(struct dev_data *dd)
{
	u16  status, i;

	for (i = 0; i < 15; i++) {
		if (spi_read_3(dd, 0x00FF, (u8 *)&status,
			       sizeof(status)) != 0)
			return -1;
		if (status == 0xABCC)
			return 0;
		if (i >= 3)
			usleep_range(500, 700);
	}
	ERROR("unexpected status %04X", status);
	return -1;
}

static int bootloader_complete(struct dev_data *dd)
{
	u16  value = 0x5432;

	return spi_write_3(dd, 0x00FF, (u8 *)&value, sizeof(value));
}

static int bootloader_read_data(struct dev_data *dd, u16 *value)
{
	u16  buffer[2];

	if (spi_read_3(dd, 0x00FE, (u8 *)buffer, sizeof(buffer)) != 0)
		return -1;
	if (buffer[1] != 0xABCC)
		return -1;

	*value = buffer[0];
	return bootloader_complete(dd);
}

static int bootloader_write_data(struct dev_data *dd, u16 value)
{
	u16  buffer[2] = {value, 0x5432};

	if (bootloader_wait_ready(dd) != 0)
		return -1;
	return spi_write_3(dd, 0x00FE, (u8 *)buffer, sizeof(buffer));
}

static int bootloader_wait_command(struct dev_data *dd)
{
	u16  value, i;

	for (i = 0; i < 15; i++) {
		if (bootloader_read_data(dd, &value) == 0 && value == 0x003E)
			return 0;
		if (i >= 3)
			usleep_range(500, 700);
	}
	return -1;
}

static int bootloader_enter(struct dev_data *dd)
{
	int i;
	u16 enter[3] = {0x0047, 0x00C7, 0x0007};

	for (i = 0; i < 3; i++) {
		if (spi_write_3(dd, 0x7F00, (u8 *)&enter[i],
				sizeof(enter[i])) != 0)
			return -1;
	}

	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int bootloader_exit(struct dev_data *dd)
{
	u16  value = 0x0000;

	if (bootloader_write_data(dd, 0x0001) != 0)
		return -1;
	return spi_write_3(dd, 0x7F00, (u8 *)&value, sizeof(value));
}

static int bootloader_get_crc(struct dev_data *dd, u16 *crc16, u16 len)
{
	u16 command[] = {0x0030, 0x0002, 0x0000, 0x0000, len & 0xFF,
			len >> 8}, value[2], i;

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (bootloader_write_data(dd, command[i]) != 0)
			return -1;
	msleep(200); /* wait 200ms for it to get done */

	for (i = 0; i < 2; i++)
		if (bootloader_read_data(dd, &value[i]) != 0)
			return -1;

	if (bootloader_wait_command(dd) != 0)
		return -1;
	*crc16 = (value[1] << 8) | value[0];
	return 0;
}

static int bootloader_set_byte_mode(struct dev_data *dd)
{
	u16  command[2] = {0x000A, 0x0000}, i;

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (bootloader_write_data(dd, command[i]) != 0)
			return -1;
	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int bootloader_erase_flash(struct dev_data *dd)
{
	if (bootloader_write_data(dd, 0x0002) != 0)
		return -1;
	msleep(60); /* wait 60ms */
	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int bootloader_write_flash(struct dev_data *dd, u16 *image, u16 len)
{
	u16  command[] = {0x00F0, 0x0000, len >> 8, 0x0000, 0x0000};
	u16  i, buffer[FLASH_BLOCK_SIZE];

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (bootloader_write_data(dd, command[i]) != 0)
			return -1;

	for (i = 0; i < ((len / sizeof(u16)) / FLASH_BLOCK_SIZE); i++) {
		if (bootloader_wait_ready(dd) != 0)
			return -1;
		memcpy(buffer, (void *)(image + i * FLASH_BLOCK_SIZE),
			sizeof(buffer));
		if (spi_write_3(dd, ((i % 2) == 0) ? 0x0000 : 0x0040,
				(u8 *)buffer, sizeof(buffer)) != 0)
			return -1;
		if (bootloader_complete(dd) != 0)
			return -1;
	}

	usleep_range(10000, 11000);
	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int device_fw_load(struct dev_data *dd, const struct firmware *fw)
{
	u16  fw_crc16, chip_crc16;

	fw_crc16 = crc16(0, fw->data, fw->size);
	INFO("firmware size (%d) CRC16(0x%04X)", (int)fw->size, fw_crc16);
	if (bootloader_enter(dd) != 0) {
		ERROR("failed to enter bootloader");
		return -1;
	}
	if (bootloader_get_crc(dd, &chip_crc16, fw->size) != 0) {
		ERROR("failed to get CRC16 from the chip");
		return -1;
	}
	INFO("chip CRC16(0x%04X)", chip_crc16);
	if (fw_crc16 != chip_crc16) {
		INFO("will reprogram chip");
		if (bootloader_erase_flash(dd) != 0) {
			ERROR("failed to erase chip flash");
			return -1;
		}
		INFO("flash erase OK");
		if (bootloader_set_byte_mode(dd) != 0) {
			ERROR("failed to set byte mode");
			return -1;
		}
		INFO("byte mode OK");
		if (bootloader_write_flash(dd, (u16 *)fw->data,
							fw->size) != 0) {
			ERROR("failed to write flash");
			return -1;
		}
		INFO("flash write OK");
		if (bootloader_get_crc(dd, &chip_crc16, fw->size) != 0) {
			ERROR("failed to get CRC16 from the chip");
			return -1;
		}
		if (fw_crc16 != chip_crc16) {
			ERROR("failed to verify programming! (0x%04X)",
			      chip_crc16);
			return -1;
		}
		INFO("chip programmed successfully, new chip CRC16(0x%04X)",
			chip_crc16);
	}
	if (bootloader_exit(dd) != 0) {
		ERROR("failed to exit bootloader");
		return -1;
	}
	return 0;
}

static int fw_request_load(struct dev_data *dd)
{
	const struct firmware *fw;
	int  ret;

	ret = request_firmware(&fw, "maxim_fp35.bin", &dd->spi->dev);
	if (ret || fw == NULL) {
		ERROR("firmware request failed (%d,%p)", ret, fw);
		return -1;
	}
	if (fw->size != FIRMWARE_SIZE) {
		release_firmware(fw);
		ERROR("incoming firmware is of wrong size (%04X)",
						(unsigned int)fw->size);
		return -1;
	}
	ret = device_fw_load(dd, fw);
	if (ret != 0 && bootloader_exit(dd) != 0)
		ERROR("failed to exit bootloader");
	release_firmware(fw);
	return ret;
}

/* ======================================================================== */

static void stop_idle_scan(struct dev_data *dd)
{
	u16 value;

	value = dd->irq_param[13];
	(void)dd->chip.write(dd, dd->irq_param[19],
				(u8 *)&value, sizeof(value));
}

static void stop_scan_canned(struct dev_data *dd)
{
	u16  value;
	u16  i, clear[2] = { 0 };

	if (dd->legacy_acceleration)
		(void)stop_legacy_acceleration_canned(dd);
	value = dd->irq_param[13];
	(void)dd->chip.write(dd, dd->irq_param[12], (u8 *)&value,
			     sizeof(value));
	usleep_range(dd->irq_param[15], dd->irq_param[15] + 1000);
	(void)dd->chip.read(dd, dd->irq_param[0], (u8 *)&clear[0],
			    sizeof(clear[0]));
	(void)dd->chip.write(dd, dd->irq_param[0], (u8 *)&clear,
			     sizeof(clear));

	for (i = 28; i < 32; i++) {
		value = dd->irq_param[i];
		(void)dd->chip.write(dd, dd->irq_param[27],
				(u8 *)&value, sizeof(value));
	}
	value = dd->irq_param[33];
	(void)dd->chip.write(dd, dd->irq_param[32],
				(u8 *)&value, sizeof(value));
	usleep_range(500, 1000);
	for (i = 28; i < 32; i++) {
		value = dd->irq_param[i];
		(void)dd->chip.write(dd, dd->irq_param[27],
				(u8 *)&value, sizeof(value));
	}
	value = dd->irq_param[13];
	(void)dd->chip.write(dd, dd->irq_param[32],
				(u8 *)&value, sizeof(value));
}

static void start_scan_canned(struct dev_data *dd)
{
	u16  value;

	if (dd->legacy_acceleration) {
		(void)start_legacy_acceleration_canned(dd);
	} else {
		value = dd->irq_param[14];
		(void)dd->chip.write(dd, dd->irq_param[12], (u8 *)&value,
				     sizeof(value));
	}
}

static void enable_gesture(struct dev_data *dd)
{
	u16  value;

	if (!dd->idle)
		stop_scan_canned(dd);

	value = dd->irq_param[13];
	(void)dd->chip.write(dd, dd->irq_param[34],
			     (u8 *)&value, sizeof(value));
	value = dd->irq_param[36];
	(void)dd->chip.write(dd, dd->irq_param[35],
			     (u8 *)&value, sizeof(value));

	value = dd->irq_param[18];
	(void)dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
			     sizeof(value));

	value = dd->irq_param[20];
	(void)dd->chip.write(dd, dd->irq_param[19], (u8 *)&value,
			     sizeof(value));
	if (!dd->idle) {
		value = dd->irq_param[26];
		(void)dd->chip.write(dd, dd->irq_param[25], (u8 *)&value,
				     sizeof(value));
	}
}

static void gesture_wake_detect(struct dev_data *dd)
{
	u16    status, value;
	int    ret = 0;

	ret = dd->chip.read(dd, dd->irq_param[0],
			    (u8 *)&status, sizeof(status));
	INFO("%s: %d = %04X", __func__, dd->irq_param[0], status);
	if (status & dd->irq_param[10])
		dd->gesture_reset = true;
	if (status & dd->irq_param[18] && dd->input_dev[INPUT_DEVICES - 1]) {
		ret = dd->chip.read(dd, dd->irq_param[22],
				    (u8 *)&value, sizeof(value));
		INFO("gesture code = 0x%04X", value);
		if (value == dd->irq_param[24]) {
			INFO("gesture detected");
			input_report_key(dd->input_dev[INPUT_DEVICES - 1], KEY_POWER, 1);
			input_sync(dd->input_dev[INPUT_DEVICES - 1]);
			input_report_key(dd->input_dev[INPUT_DEVICES - 1], KEY_POWER, 0);
			input_sync(dd->input_dev[INPUT_DEVICES - 1]);
		}
		value = dd->irq_param[18];
		ret = dd->chip.write(dd, dd->irq_param[0],
				     (u8 *)&value, sizeof(value));
	} else {
		ret = dd->chip.write(dd, dd->irq_param[0],
				     (u8 *)&status, sizeof(status));
	}
}

static void finish_gesture(struct dev_data *dd)
{
	struct maxim_sti_pdata *pdata = dd->spi->dev.platform_data;
	u16 value;
	u8  fail_count = 0;
	int ret;

	value = dd->irq_param[21];
	(void)dd->chip.write(dd, dd->irq_param[19], (u8 *)&value,
			     sizeof(value));
	do {
		msleep(20);
		ret = dd->chip.read(dd, dd->irq_param[25], (u8 *)&value,
				    sizeof(value));
		if (ret < 0 || fail_count >= 20) {
			ERROR("failed to read control register (%d)", ret);
			pdata->reset(pdata, 0);
			msleep(20);
			pdata->reset(pdata, 1);
			return;
		}
		INFO("ctrl = 0x%04X", value);
		fail_count++;
	} while (value);
}

static int regulator_init(struct dev_data *dd)
{
	int ret;

	dd->reg_avdd = devm_regulator_get(&dd->spi->dev, "avdd");
	if (IS_ERR(dd->reg_avdd)) {
		ret = PTR_ERR(dd->reg_avdd);
		dd->reg_avdd = NULL;
		dev_err(&dd->spi->dev, "can't find avdd regulator: %d\n",
				ret);
		return ret;
	}

	ret = regulator_set_voltage(dd->reg_avdd,
			VTG_AVDD_MIN_UV, VTG_AVDD_MAX_UV);
	if (ret) {
		dev_err(&dd->spi->dev,
			"can't set avdd regulator voltage: %d\n", ret);
		goto err_free_avdd;
	}

	dd->reg_dvdd = devm_regulator_get(&dd->spi->dev, "dvdd");
	if (IS_ERR(dd->reg_dvdd)) {
		ret = PTR_ERR(dd->reg_avdd);
		dd->reg_dvdd = NULL;
		dev_err(&dd->spi->dev, "can't find avdd regulator: %d\n",
				ret);
		goto err_free_avdd;
	}

	ret = regulator_set_voltage(dd->reg_dvdd,
			VTG_DVDD_MIN_UV, VTG_DVDD_MAX_UV);
	if (ret) {
		dev_err(&dd->spi->dev,
			"can't set dvdd regulator voltage: %d\n", ret);
		goto err_free_dvdd;
	}
	return 0;

err_free_dvdd:
	devm_regulator_put(dd->reg_dvdd);
	dd->reg_dvdd = NULL;
err_free_avdd:
	devm_regulator_put(dd->reg_avdd);
	dd->reg_avdd = NULL;
	return ret;
}

static void regulator_uninit(struct dev_data *dd)
{
	if (dd->reg_dvdd)
		devm_regulator_put(dd->reg_dvdd);
	if (dd->reg_avdd)
		devm_regulator_put(dd->reg_avdd);
}

static int regulator_control(struct dev_data *dd, bool on)
{
	int ret;

	if (!dd->reg_avdd || !dd->reg_dvdd)
		return 0;

	if (on && !dd->supply) {
		DBG("regulator ON.");
		ret = regulator_enable(dd->reg_dvdd);
		if (ret < 0) {
			ERROR("failed to enable dvdd regulator: %d", ret);
			return ret;
		}
		usleep_range(1000, 1020);

		ret = regulator_enable(dd->reg_avdd);
		if (ret < 0) {
			ERROR("failed to enable avdd regulator: %d", ret);
			regulator_disable(dd->reg_dvdd);
			return ret;
		}

		dd->supply = true;
	} else if (!on && dd->supply) {
		DBG("regulator OFF.");

		ret = regulator_disable(dd->reg_avdd);
		if (ret < 0)
			ERROR("Failed to disable regulator avdd: %d", ret);

		ret = regulator_disable(dd->reg_dvdd);
		if (ret < 0)
			ERROR("Failed to disable regulator dvdd: %d", ret);

		dd->supply = false;
	}

	return 0;
}

#define MAXIM_STI_GPIO_ERROR(ret, gpio, op)                                \
	if (ret < 0) {                                                     \
		pr_err("%s: GPIO %d %s failed (%d)\n", __func__, gpio, op, \
			ret);                                              \
		return ret;						   \
	}


int maxim_sti_gpio_init(struct maxim_sti_pdata *pdata, bool init)
{
	int  ret;

	if (init) {
		ret = gpio_request(pdata->gpio_irq, "maxim_sti_irq");
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_irq, "request");
		ret = gpio_direction_input(pdata->gpio_irq);
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_irq, "direction");
		ret = gpio_request(pdata->gpio_reset, "maxim_sti_reset");
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_reset, "request");
		ret = gpio_direction_output(pdata->gpio_reset,
					    pdata->default_reset_state);
		MAXIM_STI_GPIO_ERROR(ret, pdata->gpio_reset, "direction");
	} else {
		gpio_free(pdata->gpio_irq);
		gpio_free(pdata->gpio_reset);
	}

	return 0;
}

void maxim_sti_gpio_reset(struct maxim_sti_pdata *pdata, int value)
{
	gpio_set_value(pdata->gpio_reset, !!value);
}

int maxim_sti_gpio_irq(struct maxim_sti_pdata *pdata)
{
	return gpio_get_value(pdata->gpio_irq);
}

/****************************************************************************\
* Device Tree Support
\****************************************************************************/

#ifdef CONFIG_OF
static int maxim_parse_dt(struct device *dev, struct maxim_sti_pdata *pdata)
{
	struct device_node *np = dev->of_node;
	u32 flags;
	const char *str;
	int ret;

	pdata->gpio_reset = of_get_named_gpio_flags(np,
			"maxim_sti,reset-gpio", 0, &flags);
	pdata->default_reset_state = (u8)flags;

	pdata->gpio_irq = of_get_named_gpio_flags(np,
			"maxim_sti,irq-gpio", 0, &flags);

	ret = of_property_read_string(np, "maxim_sti,touch_fusion", &str);
	if (ret) {
		dev_err(dev, "%s: unable to read touch_fusion location (%d)\n",
								__func__, ret);
		goto fail;
	}
	pdata->touch_fusion = (char *)str;

	ret = of_property_read_string(np, "maxim_sti,config_file", &str);
	if (ret) {
		dev_err(dev, "%s: unable to read config_file location (%d)\n",
								__func__, ret);
		goto fail;
	}
	pdata->config_file = (char *)str;

	ret = of_property_read_string(np, "maxim_sti,fw_name", &str);
	if (ret) {
		dev_err(dev, "%s: unable to read fw_name (%d)\n",
								__func__, ret);
	}
	pdata->fw_name = (char *)str;

	return 0;

fail:
	return ret;
}
#endif

/****************************************************************************\
* Suspend/resume processing                                                  *
\****************************************************************************/

#ifdef CONFIG_PM_SLEEP
static int suspend(struct device *dev)
{
	struct dev_data  *dd = spi_get_drvdata(to_spi_device(dev));

	DBG("%s: suspending", __func__);

	if (dd->suspended) {
		DBG("%s: already suspended", __func__);
		return 0;
	}

	if (dd->suspend_in_progress) {
		dev_err(dev, "suspend already in progress!");
		return -EINVAL;
	}

	dd->suspend_in_progress = true;
	wake_up_process(dd->thread);
	wait_for_completion(&dd->suspend_resume);

	DBG("%s: suspended", __func__);

	return 0;
}

static int resume(struct device *dev)
{
	struct dev_data  *dd = spi_get_drvdata(to_spi_device(dev));

	DBG("%s: resuming", __func__);

	if (!dd->suspended && !dd->suspend_in_progress) {
		DBG("%s: not suspended", __func__);
		return 0;
	}

	dd->resume_in_progress = true;
	wake_up_process(dd->thread);
	wait_for_completion(&dd->suspend_resume);
	DBG("%s: resumed", __func__);
	return 0;
}

static const struct dev_pm_ops pm_ops = {
	.suspend = suspend,
	.resume = resume,
};

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event  *evdata = data;
	int              *blank;
	struct dev_data  *dd = container_of(self,
					    struct dev_data, fb_notifier);

	DBG("%s: event = %lu", __func__, event);
	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			resume(&dd->spi->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			suspend(&dd->spi->dev);
	}
	return 0;
}
#endif
#endif

/****************************************************************************\
* Netlink processing                                                         *
\****************************************************************************/

static inline int
nl_msg_new(struct dev_data *dd, u8 dst)
{
	dd->outgoing_skb = alloc_skb(NL_BUF_SIZE, GFP_KERNEL);
	if (dd->outgoing_skb == NULL)
		return -ENOMEM;
	nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id, dd->nl_seq++,
		    dst);
	if (dd->nl_seq == 0)
		dd->nl_seq++;
	return 0;
}

static int
nl_callback_noop(struct sk_buff *skb, struct genl_info *info)
{
	return 0;
}

static inline bool
nl_process_driver_msg(struct dev_data *dd, u16 msg_id, void *msg)
{
	struct maxim_sti_pdata        *pdata = dd->spi->dev.platform_data;
	struct dr_echo_request        *echo_msg;
	struct fu_echo_response       *echo_response;
	struct dr_chip_read           *read_msg;
	struct fu_chip_read_result    *read_result;
	struct dr_chip_write          *write_msg;
	struct dr_chip_access_method  *chip_access_method_msg;
	struct dr_delay               *delay_msg;
	struct fu_irqline_status      *irqline_status;
	struct dr_config_irq          *config_irq_msg;
	struct dr_config_input        *config_input_msg;
	struct dr_config_watchdog     *config_watchdog_msg;
	struct dr_input               *input_msg;
	struct dr_legacy_acceleration *legacy_acceleration_msg;
	struct dr_handshake           *handshake_msg;
	struct fu_handshake_response  *handshake_response;
	struct dr_config_fw           *config_fw_msg;
	struct dr_sysfs_ack           *sysfs_ack_msg;
	struct dr_idle                *idle_msg;
	struct dr_tf_status           *tf_status_msg;
	u8                            i, inp;
	int                           ret;
	u16                           read_value[2] = { 0 };

	if (dd->expect_resume_ack && msg_id != DR_DECONFIG &&
	    msg_id != DR_RESUME_ACK)
		return false;

	switch (msg_id) {
	case DR_ADD_MC_GROUP:
		INFO("warning: deprecated dynamic mc group add request!");
		return false;
	case DR_ECHO_REQUEST:
		DRV_MSG_DBG("msg: %s", "DR_ECHO_REQUEST");
		echo_msg = msg;
		echo_response = nl_alloc_attr(dd->outgoing_skb->data,
					      FU_ECHO_RESPONSE,
					      sizeof(*echo_response));
		if (echo_response == NULL)
			goto alloc_attr_failure;
		echo_response->cookie = echo_msg->cookie;
		return true;
	case DR_CHIP_READ:
		DRV_MSG_DBG("msg: %s", "DR_CHIP_READ");
		read_msg = msg;
		read_result = nl_alloc_attr(dd->outgoing_skb->data,
				FU_CHIP_READ_RESULT,
				sizeof(*read_result) + read_msg->length);
		if (read_result == NULL)
			goto alloc_attr_failure;
		read_result->address = read_msg->address;
		read_result->length = read_msg->length;
		ret = dd->chip.read(dd, read_msg->address, read_result->data,
				    read_msg->length);
		if (ret < 0)
			ERROR("failed to read from chip (%d, %d, %d)",
				read_msg->address, read_msg->length, ret);
		return true;
	case DR_CHIP_WRITE:
		DRV_MSG_DBG("msg: %s", "DR_CHIP_WRITE");
		write_msg = msg;
		if (write_msg->address == dd->irq_param[12] &&
		    write_msg->data[0] == dd->irq_param[13]) {
			ret = dd->chip.write(dd, write_msg->address,
					     write_msg->data,
					     write_msg->length);
			if (ret < 0)
				ERROR("failed to write chip (%d)", ret);
			msleep(15);
			ret = dd->chip.read(dd, dd->irq_param[0],
					    (u8 *)read_value,
					    sizeof(read_value));
			if (ret < 0)
				ERROR("failed to read from chip (%d)", ret);
			ret = dd->chip.write(dd, dd->irq_param[0],
					     (u8 *)read_value,
					     sizeof(read_value));
			return false;
		}
		if (write_msg->address == dd->irq_param[0]) {
			if (((u16 *)write_msg->data)[0] == dd->irq_param[11]) {
				ret = dd->chip.read(dd, dd->irq_param[0],
						(u8 *)read_value,
						sizeof(read_value));
				if (ret < 0)
					ERROR("failed to read from chip (%d)", ret);
				ret = dd->chip.write(dd, dd->irq_param[0],
						(u8 *)read_value,
						sizeof(read_value));
				return false;
			}
			read_value[0] = ((u16 *)write_msg->data)[0];
			ret = dd->chip.write(dd, write_msg->address,
					     (u8 *)read_value,
					     sizeof(read_value));
			return false;
		}
		ret = dd->chip.write(dd, write_msg->address, write_msg->data,
				     write_msg->length);
		if (ret < 0)
			ERROR("failed to write chip (%d, %d, %d)",
				write_msg->address, write_msg->length, ret);
		return false;
	case DR_CHIP_RESET:
		DRV_MSG_DBG("msg: %s", "DR_CHIP_RESET");
		pdata->reset(pdata, ((struct dr_chip_reset *)msg)->state);
		return false;
	case DR_GET_IRQLINE:
		DRV_MSG_DBG("msg: %s", "DR_GET_IRQLINE");
		irqline_status = nl_alloc_attr(dd->outgoing_skb->data,
					       FU_IRQLINE_STATUS,
					       sizeof(*irqline_status));
		if (irqline_status == NULL)
			goto alloc_attr_failure;
		irqline_status->status = pdata->irq(pdata);
		return true;
	case DR_DELAY:
		DRV_MSG_DBG("msg: %s", "DR_DELAY");
		delay_msg = msg;
		if (delay_msg->period > 1000)
			msleep(delay_msg->period / 1000);
		usleep_range(delay_msg->period % 1000,
			    (delay_msg->period % 1000) + 10);
		return false;
	case DR_CHIP_ACCESS_METHOD:
		DRV_MSG_DBG("msg: %s", "DR_CHIP_ACCESS_METHOD");
		chip_access_method_msg = msg;
		ret = set_chip_access_method(dd,
					     chip_access_method_msg->method);
		if (ret < 0)
			ERROR("failed to set chip access method (%d) (%d)",
			      ret, chip_access_method_msg->method);
		return false;
	case DR_CONFIG_IRQ:
		DRV_MSG_DBG("msg: %s", "DR_CONFIG_IRQ");
		config_irq_msg = msg;
		if (config_irq_msg->irq_params > MAX_IRQ_PARAMS) {
			ERROR("too many IRQ parameters");
			return false;
		}
		memcpy(dd->irq_param, config_irq_msg->irq_param,
		       config_irq_msg->irq_params * sizeof(dd->irq_param[0]));
		if (dd->irq_registered)
			return false;
		dd->service_irq = service_irq;
		ret = request_irq(dd->spi->irq, irq_handler,
			(config_irq_msg->irq_edge == DR_IRQ_RISING_EDGE) ?
				IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING,
						pdata->nl_family, dd);
		if (ret < 0) {
			ERROR("failed to request IRQ (%d)", ret);
		} else {
			dd->irq_registered = true;
			wake_up_process(dd->thread);
		}
		return false;
	case DR_CONFIG_INPUT:
		DRV_MSG_DBG("msg: %s", "DR_CONFIG_INPUT");
		config_input_msg = msg;
		for (i = 0; i < INPUT_DEVICES; i++)
			if (dd->input_dev[i] != NULL)
				return false;
		for (i = 0; i < INPUT_DEVICES; i++) {
			dd->input_dev[i] = input_allocate_device();
			if (dd->input_dev[i] == NULL) {
				ERROR("failed to allocate input device");
				continue;
			}
			snprintf(dd->input_phys, sizeof(dd->input_phys),
				 "%s/input%d", dev_name(&dd->spi->dev), i);
			dd->input_dev[i]->name = pdata->nl_family;
			dd->input_dev[i]->phys = dd->input_phys;
			dd->input_dev[i]->id.bustype = BUS_SPI;
			__set_bit(EV_SYN, dd->input_dev[i]->evbit);
			__set_bit(EV_ABS, dd->input_dev[i]->evbit);
			__set_bit(INPUT_PROP_DIRECT, dd->input_dev[i]->propbit);
			if (i == (INPUT_DEVICES - 1)) {
				__set_bit(EV_KEY, dd->input_dev[i]->evbit);
				__set_bit(BTN_TOOL_RUBBER,
					  dd->input_dev[i]->keybit);
				__set_bit(KEY_POWER,
					  dd->input_dev[i]->keybit);
			}
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_POSITION_X, 0,
					     config_input_msg->x_range, 0, 0);
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_POSITION_Y, 0,
					     config_input_msg->y_range, 0, 0);
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_TRACKING_ID, 0,
					     MAX_INPUT_EVENTS, 0, 0);
			if (i == (INPUT_DEVICES - 1))
				input_set_abs_params(dd->input_dev[i],
						     ABS_MT_TOOL_TYPE, 0,
						     MT_TOOL_MAX, 0, 0);
			else
				input_set_abs_params(dd->input_dev[i],
						     ABS_MT_TOOL_TYPE, 0,
						     MT_TOOL_FINGER, 0, 0);

			ret = input_register_device(dd->input_dev[i]);
			if (ret < 0) {
				input_free_device(dd->input_dev[i]);
				dd->input_dev[i] = NULL;
				ERROR("failed to register input device");
			}
		}
#if defined(CONFIG_FB)
		dd->fb_notifier.notifier_call = fb_notifier_callback;
		fb_register_client(&dd->fb_notifier);
#endif
		/* create symlink */
		if (dd->parent == NULL && dd->input_dev[0] != NULL) {
			dd->parent = dd->input_dev[0]->dev.kobj.parent;
			ret = sysfs_create_link(dd->parent, &dd->spi->dev.kobj,
								MAXIM_STI_NAME);
			if (ret) {
				ERROR("sysfs_create_link error\n");
				dd->parent = NULL;
			}
		}
		return false;
	case DR_CONFIG_WATCHDOG:
		DRV_MSG_DBG("msg: %s", "DR_CONFIG_WATCHDOG");
		config_watchdog_msg = msg;
		dd->fusion_process = (pid_t)config_watchdog_msg->pid;
		return false;
	case DR_DECONFIG:
		DRV_MSG_DBG("msg: %s", "DR_DECONFIG");
		if (dd->irq_registered) {
			free_irq(dd->spi->irq, dd);
			dd->irq_registered = false;
		}
		stop_scan_canned(dd);

		if (!dd->input_no_deconfig) {
			if (dd->parent != NULL) {
				sysfs_remove_link(
					dd->input_dev[0]->dev.kobj.parent,
					MAXIM_STI_NAME);
				dd->parent = NULL;
			}
		}

		if (!dd->input_no_deconfig) {
			for (i = 0; i < INPUT_DEVICES; i++) {
				if (dd->input_dev[i] == NULL)
					continue;
				input_unregister_device(dd->input_dev[i]);
				dd->input_dev[i] = NULL;
			}
#if defined(CONFIG_FB)
			fb_unregister_client(&dd->fb_notifier);
#endif
		}
		dd->expect_resume_ack = false;
		dd->eraser_active = false;
		dd->legacy_acceleration = false;
		dd->service_irq = service_irq;
		dd->fusion_process = (pid_t)0;
		dd->sysfs_update_type = DR_SYSFS_UPDATE_NONE;
		return false;
	case DR_INPUT:
		DRV_MSG_DBG("msg: %s", "DR_INPUT");
		input_msg = msg;
		if (input_msg->events == 0) {
			if (dd->eraser_active) {
				input_report_key(
					dd->input_dev[INPUT_DEVICES - 1],
					BTN_TOOL_RUBBER, 0);
				dd->eraser_active = false;
			}
			for (i = 0; i < INPUT_DEVICES; i++) {
				input_mt_sync(dd->input_dev[i]);
				input_sync(dd->input_dev[i]);
			}
		} else {
			for (i = 0; i < input_msg->events; i++) {
				switch (input_msg->event[i].tool_type) {
				case DR_INPUT_FINGER:
					inp = 0;
					input_report_abs(dd->input_dev[inp],
							 ABS_MT_TOOL_TYPE,
							 MT_TOOL_FINGER);
					break;
				case DR_INPUT_STYLUS:
					inp = INPUT_DEVICES - 1;
					input_report_abs(dd->input_dev[inp],
							 ABS_MT_TOOL_TYPE,
							 MT_TOOL_PEN);
					break;
				case DR_INPUT_ERASER:
					inp = INPUT_DEVICES - 1;
					input_report_key(dd->input_dev[inp],
						BTN_TOOL_RUBBER, 1);
					dd->eraser_active = true;
					break;
				default:
					inp = 0;
					ERROR("invalid input tool type (%d)",
					      input_msg->event[i].tool_type);
					break;
				}
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_TRACKING_ID,
						 input_msg->event[i].id);
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_POSITION_X,
						 input_msg->event[i].x);
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_POSITION_Y,
						 input_msg->event[i].y);
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_PRESSURE,
						 input_msg->event[i].z);
				input_mt_sync(dd->input_dev[inp]);
			}
			for (i = 0; i < INPUT_DEVICES; i++)
				input_sync(dd->input_dev[i]);
		}
		return false;
	case DR_RESUME_ACK:
		DRV_MSG_DBG("msg: %s", "DR_RESUME_ACK");
		dd->expect_resume_ack = false;
		if (dd->irq_registered
		    && !dd->gesture_en
		   )
			enable_irq(dd->spi->irq);
		return false;
	case DR_LEGACY_FWDL:
		DRV_MSG_DBG("msg: %s", "DR_LEGACY_FWDL");
		ret = fw_request_load(dd);
		if (ret < 0)
			ERROR("firmware download failed (%d)", ret);
		else
			INFO("firmware download OK");
		return false;
	case DR_LEGACY_ACCELERATION:
		DRV_MSG_DBG("msg: %s", "DR_LEGACY_ACCELERATION");
		legacy_acceleration_msg = msg;
		if (legacy_acceleration_msg->enable) {
			dd->service_irq = service_irq_legacy_acceleration;
			start_legacy_acceleration(dd);
			dd->legacy_acceleration = true;
		} else {
			stop_legacy_acceleration(dd);
			dd->legacy_acceleration = false;
			dd->service_irq = service_irq;
		}
		return false;
	case DR_HANDSHAKE:
		DRV_MSG_DBG("msg: %s", "DR_HANDSHAKE");
		handshake_msg = msg;
		dd->tf_ver = handshake_msg->tf_ver;
		dd->chip_id = handshake_msg->chip_id;
		dd->drv_ver = DRIVER_VERSION_NUM;
		handshake_response = nl_alloc_attr(dd->outgoing_skb->data,
						FU_HANDSHAKE_RESPONSE,
						sizeof(*handshake_response));
		if (handshake_response == NULL)
			goto alloc_attr_failure;
		handshake_response->driver_ver = dd->drv_ver;
		handshake_response->panel_id = panel_id;
		handshake_response->driver_protocol = DRIVER_PROTOCOL;
		return true;
	case DR_CONFIG_FW:
		DRV_MSG_DBG("msg: %s", "DR_CONFIG_FW");
		config_fw_msg = msg;
		dd->fw_ver = config_fw_msg->fw_ver;
		dd->fw_protocol = config_fw_msg->fw_protocol;
		return false;
	case DR_SYSFS_ACK:
		sysfs_ack_msg = msg;
		if (sysfs_ack_msg->type == DR_SYSFS_ACK_GLOVE)
			complete(&dd->sysfs_ack_glove);
		if (sysfs_ack_msg->type == DR_SYSFS_ACK_CHARGER)
			complete(&dd->sysfs_ack_charger);
		if (sysfs_ack_msg->type == DR_SYSFS_ACK_LCD_FPS)
			complete(&dd->sysfs_ack_lcd_fps);
		return false;
	case DR_IDLE:
		DRV_MSG_DBG("msg: %s", "DR_IDLE");
		idle_msg = msg;
		dd->idle = !!(idle_msg->idle);
		return false;
	case DR_TF_STATUS:
		tf_status_msg = msg;
		dd->tf_status = tf_status_msg->tf_status;
		return false;
	default:
		ERROR("unexpected message %d", msg_id);
		return false;
	}

alloc_attr_failure:
	ERROR("failed to allocate response for msg_id %d", msg_id);
	return false;
}

static int nl_process_msg(struct dev_data *dd, struct sk_buff *skb)
{
	struct nlattr  *attr;
	bool           send_reply = false;
	int            ret = 0, ret2;

	/* process incoming message */
	attr = NL_ATTR_FIRST(skb->data);
	for (; attr < NL_ATTR_LAST(skb->data); attr = NL_ATTR_NEXT(attr)) {
		if (nl_process_driver_msg(dd, attr->nla_type,
					  NL_ATTR_VAL(attr, void)))
			send_reply = true;
	}

	/* send back reply if requested */
	if (send_reply) {
		(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
		if (NL_SEQ(skb->data) == 0)
			ret = genlmsg_unicast(&init_net,
					      dd->outgoing_skb,
					      NETLINK_CB(skb).portid);
		else
			ret = genlmsg_multicast(&dd->nl_family,
					dd->outgoing_skb, 0,
					MC_FUSION, GFP_KERNEL);
		if (ret < 0)
			ERROR("could not reply to fusion (%d)", ret);

		/* allocate new outgoing skb */
		ret2 = nl_msg_new(dd, MC_FUSION);
		if (ret2 < 0)
			ERROR("could not allocate outgoing skb (%d)", ret2);
	}

	/* free incoming message */
	kfree_skb(skb);
	return ret;
}

static int
nl_callback_driver(struct sk_buff *skb, struct genl_info *info)
{
	struct dev_data  *dd;
	struct sk_buff   *skb2;
	unsigned long    flags;

	/* locate device structure */
	spin_lock_irqsave(&dev_lock, flags);
	list_for_each_entry(dd, &dev_list, dev_list)
		if (dd->nl_family.id == NL_TYPE(skb->data))
			break;
	spin_unlock_irqrestore(&dev_lock, flags);
	if (&dd->dev_list == &dev_list)
		return -ENODEV;
	if (!dd->nl_enabled)
		return -EAGAIN;

	/* queue incoming skb and wake up processing thread */
	skb2 = skb_clone(skb, GFP_ATOMIC);
	if (skb2 == NULL) {
		ERROR("failed to clone incoming skb");
		return -ENOMEM;
	} else {
		skb_queue_tail(&dd->incoming_skb_queue, skb2);
		wake_up_process(dd->thread);
		return 0;
	}
}

static int
nl_callback_fusion(struct sk_buff *skb, struct genl_info *info)
{
	struct dev_data  *dd;
	unsigned long    flags;

	/* locate device structure */
	spin_lock_irqsave(&dev_lock, flags);
	list_for_each_entry(dd, &dev_list, dev_list)
		if (dd->nl_family.id == NL_TYPE(skb->data))
			break;
	spin_unlock_irqrestore(&dev_lock, flags);
	if (&dd->dev_list == &dev_list)
		return -ENODEV;
	if (!dd->nl_enabled)
		return -EAGAIN;

	(void)genlmsg_multicast(&dd->nl_family,
			skb_clone(skb, GFP_ATOMIC), 0,
			MC_FUSION, GFP_ATOMIC);
	return 0;
}

/****************************************************************************\
* Interrupt processing                                                       *
\****************************************************************************/

static unsigned long interrupt_count;
static irqreturn_t irq_handler(int irq, void *context)
{
	struct dev_data  *dd = context;

	DBG("%s: interrupt #%lu", __func__, interrupt_count++);

	wake_up_process(dd->thread);
	return IRQ_HANDLED;
}

static void service_irq_legacy_acceleration(struct dev_data *dd)
{
	struct fu_async_data  *async_data;
	u16                   len, rx_len = 0, offset = 0;
	u16                   buf[255], rx_limit = 250 * sizeof(u16);
	int                   ret = 0, counter = 0;

	async_data = nl_alloc_attr(dd->outgoing_skb->data, FU_ASYNC_DATA,
				   sizeof(*async_data) + dd->irq_param[4] +
				   2 * sizeof(u16));
	if (async_data == NULL) {
		ERROR("can't add data to async IRQ buffer");
		return;
	}
	async_data->length = dd->irq_param[4] + 2 * sizeof(u16);
	len = async_data->length;
	async_data->address = 0;

	while (len > 0) {
		rx_len = (len > rx_limit) ? rx_limit : len;
		ret = spi_read_123(dd, 0x0000, (u8 *)&buf,
					rx_len + 4 * sizeof(u16), false);
		if (ret < 0)
			break;

		if (buf[3] == 0xBABE) {
			dd->legacy_acceleration = false;
			dd->service_irq = service_irq;
			nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
				    dd->nl_seq - 1, MC_FUSION);
			return;
		}

		if (rx_limit == rx_len)
			usleep_range(200, 300);

		if (buf[0] == 0x6060) {
			ERROR("data not ready");
			start_legacy_acceleration_canned(dd);
			ret = -EBUSY;
			break;
		} else if (buf[0] == 0x8070) {
			if (buf[1] == dd->irq_param[1] ||
					buf[1] == dd->irq_param[2])
				async_data->address = buf[1];

			if (async_data->address +
					offset / sizeof(u16) != buf[1]) {
				ERROR("sequence number incorrect %04X", buf[1]);
				start_legacy_acceleration_canned(dd);
				ret = -EBUSY;
				break;
			}
		}
		counter++;
		memcpy(async_data->data + offset, buf + 4, rx_len);
		offset += rx_len;
		len -= rx_len;
	}
	async_data->status = *(buf + rx_len / sizeof(u16) + 2);

	if (ret < 0) {
		ERROR("can't read IRQ buffer (%d)", ret);
		nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
			    dd->nl_seq - 1, MC_FUSION);
	} else {
		(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
		ret = genlmsg_multicast(&dd->nl_family, dd->outgoing_skb, 0,
				MC_FUSION, GFP_KERNEL);
		if (ret < 0) {
			ERROR("can't send IRQ buffer %d", ret);
			msleep(300);
			if (++dd->send_fail_count >= 10 &&
			    dd->fusion_process != (pid_t)0) {
				(void)kill_pid(
					find_get_pid(dd->fusion_process),
					SIGKILL, 1);
				wake_up_process(dd->thread);
			}
		} else {
			dd->send_fail_count = 0;
		}
		ret = nl_msg_new(dd, MC_FUSION);
		if (ret < 0)
			ERROR("could not allocate outgoing skb (%d)", ret);
	}
}

static void service_irq(struct dev_data *dd)
{
	struct fu_async_data  *async_data;
	u16                   status, test, xbuf, value;
	u16                   address[2] = { 0 };
	u16                   clear[2] = { 0 };
	bool                  read_buf[2] = {true, false};
	int                   ret, ret2;

	DBG("%s", __func__);
	ret = dd->chip.read(dd, dd->irq_param[0], (u8 *)&status,
			    sizeof(status));
	if (ret < 0) {
		ERROR("can't read IRQ status (%d)", ret);
		return;
	}
	DBG("%s: status = 0x%04X", __func__, status);

	if (dd->gesture_reset) {
		read_buf[0] = false;
		clear[0] = dd->irq_param[10];
		status = dd->irq_param[10];
		dd->gesture_reset = false;
	} else
	if (status & dd->irq_param[10]) {
		read_buf[0] = false;
		clear[0] = dd->irq_param[10];
	} else if (status & dd->irq_param[18]) {
		(void)dd->chip.read(dd, dd->irq_param[22], (u8 *)&value,
				    sizeof(value));
		if (value != dd->irq_param[23]) {
			ERROR("unrecognized value [%#x] => %#x",
					dd->irq_param[22], value);
			clear[0] = dd->irq_param[18];
			ret2 = dd->chip.write(dd, dd->irq_param[0],
					      (u8 *)clear, sizeof(clear));
			return;
		}

		test = status & (dd->irq_param[6] | dd->irq_param[7]);

		if (test == 0)
			return;
		else if (test == (dd->irq_param[6] | dd->irq_param[7]))
			xbuf = ((status & dd->irq_param[5]) == 0) ? 0 : 1;
		else if (test == dd->irq_param[6])
			xbuf = 0;
		else if (test == dd->irq_param[7])
			xbuf = 1;
		else {
			ERROR("unexpected IRQ handler case 0x%04X ", status);
			return;
		}

		read_buf[1] = true;
		address[1] = xbuf ? dd->irq_param[2] : dd->irq_param[1];
		address[0] = dd->irq_param[3];
		clear[0] = xbuf ? dd->irq_param[7] : dd->irq_param[6];
		clear[0] |= dd->irq_param[8];
		clear[0] |= dd->irq_param[18];

		value = dd->irq_param[17];
		(void)dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
				     sizeof(value));
	} else if (status & dd->irq_param[9]) {
		test = status & (dd->irq_param[6] | dd->irq_param[7]);

		if (test == (dd->irq_param[6] | dd->irq_param[7]))
			xbuf = ((status & dd->irq_param[5]) != 0) ? 0 : 1;
		else if (test == dd->irq_param[6])
			xbuf = 0;
		else if (test == dd->irq_param[7])
			xbuf = 1;
		else {
			ERROR("unexpected IRQ handler case");
			return;
		}
		read_buf[1] = true;
		address[1] = xbuf ? dd->irq_param[2] : dd->irq_param[1];

		address[0] = dd->irq_param[3];
		clear[0] = dd->irq_param[6] | dd->irq_param[7] |
			dd->irq_param[8] | dd->irq_param[9];
		value = dd->irq_param[13];
		(void)dd->chip.write(dd, dd->irq_param[12], (u8 *)&value,
				     sizeof(value));
	} else {
		test = status & (dd->irq_param[6] | dd->irq_param[7]);

		if (test == 0)
			return;
		else if (test == (dd->irq_param[6] | dd->irq_param[7]))
			xbuf = ((status & dd->irq_param[5]) == 0) ? 0 : 1;
		else if (test == dd->irq_param[6])
			xbuf = 0;
		else if (test == dd->irq_param[7])
			xbuf = 1;
		else {
			ERROR("unexpected IRQ handler case 0x%04X ",status);
			return;
		}

		address[0] = xbuf ? dd->irq_param[2] : dd->irq_param[1];
		clear[0] = xbuf ? dd->irq_param[7] : dd->irq_param[6];
		clear[0] |= dd->irq_param[8];
	}

	async_data = nl_alloc_attr(dd->outgoing_skb->data, FU_ASYNC_DATA,
				   sizeof(*async_data) + dd->irq_param[4]);
	if (async_data == NULL) {
		ERROR("can't add data to async IRQ buffer 1");
		return;
	}

	async_data->status = status;
	if (read_buf[0]) {
		async_data->address = address[0];
		async_data->length = dd->irq_param[4];
		ret = dd->chip.read(dd, address[0], async_data->data,
				    dd->irq_param[4]);
	}

	if (read_buf[1] && ret == 0) {
		async_data = nl_alloc_attr(dd->outgoing_skb->data,
					   FU_ASYNC_DATA,
					   sizeof(*async_data) +
						dd->irq_param[4]);
		if (async_data == NULL) {
			ERROR("can't add data to async IRQ buffer 2");
			nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
				    dd->nl_seq - 1, MC_FUSION);
			return;
		}
		async_data->address = address[1];
		async_data->length = dd->irq_param[4];
		async_data->status = status;
		ret = dd->chip.read(dd, address[1], async_data->data,
				    dd->irq_param[4]);
	}

	ret2 = dd->chip.write(dd, dd->irq_param[0], (u8 *)clear,
			     sizeof(clear));
	if (ret2 < 0)
		ERROR("can't clear IRQ status (%d)", ret2);

	if (ret < 0) {
		ERROR("can't read IRQ buffer (%d)", ret);
		nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
			    dd->nl_seq - 1, MC_FUSION);
	} else {
		DBG("%s: sending buffer", __func__);
		(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
		ret = genlmsg_multicast(&dd->nl_family, dd->outgoing_skb, 0,
				MC_FUSION, GFP_KERNEL);
		if (ret < 0) {
			ERROR("can't send IRQ buffer %d", ret);
			msleep(300);
			if (read_buf[0] == false ||
			    (++dd->send_fail_count >= 10 &&
			     dd->fusion_process != (pid_t)0)) {
				(void)kill_pid(
					find_get_pid(dd->fusion_process),
					SIGKILL, 1);
				wake_up_process(dd->thread);
			}
		} else {
			dd->send_fail_count = 0;
		}
		ret = nl_msg_new(dd, MC_FUSION);
		if (ret < 0)
			ERROR("could not allocate outgoing skb (%d)", ret);
	}
}

/****************************************************************************\
* Processing thread                                                          *
\****************************************************************************/

static int processing_thread(void *arg)
{
	struct dev_data         *dd = arg;
	struct maxim_sti_pdata  *pdata = dd->spi->dev.platform_data;
	struct sk_buff          *skb;
	char                    *argv[] = { pdata->touch_fusion, "daemon",
					    pdata->nl_family,
					    dd->config_file, NULL };
	int                     ret = 0, ret2 = 0;
	bool                    fusion_dead;

	DRV_PT_DBG("%s started", __FUNCTION__);
	sched_setscheduler(current, SCHED_FIFO, &dd->thread_sched);

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		DRV_PT_DBG("Begin");

		/* ensure that we have outgoing skb */
		if (dd->outgoing_skb == NULL)
			if (nl_msg_new(dd, MC_FUSION) < 0) {
				schedule();
				continue;
			}

		/* priority 1: start up fusion process */
		if (dd->fusion_process != (pid_t)0 && get_pid_task(
					find_get_pid(dd->fusion_process),
					PIDTYPE_PID) == NULL) {
			DRV_PT_DBG("Re-Starting TF");
			stop_scan_canned(dd);
			dd->start_fusion = true;
			dd->fusion_process = (pid_t)0;
			dd->input_no_deconfig = true;
		}
		if (dd->start_fusion) {
			DRV_PT_DBG("Spawning TF");
			do {
				ret = call_usermodehelper(argv[0], argv, NULL,
							  UMH_WAIT_EXEC);
				if (ret != 0)
					msleep(100);
			} while (ret != 0 && !kthread_should_stop());

			if (ret == 0) {
				/* power-up and reset-high */
				ret = regulator_control(dd, true);
				if (ret < 0)
					ERROR("failed to enable regulators");

				usleep_range(300, 400);
				pdata->reset(pdata, 1);

				dd->start_fusion = false;
				dd->tf_status |= TF_STATUS_BUSY;
			}
		}
		if (kthread_should_stop())
			break;

		/* priority 2: process pending Netlink messages */
		while ((skb = skb_dequeue(&dd->incoming_skb_queue)) != NULL) {
			if (kthread_should_stop())
				break;
			if (nl_process_msg(dd, skb) < 0)
				skb_queue_purge(&dd->incoming_skb_queue);
		}
		if (kthread_should_stop())
			break;

		/* priority 3: suspend/resume */
		if (dd->suspend_in_progress && !(dd->tf_status & TF_STATUS_BUSY)) {
			if (dd->irq_registered) {
				disable_irq(dd->spi->irq);
				if (dd->gesture_en) {
					enable_gesture(dd);
					enable_irq(dd->spi->irq);
				} else
				if (!dd->idle)
					stop_scan_canned(dd);
				else
					stop_idle_scan(dd);
			}

			dd->suspended = true;
			complete(&dd->suspend_resume);
			dd->expect_resume_ack = true;
			dd->gesture_reset = false;
			while (!dd->resume_in_progress) {
				/* the line below is a MUST */
				set_current_state(TASK_INTERRUPTIBLE);
				if (kthread_should_stop())
					break;
				if (dd->gesture_en && dd->irq_registered &&
				    pdata->irq(pdata) == 0)
					gesture_wake_detect(dd);
				schedule();
			}
			if (dd->irq_registered) {
				if (dd->gesture_en)
					finish_gesture(dd);
				else
				if (!dd->idle)
					start_scan_canned(dd);
			}
			dd->suspended = false;
			dd->resume_in_progress = false;
			dd->suspend_in_progress = false;
			complete(&dd->suspend_resume);

			fusion_dead = false;
			dd->send_fail_count = 0;
			do {
				if (dd->fusion_process != (pid_t)0 &&
				    get_pid_task(find_get_pid(
							dd->fusion_process),
						 PIDTYPE_PID) == NULL) {
					fusion_dead = true;
					break;
				}
				ret = nl_add_attr(dd->outgoing_skb->data,
						  FU_RESUME, NULL, 0);
				if (ret < 0) {
					ERROR("can't add data to resume " \
					      "buffer");
					nl_msg_init(dd->outgoing_skb->data,
						    dd->nl_family.id,
						    dd->nl_seq - 1, MC_FUSION);
					msleep(100);
					continue;
				}
				(void)skb_put(dd->outgoing_skb,
					      NL_SIZE(dd->outgoing_skb->data));
				ret = genlmsg_multicast(&dd->nl_family,
						dd->outgoing_skb, 0, MC_FUSION,
						GFP_KERNEL);
				if (ret < 0) {
					ERROR("can't send resume message %d",
					      ret);
					msleep(100);
					if (++dd->send_fail_count >= 10) {
						fusion_dead = true;
						ret = 0;
					}
				}
				ret2 = nl_msg_new(dd, MC_FUSION);
				if (ret2 < 0)
					ERROR("could not allocate outgoing " \
					      "skb (%d)", ret2);
			} while (ret != 0 && !kthread_should_stop());

			if (!ret && dd->send_fail_count>= 10 &&
			    dd->fusion_process != (pid_t)0)
			       (void)kill_pid(find_get_pid(dd->fusion_process),
					      SIGKILL, 1);

			dd->send_fail_count = 0;
			if (fusion_dead)
				continue;
		}
		if (kthread_should_stop())
			break;

		/* priority 4: update /sys/device information */
		if (dd->sysfs_update_type != DR_SYSFS_UPDATE_NONE)
			if (!dd->expect_resume_ack) {
				if (send_sysfs_info(dd) < 0)
					ERROR(
					      "Can not send sysfs update to touch fusion"
					     );
				dd->sysfs_update_type = DR_SYSFS_UPDATE_NONE;
			}

		/* priority 5: service interrupt */
		if (dd->irq_registered && !dd->expect_resume_ack &&
		    ((pdata->irq(pdata) == 0)
		     || dd->gesture_reset
		    ))
			dd->service_irq(dd);
		if (dd->irq_registered && !dd->expect_resume_ack &&
		    pdata->irq(pdata) == 0)
			continue;

		DRV_PT_DBG("End");
		/* nothing more to do; sleep */
		schedule();
	}

	return 0;
}

/****************************************************************************\
* SYSFS interface                                                            *
\****************************************************************************/

static ssize_t chip_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "0x%04X\n", dd->chip_id);
}

static ssize_t fw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "0x%04X\n", dd->fw_ver);
}

static ssize_t driver_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "0x%04X\n", dd->drv_ver);
}

static ssize_t tf_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "0x%04X\n", dd->tf_ver);
}

static ssize_t panel_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04X\n", panel_id);
}

static ssize_t glove_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->glove_enabled);
}

static ssize_t glove_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;
	unsigned long           time_left;

	mutex_lock(&dd->sysfs_update_mutex);

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	dd->glove_enabled = !!temp;
	set_bit(DR_SYSFS_UPDATE_BIT_GLOVE, &(dd->sysfs_update_type));
	wake_up_process(dd->thread);
	time_left = wait_for_completion_timeout(&dd->sysfs_ack_glove, 3*HZ);

	mutex_unlock(&dd->sysfs_update_mutex);

	if (time_left > 0)
		return strnlen(buf, PAGE_SIZE);
	else
		return -EAGAIN;
}

static ssize_t gesture_wakeup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->gesture_en);
}

static ssize_t gesture_wakeup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	dd->gesture_en = temp;

	return strnlen(buf, PAGE_SIZE);
}

static ssize_t charger_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->charger_mode_en);
}

static ssize_t charger_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;
	unsigned long           time_left;

	mutex_lock(&dd->sysfs_update_mutex);

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	if (temp > DR_WIRELESS_CHARGER) {
		ERROR("Charger Mode Value Out of Range");
		return -EINVAL;
	}
	dd->charger_mode_en = temp;
	set_bit(DR_SYSFS_UPDATE_BIT_CHARGER, &(dd->sysfs_update_type));
	wake_up_process(dd->thread);
	time_left = wait_for_completion_timeout(&dd->sysfs_ack_charger, 3*HZ);

	mutex_unlock(&dd->sysfs_update_mutex);

	if (time_left > 0)
		return strnlen(buf, PAGE_SIZE);
	else
		return -EAGAIN;
}

static ssize_t info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "chip id: 0x%04X driver ver: 0x%04X "
					"fw ver: 0x%04X panel id: 0x%04X "
					"tf ver: 0x%04X\n",
					dd->chip_id, dd->drv_ver, dd->fw_ver,
					panel_id, dd->tf_ver);
}

static ssize_t screen_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	if (temp)
		resume(dev);
	else
		suspend(dev);

	return strnlen(buf, PAGE_SIZE);
}

static ssize_t tf_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "0x%08X", dd->tf_status);
}

static ssize_t default_loaded_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%d",
			(dd->tf_status & TF_STATUS_DEFAULT_LOADED));
}

static ssize_t lcd_fps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->lcd_fps);
}

static ssize_t lcd_fps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;
	unsigned long           time_left;

	mutex_lock(&dd->sysfs_update_mutex);

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}

	dd->lcd_fps = temp;
	set_bit(DR_SYSFS_UPDATE_BIT_LCD_FPS, &(dd->sysfs_update_type));
	wake_up_process(dd->thread);
	time_left = wait_for_completion_timeout(&dd->sysfs_ack_lcd_fps, 3*HZ);

	mutex_unlock(&dd->sysfs_update_mutex);

	if (time_left > 0)
		return strnlen(buf, PAGE_SIZE);
	else
		return -EAGAIN;
}

static struct device_attribute dev_attrs[] = {
	__ATTR(fw_ver, S_IRUGO, fw_ver_show, NULL),
	__ATTR(chip_id, S_IRUGO, chip_id_show, NULL),
	__ATTR(tf_ver, S_IRUGO, tf_ver_show, NULL),
	__ATTR(driver_ver, S_IRUGO, driver_ver_show, NULL),
	__ATTR(panel_id, S_IRUGO, panel_id_show, NULL),
	__ATTR(glove_en, S_IRUGO | S_IWUSR, glove_show, glove_store),
	__ATTR(gesture_wakeup, S_IRUGO | S_IWUSR, gesture_wakeup_show, gesture_wakeup_store),
	__ATTR(charger_mode, S_IRUGO | S_IWUSR, charger_mode_show, charger_mode_store),
	__ATTR(info, S_IRUGO, info_show, NULL),
	__ATTR(screen_status, S_IWUSR, NULL, screen_status_store),
	__ATTR(tf_status, S_IRUGO, tf_status_show, NULL),
	__ATTR(default_loaded, S_IRUGO, default_loaded_show, NULL),
	__ATTR(lcd_fps, S_IRUGO | S_IWUSR, lcd_fps_show, lcd_fps_store),
};

static int create_sysfs_entries(struct dev_data *dd)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(dev_attrs); i++) {
		ret = device_create_file(&dd->spi->dev, &dev_attrs[i]);
		if (ret) {
			for (; i >= 0; --i) {
				device_remove_file(&dd->spi->dev,
							&dev_attrs[i]);
				dd->sysfs_created--;
			}
			break;
		}
		dd->sysfs_created++;
	}
	return ret;
}

static void remove_sysfs_entries(struct dev_data *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dev_attrs); i++)
		if (dd->sysfs_created && dd->sysfs_created--)
			device_remove_file(&dd->spi->dev, &dev_attrs[i]);
}

static int send_sysfs_info(struct dev_data *dd)
{
	struct fu_sysfs_info       *sysfs_info;
	int                        ret, ret2;

	sysfs_info = nl_alloc_attr(dd->outgoing_skb->data,
					FU_SYSFS_INFO,
					sizeof(*sysfs_info));
	if (sysfs_info == NULL) {
		ERROR("Can't allocate sysfs_info");
		return -EAGAIN;
	}
	sysfs_info->type = dd->sysfs_update_type;

	sysfs_info->glove_value = dd->glove_enabled;
	sysfs_info->charger_value = dd->charger_mode_en;
	sysfs_info->lcd_fps_value = dd->lcd_fps;

	(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
	ret = genlmsg_multicast(&dd->nl_family,
			dd->outgoing_skb, 0,
			MC_FUSION, GFP_KERNEL);
	if (ret < 0)
		ERROR("could not reply to fusion (%d)", ret);

	/* allocate new outgoing skb */
	ret2 = nl_msg_new(dd, MC_FUSION);
	if (ret2 < 0)
		ERROR("could not allocate outgoing skb (%d)", ret2);

	return 0;
}

/****************************************************************************\
* Driver initialization                                                      *
\****************************************************************************/

static int probe(struct spi_device *spi)
{
	struct maxim_sti_pdata  *pdata = spi->dev.platform_data;
	struct dev_data         *dd;
	unsigned long           flags;
	int                     ret, i;
	void                    *ptr;

#ifdef CONFIG_OF
	if (pdata == NULL && spi->dev.of_node) {
		pdata = devm_kzalloc(&spi->dev,
			sizeof(struct maxim_sti_pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&spi->dev, "failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = maxim_parse_dt(&spi->dev, pdata);
		if (ret)
			goto of_parse_failure;
		spi->dev.platform_data = pdata;
	}
#endif

	pdata->init = maxim_sti_gpio_init;
	pdata->reset = maxim_sti_gpio_reset;
	pdata->irq = maxim_sti_gpio_irq;

	pdata->nl_mc_groups = DEF_NL_MC_GROUPS;
	pdata->chip_access_method = DEF_CHIP_ACC_METHOD;
	pdata->tx_buf_size = BUF_SIZE;
	pdata->rx_buf_size = BUF_SIZE;
	pdata->nl_family = TOUCH_FUSION;

	dev_dbg(&spi->dev, "maxim_sti,reset-gpio: %d", pdata->gpio_reset);
	dev_dbg(&spi->dev, "maxim_sti,irq-gpio: %d", pdata->gpio_irq);
	dev_dbg(&spi->dev, "maxim_sti,nl_mc_groups: %d", pdata->nl_mc_groups);
	dev_dbg(&spi->dev, "maxim_sti,chip_access_method: %d", pdata->chip_access_method);
	dev_dbg(&spi->dev, "maxim_sti,default_reset_state: %d", pdata->default_reset_state);
	dev_dbg(&spi->dev, "maxim_sti,tx_buf_size: %d", pdata->tx_buf_size);
	dev_dbg(&spi->dev, "maxim_sti,rx_buf_size: %d", pdata->rx_buf_size);
	dev_dbg(&spi->dev, "maxim_sti,touch_fusion: %s", pdata->touch_fusion);
	dev_dbg(&spi->dev, "maxim_sti,config_file: %s", pdata->config_file);
	dev_dbg(&spi->dev, "maxim_sti,nl_family: %s", pdata->nl_family);
	dev_dbg(&spi->dev, "maxim_sti,fw_name: %s", pdata->fw_name);

	/* validate platform data */
	if (pdata == NULL || pdata->init == NULL || pdata->reset == NULL ||
		pdata->irq == NULL || pdata->touch_fusion == NULL ||
		pdata->config_file == NULL || pdata->nl_family == NULL ||
		GENL_CHK(pdata->nl_family) ||
		pdata->nl_mc_groups < MC_GROUPS ||
		pdata->chip_access_method == 0 ||
		pdata->chip_access_method > ARRAY_SIZE(chip_access_methods) ||
		pdata->default_reset_state > 1 ||
		pdata->tx_buf_size == 0 ||
		pdata->rx_buf_size == 0) {
		dev_err(&spi->dev, "invalid platform data!");
		ret = -EINVAL;
		goto of_parse_failure;
	}

	/* device context: allocate structure */
	dd = kzalloc(sizeof(*dd) +
		     sizeof(*dd->nl_ops) * pdata->nl_mc_groups +
		     sizeof(*dd->nl_mc_groups) * pdata->nl_mc_groups,
		     GFP_KERNEL);
	if (dd == NULL) {
		dev_err(&spi->dev, "cannot allocate memory!");
		ret = -ENOMEM;
		goto of_parse_failure;
	}

	/* device context: set up dynamic allocation pointers */
	ptr = (void *)dd + sizeof(*dd);
	dd->nl_ops = ptr;
	ptr += sizeof(*dd->nl_ops) * pdata->nl_mc_groups;
	dd->nl_mc_groups = ptr;

	/* device context: initialize structure members */
	spi_set_drvdata(spi, dd);
	spi->bits_per_word = 8;
	spi_setup(spi);
	dd->spi = spi;
	dd->nl_seq = 1;
	init_completion(&dd->suspend_resume);

	/* allocate DMA memory */
	dd->tx_buf = kzalloc(pdata->tx_buf_size + pdata->rx_buf_size,
				GFP_KERNEL | GFP_DMA);
	if (dd->tx_buf == NULL) {
		dev_err(&spi->dev, "cannot allocate DMA accesible memory");
		ret = -ENOMEM;
		goto buf_alloc_failure;
	}
	dd->rx_buf = dd->tx_buf + pdata->tx_buf_size;
	memset(dd->tx_buf, 0xFF, pdata->tx_buf_size);
	(void)set_chip_access_method(dd, pdata->chip_access_method);

	/* initialize regulators */
	regulator_init(dd);

	/* initialize platform */

	/* Get pinctrl if target uses pinctrl */
	dd->ts_pinctrl = devm_pinctrl_get(&(spi->dev));
	if (IS_ERR_OR_NULL(dd->ts_pinctrl)) {
		ret = PTR_ERR(dd->ts_pinctrl);
		pr_debug("Target does not use pinctrl %d\n", ret);
	}

	dd->pinctrl_state_active
		= pinctrl_lookup_state(dd->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(dd->pinctrl_state_active)) {
		ret = PTR_ERR(dd->pinctrl_state_active);
		pr_debug("Can not lookup active pinstate %d\n",	ret);
	}

	dd->pinctrl_state_suspended
		= pinctrl_lookup_state(dd->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(dd->pinctrl_state_suspended)) {
		ret = PTR_ERR(dd->pinctrl_state_suspended);
		pr_debug("Can not lookup active pinstate %d\n",	ret);
	}
	ret = pinctrl_select_state(dd->ts_pinctrl,
			dd->pinctrl_state_active);
	if (ret < 0)
		pr_debug("pinctrl set active fail\n");


	ret = pdata->init(pdata, true);
	if (ret < 0)
		goto pinit_failure;

	/* Netlink: initialize incoming skb queue */
	skb_queue_head_init(&dd->incoming_skb_queue);

	/* Netlink: register GENL family */
	dd->nl_family.id      = GENL_ID_GENERATE;
	dd->nl_family.version = NL_FAMILY_VERSION;
	GENL_COPY(dd->nl_family.name, pdata->nl_family);

	/* Netlink: register family ops */
	for (i = 0; i < MC_GROUPS; i++) {
		dd->nl_ops[i].cmd = i;
		dd->nl_ops[i].doit = nl_callback_noop;
	}
	dd->nl_ops[MC_DRIVER].doit = nl_callback_driver;
	dd->nl_ops[MC_FUSION].doit = nl_callback_fusion;
	dd->nl_ops[MC_EVENT_BROADCAST].doit = nl_callback_noop;

	/* Netlink: register family multicast groups */
	GENL_COPY(dd->nl_mc_groups[MC_DRIVER].name, MC_DRIVER_NAME);
	GENL_COPY(dd->nl_mc_groups[MC_FUSION].name, MC_FUSION_NAME);
	GENL_COPY(dd->nl_mc_groups[MC_EVENT_BROADCAST].name,
				MC_EVENT_BROADCAST_NAME);
	ret = _genl_register_family_with_ops_grps(&dd->nl_family,
			dd->nl_ops,
			MC_GROUPS,
			dd->nl_mc_groups,
			MC_GROUPS);
	if (ret < 0) {
		dev_err(&spi->dev, "error registering nl_mc_group");
		goto nl_family_failure;
	}
	dd->nl_mc_group_count = MC_GROUPS;

	/* Netlink: pre-allocate outgoing skb */
	ret = nl_msg_new(dd, MC_FUSION);
	if (ret < 0) {
		dev_err(&spi->dev, "error alloc msg");
		goto nl_failure;
	}

	/* start processing thread */
	dd->thread_sched.sched_priority = MAX_USER_RT_PRIO / 2;
	dd->thread = kthread_run(processing_thread, dd, pdata->nl_family);
	if (IS_ERR(dd->thread)) {
		dev_err(&spi->dev, "error creating kthread!");
		ret = PTR_ERR(dd->thread);
		goto kthread_failure;
	}

	/* Netlink: ready to start processing incoming messages */
	dd->nl_enabled = true;

	/* No sysfs update initially */
	dd->sysfs_update_type = DR_SYSFS_UPDATE_NONE;
	init_completion(&dd->sysfs_ack_glove);
	init_completion(&dd->sysfs_ack_charger);
	init_completion(&dd->sysfs_ack_lcd_fps);
	mutex_init(&dd->sysfs_update_mutex);

	/* add us to the devices list */
	spin_lock_irqsave(&dev_lock, flags);
	list_add_tail(&dd->dev_list, &dev_list);
	spin_unlock_irqrestore(&dev_lock, flags);

	ret = create_sysfs_entries(dd);
	if (ret) {
		dev_err(&spi->dev, "failed to create sysfs file");
		goto sysfs_failure;
	}
	snprintf(dd->config_file, CFG_FILE_NAME_MAX, pdata->config_file, panel_id);
	dev_dbg(&spi->dev, "configuration file: %s", dd->config_file);

	/* start up Touch Fusion */
	dd->start_fusion = true;
	wake_up_process(dd->thread);
	dev_info(&spi->dev, "%s: driver loaded; version %s; release date %s",
			dd->nl_family.name, DRIVER_VERSION, DRIVER_RELEASE);

	return 0;

sysfs_failure:
	dd->nl_enabled = false;
	spin_lock_irqsave(&dev_lock, flags);
	list_del(&dd->dev_list);
	spin_unlock_irqrestore(&dev_lock, flags);
	(void)kthread_stop(dd->thread);
kthread_failure:
	if (dd->outgoing_skb)
		kfree_skb(dd->outgoing_skb);
nl_failure:
	genl_unregister_family(&dd->nl_family);
nl_family_failure:
	pdata->init(pdata, false);
pinit_failure:
	regulator_uninit(dd);
	kfree(dd->tx_buf);
buf_alloc_failure:
	spi_set_drvdata(spi, NULL);
	kfree(dd);
of_parse_failure:
	spi->dev.platform_data = NULL;
	devm_kfree(&spi->dev, pdata);
	return ret;
}

static int remove(struct spi_device *spi)
{
	struct maxim_sti_pdata  *pdata = spi->dev.platform_data;
	struct dev_data         *dd = spi_get_drvdata(spi);
	unsigned long           flags;
	u8                      i;

	dev_dbg(&spi->dev, "remove");

	/* BEWARE: tear-down sequence below is carefully staged:            */
	/* 1) first the feeder of Netlink messages to the processing thread */
	/*    is turned off                                                 */
	/* 2) then the thread itself is shut down                           */
	/* 3) then Netlink family is torn down since no one would be using  */
	/*    it at this point                                              */
	/* 4) above step (3) insures that all Netlink senders are           */
	/*    definitely gone and it is safe to free up outgoing skb buffer */
	/*    and incoming skb queue                                        */
	dev_dbg(&spi->dev, "stopping thread..");
	dd->nl_enabled = false;
	(void)kthread_stop(dd->thread);
	dev_dbg(&spi->dev, "kthread stopped.");
	genl_unregister_family(&dd->nl_family);
	if (dd->outgoing_skb)
		kfree_skb(dd->outgoing_skb);
	skb_queue_purge(&dd->incoming_skb_queue);

	if (dd->fusion_process != (pid_t)0)
		(void)kill_pid(find_get_pid(dd->fusion_process), SIGKILL, 1);

	dev_dbg(&spi->dev, "removing sysfs entries..");
	if (dd->parent != NULL)
		sysfs_remove_link(dd->input_dev[0]->dev.kobj.parent,
						MAXIM_STI_NAME);
	remove_sysfs_entries(dd);

	dev_dbg(&spi->dev, "unregister input devices..");
	for (i = 0; i < INPUT_DEVICES; i++)
		if (dd->input_dev[i])
			input_unregister_device(dd->input_dev[i]);
#if defined(CONFIG_FB)
	fb_unregister_client(&dd->fb_notifier);
#endif

	if (dd->irq_registered) {
		dev_dbg(&spi->dev, "disabling interrupt");
		disable_irq(dd->spi->irq);
		dev_dbg(&spi->dev, "stopping scan");
		stop_scan_canned(dd);
		free_irq(dd->spi->irq, dd);
	}

	spin_lock_irqsave(&dev_lock, flags);
	list_del(&dd->dev_list);
	spin_unlock_irqrestore(&dev_lock, flags);

	pdata->reset(pdata, 0);
	usleep_range(100, 120);
	regulator_control(dd, false);
	pdata->init(pdata, false);
	regulator_uninit(dd);

	dev_dbg(&spi->dev, "detaching from spi..");
	spi_set_drvdata(spi, NULL);
	kzfree(dd);
	spi->dev.platform_data = NULL;
	devm_kfree(&spi->dev, pdata);

	dev_info(&spi->dev, "driver unloaded");
	return 0;
}

static void shutdown(struct spi_device *spi)
{
	struct maxim_sti_pdata  *pdata = spi->dev.platform_data;
	struct dev_data         *dd = spi_get_drvdata(spi);

	if (dd->parent != NULL)
		sysfs_remove_link(dd->input_dev[0]->dev.kobj.parent,
						MAXIM_STI_NAME);
	remove_sysfs_entries(dd);

	pdata->reset(pdata, 0);
	usleep_range(100, 120);
	regulator_control(dd, false);
}

/****************************************************************************\
* Module initialization                                                      *
\****************************************************************************/

static const struct spi_device_id id[] = {
	{ MAXIM_STI_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(spi, id);

#ifdef CONFIG_OF
static struct of_device_id maxim_match_table[] = {
	{ .compatible = "maxim,maxim_sti",},
	{ },
};
#endif

static struct spi_driver driver = {
	.probe          = probe,
	.remove         = remove,
	.shutdown       = shutdown,
	.id_table       = id,
	.driver = {
		.name   = MAXIM_STI_NAME,
#ifdef CONFIG_OF
		.of_match_table = maxim_match_table,
#endif
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm     = &pm_ops,
#endif
	},
};

static int maxim_sti_init(void)
{
	INIT_LIST_HEAD(&dev_list);
	spin_lock_init(&dev_lock);
	return spi_register_driver(&driver);
}

static void maxim_sti_exit(void)
{
	spi_unregister_driver(&driver);
}

module_param(panel_id, ushort, S_IRUGO);
MODULE_PARM_DESC(panel_id, "Touch Panel ID Configuration");

module_init(maxim_sti_init);
module_exit(maxim_sti_exit);

MODULE_AUTHOR("Maxim Integrated Products, Inc.");
MODULE_DESCRIPTION("Maxim SmartTouch Imager Touchscreen Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);
